#include "website.h"

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

	db.pool_size = sz + 1;
	db.map_buffer = new char[db.pool_size * 4]();
	db.label_pool = &db.map_buffer[db.pool_size];
	db.fname_pool = &db.map_buffer[db.pool_size * 2];
	db.type_pool  = &db.map_buffer[db.pool_size * 3];
	db.n_files = 0;

	fread(db.map_buffer, 1, sz, f);
	fclose(f);

	int line_no = 0;
	int pool = 0;
	bool was_ch = false;
	int offsets[] = {0, 0, 0};

	for (int i = 0; i < sz; i++) {
		char c = db.map_buffer[i];
		char character = c != ' ' && c != '\t' && c != '\n' && c != '\r' ? c : 0;

		db.map_buffer[db.pool_size * (pool + 1) + offsets[pool]] = character;
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

		db.n_files += file_inc;

		if (c == '\n')
			line_no++;
	}

	db.files = new File[db.n_files];
	memset(db.files, 0, db.n_files * sizeof(File));

	{
		int off = 0;
		for (int i = 0; i < db.n_files; i++) {
			db.files[i].label = &db.label_pool[off];
			off += strlen(db.files[i].label) + 1;
		}
	}
	{
		int off = 0;
		for (int i = 0; i < db.n_files; i++) {
			db.files[i].fname = &db.fname_pool[off];
			off += strlen(db.files[i].fname) + 1;
		}
	}
	{
		int off = 0;
		for (int i = 0; i < db.n_files; i++) {
			db.files[i].type = &db.type_pool[off];
			off += strlen(db.files[i].type) + 1;
		}
	}

	return 0;
}

int File_Database::lookup_file(char *name, int len)
{
	char *p = db.label_pool;
	int file = 0;
	int off = 0;
	int matches = 0;

	while (file < db.n_files) {
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

	if (file >= db.n_files)
		return -1;

	struct timespec spec;
	long t = clock_gettime(CLOCK_BOOTTIME, &spec);

	File *f = &db.files[file];
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
		starting_path.assign(path, len);
		if (path.last() != '/')
			path.add(' ');
	}
	else {
		starting_path.assign("./", 2);
	}

	int offsets_file[LIST_DIR_MAX_FILES];
	int offsets_dir[LIST_DIR_MAX_DIRS];

	dirs.add((FS_Directory){
		.next = -1,
		.flags = 0,
		.name_idx = nullptr,
		.first_dir = -1,
		.first_file = -1
	});

	String path(starting_path);
	return init_directory(path, 0, offsets_file, offsets_dir, list_dir_buffer);
}

int Filesystem::init_directory(String& path, int parent_dir, int *offsets_file, int *offsets_dir, char *list_dir_buffer)
{
	int path_fd = openat(AT_FDCWD, path.data(), O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC);

	auto dir = (struct linux_dirent64 *)list_dir_buffer;
	int read_sz = getdents64(path_fd, dir, LIST_DIR_LEN);

	close(path_fd);

	if (read_sz <= 0)
		return -1;

	int off = 0;
	int n_files = 0;
	int n_dirs = 0;

	while (off < read_sz) {
		int record_len = dir->d_reclen;
		int name_off = (int)(&dir->d_name[0] - (char*)dir);

		if (dir->d_type == DT_DIR) {
			if (n_dirs >= LIST_DIR_MAX_DIRS) {
				n_dirs++;
				break;
			}

			offsets_dir[n_dirs++] = name_off;
		}
		else {
			if (n_files >= LIST_DIR_MAX_FILES) {
				n_files++;
				break;
			}

			offsets_file[n_files++] = name_off;
		}

		dir = (struct linux_dirent64 *)((char*)dir + record_len);
		off += record_len;
	}

	auto sort_func = [](const void *a, const void *b, void *param) {
		return strcmp((char*)param + *(int*)a, (char*)param + *(int*)b);
	};

	if (n_dirs > 0) {
		qsort_r(offsets_dir, n_dirs, sizeof(int), sort_func, list_dir_buffer);
		dirs[parent_dir].first_dir = dirs.size;
	}
	if (n_files > 0) {
		qsort_r(offsets_file, n_files, sizeof(int), sort_func, list_dir_buffer);
		dirs[parent_dir].first_file = files.size;
	}

	for (int i = 0; i < n_dirs; i++) {
		int name_idx = name_pool.head;
		name_pool.add_string(&list_dir_buffer[offsets_dir[i]], 0);

		dirs.add((FS_Directory){
			.next = i < n_dirs-1 ? i+1 : -1,
			.flags = 0,
			.name_idx = name_idx,
			.first_dir = -1,
			.first_file = -1
		});
	}
	for (int i = 0; i < n_files; i++) {
		int name_idx = name_pool.head;
		name_pool.add_string(&list_dir_buffer[offsets_file[i]], 0);

		files.add((FS_File){
			.next = i < n_files-1 ? i+1 : -1,
			.flags = 0,
			.name_idx = name_idx,
			.size = -1,
			.buffer = nullptr,
			.last_reloaded = 0
		});
	}

	for (int i = dirs.size - n_dirs; i < dirs.size; i++) {
		char *name = &name_pool.buf[dirs[i]];
		int len = strlen(name);
		path.add(name, len);
		path.add('/');

		init_directory(path, i, offsets_file, offsets_dir, list_dir_buffer);

		path.scrub(len + 1);
	}
}
