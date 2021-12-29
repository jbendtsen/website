#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include "website.h"

#define LISTEN_BACKLOG 10

/*
struct File {
	time_t last_reloaded;
	int flags;
	int size;
	u8 *buffer;
	char *label;
	char *type;
	char *fname;
};

struct File_Database {
	//int size;
	int n_files;
	char *map_buffer;
	char *label_pool;
	char *fname_pool;
	char *type_pool;
	File *files;
};
*/

int make_file_db(File_Database& db) {
	FILE *f = fopen("file-map.txt", "r");
	if (!f) {
		log_error("Could not open file-map.txt");
		return 1;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	if (sz <= 0) {
		log_error("Failed to read from file-map.txt");
		return 2;
	}

	db.map_buffer = new char[sz * 4];
	db.label_pool = &db.map_buffer[sz];
	db.fname_pool = &db.map_buffer[sz * 2];
	db.type_pool  = &db.map_buffer[sz * 3];
	db.n_files = 0;

	fread(buf, 1, sz, f);
	fclose(f);

	int line_no = 0;
	int pool = 0;
	bool was_ch = false;
	int offsets[] = {0, 0, 0};

	for (int i = 0; i < sz; i++) {
		char c = db.map_buffer[i];
		char ch = c == ' ' || c == '\n' || c == '\r' ? 0 : c;

		if (ch || was_ch) {
			db.map_buffer[sz * (pool + 1) + offsets[pool]] = ch;
			offsets[pool]++;
		}

		if (!was_ch) {
			if (c == ' ') {
				pool++;
				if (pool >= 3) {
					pool = 0;
					log_info("file-map.txt: line {d} contains too many parameters, wrapping around", line_no);
				}
			}
			else if (c == '\r' || c == '\n') {
				pool = 0;
				db.n_files++;
			}
		}

		if (c == '\n')
			line_no++;

		was_ch = ch != 0;
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
}

int lookup_file(File_Database& db, char *name, int len) {
	char *p = db.label_pool;
	int file = -1;
	int off = 0;
	int matches = 0;

	while (file < db.n_files) {
		char c = *p++;
		if (c == name[off]) {

		}
	}

	if (file < 0)
		return -1;

	struct timespec spec;
	long t = clock_gettime(CLOCK_BOOTTIME, &spec);

	File *f = &db.files[file];
	long threshold = f->last_reloaded + SECS_UNTIL_RELOAD;

	if (!f->buffer || t >= threshold || threshold <= SECS_UNTIL_RELOAD) {
		if (f->buffer)
			delete[] f->buffer;

		FILE *fp = fopen(fname, "rb");
		if (!fp)
			return -2;

		fseek(fp, 0, SEEK_END);
		int sz = ftell(fp);
		rewind(fp);

		if (sz <= 0)
			return -3;

		f->size = sz;
		f->buffer = new char[sz];
		fread(f->buffer, 1, sz, fp);
		fclose(fp);
	}

	return file;
}

int write_http_response(int fd, char *status, char *content_type, char *data, int size) {
	char buf[256];
	char date[96];

	time_t t = time(nullptr);
	struct tm *utc = gmtime(&t);
	strftime(date, 96, "%a, %d %m %Y %H:%M:%S", utc);

	String response(buf, 256);
	write_formatted_value(response,
		"{s}\r\n"
		"Date: {s} GMT\r\n"
		"Server: jackbendtsen.com.au\r\n"
		"Content-Type: {s}\r\n"
		"Content-Length: {d}\r\n\r\n",
		status, date, content_type, size
	);

	write(fd, response.data(), response.size());
	write(fd, data, size);
}

int serve_page(int fd, char *name, int len) {

}

static File_Database db;

#define IS_SPACE(c) (c == ' ' || c == '\t' || c == '\r' || c == '\n')

int serve_request(int fd) {
	char buf[1024];

	int sz = read(fd, buf, 1024);
	if (sz <= 10) {
		log_info("Request was too small (%d bytes)", sz);
		return 1;
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
		int file = lookup_file(pool, name, len);
		if (file >= 0) {
			File *f = &pool[file];
			write_http_response(fd, "200 OK", f->type, f->buffer, f->size);
		}
		else {
			serve_page(fd, name, len);
		}
	}
	else { // home page
		serve_page(fd, nullptr, 0);
	}
}

int main() {
	init_logger();
	db = make_file_db();

	int sock_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);

	struct sockaddr_in addr;
	auto addr_ptr = (struct sockaddr *)&addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(portno);

	bind(sock_fd, addr_ptr, sizeof(addr));
	listen(sock_fd, LISTEN_BACKLOG);

	while (true) {
		int addr_len = sizeof(addr);
		int fd = accept4(sock_fd, addr_ptr, &addr_len, SOCK_NONBLOCK);

		serve_request(fd);
		close(fd);
	}

	close(sock_fd);
	return 0;
}
