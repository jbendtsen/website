#include <cstdio>
#include "website.h"

int main() {
	init_logger();

	Filesystem fs;
	char *business = new char[LIST_DIR_LEN];
	fs.init_at("../", business);
	delete[] business;

	int next_levels[100];
	char space_buf[204];

	space_buf[0] = ' ';
	space_buf[1] = ' ';
	space_buf[2] = 0;

	int dir_idx = fs.dirs[0].first_dir;

	next_levels[0] = -1;
	int lvl_cur = 1;

	printf("- /\n");

	while (lvl_cur > 0 && dir_idx >= 0) {
		FS_Directory& dir = fs.dirs[dir_idx];
		printf("%s- %s/\n", space_buf, &fs.name_pool.buf[dir.name_idx]);
		next_levels[lvl_cur] = dir_idx;

		if (dir.first_dir >= 0) {
			space_buf[lvl_cur*2]     = ' ';
			space_buf[lvl_cur*2 + 1] = ' ';
			space_buf[lvl_cur*2 + 2] = 0;

			dir_idx = dir.first_dir;
			lvl_cur++;
			continue;
		}

		int f = dir.first_file;
		while (f >= 0) {
			printf("%s    %s\n", space_buf, &fs.name_pool.buf[fs.files[f].name_idx]);
			f = fs.files[f].next;
		}

		if (dir.next >= 0) {
			dir_idx = dir.next;
			continue;
		}

		while (true) {
			lvl_cur--;
			if (lvl_cur <= 0)
				break;

			int next = fs.dirs[next_levels[lvl_cur]].next;
			if (next > 0) {
				dir_idx = next;
				break;
			}
		}

		space_buf[lvl_cur*2] = 0;
	}
}
