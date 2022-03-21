#include "website.h"

void serve_projects_overview(Filesystem& fs, int fd) {
	Expander html;
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Projects</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	html.add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	html.add("<h1>Projects Overview</h1></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}

void serve_specific_project(Filesystem& fs, int fd, char *name, int len) {
	Expander html;
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Projects</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	html.add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	html.add("<h1>Specific Project</h1></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}
