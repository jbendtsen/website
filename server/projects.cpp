#include "website.h"

#define PROJECT_PREVIEW_LINE_LIMIT 20

static void render_code_article(Expander& html, const char *input, int in_sz)
{
	html.add("<article><div class=\"code\">");

	int start = 0;
	bool was_spec = false;
	for (int i = 0; i < in_sz; i++) {
		char c = input[i];
		if (NEEDS_ESCAPE(c)) {
			if (!was_spec && i > start) {
				html.add(&input[start], i-start);
				start = i;
			}
			was_spec = true;
		}
		else {
			if (was_spec && i > start) {
				html.add_and_escape(&input[start], i-start);
				start = i;
			}
			was_spec = false;
		}
	}

	if (was_spec)
		html.add_and_escape(&input[start], in_sz-start);
	else
		html.add(&input[start], in_sz-start);

	html.add("</div></article>");
}

void produce_markdown_html(Expander& html, const char *input, int in_sz, int line_limit)
{
	render_code_article(html, input, in_sz);
}

void serve_projects_overview(Filesystem& fs, int fd)
{
	Expander html;
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Projects</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/projects.css");

	html.add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	html.add("<div id=\"projects\">");

	int projects = fs.lookup_dir("content/projects");
	int proj = fs.dirs[projects].first_dir.alpha;

	while (proj >= 0) {
		int f = fs.dirs[proj].first_file.alpha;
		while (f >= 0) {
			FS_File& file = fs.files[f];
			char *fname = fs.name_pool.at(file.name_idx);

			if (!caseless_match(fname, "readme.md")) {
				f = file.next.alpha;
				continue;
			}
			break;
		}
		if (f < 0) {
			proj = fs.dirs[proj].next.alpha;
			continue;
		}

		html.add("<a class=\"proj-box\" href=\"/projects/");

		char *pname = fs.name_pool.at(fs.dirs[proj].name_idx);
		html.add_and_escape(pname, 0);
		html.add("\"><div class=\"proj-overlay\"></div>");

		fs.refresh_file(f);
		produce_markdown_html(html, (const char*)fs.files[f].buffer, fs.files[f].size, PROJECT_PREVIEW_LINE_LIMIT);
		html.add("</a>");

		proj = fs.dirs[proj].next.alpha;
	}

	html.add("</div></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}

static void render_readme(Expander& html, Filesystem& fs, int proj_dir)
{
	int f = fs.dirs[proj_dir].first_file.alpha;
	while (f >= 0) {
		FS_File& file = fs.files[f];
		char *fname = fs.name_pool.at(file.name_idx);

		if (!caseless_match(fname, "readme.md")) {
			f = file.next.alpha;
			continue;
		}
		break;
	}
	if (f >= 0) {
		fs.refresh_file(f);
		produce_markdown_html(html, (const char*)fs.files[f].buffer, fs.files[f].size, 0);
	}
}

static void add_directory_html(Expander& html, Filesystem& fs, int didx)
{
	
}

void serve_specific_project(Filesystem& fs, int fd, char *name, int name_len)
{
	Expander html;
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>");
	html.add_and_escape(name, name_len);
	html.add("</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/projects.css");

	html.add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	String path;
	path.add("content/projects/");

	int proj_len = 0;
	while (proj_len < name_len && name[proj_len] && name[proj_len] != '/')
		proj_len++;

	int path_start = proj_len;
	while (path_start < name_len && name[path_start] && name[path_start] == '/')
		path_start++;

	path.add(name, proj_len);
	int proj_dir = fs.lookup_dir(path.data());

	if (proj_dir < 0) {
		serve_404(fs, fd);
		return;
	}

	html.add("<div id=\"proj-screen\"><div id=\"proj-sidebar\"><p>lol</p>");

	html.add("</div><div id=\"proj-content\">");

	if (path_start < name_len) {
		path.add('/');
		path.add(&name[path_start], name_len - path_start);
		int fidx = fs.lookup_file(path.data());

		if (fidx < 0) {
			html.add("<article><h1>Could not open file: ");
			html.add_and_escape(path.data(), path.len);
			html.add("</h1></article>");
		}
		else {
			fs.refresh_file(fidx);
			render_code_article(html, (const char*)fs.files[fidx].buffer, fs.files[fidx].size);
		}
	}
	else {
		render_readme(html, fs, proj_dir);
	}

	html.add("</div></div></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}
