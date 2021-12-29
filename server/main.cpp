#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "website.h"

#define LISTEN_BACKLOG 10

/*
struct File {
	int flags;
	int size;
	u8 *buffer;
	char *label;
	char *type;
	char *fname;
};

struct File_Pool {
	int size;
	int n_files;
	char *label_pool;
	char *type_pool;
	File *files;
};
*/

int make_file_pool(File_Pool& fp) {
	FILE *f = fopen("file-map.txt", "r");
	if (!f) {
		log_error("Could not open file-map.txt");
		return 1;
	}

	fseek(f, 0, SEEK_END);
	int sz = ftell(f);
	rewind(f);

	if (sz <= 0) {
		log_error("Failed to read from file-map.txt")
	}

	char *pool = new char[sz];
	fread(pool, 1, sz, f);
	fclose(f);


}

int lookup_file(File_Pool& fp, char *str, int len) {

}

/*
int main() {
	Bucket_Array<1024, 64> response;
	response.append("Hello!");
}
*/

static File_Pool pool;

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
			char date[100];
			format("{s}, {d} {d} {d} {d}:{d}:{d}");

			String response(buf, 1024);
			write_formatted_value(response,
				"200 OK\r\n"
				"Date: {s} GMT\r\n"
				"Server: jackbendtsen.com.au\r\n"
				"Content-Type: {s}\r\n"
				"Content-Length: {d}\r\n\r\n",
				date, pool.files[file].type, pool.files[file].size
			);

			write(fd, response.data(), response.size());
			write(fd, pool.files[file].buffer, pool.files[file].size);
		}
		else {
			serve_page(name, len);
		}
	}
	else { // home page
		serve_page(nullptr, 0);
	}

}

int main() {
	init_logger();
	pool = make_file_pool();

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
