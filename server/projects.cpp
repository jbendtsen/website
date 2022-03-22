#include "website.h"

#define PROJECT_PREVIEW_LINE_LIMIT 20

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

void serve_specific_project(Filesystem& fs, int fd, char *name, int len) {
	Expander html;
	html.add("<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Projects</title><style>");

	fs.add_file_to_html(html, "client/banner.css");
	html.add("</style></head><body>");
	add_banner(fs, html, NAV_IDX_PROJECTS);

	html.add("<h1>Specific Project</h1></body></html>");

	int out_sz = 0;
	char *out = html.get(&out_sz);
	write_http_response(fd, "200 OK", "text/html", out, out_sz);
}
