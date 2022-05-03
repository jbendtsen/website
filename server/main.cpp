#include <cstdio>
#include <cstdlib>

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>
#include <sys/signalfd.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "website.h"

#define MAX_THREADS 4096

#define THREAD_FLAG_BUSY     1
#define THREAD_FLAG_CREATED  2
#define THREAD_FLAG_PIPES    4

// Should some of these fields be atomic?
struct Thread {
	u32 flags;
	pthread_t handle;
	int from_main[2];
	int to_main[2];
	int conn_fd;
	Request request;
	Response response;
};

static void close_thread_pipes(Thread *thread)
{
	int fds[4];
	fds[0] = thread->from_main[0];
	thread->from_main[0] = 0;
	fds[1] = thread->from_main[1];
	thread->from_main[1] = 0;
	fds[2] = thread->to_main[0];
	thread->to_main[0] = 0;
	fds[3] = thread->to_main[1];
	thread->to_main[1] = 0;

	if (fds[0] > 0) close(fds[0]);
	if (fds[1] > 0) close(fds[1]);
	if (fds[2] > 0) close(fds[2]);
	if (fds[3] > 0) close(fds[3]);
}

static void receive_request(int fd, Request& request)
{
	char buf[1024];
	request.str.len = 0;

	int flags = fcntl(fd, F_GETFL);
	flags &= ~O_NONBLOCK;
	fcntl(fd, F_SETFL, flags);

	request.header_size = 0;
	struct pollfd pl = { .fd = fd, .events = POLLIN };

	while (true) {
		poll(&pl, 1, 1000);
		if ((pl.revents & POLLIN) == 0)
			break;

		int res = 0;
		while (true) {
			res = recv(fd, buf, 1024, 0);
			if (res < 1)
				break;

			request.str.add(buf, res);
			if (request.str.len >= 1024*1024) {
				res = -1;
				break;
			}

			poll(&pl, 1, 0);
			if ((pl.revents & POLLIN) == 0)
				break;
		}

		if (res < 0)
			break;

		const char *str = request.str.data();
		int length = 0;

		for (int i = 0; i < request.str.len-3; i++) {
			if (str[i] == '\r' && str[i+1] == '\n' && str[i+2] == '\r' && str[i+3] == '\n') {
				request.header_size = i + 4;
				break;
			}
			if (i >= request.str.len-15)
				continue;

			int j = 0;
			for (; j < 15; j++) {
				char a = str[i+j];
				if (a >= 'A' && a <= 'Z')
					a += 0x20;
				if (a != "content-length:"[j])
					break;
			}
			if (j != 15)
				continue;

			length = 0;
			for (j = i + 15; j < request.str.len; j++) {
				char c = str[j];
				if (c == '\r' || c == '\n')
					break;

				if (c >= '0' && c <= '9') {
					length *= 10;
					length += c - '0';
				}
			}
		}

		if (request.header_size > 0) {
			int left = length - (request.str.len - request.header_size);
			while (left > 0 && request.str.len < 1024*1024) {
				poll(&pl, 1, 1000);
				if ((pl.revents & POLLIN) == 0)
					break;

				int chunk = left < 1024 ? left : 1024;
				res = recv(fd, buf, chunk, 0);
				if (res < 1)
					break;

				request.str.add(buf, res);
				left -= res;
			}
			break;
		}
		else if (res == 0) {
			break;
		}
	}

	if (request.header_size <= 0)
		request.header_size = request.str.len;
}

static void send_response(Response *res, int fd)
{
	struct pollfd ps = {0};
	ps.fd = fd;
	ps.events = POLLOUT;
	poll(&ps, 1, 0);

	if ((ps.revents & POLLOUT) == 0)
		return;

	if (res->format == RESPONSE_FILE) {
		write_http_header(fd, res->status, res->mime, res->file_size);
		send(fd, res->file_buffer, res->file_size, 0);
	}
	else if (res->format == RESPONSE_HTML) {
		write_http_header(fd, res->status, res->mime, res->html.len);
		send(fd, res->html.data(), res->html.len, 0);
	}
	else if (res->format == RESPONSE_MULTI) {
		// It's assumed that the first message in this list contains the HTTP headers
		auto msg = (struct msghdr*)res->html.data();
		sendmsg(fd, msg, 0);
	}
}

static void *handle_socket(void *data)
{
	Thread *thread = (Thread*)data;

	while (true) {
		int request_fd = 0;
		read(thread->from_main[0], &request_fd, sizeof(int));

		if (request_fd < 0)
			break;

		if (request_fd > 0) {
			if (thread->conn_fd > 0)
				close(thread->conn_fd);

			thread->conn_fd = request_fd;
			receive_request(thread->conn_fd, thread->request);

			int res = 0;
			write(thread->to_main[1], &res, 4);
		}
		else if (thread->conn_fd > 0) {
			send_response(&thread->response, thread->conn_fd);
			close(thread->conn_fd);
			thread->conn_fd = -1;
			thread->flags &= ~THREAD_FLAG_BUSY;
		}
	}

	if (thread->conn_fd > 0)
		close(thread->conn_fd);

	close_thread_pipes(thread);
	return NULL;
}

void serve_page(Filesystem& fs, Request& request, Response& response, char *name, int len)
{
	if (!name || !len) {
		serve_home_page(fs, response);
		return;
	}

	int slash_pos = find_character(name, '/', len);
	if (!memcmp(name, "blog", 4)) {
		if (slash_pos > 0 && slash_pos < len-1) {
			int pos = slash_pos + 1;
			serve_specific_blog(fs, response, name + pos, len - pos);
		}
		else {
			serve_blog_overview(fs, response);
		}
	}
	else if (!memcmp(name, "projects", 8)) {
		if (slash_pos > 0 && slash_pos < len-1) {
			serve_specific_project(fs, response, name, slash_pos, len - (slash_pos+1));
		}
		else if (len == 8) {
			serve_projects_overview(fs, response);
		}
	}
	else if (!memcmp(name, "markdown", 8)) {
		serve_markdown_tester(fs, request, response);
	}
	else {
		serve_404(fs, response);
	}
}

