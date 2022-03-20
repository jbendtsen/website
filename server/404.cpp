#include "website.h"

void serve_404(Filesystem& fs, int fd) {
	String page;
	page.reformat(
		"<!DOCTYPE html><html><head>"
		"<meta charset=\"UTF-8\">"
		"<title>404 - Jack Bendtsen</title>"
		"<link rel=\"stylesheet\" href=\"light.css\">"
		"</head><body>"
		"<h1>404</h1>"
		"<p>Looks like you're outta luck, bud!</p>"
		"</body></html>"
	);

	write_http_response(fd, "404 Not Found", "text/html", page.data(), page.size());
}


