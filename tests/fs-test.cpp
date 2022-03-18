#include <cstdio>
#include "../server/website.h"

void print_fs_tree(Filesystem& fs, int order) {
	int next_levels[100];
	char space_buf[204];

	space_buf[0] = ' ';
	space_buf[1] = ' ';
	space_buf[2] = 0;

	int dir_idx = fs.dirs[0].first_dir.array[order];

	next_levels[0] = -1;
	int lvl_cur = 1;

	printf("- /\n");

	while (lvl_cur > 0 && dir_idx >= 0) {
		FS_Directory& dir = fs.dirs[dir_idx];
		printf("%s- %s/\n", space_buf, &fs.name_pool.buf[dir.name_idx]);
		next_levels[lvl_cur] = dir_idx;

		if (dir.first_dir.array[order] >= 0) {
			space_buf[lvl_cur*2]     = ' ';
			space_buf[lvl_cur*2 + 1] = ' ';
			space_buf[lvl_cur*2 + 2] = 0;

			dir_idx = dir.first_dir.array[order];
			lvl_cur++;
			continue;
		}

		int f = dir.first_file.array[order];
		while (f >= 0) {
			printf("%s    %s\n", space_buf, &fs.name_pool.buf[fs.files[f].name_idx]);
			f = fs.files[f].next.array[order];
		}

		if (dir.next.array[order] >= 0) {
			dir_idx = dir.next.array[order];
			continue;
		}

		while (true) {
			lvl_cur--;
			if (lvl_cur <= 0)
				break;

			int next = fs.dirs[next_levels[lvl_cur]].next.array[order];
			if (next > 0) {
				dir_idx = next;
				break;
			}
		}

		space_buf[lvl_cur*2] = 0;
	}
}

int main() {
	init_logger();

	Filesystem fs;
	char *business = new char[LIST_DIR_LEN];
	fs.init_at("../", business);
	delete[] business;

	for (int i = 0; i < 3; i++)
		print_fs_tree(fs, i);
}
