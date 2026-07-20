#pragma once
// NOTE: was #include "meta_ham/HamMemcardAction.h" (DC3 stub) — that header
// defines stub Save/LoadMemcardAction bodies that collide with the real ones in
// band3/meta_band/SaveLoadManager.cpp. The include existed only for the friend
// decl below, which forward-declares the class itself. Replaced with a plain
// forward declaration so RB3's real subclasses are the sole definitions.
class LoadMemcardAction;
class SaveMemcardAction;
#include "utl/BinStream.h"
#include "utl/BufStream.h"
#include "meta/FixedSizeSaveableStream.h"
#include "os/Debug.h"
#include "utl/Std.h"
#include <set>
#include <hash_map>

enum {
    kSymbolSize = 0x32,
    kStringSize = 0x80
};

typedef int SaveSizeMethodFunc(int);

class FixedSizeSaveable {
    friend FixedSizeSaveableStream &
    operator<<(FixedSizeSaveableStream &fs, const FixedSizeSaveable &saveable);
    friend FixedSizeSaveableStream &
    operator>>(FixedSizeSaveableStream &fs, FixedSizeSaveable &saveable);
    friend class LoadMemcardAction; // hack

public:
    FixedSizeSaveable();
    virtual ~FixedSizeSaveable();
    virtual void SaveFixed(FixedSizeSaveableStream &) const = 0;
    virtual void LoadFixed(FixedSizeSaveableStream &, int) = 0;

    static void Init(int, int);
    static void SaveFixedSymbol(FixedSizeSaveableStream &, const Symbol &);
    static void LoadFixedSymbol(FixedSizeSaveableStream &, Symbol &);
    static void SaveFixedString(FixedSizeSaveableStream &, const class String &);
    static void LoadFixedString(FixedSizeSaveableStream &, class String &);
    static void SaveSymbolID(FixedSizeSaveableStream &, Symbol);
    static void LoadSymbolFromID(FixedSizeSaveableStream &, Symbol &);
    static void SaveSymbolTable(FixedSizeSaveableStream &, int, int);
    static void LoadSymbolTable(FixedSizeSaveableStream &, int, int);
    static void SaveStd(FixedSizeSaveableStream &, const std::vector<Symbol> &, int);
    static void LoadStd(FixedSizeSaveableStream &, std::vector<Symbol> &, int);
    static void SaveStd(FixedSizeSaveableStream &, const std::list<Symbol> &, int);
    static void LoadStd(FixedSizeSaveableStream &, std::list<Symbol> &, int);
    static void SaveStd(FixedSizeSaveableStream &, const std::set<Symbol> &, int);
    static void LoadStd(FixedSizeSaveableStream &, std::set<Symbol> &, int);
    static void EnablePrintouts(bool);

    // Note: `Allocator` here is actually the size/capacity type parameter on Wii.
    // The name is based on Xbox 360 symbols, which show the allocator type instead.
    template <class T, class Allocator>
    static void SaveStdFixed(
        FixedSizeSaveableStream &stream, const std::vector<T, Allocator> &vec, int maxsize
    ) {
        int savesize = T::SaveSize(FixedSizeSaveable::sSaveVersion);
        int vecsize = vec.size();
        if (vecsize > maxsize) {
            MILO_NOTIFY(
                "The vector size is greater than the maximum supplied! size=%i max=%i\n",
                vecsize,
                maxsize
            );
            vecsize = maxsize;
        }
        stream << vecsize;
        for (int i = 0; i < vecsize; i++) {
            vec[i].SaveFixed(stream);
        }
        if (maxsize > vecsize)
            PadStream(stream, savesize * (maxsize - vecsize));
    }

    template <class T, class Allocator>
    static void LoadStdFixed(
        FixedSizeSaveableStream &stream,
        std::vector<T, Allocator> &vec,
        int maxsize,
        int i2
    ) {
        int savesize = T::SaveSize(i2);
        if (vec.size() != 0) {
            MILO_NOTIFY("vector is not empty!");
            vec.clear();
        }
        int vecsize;
        stream >> vecsize;
        vec.resize(vecsize);
        for (int i = 0; i < vecsize; i++) {
            vec[i].LoadFixed(stream, i2);
        }
        if (maxsize > vecsize)
            DepadStream(stream, savesize * (maxsize - vecsize));
    }

