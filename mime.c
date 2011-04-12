#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include "mime.h"

mime_msg *mimemsg_new() {
	mime_msg *m = (mime_msg *)malloc(sizeof(mime_msg));
	memset(m, 0, sizeof(mime_msg));

	mimemsg_set_header(m, "Mime-Version", "1.0");

	return m;
}

void mimemsg_free(mime_msg *m) {
	assert(m);

	mime_header *ph, *h = m->header_head;
	while(h) {
		ph = h->next;
		free(h);
		h = ph;
	}

	mime_part *pp, *p = m->part_head;
	while(p) {
		pp = p->next;
		mimepart_free(p);
		p = pp;
	}

	if(m->boundary) free(m->boundary);

	free(m);
}

int mimemsg_add_part(mime_msg *m, mime_part *part) {
	part->next = NULL;
	if(m->part_tail) {
		m->part_tail->next = part;
		m->part_tail = part;
	} else {
		m->part_head = m->part_tail = part;
	}
	return ++m->n_parts;
}

int mimemsg_set_header(mime_msg *m, const char *key, const char *value) {
	mime_header *h;
	for(h = m->header_head; h; h = h->next) {
		if(strcasecmp(h->key, key) == 0) {
			free(h->value);
			h->value = strdup(value);
			return 1;
		}
	}

	h = (mime_header *)malloc(sizeof(mime_header));
	memset(h, 0, sizeof(mime_header));
	h->key = strdup(key);
	h->value = strdup(value);
	h->next = NULL;

	if(m->header_tail) {
		m->header_tail->next = h;
		m->header_tail = h;
	} else {
		m->header_head = m->header_tail = h;
	}

	return ++m->n_heads;
}

int mimemsg_set_boundary(mime_msg *m, const char *boundary) {
	static int sranded = 0;
	static const char *randchars = 
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	static const char *randprefix = "BOUNDARY-";

	if(m->boundary) free(m->boundary);

	if(boundary) {
		m->boundary = strdup(boundary);
	} else {
		if(!sranded) srand(time(NULL));

		int i, rlen = strlen(randchars);
		char buf[40];
		for(i = 0; i < 16; i++)
			buf[i] = randchars[rand() % rlen];
		buf[i] = '\0';

		m->boundary = (char *)malloc(40+strlen(randprefix));
		sprintf(m->boundary, "%s%s", randprefix, buf);
	}
	return 1;
}

static int mime__write_string(mime_stream_write_func writer, void *ctx,
		const char *s) {
	return writer(ctx, s, strlen(s));
}

static int mime__write_strings(mime_stream_write_func writer, void *ctx,
		...) {
	int ret = 0;
	char *str;
	va_list va;
	va_start(va, ctx);
	while((str = va_arg(va, char *)) != NULL)
		ret += mime__write_string(writer, ctx, str);
	va_end(va);
	return ret;
}

static int mimemsg__write_boundary(mime_msg *m, int lastpart,
		mime_stream_write_func writer, void *ctx) {
	mime__write_strings(writer, ctx, "--", m->boundary, NULL);
	if(lastpart) mime__write_string(writer, ctx, "--");
	mime__write_string(writer, ctx, "\r\n");
	return 1;
}

static int mimemsg__real_write_stream(mime_msg *m,
		mime_stream_write_func writer, void *ctx) {
	if(!m->boundary)
		mimemsg_set_boundary(m, NULL);

	mime_header *h;
	for(h = m->header_head; h; h = h->next) {
		mime__write_strings(writer, ctx,
				h->key, ": ", h->value, "\r\n", NULL);
	}

	mime_part *p = m->part_head;
	if(m->n_parts == 1) {
		mimepart_write_stream(p, writer, ctx);
		mime__write_string(writer, ctx, "\r\n");
	} else if(m->n_parts > 1) {
		mime__write_strings(writer, ctx,
			"Content-Type: multipart/mixed; boundary=\"",
			m->boundary, "\"\r\n\r\n", NULL);
		for(; p; p = p->next) {
			mimemsg__write_boundary(m, 0, writer, ctx);
			mimepart_write_stream(p, writer, ctx);
			mime__write_string(writer, ctx, "\r\n");
		}
		mimemsg__write_boundary(m, 1, writer, ctx);
	}
	return 1;
}

typedef struct _mimemsg_wrapper {
	mime_line_write_func orig_writer;
	void *orig_ctx;
	char *buffer;
	int len;
	int wrap;
} _mimemsg_wrapper;

static int mimemsg__wrapper(void *ctx, const void *buf, int len) {
	_mimemsg_wrapper *w = (_mimemsg_wrapper *)ctx;

	while(len > 0) {
		// fill buffer to wrap length
		int remain = w->wrap - w->len;
		if(remain > len) remain = len;
		memcpy(&w->buffer[w->len], buf, remain);
		w->len += remain;

		// shift input buffer
		buf = &(((char *)buf)[remain]);
		len -= remain;

		// shift if there exists a line break
		int wraplen;
		int hasCR = 0;
		for(wraplen = 0; wraplen < w->len; wraplen++) {
			if(w->buffer[wraplen] == '\n') {
				if(wraplen > 0 && w->buffer[wraplen-1] == '\r') {
					wraplen--;
					hasCR = 1;
				}
				break;
			}
		}
		if(wraplen < w->len) {
			w->orig_writer(w->orig_ctx, w->buffer, wraplen);

			// remove line break
			wraplen++;
			if(hasCR) wraplen++;
		} else if(w->len >= w->wrap) {
			// find a space
			for(wraplen = w->len-1; wraplen > 0; wraplen--)
				if(w->buffer[wraplen] == ' ' || w->buffer[wraplen] == '\t')
					break;
			if(wraplen <= 0) wraplen = w->len; // hard wrap

			w->orig_writer(w->orig_ctx, w->buffer, wraplen);
		} else
			continue;

		// shift buffer
		w->len -= wraplen;
		if(w->len)
			memmove(w->buffer, &w->buffer[wraplen], w->len);
	}
	return 1;
}

int mimemsg_write_line(mime_msg *m, int wrap,
		mime_line_write_func writer, void *ctx) {
	_mimemsg_wrapper w;
	w.orig_writer = writer;
	w.orig_ctx = ctx;
	w.buffer = (char *)malloc(wrap + 1);
	w.len = 0;
	w.wrap = wrap;

	int ret = mimemsg__real_write_stream(m, &mimemsg__wrapper, &w);

	// flush buffer
	if(w.len)
		ret += writer(ctx, w.buffer, w.len);

	free(w.buffer);
	return ret;
}
