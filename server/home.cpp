#include "website.h"

void serve_home_page(Filesystem& fs, int fd) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>This is a test page</p></body></html>";
	write_http_response(fd, "200 OK", "text/html", hello, strlen(hello));
}
