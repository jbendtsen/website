#include <stdio.h>
#include "website.h"

struct Debug_Output {
	FILE *output;
	int min_level;
	int max_level;
	String name;
};

Debug_Output info;
Debug_Output error;

void init_logger() {
	info.output = stdout;
	info.min_level = info.max_level = 0;
	info.name = "info";

	error.output = stderr;
	error.min_level = info.max_level = 0;
	error.name = "error";
}

static void log(Debug_Output& dbg, const char *fmt, va_list args) {
	String out;
	write_formatted_string(out, fmt, args);
	fwrite(out.data(), 1, out.size(), dbg.output);

	if (out.data()[out.size()-1] != '\n')
		fputc('\n', dbg.output);
}

void log_info(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log(info, fmt, args);
	va_end(args);
}

void log_error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	log(error, fmt, args);
	va_end(args);
}
