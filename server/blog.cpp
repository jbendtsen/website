#include "website.h"

#define BLOG_PREVIEW_LINE_LIMIT 20

static void render_blog(Filesystem& fs, Expander& html, int blog_idx) {
	html.add_string("<div class=\"article-preview\"><div class=\"preview-overlay\"></div>", 0);

	fs.refresh_file(blog_idx);

	FS_File& file = fs.files[blog_idx];
	produce_article_html(html, (const char*)file.buffer, file.size, BLOG_PREVIEW_LINE_LIMIT);

	html.add_string("</div>", 0);
}

void serve_blog_overview(Filesystem& fs, int request_fd)
{
	Inline_Vector<int> blogs;
	FS_Directory folder = fs.dirs[fs.lookup_dir("content/blog")];

	int fidx = folder.first_file.created;
	while (fidx >= 0) {
		FS_File& file = fs.files[fidx];
		char *fname = fs.name_pool.at(file.name_idx);
		int len = strlen(fname);

		if (len < 4 || fname[len-3] != '.' || fname[len-2] != 'j' || fname[len-1] != 'b') {
			fidx = file.next.created;
			continue;
		}

		blogs.add(fidx);
		fidx = file.next.created;
	}

	Expander html;
	html.init(2048, false, 0);
	html.add_string("<!DOCTYPE html><html><head><title>Blogs</title><style>\n", 0);

	int css_idx = fs.lookup_file("client/article.css");
	fs.refresh_file(css_idx);
	html.add_string((const char*)fs.files[css_idx].buffer, fs.files[css_idx].size);

	css_idx = fs.lookup_file("client/blogs.css");
	fs.refresh_file(css_idx);
	html.add_string((const char*)fs.files[css_idx].buffer, fs.files[css_idx].size);

	html.add_string("</style></head><body>\n", 0);

	html.add_string("<h1>Blogs</h1>", 0);

	html.add_string("<div id=\"articles\"><div class=\"article-column article-left\">", 0);

	for (int i = 0; i < blogs.size; i += 2)
		render_blog(fs, html, blogs[blogs.size - i - 1]);

	html.add_string("</div><div class=\"article-column article-right\">", 0);

	for (int i = 1; i < blogs.size; i += 2)
		render_blog(fs, html, blogs[blogs.size - i - 1]);

	html.add_string("</div></body></html>", 0);

	int html_size = 0;
	char *response = html.get(&html_size);
	write_http_response(request_fd, "200 OK", "text/html", response, html_size);
}

void serve_specific_blog(Filesystem& fs, int request_fd, char *name, int len) {
	const char *hello =
		"<!DOCTYPE html><html><body><h1>Hello!</h1><p>This is a specific blog</p></body></html>";
	write_http_response(request_fd, "200 OK", "text/html", hello, strlen(hello));
}
