#pragma once

#include <stdarg.h>

typedef unsigned char u8;
typedef unsigned char u32;

template<int, int>
struct Bucket_Array<int bs, int max> {
	constexpr int bucket_size = bs;
	constexpr int max_buckets = max;

	u8 *buckets[max] = {nullptr};
	int buck;
	int head;

	void append(const char *str) {
		
	}
};

struct File {
	int flags;
	int size;
	u8 *buffer;
	char *label;
	char *fname;
};

struct File_Pool {
	int size;
	int n_files;
	u32 *pool;
	File *files;
};

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
	~String() {
		if (ptr) delete[] ptr;
	}

	char *data() {
		return ptr ? ptr : &initial_buf[0];
	}
	int size() {
		return len;
	}

	void resize(int sz);
	void add(String& str);
	void add(const char *str, int len);

	void operator=(const char *str) {
		int sz = strlen(str);
		int head = len;
		resize(len + sz + 1);
		strcpy(data() + head, str);
	}

	static String format(const char *fmt, ...) {
		va_list args;
		va_start(args, fmt);
		String str = make_formatted_string(fmt, args);
		va_end(args);
		return str;
	}
};
