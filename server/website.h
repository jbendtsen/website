#pragma once

#include <cstdarg>
#include <cstring>

#define SECS_UNTIL_RELOAD 60

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct String;
struct Expander;
template<typename T> struct Vector;

void write_formatted_string(String& str, const char *fmt, va_list args);
char *append_string(char *dst, char *src, int len);

struct File {
	long last_reloaded;
	u32 flags;
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

// TODO: This design does not account for if upon refreshing the number of things in the directory has changed
struct Directory {
	char *name;
	int first_dir;
	int last_dir;
	int first_file;
	int last_file;
};

template<typename T, int size>
struct Bucket {
	T data[size];
	Bucket<T, size> *next;
};

struct Filesystem {
	Expander name_pool;
	Bucket<Directory, 16> dirs;
	Bucket<File, 16> files;
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

struct Expander {
	char *buf;
	int cap;
	int head;
	int start;
	int extra;

	Expander() = default;
	~Expander() {
		if (buf) delete[] buf;
	}

	char *get(int *size) {
		if (size) *size = head - start;
		return buf + start;
	}

	void init(int capacity, bool use_terminators) {
		cap = capacity;
		buf = new char[cap]();
		head = 0;
		start = 0;
		extra = (int)use_terminators;
	}

	void add_string(const char *add, int add_len) {
		if (add && add_len <= 0)
			add_len = strlen(add);

		int growth = add_len + extra;

		int h = head;
		int c = cap;
		if (c <= 0) c = 64;

		while (h + growth > c)
			c *= 2;

		if (c != cap) {
			char *new_str = new char[c];
			memcpy(new_str, buf, cap);
			memset(&new_str[cap], 0, c - cap);
			delete[] buf;
			buf = new_str;
			cap = c;
		}

		if (add)
			memcpy(&buf[head], add, add_len);

		head += growth;
	}
};

template<typename T>
struct Vector {
	T *data;
	int cap;
	int size;

	Vector<T>() {
		data = nullptr;
		cap = 0;
		size = 0;
	}
	~Vector<T>() {
		if (data) delete[] data;
	}

	void resize(int new_size) {
		if (new_size <= size)
			return;

		int old_cap = cap;
		if (cap < 16)
			cap = 16;
		while (cap < new_size)
			cap *= 2;

		if (cap > old_cap) {
			T *new_data = new T[cap];
			if (data) {
				memcpy(new_data, data, old_cap);
				delete[] data;
			}
			data = new_data;
		}

		size = new_size;
	}

	void add(T t) {
		int old_size = size;
		resize(old_size + 1);
		data[old_size] = t;
	}

	void add_multiple(T *array, int n) {
		int old_size = size;
		resize(old_size + n);
		memcpy(&data[old_size], array, n * sizeof(T));
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

