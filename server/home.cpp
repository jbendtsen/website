#include "website.h"

void serve_home_page(Filesystem& fs, Response& response) {
	String *html = &response.html;
	html->add("<!DOCTYPE html><html><head>" HTML_METAS "<title>Jack Bendtsen</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	html->add(
		"#home-main {\n"
		"\tpadding: 1rem;\n"
		"}\n"
		"#home-main h1 {\n"
		"\tfont-size: 3rem;\n"
		"}\n"
	);

	html->add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_HOME);

	int home_jb_idx = fs.lookup_file("client/home.jb");
	if (home_jb_idx >= 0) {
		html->add("<article id=\"home-main\">");
		Markdown_Params md_params = {0};
		produce_markdown_html(*html, (const char *)fs.files[home_jb_idx].buffer, fs.files[home_jb_idx].size, md_params);
		html->add("</article>");
	}

	html->add("</body></html>");
}
