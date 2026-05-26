#include "obj/Utl.h"
#include "obj/Data.h"
#include "obj/DataFile.h"
#include "obj/DataUtl.h"
#include "obj/Object.h"
#include "os/File.h"
#include "os/System.h"
#include "utl/MakeString.h"
#include "utl/Str.h"
#include <algorithm>
#include <cstdio>
#ifdef HX_NATIVE
#include <cstdlib>
#include <setjmp.h>
#include <signal.h>
#endif

std::list<String> sFilePaths;
std::list<Symbol> sFiles;
FileCallbackFunc *sCBack;

const char *PathName(const Hmx::Object *o) {
    return !o ? "NULL Object" : ((Hmx::Object *)o)->FindPathName();
}

MergeFilter::SubdirAction
MergeFilter::DefaultSubdirAction(ObjectDir *dir, Subdirs subdirs) {
    switch (subdirs) {
    case kNoSubdirs:
        return kMergeKeep;
    case kAllSubdirs:
        return kMergeMerge;
    case kMoveAllSubdirs:
        return kMergeReplace;
    case kInlineSubdirs:
        if (dir->InlineSubDirType() == kInlineNever
            || dir->InlineSubDirType() == kInlineCachedShared)
            return kMergeKeep;
    case kMergeInlinedMoveSharedSubdirs:
        if (dir->InlineSubDirType() == kInlineNever
            || dir->InlineSubDirType() == kInlineCachedShared)
            return kMergeReplace;
    default:
        break;
    }
    return kMergeMerge;
}

bool RecurseSuperClassesSearch(Symbol classSym, Symbol searchClass) {
    DataArray *classCfg = SystemConfig("objects")->FindArray(classSym, false);
    if (!classCfg)
        return false;
    DataArray *parents = classCfg->FindArray("superclasses", false);
    if (parents) {
        for (int i = 1; i < parents->Size(); i++) {
            Symbol curParent = parents->Sym(i);
            if (searchClass == curParent)
                return true;
            if (RecurseSuperClassesSearch(curParent, searchClass))
                return true;
        }
    }
    return false;
}

bool IsASubclass(Symbol child, Symbol parent) {
    if ((parent == "Object") || (child == parent))
        return true;
    else
        return RecurseSuperClassesSearch(child, parent);
}

const char *ClassExt(Symbol s) {
    static Symbol objects("objects");
    DataArray *cfg = SystemConfig(objects, s);
    DataArray *arr = cfg->FindArray("ext", false);
    return arr ? arr->Str(1) : nullptr;
}

void AddClassExt(char *file, Symbol s) {
    const char *ext = FileGetExt(file);
    if (*ext == '\0') {
        ext = ClassExt(s);
        if (ext) {
            strcat(file, ".");
            strcat(file, ext);
        }
    }
}

void MergeObject(
    Hmx::Object *o1, Hmx::Object *o2, ObjectDir *dir, MergeFilter::Action act
) {
    if (o1 == o2 || act == MergeFilter::kIgnore)
        return;
#ifdef HX_NATIVE
    static int sMergeDebug = -1;
    if (sMergeDebug < 0) {
        const char *env = getenv("MILO_DEBUG_MERGE");
        sMergeDebug = (env && atoi(env) != 0) ? 1 : 0;
    }
    if (sMergeDebug) {
        const char *actNames[] = {"kMerge", "kReplace", "kIgnore", "kKeep"};
        const char *actName = (act >= 0 && act <= 3) ? actNames[act] : "?";
        MILO_LOG("MERGE: %s [%s] -> %s action=%s dir=%s\n",
            PathName(o1), o1->ClassName(), o2 ? PathName(o2) : "(new)", actName,
            PathName(dir));
    }
#endif
    if (o2) {
        o1->ReplaceRefs(o2);
        if (act == MergeFilter::kMerge)
            o2->Copy(o1, Hmx::Object::kCopyFromMax);
        else if (act == MergeFilter::kReplace)
            o2->Copy(o1, Hmx::Object::kCopyDeep);
    } else if (act != MergeFilter::kKeep) {
        o1->SetName(o1->Name(), dir);
    }
}

int SubDirStringUsed(ObjectDir *dir) {
    if (!dir)
        return 0;
    else {
        int size = dir->StrTableUsedSize();
        const std::vector<ObjDirPtr<ObjectDir> > &subdirs = dir->SubDirs();
        for (std::vector<ObjDirPtr<ObjectDir> >::const_iterator it = subdirs.begin();
             it != subdirs.end();
             ++it) {
            size += SubDirStringUsed(*it);
        }
        return size;
    }
}

