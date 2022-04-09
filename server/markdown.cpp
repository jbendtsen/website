#include "website.h"

#define TAG_P  0
#define TAG_H1 1
#define TAG_H2 2
#define TAG_H3 3
#define TAG_H4 4
#define TAG_H5 5
#define TAG_H6 6
#define TAG_LI 7

static const char *closing_tag_strings[] = {
	"</p>",
	"</h1>",
	"</h2>",
	"</h3>",
	"</h4>",
	"</h5>",
	"</h6>",
	"</li>",
};

void produce_markdown_html(Expander& html, const char *input, int in_sz, const char *path, int line_limit)
{
	html.reserve_extra(in_sz);

	int tag_levels[16] = {0};
	int tag_cursor = 0;
	int header_level = 0;
	int quote_level = 0;
	int code_level = 0;
	int list_level = 0;
	int new_list_level = 0;
	int consq_backticks = 0;
	int consq_asterisks = 0;
	int leading_spaces = 0;
	int nl_count = 0;
	int prev_line_len = 0;
	int prev_nl_pos = 0;
	int code_type = 0;
	bool should_not_open_tag = false;
	bool started_line = false;
	bool is_italic = false;
	bool is_bold = false;
	bool was_esc = false;
	char prev = 0;

	char chbuf[8];

	for (int i = 0; i < in_sz; i++) {
		char c = input[i];
		bool should_process = true;

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
			else if (code_type == 0 && c == '>') {
				quote_level++;
			}
			else if (code_type == 0 && (c == '-' || c == '+')) {
				new_list_level = (leading_spaces / 4) + 1;
				started_line = true;
			}
			else if (code_type == 0 && c == '#') {
				header_level++;
				if (header_level > 6)
					started_line = true;
			}
			else if (c != '\n') {
				started_line = true;
				should_process = true;
			}

			if (started_line) {
				if (tag_cursor > 0) {
					int prev_cursor = tag_cursor;
					int tag = tag_levels[tag_cursor];
					if (tag == TAG_P && prev_line_len > 1) {
						html.add("<br>");
						should_not_open_tag = true;
					}
					else {
						const char *tag_str = closing_tag_strings[tag];
						tag_cursor--;
						if (tag_cursor < 0) {
							tag_cursor = 0;
							tag_levels[0] = 0;
						}

						//log_info("prev_cursor={d}", prev_cursor);
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

						tag_levels[tag_cursor] = header_level;
					}
					else {
						if (list_level > 0) {
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

		if (c == '`' && !was_esc) {
			// this avoids the case where a code block is formed using leading spaces, since code_level would equal 0
			consq_backticks++;
		}
		else {
			if (code_type == 1 && consq_backticks == code_level) {
				code_level = 0;
				html.add("</code>");
				code_type = 0;
			}
			else if (code_type == 0 && consq_backticks) {
				code_level = consq_backticks;
				html.add("<code>");
				code_type = 1;
			}
			consq_backticks = 0;
		}

		if (c == '*' && !was_esc) {
			consq_asterisks++;
		}
		else {
			consq_asterisks = 0;
		}

		if (code_type != 0) {
			should_process = false;

			if (c != '`') {
				char back = '`';
				for (int j = 0; j < consq_backticks; j++) {
					html.add("`");
				}

				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				html.add(chbuf);
			}
		}
		else if (!was_esc) {
			if (c == '!') {
				if (i < in_sz-1 && input[i+1] == '[')
					should_process = false;
			}
			else if (c == '[') {
				should_process = false;

				if (prev == '!')
					html.add("<img src=\"");
				else
					html.add("<a href=\"");

				int j = i+1;
				int alt = j;
				bool contains_colon = false;
				for (; j < in_sz && input[j] != ']'; j++) {
					if (input[j] == ':')
						contains_colon = true;
				}
				int alt_end = j;

				if (path && !contains_colon && input[alt] != '/') {
					if (path[0] != '/') html.add("/");
					html.add(path);
					html.add("/");
				}

				for (; j < in_sz && input[j] != '('; j++);
				int link = ++j;

				for (; j < in_sz && input[j] != ')'; j++);

				html.add_and_escape(&input[link], j - link);

				if (prev == '!')
					html.add("\" alt=\"");
				else
					html.add("\">");

				html.add_and_escape(&input[alt], alt_end - alt);

				if (prev == '!')
					html.add("\">");
				else
					html.add("</a>");

				i = j+1;
			}
		}

		if (should_process) {
			bool should_add_c = true;
			if (!was_esc) {
				should_add_c = c != '\\' && c != '\n' && c != '*' && c != '`';
			}

			if (should_add_c) {
				if (consq_asterisks & 1) {
					html.add(is_italic ? "</em>" : "<em>");
					is_italic = !is_italic;
				}
				if (consq_asterisks & 2) {
					html.add(is_bold ? "</strong>" : "<strong>");
					is_bold = !is_bold;
				}

				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				html.add(chbuf);
			}
		}

		if (c == '\n') {
			started_line = false;
			should_not_open_tag = false;
			quote_level = 0;
			header_level = 0;
			leading_spaces = 0;
			new_list_level = 0;

			prev_line_len = i - prev_nl_pos - 1;
			prev_nl_pos = i;

			nl_count++;
			if (line_limit > 0 && nl_count >= line_limit)
				break;
		}

		if (!code_type)
			was_esc = c == '\\' && !was_esc;
		else
			was_esc = false;

		prev = c;
	}

	while (tag_cursor > 0) {
		const char *tag = closing_tag_strings[tag_levels[tag_cursor]];
		tag_cursor--;

		html.add(tag);
	}
}
