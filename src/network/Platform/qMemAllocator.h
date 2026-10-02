#pragma once
#include "Platform/MemoryManager.h"
#include <vector>

namespace Quazal {

    // the allocators are apparently Quazal::MemAllocator
    // pretty much copy pasted the StlNodeAlloc implementation nate did...
    // thank you nate lol
    template <class T>
    class MemAllocator {
    public:
        typedef std::size_t size_type;
        typedef std::ptrdiff_t difference_type;

        typedef T value_type;
        typedef T *pointer;
        typedef T &reference;
        typedef const T *const_pointer;
        typedef const T &const_reference;

        template <class T2>
        struct rebind {
            typedef MemAllocator<T2> other;
        };

#if defined(VERSION_SZBE69_B8) || defined(RB3_QUAZAL_MEMALLOCATOR_CTORS)
        // Retail doesn't have constructor calls (Wii B8). X360 TUs built with
        // RB3_QUAZAL_MEMALLOCATOR_CTORS (PRUDPStream) convert through these
        // constructors: the retail /Od _Rb_tree constructor at 0x82AFF400 is a
        // leaf with no EH frame, so the converted temporaries need no destruction.
        MemAllocator() {}
        MemAllocator(MemAllocator<T> const &) {}
        template <class T2>
        MemAllocator(const MemAllocator<T2> &) {}
#endif

        // ...but still has the destructor. Two X360 TUs show none in retail:
        // StationURL (no unwind action after clear() in its ~qMap) and
        // PRUDPStream (the leaf _Rb_tree constructor above). Elsewhere it is
        // load-bearing: without it JobBackEndServicesLogin's ConnectStream
        // becomes inlinable and is no longer emitted out of line.
#if !defined(RB3_QUAZAL_RETAIL_MEMALLOCATOR) && !defined(RB3_QUAZAL_MEMALLOCATOR_CTORS)
        ~MemAllocator() {}
#endif

#if defined(VERSION_SZBE69) || (!defined(VERSION_SZBE69_B8) && !defined(RB3_QUAZAL_MEMALLOCATOR_CTORS))
        // This is the only way to make allocator conversions
        // work in retail without using constructors.
        // rb3-xenon (X360 retail) defines neither VERSION_SZBE69 nor
        // VERSION_SZBE69_B8, which previously left MemAllocator with NO rebind
        // path -> STLport _List_base(const MemAllocator&) failed to convert
        // MemAllocator<T> to MemAllocator<_List_node<T>>. The retail SKU used
        // this conversion-operator path (the comment above), so enable it when
        // the constructor path (VERSION_SZBE69_B8) is absent.
        template <class T2>
        operator MemAllocator<T2>() const {
            return MemAllocator<T2>();
        }
#endif

        template <class T2>
        MemAllocator<T> &operator=(const MemAllocator<T2> &right) {}

        template <class T2>
        bool operator==(const MemAllocator<T2> &) const {
            return true;
        }
        template <class T2>
        bool operator!=(const MemAllocator<T2> &) const {
            return false;
        }

        pointer address(reference value) const { return &value; }
        const_pointer address(const_reference value) const { return &value; }
        size_type max_size() const { return size_type(-1) / sizeof(T); }

        pointer allocate(const size_type count, const void *hint = nullptr) const {
#ifdef STL_NODE_ALLOC_DEBUG
            // A leftover from the earlier prototype versions of RB3;
            // bank 5/6 use type info for allocation tracing purposes
            typeid(pointer);
#endif
#ifdef RB3_QUAZAL_RETAIL_MEMALLOCATOR
            return reinterpret_cast<pointer>(MemoryManager::Allocate(
                MemoryManager::GetDefaultMemoryManager(),
                count * sizeof(T),
                "Unknown",
                0,
                MemoryManager::_InstType7
            ));
#else
            return reinterpret_cast<pointer>(
                MemoryManager::Allocate(count * sizeof(T), MemoryManager::_InstType7)
            );
#endif
        }

        void deallocate(pointer ptr, size_type count) const {
            MemoryManager::Free(
                MemoryManager::GetDefaultMemoryManager(), ptr, MemoryManager::_InstType7
            );
        }

        void construct(pointer ptr, const_reference value) const { new (ptr) T(value); }
        void destroy(pointer ptr) const { ptr->~T(); }
    };

}