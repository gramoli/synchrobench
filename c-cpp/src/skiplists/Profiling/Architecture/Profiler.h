#ifndef PROFILER_H
#define PROFILER_H

#include <numa.h>
#include <numaif.h>

int check_addr(int supposed_node, void* addr);
void zone_access_check(int supposed_node, void* addr, volatile long* local, volatile long* foreign, int dont_count);

#endif