const char *NextName(const char *old_name, ObjectDir *dir) {
    if (!dir->FindObject(old_name, false, true))
        return old_name;
    const char *base = FileGetBase(old_name);
    const char *ext = FileGetExt(old_name);
    int len = (int)strlen(base);
    char *ptr;
    for (ptr = (char *)base + len; (ptr > base && ptr[-1] >= '0' && ptr[-1] <= '9'); ptr--)
        ;
    int numDigits = (int)(base + len - ptr);
    int atoied = 0;
    if (numDigits <= 1)
        numDigits = 1;
    if (*ptr != '\0')
        atoied = atoi(ptr);
    char buf[128];
    do {
        char fmt[] = "%02d";
        atoied++;
        fmt[2] = '0' + numDigits;
        sprintf(ptr, fmt, atoied);
        if (*ext != '\0') {
            sprintf(buf, "%s.%s", base, ext);
        } else {
            strcpy(buf, base);
        }
    } while (dir->FindObject(buf, false, true));

    return MakeString(buf);
}

bool PathCompare(DataArray *arr1, DataArray *arr2) {
    if (arr1 == arr2)
        return true;
    if (!arr1 || !arr2)
        return false;
    int arr1size = arr1->Size();
    if (arr1size != arr2->Size())
        return false;
    for (int i = 0; i < arr1size; i++) {
        DataType arr1type = CONST_ARRAY(arr1)->Node(i).Type();
        if (arr1type != CONST_ARRAY(arr2)->Node(i).Type())
            return false;
        else
            switch (arr1type) {
            case kDataSymbol:
            case kDataString:
                if (arr1->Str(i) != arr2->Str(i))
                    return false;
                break;
            case kDataInt:
                if (arr1->Int(i) != arr2->Int(i))
                    return false;
                break;
            default:
                break;
            }
    }
    return true;
}

const char *PrintPropertyPath(DataArray *arr) {
    StackString<256> str;
    str << arr;
    str[0U] = '[';
    str[str.length() - 1] = ']';
    return MakeString(str.c_str());
}

int SubDirHashUsed(ObjectDir *dir) {
    if (!dir)
        return 0;
    else {
        int size = dir->HashTableUsedSize();
        const std::vector<ObjDirPtr<ObjectDir> > &subdirs = dir->SubDirs();
        for (std::vector<ObjDirPtr<ObjectDir> >::const_iterator it = subdirs.begin();
             it != subdirs.end();
             ++it) {
            size += SubDirHashUsed(*it);
        }
        return size;
    }
}

int GetPropSize(Hmx::Object *o, DataArray *arr, int size) {
    DataArrayPtr ptr(new DataArray(size));
    for (int x = 0; x < size; x++) {
        ptr->Node(x) = arr->Node(x);
    }
    int ret = o->PropertySize(ptr);
    return ret;
}

bool IsPropPathValid(Hmx::Object *o, DataArray *prop) {
    for (int i = 0; i < prop->Size(); i++) {
        if (prop->Type(i) == kDataInt) {
            if (prop->Int(i) + 1 > GetPropSize(o, prop, i))
                return false;
        }
    }
    return true;
}

const DataNode *GetPropertyVal(Hmx::Object *o, DataArray *prop, bool fail) {
    return !IsPropPathValid(o, prop) ? nullptr : o->Property(prop, fail);
}

void ReserveToFit(ObjectDir *src, ObjectDir *dst, int extraObjects) {
    int stringSize = dst->StrTableUsedSize() + SubDirStringUsed(src) + extraObjects * 10;
    int hashSize = (dst->HashTableUsedSize() + SubDirHashUsed(src) + extraObjects) * 2;
    dst->Reserve(hashSize, stringSize);
}

void WalkProps(DataArray *ed, std::list<Symbol> &props, std::list<Symbol> *arrayProps) {
    for (int i = 1; i < ed->Size(); i++) {
        DataArray *arr = ed->Array(i);
        if ((arr->Type(1) == kDataSymbol) && arr->Sym(1) != "script")
            props.push_back(arr->Sym(0));
        else if (arr->Type(1) == kDataArray) {
            if (arr->Array(1)->Sym(0) == "indent") {
                WalkProps(arr->Array(1), props, arrayProps);
            } else if (arrayProps && arr->Array(1)->Sym(0) == "array") {
                arrayProps->push_back(arr->Sym(0));
            }
        }
    }
}

