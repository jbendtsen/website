#include <sys/types.h>
#include <sys/socket.h>

#include "website.h"

struct ZIP_Headers {
	struct iovec *cur_iv;
	u8 *cur_data;
	int top_dir;
	u32 cur_offset;
};

struct ZIP_Stats {
	size_t total_file_size;
	int total_name_size;
	int n_entries;
	int top_dir;
};

static void enum_dirs(Filesystem *fs, int didx, void *data) {
	auto stats = (ZIP_Stats*)data;
	stats->n_entries++;

	int top_name = fs->dirs[stats->top_dir].name_idx;
	int path_len = top_name < 0 ? 1 : strlen(fs->name_pool.at(top_name)) + 1;

	int d = didx;
	while (d >= 0 && d != stats->top_dir) {
		path_len += strlen(fs->name_pool.at(fs->dirs[d].name_idx)) + 1;
		d = fs->dirs[d].parent;
	}

	stats->total_name_size += path_len;
}

static void enum_files(Filesystem *fs, int fidx, void *data) {
	auto stats = (ZIP_Stats*)data;
	stats->n_entries++; // n_entries
	stats->total_file_size += (size_t)fs->files[fidx].size; // total_size

	int top_name = fs->dirs[stats->top_dir].name_idx;
	int path_len = top_name < 0 ? 1 : strlen(fs->name_pool.at(top_name)) + 1;

	path_len += strlen(fs->name_pool.at(fs->files[fidx].name_idx));
	int parent = fs->files[fidx].parent;

	while (parent >= 0 && parent != stats->top_dir) {
		path_len += strlen(fs->name_pool.at(fs->dirs[parent].name_idx)) + 1;
		parent = fs->dirs[parent].parent;
	}

	stats->total_name_size += path_len;
}

static void write_dir_header(Filesystem *fs, int didx, void *data)
{
	auto hdr = (ZIP_Headers*)data;

	u8 *p = hdr->cur_data;
	*p++ = 'P';
	*p++ = 'K';
	*p++ = 3;
	*p++ = 4;
	*p++ = 10;
	p += 25;

	char *name = fs->name_pool.at(fs->dirs[didx].name_idx);
	int name_len = 0;

	if (didx == hdr->top_dir) {
		name_len = strlen(name);
		memcpy(p, name, name_len);
	}
	else {
		FS_Directory *dir = &fs->dirs[didx];
		name_len = fs->get_path((char*)p, hdr->top_dir, dir->parent, name);
	}

	name_len++; // add trailing slash
	p[-4] = (u8)name_len;
	p[-3] = (u8)(name_len >> 8);

	p += name_len;
	p[-1] = '/';

	*hdr->cur_iv++ = { hdr->cur_data, (size_t)(p - hdr->cur_data) };
	hdr->cur_data = p;
}

static void write_file_header(Filesystem *fs, int fidx, void *data)
{
	auto hdr = (ZIP_Headers*)data;
	FS_File *file = &fs->files[fidx];

	u8 *p = hdr->cur_data;
	*p++ = 'P';
	*p++ = 'K';
	*p++ = 3;
	*p++ = 4;
	*p++ = 10;
	p += 9;
	*p++ = (u8)file->crc;
	*p++ = (u8)(file->crc >> 8);
	*p++ = (u8)(file->crc >> 16);
	*p++ = (u8)(file->crc >> 24);
	p[0] = p[4] = (u8)file->size;
	p[1] = p[5] = (u8)(file->size >> 8);
	p[2] = p[6] = (u8)(file->size >> 16);
	p[3] = p[7] = (u8)(file->size >> 24);
	p += 12;

	int name_len = fs->get_path((char*)p, hdr->top_dir, file->parent, fs->name_pool.at(fs->files[fidx].name_idx));

	p[-4] = (u8)name_len;
	p[-3] = (u8)(name_len >> 8);
	p += name_len;

	*hdr->cur_iv++ = { hdr->cur_data, (size_t)(p - hdr->cur_data) };
	hdr->cur_data = p;

	if (file->size)
		*hdr->cur_iv++ = { file->buffer, (size_t)file->size };
}

static void write_dir_central(Filesystem *fs, int didx, void *data)
{
	auto hdr = (ZIP_Headers*)data;

	u8 *p = hdr->cur_data;
	*p++ = 'P';
	*p++ = 'K';
	*p++ = 1;
	*p++ = 2;
	*p++ = 30;
	*p++ = 3;
	*p++ = 10;
	p += 21;

	char *name = fs->name_pool.at(fs->dirs[didx].name_idx);
	int name_len = 0;

	if (didx == hdr->top_dir) {
		name_len = strlen(name);
		memcpy(&p[18], name, name_len);
	}
	else {
		FS_Directory *dir = &fs->dirs[didx];
		name_len = fs->get_path((char*)&p[18], hdr->top_dir, dir->parent, name);
	}

	name_len++; // add trailing slash

	*p++ = (u8)name_len;
	*p++ = (u8)(name_len >> 8);
	p += 8;
	*p++ = 0x10;
	*p++ = 0;
	*p++ = 0xb6; // permissions 1:   w-rw-rw-
	*p++ = 0x41; // permissions 2: dr
	*p++ = (u8)hdr->cur_offset;
	*p++ = (u8)(hdr->cur_offset >> 8);
	*p++ = (u8)(hdr->cur_offset >> 16);
	*p++ = (u8)(hdr->cur_offset >> 24);

	p += name_len;
	p[-1] = '/';

	*hdr->cur_iv++ = { hdr->cur_data, (size_t)(p - hdr->cur_data) };
	hdr->cur_data = p;
	hdr->cur_offset += 30 + name_len;
}

