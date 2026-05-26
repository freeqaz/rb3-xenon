#pragma once

#include "os/Debug.h"

// Trie data structure for string storage
//
// Memory layout - each node is 17 bytes (0x11):
//   Offset 0x00 (4 bytes): First child index
//   Offset 0x04 (4 bytes): Next sibling index
//   Offset 0x08 (4 bytes): Parent index
//   Offset 0x0C (4 bytes): Count field (big-endian)
//       - Upper 24 bits (>> 8): duplicate/reference count
//       - Lower 8 bits (byte at 0x0F): sibling count
//   Offset 0x10 (1 byte): Character
//
// Header area at offset 0x220000 from base:
//   Offset 0x00 (4 bytes): _nodeCount - total allocated nodes
//   Offset 0x04 (4 bytes): free list head index
//
// Legacy header at offset 0x20 (used by remove):
//   Offset 0x20 (1 byte): root sibling count
//   Offset 0x21 (1 byte): root character

class Trie {
public:
    int store(const char *str);
    void remove(unsigned int index);
    unsigned int get_free_node();
    void delete_node(unsigned int index);
    void inc_count(unsigned int index);
    void dec_count(unsigned int index);
    void inc_dup_count(unsigned int index);
    void dec_dup_count(unsigned int index);

    void check_index(unsigned int index) {
        MILO_ASSERT(index < 0x20000, 0x36);
    }

    char *get(int index, char *buffer, int bufSize) {
        if (index <= 0 || index >= 0x20000) {
            *buffer = 0;
            return buffer;
        }

        check_index(index);
        char *node = (char *)this + index * 0x11;
        if (*(unsigned char *)(node + 0x10) != 0) {
            *buffer = 0;
            return buffer;
        }

        char *end = buffer + bufSize;
        int count = 0;
        char *ptr = end - 1;

        do {
            if (count >= bufSize) break;
            check_index(index);
            char *node = (char *)this + index * 0x11;
            *ptr = *(unsigned char *)(node + 0x10);
            check_index(index);
            count++;
            ptr--;
            index = *(unsigned int *)(node + 0x08);
        } while (index != 0);

        *(end - 1) = 0;
        return (ptr == end - 1) ? ptr : ptr + 1;
    }
};
