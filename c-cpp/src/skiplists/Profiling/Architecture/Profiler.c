#include "Profiler.h"
int check_addr(int supposed_node, void* addr) {
	if(!addr) return -1;
	int actual_node = -1;
	if(-1 == get_mempolicy(&actual_node, NULL, 0, (void*)addr, MPOL_F_NODE | MPOL_F_ADDR)) {
		exit(-9);
	}
	if(actual_node == supposed_node)	return 0;
	else 								return 1;
}

/**
 * zone_access_check() - checks address and updates local or foreign accesses accordingly
 */
void zone_access_check(int node, void* addr, volatile long* local, volatile long* foreign, int dont_count) {
	int result;
	if(1 == (result = check_addr(node, addr))) {
		(*foreign)++;
		if(dont_count){ (*foreign)--; }
	} else if(result == 0) {
		(*local)++;
	}
}