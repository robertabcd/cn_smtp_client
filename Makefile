client:
	gcc -Wall -g -o SimpleMail SimpleMail.c buffer.c smtp.c mime.c mimepart.c base64.c
cmdline:
	gcc -Wall -g -o client client.c buffer.c smtp.c mime.c mimepart.c base64.c
test:
	gcc -Wall -g -o test_b64 test_b64.c base64.c
	gcc -Wall -g -o test_mime test_mime.c mime.c mimepart.c base64.c
	gcc -Wall -g -DSMTP_NEWLINE_UNIX -o test_smtp test_smtp.c smtp.c mime.c mimepart.c base64.c buffer.c
all: client test
clean:
	rm -f SimpleMail client test_b64 test_mime test_smtp
