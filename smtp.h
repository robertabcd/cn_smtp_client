#ifndef _SMTP_H
#	define	_SMTP_H

#include <unistd.h>

#include "buffer.h"

#ifdef SMTP_NEWLINE_UNIX
#	define SMTP_NEWLINE		"\n"
#	define SMTP_NEWLINE_LEN	(1)
#else
#	define SMTP_NEWLINE		"\r\n"
#	define SMTP_NEWLINE_LEN	(2)
#endif

struct smtp;

// return > 0 means success, otherwise error
typedef int (* smtp_data_callback) (struct smtp *s, void *ctx);

typedef struct smtp {
	int rfd, wfd;
	int code;
	int multiline_reply;
	buffer_ctx *msg;
	buffer_ctx *readbuf;
} smtp;

smtp *smtp_new();
void smtp_free(smtp *s);

void smtp_set_fd(smtp *s, int rfd, int wfd);

// return value: 0 error, 1 success
int smtp_read_welcome(smtp *s);
int smtp_helo(smtp *s, const char *id);
int smtp_mail_from(smtp *s, const char *addr);
int smtp_rcpt_to(smtp *s, const char *addr);
int smtp_data(smtp *s, smtp_data_callback cb, void *ctx);
int smtp_quit(smtp *s);

// return value: <0 error, >=0 success
int smtp_write_string(smtp *s, const char *str);
int smtp_write(smtp *s, const char *buf, int len);
int smtp_write_line(smtp *s, const char *buf, int len);

int smtp_is_positive_response(smtp *s);
int smtp_get_code(smtp *s);
const char *smtp_get_msg(smtp *s);

#endif
