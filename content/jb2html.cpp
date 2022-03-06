#include <cstdio>
#include <cstring>

#define N_CLOSING_TAGS 100

struct String_Vec {
	char *buf;
	int cap;
	int head;

	String_Vec() = default;
	~String_Vec() {
		if (buf) delete[] buf;
	}

	void init(int capacity) {
		cap = capacity;
		buf = new char[cap];
		head = 0;
	}

	void add_string(const char *add, int add_len) {
		if (add && add_len <= 0)
			add_len = strlen(add);

		int h = head;
		int c = cap;
		if (c <= 0) c = 64;

		while (h + add_len > c)
			c *= 2;

		if (c != cap) {
			char *new_str = new char[c];
			memcpy(new_str, buf, cap);
			delete[] buf;
			buf = new_str;
			cap = c;
		}

		if (add)
			memcpy(&buf[head], add, add_len);

		head += add_len;
	}
};

struct Space {
	int offset;
	int size;
};

#define CMD_BLOCK     1
#define CMD_TITLE     2
#define CMD_SUBTITLE  3

const char *closing_tag_strings[] = {
	"</div>",
	"</h1>",
	"</h2>",
};

Space produce_article_html(String_Vec& article, char *input, int in_sz) {
	Space title_space = {0};

	int closing_tags[N_CLOSING_TAGS] = {0};
	int tag_level = 0;

	char c = 0, next = 0, prev = 0;
	int mode = 0;
	int len_of_mode = 0;
	int div_levels = 0;
	bool inside_para = false;
	bool should_start_para = false;
	bool div_emit_nl = false;

	for (int i = 0; i < in_sz; i++) {
		c = input[i];
		next = input[i+1]; // safe, actually

		if (mode == 0) {
			if (c == '\r') {
				char sp = ' ';
				article.add_string(&sp, 1);
			}
			else if (c == '\n') {
				// in CSS, does .class p {...} apply to all p elements inside an element that has a class called .class,
				// or does it apply to all p elements that have a class called .class?
				if (should_start_para || (next == '\n' && div_levels <= 0)) {
					if (inside_para) {
						article.add_string("</p>\n<p>", 0);
					}
					else {
						article.add_string("\n<p>", 0);
					}
					inside_para = true;
					should_start_para = false;
				}
				else if (true || prev != '\n') {
					if (div_emit_nl)
						article.add_string(&c, 1);
					div_emit_nl = true;
				}
			}
			else if (c < ' ' || c > '~' || c == '&' || c == '<' || c == '>' || c == '"') {
				int n = c;
				int len = 0;

				char esc_buf[16];
				char *p = &esc_buf[15];

				*p = 0; p--;
				*p = ';'; p--; len++;

				if (n > 0) {
					while (n > 0) {
						*p = '0' + (n % 10); p--; len++;
						n /= 10;
					}
				}
				else {
					*p = '0'; p--; len++;
				}

				*p = '#'; p--; len++;
				*p = '&'; len++;
				article.add_string(p, len);
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
				article.add_string(&c, 1);
			}
		}
		else if (mode > 0) {
			if (c == '[' || c == ']' || c == '|') {
				if (len_of_mode > 0) {
					if (mode == 1) {
						article.add_string("<span class=\"", 0);
						article.add_string(&input[i - len_of_mode], len_of_mode);
						article.add_string("\">", 2);
					}
					else if (mode == 2) {
						article.add_string("</span>", 0);
					}
					else if (mode == 3) {
						char *s = &input[i - len_of_mode];
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
									article.add_string("</p>\n", 0);
								}
								inside_para = 0;

								if (len_of_mode >= orig_len + 2) {
									article.add_string(tag_to_add, 0);
									article.add_string(" class=\"", 0);
									article.add_string(&s[6], len_of_mode - 6);
									article.add_string("\">", 0);
								}
								else {
									char gt = '>';
									article.add_string(tag_to_add, 0);
									article.add_string(&gt, 1);
								}
							}
						}
						else {
							if (tag_level > 0) {
								int tag = closing_tags[--tag_level];
								if (tag > 0) {
									article.add_string(closing_tag_strings[tag-1], 0);
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
									should_start_para = true;
								}
							}
						}
					}
				}
				else {
					char orig = "[]|"[mode-1];
					article.add_string(&orig, 1);
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
		article.add_string("</p>", 0);
	for (int i = 0; i < div_levels; i++)
		article.add_string("</div>", 0);

	return title_space;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		printf("Usage: %s <input .jb> <output .html>\n", argv[0]);
		return 1;
	}

	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		printf("Could not open %s\n", argv[1]);
		return 2;
	}

	fseek(f, 0, SEEK_END);
	int in_sz = ftell(f);
	rewind(f);

	if (in_sz < 1) {
		printf("%s could not be read\n", argv[1]);
		return 3;
	}

	char *input = new char[in_sz + 2];
	fread(input, 1, in_sz, f);
	input[in_sz] = 0;
	input[in_sz + 1] = 0;
	fclose(f);

	int out_cap = in_sz * 2;
	String_Vec article;
	article.init(out_cap);

	Space title_space = produce_article_html(article, input, in_sz);

	String_Vec header;
	header.init(256);
	header.add_string("<!DOCTYPE html><html><head><title>", 0);
	header.add_string(&input[title_space.offset], title_space.size);
	header.add_string("</title><style>\n", 0);

	f = fopen("article.css", "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		int css_sz = ftell(f);
		rewind(f);

		if (css_sz > 0) {
			header.add_string(nullptr, css_sz);;
			fread(&header.buf[header.head - css_sz], 1, css_sz, f);
		}

		fclose(f);
	}

	header.add_string("\n</style></head><body>", 0);

	article.add_string("</body></html>", 0);

	f = fopen(argv[2], "wb");
	fwrite(header.buf, 1, header.head, f);
	fwrite(article.buf, 1, article.head, f);
	fclose(f);

	delete[] input;

	return 0;
}
