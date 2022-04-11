#include <cstdio>
#include "../server/website.h"

void print_dir(Filesystem *fs, int didx, void *data) {
	char buf[1024];

	int name_idx = fs->dirs[didx].name_idx;
	char *name = name_idx >= 0 ? fs->name_pool.at(name_idx) : NULL;

	int len = fs->get_path(buf, 0, fs->dirs[didx].parent, name);
	buf[len++] = '\n';
	buf[len] = 0;
	((String*)data)->add(buf, len);
}

void print_file(Filesystem *fs, int fidx, void *data) {
	char buf[1024];
	char *name = fs->name_pool.at(fs->files[fidx].name_idx);

	int len = fs->get_path(buf, 0, fs->files[fidx].parent, name);
	buf[len++] = '\n';
	buf[len] = 0;
	((String*)data)->add(buf, len);
}

int main() {
	init_logger();

	Expander allowed_dirs;
	allowed_dirs.init(256, true, 0);
	allowed_dirs.add("content");
	allowed_dirs.add("client");

	Filesystem fs;
	char *business = new char[LIST_DIR_LEN];
	fs.init_at("../", allowed_dirs, business);
	delete[] business;

	String output;

	fs.walk(0, 0, print_dir, print_file, &output);

	fwrite(output.data(), 1, output.len, stdout);
	return 0;
}
