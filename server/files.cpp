#include "website.h"

int File_Database::init(const char *fname) {
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

int File_Database::lookup_file(char *name, int len) {
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

void Filesystem::init_at(const char *initial_path, char *list_dir_buffer) {
	{
		int len = initial_path ? strlen(initial_path) : 0;
		if (len) {
			starting_path.assign(path, len);
			if (path.last() != '/')
				path.add(' ');
		}
		else {
			starting_path.assign("./", 2);
		}
	}

	String path(starting_path);
	int path_end = path.size();

	int path_fd = openat(AT_FDCWD, path.data(), O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC);

	while (true) {
		auto dir = (struct linux_dirent64 *)list_dir_buffer;
		int len = getdents64(path_fd, dir, LIST_DIR_LEN);
		if (len <= 0)
			break;

		
	}

	close(fd);
}