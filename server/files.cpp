#include <algorithm>
#include <cstdio>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
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

int Filesystem::init_at(const char *initial_path, char *list_dir_buffer)
{
	name_pool.init(512, /*use_terminators=*/true);

	int len = initial_path ? strlen(initial_path) : 0;

	if (len) {
		starting_path.assign(initial_path, len);
		if (starting_path.last() != '/')
			starting_path.add('/');
	}
	else {
		starting_path.assign("./", 2);
	}

	char *offsets_file[LIST_DIR_MAX_FILES];
	char *offsets_dir[LIST_DIR_MAX_DIRS];

	dirs.add((FS_Directory){
		.next = -1,
		.flags = 0,
		.name_idx = -1,
		.first_dir = -1,
		.first_file = -1
	});

	String path(starting_path);
	return init_directory(path, 0, offsets_file, offsets_dir, list_dir_buffer);
}

int Filesystem::init_directory(String& path, int parent_dir, char **offsets_file, char **offsets_dir, char *list_dir_buffer)
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

		if (dir->d_type == DT_DIR) {
			if (n_dirs >= LIST_DIR_MAX_DIRS) {
				n_dirs++;
				break;
			}

			offsets_dir[n_dirs++] = name;
		}
		else {
			if (n_files >= LIST_DIR_MAX_FILES) {
				n_files++;
				break;
			}

			offsets_file[n_files++] = name;
		}

		dir = (linux_dirent64*)((char*)dir + record_len);
		off += record_len;
	}

	auto sort_names_func = [](char *a, char *b) {
		return strcmp(a, b) < 0;
	};

	if (n_dirs > 0) {
		std::sort(offsets_dir, offsets_dir + n_dirs, sort_names_func);
		dirs[parent_dir].first_dir = dirs.size;
	}
	else {
		dirs[parent_dir].first_dir = -1;
	}

	if (n_files > 0) {
		std::sort(offsets_file, offsets_file + n_files, sort_names_func);
		dirs[parent_dir].first_file = files.size;
	}
	else {
		dirs[parent_dir].first_file = -1;
	}

	for (int i = 0; i < n_dirs; i++) {
		int name_idx = name_pool.head;
		name_pool.add_string(offsets_dir[i], 0);

		int next = i < n_dirs-1 ? dirs.size+1 : -1;
		dirs.add((FS_Directory){
			.next = next,
			.flags = 0,
			.name_idx = name_idx,
			.first_dir = -1,
			.first_file = -1
		});
	}
	for (int i = 0; i < n_files; i++) {
		int name_idx = name_pool.head;
		name_pool.add_string(offsets_file[i], 0);

		int next = i < n_files-1 ? files.size+1 : -1;
		files.add((FS_File){
			.next = next,
			.flags = 0,
			.name_idx = name_idx,
			.size = -1,
			.buffer = nullptr,
			.last_reloaded = 0
		});
	}

	int end = dirs.size;
	int start = end - n_dirs;

	for (int i = start; i < end; i++) {
		int n = dirs[i].name_idx;
		char *name = &name_pool.buf[n];
		int len = strlen(name);
		path.add(name, len);
		path.add('/');

		init_directory(path, i, offsets_file, offsets_dir, list_dir_buffer);

		path.scrub(len + 1);
	}

	return 0;
}
