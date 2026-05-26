#include "trie.h"

// Node access helpers - uses pointer arithmetic due to unusual 17-byte node size
#define NODE_SIZE 0x11
#define MAX_NODES 0x20000

// Get pointer to node at given index
static inline char *NodePtr(Trie *trie, unsigned int idx) {
    return (char *)trie + idx * NODE_SIZE;
}

// Node field accessors
static inline unsigned int &FirstChild(char *node) {
    return *(unsigned int *)(node + 0x00);
}

static inline unsigned int &NextSibling(char *node) {
    return *(unsigned int *)(node + 0x04);
}

static inline unsigned int &Parent(char *node) {
    return *(unsigned int *)(node + 0x08);
}

static inline unsigned int &CountField(char *node) {
    return *(unsigned int *)(node + 0x0C);
}

static inline unsigned char &SiblingCount(char *node) {
    return *(unsigned char *)(node + 0x0F);
}

static inline unsigned char &Character(char *node) {
    return *(unsigned char *)(node + 0x10);
}

// Get dup count from count field (upper 24 bits)
static inline unsigned int GetDupCount(unsigned int countField) {
    return countField >> 8;
}

// Header is at offset 0x220000 from trie base
// Header layout:
//   Offset 0: _nodeCount (4 bytes)
//   Offset 4: free list head (4 bytes)
#define HEADER_OFFSET 0x220000

static inline unsigned int &NodeCount(Trie *trie) {
    return *(unsigned int *)((char *)trie + HEADER_OFFSET);
}

static inline unsigned int &FreeListHead(Trie *trie) {
    return *(unsigned int *)((char *)trie + HEADER_OFFSET + 4);
}

void Trie::inc_count(unsigned int index) {
    check_index(index);
    char *node = NodePtr(this, index);
    unsigned int *cf = &CountField(node);
    unsigned int count = SiblingCount(node);
    check_index(index);
    *cf = (*cf & 0xFFFFFF00) | (count + 1);
}

void Trie::dec_count(unsigned int index) {
    check_index(index);
    char *node = NodePtr(this, index);
    unsigned int *cf = &CountField(node);
    unsigned int count = SiblingCount(node);
    check_index(index);
    *cf = (*cf & 0xFFFFFF00) | (count - 1);
}

void Trie::inc_dup_count(unsigned int index) {
    check_index(index);
    char *node = NodePtr(this, index);
    unsigned int dupCount = GetDupCount(CountField(node));
    check_index(index);
    CountField(node) = ((dupCount + 1) << 8) | SiblingCount(node);
}

void Trie::dec_dup_count(unsigned int index) {
    check_index(index);
    char *node = NodePtr(this, index);
    unsigned int *cf = &CountField(node);
    unsigned int dupCount = GetDupCount(CountField(node));
    check_index(index);
    *cf = ((dupCount - 1) << 8) | SiblingCount(node);
}

unsigned int Trie::get_free_node() {
    unsigned int freeHead = FreeListHead(this);

    if (freeHead != 0) {
        // Pop from free list - next free is stored in NextSibling slot
        check_index(freeHead);
        char *freeNode = NodePtr(this, freeHead);
        FreeListHead(this) = NextSibling(freeNode);
        return freeHead;
    }

    // Allocate new node - increment node count
    int &_nodeCount = *(int *)((char *)this + HEADER_OFFSET);
    MILO_ASSERT(_nodeCount < MAX_NODES, 0x82);
    unsigned int newIdx = _nodeCount + 1;
    _nodeCount = newIdx;
    return newIdx;
}

void Trie::delete_node(unsigned int index) {
    check_index(index);
    // Clear FirstChild before recomputing node ptr
    FirstChild(NodePtr(this, index)) = 0;
    check_index(index);
    char *node = NodePtr(this, index);

    NextSibling(node) = 0;
    check_index(index);

    Parent(node) = 0;
    CountField(node) = 0;
    check_index(index);

    *(char *)(node + 0x10) = -1;

    // Get old free list head and link
    unsigned int oldHead = FreeListHead(this);
    if (oldHead != 0) {
        check_index(index);
        NextSibling(node) = FreeListHead(this);
    }

    FreeListHead(this) = index;
}

