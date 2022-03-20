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

void write_http_response(int request_fd, const char *status, const char *content_type, const char *data, int size) {
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

	write(request_fd, response.data(), response.size());

	if (data)
		write(request_fd, data, size);
}

void serve_page(Filesystem& fs, int request_fd, char *name, int len) {
	if (!name || !len) {
		serve_home_page(fs, request_fd);
	}
	else if (!memcmp(name, "blog", 4)) {
		if (len > 5 && name[4] == '/') {
			serve_specific_blog(fs, request_fd, name + 5, len - 5);
		}
		else if (len == 4) {
			serve_blog_overview(fs, request_fd);
		}
	}
	else if (!memcmp(name, "projects", 8)) {
		if (len > 9 && name[8] == '/') {
			serve_specific_project(fs, request_fd, name + 9, len - 9);
		}
		else if (len == 8) {
			serve_projects_overview(fs, request_fd);
		}
	}
	else {
		serve_404(fs, request_fd);
	}
}

#define IS_SPACE(c) (c == ' ' || c == '\t' || c == '\r' || c == '\n')

int serve_request(int request_fd, File_Database& global, Filesystem& fs) {
	char buf[1024];

	int read_fd = request_fd;
	if (read_fd == STDOUT_FILENO)
		read_fd = STDIN_FILENO;

	int sz = read(read_fd, buf, 1024);
	if (sz < 0)
		return 0;

	if (sz <= 4) {
		log_info("Request was too small ({d} bytes)", sz);
		return 1;
	}
	else {
		buf[sz-1] = 0;
		puts(buf);
	}

	if (sz == 1024) {
		char fluff[256];
		while (read(read_fd, fluff, 256) == 256);
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
		int file = global.lookup_file(name, len);
		if (file >= 0) {
			DB_File *f = &global.files[file];
			write_http_response(request_fd, "200 OK", f->type, (char*)f->buffer, f->size);
		}
		else {
			serve_page(fs, request_fd, name, len);
		}
	}
	else { // home page
		serve_home_page(fs, request_fd);
	}

	return 0;
}

void http_loop(File_Database& global, Filesystem& fs)
{
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

		serve_request(fd, global, fs);
		close(fd);
	}

	close(sock_fd);
}

int main() {
	init_logger();

	File_Database global;
	if (global.init("global-files.txt") != 0)
		return 1;

	char *list_dir_buffer = new char[LIST_DIR_LEN];

	Filesystem fs;
	fs.init_at(".", list_dir_buffer);

#ifdef DEBUG
	while (true) {
		serve_request(STDOUT_FILENO, global, fs);
	}
#else
	http_loop(global, fs);
#endif

	delete[] list_dir_buffer;

	return 0;
}
