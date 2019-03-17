//Thomas Salemy

#ifndef HAZARD_H
#define HAZARD_H

#include "LinkedList.h"
#define MAX_DEPTH 5

typedef struct HazardNode {
    void* hp0;
    void* hp1;
    struct HazardNode* next;
} HazardNode_t;

HazardNode_t* constructHazardNode();

typedef struct HazardContainer {
    HazardNode_t* head;
    int H;
} HazardContainer_t;

HazardContainer_t* constructHazardContainer();

extern HazardContainer_t* memoryLedger;

void retireElement(HazardNode_t* hazardNode, void* ptr, void (*reclaimMemory)(void*));
void scan(HazardNode_t* hazardNode, void (*reclaimMemory)(void*));
void reclaimIndexNode(void* ptr);
void reclaimDataLayerNode(void* ptr);

#define RETIRE_INDEX_NODE(retiredList, ptr) retireElement((retiredList), (ptr), reclaimIndexNode)
#define RETIRE_NODE(retiredList, ptr) retireElement((retiredList), (ptr), reclaimDataLayerNode)

#endif
