#include "website.h"

#define PROJECT_PREVIEW_LINE_LIMIT 20

static void render_code_article(Expander& html, const char *input, int in_sz)
{
	html.add("<div class=\"file-code\"><code>");

	int start = 0;
	bool was_spec = false;
	char prev = 0;

	for (int i = 0; i < in_sz; i++) {
		char c = input[i];
		if (c == '\n' || c == '\t') {
			if (i > start) {
				if (was_spec)
					html.add(&input[start], i-start);
				else
					html.add_and_escape(&input[start], i-start);

				start = i;
				while (start < in_sz && (input[start] == '\n' || input[start] == '\t'))
					start++;
			}

			if (c == '\n') {
				if (prev == '\n') html.add_and_escape(&c, 1);
				html.add("</code><code>");
			}
			else if (c == '\t') {
				html.add("    ");
			}
		}
		else if (NEEDS_ESCAPE(c)) {
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

		prev = c;
	}

	if (start < in_sz) {
		if (was_spec)
			html.add_and_escape(&input[start], in_sz-start);
		else
			html.add(&input[start], in_sz-start);
	}

	html.add("</code></div></article>");
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
	int proj = fs.dirs[projects].first_dir.modified;

	String md_path;
	md_path.add("content/projects");

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
			proj = fs.dirs[proj].next.modified;
			continue;
		}

		html.add("<a class=\"proj-box\" href=\"/projects/");

		char *pname = fs.name_pool.at(fs.dirs[proj].name_idx);
		html.add_and_escape(pname, 0);
		html.add("\"><div class=\"proj-overlay\"></div>");

		md_path.add("/");
		md_path.add(pname);
		int pname_len = strlen(pname);

		fs.refresh_file(f);
		html.add("<article class=\"proj-md\">");
		produce_markdown_html(html, (const char*)fs.files[f].buffer, fs.files[f].size, md_path.data(), PROJECT_PREVIEW_LINE_LIMIT);
		html.add("</article></a>");

		md_path.scrub(pname_len + 1);
		proj = fs.dirs[proj].next.alpha;
	}

	html.add("</div></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}

static void add_directory_html(Expander& html, Filesystem& fs, int didx, char *path_buf, int path_len)
{
	if (didx < 0)
		return;

	html.add("<ul>");

	int d = fs.dirs[didx].first_dir.alpha;
	while (d > 0) {
		char *name = fs.name_pool.at(fs.dirs[d].name_idx);
		int name_len = strlen(name);

		// TODO: html.add("<details open>" instead of "<details>") if directory is ancestor of current file
		html.add("<details><summary class=\"proj-tree-item\">");
		html.add_and_escape(name, name_len);
		html.add("</summary>");

		memcpy(&path_buf[path_len], name, name_len);
		path_buf[path_len + name_len] = '/';

		add_directory_html(html, fs, d, path_buf, path_len + name_len + 1);

		html.add("</details>");

		d = fs.dirs[d].next.alpha;
	}

	int f = fs.dirs[didx].first_file.alpha;
	while (f > 0) {
		char *name = fs.name_pool.at(fs.files[f].name_idx);
		int name_len = strlen(name);

		html.add("<li class=\"proj-tree-item\"><a href=\"");
		html.add_and_escape(path_buf, path_len);
		html.add_and_escape(name);
		html.add("\">");
		html.add_and_escape(name);
		html.add("</a></li>");

		f = fs.files[f].next.alpha;
	}

	html.add("</ul>");
}

void serve_specific_project(Filesystem& fs, int fd, char *name, int name_len)
{
	String path;
	path.add("content/projects/");

	if (name_len > 4) {
		char *e = &name[name_len-4];
		if (e[0] == '.' && (e[1] == 'z' || e[1] == 'Z') && (e[2] == 'i' || e[2] == 'I') && (e[3] == 'p' || e[3] == 'P')) {
			path.add(name, name_len - 4);
			int dir_idx = fs.lookup_dir(path.data());
			if (dir_idx >= 0) {
				write_zip_to_socket(fs, dir_idx, fd);
				return;
			}
		}
	}

	Expander html;
	html.add("<!DOCTYPE html><html class=\"full\"><head><meta charset=\"UTF-8\"><title>");
	html.add_and_escape(name, name_len);
	html.add("</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/projects.css");

	html.add("</style></head><body class=\"full\">");
	html.add("<div id=\"proj-screen\">");
	add_banner(fs, html, NAV_IDX_PROJECTS);

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

	html.add("<div id=\"proj-page\"><div id=\"proj-sidebar\">");

	html.add("<h2 id=\"proj-name\"><a href=\"/projects/");
	html.add_and_escape(name, proj_len);
	html.add("\">");
	html.add_and_escape(name, proj_len);
	html.add("</a></h2>");

	html.add("<hr class=\"no-margin-top-bottom\">");
	html.add("<div id=\"proj-tree\">");

	char dir_path[1024];
	memcpy(dir_path, "/projects/", 10);
	memcpy(&dir_path[10], name, proj_len);
	dir_path[10+proj_len] = '/';
	dir_path[11+proj_len] = 0;

	add_directory_html(html, fs, proj_dir, dir_path, 11+proj_len);

	html.add("</div><hr />");
	html.add("<a id=\"proj-download\" href=\"/projects/");
	html.add_and_escape(name, proj_len);
	html.add(".zip\"><div class=\"button\">DOWNLOAD</div>");

	html.add("</div></a><div id=\"proj-main\">");

	if (path_start < name_len) {
		path.add('/');
		path.add(&name[path_start], name_len - path_start);
		int fidx = fs.lookup_file(path.data());

		if (fidx < 0) {
			html.add("<div id=\"proj-content\"><h1>Could not open file: ");
			html.add_and_escape(path.data(), path.len);
			html.add("</h1></div>");
		}
		else {
			html.add("<div id=\"proj-header\"><h3>");
			html.add_and_escape(fs.name_pool.at(fs.files[fidx].name_idx));
			html.add("</h3></div><div id=\"proj-content\"><article>");

			fs.refresh_file(fidx);
			render_code_article(html, (const char*)fs.files[fidx].buffer, fs.files[fidx].size);
			html.add("</article></div>");
		}
	}
	else {
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
			html.add("<div id=\"proj-content\"><article class=\"proj-md\">");

			fs.refresh_file(f);
			produce_markdown_html(html, (const char*)fs.files[f].buffer, fs.files[f].size, path.data(), 0);
			html.add("</article></div>");
		}
	}

	html.add("</div></div></div></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}
