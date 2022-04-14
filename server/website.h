#pragma once

#include <cstdarg>
#include <cstring>

#define NEEDS_ESCAPE(c) ((c) < ' ' || (c) > '~' || (c) == '&' || (c) == '<' || (c) == '>' || (c) == '"')
#define IS_SPACE(c) (c == ' ' || c == '\t' || c == '\r' || c == '\n')

#define SECS_UNTIL_RELOAD 60

#define RESPONSE_FILE  1
#define RESPONSE_HTML  2
#define RESPONSE_MULTI 3

#define LIST_DIR_MAX_FILES  (1 << 12)
#define LIST_DIR_LEN        (1 << 20)

#define N_CLOSING_TAGS 100

#define NAV_IDX_NULL     -1
#define NAV_IDX_HOME      0
#define NAV_IDX_BLOG      1
#define NAV_IDX_PROJECTS  2

typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;

struct Space {
	int offset;
	int size;
};

struct String;

void init_logger();
void log_info(const char *fmt, ...);
void log_error(const char *fmt, ...);

void write_formatted_string(String& str, const char *fmt, va_list args);
char *append_string(char *dst, char *src, int len);

void write_escaped_byte(int ch, char *buf);

bool caseless_match(const char *a, const char *b);

// Can be used as a managed or unmanaged string.
// If the string is unmanaged, the LSB of 'ptr' is set to 1.
// This class *must* have access to at least len+1 bytes of memory.
struct String {
	static constexpr int INITIAL_SIZE = 32;

	char *ptr;
	int capacity;
	int len;
	char initial_buf[INITIAL_SIZE];

	String();
	String(char *buf, int size);
	String(String& other);
	~String();

	char *data();
	char last();

	void scrub(int len_from_end);

	int resize(int sz);
	void reserve(int sz);

	void add(String str);
	void add(const char *str, int size);
	void add(char c);
	void add(const char *str);

	void add_chars(char c, int n);

	void add_and_escape(const char *str, int size = 0);

	void assign(const char *str, int size);

	static String make_escaped(const char *str, int size) {
		String s;
		s.add_and_escape(str, size);
		return s;
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

struct Pool {
	char *buf = nullptr;
	int cap = 0;
	int head = 0;

	Pool();
	~Pool();

	void init(int capacity);

	char *at(int offset);

	int add(const char *str, int add_len = 0);
	void add_and_escape(const char *str, int size = 0);

	void reserve_extra(int bytes);
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
		while (new_size > cap)
			cap *= 2;

		if (cap > old_cap) {
			T *new_data = new T[cap];
			if (data) {
				if (old_cap > 0) memcpy(new_data, data, old_cap * sizeof(T));
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

template <typename T>
struct Inline_Vector {
	static constexpr int INLINE_COUNT = 32;

	T space[INLINE_COUNT];
	T *ptr;
	int cap;
	int size;

	Inline_Vector() {
		ptr = nullptr;
		cap = INLINE_COUNT;
		size = 0;
	}
	~Inline_Vector() {
		if (ptr) delete[] ptr;
	}

	T *data() { return ptr ? ptr : space; }

	T& operator[](int idx) { return data()[idx]; }

	void resize(int new_size) {
		if (new_size < 0)
			new_size = 0;

		int c = cap;
		if (c < INLINE_COUNT)
			c = INLINE_COUNT;
		while (new_size > c)
			c *= 2;

		if (c != cap && c > INLINE_COUNT) {
			T *new_data = new T[c];
			memcpy(new_data, data(), cap * sizeof(T));
			if (ptr) delete[] ptr;
			ptr = new_data;
		}

		cap = c;
		size = new_size;
	}

	void add(T t) {
		int idx = size;
		resize(size+1);
		data()[idx] = t;
	}
};

struct Response {
	int type;
	int file_size;
	char *file_buffer;
	const char *status;
	String html;
};

struct DB_File {
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
	DB_File *files;

	int init(const char *fname);
	int lookup_file(char *name, int len);
};

#define FS_ORDER_ALPHA    0
#define FS_ORDER_MODIFIED 1
#define FS_ORDER_CREATED  2

union FS_Next {
	struct {
		int alpha;
		int modified;
		int created;
	};
	int array[3];
};

struct FS_Directory {
	FS_Next next;
	int parent;
	int name_idx;
	long created_time;
	long modified_time;
	FS_Next first_dir;
	FS_Next first_file;

	static FS_Directory make_empty() {
		return {
			.next = {-1, -1, -1},
			.parent = -1,
			.name_idx = -1,
			.created_time = 0,
			.modified_time = 0,
			.first_dir = {-1, -1, -1},
			.first_file = {-1, -1, -1}
		};
	}
};

#define FILE_FLAG_ASCII  1

struct FS_File {
	FS_Next next;
	int parent;
	int name_idx;
	long created_time;
	long modified_time;
	u8 *buffer;
	int size;
	u32 crc;
	u32 flags;

	static FS_File make_empty() {
		return {
			.next = {-1, -1, -1},
			.parent = -1,
			.name_idx = -1,
			.created_time = 0,
			.modified_time = 0,
			.buffer = nullptr,
			.size = 0,
			.flags = 0
		};
	}
};

struct Filesystem {
	String starting_path;
	Pool name_pool;
	Vector<FS_Directory> dirs;
	Vector<FS_File> files;
	int total_name_tree_size;

	Filesystem() = default;
	~Filesystem() {
		for (int i = 0; i < files.size; i++) {
			if (files[i].buffer)
				delete[] files[i].buffer;
		}
	}

	int init_at(const char *initial_path, Pool& allowed_dirs, char *list_dir_buffer);
	void lookup(int *dir_idx, int *file_idx, const char *path, int max_len);
	int lookup_dir(const char *path);
	int lookup_file(const char *path);
	void walk(int dir_idx, int order, void (*dir_cb)(Filesystem*, int, void*), void (*file_cb)(Filesystem*, int, void*), void *cb_data);
	int get_path(char *buf, int ancestor, int parent, char *name);
	//int refresh_file(int idx);
	bool add_file_to_html(String *html, const char *path);
};

struct HTML_Type {
	const char *mime;
	const char *tag;
};

HTML_Type lookup_ext(const char *ext);
void get_datetime(char *buf);
void write_http_header(int request_fd, const char *status, const char *content_type, int size);
void add_banner(Filesystem& fs, String *html, int hl_idx);

void write_zip_as_response(Filesystem& fs, int dir_idx, Response& response);

Space produce_markdown_html(String& html, const char *input, int in_sz, const char *path, long created_time, int line_limit);

void serve_404(Filesystem& fs, Response& response);
void serve_home_page(Filesystem& fs, Response& response);
void serve_blog_overview(Filesystem& fs, Response& response);
void serve_specific_blog(Filesystem& fs, Response& response, char *name, int len);
void serve_projects_overview(Filesystem& fs, Response& response);
void serve_specific_project(Filesystem& fs, Response& response, char *name, int len);

