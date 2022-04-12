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
	clock_gettime(CLOCK_BOOTTIME, &spec);

	DB_File *f = &files[file];
	long t = spec.tv_sec;
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

static const u32 crc32_poly8_lookup[256] =
{
	0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
	0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
	0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
	0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
	0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
	0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
	0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
	0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
	0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
	0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
	0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
	0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
	0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
	0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
	0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
	0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
	0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
	0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
	0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
	0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
	0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
	0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
	0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
	0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
	0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
	0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
	0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
	0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
	0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
	0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
	0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
	0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

int refresh_file(FS_File *file, const char *path)
{
	/*
	if (f < 0 || f >= files.size)
		return -1;

	struct timespec spec;
	clock_gettime(CLOCK_BOOTTIME, &spec);

	long threshold = files[f].last_reloaded + SECS_UNTIL_RELOAD;

	if (files[f].buffer && spec.tv_sec < threshold)
		return 0;

	char buf[1024];
	int len = get_path(buf, -1, files[f].parent, name_pool.at(files[f].name_idx));
	if (len < 0)
		return -2;

	String path(starting_path);
	path.add(buf + 1, len - 1);
	//FILE *fp = fopen(path.data(), "rb");
	*/

	FILE *fp = fopen(path, "rb");
	if (!fp) {
		log_error("Could not refresh file {s}\n", path);
		return -3;
	}

	//files[f].last_reloaded = spec.tv_sec;

	fseek(fp, 0, SEEK_END);
	file->size = ftell(fp);
	rewind(fp);

	if (file->size <= 0) {
		file->buffer = nullptr;
		return 0;
	}

	file->buffer = new u8[file->size];
	fread(file->buffer, 1, file->size, fp);
	fclose(fp);

	u32 crc = 0xffffffff;
	u8 *data = file->buffer;
	int sz = file->size;

	for (int i = 0; i < sz; i++)
		crc = (crc >> 8) ^ crc32_poly8_lookup[(u8)crc ^ data[i]];

	file->crc = ~crc;
	return 0;
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
static long fs_add_entries(Vector<Entry>& entries, Pool& name_pool, int parent_dir, String& path, Sort_Buffer *sorter, int count)
{
	std::sort(sorter->offsets, sorter->offsets + count, [](char *a, char *b) { return strcmp(a, b) < 0; });

	struct stat st;
	int first = entries.size;

	for (int i = 0; i < count; i++) {
		int name_idx = name_pool.head;
		int len = name_pool.add(sorter->offsets[i]);

		path.add(name_pool.at(name_idx), len);
		int res = stat(path.data(), &st);

		sorter->indices[i] = i;
		sorter->modified[i] = st.st_mtim.tv_sec;
		sorter->created[i] = st.st_ctim.tv_sec;

		int next = i < count-1 ? entries.size+1 : -1;

		Entry e = Entry::make_empty();
		e.next.alpha = next;
		e.parent = parent_dir;
		e.name_idx = name_idx;
		e.created_time = st.st_ctim.tv_sec;
		e.modified_time = st.st_mtim.tv_sec;

		if constexpr (std::is_same_v<Entry, FS_File>) {
			refresh_file(&e, path.data());
		}

		entries.add(e);

		path.scrub(len);
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

static int fs_init_directory(Filesystem& fs, String& path, int parent_dir, Sort_Buffer *sort_buf_dirs, Sort_Buffer *sort_buf_files, Pool& allowed_dirs, char *list_dir_buffer)
{
	int path_fd = openat(AT_FDCWD, path.data(), O_RDONLY | O_NONBLOCK | O_DIRECTORY | O_CLOEXEC);
	if (path_fd < 0)
		return -1;

	auto dir = (linux_dirent64*)list_dir_buffer;
	int read_sz = getdents64(path_fd, dir, LIST_DIR_LEN);

	close(path_fd);

	if (read_sz <= 0)
		return -2;

	int was_empty_tree = fs.total_name_tree_size == 0;

	int off = 0;
	int n_files = 0;
	int n_dirs = 0;

	while (off < read_sz) {
		int record_len = dir->d_reclen;
		char *name = dir->d_name;

		if (name[0] == '.' && (!name[1] || (name[1]=='.' && !name[2]) || (name[1]=='g' && name[2]=='i' && name[3]=='t' && !name[4]))) {
			dir = (linux_dirent64*)((char*)dir + record_len);
			off += record_len;
			continue;
		}

		Sort_Buffer *sorter = sort_buf_files;
		int *n = &n_files;

		if (dir->d_type == DT_DIR) {
			bool allowed = true;
			if (was_empty_tree) {
				allowed = false;
				int idx = 0;
				while (idx < allowed_dirs.head) {
					char *str = &allowed_dirs.buf[idx];
					if (!strcmp(name, str)) {
						allowed = true;
						break;
					}
					idx += strlen(str) + 1;
				}
			}
			if (allowed) {
				sorter = sort_buf_dirs;
				n = &n_dirs;
			}
		}

		// TODO: do something when this happens
		if (*n >= LIST_DIR_MAX_FILES) {
			*n += 1;
			break;
		}

		fs.total_name_tree_size += path.len + strlen(name) + (dir->d_type == DT_DIR);

		sorter->offsets[*n] = name;
		*n += 1;

		dir = (linux_dirent64*)((char*)dir + record_len);
		off += record_len;
	}

	if (n_dirs > 0) {
		int cur = fs.dirs.size;
		long info = fs_add_entries(fs.dirs, fs.name_pool, parent_dir, path, sort_buf_dirs, n_dirs);

		fs.dirs[parent_dir].first_dir = (FS_Next){
			/*.alpha =*/ cur,
			/*.modified =*/ cur + (int)(info >> 32),
			/*.created =*/ cur + (int)info
		};
	}
	if (n_files > 0) {
		int cur = fs.files.size;
		long info = fs_add_entries(fs.files, fs.name_pool, parent_dir, path, sort_buf_files, n_files);

		fs.dirs[parent_dir].first_file = (FS_Next){
			/*.alpha =*/ cur,
			/*.modified =*/ cur + (int)(info >> 32),
			/*.created =*/ cur + (int)info
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

		fs_init_directory(fs, path, i, sort_buf_dirs, sort_buf_files, allowed_dirs, list_dir_buffer);

		path.scrub(len + 1);
	}

	return 0;
}

int Filesystem::init_at(const char *initial_path, Pool& allowed_dirs, char *list_dir_buffer)
{
	name_pool.init(512);

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

	total_name_tree_size = 0;
	files.resize(0);
	dirs.resize(0);
	dirs.add(FS_Directory::make_empty());

	String path(starting_path);
	return fs_init_directory(*this, path, 0, &sort_buf_dirs, &sort_buf_files, allowed_dirs, list_dir_buffer);
}

int Filesystem::lookup_file(const char *path, int max_len)
{
	const char *p = path;
	int parent = 0;

	while (*p) {
		while (*p == '/')
			p++;

		if (*p == 0 || (max_len > 0 && p >= &path[max_len]))
			return -1;

		int len = 0;
		while (p[len] && p[len] != '/' && !(max_len > 0 && &p[len] >= &path[max_len]))
			len++;

		if (!(max_len > 0 && &p[len] >= &path[max_len]) && p[len] == '/') {
			int idx = dirs[parent].first_dir.alpha;
			while (idx >= 0) {
				char *name = name_pool.at(dirs[idx].name_idx);
				int name_len = strlen(name);
				if (len == name_len && !memcmp(p, name, len)) {
					break;
				}
				idx = dirs[idx].next.alpha;
			}
			if (idx < 0)
				break;

			parent = idx;
		}
		else {
			int idx = dirs[parent].first_file.alpha;
			while (idx >= 0) {
				char *name = name_pool.at(files[idx].name_idx);
				int name_len = strlen(name);
				if (len == name_len && !memcmp(p, name, len)) {
					return idx;
				}
				idx = files[idx].next.alpha;
			}
			break;
		}

		p += len + 1;
	}

	return -1;
}

int Filesystem::lookup_dir(const char *path, int max_len)
{
	if (!path || !path[0] || (path[0] == '/' && !path[1]))
		return 0;

	const char *p = path;
	int parent = 0;

	while (*p) {
		while (*p == '/')
			p++;

		if (*p == 0 || (max_len > 0 && p >= &path[max_len]))
			return -1;

		int len = 0;
		while (p[len] && p[len] != '/' && !(max_len > 0 && &p[len] >= &path[max_len]))
			len++;

		int idx = dirs[parent].first_dir.alpha;
		while (idx >= 0) {
			char *name = name_pool.at(dirs[idx].name_idx);
			int name_len = strlen(name);
			if (len == name_len && !memcmp(p, name, len)) {
				break;
			}
			idx = dirs[idx].next.alpha;
		}
		if (idx < 0)
			break;

		if (p[len] != '/')
			return idx;

		parent = idx;
		p += len + 1;
	}

	return -1;
}

void Filesystem::walk(int dir_idx, int order, void (*dir_cb)(Filesystem*, int, void*), void (*file_cb)(Filesystem*, int, void*), void *cb_data)
{
	int next_levels[100];
	next_levels[0] = -1;

	int lvl_cur = 0;
	int total_files_size = 0;
	int didx = dir_idx;
	bool skip_to_files = false;

	while (didx >= 0) {
		FS_Directory& dir = dirs[didx];

		if (!skip_to_files) {
			if (dir_cb)
				dir_cb(this, didx, cb_data);

			next_levels[lvl_cur] = didx;

			if (dir.first_dir.array[order] >= 0) {
				didx = dir.first_dir.array[order];
				lvl_cur++;
				continue;
			}
		}
		skip_to_files = false;

		int f = dir.first_file.array[order];
		while (f >= 0) {
			if (file_cb)
				file_cb(this, f, cb_data);

			f = files[f].next.array[order];
		}

		if (lvl_cur > 0 && dir.next.array[order] >= 0) {
			didx = dir.next.array[order];
			continue;
		}

		while (true) {
			lvl_cur--;
			if (lvl_cur <= 0)
				break;

			int next = next_levels[lvl_cur]; //.next.array[order];
			if (next > 0) {
				skip_to_files = true;
				didx = next;
				break;
			}
		}

		if (lvl_cur <= 0)
			break;
	}
}

int Filesystem::get_path(char *buf, int ancestor, int parent, char *name)
{
	int name_len = name ? strlen(name) : 0;
	if (name_len >= 1023)
		return -1;

	int pos = 0;
	for (int i = 0; i < name_len && pos < 1023; i++)
		buf[pos++] = name[name_len-i-1];
	buf[pos++] = '/';

	while (parent >= 0) {
		int name_idx = dirs[parent].name_idx;
		if (name_idx < 0)
			break;

		name = name_pool.at(name_idx);
		name_len = strlen(name);
		if (name_len >= 1023)
			return -1;

		for (int i = 0; i < name_len && pos < 1023; i++)
			buf[pos++] = name[name_len-i-1];

		if (parent == ancestor)
			break;

		buf[pos++] = '/';

		parent = dirs[parent].parent;
	}

	for (int i = 0; i < pos/2; i++) {
		char c = buf[i];
		buf[i] = buf[pos-i-1];
		buf[pos-i-1] = c;
	}

	return pos;
}

bool Filesystem::add_file_to_html(String *html, const char *path)
{
	int fidx = lookup_file(path);
	if (fidx >= 0) {
		//refresh_file(fidx);
		if (files[fidx].buffer) {
			html->add((const char*)files[fidx].buffer, files[fidx].size);
			return true;
		}
	}
	return false;
}
