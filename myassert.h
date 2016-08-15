#include <error.h>
#include <errno.h>

#define assert(test) if(!(test)) { error(23,errno,"Assertion failure: " #test " " __FILE__ ":%d", __LINE__); }
