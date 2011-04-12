#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "smtp.h"

smtp *smtp_new() {
	smtp *s = (smtp *)malloc(sizeof(smtp));
	memset(s, 0, sizeof(smtp));

	s->rfd = 0;
	s->wfd = 1;
	s->code = -1;
	s->multiline_reply = 0;
	s->msg = buffer_new(0);
	s->readbuf = buffer_new(0);

	return s;
}

void smtp_free(smtp *s) {
	assert(s);
	buffer_free(s->msg);
	buffer_free(s->readbuf);
	free(s);
}

void smtp_set_fd(smtp *s, int rfd, int wfd) {
	s->rfd = rfd;
	s->wfd = wfd;
}

static int smtp__read_line(smtp *s) {
	char buf[4096];
	char *line;
	while(NULL == (line = strstr(buffer_cstr(s->readbuf), SMTP_NEWLINE))) {
		int r = read(s->rfd, buf, sizeof(buf));
		if(r < 0) return -1;
		buffer_append(s->readbuf, buf, r);
	}

	const char *start = buffer_data(s->readbuf);
	int len = line - start + SMTP_NEWLINE_LEN;

	if(len >= 4) {
		buffer_append(s->msg, start, len);
		s->code = atoi(start);
		s->multiline_reply = (start[3] == '-') ? 1 : 0;
		buffer_shift(s->readbuf, len);
		return 1;
	}
	return -1;
}

static int smtp__read_response(smtp *s) {
	buffer_shift(s->msg, buffer_length(s->msg));
	do {
		if(smtp__read_line(s) < 0)
			return 0;
	} while(s->multiline_reply);
	return 1;
}

int smtp_is_positive_response(smtp *s) {
	if(s->code >= 200 && s->code < 300)
		return 1;
	return 0;
}

static int smtp__write_end_data(smtp *s) {
	return write(s->wfd, ".\r\n", 3);
}

int smtp_write(smtp *s, const char *buf, int len) {
	return write(s->wfd, buf, len);
}

int smtp_write_line(smtp *s, const char *buf, int len) {
	if(len > 0 && buf[0] == '.' && write(s->wfd, ".", 1) < 0)
		return -1;
	if(write(s->wfd, buf, len) >= 0 && write(s->wfd, "\r\n", 2) >= 0)
		return 1;
	return -1;
}

int smtp_write_string(smtp *s, const char *str) {
	return smtp_write(s, str, strlen(str));
}

static int smtp__write_strings(smtp *s, ...) {
	int ret = 0;
	char *str;
	va_list va;
	va_start(va, s);
	while((str = va_arg(va, char *)) != NULL)
		ret += smtp_write_string(s, str);
	va_end(va);
	return ret;
}

int smtp_read_welcome(smtp *s) {
	if(smtp__read_response(s) &&
			smtp_is_positive_response(s))
		return 1;
	return 0;
}

int smtp_helo(smtp *s, const char *id) {
	if(smtp__write_strings(s, "HELO ", id, "\r\n", NULL) > 0 &&
			smtp__read_response(s) &&
			smtp_is_positive_response(s))
		return 1;
	return 0;
}

int smtp_mail_from(smtp *s, const char *addr) {
	if(smtp__write_strings(s, "MAIL FROM:", addr, "\r\n", NULL) > 0 &&
			smtp__read_response(s) &&
			smtp_is_positive_response(s))
		return 1;
	return 0;
}

int smtp_rcpt_to(smtp *s, const char *addr) {
	if(smtp__write_strings(s, "RCPT TO:", addr, "\r\n", NULL) > 0 &&
			smtp__read_response(s) &&
			smtp_is_positive_response(s))
		return 1;
	return 0;
}

int smtp_data(smtp *s, smtp_data_callback cb, void *ctx) {
	if(smtp_write_string(s, "DATA\r\n") > 0 &&
			smtp__read_response(s) &&
			smtp_get_code(s) == 354 &&
			cb(s, ctx) > 0 &&
			smtp__write_end_data(s) &&
			smtp__read_response(s) &&
			smtp_is_positive_response(s))
		return 1;
	return 0;
}

int smtp_quit(smtp *s) {
	if(smtp_write_string(s, "QUIT\r\n") > 0 &&
			smtp__read_response(s))
		return 1;
	return 0;
}

int smtp_get_code(smtp *s) {
	return s->code;
}

const char *smtp_get_msg(smtp *s) {
	return buffer_cstr(s->msg);
}
