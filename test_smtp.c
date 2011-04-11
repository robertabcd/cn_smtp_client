#include <stdio.h>

#include "smtp.h"

int data_cb(smtp *s, void *ctx) {
	smtp_write_string(s, "data writing...\r\n");
	smtp_write_string(s, ".dot writing...\r\n");
	return 1;
}

int main() {
	smtp *s = smtp_new();

	smtp_read_welcome(s);
	smtp_helo(s, "jizz.com");
	smtp_mail_from(s, "<user@example.com>");
	smtp_rcpt_to(s, "<jizz@qbey.tw>");
	smtp_data(s, &data_cb, NULL);
	smtp_quit(s);

	smtp_free(s);
	return 0;
}
