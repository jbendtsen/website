#include "website.h"

void serve_blog_overview(File_Database& internal, int fd) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>Blog Overview</p></body></html>";
	write_http_response(fd, "200 OK", "text/html", hello, strlen(hello));
}

void serve_specific_blog(File_Database& internal, int fd, char *name, int len) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>This is a specific blog</p></body></html>";
	write_http_response(fd, "200 OK", "text/html", hello, strlen(hello));
}