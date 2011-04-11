#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "mime.h"
#include "base64.h"

static mime_part *mimepart__alloc() {
	mime_part *p = (mime_part *)malloc(sizeof(mime_part));
	memset(p, 0, sizeof(mime_part));
	return p;
}

static int mimepart__write_string(mime_stream_write_func writer, void *ctx,
		const char *s) {
	return writer(ctx, s, strlen(s));
}

static int mimepart__write_strings(mime_stream_write_func writer, void *ctx,
		...) {
	int ret = 0;
	char *str;
	va_list va;
	va_start(va, ctx);
	while((str = va_arg(va, char *)) != NULL)
		ret += mimepart__write_string(writer, ctx, str);
	va_end(va);
	return ret;
}

static int mimepart__write_stream_header(mime_part *p,
		mime_stream_write_func writer, void *ctx) {
	mimepart__write_strings(writer, ctx,
			"Content-Type: ", p->content_type, "\r\n", NULL);

	switch(p->transfer_encoding) {
		case MIME_TRANSFER_ENCODING_PLAIN:
			mimepart__write_string(writer, ctx,
					"Content-Transfer-Encoding: 7bit\r\n");
			break;
		case MIME_TRANSFER_ENCODING_BASE64:
			mimepart__write_string(writer, ctx,
					"Content-Transfer-Encoding: base64\r\n");
			break;
	}
	return 1;
}

void mimepart_free(mime_part *p) {
	p->free(p);
}

int mimepart_write_stream(mime_part *p, 
		mime_stream_write_func writer, void *ctx) {
	return p->writer(p, writer, ctx);
}

/*** Plain text ***/
typedef struct _mimepart_plain {
	char *str;
} _mimepart_plain;

static int mimepart__plain_writer(mime_part *p,
		mime_stream_write_func writer, void *ctx) {
	_mimepart_plain *c = (_mimepart_plain *)p->writer_ctx;

	mimepart__write_stream_header(p, writer, ctx);
	mimepart__write_string(writer, ctx, "\r\n");

	return writer(ctx, c->str, strlen(c->str));
}

void mimepart__plain_free(mime_part *p) {
	_mimepart_plain *c = (_mimepart_plain *)p->writer_ctx;
	free(c->str);
	free(c);
	free(p);
}

mime_part *mimepart_new_plain(const char *str) {
	mime_part *p = mimepart__alloc();

	strcpy(p->content_type, "text/plain; charset=US-ASCII");
	p->transfer_encoding = MIME_TRANSFER_ENCODING_PLAIN;

	_mimepart_plain *ctx = (_mimepart_plain *)malloc(sizeof(_mimepart_plain));
	ctx->str = strdup(str);

	p->writer_ctx = ctx;
	p->writer = &mimepart__plain_writer;
	p->free = &mimepart__plain_free;

	return p;
}

/*** Attachment with base64 encoding ***/
typedef struct _mimepart_attach {
	char path[PATH_MAX];
	char fn[128];
	int fd;
} _mimepart_attach;

typedef struct _mimepart_attach_file {
	mime_stream_write_func writer;
	void *ctx;
	_mimepart_attach *att;
} _mimepart_attach_file;

int mimepart__parse_filename(const char *path, char *buf, int blen) {
	int i;
	for(i = strlen(path)-1; i >= 0; i--)
		if(path[i] == '/') break;
	strncpy(buf, &path[i+1], blen);
	return 1;
}

int mimepart__attach_file_reader(void *ctx, void *buf, int len) {
	return read(((_mimepart_attach_file *)ctx)->att->fd, buf, len);
}

int mimepart__attach_file_writer(void *ctx, const void *buf, int len) {
	_mimepart_attach_file *fc = (_mimepart_attach_file *)ctx;
	return fc->writer(fc->ctx, buf, len);
}

int mimepart__attach_writer(mime_part *p, 
		mime_stream_write_func writer, void *ctx) {
	_mimepart_attach *att = (_mimepart_attach *)p->writer_ctx;

	mimepart__write_stream_header(p, writer, ctx);
	mimepart__write_strings(writer, ctx,
			"Content-Disposition: attachment; filename=\"",
			att->fn, "\"\r\n\r\n", NULL);

	lseek(att->fd, SEEK_SET, 0);

	_mimepart_attach_file fc;
	fc.writer = writer;
	fc.ctx = ctx;
	fc.att = att;

	return base64_encode_stream(
			&mimepart__attach_file_reader,
			&mimepart__attach_file_writer,
			&fc);
}

void mimepart__attach_free(mime_part *p) {
	_mimepart_attach *ctx = (_mimepart_attach *)p->writer_ctx;
	close(ctx->fd);
	free(ctx);
	free(p);
}

mime_part *mimepart_new_attachment(const char *path) {
	_mimepart_attach *ctx = 
		(_mimepart_attach *)malloc(sizeof(_mimepart_attach));

	if((ctx->fd = open(path, O_RDONLY)) < 0) {
		free(ctx);
		return NULL;
	}
	strcpy(ctx->path, path);
	mimepart__parse_filename(path, ctx->fn, sizeof(ctx->fn));

	mime_part *p = mimepart__alloc();
	sprintf(p->content_type, "application/x-msdownload; name=\"%s\"", ctx->fn);
	p->transfer_encoding = MIME_TRANSFER_ENCODING_BASE64;

	p->writer = &mimepart__attach_writer;
	p->writer_ctx = ctx;
	p->free = &mimepart__attach_free;

	return p;
}
