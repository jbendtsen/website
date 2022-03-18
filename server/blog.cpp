#include "website.h"

void serve_blog_overview(Filesystem& fs, int request_fd) {
	Vector<int>
}

void serve_specific_blog(Filesystem& fs, int request_fd, char *name, int len) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>This is a specific blog</p></body></html>";
	write_http_response(request_fd, "200 OK", "text/html", hello, strlen(hello));
}
