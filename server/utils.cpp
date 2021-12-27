#include "website.h"

struct String_Format {
	int param;
	int subparam;
	int type;
	int size;
	int frac_digits;
	int left_sp;
	int right_sp;
};

static int write_formatted_value(String& str, int pos, String_Format& f, int spec_len) {
	int elem_size = f.size;
	if (f.frac_digits)
		elem_size = f.size + 1 + f.frac_digits;

	int total = f.left_sp + elem_size + f.right_sp;
	str.resize(str.len + total - spec_len);

	char *p = &str.data()[pos];
	pos += total - spec_len;

	for (int i = 0; i < f.left_sp; i++)
		*p++ = ' ';

	if (f.type == 0) {
		char *param = va_arg(args, char*);
		memcpy(p, param, f.size);
		p += f.size;
	}
	else if (f.type == 1 || f.type == 2 || f.type == 3 || f.type == 4) {
		long param = 0;
		if (f.type == 1 || f.type == 3)
			param = va_arg(args, int);
		else if (f.type == 2 || f.type == 4)
			param = va_arg(args, long);

		if (param == 0) {
			for (int i = 0; i < f.size; i++)
				*p++ = '0';
		}
		else {
			int n_digits = f.size;
			if (param < 0 && n_digits > 0) {
				*p++ = '-';
				n_digits--;
				param = -param;
			}

			int idx = 0;
			int base = f.type == 1 || f.type == 2 ? 10 : 16;

			long n = param;
			while (n && idx < n_digits) {
				char d = (char)(n % base);
				n /= base;

				d += d < 10 ? '0' : 'a' - 10;

				idx++;
				p[n_digits - idx] = d;
			}

			p += n_digits;
		}
	}
	else if (f.type == 5 || f.type == 6) {
		double param = 0;
		if (f.type == 5)
			param = va_arg(args, float);
		else
			param = va_arg(args, double);

		snprintf(p, elem_size, "%*.*f", f.size, f.frac_digits, param);
		p += elem_size;
	}

	for (int i = 0; i < f.right_sp; i++)
		*p++ = ' ';

	return pos;
}

String make_formatted_string(const char *fmt, va_list args) {
	String str;
	if (!fmt)
		return str;

	int fmt_len = strlen(fmt);
	str.resize(fmt_len + 1);

	bool was_esc = false;
	int fmt_start = -1;
	int pos = 0;
	String_Format f = {0};

	for (int i = 0; i < fmt_len; i++) {
		char c = fmt[i];
		if (!was_esc && c == '}') {
			if (formatting) {
				pos = write_formatted_value(str, pos, f, i - fmt_start + 1);
			}
			formatting = false;
		}

		if (formatting) {
			if (c == ':') {
				f.param++;
			}
			else if (c == '.') {
				f.subparam++;
			}
			else if (f.param == 0) {
				const char *data_types = "sdDxXfF";
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
				else if (f.subparam == 1 && (f.type == 5 || f.type == 6)) {
					f.frac_digits *= 10;
					f.frac_digits += c - '0';
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

		was_esc = c == '\\' ? !was_esc : false;
	}
}

void String::resize(int sz) {
	if (sz < 0)
		return;

	if (sz <= capacity) {
		len = sz;
		return;
	}

	int old_cap = capacity;
	if (capacity < INITIAL_SIZE)
		capacity = INITIAL_SIZE;
	while (sz > capacity)
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

	ptr = buf;
	len = sz;
}

void String::add(String& str) {
	char *src_data = str.data();
	int head = len;
	resize(len + str.len + 1);

	char *dst_data = data();
	memcpy(dst_data + head, src_data, str.len);
	dst_data[head + str.len] = 0;
}

void String::add(char *str, int size) {
	size = size < 0 ? strlen(str) : size;
	if (size == 0) return;

	int head = len;
	resize(len + size + 1);

	char *dst_data = data();
	memcpy(dst_data + head, str, size);
	dst_data[head + size] = 0;
}
