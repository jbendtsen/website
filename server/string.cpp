#include "website.h"

String::String() {
	ptr = nullptr;
	capacity = INITIAL_SIZE;
	len = 0;
	initial_buf[0] = 0;
}
String::String(char *buf, int size) {
	ptr = (char*)((u64)buf | 1ULL);
	capacity = size;
	len = 0;
}
String::String(String& other) {
	capacity = other.capacity;
	len = other.len;
	ptr = other.ptr ? new char[other.capacity] : nullptr;
	memcpy(data(), other.data(), len + 1);
}
String::~String() {
	if (ptr && ((u64)ptr & 1ULL) == 0)
		delete[] ptr;
}

char *String::data() {
	return ptr ? (char*)((u64)ptr & ~1ULL) : &initial_buf[0];
}

char String::last() {
	return data()[len-1];
}

void String::scrub(int len_from_end) {
	if (len_from_end <= 0)
		return;

	int new_head = len - len_from_end;
	if (new_head < 0) new_head = 0;

	memset(data() + new_head, 0, len - new_head);
	len = new_head;
}

int String::resize(int sz) {
	if (sz < 0)
		return len;

	int actual_sz = sz + 1;

	if (actual_sz <= capacity) {
		len = sz;
		return len;
	}

	if ((u64)ptr & 1ULL) {
		if (actual_sz > capacity)
			sz = capacity - 1;

		len = sz >= 0 ? sz : 0;
		return len;
	}

	int old_cap = capacity;
	if (capacity < INITIAL_SIZE)
		capacity = INITIAL_SIZE;
	while (actual_sz > capacity)
		capacity *= 2;

	char *buf = new char[capacity];
	if (old_cap > 0) {
		if (ptr) {
			memcpy(buf, ptr, old_cap);
			delete[] ptr;
		}
		else if (old_cap <= INITIAL_SIZE) {
			memcpy(buf, initial_buf, old_cap);
		}
	}

	buf[sz] = 0;
	ptr = buf;
	len = sz;
	return len;
}

void String::add(String& str) {
	char *src_data = str.data();
	int head = len;
	int new_size = resize(len + str.len);
	int to_add = new_size - head;

	if (to_add > 0) {
		char *dst_data = data();
		memcpy(dst_data + head, src_data, to_add);
		dst_data[head + to_add] = 0;
	}
}

void String::add(const char *str, int size) {
	size = size < 0 ? strlen(str) : size;
	if (size == 0) return;

	int head = len;
	int new_size = resize(len + size);
	int to_add = new_size - head;

	if (to_add > 0) {
		char *dst_data = data();
		memcpy(dst_data + head, str, to_add);
		dst_data[head + to_add] = 0;
	}
}

void String::add(char c) {
	add(&c, 1);
}
void String::add(const char *str) {
	add(str, -1);
}

void String::assign(const char *str, int size) {
	resize(0);
	add(str, size);
}

void String::add_and_escape(const char *str, int size) {
	int head = len;
	resize(len + size);

	for (int i = 0; str[i] && i < size; i++) {
		char c = str[i];
		if (NEEDS_ESCAPE(c)) {
			int h = head + 6;
			if (h+1 >= capacity)
				resize(h+1);

			write_escaped_byte(c, data() + head);
			head = h;
		}
		else {
			int h = head + 1;
			if (h+1 >= capacity)
				resize(h+1);

			data()[head] = c;
			head = h;
		}
	}

	len = head;
}

struct String_Format {
	int param;
	int subparam;
	int type;
	int size;
	int left_sp;
	int right_sp;
};

static int fmt_write_string(char *param, String& str, int pos, String_Format& f, int spec_len) {
	int elem_size = f.size;
	int len = strlen(param);
	if (!elem_size || elem_size > len)
		elem_size = len;

	str.resize(str.len + f.left_sp + elem_size + f.right_sp - spec_len);
	char *p = &str.data()[pos];

	for (int i = 0; i < f.left_sp; i++)
		*p++ = ' ';

	memcpy(p, param, elem_size);
	p += elem_size;

	for (int i = 0; i < f.right_sp; i++)
		*p++ = ' ';

	return f.left_sp + elem_size + f.right_sp;
}

