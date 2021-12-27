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

	info.output = stdout;
	info.min_level = info.max_level = 0;
	info.name = "error";
}

void log_error(const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	
}