void ReloadObjectType(Hmx::Object *obj, DataArray *arr) {
    if (obj) {
        DataArray *def = obj->TypeDef();
        if (def) {
            std::list<DataArray *> arrs;
            if (!arr) {
                DataArray *file = DataReadFile(SystemConfig()->File(), true);
                arr = file->FindArray("objects");
            }
            arrs.push_back(arr);
            static Symbol types("types");
            DataArray *objArr = arr->FindArray(obj->ClassName(), types, obj->Type());
            DataUpdateArray(def, objArr);
            obj->SetTypeDef(def);
            if ((long)arr & 0xFF)
                arr->Release();
        }
    }
}

DataNode ObjectList(ObjectDir *dir, Symbol parentSym, bool b) {
    std::list<const char *> sList;
    if (dir) {
        for (ObjDirItr<Hmx::Object> it(dir, true); it != 0; ++it) {
            if (IsASubclass(it->ClassName(), parentSym)) {
                sList.push_back(it->Name());
            }
        }
    }
    DataArrayPtr ptr(new DataArray(b + sList.size()));
    int idx = 0;
    if (b)
        ptr->Node(idx++) = "";
    for (std::list<const char *>::iterator it = sList.begin(); it != sList.end(); ++it) {
        ptr->Node(idx++) = *it;
    }
    ptr->SortNodes(0);
    return ptr;
}

void RecurseSuperClasses(Symbol classSym, std::vector<Symbol> &classes) {
    DataArray *cfg = SystemConfig("objects", classSym);
    DataArray *found = cfg->FindArray("superclasses", false);
    if (found) {
        for (int i = 1; i < found->Size(); i++) {
            Symbol foundSym = found->Sym(i);
            if (classes.end() == std::find(classes.begin(), classes.end(), foundSym)) {
                classes.push_back(foundSym);
            }
            RecurseSuperClasses(foundSym, classes);
        }
    }
}

void ListSuperClasses(Symbol classSym, std::vector<Symbol> &classes) {
    RecurseSuperClasses(classSym, classes);
    classes.push_back("Object");
}

void ListProperties(
    std::list<Symbol> &props, Symbol classnm, Symbol type, std::list<Symbol> *arrayProps, bool walkSuper
) {
    static Symbol objects("objects");
    DataArray *cfg = SystemConfig(objects, classnm);
    if (type != gNullStr) {
        DataArray *typesArr = cfg->FindArray("types", false);
        if (typesArr) {
            typesArr = typesArr->FindArray(type, false);
        } else {
            typesArr = NULL;
        }
        DataArray *ed = typesArr->FindArray("editor", false);
        if (ed) {
            WalkProps(ed, props, arrayProps);
        }
    }
    DataArray *ed = cfg->FindArray("editor", false);
    if (ed) {
        WalkProps(ed, props, arrayProps);
    }
    if (walkSuper) {
        std::vector<Symbol> superclasses;
        ListSuperClasses(classnm, superclasses);
        for (std::vector<Symbol>::iterator it = superclasses.begin();
             it != superclasses.end();
             ++it) {
            DataArray *scfg = SystemConfig(objects, *it);
            DataArray *sced = scfg->FindArray("editor", false);
            if (sced) {
                WalkProps(sced, props, arrayProps);
            }
        }
    }
}

void MergeObjectsRecurse(ObjectDir *fromDir, ObjectDir *toDir, MergeFilter &filt, bool b) {
    if (!b) {
        switch (filt.FilterSubdir(fromDir, toDir)) {
        case MergeFilter::kMergeReplace:
            if (!toDir->HasSubDir(fromDir)) {
                ObjDirPtr<ObjectDir> dirPtr(fromDir);
                toDir->AppendSubDir(dirPtr);
            }
            return;
        case MergeFilter::kMergeKeep:
            return;
        default:
            break;
        }
        ObjRef tempRefs;
        tempRefs.Clear();
        for (ObjRef *it = fromDir->mRefs.next; it != &fromDir->mRefs;) {
            Hmx::Object *owner = it->RefOwner();
            if (owner && owner->Dir() == fromDir) {
                ObjRef *prevRef = it->prev;
                it->Release(nullptr);
                it->AddRef(&tempRefs);
                it = prevRef;
            }
            it = it->next;
        }
        tempRefs.ReplaceList(toDir);
    }

    for (ObjectDir::Entry *entry = fromDir->mHashTable.Begin(); entry != 0;
         entry = fromDir->mHashTable.Next(entry)) {
        Hmx::Object *curObj = entry->obj;
        if (curObj) {
            Hmx::Object *foundObj = toDir->FindObject(curObj->Name(), false, true);
            if (foundObj != curObj) {
                MergeObject(curObj, foundObj, toDir, filt.Filter(curObj, foundObj, toDir));
            }
        }
    }

    std::vector<ObjDirPtr<ObjectDir> > &subDirs = fromDir->mSubDirs;
    for (int i = 0; i < subDirs.size();) {
        ObjectDir *sd = subDirs[i];
        if (sd) {
            switch (filt.FilterSubdir(sd, toDir)) {
            case MergeFilter::kMergeKeep:
                break;
            case MergeFilter::kMergeReplace: {
                if (!toDir->HasSubDir(sd)) {
                    toDir->AppendSubDir(subDirs[i]);
                }
                fromDir->RemovingSubDir(subDirs[i]);
                subDirs.erase(subDirs.begin() + i);
                continue;
            }
            default:
                MergeObjectsRecurse(sd, toDir, filt, false);
                break;
            }
        }
        i++;
    }
}

