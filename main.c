#define _DEFAULT_SOURCE // DT_*
#include <sys/stat.h>
#include <dirent.h> // fdopendir
#include <stdbool.h>
#include <stdlib.h> // NULL
#include <string.h> // memcpy
#include <fcntl.h> // openat
#include <unistd.h> // chdir

#include "myassert.h"


struct level {
	DIR* d;
	struct stat info;
};

bool maybe_adjust_time(struct timespec* a, const struct timespec* b) {
	if(a->tv_sec < b->tv_sec) return false;
	if(a->tv_sec == b->tv_sec &&
		 a->tv_nsec < b->tv_nsec)
		return false;
	memcpy(a,b,sizeof(*a));
	return true;
}

void maybe_adjust(struct level* a, const struct level* b) {
	if(maybe_adjust_time(&a->info.st_mtim, &b->info.st_mtim)) {
		assert(a->d != NULL);
		struct timespec times[2] = { a->info.st_atim, a->info.st_mtim };
		assert(0==futimens(dirfd(a->d),times));
	}
}

DIR* opendirat(int parent, const char* name) {
	int dir = openat(parent, name, O_DIRECTORY);
	assert(dir >= 0);
	return fdopendir(dir);
}

void one_level(struct level* top) {
	struct dirent* ent;
	struct timespec latest[2];
	while((ent = readdir(top->d))) {
		assert(ent->d_type != DT_UNKNOWN); // filesystem should support this... use ext4
		struct level sub;
		if(ent->d_type == DT_DIR) {
			sub.d = opendirat(dirfd(top->d), ent->d_name);
			assert(sub.d != NULL);
			assert(0==fstat(dirfd(sub.d), &sub.info));
			maybe_adjust_time(&sub.info.st_mtim, &top->info.st_mtim);
			one_level(&sub);
			assert(0==closedir(sub.d));
		} else {
			assert(0==fstatat(dirfd(top->d), ent->d_name, &sub.info, 0));
		}
		maybe_adjust(top, &sub);
	}
}

int main(int argc, char *argv[])
{
	if(argc == 2)
		assert(0==chdir(argv[1]));
	struct level top;
	top.d = opendir(".");
	assert(NULL != top.d);
	assert(0==fstat(dirfd(top.d),&top.info));
	one_level(&top);
	return 0;
}