    template <class T, class Allocator>
    static void SaveStd(
        FixedSizeSaveableStream &stream,
        const std::vector<T, Allocator> &vec,
        int maxsize,
        int savesize
    ) {
        int vecsize = vec.size();
        if (vecsize > maxsize) {
            MILO_NOTIFY(
                "The vector size is greater than the maximum supplied! size=%i max=%i\n",
                vecsize,
                maxsize
            );
            vecsize = maxsize;
        }
        stream << vecsize;
        for (int i = 0; i < vecsize; i++) {
            stream << vec[i];
        }
        if (maxsize > vecsize)
            PadStream(stream, (savesize * (maxsize - vecsize)));
    }

    template <class T, class Allocator>
    static void LoadStd(
        FixedSizeSaveableStream &stream,
        std::vector<T, Allocator> &vec,
        int maxsize,
        int savesize
    ) {
        if (vec.size() > 0) {
            MILO_NOTIFY("vector is not empty!");
            vec.clear();
        }
        int vecsize;
        stream >> vecsize;
        vec.resize(vecsize);
        for (int i = 0; i < vecsize; i++) {
            stream >> vec[i];
        }
        if (maxsize > vecsize)
            DepadStream(stream, savesize * (maxsize - vecsize));
    }

    // These progress maps are STLport hash_map (Harmonix's original type; the
    // "hash_map" notify strings above are the save-format proof). Only
    // AccomplishmentProgress::SaveFixed/LoadFixed call these 4-arg overloads.
    template <class T>
    static void SaveStd(
        FixedSizeSaveableStream &stream,
        const std::hash_map<Symbol, T> &map,
        int maxsize,
        int savesize
    ) {
        int mapsize = map.size();
        if (mapsize > maxsize) {
            MILO_NOTIFY(
                "The hash_map size is greater than the maximum supplied! size=%i max=%i\n",
                mapsize,
                maxsize
            );
            mapsize = maxsize;
        }
        stream << mapsize;
        for (std::hash_map<Symbol, T>::const_iterator it = map.begin(); it != map.end();
             ++it) {
            FixedSizeSaveable::SaveSymbolID(stream, it->first);
            stream << it->second;
        }
        if (maxsize > mapsize)
            PadStream(stream, savesize * (maxsize - mapsize));
    }

    // std::map<Symbol, T> 4-arg overload (BandProfile::mLessonCompletions is a
    // genuine rbtree std::map per the BandProfile.h offset annotations: 0x30
    // map -> 0x48 mScores = 0x18 = sizeof(rbtree), not 0x1c hashtable). Mirrors
    // the rb3-Wii oracle's std::map<Symbol,T> SaveStd.
    template <class T>
    static void SaveStd(
        FixedSizeSaveableStream &stream,
        const std::map<Symbol, T> &map,
        int maxsize,
        int savesize
    ) {
        int mapsize = map.size();
        if (mapsize > maxsize) {
            MILO_NOTIFY(
                "The hash_map size is greater than the maximum supplied! size=%i max=%i\n",
                mapsize,
                maxsize
            );
            mapsize = maxsize;
        }
        stream << mapsize;
        for (std::map<Symbol, T>::const_iterator it = map.begin(); it != map.end();
             ++it) {
            FixedSizeSaveable::SaveSymbolID(stream, it->first);
            stream << it->second;
        }
        if (maxsize > mapsize)
            PadStream(stream, savesize * (maxsize - mapsize));
    }

    template <class T1, class T2>
    static void SaveStd(
        FixedSizeSaveableStream &stream,
        const std::hash_map<T1, T2> &map,
        int maxsize,
        int savesize
    ) {
        int mapsize = map.size();
        if (mapsize > maxsize) {
            MILO_NOTIFY(
                "The hash_map size is greater than the maximum supplied! size=%i max=%i\n",
                mapsize,
                maxsize
            );
            mapsize = maxsize;
        }
        stream << mapsize;
        for (std::hash_map<T1, T2>::const_iterator it = map.begin(); it != map.end();
             ++it) {
            stream << it->first;
            stream << it->second;
        }
        if (maxsize > mapsize)
            PadStream(stream, savesize * (maxsize - mapsize));
    }