void MergeDirs(ObjectDir *fromDir, ObjectDir *toDir, MergeFilter &filt) {
    MILO_ASSERT(fromDir && toDir, 0x193);
    Hmx::Object *toObj = toDir;
    Hmx::Object *fromObj = fromDir;
    if (toObj != fromObj) {
#ifdef HX_NATIVE
        // Suppress COPY_MEMBER(mSubDirs) in ObjectDir::Copy during merge.
        // MergeObjectsRecurse handles subdirs separately; copying them here
        // triggers cascading deletion that corrupts live ref rings.
        ObjectDir::SetInMergeDirs(true);
#endif
        MergeObject(fromObj, toObj, toDir, filt.Filter(fromObj, toObj, toDir));
#ifdef HX_NATIVE
        ObjectDir::SetInMergeDirs(false);
#endif
    }
    CopyTypeProperties(fromDir, toDir);
    MergeObjectsRecurse(fromDir, toDir, filt, true);
}

Hmx::Object *CopyObject(
    Hmx::Object *from, Hmx::Object *to, Hmx::Object::CopyType ty, bool setProxyFile
) {
    to->Copy(from, ty);
    if (setProxyFile) {
        ObjectDir *dir2 = dynamic_cast<ObjectDir *>(to);
        if (dir2 && dir2->InlineProxyType() != kInlineAlways) {
            ObjectDir *dir1 = dynamic_cast<ObjectDir *>(from);
            dir2->SetProxyFile(dir1->ProxyFile(), false);
        }
    } else
        CopyTypeProperties(from, to);
    return to;
}

Hmx::Object *CloneObject(Hmx::Object *from, bool instance) {
    MILO_ASSERT(from, 0x1EE);
    return CopyObject(
        from,
        Hmx::Object::NewObject(from->ClassName()),
        instance ? Hmx::Object::kCopyShallow : Hmx::Object::kCopyDeep,
        true
    );
}

void ReplaceObject(
    Hmx::Object *from, Hmx::Object *to, bool copyDeep, bool deleteFrom, bool setProxyFile
) {
    const char *name = from->Name();
    ObjectDir *dir = from->Dir();
    from->SetName(nullptr, nullptr);
    if (to) {
        to->SetName(name, dir);
        if (copyDeep)
            CopyObject(from, to, Hmx::Object::kCopyDeep, setProxyFile);
    }
    from->ReplaceRefs(to);
    if (deleteFrom)
        delete from;
}

void FileCallbackFullPath(const char *cc1, const char *cc2) {
    String str(MakeString("%s/%s/%s", FileRoot(), cc1, cc2));
    str.ReplaceAll('\\', '/');
    sFilePaths.push_back(str);
}

DataNode MakeFileListFullPath(const char *cc) {
    char buf[256];
    strcpy(buf, cc);
    sFilePaths.clear();
    FileRecursePattern(buf, &FileCallbackFullPath, true);
    sFilePaths.sort();
    sFilePaths.unique();
    DataArrayPtr ptr(new DataArray(sFilePaths.size()));
    int idx = 0;
    for (std::list<String>::iterator it = sFilePaths.begin(); it != sFilePaths.end();
         ++it) {
        ptr->Node(idx) = *it;
        idx++;
    }
    sFilePaths.clear();
    return ptr;
}

void FileCallback(const char *cc1, const char *cc2) {
    if (!sCBack) {
        sFiles.push_back(FileGetBase(cc2));
    } else {
        char buf[256];
        strcpy(buf, MakeString("%s/%s", cc1, cc2));
        if ((*sCBack)(buf)) {
            sFiles.push_back(FileGetBase(buf));
        }
    }
}