static void produce_response(Request& request, Response& response, File_Database *global, Filesystem *fs)
{
	char *header = request.str.data();
	char *end = &header[request.header_size-1];

	log_info("{S}", header, request.header_size);

	char *p = header;
	// move past method
	while (!IS_SPACE(*p) && p < end)
		p++;
	// move to url
	while (IS_SPACE(*p) && p < end)
		p++;
	p++;

	char *q = p;
	request.accept = 0;
	while (q < end-8 && *q) {
		int i;
		for (i = 0; i < 8; i++) {
			char c = q[i];
			if (c >= 'A' && c <= 'Z')
				c += 0x20;
			if (c != "\naccept:"[i])
				break;
		}
		if (i == 8) {
			request.accept = (q + 8) - header;
		}
		q++;
	}

	char *name = NULL;
	int len = 0;

	// default values
	response.status = "200 OK";
	response.mime = "text/html";

	if (!IS_SPACE(*p)) {
		name = p;
		while (!IS_SPACE(*p) && p < end)
			p++;

		len = p - name;
		bool allow_html = true;

		if (request.accept > 0) {
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
						mime = lookup_ext(q+1).mime;

					//fs.refresh_file(file);
					FS_File *f = &fs->files[file];
					buffer = (char*)f->buffer;
					size = f->size;
				}
			}

			if (buffer && size) {
				if (!mime)
					mime = "text/plain";

				response.format = RESPONSE_FILE;
				response.html.resize(0);
				response.mime = mime;
				response.file_size = size;
				response.file_buffer = buffer;

				return;
			}
		}
	}

	response.format = RESPONSE_HTML;
	response.html.resize(0);
	serve_page(*fs, request, response, name, len);
}

static int delegate_socket(Thread *threads, struct pollfd *poll_list, int fd)
{
	int tidx = -1;
	for (int i = 0; i < MAX_THREADS; i++) {
		if ((threads[i].flags & THREAD_FLAG_BUSY) == 0) {
			tidx = i;
			break;
		}
	}
	if (tidx < 0) {
		log_error("Could not find a thread to read the request with");
		close(fd);
		return -1;
	}

	Thread *t = &threads[tidx];
	t->flags |= THREAD_FLAG_BUSY;

	int yes = 1;
	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));

	if ((t->flags & THREAD_FLAG_PIPES) == 0) {
		t->flags |= THREAD_FLAG_PIPES;
		pipe(t->from_main);
		pipe(t->to_main);

		poll_list[2 + tidx].fd = t->to_main[0];
		poll_list[2 + tidx].events = POLLIN;
	}

	if ((t->flags & THREAD_FLAG_CREATED) == 0) {
		t->flags |= THREAD_FLAG_CREATED;
		int res = pthread_create(&t->handle, NULL, handle_socket, t);
		if (res != 0) {
			if (t->flags & THREAD_FLAG_PIPES) {
				close_thread_pipes(t);
				poll_list[2 + tidx].fd = -1;
			}

			t->flags = 0;
			log_error("Failed to create thread (pthread_create() returned {d})", res);
			close(fd);
			return -2;
		}
	}

	write(t->from_main[1], &fd, 4);
	return tidx;
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
	int highest_thread_idx = -1;

	auto poll_list = new pollfd[MAX_THREADS + 2]();
	poll_list[0] = { .fd = cancel_fd, .events = POLLIN };
	poll_list[1] = { .fd = sock_fd, .events = POLLIN };

	while (true) {
		int n_read_fds = 2 + highest_thread_idx + 1;
		poll(poll_list, n_read_fds, -1);

		if (poll_list[0].revents & POLLIN) {
			log_info("Exiting");
			break;
		}

		if (poll_list[1].revents & POLLIN) {
			socklen_t addr_len = sizeof(addr);
			int fd = accept(sock_fd, addr_ptr, &addr_len);
			if (fd <= 0) {
				log_error("accept() failed, errno={d}\n", errno);
				break;
			}

			int tidx = delegate_socket(threads, poll_list, fd);
			if (tidx > highest_thread_idx)
				highest_thread_idx = tidx;
		}

		for (int i = 2; i < n_read_fds; i++) {
			if ((poll_list[i].revents & POLLIN) == 0)
				continue;

			int res = 0;
			read(poll_list[i].fd, &res, 4);

			Thread *t = &threads[i - 2];
			produce_response(t->request, t->response, global, fs);

			res = 0;
			write(t->from_main[1], &res, 4);
		}
	}

	close(sock_fd);

	for (int i = 0; i < MAX_THREADS; i++) {
		if (threads[i].flags & THREAD_FLAG_CREATED) {
			int fd = threads[i].from_main[1];
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
	// dangerous
	signal(SIGPIPE, SIG_IGN);

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

	Pool links;
	links.init(256);
	links.add("content/projects/website:client:");
	links.add("content/projects/website:server:");
	//links.add("content/projects/website:tests:");

	Filesystem *fs = new Filesystem();
	fs->init_at(".", allowed_dirs, links, list_dir_buffer);

	http_loop(global, fs);

	if (cancel_fd >= 0) close(cancel_fd);

	delete[] list_dir_buffer;
	delete fs;
	delete global;

	return 0;
}