    template <class T, class Allocator>
    static void SaveStdPtr(
        FixedSizeSaveableStream &stream,
        const std::list<T *, Allocator> &list,
        int maxsize,
        int savesize
    ) {
        int lsize = list.size();
        if (lsize > maxsize) {
            MILO_NOTIFY(
                "The list size is greater than the maximum supplied! size=%i max=%i\n",
                lsize,
                maxsize
            );
            lsize = maxsize;
        }
        stream << lsize;
        for (std::list<T *, Allocator>::const_iterator it = list.begin();
             it != list.end();
             ++it) {
            stream << *(*it);
        }
        if (maxsize > lsize)
            PadStream(stream, (savesize * (maxsize - lsize)));
    }

    template <class T, class Allocator>
    static void SaveStdPtr(
        FixedSizeSaveableStream &stream,
        const std::vector<T *, Allocator> &vec,
        int maxsize,
        int savesize
    ) {
        int lsize = vec.size();
        if (lsize > maxsize) {
            MILO_NOTIFY(
                "The vector size is greater than the maximum supplied! size=%i max=%i\n",
                lsize,
                maxsize
            );
            lsize = maxsize;
        }
        stream << lsize;
        for (int i = 0; i < lsize; i++) {
            stream << *vec[i];
        }
        if (maxsize > lsize)
            PadStream(stream, (savesize * (maxsize - lsize)));
    }

    template <class T>
    static void SaveStdPtr(
        FixedSizeSaveableStream &stream,
        const std::map<Symbol, T *> &map,
        int maxsize,
        int savesize
    ) {
        int lsize = map.size();
        if (lsize > maxsize) {
            MILO_NOTIFY(
                "The map size is greater than the maximum supplied! size=%i max=%i\n",
                lsize,
                maxsize
            );
            lsize = maxsize;
        }
        stream << lsize;
        for (auto it = map.begin(); it != map.end(); it++) {
            FixedSizeSaveable::SaveSymbolID(stream, it->first);
            stream << *it->second;
        }
        if (maxsize > lsize)
            PadStream(stream, (savesize * (maxsize - lsize)));
    }

    // Int-keyed hash_map of owned pointers whose value type is itself
    // FixedSizeSaveable (e.g. SongStatusMgr::mSongStatusCache). Retail keeps
    // this instantiation out-of-line as a shared helper (verified: target
    // SongStatusMgr::SaveFixed calls a standalone fn_825D16B8(stream, &map,
    // maxsize, savesize) rather than inlining the loop) — write it the same
    // way here (call this method) rather than hand-inlining the loop in the
    // caller, so /Ob2 makes the same out-of-line decision.
    template <class T>
    static void SaveStdPtr(
        FixedSizeSaveableStream &stream,
        const std::hash_map<int, T *> &map,
        int maxsize,
        int savesize
    ) {
        int lsize = map.size();
        if (lsize > maxsize) {
            MILO_NOTIFY(
                "The hash_map size is greater than the maximum supplied! size=%i max=%i\n",
                lsize,
                maxsize
            );
            lsize = maxsize;
        }
        stream << lsize;
        for (std::hash_map<int, T *>::const_iterator it = map.begin(); it != map.end();
             ++it) {
            stream << it->first;
            it->second->SaveFixed(stream);
        }
        if (maxsize > lsize)
            PadStream(stream, (savesize * (maxsize - lsize)));
    }

    template <class T>
    static void LoadStd(
        FixedSizeSaveableStream &stream,
        std::hash_map<Symbol, T> &map,
        int maxsize,
        int savesize
    ) {
        if (map.size() > 0) {
            MILO_NOTIFY("hash_map is not empty!");
            map.clear();
        }
        int mapsize;
        stream >> mapsize;
        for (int i = 0; i < mapsize; i++) {
            Symbol key;
            FixedSizeSaveable::LoadSymbolFromID(stream, key);
            T value;
            stream >> value;
            map[key] = value;
        }
        if (maxsize > mapsize)
            DepadStream(stream, savesize * (maxsize - mapsize));
    }

    // std::map<Symbol, T> 4-arg overload (see SaveStd note above).
    template <class T>
    static void LoadStd(
        FixedSizeSaveableStream &stream,
        std::map<Symbol, T> &map,
        int maxsize,
        int savesize
    ) {
        if (map.size() != 0) {
            MILO_NOTIFY("hash_map is not empty!");
            map.clear();
        }
        int mapsize;
        stream >> mapsize;
        for (int i = 0; i < mapsize; i++) {
            Symbol key;
            FixedSizeSaveable::LoadSymbolFromID(stream, key);
            T value;
            stream >> value;
            map[key] = value;
        }
        if (maxsize > mapsize)
            DepadStream(stream, savesize * (maxsize - mapsize));
    }

