#include "website.h"

#define PROJECT_PREVIEW_LINE_LIMIT 20

static void render_code_article(String *html, const char *input, int in_sz)
{
	html->add("<div class=\"file-code\"><code>");

	int start = 0;
	bool was_spec = false;
	char prev = 0;

	for (int i = 0; i < in_sz; i++) {
		char c = input[i];
		if (c == '\n' || c == '\t') {
			if (i > start) {
				if (was_spec)
					html->add(&input[start], i-start);
				else
					html->add_and_escape(&input[start], i-start);

				start = i;
				while (start < in_sz && (input[start] == '\n' || input[start] == '\t'))
					start++;
			}

			if (c == '\n') {
				if (prev == '\n') html->add_and_escape(&c, 1);
				html->add("</code><code>");
			}
			else if (c == '\t') {
				html->add("    ");
			}
		}
		else if (NEEDS_ESCAPE(c)) {
			if (!was_spec && i > start) {
				html->add(&input[start], i-start);
				start = i;
			}
			was_spec = true;
		}
		else {
			if (was_spec && i > start) {
				html->add_and_escape(&input[start], i-start);
				start = i;
			}
			was_spec = false;
		}

		prev = c;
	}

	if (start < in_sz) {
		if (was_spec)
			html->add_and_escape(&input[start], in_sz-start);
		else
			html->add(&input[start], in_sz-start);
	}

	html->add("</code></div></article>");
}

void serve_projects_overview(Filesystem& fs, Response& response)
{
	String *html = &response.html;
	html->reserve(2048);

	html->add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Projects</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/projects.css");

	html->add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	html->add("<div id=\"projects\">");

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

		html->add("<a class=\"proj-box\" href=\"/projects/");

		char *pname = fs.name_pool.at(fs.dirs[proj].name_idx);
		html->add_and_escape(pname, 0);
		html->add("\"><div class=\"proj-overlay\"></div>");

		md_path.add("/");
		md_path.add(pname);
		int pname_len = strlen(pname);

		//fs.refresh_file(f);
		html->add("<article class=\"proj-md\">");
		Markdown_Params md_params = {
			.path = md_path.data(),
			.line_limit = PROJECT_PREVIEW_LINE_LIMIT,
			.disable_anchors = true
		};
		produce_markdown_html(*html, (const char*)fs.files[f].buffer, fs.files[f].size, md_params);
		html->add("</article></a>");

		md_path.scrub(pname_len + 1);
		proj = fs.dirs[proj].next.modified;
	}

	html->add("</div></body></html>");
}

static void add_directory_html(String *html, Filesystem& fs, int didx, char *path_buf, int path_len, bool raw_url)
{
	if (didx < 0)
		return;

	html->add("<ul>");

	int d = fs.dirs[didx].first_dir.alpha;
	while (d > 0) {
		char *name = fs.name_pool.at(fs.dirs[d].name_idx);
		int name_len = strlen(name);

		// TODO: html->add("<details open>" instead of "<details>") if directory is ancestor of current file
		html->add("<details><summary class=\"proj-tree-item\">");
		html->add_and_escape(name, name_len);
		html->add("</summary>");

		memcpy(&path_buf[path_len], name, name_len);
		path_buf[path_len + name_len] = '/';

		add_directory_html(html, fs, d, path_buf, path_len + name_len + 1, raw_url);

		html->add("</details>");

		d = fs.dirs[d].next.alpha;
	}

	int f = fs.dirs[didx].first_file.alpha;
	while (f > 0) {
		char *name = fs.name_pool.at(fs.files[f].name_idx);
		int name_len = strlen(name);

		html->add("<li class=\"proj-tree-item\"><a href=\"");

		int pos = 9;
		html->add_and_escape(path_buf, pos);
		if (raw_url)
			html->add("-raw");
		if (path_len > pos)
			html->add_and_escape(path_buf + pos, path_len - pos);

		html->add_and_escape(name);
		html->add("\">");
		html->add_and_escape(name);
		html->add("</a></li>");

		f = fs.files[f].next.alpha;
	}

	html->add("</ul>");
}

