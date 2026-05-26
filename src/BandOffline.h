#pragma once
#include "utl/Symbol.h"
#include "utl/Str.h"
#include "utl/StlAlloc.h"
#include "bandobj/BandCharacter.h"
#include <map>

// Forward declarations
class DataArray;
class DataNode;
class BandCharacter;

// CharStatKeeper: per-character animation stat accumulator used by MakeCharClipBudget.
// Layout (from ctor asm):
//   0x00..0x17 : map<int,float,less<int>,StlNodeAlloc>   (mByIntensity)
//   0x18..0x2f : map<String,float,less<String>,StlNodeAlloc> (mByGroup)

class CharStatKeeper {
public:
    CharStatKeeper();
    ~CharStatKeeper();

    // Accumulate dt seconds for the given intensity level and group name.
    void OnPoll(int intensity, String groupName, float dt);

    // In-place compound assignment operators.
    void AddEq(const CharStatKeeper &c);
    void MaxEq(const CharStatKeeper &c);
    void ScaleEq(float scale);

    std::map<int, float, std::less<int>,
             STLPORT::StlNodeAlloc<std::pair<const int, float> > > mByIntensity; // 0x00
    std::map<String, float, std::less<String>,
             STLPORT::StlNodeAlloc<std::pair<const String, float> > > mByGroup;  // 0x18
};

// BandOffline: static helper that exposes MakeCharClipBudget to the data script layer.
// No instance state; all methods are effectively static or free functions.
class BandOffline {
public:
    static void Init();
    static void Poll();
    static DataNode MakeCharClipBudget(DataArray *da);
};

// Free helper: index of prefix character in "gjpr" (0-based), used to bucket by section.
int GetStatKeeperIndex(const char *section);


// Comparator for sorting vector<pair<String,float>> in descending poll-time order.
struct SortGroupPolls {
    inline bool operator()(
        const std::pair<String, float> &a, const std::pair<String, float> &b
    ) const {
        return a.second > b.second;
    }
};