int Trie::store(const char *str) {
    if (str == 0 || *str == 0)
        goto return_zero;

    unsigned int curIdx = 1;
    unsigned int parentIdx = 0;

    // Walk string to compute length
    const char *p = str;
    while (true) {
        unsigned char c = *p;
        p++;
        if (c == 0) break;
    }
    int strLen = (int)(p - str) - 1;
    int i = 0;

    do {
        unsigned int nodeIdx = curIdx;
        char ch = str[i];
        check_index(nodeIdx);
        int j = 0;
        int sibCount = (signed char)(int)(*(unsigned int *)((char *)this + nodeIdx * NODE_SIZE + 0x0C));
        curIdx = nodeIdx;
        if ('\0' < sibCount) {
            do {
                check_index(nodeIdx);
                char *node = NodePtr(this, nodeIdx);
                if (node[0x10] == ch) {
                    parentIdx = curIdx;
                    check_index(nodeIdx);
                    curIdx = FirstChild(node);
                    goto found;
                }
                if (j != sibCount - 1) {
                    check_index(nodeIdx);
                    nodeIdx = NextSibling(node);
                }
                j++;
                curIdx = nodeIdx;
            } while (j < sibCount);
        }

        // Not found - insert new node
        {
            nodeIdx = get_free_node();
            if (sibCount == 0) {
                if ((int)parentIdx > 0) {
                    check_index(parentIdx);
                    FirstChild(NodePtr(this, parentIdx)) = nodeIdx;
                }
            } else {
                check_index(curIdx);
                NextSibling(NodePtr(this, curIdx)) = nodeIdx;
            }
            check_index(nodeIdx);
            char *newNode = NodePtr(this, nodeIdx);
            newNode[0x10] = ch;
            check_index(nodeIdx);
            *(unsigned int *)(newNode + 0x08) = parentIdx;
            unsigned int firstChildIdx;
            if ((int)parentIdx > 0) {
                check_index(parentIdx);
                firstChildIdx = FirstChild(NodePtr(this, parentIdx));
            } else {
                firstChildIdx = 1;
            }
            inc_count(firstChildIdx);
            parentIdx = nodeIdx;
            curIdx = nodeIdx;
            if (str[i] != '\0') goto fast_path;
        }

    found:
        i++;
    } while (i <= strLen);

    goto done;

fast_path:
    do {
        i++;
        unsigned int newIdx = get_free_node();
        check_index(curIdx);
        FirstChild(NodePtr(this, curIdx)) = newIdx;
        check_index(newIdx);
        char *newNode = NodePtr(this, newIdx);
        *(unsigned int *)(newNode + 0x08) = curIdx;
        char c2 = str[i];
        check_index(newIdx);
        newNode[0x10] = c2;
        inc_count(newIdx);
        curIdx = newIdx;
    } while (i < strLen);

done:
    int result = curIdx;
    if (result == 0)
        result = parentIdx;
    inc_dup_count(result);
    return result;
return_zero:
    return 0;
}

