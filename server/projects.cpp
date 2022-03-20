#include "website.h"

void serve_projects_overview(Filesystem& fs, int fd) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>Projects Overview</p></body></html>";
	write_http_response(fd, "200 OK", "text/html", hello, strlen(hello));
}

void serve_specific_project(Filesystem& fs, int fd, char *name, int len) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>This is a specific project</p></body></html>";
	write_http_response(fd, "200 OK", "text/html", hello, strlen(hello));
}
