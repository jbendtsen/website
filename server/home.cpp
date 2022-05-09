#include "website.h"

void serve_home_page(Filesystem& fs, Response& response) {
	String *html = &response.html;
	html->add("<!DOCTYPE html><html><head>" HTML_METAS "<title>Jack Bendtsen</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/home.css");

	html->add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_HOME);

	html->add(
		"<article id=\"home-main\" style=\"margin-top: 1rem; margin-left: 1rem; max-width: 60rem\">"
			"<img style=\"float: left; height: 16rem; margin-bottom: 2rem; margin-right: 3rem; box-shadow: 4px 2px 10px #bbb;\" src=\"client/me.jpg\">"
			"<h1>Jack Bendtsen</h1>"
			"<div id=\"home-socials\">"
				"<a href=\"https://github.com/jbendtsen\"><img src=\"/content/github.png\"><span>jbendtsen</span></a>"
				"<a href=\"https://www.linkedin.com/in/jack-bendtsen\"><img src=\"/content/linkedin.png\"><span>jack-bendtsen</span></a>"
				"<a href=\"mailto:jackdbendtsen@gmail.com\"><img src=\"/content/email.png\"><span>jackdbendtsen@gmail.com</span></a>"
			"</div>"
	);

	FS_File *home_md = &fs.files[fs.lookup_file("content/home.jb")];

	Markdown_Params md_params = {0};
	produce_markdown_html(*html, (const char *)home_md->buffer, home_md->size, md_params);

	html->add("</article></body></html>");
}