void Trie::remove(unsigned int index) {
    unsigned int curIdx = index;
    check_index(curIdx);
    char *curNode = NodePtr(this, curIdx);

    // Only process if char is 0 (end of string marker) and has dup count
    if (Character(curNode) != 0) {
        return;
    }

    check_index(curIdx);
    unsigned int counts = CountField(curNode);
    if ((counts && 0xFFFFFF00) == 0) {
        return;
    }

    check_index(curIdx);
    bool hasRefs = (counts & 0xFFFFFF00) != 0;
    if ((hasRefs) != 0x100) {
        // More than one reference, just decrement
        dec_dup_count(curIdx);
        return;
    }

    // Single reference - remove the node chain
loop_start:
    if (curIdx != 0) {
        check_index(curIdx);
        unsigned int parentIdx = Parent(curNode);
        if (parentIdx != 0) {
            check_index(parentIdx);
            unsigned int parentFirstChild = FirstChild(NodePtr(this, parentIdx));
            check_index(parentFirstChild);
            char *firstChildNode = NodePtr(this, parentFirstChild);
            if (SiblingCount(firstChildNode) == 1) {
                // Single child - merge upward
                check_index(curIdx);
                unsigned int toDelete = curIdx;
                curIdx = Parent(curNode);
                delete_node(toDelete);
                goto update_node;
            }
        }
    }

    // Find the first child in the sibling chain
    check_index(curIdx);
    unsigned int firstChildIdx;
    if (Parent(curNode) == 0) {
        firstChildIdx = 1;
    } else {
        check_index(curIdx);
        unsigned int parentIdx = Parent(curNode);
        check_index(parentIdx);
        firstChildIdx = FirstChild(NodePtr(this, parentIdx));
    }

    unsigned int sibIdx = firstChildIdx;
    check_index(firstChildIdx);
    unsigned char sibCount = SiblingCount(NodePtr(this, firstChildIdx));
    unsigned int prevSib = 0;
    unsigned int traverseCount = 0;

    if (sibCount == 0) {
        goto update_node;
    }

    // Find this node in sibling chain
    while (sibIdx != curIdx) {
        prevSib = sibIdx;
        check_index(sibIdx);
        traverseCount++;
        sibIdx = NextSibling(NodePtr(this, sibIdx));
        if (traverseCount >= sibCount) {
            goto update_node;
        }
    }

    // Found the node - unlink from chain
    if (prevSib != 0) {
        check_index(curIdx);
        check_index(prevSib);
        NextSibling(NodePtr(this, prevSib)) = NextSibling(curNode);
        delete_node(curIdx);
        dec_count(firstChildIdx);
        return;
    }

    // Node is first in chain
    if (curIdx == 1) {
        // Root level special handling
        unsigned int scanCount = 0;
#ifdef HX_NATIVE
        unsigned char rootCount = SiblingCount(NodePtr(this, 1));
#else
        unsigned char rootCount = *(unsigned char *)((char *)this + 0x20);
#endif

        while (scanCount < rootCount - 1) {
            check_index(curIdx);
            scanCount++;
            curIdx = NextSibling(curNode);
            curNode = NodePtr(this, curIdx);
        }

        if (curIdx == 1) {
            delete_node(1);
            return;
        }

        // Move last sibling to position 1
        check_index(curIdx);
        FirstChild(NodePtr(this, 1)) = FirstChild(curNode);
        check_index(curIdx);
#ifdef HX_NATIVE
        Character(NodePtr(this, 1)) = Character(curNode);
#else
        *(unsigned char *)((char *)this + 0x21) = Character(curNode);
#endif
        delete_node(curIdx);
        dec_count(firstChildIdx);

        // Update parent pointers of children
        unsigned int updateCount = 0;
        auto _tmp2 = NodePtr(this, 1);
        unsigned int updateIdx = FirstChild(_tmp2);

        while (true) {
            unsigned int childIdx = FirstChild(NodePtr(this, 1));
            check_index(childIdx);
            if (updateCount >= SiblingCount(NodePtr(this, childIdx))) {
                break;
            }
            check_index(updateIdx);
            char *updateNode = NodePtr(this, updateIdx);
            Parent(updateNode) = 1;
            check_index(updateIdx);
            updateCount++;
            updateIdx = NextSibling(updateNode);
        }
        return;
    }

    // Not root - update parent's first child
    check_index(curIdx);
    check_index(curIdx);
    unsigned int parentIdx2 = Parent(curNode);
    check_index(parentIdx2);
    FirstChild(NodePtr(this, parentIdx2)) = NextSibling(curNode);

    // Update sibling count
    check_index(curIdx);
    unsigned char newSibCount = SiblingCount(curNode) - 1;
    check_index(curIdx);
    unsigned int parentIdx3 = Parent(curNode);
    check_index(parentIdx3);
    unsigned int newFirstChild = FirstChild(NodePtr(this, parentIdx3));
    check_index(newFirstChild);
    char *newFirstChildNode = NodePtr(this, newFirstChild);
    CountField(newFirstChildNode) = (CountField(newFirstChildNode) & 0xFFFFFF00) | newSibCount;

    delete_node(curIdx);
    return;

update_node:
    check_index(curIdx);
    curNode = NodePtr(this, curIdx);
    if (Character(curNode) == 0) {
        return;
    }
    goto loop_start;
}
