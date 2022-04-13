#include <cstdio>
#include <cstdlib>

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/signalfd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include "website.h"

#define MAX_THREADS 4096

#define THREAD_FLAG_BUSY     1
#define THREAD_FLAG_CREATED  2

struct Thread {
	u32 flags;
	pthread_t handle;
	int comms_fds[2];
	Response response;
};

static void close_thread_pipe(Thread *thread)
{
	int fds[2];
	fds[0] = thread->comms_fds[0];
	thread->comms_fds[0] = 0;
	fds[1] = thread->comms_fds[1];
	thread->comms_fds[1] = 0;

	if (fds[0] > 0) close(fds[0]);
	if (fds[1] > 0) close(fds[1]);
}

static void send_response(Response *res, int fd)
{
	if (res->type == RESPONSE_FILE) {
		char *mime = res->html.data();
		write_http_header(fd, res->status, mime, res->file_size);
		write(fd, res->file_buffer, res->file_size);
	}
	else if (res->type == RESPONSE_HTML) {
		write_http_header(fd, res->status, "text/html", res->html.len);
		write(fd, res->html.data(), res->html.len);
	}
	else if (res->type == RESPONSE_MULTI) {
		auto msg = (struct msghdr*)res->html.data();
		sendmsg(fd, msg, 0);
	}
}

static void *handle_response(void *data)
{
	Thread *thread = (Thread*)data;

	while (true) {
		int request_fd = 0;
		read(thread->comms_fds[0], &request_fd, sizeof(int));

		if (request_fd < 0)
			break;

		Response *res = &thread->response;
		send_response(res, request_fd);

		close(request_fd);
		thread->flags &= ~THREAD_FLAG_BUSY;
	}

	close_thread_pipe(thread);
	return NULL;
}

static void serve_response(Thread *t, int request_fd)
{
	t->flags |= THREAD_FLAG_BUSY;

	if (t->comms_fds[1] <= 0)
		pipe(t->comms_fds);

	write(t->comms_fds[1], &request_fd, sizeof(int));

	if ((t->flags & THREAD_FLAG_CREATED) == 0) {
		t->flags |= THREAD_FLAG_CREATED;

		int res = pthread_create(&t->handle, NULL, handle_response, t);
		if (res != 0) {
			t->flags = 0;
			close_thread_pipe(t);
			log_info("Failed to create thread (pthread_create() returned {d})", res);
			return;
		}
	}
}

void serve_page(Filesystem& fs, Response& response, char *name, int len)
{
	if (!name || !len) {
		serve_home_page(fs, response);
	}
	else if (!memcmp(name, "blog", 4)) {
		if (len > 5 && name[4] == '/') {
			serve_specific_blog(fs, response, name + 5, len - 5);
		}
		else if (len == 4) {
			serve_blog_overview(fs, response);
		}
	}
	else if (!memcmp(name, "projects", 8)) {
		if (len > 9 && name[8] == '/') {
			serve_specific_project(fs, response, name + 9, len - 9);
		}
		else if (len == 8) {
			serve_projects_overview(fs, response);
		}
	}
	else {
		serve_404(fs, response);
	}
}

static void produce_response(char *header, int sz, Response& response, File_Database *global, Filesystem *fs)
{
	char *end = &header[sz-1];

	char *p = &header[3];
	while (IS_SPACE(*p) && p < end)
		p++;
	p++;

	char *name = NULL;
	int len = 0;

	// default value, should be changed if the request/response doesn't succeed
	response.status = "200 OK";

	if (!IS_SPACE(*p)) {
		name = p;
		while (!IS_SPACE(*p) && p < end)
			p++;

		len = p - name;

		char *q = p;
		char *accept_types = NULL;
		while (q < end-8 && *q) {
			int i = 0;
			for (; i < 8; i++) {
				if (q[i] != "\nAccept:"[i])
					break;
			}
			if (i == 8) {
				accept_types = q + 8;
				break;
			}
			q++;
		}

		bool allow_html = true;

		if (accept_types) {
			allow_html = false;

			while (q < end-4 && *q && *q != '\r' && *q != '\n') {
				if (q[0] == 'h' && q[1] == 't' && q[2] == 'm' && q[3] == 'l') {
					allow_html = true;
					break;
				}
				q++;
			}
		}

		if (!allow_html) {
			const char *mime = NULL;
			char *buffer = NULL;
			int size = 0;

			int file = global->lookup_file(name, len);
			if (file >= 0) {
				DB_File *f = &global->files[file];
				mime = f->type;
				buffer = (char*)f->buffer;
				size = f->size;
			}
			else {
				fs->lookup(nullptr, &file, name, len);
				if (file >= 0) {
					char *q = &name[len-1];
					while (q > name && *q != '.')
						q--;

					if (q > name)
						mime = lookup_mime_ext(q+1);

					//fs.refresh_file(file);
					FS_File *f = &fs->files[file];
					buffer = (char*)f->buffer;
					size = f->size;
				}
			}

			if (buffer && size) {
				if (!mime)
					mime = "text/plain";

				response.type = RESPONSE_FILE;
				response.html.resize(0);
				response.html.add(mime);
				response.file_size = size;
				response.file_buffer = buffer;

				return;
			}
		}
	}

	response.type = RESPONSE_HTML;
	response.html.resize(0);
	serve_page(*fs, response, name, len);
}

