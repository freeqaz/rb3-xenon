/*
 * Specialized implementations for DC3-specific scenarios
 */

#ifndef _STLP_ALGO_SPECIAL_C
#define _STLP_ALGO_SPECIAL_C

#if !defined (_STLP_INTERNAL_ALGO_H)
#  include <stl/_algo.h>
#endif

_STLP_BEGIN_NAMESPACE

template <>
inline StoreOffer ** __unguarded_partition<StoreOffer **, StoreOffer *, SortCmp>(StoreOffer ** __first,
                                                                                StoreOffer ** __last,
                                                                                StoreOffer *,
                                                                                SortCmp) {
  StoreOffer ***first = __first;
  StoreOffer ***last = __last;
  StoreOffer **temp;
  StoreOffer **swap_temp;

loop:
  temp = *first;
  if (first < last) {
    swap_temp = *first;
    *first = *last;
    first++;
    *last = swap_temp;
    goto loop;
  }
  return (StoreOffer **)first;
}

_STLP_END_NAMESPACE

#endif /* _STLP_ALGO_SPECIAL_C */