#define ALGONAME "Fomitchev & Ruppert linked list with wait-free contains"

struct node {
  val_t val;
  _Atomic(struct node *) next;
  struct node * backlink;
};

struct intset {
  node_t *head;
};
