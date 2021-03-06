#include "website.h"

#define BLOG_PREVIEW_LINE_LIMIT 20

static void render_blog_preview(Filesystem& fs, String *html, const char *class_name, int blog_idx)
{
	FS_File& file = fs.files[blog_idx];

	char *fname = fs.name_pool.at(file.name_idx);
	int name_len = strlen(fname) - 3;
	if (name_len <= 0) {
		html->add("<div class=\"article-preview\"><div class=\"preview-overlay\"></div></div>");
		return;
	}

	html->add("<a href=\"/blog/");
	html->add_and_escape(fname, name_len);
	if (class_name) {
		html->add("\" class=\"");
		html->add_and_escape(class_name);
	}
	html->add("\"><div class=\"article-preview\"><div class=\"preview-overlay\"></div>");

	//fs.refresh_file(blog_idx);

	html->add("<article class=\"blog\">");
	Markdown_Params md_params = {
		.created_time = file.created_time,
		.line_limit = BLOG_PREVIEW_LINE_LIMIT,
		.disable_anchors = true
	};
	Space title = produce_markdown_html(fs, *html, (const char*)file.buffer, file.size, md_params);
	html->add("</article>");

	html->add("</div></a>");
}

void serve_blog_overview(Filesystem& fs, Response& response)
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

	String *html = &response.html;
	html->reserve(2048);
	html->add("<!DOCTYPE html><html><head>" HTML_METAS "<title>Blogs</title><style>\n");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/blogs.css");

	html->add(
		"article h1 { font-size: 1.6rem; }\n"
		"article h2 { font-size: 1.25rem; }\n"
		"article h3 { font-size: 1rem; }\n"
		"article p  { font-size: 0.9rem;  }\n"
	);

	html->add("\n</style></head><body>\n");

	add_banner(fs, html, NAV_IDX_BLOG);

	html->add("<div id=\"articles\">");

	for (int i = 0; i < blogs.size; i++) {
		const char *kind = i % 2 ? "blog-right" : "blog-left";
		render_blog_preview(fs, html, kind, blogs[blogs.size - i - 1]);
	}

	html->add("</div></body></html>");
}

void serve_specific_blog(Filesystem& fs, Response& response, char *name, int name_len)
{
	String path;
	path.add("content/blog/");
	path.add_and_escape(name, name_len);
	path.add(".jb");

	int fidx = fs.lookup_file(path.data());
	if (fidx < 0) {
		serve_404(fs, response);
		return;
	}

	//fs.refresh_file(fidx);
	FS_File& file = fs.files[fidx];

	String *html = &response.html;
	html->reserve(2048);
	html->add("<!DOCTYPE html><html><head>" HTML_METAS);

	int title_dst_pos = html->len;
	html->add_chars(' ', 128);

	html->add("\n<style>\n");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");

	html->add(
		"article h1 { font-size: 2.5rem; }\n"
		"article h2 { font-size: 1.8rem; }\n"
		"article h3 { font-size: 1.4rem; }\n"
		"article p  { font-size: 1.2rem; }\n"
	);

	html->add("\n</style></head><body>");

	add_banner(fs, html, NAV_IDX_BLOG);

	html->add("<article class=\"blog\">");
	Markdown_Params md_params = {
		.created_time = file.created_time
	};
	Space title = produce_markdown_html(fs, *html, (const char*)file.buffer, file.size, md_params);
	html->add("</article>");

	html->add("</body></html>");

	title.size = title.size < 113 ? title.size : 113; // 128 - length("<title>") - length("</title>")
	char *title_dst = html->data() + title_dst_pos;

	strcpy(title_dst, "<title>");
	memcpy(title_dst + 7, (const char*)&file.buffer[title.offset], title.size);

	int title_end = 7 + title.size;
	if (title.size >= 110) {
		strcpy(title_dst + 117, "...");
		title_end = 120;
	}

	memcpy(title_dst + title_end, "</title>", 8);
}
