#ifndef _BASE64_H
#	define _BASE64_H

// When encoding stream, read BASE*3 bytes and encode into BASE*4 bytes.
#define BASE64_BLOCK_BASE (1024)

typedef int (* read_func) (void *ctx, void *buf, int len);
typedef int (* write_func) (void *ctx, const void *buf, int len);

int base64_encode(const unsigned char *data, int dlen, char *buf, int *blen);
int base64_encode_stream(read_func reader, write_func writer, void *ctx);

#endif
