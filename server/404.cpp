#include "website.h"

void serve_404(Filesystem& fs, int fd) {
	String page;
	page.add("<!DOCTYPE html><html><head>"
		"<meta charset=\"UTF-8\">"
		"<title>404 - Page Not Found</title><style>\n");

	fs.add_file_to_html(page, "client/banner.css");

	page.add("</style></head><body>");

	add_banner(fs, page, NAV_IDX_NULL);

	page.add("<h1>404</h1>"
		"<p>Looks like you're outta luck, bud!</p>"
		"</body></html>");

	write_http_response(fd, "404 Not Found", "text/html", page.data(), page.len);
}

