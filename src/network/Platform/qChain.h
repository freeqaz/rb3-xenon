#pragma once
#include "Platform/RootObject.h"

namespace Quazal {

    template <class T>
    class DefaultChainPolicy {
    public:
    };

    template <class T, class X = DefaultChainPolicy<T> >
    class qChain : public RootObject {
    public:
        class iterator {
        public:
            iterator() : mLink(0) {}

            T mLink; // 0x0
        };

        qChain() : mNBLinks(0) {}
        ~qChain() { erase(mItFirst, mItEnd); }

        void clear();
        iterator erase(iterator);
        iterator erase(iterator first, iterator last) {
            T end_link = last.mLink;
            T cur = first.mLink;
            if (cur != end_link) {
                T zero = (T)0;
                while (cur != end_link) {
                    T prev = *(T*)((char*)cur + sizeof(T*));
                    T next = *(T*)cur;
                    if (prev)
                        *(T*)prev = next;
                    *(T*)((char*)cur + sizeof(T*)) = zero;
                    if (next)
                        *(T*)((char*)next + sizeof(T*)) = prev;
                    *(T*)cur = zero;
                    if (mItFirst.mLink == cur)
                        mItFirst.mLink = next;
                    if (mItLast.mLink == cur)
                        mItLast.mLink = prev;
                    mNBLinks--;
                    cur = next;
                }
            }
            first.mLink = cur;
            return first;
        }
        void push_back(const T &item) {
            if (mItFirst.mLink != mItEnd.mLink) {
                *(T*)mItLast.mLink = item;
                *(T*)((char*)item + sizeof(T*)) = mItLast.mLink;
                mItLast.mLink = item;
            } else {
                mItFirst.mLink = item;
                mItLast.mLink = item;
            }
            mNBLinks++;
        }
        void push_front(const T &);

        iterator mItFirst; // 0x0
        iterator mItLast; // 0x4
        iterator mItEnd; // 0x8
        unsigned long mNBLinks; // 0xc
    };
}