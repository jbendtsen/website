#include "website.h"

Expander::Expander() {
	buf = nullptr;
	cap = 0;
	head = 0;
	start = 0;
	extra = 0;
}

Expander::~Expander() {
	if (buf) delete[] buf;
}

char *Expander::get(int *size) {
	if (size) *size = head - start;
	return buf + start;
}

char *Expander::at(int offset) {
	return buf + offset;
}

void Expander::init(int capacity, bool use_terminators, int prepend_space) {
	cap = capacity;
	if (prepend_space > 0 && cap <= prepend_space)
		cap += prepend_space;

	buf = new char[cap]();
	head = prepend_space;
	start = prepend_space;
	extra = (int)use_terminators;
}

int Expander::add(const char *str, int add_len) {
	if (str && add_len <= 0)
		add_len = strlen(str);

	int growth = add_len + extra;

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

void Expander::add_and_escape(const char *str, int size) {
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

int Expander::prepend_string_trunc(const char *add, int add_len) {
	if (start <= 0)
		return 0;
	if (add && add_len <= 0)
		add_len = strlen(add);
	if (add_len > start - extra)
		add_len = start - extra;

	start -= add_len + extra;
	memcpy(buf + start, add, add_len);

	for (int i = 0; i < extra; i++)
		buf[start + add_len + i] = 0;

	return add_len;
}

int Expander::prepend_string_overlap(const char *add, int add_len) {
	if (add && add_len <= 0)
		add_len = strlen(add);

	int pos = start - add_len - extra;
	pos = pos > 0 ? pos : 0;

	if (add_len > cap - pos)
		add_len = cap - pos;

	start = pos;
	memcpy(buf + start, add, add_len);

	for (int i = 0; i < extra; i++)
		buf[start + add_len + i] = 0;

	return add_len;
}