static int fmt_write_number(long param, String& str, int pos, String_Format& f, int spec_len) {
	int elem_size = f.size;
	bool neg = param < 0;
	if (neg)
		param = -param;

	char digits[24];
	int n_digits = 0;

	if (param == 0) {
		digits[0] = '0';
		n_digits = 1;
	}
	else {
		int base = f.type == 1 || f.type == 2 ? 10 : 16;

		long n = param;
		while (n) {
			char d = (char)(n % base);
			n /= base;

			d += d < 10 ? '0' : 'a' - 10;
			digits[n_digits++] = d;
		}
	}

	if (!elem_size)
		elem_size = n_digits + (neg ? 1 : 0);

	str.resize(str.len + f.left_sp + elem_size + f.right_sp - spec_len);
	char *p = &str.data()[pos];

	for (int i = 0; i < f.left_sp; i++)
		*p++ = ' ';

	int to_write = elem_size;
	if (neg) {
		*p++ = '-';
		to_write--;
	}

	for (int i = n_digits; i < to_write; i++)
		*p++ = '0';

	int end = to_write < n_digits ? to_write : n_digits;
	for (int i = end-1; i >= 0; i--)
		*p++ = digits[i];

	for (int i = 0; i < f.right_sp; i++)
		*p++ = ' ';

	return f.left_sp + elem_size + f.right_sp;
}

void write_formatted_string(String& str, const char *fmt, va_list args) {
	str.resize(0);
	if (!fmt) {
		str.data()[0] = 0;
		return;
	}

	int fmt_len = strlen(fmt);
	str.resize(fmt_len + 1);

	bool was_esc = false;
	bool formatting = false;
	int fmt_start = -1;
	int pos = 0;
	String_Format f = {0};

	for (int i = 0; i < fmt_len; i++) {
		char c = fmt[i];
		if (c == '\\') {
			if (was_esc)
				str.data()[pos++] = '\\';

			was_esc = !was_esc;
			continue;
		}

		if (!was_esc && c == '}') {
			if (formatting) {
				int written = 0;
				int spec_len = i - fmt_start + 1;

				if (f.type == 0) {
					char *param = va_arg(args, char*);
					written = fmt_write_string(param, str, pos, f, spec_len);
				}
				else if (f.type >= 1 && f.type <= 4) {
					long param = 0;
					if (f.type == 1 || f.type == 3)
						param = va_arg(args, int);
					else if (f.type == 2 || f.type == 4)
						param = va_arg(args, long);

					written = fmt_write_number(param, str, pos, f, spec_len);
				}

				pos += written;
			}
			formatting = false;
			continue;
		}

		if (formatting) {
			if (c == ':') {
				f.param++;
			}
			else if (c == '.') {
				f.subparam++;
			}
			else if (f.param == 0) {
				const char *data_types = "sdDxX";
				int s = strlen(data_types);
				for (int i = 0; i < s; i++) {
					if (c == data_types[i]) {
						f.type = i;
						break;
					}
				}
			}
			else if (f.param == 1 && c >= '0' && c <= '9') {
				if (f.subparam == 0) {
					f.size *= 10;
					f.size += c - '0';
				}
			}
			else if (f.param == 2 && c >= '0' && c <= '9') {
				if (f.subparam == 1) {
					f.left_sp *= 10;
					f.left_sp += c - '0';
				}
				else if (f.subparam == 1) {
					f.left_sp *= 10;
					f.right_sp += c - '0';
				}
			}
		}
		else {
			formatting = !was_esc && c == '{';
			if (formatting) {
				memset(&f, 0, sizeof(String_Format));
				fmt_start = i;
			}
			else if (!(c == '\\' && !was_esc)) {
				str.data()[pos++] = c;
			}
		}

		was_esc = false;
	}

	str.data()[pos] = 0;
	str.resize(str.len - 1);
}

char *append_string(char *dst, char *src, int size) {
	if (size <= 0) {
		while (*src)
			*dst++ = *src++;
	}
	else {
		for (int i = 0; i < size; i++)
			*dst++ = *src++;
	}

	*dst = 0;
	return dst;
}

void write_escaped_byte(int ch, char *buf) {
	u64 map[2];
	map[0] = 0x3736353433323130;
	map[1] = 0x6665646362613938;

	char *p = buf;
	*p++ = '&';
	*p++ = '#';
	*p++ = 'x';
	*p++ = ((char*)map)[(ch >> 4) & 0xf];
	*p++ = ((char*)map)[ch & 0xf];
	*p++ = ';';
	*p = 0;
}

bool caseless_match(const char *a, const char *b) {
	while (*a && *b) {
		if (*a++ != *b++)
			return false;
	}
	return *a == 0 && *b == 0;
}
