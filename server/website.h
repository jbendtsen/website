#pragma once

#include <stdarg.h>
#include <string.h>

#define SECS_UNTIL_RELOAD 60

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct String;

void write_formatted_string(String& str, const char *fmt, va_list args);
char *append_string(char *dst, char *src, int len);

struct File {
	long last_reloaded;
	int flags;
	int size;
	u8 *buffer;
	char *label;
	char *type;
	char *fname;
};

struct File_Database {
	int pool_size;
	int n_files;
	char *map_buffer;
	char *label_pool;
	char *fname_pool;
	char *type_pool;
	File *files;
};

// Can be used as a managed or unmanaged string.
// If the string is unmanaged, the LSB of 'ptr' is set to 1.
struct String {
	static constexpr int INITIAL_SIZE = 32;

	char *ptr;
	int capacity;
	int len;
	char initial_buf[INITIAL_SIZE];

	String() {
		ptr = nullptr;
		capacity = INITIAL_SIZE;
		len = 0;
		initial_buf[0] = 0;
	}
	String(char *buf, int size) {
		ptr = (char*)((u64)buf | 1ULL);
		capacity = size;
		len = 0;
	}
	~String() {
		if (ptr && ((u64)ptr & 1ULL) == 0)
			delete[] ptr;
	}

	char *data() {
		return ptr ? (char*)((u64)ptr & ~1ULL) : &initial_buf[0];
	}
	int size() {
		return len;
	}

	int resize(int sz);
	void add(String& str);
	void add(const char *str, int len);

	void assign(const char *str, int size) {
		resize(0);
		add(str, size);
	}

	void reformat(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		write_formatted_string(*this, fmt, args);
		va_end(args);
	}

	static String format(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		String str;
		write_formatted_string(str, fmt, args);
		va_end(args);
		return str;
	}
};

void init_logger();
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

int lookup_file(File_Database& db, char *name, int len);
void write_http_response(int fd, const char *status, const char *content_type, const char *data, int size);

void serve_404(File_Database& internal, int fd);
void serve_home_page(File_Database& internal, int fd);
void serve_blog_overview(File_Database& internal, int fd);
void serve_specific_blog(File_Database& internal, int fd, char *name, int len);
void serve_projects_overview(File_Database& internal, int fd);
void serve_specific_project(File_Database& internal, int fd, char *name, int len);

