#include "base64.h"

static const char *b64_en = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz0123456789+/";

int base64_encode(const unsigned char *data, int dlen, char *buf, int *blen) {
	const unsigned char *d = data;
	const unsigned char *e = &d[dlen];
	int i = 0;

	while(&d[3] <= e) {
		if(i + 4 > *blen) {
			*blen = i;
			return -1;
		}
		buf[i++] = b64_en[(d[0] >> 2)];
		buf[i++] = b64_en[(d[0] & 0x03) << 4 | d[1] >> 4];
		buf[i++] = b64_en[(d[1] & 0x0F) << 2 | d[2] >> 6];
		buf[i++] = b64_en[(d[2] & 0x3F)];
		d += 3;
	}

	switch(e - d) {
		case 1:
			buf[i++] = b64_en[(d[0] >> 2)];
			buf[i++] = b64_en[(d[0] & 0x03) << 4];
			buf[i++] = '=';
			buf[i++] = '=';
			break;
		case 2:
			buf[i++] = b64_en[(d[0] >> 2)];
			buf[i++] = b64_en[(d[0] & 0x03) << 4 | (d[1] >> 4)];
			buf[i++] = b64_en[(d[1] & 0x0F) << 2];
			buf[i++] = '=';
			break;
	}

	*blen = i;
	return i;
}

int base64_encode_stream(read_func reader, write_func writer, void *ctx) {
	unsigned char rbuf[BASE64_BLOCK_BASE*3];
	char wbuf[BASE64_BLOCK_BASE*4];
	int rlen, wlen;
	while((rlen = reader(ctx, rbuf, sizeof(rbuf))) > 0) {
		wlen = sizeof(wbuf);
		if(base64_encode(rbuf, rlen, wbuf, &wlen) < 0)
			return -2;
		if(writer(ctx, wbuf, wlen) < 0)
			return -1;
	}
	return 1;
}
