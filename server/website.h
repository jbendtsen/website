#pragma once

#include <cstdarg>
#include <cstring>

#define SECS_UNTIL_RELOAD 60

#define LIST_DIR_MAX_FILES  (1 << 12)
#define LIST_DIR_MAX_DIRS   (1 << 12)
#define LIST_DIR_LEN        (1 << 20)

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct String;
struct Expander;
template<typename T> struct Vector;

void write_formatted_string(String& str, const char *fmt, va_list args);
char *append_string(char *dst, char *src, int len);

struct DB_File {
	long last_reloaded;
	u32 flags;
	int size;
	u8 *buffer;
	char *label;
	char *type;
	char *fname;

	int init(const char *fname);
	int lookup_file(char *name, int len);
};

struct File_Database {
	int pool_size;
	int n_files;
	char *map_buffer;
	char *label_pool;
	char *fname_pool;
	char *type_pool;
	DB_File *files;
};

struct FS_Directory {
	int next;
	u32 flags;
	int name_idx;
	int first_dir;
	int first_file;
};

struct FS_File {
	int next;
	u32 flags;
	int name_idx;
	int size;
	u8 *buffer;
	long last_reloaded;
};

template<typename T, int size>
struct Bucket {
	T data[size];
	Bucket<T, size> *next;
};

struct Filesystem {
	String starting_path;
	Expander name_pool;
	Vector<FS_Directory> dirs;
	Vector<FS_File> files;

	void init_at(const char *initial_path, char *list_dir_buffer);
	int init_directory(String& path, int parent_dir, int *offsets_file, int *offsets_dir, char *list_dir_buffer);
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

	char last() {
		return data()[len-1];
	}

	void scrub(int len_from_end) {
		if (len_from_end <= 0)
			return;

		int new_head = len - len_from_end;
		if (new_head < 0) new_head = 0;

		memset(data() + new_head, 0, len - new_head);
		len = new_head;
	}

	int resize(int sz);
	void add(String& str);
	void add(const char *str, int len);

	char add(char c) {
		add(&c, 1);
	}

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

	T& operator[](int idx) { return data[idx]; }

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

