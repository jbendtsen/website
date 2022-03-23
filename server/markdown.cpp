#include "website.h"

static const char *closing_tag_strings[] = {
	"</p>",
	"</h1>",
	"</h2>",
	"</h3>",
	"</h4>",
	"</h5>",
	"</h6>",
};

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
	bool in_code = false;
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

		if (in_code) {
			should_process = false;

			if (c != '`') {
				char back = '`';
				for (int j = 0; j < consq_backticks; j++)
					html.add(&back, 1);

				if (NEEDS_ESCAPE(c))
					write_escaped_byte(c, chbuf);

				html.add(chbuf);
			}
		}
		else if (!started_line) {
			if (c == ' ') {
				leading_spaces++;
				should_process = false;
			}
			else if (c == '\t') {
				leading_spaces += 4;
				should_process = false;
			}
			else if (c == '>') {
				quote_level++;
				should_process = false;
			}
			else if (c == '-' || c == '+') {
				int new_list_level = (leading_spaces / 4) + 1;
				if (new_list_level > list_level) {
					for (int i = 0; i < new_list_level - list_level; i++)
						html.add("<ul>");
				}
				else if (new_list_level < list_level) {
					for (int i = 0; i < list_level - new_list_level; i++)
						html.add("</ul>");
				}
				list_level = new_list_level;
				started_line = true;
				should_process = false;
			}
			else {
				for (int i = 0; i < list_level; i++)
					html.add("</ul>");

				list_level = 0;

				if (c != '#') {
					if (leading_spaces >= 4)
						in_code = true;

					started_line = true;
					should_process = false;
				}
			}

			if (c == '#') {
				header_level++;
				if (header_level > 6) {
					if (tag_cursor < 15)
						tag_cursor++;

					tag_levels[tag_cursor] = 0;
					html.add("<p>#######");
					started_line = true;
					should_process = false;
				}
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
					html.add(tag);

					tag_levels[tag_cursor] = header_level;
				}
				else {
					html.add("<p>");
					tag_levels[tag_cursor] = 0;
				}

				header_level = 0;
			}
		}

		if (should_process) {
			bool should_add_c = true;
			if (c == '\n') {
				const char *tag = closing_tag_strings[tag_cursor];
				tag_cursor--;
				if (tag_cursor < 0) {
					tag_cursor = 0;
					tag_levels[0] = 0;
				}

				html.add(tag);
			}
			else if (!was_esc) {
				should_add_c = c != '\\' && c != '*' && c != '`';
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

		if (c == '`' && !was_esc) {
			// this avoids the case where a code block is formed using leading spaces, since code_level would equal 0
			consq_backticks++;
			if (consq_backticks == code_level) {
				code_level = 0;
				in_code = false;
			}
		}
		else {
			if (consq_backticks) {
				code_level = consq_backticks;
				in_code = true;
			}
			consq_backticks = 0;
		}

		if (c == '*' && !was_esc) {
			consq_asterisks++;
		}
		else {
			consq_asterisks = 0;
		}

		if (c == '\n') {
			started_line = false;
			quote_level = 0;
			header_level = 0;
			leading_spaces = 0;
		}

		was_esc = c == '\\' && !was_esc;
	}
}
