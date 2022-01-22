#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include "website.h"

#define LISTEN_BACKLOG 10

int make_file_db(File_Database& db, const char *fname) {
	FILE *f = fopen(fname, "r");
	if (!f) {
		log_error("Could not open {s}", fname);
		return 1;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	if (sz <= 0) {
		log_error("Failed to read from file-map.txt");
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
					log_info("file-map.txt: line {d} contains too many parameters, wrapping around", line_no);
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

int lookup_file(File_Database& db, char *name, int len) {
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

void write_http_response(int fd, const char *status, const char *content_type, const char *data, int size) {
	char hdr[256];
	char datetime[96];

	time_t t = time(nullptr);
	struct tm *utc = gmtime(&t);
	strftime(datetime, 96, "%a, %d %m %Y %H:%M:%S", utc);

	if (!data)
		size = 0;

	String response(hdr, 256);
	response.reformat(
		"HTTP/1.1 {s}\r\n"
		"Date: {s} GMT\r\n"
		"Server: jackbendtsen.com.au\r\n"
		"Content-Type: {s}\r\n"
		"Content-Length: {d}\r\n\r\n",
		status, datetime, content_type, size
	);

	write(fd, response.data(), response.size());

	if (data)
		write(fd, data, size);
}

void serve_page(File_Database& internal, int fd, char *name, int len) {
	if (!name || !len) {
		serve_home_page(internal, fd);
	}
	else if (!memcmp(name, "blog", 4)) {
		if (len > 5 && name[4] == '/') {
			serve_specific_blog(internal, fd, name + 5, len - 5);
		}
		else if (len == 4) {
			serve_blog_overview(internal, fd);
		}
	}
	else if (!memcmp(name, "projects", 8)) {
		if (len > 9 && name[8] == '/') {
			serve_specific_project(internal, fd, name + 9, len - 9);
		}
		else if (len == 8) {
			serve_projects_overview(internal, fd);
		}
	}
	else {
		serve_404(internal, fd);
	}
}

#define IS_SPACE(c) (c == ' ' || c == '\t' || c == '\r' || c == '\n')

int serve_request(int fd, File_Database& global, File_Database& internal) {
	char buf[1024];

	int sz = read(fd, buf, 1024);
	if (sz < 0)
		return 0;

	if (sz <= 10) {
		log_info("Request was too small ({d} bytes)", sz);
		return 1;
	}
	else {
		buf[sz-1] = 0;
		puts(buf);
	}

	if (sz == 1024) {
		char fluff[256];
		while (read(fd, fluff, 256) == 256);
	}

	char *end = &buf[sz-1];

	if (buf[0] != 'G' || buf[1] != 'E' || buf[2] != 'T') {
		log_info("Discarding request (was not a GET request)");
		return 2;
	}

	char *p = &buf[3];
	while (IS_SPACE(*p) && p < end)
		p++;

	p++;
	if (!IS_SPACE(*p)) {
		char *name = p;
		while (!IS_SPACE(*p) && p < end)
			p++;

		int len = p - name;
		int file = lookup_file(global, name, len);
		if (file >= 0) {
			File *f = &global.files[file];
			write_http_response(fd, "200 OK", f->type, (char*)f->buffer, f->size);
		}
		else {
			serve_page(internal, fd, name, len);
		}
	}
	else { // home page
		serve_home_page(internal, fd);
	}

	return 0;
}

int main() {
	init_logger();

	File_Database global, internal;
	if (make_file_db(global, "global-files.txt") != 0)
		return 1;
	if (make_file_db(internal, "internal-files.txt") != 0)
		return 2;

	int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	struct sockaddr_in addr;
	auto addr_ptr = (struct sockaddr *)&addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(8080);

	bind(sock_fd, addr_ptr, sizeof(addr));
	listen(sock_fd, LISTEN_BACKLOG);

	while (true) {
		socklen_t addr_len = sizeof(addr);
		int fd = accept(sock_fd, addr_ptr, &addr_len);

		fd_set set;
		FD_ZERO(&set);
		FD_SET(fd, &set);
		select(fd + 1, &set, nullptr, nullptr, nullptr);

		int flags = fcntl(fd, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(fd, F_SETFL, flags);

		serve_request(fd, global, internal);
		close(fd);
	}

	close(sock_fd);
	return 0;
}
