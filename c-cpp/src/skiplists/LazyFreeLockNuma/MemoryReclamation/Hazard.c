//Thomas Salemy

#ifndef HAZARD_C
#define HAZARD_C
#include "Hazard.h"

HazardNode_t* constructHazardNode() {
    HazardNode_t* node = (HazardNode_t*)malloc(sizeof(HazardNode_t));
    node -> hp0 = node -> hp1 = NULL;
    node -> next = NULL;
    return node;
}

HazardContainer_t* constructHazardContainer(HazardNode_t* head, int H) {
    HazardContainer_t* container = (HazardContainer_t*)malloc(sizeof(HazardContainer_t));
    container -> head = head;
    container -> H = H;
    return container;
}

void retireElement(LinkedList_t* retiredList, void* ptr) {
    push(retiredList, ptr);
    if (retiredList -> size >= MAX_DEPTH) {
        scan(retiredList);
    }
}

void scan(LinkedList_t* retiredList) {
    //Collect all valid hazard pointers across application threads
    LinkedList_t* ptrList = constructLinkedList();
    HazardNode_t* runner = memoryLedger -> head;
    while (runner != NULL) {
        if (runner -> hp0 != NULL) {
            push(ptrList, runner -> hp0);
        }
        if (runner -> hp1 != NULL) {
            push(ptrList, runner -> hp1);
        }
        runner = runner -> next;
    }
    
    //Compare retired candidates against active hazard nodes, reclaiming or procastinating
    int listSize = retiredList -> size;
    void** tmpList = (void**)malloc(listSize * sizeof(void*));
    popAll(retiredList, tmpList);
    for (int i = 0; i < listSize; i++) {
        if (findElement(ptrList, tmpList[i])) {
            push(retiredList, tmpList[i]);
        }
        else {
            reclaimMemory(tmpList[i]);
        }
    }
    free(tmpList);
}

void reclaimIndexNode(void* ptr) {
    inode_t* node = (inode_t*)ptr;
    FAD(&node -> dataLayer -> references);
    free(node);
}

void reclaimDataLayerNode(void* ptr) {
    free(ptr);
}

#endif