void serve_specific_project(Filesystem& fs, Response& response, char *project_type, int slash_pos, int name_len)
{
	char dir_path[1024];
	String path(dir_path, 1024);
	path.add("content/projects/");

	char *name = &project_type[slash_pos + 1];

	bool show_raw_file = slash_pos == 12 && project_type[8] == '-' && project_type[9] == 'r' && project_type[10] == 'a' && project_type[11] == 'w';

	if (name_len > 4) {
		char *e = &name[name_len-4];
		if (e[0] == '.' && (e[1] == 'z' || e[1] == 'Z') && (e[2] == 'i' || e[2] == 'I') && (e[3] == 'p' || e[3] == 'P')) {
			path.add(name, name_len - 4);
			int dir_idx = fs.lookup_dir(path.data());
			if (dir_idx >= 0) {
				write_zip_as_response(fs, dir_idx, response);
				return;
			}
			path.scrub(name_len - 4);
		}
	}

	int proj_len = 0;
	while (proj_len < name_len && name[proj_len] && name[proj_len] != '/')
		proj_len++;

	int path_start = proj_len;
	while (path_start < name_len && name[path_start] && name[path_start] == '/')
		path_start++;

	path.add(name, proj_len);
	path.add('/');
	int proj_dir = fs.lookup_dir(path.data());

	if (proj_dir < 0) {
		serve_404(fs, response);
		return;
	}

	String *html = &response.html;
	html->reserve(2048);

	html->add("<!DOCTYPE html><html class=\"full\"><head><meta charset=\"UTF-8\"><title>");
	html->add_and_escape(name, name_len);
	html->add("</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	fs.add_file_to_html(html, "client/projects.css");

	html->add("</style></head><body class=\"full\">");
	html->add("<div id=\"proj-screen\">");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	html->add("<div id=\"proj-page\"><div id=\"proj-sidebar\">");

	html->add("<h2 id=\"proj-name\"><a href=\"/projects/");
	html->add_and_escape(name, proj_len);
	html->add("\">");
	html->add_and_escape(name, proj_len);
	html->add("</a></h2>");

	html->add("<hr class=\"no-margin-top-bottom\">");
	html->add("<div id=\"proj-tree\">");

	const int proj_root = 7; // skips past "content" (length 7), starts at a '/'
	add_directory_html(html, fs, proj_dir, path.data() + proj_root, 11+proj_len, show_raw_file);
	path.data()[path.len] = 0;

	html->add("</div><hr />");
	html->add("<a id=\"proj-download\" href=\"/projects/");
	html->add_and_escape(name, proj_len);
	html->add(".zip\"><div class=\"button\">DOWNLOAD</div>");

	html->add("</div></a><div id=\"proj-main\">");

	if (path_start < name_len) {
		path.add(&name[path_start], name_len - path_start);
		int fidx = fs.lookup_file(path.data());

		if (fidx < 0) {
			html->add("<div id=\"proj-content\"><h1>Could not open file: ");
			html->add_and_escape(path.data(), path.len);
			html->add("</h1></div>");
		}
		else {
			HTML_Type file_type = { NULL };

			if ((fs.files[fidx].flags & FS_FLAG_ASCII) == 0) {
				char *fname = fs.name_pool.at(fs.files[fidx].name_idx);
				int len = strlen(fname);
				char *p = &fname[len-1];
				while (p > fname && *p != '.')
					p--;

				file_type = lookup_ext(p);
			}

			html->add("<div id=\"proj-header\"><div id=\"file-hdr-margin\">");

			if (file_type.tag) {
				html->add("<a id=\"file-hdr-type-select\" href=\"");
				html->add(show_raw_file ? "/projects/" : "/projects-raw/");
				html->add_and_escape(name, name_len);
				html->add("\"><div class=\"button\">");
				html->add(show_raw_file ? "blob" : "raw");
				html->add("</div></a>");
			}

			html->add("</div><h3>");
			html->add_and_escape(&name[path_start], name_len - path_start);
			html->add("</h3></div>");

			if (!show_raw_file && file_type.tag) {
				const char *blob_type = "outer";
				bool use_inner = !strings_match(file_type.tag, "iframe", 6);
				if (use_inner) {
					html->add("<div id=\"proj-blob-outer\">");
					blob_type = file_type.tag;
				}
				html->add("<");
				html->add(file_type.tag);
				html->add(" id=\"proj-blob-");
				html->add(blob_type);
				html->add("\" src=\"/");
				html->add_and_escape(path.data());
				html->add("\"></");
				html->add(file_type.tag);
				html->add(">");
				if (use_inner)
					html->add("</div>");
			}
			else {
				html->add("<div id=\"proj-content\">");
				render_code_article(html, (const char*)fs.files[fidx].buffer, fs.files[fidx].size);
				html->add("</div>");
			}
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
			html->add("<div id=\"proj-content\"><article class=\"proj-md\">");

			Markdown_Params md_params = {
				.path = path.data()
			};
			produce_markdown_html(*html, (const char*)fs.files[f].buffer, fs.files[f].size, md_params);
			html->add("</article></div>");
		}
	}

	html->add("</div></div></div></body></html>");
}
