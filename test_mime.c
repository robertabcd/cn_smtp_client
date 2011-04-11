#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mime.h"

int write_to_stdout(void *ctx, const void *buf, int len) {
	return write(STDOUT_FILENO, buf, len);
}

int main() {
	mime_msg *m = mimemsg_new();
	mime_part *p1 = mimepart_new_plain("hello world!");
	mime_part *p2 = mimepart_new_attachment("buffer.c");
	mime_part *p3 = mimepart_new_attachment("smtp.c");

	mimemsg_add_part(m, p1);
	mimemsg_add_part(m, p2);
	mimemsg_add_part(m, p3);

	const char *from = "<test@cnmail.csie.org>",
		  *to = "<madoka@qbey.tw>",
		  *subject = "jizzed";
	mimemsg_set_header(m, "From", from);
	mimemsg_set_header(m, "To", to);
	mimemsg_set_header(m, "Subject", subject);

	mimemsg_write_stream(m, 76, &write_to_stdout, NULL);

	mimemsg_free(m);
	return 0;
}
