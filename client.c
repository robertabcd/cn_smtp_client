#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>

#include "mime.h"
#include "smtp.h"

#define MAX_ENT (512)
typedef struct Config {
	char *server;
	short port;
	char *from;
	char *to[MAX_ENT], *cc[MAX_ENT], *at[MAX_ENT];
	int nto, ncc, nat;
	char *subject;
	char *content_fn;
	char *content;
} Config;

static int connect_server(const char *host, short port) {
	char strport[8];
	snprintf(strport, sizeof(strport), "%d", (int)port);

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
	return mimemsg_write_stream((mime_msg *)ctx, 76,
			(mime_stream_write_func)&smtp_write, s);
}

static int print_smtp_reply(smtp *s) {
	fprintf(stderr, "%s\n", smtp_get_msg(s));
	return 1;
}

static int Error(const char *msg) {
	fprintf(stderr, "%s", msg);
	return 0;
}

static const char *NormalizeAddress(const char *addr) {
	static char buffer[256];
	if(strlen(addr) < 2) return NULL;
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
			"Usage: %s\n"
			"  -h host\n"
			" [-p port]\n"
			"  -f from_address\n"
			"  -t to_addr1 [-t to_addr2] [...]\n"
			" [-c cc_addr1] [-c cc_addr2] [...]\n"
			" [-s subject]\n"
			"  -d content | -D content_file\n"
			" [-a attach_file1] [-a attach_file2] [...]\n",
			argv[0]);
	return 0;
}

static int ParseArgs(int argc, char *argv[], Config *c) {
	c->port = 25;

	int ch;
	while((ch = getopt(argc, argv, "h:p:f:t:c:s:d:D:a:")) != -1) {
		switch(ch) {
			case 'h':
				c->server = strdup(optarg);
				break;
			case 'p':
				c->port = atoi(optarg);
				break;
			case 'f':
				if(c->from)
					return Error("Only one -f argument can bge specified.\n");
				c->from = strdup(NormalizeAddress(optarg));
				break;
			case 't':
				if(c->nto >= MAX_ENT)
					return Error("Too many receivers.\n");
				c->to[c->nto++] = strdup(NormalizeAddress(optarg));
				break;
			case 'c':
				if(c->ncc >= MAX_ENT)
					return Error("Too many CCs.\n");
				c->cc[c->ncc++] = strdup(NormalizeAddress(optarg));
				break;
			case 's':
				if(c->subject)
					return Error("Only one -s argument can be specified.\n");
				c->subject = strdup(optarg);
				break;
			case 'd':
				if(c->content)
					return Error("Only one -d argument can be specified.\n");
				if(c->content_fn)
					return Error("Only one of -d and -D can be specified.\n");
				c->content = strdup(optarg);
				break;
			case 'D':
				if(c->content_fn)
					return Error("Only one -D argument can be specified.\n");
				if(c->content)
					return Error("Only one of -d and -D can be specified.\n");
				c->content_fn = strdup(optarg);
				break;
			case 'a':
				if(c->nat >= MAX_ENT)
					return Error("Too many attachments.\n");
				c->at[c->nat++] = strdup(optarg);
				break;
			case '?':
			default:
				Usage(argc, argv);
				return 0;
		}
	}
	return 1;
}

static int ResetConfig(Config *c) {
	int i;
	for(i = 0; i < MAX_ENT; i++) {
		if(c->to[i]) free(c->to[i]);
		if(c->cc[i]) free(c->cc[i]);
		if(c->at[i]) free(c->at[i]);
	}
	if(c->from) free(c->from);
	if(c->server) free(c->server);
	if(c->subject) free(c->subject);
	return 1;
}

static int SetupMimeMsg(mime_msg *m, Config *c) {
	if(!c->from)
		return Error("No from address found.\n");
	if(!c->nto)
		return Error("No recipients.\n");

	int i;
	buffer_ctx *buffer = buffer_new(0);

	// From
	mimemsg_set_header(m, "From", c->from);

	// To
	buffer_append_string(buffer, c->to[0]);
	for(i = 1; i < c->nto; i++) {
		buffer_append_string(buffer, ", ");
		buffer_append_string(buffer, c->to[i]);
	}
	mimemsg_set_header(m, "To", buffer_cstr(buffer));

	// Cc
	if(c->ncc) {
		buffer_shift(buffer, buffer_length(buffer));
		buffer_append_string(buffer, c->cc[0]);
		for(i = 1; i < c->ncc; i++) {
			buffer_append_string(buffer, ", ");
			buffer_append_string(buffer, c->cc[i]);
		}
		mimemsg_set_header(m, "Cc", buffer_cstr(buffer));
	}

	buffer_free(buffer);

	// Subject
	if(c->subject)
		mimemsg_set_header(m, "Subject", c->subject);

	// Content
	mime_part *mp;
	if(c->content_fn) {
		FILE *fp = fopen(c->content_fn, "r");
		if(!fp) return Error("Cannot open content file.\n");

		int readlen;
		char readbuf[4096];

		buffer = buffer_new(0);
		do {
			readlen = fread(readbuf, 1, sizeof(readbuf), fp);
			buffer_append(buffer, readbuf, readlen);
		} while(readlen == sizeof(readbuf));
		fclose(fp);

		mp = mimepart_new_plain(buffer_cstr(buffer));

		buffer_free(buffer);
	} else if(c->content) {
		mp = mimepart_new_plain(c->content);
	} else {
		return Error("No content specified.\n");
	}
	if(!mp) return Error("Internal error: mimepart_new_plain\n");
	mimemsg_add_part(m, mp);

	// Attachments
	if(c->nat) {
		for(i = 0; i < c->nat; i++) {
			mime_part *mp = mimepart_new_attachment(c->at[i]);
			if(!mp) return 0;
			mimemsg_add_part(m, mp);
		}
	}

	return 1;
}

static int SendMail(Config *c, smtp *s, mime_msg *m) {
	int i;

	if(!smtp_read_welcome(s) ||
			!smtp_helo(s, "jizz.com") ||
			!smtp_mail_from(s, c->from))
		return 0;

	for(i = 0; i < c->nto; i++)
		if(!smtp_rcpt_to(s, c->to[i]))
			return 0;

	for(i = 0; i < c->ncc; i++)
		if(!smtp_rcpt_to(s, c->cc[i]))
			return 0;

	if(!smtp_data(s, &data_cb, m) ||
			!smtp_quit(s))
		return 0;

	return 1;
}

int main(int argc, char *argv[]) {
	Config cfg;
	memset(&cfg, 0, sizeof(cfg));

	mime_msg *m = mimemsg_new();

	if(ParseArgs(argc, argv, &cfg) && SetupMimeMsg(m, &cfg)) {
		if(!cfg.server)
			return Error("No server specified.\n");

		int fd = connect_server(cfg.server, cfg.port);
		if(fd < 0) {
			fprintf(stderr, "Unable to connect\n");
			return -1;
		}

		smtp *s = smtp_new();
#ifndef SMTP_NEWLINE_UNIX
		smtp_set_fd(s, fd, fd);
#endif

		if(SendMail(&cfg, s, m)) {
			fprintf(stderr, "message sent\n");
			print_smtp_reply(s);
		} else {
			fprintf(stderr, "[Error]\n");
			print_smtp_reply(s);
		}

		smtp_free(s);
		close(fd);
	} else {
		Usage(argc, argv);
	}

	mimemsg_free(m);

	ResetConfig(&cfg);
	return 0;
}