struct SymbolSort {
    bool operator()(Symbol s1, Symbol s2) { return strcmp(s1.Str(), s2.Str()) < 0; }
};

DataNode MakeFileList(const char *cc, bool b, FileCallbackFunc *callback) {
    char buf[256];
    strcpy(buf, cc);
    sCBack = callback;
    sFiles.clear();
    FileRecursePattern(buf, &FileCallback, true);
    sCBack = nullptr;
    if (b)
        sFiles.push_back(Symbol());
    sFiles.sort(SymbolSort());
    sFiles.unique();
    DataArrayPtr ptr(new DataArray(sFiles.size()));
    int idx = 0;
    for (std::list<Symbol>::iterator it = sFiles.begin(); it != sFiles.end(); ++it) {
        ptr->Node(idx) = *it;
        idx++;
    }
    sFiles.clear();
    return ptr;
}

void CopyTypeProperties(Hmx::Object *from, Hmx::Object *to) {
    if (from->ClassName() == to->ClassName())
        return;
    if (from->Type().Null())
        return;
    static Symbol objects("objects");
    static Symbol types("types");
    static Symbol editor("editor");
    std::list<Symbol> fromProps;
    std::list<Symbol> toProps;
    std::list<Symbol> fromArrayProps;
    std::list<Symbol> toArrayProps;
    DataArray *fromTypeArr =
        SystemConfig(objects, from->ClassName())->FindArray(types, from->Type());
    DataArray *fromTypeEditor = fromTypeArr->FindArray(editor, false);
    if (fromTypeEditor) {
        WalkProps(fromTypeEditor, fromProps, &fromArrayProps);
    }
    ListProperties(toProps, to->ClassName(), to->Type(), &toArrayProps, true);

    fromProps.sort();
    toProps.sort();
    fromArrayProps.sort();
    toArrayProps.sort();

    for (std::list<Symbol>::iterator fromIt = fromProps.begin(),
                                     toIt = toProps.begin();
         fromIt != fromProps.end() && toIt != toProps.end();
         ++fromIt) {
        Symbol prop = *fromIt;
        DataArray *fromValArr = fromTypeArr->FindArray(prop, false);
        if (fromValArr) {
            for (; toIt != toProps.end() && *toIt < prop; ++toIt)
                ;
            if (toIt != toProps.end()) {
                if (*toIt == prop) {
                    const DataNode *fromVal = from->Property(prop, true);
                    DataType fromType = fromVal->Type();
                    DataType toType = to->Property(prop, true)->Type();
                    if (fromType == toType) {
                        to->SetProperty(prop, *fromVal);
                    } else if (fromType == kDataSymbol && toType == kDataObject) {
                        if (!fromVal->Sym().Null()) {
                            Hmx::Object *objProp =
                                from->Dir()->FindObject(fromVal->Sym().Str(), false, true);
                            if (objProp) {
                                to->SetProperty(prop, objProp);
                            } else {
                                MILO_WARN(
                                    "Trying to convert Symbol prop to Object prop, but cannot find Object '%s'",
                                    fromVal->Sym().Str()
                                );
                            }
                        }
                    } else {
                        MILO_LOG(
                            "mismatched property %s, from: %d, to: %d\n",
                            prop.Str(),
                            fromType,
                            toType
                        );
                    }
                }
            }
        }
    }

    for (std::list<Symbol>::iterator fromIt = fromArrayProps.begin(),
                                     toIt = toArrayProps.begin();
         fromIt != fromArrayProps.end() && toIt != toArrayProps.end();
         ++fromIt) {
        Symbol prop = *fromIt;
        DataArray *fromValArr = fromTypeArr->FindArray(prop, false);
        if (fromValArr) {
            for (; toIt != toArrayProps.end() && *toIt < prop; ++toIt)
                ;
            if (toIt != toArrayProps.end()) {
                if (*toIt == prop) {
                    DataArray *fromPropArr = from->Property(prop, true)->Array();
                    DataArrayPtr propIdx(prop, 0);
                    DataArrayPtr propTag(prop);
                    while (to->PropertySize(propTag) != 0) {
                        to->RemoveProperty(propIdx);
                    }
                    for (int i = 0; i < fromPropArr->Size(); i++) {
                        propIdx->Node(1) = i;
                        to->InsertProperty(propIdx, *from->Property(propIdx, true));
                    }
                }
            }
        }
    }
}
