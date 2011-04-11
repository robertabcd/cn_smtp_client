#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "base64.h"

typedef struct en_stream {
	FILE *fp;
	int fd;
	char buf[80];
	int buflen;
	int wrap;
} en_stream;

int readfp(void *ctx, void *buf, int len) {
	return fread(buf, 1, len, ((en_stream *)ctx)->fp);
}

int write_stdout(void *ctx, const void *buf, int len) {
	int w = 0, wlen = len;
	en_stream *ens = (en_stream *)ctx;
	if(ens->buflen + len >= ens->wrap) {
		if(ens->buflen)
			w += write(ens->fd, ens->buf, ens->buflen);
		int remain_len = ens->wrap - ens->buflen;
		w += write(ens->fd, buf, remain_len);
		write(ens->fd, "\n", 1);
		len -= remain_len;
		buf += remain_len;
		ens->buflen = 0;
	}

	while(len >= ens->wrap) {
		w += write(ens->fd, buf, ens->wrap);
		write(ens->fd, "\n", 1);
		len -= ens->wrap;
		buf += ens->wrap;
	}

	if(len > 0) {
		memcpy(&ens->buf[ens->buflen], buf, len);
		ens->buflen += len;
	}

	return wlen;
}

int ens_flush_write_buf(en_stream *ens) {
	if(ens->buflen)
		return write(ens->fd, ens->buf, ens->buflen);
	return 0;
}

int main(int argc, char *argv[]) {
	if(argc != 2) {
		fprintf(stderr, "Usage: %s file\n", argv[0]);
		return -1;
	}

	char *fn = argv[1];
	en_stream ens;
	FILE *fp = fopen(fn, "r");

	ens.fp = fp;
	ens.fd = STDOUT_FILENO;
	ens.buflen = 0;
	ens.wrap = 76;
	base64_encode_stream(&readfp, &write_stdout, (void *)&ens);

	ens_flush_write_buf(&ens);

	fclose(fp);

	return 0;
}
