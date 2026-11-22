// Native only (lane A5, 2026-11-22): force-included (native/CMakeLists.txt) into
// TUs whose first header to spell `hash<Symbol>` has only the STLport form.
//
// Eleven src/ headers guard the same specialization with RB3_HASH_SYMBOL_DEFINED;
// only src/system/meta/FixedSizeSaveableStream.h carries an HX_NATIVE arm
// (native hash_map aliases std::unordered_map, which takes std::hash<K>). A TU
// that reaches FixedSizeSaveableStream.h first compiles natively; one that
// reaches e.g. meta_band/NameGenerator.h first sees `_STLP_TEMPLATE_NULL` and
// fails. This header is that HX_NATIVE arm, verbatim, defined first. The hash is
// the interned char* word identity in both builds, as retail does.
#pragma once
#include "utl/Symbol.h"
#include <cstddef>
#include <functional>

#ifdef HX_NATIVE
#ifndef RB3_HASH_SYMBOL_DEFINED
#define RB3_HASH_SYMBOL_DEFINED
namespace std {
template <> struct hash<Symbol> {
    size_t operator()(const Symbol &s) const { return (size_t)s.Str(); }
};
}
#endif
#endif
