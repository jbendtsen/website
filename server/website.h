#pragma once

#include <stdarg.h>

typedef unsigned char u8;
typedef unsigned char u32;

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
	//int size;
	int n_files;
	char *label_pool;
	char *fname_pool;
	char *type_pool;
	File *files;
};

// Can be used as a managed or unmanaged string.
// If the string is unmanaged, the LSB of 'ptr' is set to 1.
struct String {
	static constexpr INITIAL_SIZE = 32;

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
		ptr = buf | 1;
		capacity = size;
		len = 0;
	}
	~String() {
		if (ptr && (ptr & 1) == 0)
			delete[] ptr;
	}

	char *data() {
		return ptr ? (ptr & ~1) : &initial_buf[0];
	}
	int size() {
		return len;
	}

	int resize(int sz);
	void add(String& str);
	void add(const char *str, int len);

	static String format(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		String str;
		make_formatted_string(str, fmt, args);
		va_end(args);
		return str;
	}
};

void write_formatted_string(String& str, const char *fmt, va_list args);
char *append_string(char *dst, char *src, int len);