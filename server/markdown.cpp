#include "website.h"
#include <ctime>

#define TAG_P  0
#define TAG_H1 1
#define TAG_H2 2
#define TAG_H3 3
#define TAG_H4 4
#define TAG_H5 5
#define TAG_H6 6
#define TAG_LI 7
#define TAG_TR 8
#define TAG_TD 9

static const char *closing_tag_strings[] = {
	"</p>\n",
	"</h1>\n",
	"</h2>\n",
	"</h3>\n",
	"</h4>\n",
	"</h5>\n",
	"</h6>\n",
	"</li>\n",
	"</tr>\n",
	"</td>\n"
};

static const char *months[] = {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
};

Space produce_markdown_html(String& html, const char *input, int in_sz, Markdown_Params& md_params)
{
	Space title = {0};
	html.reserve(html.len + in_sz);

	int tag_levels[16] = {0};
	int tag_cursor = 0;
	int tag_of_prev_line = 0;
	int header_level = 0;
	int quote_level = 0;
	int code_level = 0;
	int list_level = 0;
	int new_list_level = 0;
	int consq_backticks = 0;
	int consq_tildes = 0;
	int consq_asterisks = 0;
	int level_asterisks = 0;
	int consq_underscores = 0;
	int level_underscores = 0;
	int leading_spaces = 0;
	int nl_count = 0;
	int prev_line_len = 0;
	int prev_nl_pos = 0;
	int code_type = 0;
	int table_mode = 0;
	int new_table_mode = 0;
	int table_col_idx = 0;
	bool ignore_underscores = false;
	bool should_not_open_tag = false;
	bool started_line = false;
	bool is_strikethrough = false;
	bool was_esc = false;
	char prev = 0;

	char chbuf[8];

	for (int i = 0; i < in_sz; i++) {
		char c = input[i];
		bool should_process = true;
		bool is_list_asterisk = false;

		chbuf[0] = c;
		chbuf[1] = 0;

		if (!started_line && code_type != 1) {
			should_process = false;
			if (c == ' ') {
				leading_spaces++;
			}
			else if (c == '\t') {
				leading_spaces += 4;
			}
			else if (code_type == 0) {
				if (c == '>') {
					quote_level++;
				}
				else if (c == '#') {
					header_level++;
					if (header_level > 6)
						started_line = true;
				}
				else if (c == '|') {
					should_process = false;
					bool is_sep = false;
					int j = i;
					for (; j < in_sz; j++) {
						if (input[j] == '\n')
							break;
						if (input[j] != '|' && input[j] != ' ' && input[j] != '-' && input[j] != '+' && input[j] != ':')
							break;
						if (input[j] == '-')
							is_sep = true;
					}
					if (is_sep || j >= in_sz) {
						i = j;
						c = '\n';
						should_not_open_tag = true;
					}

					new_table_mode = table_mode ? table_mode : 1;
					new_table_mode = is_sep ? 2 : new_table_mode;
					started_line = true;
				}
				else if (c == '-' || c == '+' || c == '*') {
					if (i < in_sz-1 && input[i+1] == ' ') {
						new_list_level = (leading_spaces / 4) + 1;
						is_list_asterisk = c == '*';
					}
					else if (i < in_sz-2 && input[i+1] == c && input[i+2] == c) {
						html.add("<hr>");
						should_process = false;
						while (i < in_sz && input[i] != '\n')
							i++;
						c = '\n';
					}
					started_line = true;
				}
				else if (c != '\n') {
					started_line = true;
					should_process = true;
				}
			}
			else if (c != '\n') {
				started_line = true;
				should_process = true;
			}

			if (started_line) {
				if (tag_cursor > 0) {
					int prev_cursor = tag_cursor;
					int tag = tag_levels[tag_cursor];
					if (tag == TAG_P && tag_of_prev_line == TAG_P && !header_level && !new_list_level && !new_table_mode && prev_line_len > 1) {
						html.add("&nbsp;");
						should_not_open_tag = true;
					}
					else {
						const char *tag_str = closing_tag_strings[tag];
						tag_cursor--;
						if (tag_cursor < 0) {
							tag_cursor = 0;
							tag_levels[0] = 0;
						}

						html.add(tag_str);
					}
				}

				if (new_list_level > list_level) {
					for (int j = 0; j < new_list_level - list_level; j++) {
						html.add("<ul>");
					}
				}
				else if (new_list_level < list_level) {
					for (int j = 0; j < list_level - new_list_level; j++) {
						html.add("</ul>");
					}
				}
				list_level = new_list_level;

				int prev_code_type = code_type;
				if (quote_level == 0 && list_level == 0 && leading_spaces >= 4) {
					code_type = 2;
				}
				else if (code_type == 2) {
					code_type = 0;
				}

				if (!prev_code_type && code_type) {
					html.add("<code>");
				}
				else if (prev_code_type && !code_type) {
					html.add("</code>");
				}

				if (new_table_mode != table_mode) {
					if (table_mode == 0) {
						html.add("<table>");
						html.add(new_table_mode == 1 ? "<thead>" : "<tbody>");
					}
					else {
						if (table_mode == 1) {
							html.add("</thead>");
							if (new_table_mode)
								html.add("<tbody>");
						}
						else {
							if (new_table_mode == 1)
								html.add("</tbody><thead>");
							else if (!new_table_mode)
								html.add("</tbody>");
						}
						if (!new_table_mode)
							html.add("</table>");
					}
					table_mode = new_table_mode;
				}

				if (!should_not_open_tag) {
					if (tag_cursor < 15)
						tag_cursor++;

					if (header_level > 0 && header_level <= 6) {
						char tag[8];
						tag[0] = '<';
						tag[1] = 'h';
						tag[2] = '0' + header_level;
						tag[3] = '>';
						tag[4] = 0;
						html.add(tag);

						if (!title.offset)
							title.offset = i;

						tag_levels[tag_cursor] = header_level;
					}
					else {
						if (table_mode) {
							tag_levels[tag_cursor] = TAG_TR;
							html.add("<tr>");
							html.add("<td>");
						}
						else if (list_level > 0) {
							tag_levels[tag_cursor] = TAG_LI;
							html.add("<li>");
						}
						else {
							tag_levels[tag_cursor] = TAG_P;
							html.add("<p>");
						}
					}
				}

				if (header_level > 6)
					html.add("#######");

				header_level = 0;
			}
		}

		if (code_type == 0) {
			int prev_em = level_asterisks + level_underscores;

			if (c == '*' && !was_esc && !is_list_asterisk) {
				consq_asterisks++;
				should_process = false;
			}
			else if (consq_asterisks) {
				level_asterisks = level_asterisks ? 0 : consq_asterisks;
				consq_asterisks = 0;
			}

			if (c == '_' && !was_esc) {
				if ((prev >= '0' && prev <= '9') || (prev >= 'A' && prev <= 'Z') || (prev >= 'a' && prev <= 'z'))
					ignore_underscores = !level_underscores;

				if (!ignore_underscores) {
					consq_underscores++;
					should_process = false;
				}
			}
			else {
				if (consq_underscores) {
					level_underscores = level_underscores ? 0 : consq_underscores;
					consq_underscores = 0;
				}
				ignore_underscores = false;
			}

			int diff_em = (level_asterisks + level_underscores) - prev_em;
			if (diff_em < -2) diff_em = -2;
			if (diff_em > 2)  diff_em = 2;

			const char *em_tags[] = { "</strong>", "</em>", "", "<em>", "<strong>" };
			html.add(em_tags[diff_em + 2]);

			if (c == '~' && !was_esc) {
				consq_tildes++;
				should_process = false;
			}
			else {
				if (consq_tildes == 2) {
					is_strikethrough = !is_strikethrough;
					html.add(is_strikethrough ? "<s>" : "</s>");
				}
				consq_tildes = 0;
			}

			if (c == '|' && table_mode) {
				if (table_col_idx)
					html.add("</td><td>");

				table_col_idx++;
				should_process = false;
			}
		}

		int prev_backticks = consq_backticks;

		if (c == '`' && !was_esc) {
			// this avoids the case where a code block is formed using leading spaces, since code_level would equal 0
			consq_backticks++;
			should_process = false;
		}
		else {
			if (code_type == 1 && consq_backticks == code_level) {
				code_level = 0;
				html.add(consq_backticks >= 3 ? "</div>" : "</code>");
				code_type = 0;
			}
			else if (code_type == 0 && consq_backticks) {
				code_level = consq_backticks;
				html.add(consq_backticks >= 3 ? "<div class=\"code block\">" : "<code>");
				code_type = 1;
			}
			consq_backticks = 0;
		}

		if (code_type != 0) {
			should_process = false;

			if (c != '`' && !(prev_backticks > 0 && c == '\n')) {
				for (int j = 0; j < consq_backticks; j++)
					html.add("`");

				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				html.add(chbuf);
			}
		}
		else if (!was_esc) {
			if (c == '!' || c == '?') {
				if (i < in_sz-1 && input[i+1] == '[')
					should_process = false;
			}
			else if (c == '[') {
				should_process = false;

				if (prev == '!')
					html.add("<img src=\"");
				else if (prev == '?')
					html.add("<span ");
				else if (md_params.disable_anchors)
					html.add("<span href=\"");
				else
					html.add("<a href=\"");

				int j = i+1;
				int alt = j;
				bool alt_contains_colon = false;
				for (; j < in_sz && input[j] != ']'; j++) {
					if (input[j] == ':')
						alt_contains_colon = true;
				}
				int alt_end = j;

				for (; j < in_sz && input[j] != ']'; j++);

				int link = alt;
				int link_end = alt_end;
				bool link_contains_colon = alt_contains_colon;
				if (j < in_sz-1 && input[j+1] == '(') {
					link_contains_colon = false;
					link = j + 2;

					for (; j < in_sz && input[j] != ')'; j++) {
						if (input[j] == ':')
							link_contains_colon = true;
					}
					link_end = j;
					for (; j < in_sz && input[j] != ')'; j++);
				}

				if (prev != '?') {
					if (md_params.path && !link_contains_colon && input[link] != '/') {
						if (md_params.path[0] != '/') html.add("/");
						html.add(md_params.path);
						html.add("/");
					}

					if (link_end > link)
						html.add_and_escape(&input[link], link_end - link);
				}

				if (link <= alt) {
					html.add("\">");
					if (prev != '!' && prev != '?')
						html.add_and_escape(&input[link], link_end - link);
				}
				else if (prev == '?') {
					if (input[alt] == '=' && alt_end - alt > 1) {
						html.add("class=\"");
						alt++;
					}
					else if (alt_contains_colon) {
						html.add("style=\"");
					}
					html.add_and_escape(&input[alt], alt_end - alt);
					html.add("\">");
					html.add_and_escape(&input[link], link_end - link);
				}
				else {
					if (prev == '!')
						html.add("\" alt=\"");
					else
						html.add("\">");

					if (alt_end > alt) {
						html.add_and_escape(&input[alt], alt_end - alt);
					}

					if (prev == '!') {
						if (input[alt] == '=' && alt_end - alt > 1) {
							html.add("\" class=\"");
							alt++;
							html.add_and_escape(&input[alt], alt_end - alt);
						}
						else if (alt_contains_colon) {
							html.add("\" style=\"");
							html.add_and_escape(&input[alt], alt_end - alt);
						}
						html.add("\">");
					}
				}

				if (prev == '?')
					html.add("</span>");
				else if (prev != '!')
					html.add(md_params.disable_anchors ? "</span>" : "</a>");

				i = j;
			}
		}

		if (should_process) {
			bool should_add_c = true;
			if (!was_esc) {
				should_add_c = c != '\\' && c != '\n';
			}

			if (should_add_c) {
				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				html.add(chbuf);
			}
		}

		if (c == '\n') {
			if (header_level > 0) {
				while (tag_cursor > 0) {
					int tag = tag_levels[tag_cursor];
					const char *tag_str = closing_tag_strings[tag];
					tag_cursor--;

					html.add(tag_str);
					if (tag >= 1 && tag <= 6)
						break;
				}
			}

			if (header_level == 1 && !title.size) {
				title.size = i - title.offset;
			}
			else if (header_level == 2 && md_params.created_time) {
				struct tm *date = localtime(&md_params.created_time);
				int day_kind = 0;
				int day_mod10 = date->tm_mday % 10;
				if (day_mod10 < 4 && (date->tm_mday < 4 || date->tm_mday > 20))
					day_kind = day_mod10;

				const char *kinds[] = {"th", "st", "nd", "rd"};
				char datebuf[128];
				String date_str(datebuf, 128);
				date_str.reformat(
					"<em>{d}{s} {s}, {d}</em>",
					date->tm_mday, kinds[day_kind], months[date->tm_mon], 1900 + date->tm_year
				);

				html.add(datebuf);
				md_params.created_time = 0;
			}

			if (table_mode) {
				while (tag_cursor > 0) {
					int tag = tag_levels[tag_cursor];
					const char *tag_str = closing_tag_strings[tag];
					tag_cursor--;

					if (tag == TAG_TR) {
						html.add("</td></tr>");
						break;
					}
					html.add(tag_str);
				}
			}

			header_level = 0;
			tag_of_prev_line = tag_levels[tag_cursor];
			started_line = false;
			should_not_open_tag = false;
			quote_level = 0;
			leading_spaces = 0;
			new_list_level = 0;
			new_table_mode = 0;
			table_col_idx = 0;

			prev_line_len = i - prev_nl_pos - 1;
			prev_nl_pos = i;

			nl_count++;
			if (md_params.line_limit > 0 && nl_count >= md_params.line_limit)
				break;
		}

		if (!code_type)
			was_esc = c == '\\' && !was_esc;
		else
			was_esc = false;

		prev = c;
	}

	if (code_type)
		html.add("</code>\n");
	if (is_strikethrough)
		html.add("</s>\n");

	int em_level = level_asterisks + level_underscores;
	if (em_level == 1 || em_level == 3)
		html.add("</em>\n");
	if (em_level == 2 || em_level == 3)
		html.add("</strong>\n");

	while (tag_cursor > 0) {
		const char *tag = closing_tag_strings[tag_levels[tag_cursor]];
		tag_cursor--;

		html.add(tag);
	}

	if (table_mode) {
		html.add(table_mode == 1 ? "</thead>" : "</tbody>");
		html.add("</table>");
	}

	return title;
}

