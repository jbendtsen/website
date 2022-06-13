#include <ctime>
#include <unistd.h>
#include "website.h"

bool strings_match(const char *a, const char *b, int len)
{
	if (!a && !b)
		return true;
	if (!a || !b)
		return false;

	int b_len = strlen(b);
	if (b_len != len)
		return false;

	for (int i = 0; i < len; i++) {
		if (*a++ != *b++)
			return false;
	}

	return true;
}

int find_character(const char *str, char c, int len) {
	if (len < 0)
		len = strlen(str);

	int idx = -1;
	for (int i = 0; i < len && str[i]; i++) {
		if (str[i] == c) {
			idx = i;
			break;
		}
	}

	return idx;
}

HTML_Type lookup_ext(const char *ext)
{
	if (!ext)
		return { nullptr, nullptr };

	while (*ext == '.')
		ext++;

	int len = 0;
	for (len = 0; ext[len] && ext[len] != ' ' && ext[len] != '\t' && ext[len] != '\r' && ext[len] != '\n'; len++);

	if (strings_match(ext, "png", len) || strings_match(ext, "ico", len))
		return { "image/png", "img" };
	if (strings_match(ext, "jpg", len) || strings_match(ext, "jpeg", len))
		return { "image/jpeg", "img" };
	if (strings_match(ext, "css", len))
		return { "text/css", "style" };
	if (strings_match(ext, "js", len))
		return { "text/js", "script" };
	if (strings_match(ext, "html", len))
		return { "text/html", "iframe" };
	if (strings_match(ext, "pdf", len))
		return { "application/pdf", "iframe" };

	return { nullptr, nullptr };
}

void get_datetime(char *buf)
{
	time_t t = time(nullptr);
	struct tm *utc = gmtime(&t);
	strftime(buf, 96, "%a, %d %m %Y %H:%M:%S", utc);
}

void write_http_header(int request_fd, const char *status, const char *content_type, int size)
{
	char hdr[256];
	char datetime[96];
	get_datetime(datetime);

	String response(hdr, 256);
	response.reformat(
		"HTTP/1.1 {s}\r\n"
		"Date: {s} GMT\r\n"
		"Server: jackbendtsen.com.au\r\n"
		"Content-Type: {s}\r\n"
		"Content-Length: {d}\r\n\r\n",
		status, datetime, content_type, size
	);

	write(request_fd, response.data(), response.len);
}

void add_banner(Filesystem& fs, String *html, int hl_idx)
{
	html->add("<nav><div id=\"inner-nav\"><a class=\"nav-item");
	if (hl_idx == 0) html->add(" nav-item-cur");
	html->add("\" href=\"/\"><span>HOME");

	html->add("</span></a><a class=\"nav-item");
	if (hl_idx == 1) html->add(" nav-item-cur");
	html->add("\" href=\"/blog\"><span>BLOG");

	html->add("</span></a><a class=\"nav-item");
	if (hl_idx == 2) html->add(" nav-item-cur");
	html->add("\" href=\"/projects\"><span>PROJECTS");

	html->add("</span></a></div>");

	html->add("<div class=\"nav-item nav-dropdown\"><span>TOOLS</span><div>");
	html->add("<p><a href=\"/markdown\">Markdown Editor</a></p>");

	html->add("</div></div></nav>");
}

Pool::Pool() {
	buf = nullptr;
	cap = 0;
	head = 0;
}

Pool::~Pool() {
	if (buf) delete[] buf;
}

char *Pool::at(int offset) {
	return buf + offset;
}

void Pool::init(int capacity) {
	cap = capacity;
	buf = new char[cap]();
	head = 0;
}

int Pool::add(const char *str, int add_len) {
	if (str && add_len <= 0)
		add_len = strlen(str);

	int growth = add_len + 1;

	int c = cap;
	if (c <= 0) c = 64;

	while (head + growth > c)
		c *= 2;

	if (c > cap) {
		char *new_buf = new char[c];
		memcpy(new_buf, buf, cap);
		memset(&new_buf[cap], 0, c - cap);
		if (buf) delete[] buf;
		buf = new_buf;
		cap = c;
	}

	if (str)
		memcpy(&buf[head], str, add_len);

	head += growth;
	return add_len;
}

void Pool::add_and_escape(const char *str, int size) {
	if (size <= 0)
		size = strlen(str);

	int pos = head;
	add(nullptr, size * 6 + 1);

	for (int i = 0; i < size && str[i]; i++) {
		char c = str[i];
		if (NEEDS_ESCAPE(c)) {
			write_escaped_byte(c, &buf[pos]);
			pos += 6;
		}
		else {
			buf[pos++] = c;
		}
	}

	head = pos;
}

void Pool::reserve_extra(int bytes)
{
	if (bytes <= 0)
		return;

	int h = head;
	add(nullptr, bytes);
	head = h;
}
