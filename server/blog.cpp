#include "website.h"

#define BLOG_PREVIEW_LINE_LIMIT 20

static void render_blog_preview(Filesystem& fs, Expander& html, int blog_idx)
{
	FS_File& file = fs.files[blog_idx];

	char *fname = fs.name_pool.at(file.name_idx);
	int name_len = strlen(fname) - 3;
	if (name_len <= 0) {
		html.add_string("<div class=\"article-preview\"><div class=\"preview-overlay\"></div></div>", 0);
		return;
	}

	html.add_string("<a href=\"blog/", 0);
	html.add_and_escape(fname, name_len);
	html.add_string("\"><div class=\"article-preview\"><div class=\"preview-overlay\"></div>", 0);

	fs.refresh_file(blog_idx);

	produce_article_html(html, (const char*)file.buffer, file.size, file.created_time, BLOG_PREVIEW_LINE_LIMIT);

	html.add_string("</div></a>", 0);
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
	html.init(blogs.size * 2048, false, 0);
	html.add_string("<!DOCTYPE html><html><head><title>Blogs</title><style>\n", 0);

	int css_idx = fs.lookup_file("client/article.css");
	if (css_idx >= 0) {
		fs.refresh_file(css_idx);
		html.add_string((const char*)fs.files[css_idx].buffer, fs.files[css_idx].size);
	}

	css_idx = fs.lookup_file("client/blogs.css");
	if (css_idx >= 0) {
		fs.refresh_file(css_idx);
		html.add_string((const char*)fs.files[css_idx].buffer, fs.files[css_idx].size);
	}

	html.add_string("\n</style></head><body>\n", 0);

	html.add_string("<h1>Blogs</h1>", 0);

	html.add_string("<div id=\"articles\"><div class=\"article-column article-left\">", 0);

	for (int i = 0; i < blogs.size; i += 2)
		render_blog_preview(fs, html, blogs[blogs.size - i - 1]);

	html.add_string("</div><div class=\"article-column article-right\">", 0);

	for (int i = 1; i < blogs.size; i += 2)
		render_blog_preview(fs, html, blogs[blogs.size - i - 1]);

	html.add_string("</div></body></html>", 0);

	int html_size = 0;
	char *response = html.get(&html_size);
	write_http_response(request_fd, "200 OK", "text/html", response, html_size);
}

void serve_specific_blog(Filesystem& fs, int request_fd, char *name, int name_len)
{
	String path;
	path.add("content/blog/");
	path.add_and_escape(name, name_len);
	path.add(".jb");

	int fidx = fs.lookup_file(path.data());
	if (fidx < 0) {
		serve_404(fs, request_fd);
		return;
	}

	fs.refresh_file(fidx);
	FS_File& file = fs.files[fidx];

	Expander html;
	html.init(file.size * 2, false, 256);
	html.add_string("</title><style>\n", 0);

	int css_idx = fs.lookup_file("client/article.css");
	if (css_idx >= 0) {
		fs.refresh_file(css_idx);
		html.add_string((const char*)fs.files[css_idx].buffer, fs.files[css_idx].size);
	}

	html.add_string("\n</style></head><body>", 0);

	Space title_space = produce_article_html(html, (const char*)file.buffer, file.size, file.created_time, 0);

	html.add_string("</body></html>", 0);

	html.prepend_string_trunc((const char*)&file.buffer[title_space.offset], title_space.size);
	html.prepend_string_overlap("<!DOCTYPE html><html><head><title>", 0);

	int out_size = 0;
	char *out = html.get(&out_size);
	write_http_response(request_fd, "200 OK", "text/html", out, out_size);
}