void serve_markdown_tester(Filesystem& fs, Request& request, Response& response)
{
	bool allow_html = false;
	if (request.accept > 0) {
		char *p = request.str.data() + request.accept;
		while (*p && *p != '\r' && *p != '\n') {
			if (p[0] == 'h' && p[1] == 't' && p[2] == 'm' && p[3] == 'l') {
				allow_html = true;
				break;
			}
			p++;
		}
	}

	if (!allow_html) {
		const char *md_text = request.str.data() + request.header_size;
		int md_len = request.str.len - request.header_size;

		response.mime = "text/plain";
		Markdown_Params md_params = {0};
		produce_markdown_html(response.html, md_text, md_len, md_params);
		return;
	}

	String *html = &response.html;
	html->add(
		"<!DOCTYPE html><html class=\"full\"><head>"
		"<meta charset=\"UTF-8\">"
		"<title>Markdown Editor</title><style>\n"
	);

	fs.add_file_to_html(html, "client/banner.css");
	fs.add_file_to_html(html, "client/article.css");
	html->add(
		"article h1 { font-size: 200%; }\n"
		"article h2 { font-size: 150%; }\n"
		"article h3 { font-size: 120%; }\n"
	);
	fs.add_file_to_html(html, "client/md-editor.css");
	html->add(
		"canvas { image-rendering: crisp-edges; }\n"
	);

	html->add("</style><script>");
	fs.add_file_to_html(html, "client/code-editor.js");
	fs.add_file_to_html(html, "client/md-editor.js");

	html->add(
		"</script></head>"
		"<body class=\"full\" onload=\"load_code_editors(); setup_mdedit_editor();\""
			"onmousemove=\"global_mouse_handler(event)\" onmouseup=\"stop_dragging()\">"
		"<div class=\"full flex-column\">"
	);

	add_banner(fs, html, NAV_IDX_NULL);

	html->add(
		"<div id=\"mdedit-main\">"
			"<div id=\"mdedit-left\">"
				"<p>Editor</p>"
				"<div id=\"mdedit-editor\" class=\"mdedit-container code-editor\" data-listener=\"mdedit_listener\"></div>"
			"</div>"
			"<div id=\"mdedit-separator\" onmousedown=\"start_dragging()\"></div>"
			"<div id=\"mdedit-right\">"
				"<p>Preview</p>"
				"<div id=\"mdedit-preview-container\" class=\"mdedit-container\">"
					"<article id=\"mdedit-preview\"></article>"
				"</div>"
			"</div>"
		"</div>"
	);

	html->add("</div></body></html>");
}
