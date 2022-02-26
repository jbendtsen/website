#include <cstdio>
#include <cstring>

#define N_CLOSING_TAGS 100

void add_to_string(char **str, int *head, int *cap, const char *add, int add_len) {
	if (add_len <= 0)
		add_len = strlen(add);

	int h = *head;
	int c = *cap;
	if (c <= 0) c = 64;

	while (h + add_len > c)
		c *= 2;

	if (c != *cap) {
		char *new_str = new char[c];
		memcpy(new_str, *str, *cap);
		delete[] *str;
		*str = new_str;
		*cap = c;
	}

	char *s = *str;
	memcpy(&s[*head], add, add_len);
	*head += add_len;
}

const char *closing_tag_strings[] = {
	"</div>",
	"</h1>",
	"</h2>",
};

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

	int closing_tags[N_CLOSING_TAGS] = {0};
	int tag_level = 0;

	int head = 0;
	int out_cap = in_sz * 2;
	char *output = new char[out_cap];

	char c = 0, next = 0, prev = 0;
	int mode = 0;
	int len_of_mode = 0;
	int div_levels = 0;
	int inside_para = 0;
	int should_start_para = 0;

	for (int i = 0; i < in_sz; i++) {
		c = input[i];
		next = input[i+1]; // safe, actually

		if (mode == 0) {
			if (c == '\r') {
				char sp = ' ';
				add_to_string(&output, &head, &out_cap, &sp, 1);
			}
			else if (c == '\n') {
				// in CSS, does .class p {...} apply to all p elements inside an element that has a class called .class,
				// or does it apply to all p elements that have a class called .class?
				if (should_start_para || (next == '\n' && div_levels <= 0)) {
					if (inside_para) {
						add_to_string(&output, &head, &out_cap, "</p>\n<p>", 0);
					}
					else {
						add_to_string(&output, &head, &out_cap, "\n<p>", 0);
					}
					inside_para = 1;
					should_start_para = 0;
				}
				else if (prev != '\n') {
					add_to_string(&output, &head, &out_cap, &c, 1);
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
				add_to_string(&output, &head, &out_cap, p, len);
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
				add_to_string(&output, &head, &out_cap, &c, 1);
			}
		}
		else if (mode > 0) {
			if (c == '[' || c == ']' || c == '|') {
				if (len_of_mode > 0) {
					if (mode == 1) {
						add_to_string(&output, &head, &out_cap, "<span class=\"", 0);
						add_to_string(&output, &head, &out_cap, &input[i - len_of_mode], len_of_mode);
						add_to_string(&output, &head, &out_cap, "\">", 2);
					}
					else if (mode == 2) {
						add_to_string(&output, &head, &out_cap, "</span>", 0);
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
									closing_tags[tag_level++] = 1;

								div_levels++;
							}
							else if (len_of_mode >= 5 && !memcmp(s, "title", 5)) {
								tag_to_add = "<h1";
								orig_len = 5;
								if (tag_level < N_CLOSING_TAGS)
									closing_tags[tag_level++] = 2;
							}
							else if (len_of_mode >= 8 && !memcmp(s, "subtitle", 8)) {
								tag_to_add = "<h2";
								orig_len = 8;
								if (tag_level < N_CLOSING_TAGS)
									closing_tags[tag_level++] = 3;
							}

							if (tag_to_add) {
								if (inside_para) {
									add_to_string(&output, &head, &out_cap, "</p>\n", 0);
								}
								inside_para = 0;

								if (len_of_mode >= orig_len + 2) {
									add_to_string(&output, &head, &out_cap, tag_to_add, 0);
									add_to_string(&output, &head, &out_cap, " class=\"", 0);
									add_to_string(&output, &head, &out_cap, &s[6], len_of_mode - 6);
									add_to_string(&output, &head, &out_cap, "\">", 0);
								}
								else {
									char gt = '>';
									add_to_string(&output, &head, &out_cap, tag_to_add, 0);
									add_to_string(&output, &head, &out_cap, &gt, 1);
								}
							}
						}
						else {
							if (tag_level > 0) {
								int tag = closing_tags[--tag_level];
								if (tag > 0) {
									add_to_string(&output, &head, &out_cap, closing_tag_strings[tag-1], 0);
								}
								if (tag == 1) {
									div_levels--;
									if (div_levels <= 0) {
										should_start_para = 1;
									}
								}
							}
						}
					}
				}
				else {
					char orig = "[]|"[mode-1];
					add_to_string(&output, &head, &out_cap, &orig, 1);
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

	f = fopen(argv[2], "wb");
	fwrite(output, 1, head, f);
	fclose(f);

	delete[] input;
	delete[] output;

	return 0;
}
