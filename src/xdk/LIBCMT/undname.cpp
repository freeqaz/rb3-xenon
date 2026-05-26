// MSVC C++ name undecorator implementation
// This is a stub implementation for the Dance Central 3 decomp

class DNameNode;
class _HeapManager;
class pairNode;

extern void* pairNode_vtable;

// DName class - represents a decorated name being processed
class DName {
public:
    DNameNode* mNode;
    char mStatus;

    void append(DNameNode* node);
};

// pairNode - linked list node for DName processing
class pairNode {
public:
    void** vtable;
    void* next;
    DNameNode* node;
    int flag;
};

// _HeapManager - memory allocation manager
class _HeapManager {
public:
    void* getMemory(int size, int align);
};

// External symbols referenced
extern _HeapManager gHeapManager;
extern void* pairNode_vtable;

// Appends a DNameNode to the DName's linked list
// Creates a pairNode wrapper and prepends it to the list
// Sets error status (3) if node is null or allocation fails
void DName::append(DNameNode* node) {
    if (node != 0) {
        pairNode* newNode = (pairNode*)gHeapManager.getMemory(0x10, 0);
        if (newNode != 0) {
            newNode->node = node;
            newNode->flag = -1;
            newNode->vtable = &pairNode_vtable;
            newNode->next = mNode;
        }
        mNode = (DNameNode*)newNode;
        if (newNode == 0) {
            goto error;
        }
    } else {
error:
        mStatus = 3;
    }
}
