#include "website.h"

#define BLOG_PREVIEW_LINE_LIMIT 20

static void render_blog_preview(Filesystem& fs, Expander& html, int blog_idx)
{
	FS_File& file = fs.files[blog_idx];

	char *fname = fs.name_pool.at(file.name_idx);
	int name_len = strlen(fname) - 3;
	if (name_len <= 0) {
		html.add("<div class=\"article-preview\"><div class=\"preview-overlay\"></div></div>", 0);
		return;
	}

	html.add("<a href=\"/blog/", 0);
	html.add_and_escape(fname, name_len);
	html.add("\"><div class=\"article-preview\"><div class=\"preview-overlay\"></div>", 0);

	fs.refresh_file(blog_idx);

	html.add("<article class=\"blog\">");
	produce_article_html(html, (const char*)file.buffer, file.size, file.created_time, BLOG_PREVIEW_LINE_LIMIT);
	html.add("</article>");

	html.add("</div></a>", 0);
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
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Blogs</title><style>\n", 0);

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/blogs.css");

	html.add(
		"article h1 { font-size: 160%; }\n"
		"article h2 { font-size: 125%; }\n"
		"article h3 { font-size: 100%; }\n"
		"article p  { font-size: 90%;  }\n"
	);

	html.add("\n</style></head><body>\n", 0);

	add_banner(fs, html, NAV_IDX_BLOG);

	html.add("<div id=\"articles\"><div class=\"article-column article-left\">", 0);

	for (int i = 0; i < blogs.size; i += 2)
		render_blog_preview(fs, html, blogs[blogs.size - i - 1]);

	html.add("</div><div class=\"article-column article-right\">", 0);

	for (int i = 1; i < blogs.size; i += 2)
		render_blog_preview(fs, html, blogs[blogs.size - i - 1]);

	html.add("</div></body></html>", 0);

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
	html.add("</title><style>\n", 0);

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");

	html.add(
		"article h1 { font-size: 200%; }\n"
		"article h2 { font-size: 150%; }\n"
		"article h3 { font-size: 120%; }\n"
	);

	html.add("\n</style></head><body>", 0);

	add_banner(fs, html, NAV_IDX_BLOG);

	html.add("<article class=\"blog\">");
	Space title_space = produce_article_html(html, (const char*)file.buffer, file.size, file.created_time, 0);
	html.add("</article>");

	html.add("</body></html>", 0);

	html.prepend_string_trunc((const char*)&file.buffer[title_space.offset], title_space.size);
	html.prepend_string_overlap("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>", 0);

	int out_size = 0;
	char *out = html.get(&out_size);
	write_http_response(request_fd, "200 OK", "text/html", out, out_size);
}