static void write_file_central(Filesystem *fs, int fidx, void *data)
{
	auto hdr = (ZIP_Headers*)data;
	FS_File *file = &fs->files[fidx];

	u8 *p = hdr->cur_data;
	*p++ = 'P';
	*p++ = 'K';
	*p++ = 1;
	*p++ = 2;
	*p++ = 30;
	*p++ = 3;
	*p++ = 10;
	p += 9;
	*p++ = (u8)file->crc;
	*p++ = (u8)(file->crc >> 8);
	*p++ = (u8)(file->crc >> 16);
	*p++ = (u8)(file->crc >> 24);
	p[0] = p[4] = (u8)file->size;
	p[1] = p[5] = (u8)(file->size >> 8);
	p[2] = p[6] = (u8)(file->size >> 16);
	p[3] = p[7] = (u8)(file->size >> 24);
	p += 8;

	int name_len = fs->get_path((char*)&p[18], hdr->top_dir, file->parent, fs->name_pool.at(fs->files[fidx].name_idx));

	*p++ = (u8)name_len;
	*p++ = (u8)(name_len >> 8);
	p += 10;
	*p++ = 0xb6; // permissions 1:   w-rw-rw-
	*p++ = 0x81; // permissions 2: -r
	*p++ = (u8)hdr->cur_offset;
	*p++ = (u8)(hdr->cur_offset >> 8);
	*p++ = (u8)(hdr->cur_offset >> 16);
	*p++ = (u8)(hdr->cur_offset >> 24);

	p += name_len;

	*hdr->cur_iv++ = { hdr->cur_data, (size_t)(p - hdr->cur_data) };
	hdr->cur_data = p;
	hdr->cur_offset += 30 + name_len + file->size;
}

// Creates an uncompressed .zip file from a directory by creating ZIP headers for each file,
//  then writes the output as a scattered set of messages to the given socket
void write_zip_as_response(Filesystem& fs, int dir_idx, Response& response)
{
	response.format = RESPONSE_MULTI;

	ZIP_Stats stats = {0};
	stats.top_dir = dir_idx;
	fs.walk(dir_idx, FS_ORDER_ALPHA, enum_dirs, enum_files, &stats);

	int zip_headers_size = 22 + stats.n_entries * (30 + 46) + stats.total_name_size * 2;

	// combined file size + combined zip header size + size of ZIP "end of central directory record"
	size_t response_size = stats.total_file_size + zip_headers_size;

	char datetime[128];
	char http_buf[512];

	String http((char*)http_buf, 512);
	get_datetime(datetime);

	http.reformat(
		"HTTP/1.1 200 OK\r\n"
		"Date: {s} GMT\r\n"
		"Server: jackbendtsen.com.au\r\n"
		"Content-Disposition: attachment\r\n"
		"Content-Type: application/zip\r\n"
		"Content-Length: {d}\r\n\r\n",
		datetime, response_size
	);

	int n_msgs = 3 * stats.n_entries + 2;
	int buf_size = sizeof(struct msghdr) + n_msgs * sizeof(struct iovec) + http.len + zip_headers_size;

	response.html.resize(buf_size + 4); // +4 just in case
	u8 *buf = (u8*)response.html.data();
	memset(buf, 0, buf_size);

	auto msg = (struct msghdr*)buf;
	msg->msg_iov = (struct iovec*)&buf[sizeof(struct msghdr)];

	memcpy(&msg->msg_iov[n_msgs], http.data(), http.len);
	msg->msg_iov[0] = { &msg->msg_iov[n_msgs], (size_t)http.len };

	ZIP_Headers zip = {
		&msg->msg_iov[1],
		&buf[buf_size - zip_headers_size],
		dir_idx,
		0
	};

	fs.walk(dir_idx, FS_ORDER_ALPHA, write_dir_header, write_file_header, &zip);
	u8 *zip_local_end = zip.cur_data;
	fs.walk(dir_idx, FS_ORDER_ALPHA, write_dir_central, write_file_central, &zip);

	u8 *zip_end = &buf[buf_size - 22];
	memset(zip_end, 0, 22);

	{
		u8 *p = zip_end;
		*p++ = 'P';
		*p++ = 'K';
		*p++ = 5;
		*p++ = 6;
		*(u32*)p = 0;
		p += 4;
		p[0] = p[2] = (u8)stats.n_entries;
		p[1] = p[3] = (u8)(stats.n_entries >> 8);
		p += 4;

		u32 central_offset = zip.cur_offset;
		u32 central_size = (u32)(zip.cur_data - zip_local_end);

		*p++ = (u8)central_size;
		*p++ = (u8)(central_size >> 8);
		*p++ = (u8)(central_size >> 16);
		*p++ = (u8)(central_size >> 24);
		*p++ = (u8)central_offset;
		*p++ = (u8)(central_offset >> 8);
		*p++ = (u8)(central_offset >> 16);
		*p++ = (u8)(central_offset >> 24);
	}

	*zip.cur_iv++ = { zip_end, 22 };
	msg->msg_iovlen = (size_t)((u8*)zip.cur_iv - &buf[sizeof(struct msghdr)]) / sizeof(struct iovec);
}
