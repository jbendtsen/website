#include "website.h"

void serve_home_page(Filesystem& fs, int fd) {
	Expander html;
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Home</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	html.add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_HOME);

	html.add("<h1>Hello!</h1><p>This is a test page</p></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}
