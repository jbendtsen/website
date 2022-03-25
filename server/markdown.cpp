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

static void tryhard(Expander& html, char *chbuf, const char *str, int line, int idx) {
	int i = 0;
	for ( ; i < 8 && chbuf[i]; i++) {
		char c = chbuf[i];
		if (c != '\n' && c != '\t' && (c < 0x20 || c >= 0x7f)) {
			log_info("Error on line {d} at position {d}\n", line, idx);
		}
	}
	if (i == 8) {
		log_info("Overflow on line {d} at position {d}\n", line, idx);
	}

	char c = html.buf[html.head-1];
	if (c != '\n' && c != '\t' && (c < 0x20 || c >= 0x7f))
		log_info("Non-ascii character before line {d} at position {d}\n", line, idx);

	html.add(str);

	c = html.buf[html.head-1];
	if (c != '\n' && c != '\t' && (c < 0x20 || c >= 0x7f))
		log_info("Non-ascii character after line {d} at position {d}\n", line, idx);
}

void produce_markdown_html(Expander& html, const char *input, int in_sz, int line_limit)
{
	html.reserve_extra(in_sz);

	int tag_levels[16] = {0};
	int tag_cursor = 0;
	int header_level = 0;
	int quote_level = 0;
	int code_level = 0;
	int list_level = 0;
	int consq_backticks = 0;
	int consq_asterisks = 0;
	int leading_spaces = 0;
	int nl_count = 0;
	int code_type = 0;
	bool started_line = false;
	bool is_italic = false;
	bool is_bold = false;
	bool was_esc = false;

	char chbuf[8];

	for (int i = 0; i < in_sz; i++) {
		char c = input[i];
		bool should_process = true;

		chbuf[0] = c;
		chbuf[1] = 0;

		if (!started_line && code_type != 1) {
			if (c == ' ') {
				leading_spaces++;
				should_process = false;
			}
			else if (c == '\t') {
				leading_spaces += 4;
				should_process = false;
			}
			else if (code_type == 0 && c == '>') {
				quote_level++;
				should_process = false;
			}
			else if (code_type == 0 && (c == '-' || c == '+')) {
				int new_list_level = (leading_spaces / 4) + 1;
				if (new_list_level > list_level) {
					for (int j = 0; j < new_list_level - list_level; j++) {
						tryhard(html, chbuf, "<ul>", __LINE__, i);
					}
				}
				else if (new_list_level < list_level) {
					for (int j = 0; j < list_level - new_list_level; j++) {
						tryhard(html, chbuf, "</ul>", __LINE__, i);
					}
				}
				list_level = new_list_level;
				started_line = true;
				should_process = false;
			}
			else {
				for (int j = 0; j < list_level; j++) {
					tryhard(html, chbuf, "</ul>", __LINE__, i);
				}

				list_level = 0;

				if (c != '#') {
					int prev_code_type = code_type;
					if (quote_level == 0 && list_level == 0 && leading_spaces >= 4) {
						code_type = 2;
					}
					else if (code_type == 2) {
						code_type = 0;
					}

					if (!prev_code_type && code_type) {
						tryhard(html, chbuf, "<code>", __LINE__, i);
					}
					else if (prev_code_type && !code_type) {
						tryhard(html, chbuf, "</code>", __LINE__, i);
					}

					started_line = true;
				}
			}

			if (c == '#') {
				header_level++;
				if (header_level > 6) {
					if (tag_cursor < 15)
						tag_cursor++;

					if (list_level > 0) {
						tag_levels[tag_cursor] = TAG_LI;
						tryhard(html, chbuf, "<li>", __LINE__, i);
					}
					else {
						tag_levels[tag_cursor] = TAG_P;
						tryhard(html, chbuf, "<p>", __LINE__, i);
					}
					tryhard(html, chbuf, "#######", __LINE__, i);
					started_line = true;
				}
				should_process = false;
			}
			else {
				if (tag_cursor < 15)
					tag_cursor++;

				if (header_level > 0 && header_level <= 6) {
					char tag[8];
					tag[0] = '<';
					tag[1] = 'h';
					tag[2] = '0' + header_level;
					tag[3] = '>';
					tag[4] = 0;
					tryhard(html, chbuf, tag, __LINE__, i);

					tag_levels[tag_cursor] = header_level;
				}
				else {
					if (list_level > 0) {
						tag_levels[tag_cursor] = TAG_LI;
						tryhard(html, chbuf, "<li>", __LINE__, i);
					}
					else {
						tag_levels[tag_cursor] = TAG_P;
						tryhard(html, chbuf, "<p>", __LINE__, i);
					}
				}

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
				tryhard(html, chbuf, "</code>", __LINE__, i);
				code_type = 0;
			}
			else if (code_type == 0 && consq_backticks) {
				code_level = consq_backticks;
				tryhard(html, chbuf, "<code>", __LINE__, i);
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
					tryhard(html, chbuf, "`", __LINE__, i);
				}

				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				tryhard(html, chbuf, chbuf, __LINE__, i);
			}
		}

		if (should_process) {
			bool should_add_c = true;
			if (c == '\n') {
				int prev_cursor = tag_cursor;

				const char *tag = closing_tag_strings[tag_levels[tag_cursor]];
				tag_cursor--;
				if (tag_cursor < 0) {
					tag_cursor = 0;
					tag_levels[0] = 0;
				}

				//log_info("prev_cursor={d}", prev_cursor);
				tryhard(html, chbuf, tag, __LINE__, i);
			}
			else if (!was_esc) {
				should_add_c = c != '\\' && c != '*' && c != '`';
			}

			if (should_add_c) {
				if (consq_asterisks & 1) {
					tryhard(html, chbuf, is_italic ? "</em>" : "<em>", __LINE__, i);
					is_italic = !is_italic;
				}
				if (consq_asterisks & 2) {
					tryhard(html, chbuf, is_bold ? "</strong>" : "<strong>", __LINE__, i);
					is_bold = !is_bold;
				}

				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				tryhard(html, chbuf, chbuf, __LINE__, i);
			}
		}

		if (c == '\n') {
			started_line = false;
			quote_level = 0;
			header_level = 0;
			leading_spaces = 0;

			nl_count++;
			if (line_limit > 0 && nl_count >= line_limit)
				break;
		}

		was_esc = c == '\\' && !was_esc;
	}
}