static int read_request_header(int fd, char *buf, int max_len)
{
	int read_fd = fd;
	if (read_fd == STDOUT_FILENO)
		read_fd = STDIN_FILENO;

	int sz = read(read_fd, buf, max_len);
	if (sz < 0) {
		log_error("read() failed, errno={d}\n", errno);
		return -1;
	}
	if (sz <= 4) {
		log_info("Request was too small ({d} bytes)", sz);
		return -2;
	}
	if (buf[0] != 'G' || buf[1] != 'E' || buf[2] != 'T') {
		log_info("Discarding request (was not a GET request)");
		return -3;
	}

	buf[sz-1] = 0;
	puts(buf);

	if (sz == max_len) {
		char fluff[256];
		while (read(read_fd, fluff, 256) == 256);
	}

	return sz;
}

static int cancel_fd = -1;

static void http_loop(File_Database *global, Filesystem *fs)
{
	char header[1024];

	int sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock_fd < 0) {
		log_error("socket() failed, errno={d}\n", errno);
		return;
	}

	int yes = 1;
	setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	int flags = fcntl(sock_fd, F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(sock_fd, F_SETFL, flags);

	struct sockaddr_in addr = {0};
	auto addr_ptr = (struct sockaddr *)&addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(8080);

	if (bind(sock_fd, addr_ptr, sizeof(addr)) < 0) {
		log_error("bind() failed, errno={d}\n", errno);
		return;
	}

	if (listen(sock_fd, MAX_THREADS) < 0) {
		log_error("listen() failed, errno={d}\n", errno);
		return;
	}

	Thread *threads = new Thread[MAX_THREADS]();

	while (true) {
		fd_set set;
		FD_ZERO(&set);
		FD_SET(sock_fd, &set);

		int highest_fd = sock_fd;
		if (cancel_fd >= 0) {
			FD_SET(cancel_fd, &set);
			if (cancel_fd > highest_fd)
				highest_fd = cancel_fd;
		}

		select(highest_fd + 1, &set, nullptr, nullptr, nullptr);

		if (cancel_fd >= 0 && FD_ISSET(cancel_fd, &set)) {
			log_info("Exiting");
			break;
		}

		socklen_t addr_len = sizeof(addr);
		int fd = accept(sock_fd, addr_ptr, &addr_len);
		if (fd < 0) {
			log_error("accept() failed, errno={d}\n", errno);
			break;
		}

		FD_ZERO(&set);
		FD_SET(fd, &set);
		select(fd + 1, &set, nullptr, nullptr, nullptr);

		int flags = fcntl(fd, F_GETFL);
		flags |= O_NONBLOCK;
		fcntl(fd, F_SETFL, flags);

		int sz = read_request_header(fd, header, 1024);
		if (sz <= 0) {
			close(fd);
			continue;
		}

		int tidx = -1;
		for (int i = 0; i < MAX_THREADS; i++) {
			if ((threads[i].flags & THREAD_FLAG_BUSY) == 0) {
				tidx = i;
				break;
			}
		}

		if (tidx < 0) {
			log_info("Could not find a thread to respond with");
			close(fd);
			continue;
		}

		produce_response(header, sz, threads[tidx].response, global, fs);

		// the responding thread is responsible for closing the connection
		serve_response(&threads[tidx], fd);
	}

	close(sock_fd);

	for (int i = 0; i < MAX_THREADS; i++) {
		if (threads[i].flags & THREAD_FLAG_CREATED) {
			int fd = threads[i].comms_fds[1];
			if (fd > 0) {
				int msg = -1;
				write(fd, &msg, sizeof(int));
			}
			pthread_join(threads[i].handle, NULL);
		}
	}

	delete[] threads;
}

int main()
{
	sigset_t sig_mask = {0};

	sigemptyset(&sig_mask);
	sigaddset(&sig_mask, SIGINT);
	sigprocmask(SIG_BLOCK, &sig_mask, nullptr);

	cancel_fd = signalfd(-1, &sig_mask, 0);

	init_logger();

	File_Database *global = new File_Database();
	if (global->init("global-files.txt") != 0) {
		delete global;
		return 1;
	}

	char *list_dir_buffer = new char[LIST_DIR_LEN];

	Pool allowed_dirs;
	allowed_dirs.init(256);
	allowed_dirs.add("content");
	allowed_dirs.add("client");

	Filesystem *fs = new Filesystem();
	fs->init_at(".", allowed_dirs, list_dir_buffer);

#ifdef DEBUG
	char header[1024];
	Response response;

	while (true) {
		log_info("");

		int sz = read_request_header(STDIN_FILENO, header, 1024);
		if (sz <= 0)
			break;

		produce_response(header, sz, response, global, fs);
		send_response(&response, STDOUT_FILENO);
	}
#else
	http_loop(global, fs);
#endif

	if (cancel_fd >= 0) close(cancel_fd);

	delete[] list_dir_buffer;
	delete fs;
	delete global;

	return 0;
}