    template <class T1, class T2>
    static void LoadStd(
        FixedSizeSaveableStream &stream,
        std::hash_map<T1, T2> &map,
        int maxsize,
        int savesize
    ) {
        if (map.size() > 0) {
            MILO_NOTIFY("hash_map is not empty!");
            map.clear();
        }
        int size;
        stream >> size;
        MILO_ASSERT(size >= 0, 0x99);
        for (int i = 0; i < size; i++) {
            T1 key;
            stream >> key;
            T2 value;
            stream >> value;
            map[key] = value;
        }
        if (maxsize > size)
            DepadStream(stream, savesize * (maxsize - size));
    }

    template <class T, class Allocator>
    static void LoadStdPtr(
        FixedSizeSaveableStream &stream,
        std::list<T *, Allocator> &list,
        int maxsize,
        int savesize
    ) {
        if (list.size() != 0) {
            MILO_NOTIFY("list is not empty!");
            DeleteAll(list);
        }
        int lsize;
        stream >> lsize;
        for (int i = 0; i < lsize; i++) {
            T *itemptr = new T();
            stream >> *itemptr;
            list.push_back(itemptr);
        }
        if (maxsize > lsize)
            DepadStream(stream, (savesize * (maxsize - lsize)));
    }

    template <class T, class Allocator>
    static void LoadStdPtr(
        FixedSizeSaveableStream &stream,
        std::vector<T *, Allocator> &vec,
        int maxsize,
        int savesize
    ) {
        if (vec.size() != 0) {
            MILO_NOTIFY("vector is not empty!");
            DeleteAll(vec);
        }
        int vecsize;
        stream >> vecsize;
        vec.resize(vecsize);
        for (int i = 0; i < vecsize; i++) {
            T *obj = new T();
            stream >> *obj;
            vec[i] = obj;
        }
        if (maxsize > vecsize)
            DepadStream(stream, savesize * (maxsize - vecsize));
    }

    template <class T>
    static void LoadStdPtr(
        FixedSizeSaveableStream &stream,
        std::map<Symbol, T *> &map,
        int maxsize,
        int savesize
    ) {
        if (map.size() != 0) {
            MILO_NOTIFY("hash_map is not empty!");
            FOREACH (it, map) {
                RELEASE(it->second);
            }
            map.clear();
        }
        int mapsize;
        stream >> mapsize;
        for (int i = 0; i < mapsize; i++) {
            Symbol key;
            FixedSizeSaveable::LoadSymbolFromID(stream, key);
            T *obj = new T();
            stream >> *obj;
            map[key] = obj;
        }
        if (maxsize > mapsize)
            DepadStream(stream, savesize * (maxsize - mapsize));
    }

    template <class T, class Allocator>
    static void LoadStdPtrReplace(
        FixedSizeSaveableStream &stream,
        std::vector<T *, Allocator> &v,
        int max,
        int savesize
    ) {
        int size;
        stream >> size;
        MILO_ASSERT(v.size() == size, 0x148);
        MILO_ASSERT(size == max, 0x149);
        for (int i = 0; i < size; i++) {
            stream >> *v[i];
        }
    }

    static int GetMaxSymbols() {
        MILO_ASSERT(sMaxSymbols >= 0, 0x1F5);
        return sMaxSymbols;
    }

    static unsigned char sPadder;
    static bool sPrintoutsEnabled;
    static int GetSaveVersion() { return sSaveVersion; }

protected:
    static void PadStream(FixedSizeSaveableStream &, int);
    static void DepadStream(FixedSizeSaveableStream &, int);

    SaveSizeMethodFunc *mSaveSizeMethod; // 0x4

private:
    static int sCurrentMemcardLoadVer;
    static int sSaveVersion;
    static int sMaxSymbols;
};

FixedSizeSaveableStream &operator<<(FixedSizeSaveableStream &, const FixedSizeSaveable &);
FixedSizeSaveableStream &operator>>(FixedSizeSaveableStream &, FixedSizeSaveable &);

#define REPORT_SIZE(name, size)                                                          \
    int ret = size;                                                                      \
    if (FixedSizeSaveable::sPrintoutsEnabled) {                                          \
        MILO_LOG("* %s = %i\n", name, ret);                                              \
    }                                                                                    \
    return ret;
