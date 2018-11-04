#include "intset.h"
#include "stdio.h"
#include "stdlib.h"

int main(int argc, char** argv) {
  struct skipList_t* sl = constructSkipList(6);
  for (int i = 0; i < 40; i++) {
    sl_insert(sl, rand() % 400);
  }
  fprintf(stdout, "Size: %d\n", getSize(sl));
  return 0;
}
