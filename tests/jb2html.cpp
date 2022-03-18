#include <cstdio>
#include "../server/website.h"

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

	int out_cap = in_sz * 2 + 256;
	Expander article;
	article.init(out_cap, false, 256);

	article.add_string("</title><style>\n", 0);

	f = fopen("../client/article.css", "rb");
	if (f) {
		fseek(f, 0, SEEK_END);
		int css_sz = ftell(f);
		rewind(f);

		if (css_sz > 0) {
			article.add_string(nullptr, css_sz);
			fread(&article.buf[article.head - css_sz], 1, css_sz, f);
		}

		fclose(f);
	}

	article.add_string("\n</style></head><body>", 0);

	Space title_space = produce_article_html(article, input, in_sz, 0);

	article.add_string("</body></html>", 0);

	article.prepend_string_trunc(&input[title_space.offset], title_space.size);
	article.prepend_string_overlap("<!DOCTYPE html><html><head><title>", 0);

	int output_sz = 0;
	char *output = article.get(&output_sz);

	f = fopen(argv[2], "wb");
	fwrite(output, 1, output_sz, f);
	fclose(f);

	delete[] input;

	return 0;
}
