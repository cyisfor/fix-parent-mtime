#define _DEFAULT_SOURCE // DT_*

#include "myassert.h"

#include <stdio.h>

#include <sys/stat.h>
#include <dirent.h> // fdopendir
#include <stdbool.h>
#include <stdlib.h> // NULL
#include <string.h> // memcpy
#include <fcntl.h> // openat
#include <unistd.h> // chdir

struct level {
	DIR* d;
	const char* name;
	struct timespec latest[2];
};

bool before(struct timespec a, const struct timespec b) {
	if(a.tv_sec > b.tv_sec) return false;
	if(a.tv_sec == b.tv_sec &&
		 a.tv_nsec >= b.tv_nsec)
		return false;
	return true;
}

void print_timespec(struct timespec spec) {
	printf("%d:%d",spec.tv_sec,spec.tv_nsec);
}

DIR* opendirat(int parent, const char* name) {
	int dir = openat(parent, name, O_DIRECTORY);
	assert(dir >= 0);
	return fdopendir(dir);
}


void one_level(struct level* top) {
	struct dirent* ent;
	bool dirty = false;
	// ignore the directory's mtime, because the contents could make
	// it older, or newer
	while((ent = readdir(top->d))) {
		// filesystem should support this... use ext4
		assert(ent->d_type != DT_UNKNOWN);
		struct level sub = {};

		if(ent->d_type == DT_DIR) {
			if(ent->d_name[0] == '.') {
				if(ent->d_name[1] == '.') {
					if(ent->d_name[2] == '\0')
						continue;
				} else if(ent->d_name[1] == '\0')
					continue;
			}
			printf("descending into %s\n",ent->d_name);
			sub.d = opendirat(dirfd(top->d), ent->d_name);
			sub.name = ent->d_name;
			assert(sub.d != NULL);
			one_level(&sub);
			if(before(top->latest[1], sub.latest[1])) {
				top->latest[0] = sub.latest[0];
				top->latest[1] = sub.latest[1];
				dirty = true;
			}
			assert(0==closedir(sub.d));
		} else {
			struct stat info;
			if(0==fstatat(dirfd(top->d), ent->d_name, &info,
										// let's not go into /proc/self/mounts thanks
										AT_SYMLINK_NOFOLLOW)) {
				if(before(top->latest[1],info.st_mtim)) {
					top->latest[0] = info.st_atim;
					top->latest[1] = info.st_mtim;
					dirty = true;
				}
			} else {
				error(0,errno,"%s no mtime?",ent->d_name);
				continue;
			}
		}
	}
	if(dirty) {
		struct stat info;
		// only adjust utime if it changed

		assert(0==fstat(dirfd(top->d), &info));
		if(info.st_mtim.tv_sec == top->latest[1].tv_sec &&
			 info.st_mtim.tv_nsec == top->latest[1].tv_nsec)
			return;

		printf("adjusting %s ",top->name);
		print_timespec(top->latest[1]);
		putchar('\n');
		assert(0==futimens(dirfd(top->d),top->latest));
	}
}

int main(int argc, char *argv[])
{
	struct level top;
	if(argc == 2) {
		top.name = argv[1];
	} else {
		top.name = ".";
	}
	top.d = opendir(top.name);
	assert(NULL != top.d);
	one_level(&top);
	return 0;
}
