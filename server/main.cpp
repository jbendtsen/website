#include <stdio.h>
#include "website.h"

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

File_Pool make_file_pool() {
	File_Pool fp = {0};
	FILE *f = fopen("file-map.txt", "r");
	if (!f) {
		log_error("bruh");
	}
}

/*
int main() {
	Bucket_Array<1024, 64> response;
	response.append("Hello!");
}
*/

int main() {
	init_logger();
}
