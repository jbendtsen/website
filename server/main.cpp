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
	//char *label;
	char *fname;
};

struct File_Pool {
	int size;
	int n_files;
	char *label_pool;
	//u32 *pool;
	File *files;
};
*/

int make_file_pool(File_Pool& fp) {
	FILE *f = fopen("file-map.txt", "r");
	if (!f) {
		log_error("bruh");
		return 1;
	}


}

/*
int main() {
	Bucket_Array<1024, 64> response;
	response.append("Hello!");
}
*/

static File_Pool pool;

int serve_request(int fd) {

}

int main() {
	init_logger();
	pool = make_file_pool();

	int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	struct sockaddr_in addr;
	auto addr_ptr = (struct sockaddr *)&addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(portno);

	bind(sock_fd, addr_ptr, sizeof(addr));
	listen(sock_fd, LISTEN_BACKLOG);

	while (true) {
		int addr_len = sizeof(addr);
		int fd = accept(sock_fd, addr_ptr, &addr_len);

		serve_request(fd);
		close(fd);
	}

	close(sock_fd);
	return 0;
}
