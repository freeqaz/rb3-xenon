#pragma once
#ifndef HX_NATIVE

#include "../stlport/stl/_vector.h"

namespace DSP {
namespace Synapse {
namespace GranularSynth {

struct Voice;
struct Granule;

} // namespace GranularSynth
} // namespace Synapse
} // namespace DSP

namespace stlpmtx_std {

// Forward declaration of _Vector_base constructor
template <>
_Vector_base<DSP::Synapse::GranularSynth::Voice, StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>>::_Vector_base(
    size_t __n,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Voice>& __a
);

// Explicit specialization of vector constructor for Granule
template <>
vector<DSP::Synapse::GranularSynth::Granule, StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>>::vector(
    unsigned int __n,
    const DSP::Synapse::GranularSynth::Granule& __val,
    const StlNodeAlloc<DSP::Synapse::GranularSynth::Granule>& __a
);

} // namespace stlpmtx_std

#endif // HX_NATIVE
