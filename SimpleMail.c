#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>

#include "mime.h"
#include "smtp.h"

static int connect_server(const char *host, const char *strport) {
	struct addrinfo hints, *result;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	int error = getaddrinfo(host, strport, &hints, &result);
	if(error) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
		return -1;
	}

	int fd;
	const struct addrinfo *ai;
	for(ai = result; ai; ai = ai->ai_next) {
		if((fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)) < 0)
			continue;

		if(connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
			close(fd);
			continue;
		}

		freeaddrinfo(result);
		return fd;
	}

	freeaddrinfo(result);
	return -1;
}

static int data_cb(smtp *s, void *ctx) {
	return mimemsg_write_line((mime_msg *)ctx, 76,
			(mime_line_write_func)&smtp_write_line, s);
}

static int Error(const char *msg) {
	fprintf(stderr, "%s", msg);
	return 0;
}

static const char *NormalizeAddress(const char *addr) {
	static char buffer[256];
	if(strlen(addr) < 1) return NULL;
	if(addr[0] == '<')
		strncpy(buffer, addr, sizeof(buffer));
	else
		strncpy(&buffer[1], addr, sizeof(buffer)-1);
	buffer[0] = '<';
	if(buffer[strlen(buffer)-1] != '>')
		strncat(buffer, ">", sizeof(buffer)-strlen(buffer)-1);
	return buffer;
}

static int Usage(int argc, char *argv[]) {
	fprintf(stderr,
			"Usage: %s host port\n",
			argv[0]);
	return 0;
}

static const char *ReadLineToBuffer(buffer_ctx *b) {
	buffer_shift(b, buffer_length(b));
	char buffer[128];
	while(1) {
		if(fgets(buffer, sizeof(buffer), stdin) == NULL) return NULL;
		int len = strlen(buffer);
		if(buffer[len-1] == '\n') {
			buffer[len-1] = '\0';
			buffer_append_string(b, buffer);
			return buffer_cstr(b);
		}
		buffer_append_string(b, buffer);
	}
	return NULL;
}

static void Prompt(const char *fmt, ...) {
	va_list va;
	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	va_end(va);
}

static char PromptOpt(const char *msg, const char *opts, buffer_ctx *b) {
	while(1) {
		Prompt("%s", msg);
		const char *buffer = ReadLineToBuffer(b);
		if(buffer == NULL) return '\0';
		int i;
		for(i = 0; opts[i]; i++)
			if(buffer[0] == opts[i]) return opts[i];
		return opts[0];
	}
}

static int HandleSmtpError(smtp *s) {
	int code = smtp_get_code(s);

	Prompt("Received %d error: %s", code, smtp_get_msg(s));

	if(code == 421) // we must exit NOW!
		return 0;
	else
		return 1;
}

static int ReadAndSendMail(mime_msg *m, smtp *s,
		buffer_ctx *b, buffer_ctx *strbuf) {
	// From
	while(1) {
		Prompt("From: ");
		const char *addr = NormalizeAddress(ReadLineToBuffer(b));
		if(smtp_mail_from(s, addr)) {
			mimemsg_set_header(m, "From", addr);
			break;
		} else if(!HandleSmtpError(s)) return 0;
	}

	// To
	int i;
	Prompt("Where should I send this message? Blank line to end.\n");
	buffer_shift(strbuf, buffer_length(strbuf));
	i = 0;
	while(1) {
		Prompt("To [%d]: ", i+1);
		const char *addr = ReadLineToBuffer(b);
		if(strlen(addr) == 0) {
			if(i > 0) break;
			Prompt("You have to specify at least one address.\n");
			continue;
		}
		addr = NormalizeAddress(addr);
		if(smtp_rcpt_to(s, addr)) {
			if(i > 0) buffer_append_string(strbuf, ", ");
			buffer_append_string(strbuf, addr);
			i++;
		} else if(!HandleSmtpError(s)) return 0;
	}
	mimemsg_set_header(m, "To", buffer_cstr(strbuf));

	// Cc
	Prompt("Should I CC this message? Blank line to end.\n");
	buffer_shift(strbuf, buffer_length(strbuf));
	i = 0;
	while(1) {
		Prompt("Cc [%d]: ", i+1);
		const char *addr = ReadLineToBuffer(b);
		if(strlen(addr) == 0)
			break;
		addr = NormalizeAddress(addr);
		if(smtp_rcpt_to(s, addr)) {
			if(i > 0) buffer_append_string(strbuf, ", ");
			buffer_append_string(strbuf, addr);
			i++;
		} else if(!HandleSmtpError(s)) return 0;
	}
	if(i > 0)
		mimemsg_set_header(m, "Cc", buffer_cstr(strbuf));

	// Subject
	Prompt("Subject: ");
	const char *subject = ReadLineToBuffer(b);
	if(strlen(subject))
		mimemsg_set_header(m, "Subject", subject);

	// Read content
	Prompt("Enter content, a single line with \"--end--\" to end.\n");
	buffer_shift(strbuf, buffer_length(strbuf));
	int lines = 0;
	Prompt("%3d: ", ++lines);
	while(1) {
		const char *line = ReadLineToBuffer(b);
		if(line == NULL) return 0;
		if(strcmp("--end--", line) == 0) break;
		Prompt("%3d: ", ++lines);
		buffer_append_string(strbuf, buffer_cstr(b));
		buffer_append_string(strbuf, "\n");
	}
	mimemsg_add_part(m, mimepart_new_plain(buffer_cstr(strbuf)));

	// Attachments
	Prompt("Add attachments, blank line to end.\n");
	i = 0;
	while(1) {
		Prompt("Attachment [%d]: ", i+1);
		const char *fn = ReadLineToBuffer(b);
		if(strlen(fn) == 0) break;
		mime_part *mp = mimepart_new_attachment(fn);
		if(!mp) {
			Prompt("Unable to attach this file.\n");
			continue;
		}
		mimemsg_add_part(m, mp);
		i++;
	}

	// Send to server
	Prompt("Sending message...\n");
	if(!smtp_data(s, &data_cb, m))
		return HandleSmtpError(s);
	Prompt("Message sent.\n");

	return 1;
}

int main(int argc, char *argv[]) {
	if(argc != 3) {
		Usage(argc, argv);
		return -1;
	}

	int fd = connect_server(argv[1], argv[2]);
	if(fd < 0) {
		fprintf(stderr, "Unable to connect to server.\n");
		return -1;
	}

	Prompt("Connected to %s:%s\n", argv[1], argv[2]);

	buffer_ctx *b = buffer_new(0);
	buffer_ctx *strbuf = buffer_new(0);
	smtp *s = smtp_new();
	smtp_set_fd(s, fd, fd);

	// Wait for welcome msg
	if(!smtp_read_welcome(s) && !smtp_helo(s, "cnmail.csie.org"))
		return Error("No welcome message.");

	int ret;
	do {
		mime_msg *m = mimemsg_new();
		ret = ReadAndSendMail(m, s, b, strbuf);
		mimemsg_free(m);
	} while(ret &&
			tolower(PromptOpt("Send another [y/N]? ", "nyYN", b)) == 'y');

	smtp_quit(s);
	smtp_free(s);
	buffer_free(b);
	buffer_free(strbuf);

	return 0;
}
