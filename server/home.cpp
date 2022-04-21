#include "website.h"

void serve_home_page(Filesystem& fs, Response& response) {
	String *html = &response.html;
	html->add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Jack Bendtsen</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	html->add(
		"#home-main {\n"
		"\tpadding: 1em;\n"
		"}"
	);

	html->add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_HOME);

	html->add("<div id=\"home-main\" contenteditable=\"true\">");
	fs.add_file_to_html(html, "client/home.html");
	html->add("</div></body></html>");
}
