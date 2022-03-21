#include <cstdio>
#include <cstdlib>
#include <ctime>
#include "website.h"

#define CMD_BLOCK     1
#define CMD_TITLE     2
#define CMD_SUBTITLE  3

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

const char *closing_tag_strings[] = {
	"</div>",
	"</h1>",
	"</h2>",
};

Space produce_article_html(Expander& article, const char *input, int in_sz, long created_time, int line_limit) {
	Space title_space = {0};

	article.add("<article>");

	int closing_tags[N_CLOSING_TAGS] = {0};
	int tag_level = 0;

	char c = 0, next = 0, prev = 0;
	int mode = 0;
	int len_of_mode = 0;
	int div_levels = 0;
	bool inside_para = false;
	bool should_start_para = false;
	bool div_emit_nl = false;
	int nl_count = 0;

	for (int i = 0; i < in_sz; i++) {
		c = input[i];
		next = input[i+1]; // safe, actually

		if (mode == 0) {
			if (c == '\r') {
				char sp = ' ';
				article.add(&sp, 1);
			}
			else if (c == '\n') {
				nl_count++;
				if (line_limit > 0 && nl_count > line_limit)
					break;

				// in CSS, does .class p {...} apply to all p elements inside an element that has a class called .class,
				// or does it apply to all p elements that have a class called .class?
				if (should_start_para || (next == '\n' && div_levels <= 0)) {
					if (inside_para) {
						article.add("</p>\n<p>");
					}
					else {
						article.add("\n<p>");
					}
					inside_para = true;
					should_start_para = false;
				}
				else if (true || prev != '\n') {
					if (div_emit_nl)
						article.add(&c, 1);
					div_emit_nl = true;
				}
			}
			else if (NEEDS_ESCAPE(c)) {
				char esc_buf[8];
				write_escaped_byte(c, esc_buf);
				article.add(esc_buf, 6);
			}
			else if (c == '[' && div_levels <= 0) {
				mode = 1;
			}
			else if (c == ']' && div_levels <= 0) {
				mode = 2;
			}
			else if (c == '|' && (prev == '\n' || div_levels <= 0)) {
				mode = 3;
			}
			else {
				article.add(&c, 1);
			}
		}
		else {
			if (c == '\n') {
				nl_count++;
				if (line_limit > 0 && nl_count > line_limit)
					break;
			}
			if (c == '[' || c == ']' || c == '|') {
				if (len_of_mode > 0) {
					if (mode == 1) {
						article.add("<span class=\"");
						article.add(&input[i - len_of_mode], len_of_mode);
						article.add("\">", 2);
					}
					else if (mode == 2) {
						article.add("</span>");
					}
					else if (mode == 3) {
						const char *s = &input[i - len_of_mode];
						if (c != ']') {
							const char *tag_to_add = nullptr;
							int orig_len = 0;

							if (len_of_mode >= 5 && !memcmp(s, "block", 5)) {
								tag_to_add = "<div";
								orig_len = 5;
								if (tag_level < N_CLOSING_TAGS)
									closing_tags[tag_level++] = CMD_BLOCK;

								div_levels++;
								div_emit_nl = false;
							}
							else if (len_of_mode >= 5 && !memcmp(s, "title", 5)) {
								tag_to_add = "<h1";
								orig_len = 5;
								if (tag_level < N_CLOSING_TAGS)
									closing_tags[tag_level++] = CMD_TITLE;

								title_space.offset = i + 1;
							}
							else if (len_of_mode >= 8 && !memcmp(s, "subtitle", 8)) {
								tag_to_add = "<h2";
								orig_len = 8;
								if (tag_level < N_CLOSING_TAGS)
									closing_tags[tag_level++] = CMD_SUBTITLE;
							}

							if (tag_to_add) {
								if (inside_para) {
									article.add("</p>\n");
								}
								inside_para = 0;

								if (len_of_mode >= orig_len + 2) {
									int class_len = len_of_mode - 6;
									int short_len = class_len;
									const char *class_name = &s[6];
									for (int j = 0; j < class_len; j++) {
										if (class_name[j] == '-') {
											short_len = j;
											break;
										}
									}

									article.add(tag_to_add);
									article.add(" class=\"");
									article.add(class_name, short_len);

									if (short_len != class_len) {
										article.add(" ", 1);
										article.add(class_name, class_len);
									}

									article.add("\">", 2);
								}
								else {
									char gt = '>';
									article.add(tag_to_add);
									article.add(&gt, 1);
								}
							}
						}
						else {
							if (tag_level > 0) {
								int tag = closing_tags[--tag_level];
								if (tag > 0) {
									article.add(closing_tag_strings[tag-1]);
								}

								if (tag == CMD_BLOCK) {
									div_levels--;
									if (div_levels <= 0) {
										should_start_para = true;
									}
								}
								else if (tag == CMD_TITLE) {
									title_space.size = i - title_space.offset - len_of_mode - 1;
									should_start_para = true;
								}
								else if (tag == CMD_SUBTITLE) {
									struct tm *date = localtime(&created_time);
									int day_kind = 0;
									int day_mod10 = date->tm_mday % 10;
									if (day_mod10 < 4 && (date->tm_mday < 4 || date->tm_mday > 20))
										day_kind = day_mod10;

									const char *kinds[] = {"th", "st", "nd", "rd"};
									char datebuf[128];
									String date_str(datebuf, 128);
									date_str.reformat("<p class=\"italic\">{d}{s} {s}, {d}</p>", date->tm_mday, kinds[day_kind], months[date->tm_mon], 1900 + date->tm_year);

									article.add(date_str.data(), date_str.len);
									should_start_para = true;
								}
							}
						}
					}
				}
				else {
					char orig = "[]|"[mode-1];
					article.add(&orig, 1);
				}

				mode = 0;
				len_of_mode = 0;
			}
			else {
				len_of_mode++;
			}
		}

		prev = c;
	}

	if (inside_para)
		article.add("</p>");
	for (int i = 0; i < div_levels; i++)
		article.add("</div>");

	article.add("</article>");

	return title_space;
}
