#include "website.h"

void produce_markdown_html(Expander& html, const char *input, int in_sz, int line_limit)
{
    html.add("<article><div class=\"code\">");

	int start = 0;
    bool was_spec = false;
    for (int i = 0; i < in_sz; i++) {
        char c = input[i];
        if (NEEDS_ESCAPE(c)) {
            if (!was_spec && i > start) {
                html.add(&input[start], i-start);
                start = i;
            }
            was_spec = true;
        }
        else {
            if (was_spec && i > start) {
                html.add_and_escape(&input[start], i-start);
                start = i;
            }
            was_spec = false;
        }
    }

    if (was_spec)
        html.add_and_escape(&input[start], in_sz-start);
    else
        html.add(&input[start], in_sz-start);

    html.add("</div></article>");
}
