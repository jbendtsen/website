#include <algorithm>
#include <cstdio>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "website.h"

struct linux_dirent64 {
	ino64_t        d_ino;    /* 64-bit inode number */
	off64_t        d_off;    /* 64-bit offset to next structure */
	unsigned short d_reclen; /* Size of this dirent */
	unsigned char  d_type;   /* File type */
	char           d_name[]; /* Filename (null-terminated) */
};

int File_Database::init(const char *fname)
{
	FILE *f = fopen(fname, "r");
	if (!f) {
		log_error("Could not open {s}", fname);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	if (sz <= 0) {
		log_error("Failed to read from {s}", fname);
		return 2;
	}

	pool_size = sz + 1;
	map_buffer = new char[pool_size * 4]();
	label_pool = &map_buffer[pool_size];
	fname_pool = &map_buffer[pool_size * 2];
	type_pool  = &map_buffer[pool_size * 3];
	n_files = 0;

	fread(map_buffer, 1, sz, f);
	fclose(f);

	int line_no = 0;
	int pool = 0;
	bool was_ch = false;
	int offsets[] = {0, 0, 0};

	for (int i = 0; i < sz; i++) {
		char c = map_buffer[i];
		char character = c != ' ' && c != '\t' && c != '\n' && c != '\r' ? c : 0;

		map_buffer[pool_size * (pool + 1) + offsets[pool]] = character;
		offsets[pool]++;

		int file_inc = 0;

		if (!character) {
			if (c == ' ' || c == '\t') {
				pool++;
				if (pool >= 3) {
					pool = 0;
					log_info("{s}: line {d} contains too many parameters, wrapping around", fname, line_no);
				}
			}
			else if (c == '\r' || c == '\n') {
				pool = 0;
				file_inc = 1;
			}
		}

		if (i == sz-1)
			file_inc = 1;

		n_files += file_inc;

		if (c == '\n')
			line_no++;
	}

	files = new DB_File[n_files];
	memset(files, 0, n_files * sizeof(DB_File));

	{
		int off = 0;
		for (int i = 0; i < n_files; i++) {
			files[i].label = &label_pool[off];
			off += strlen(files[i].label) + 1;
		}
	}
	{
		int off = 0;
		for (int i = 0; i < n_files; i++) {
			files[i].fname = &fname_pool[off];
			off += strlen(files[i].fname) + 1;
		}
	}
	{
		int off = 0;
		for (int i = 0; i < n_files; i++) {
			files[i].type = &type_pool[off];
			off += strlen(files[i].type) + 1;
		}
	}

	return 0;
}

int File_Database::lookup_file(char *name, int len)
{
	char *p = label_pool;
	int file = 0;
	int off = 0;
	int matches = 0;

	while (file < n_files) {
		char c = *p++;
		if (c == 0) {
			if (off == len && matches == len)
				break;

			off = -1;
			matches = 0;
			file++;
		}
		else if (off < len && c == name[off]) {
			matches++;
		}

		off++;
	}

	if (file >= n_files)
		return -1;

	struct timespec spec;
	long t = clock_gettime(CLOCK_BOOTTIME, &spec);

	DB_File *f = &files[file];
	long threshold = f->last_reloaded + SECS_UNTIL_RELOAD;

	if (!f->buffer || t >= threshold || t < threshold - (1LL << 63)) {
		if (f->buffer)
			delete[] f->buffer;

		FILE *fp = fopen(f->fname, "rb");
		if (!fp)
			return -2;

		fseek(fp, 0, SEEK_END);
		int sz = ftell(fp);
		rewind(fp);

		if (sz <= 0)
			return -3;

		f->last_reloaded = t;
		f->size = sz;
		f->buffer = new u8[sz];

		fread(f->buffer, 1, sz, fp);
		fclose(fp);
	}

	return file;
}

struct Sort_Buffer {
	char *offsets[LIST_DIR_MAX_FILES];
	int indices[LIST_DIR_MAX_FILES];
	long modified[LIST_DIR_MAX_FILES];
	long created[LIST_DIR_MAX_FILES];
};

struct Sort_Numbers {
	long *buffer;

	bool operator()(int a, int b) {
		return buffer[a] < buffer[b];
	}
};

template <typename Entry>
static long fs_add_entries(Vector<Entry>& entries, Expander& name_pool, String& path, Sort_Buffer *sorter, int count)
{
	std::sort(sorter->offsets, sorter->offsets + count, [](char *a, char *b) { return strcmp(a, b) < 0; });

	struct stat st;
	int first = entries.size;

	for (int i = 0; i < count; i++) {
		int name_idx = name_pool.head;
		int len = name_pool.add_string(sorter->offsets[i], 0);

		path.add(name_pool.at(name_idx), len);
		int res = stat(path.data(), &st);
		path.scrub(len);

		sorter->indices[i] = i;
		sorter->modified[i] = st.st_mtim.tv_sec;
		sorter->created[i] = st.st_ctim.tv_sec;

		int next = i < count-1 ? entries.size+1 : -1;

		Entry e = Entry::make_empty();
		e.name_idx = name_idx;
		e.next.alpha = next;

		entries.add(e);
	}

	Sort_Numbers numbers;
	numbers.buffer = sorter->modified;
	std::sort(sorter->indices, sorter->indices + count, numbers);
	long modified_first = sorter->indices[0];

	for (int i = 0; i < count - 1; i++) {
		entries[first + sorter->indices[i]].next.modified = first + sorter->indices[i+1];
	}
	entries[first + sorter->indices[count-1]].next.modified = -1;

	numbers.buffer = sorter->created;
	std::sort(sorter->indices, sorter->indices + count, numbers);
	long created_first = sorter->indices[0];

	for (int i = 0; i < count - 1; i++) {
		entries[first + sorter->indices[i]].next.created = first + sorter->indices[i+1];
	}
	entries[first + sorter->indices[count-1]].next.created = -1;

	return (modified_first << 32) | created_first;
}

static int fs_init_directory(Filesystem& fs, String& path, int parent_dir, Sort_Buffer *sort_buf_dirs, Sort_Buffer *sort_buf_files, char *list_dir_buffer)
{
	int path_fd = openat(AT_FDCWD, path.data(), O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC);
	if (path_fd < 0)
		return -1;

	auto dir = (linux_dirent64*)list_dir_buffer;
	int read_sz = getdents64(path_fd, dir, LIST_DIR_LEN);

	close(path_fd);

	if (read_sz <= 0)
		return -2;

	int off = 0;
	int n_files = 0;
	int n_dirs = 0;

	while (off < read_sz) {
		int record_len = dir->d_reclen;
		char *name = dir->d_name;

		if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) {
			dir = (linux_dirent64*)((char*)dir + record_len);
			off += record_len;
			continue;
		}

		Sort_Buffer *sorter = sort_buf_files;
		int *n = &n_files;

		if (dir->d_type == DT_DIR) {
			sorter = sort_buf_dirs;
			n = &n_dirs;
		}

		// TODO: do something when this happens
		if (*n >= LIST_DIR_MAX_FILES) {
			*n += 1;
			break;
		}

		sorter->offsets[*n] = name;
		*n += 1;

		dir = (linux_dirent64*)((char*)dir + record_len);
		off += record_len;
	}

	if (n_dirs > 0) {
		int cur = fs.dirs.size;
		long info = fs_add_entries(fs.dirs, fs.name_pool, path, sort_buf_dirs, n_dirs);

		fs.dirs[parent_dir].first_dir = {
			.alpha = cur,
			.modified = cur + (int)(info >> 32),
			.created = cur + (int)info
		};
	}
	if (n_files > 0) {
		int cur = fs.files.size;
		long info = fs_add_entries(fs.files, fs.name_pool, path, sort_buf_files, n_files);

		fs.dirs[parent_dir].first_file = {
			.alpha = cur,
			.modified = cur + (int)(info >> 32),
			.created = cur + (int)info
		};
	}

	int end = fs.dirs.size;
	int start = end - n_dirs;

	for (int i = start; i < end; i++) {
		int n = fs.dirs[i].name_idx;
		char *name = &fs.name_pool.buf[n];
		int len = strlen(name);
		path.add(name, len);
		path.add('/');

		fs_init_directory(fs, path, i, sort_buf_dirs, sort_buf_files, list_dir_buffer);

		path.scrub(len + 1);
	}

	return 0;
}

int Filesystem::init_at(const char *initial_path, char *list_dir_buffer)
{
	name_pool.init(512, /*use_terminators=*/true, 0);

	int len = initial_path ? strlen(initial_path) : 0;

	if (len) {
		starting_path.assign(initial_path, len);
		if (starting_path.last() != '/')
			starting_path.add('/');
	}
	else {
		starting_path.assign("./", 2);
	}

	Sort_Buffer sort_buf_dirs;
	Sort_Buffer sort_buf_files;

	dirs.add(FS_Directory::make_empty());

	String path(starting_path);
	return fs_init_directory(*this, path, 0, &sort_buf_dirs, &sort_buf_files, list_dir_buffer);
}
