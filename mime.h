#ifndef _MIME_H
#	define _MIME_H

#define MIME_TRANSFER_ENCODING_PLAIN	(0)
#define MIME_TRANSFER_ENCODING_BASE64	(1)

struct mime_part;

typedef int (* mime_stream_write_func) (void *ctx, const void *buf, int len);
typedef int (* mimepart_stream_write_func)
	(struct mime_part *p, mime_stream_write_func writer, void *ctx);

typedef struct mime_part {
	struct mime_part *next;
	char content_type[256];
	int transfer_encoding;

	void *writer_ctx;
	mimepart_stream_write_func writer;

	void (*free) (struct mime_part *);
} mime_part;

typedef struct mime_header {
	struct mime_header *next;
	char *key;
	char *value;
} mime_header;

typedef struct {
	mime_part *part_head, *part_tail;
	int n_parts;
	mime_header *header_head, *header_tail;
	int n_heads;
	char *boundary;
} mime_msg;


mime_msg *mimemsg_new();
void mimemsg_free(mime_msg *m);

int mimemsg_add_part(mime_msg *m, mime_part *part);
int mimemsg_set_header(mime_msg *m, const char *key, const char *value);
int mimemsg_set_boundary(mime_msg *m, const char *boundary);

int mimemsg_write_stream(mime_msg *m, int wrap,
		mime_stream_write_func writer, void *ctx);


mime_part *mimepart_new_plain(const char *str);
mime_part *mimepart_new_attachment(const char *path);
void mimepart_free(mime_part *p);
int mimepart_write_stream(mime_part *m,
		mime_stream_write_func writer, void *ctx);

#endif
