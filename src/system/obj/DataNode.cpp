#include "Data.h"
#include "Dir.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "utl/BinStream.h"
#include "utl/Str.h"
#include "utl/TextStream.h"
#include "obj/DataFunc.h"
#include "obj/Object.h"
#include "obj/DataUtl.h"
#include <cstring>
#include <map>

int gEvalIndex;
std::map<Symbol, DataNode> gDataVars;
DataNode gEvalNode[8];

// sw3 scatter: RB3 retail linker placed DataFunc.cpp's gDataThisPtr dynamic
// initializer + scalar-deleting-destructor glue inside this TU's .text span
// (same phenomenon as the DataArray.cpp/BandSongMetadata.cpp scatter-includes
// below -- static-initializer COMDATs land by link order, not by TU).
#if !HX_NATIVE  // native: gDataThisPtr is really defined in DataFunc.cpp
DataThisPtr gDataThisPtr;
#endif

bool DataNode::CompatibleType(DataType ty) const {
    if (mType == ty)
        return true;
    switch (mType) {
    case kDataInt:
        return ty == kDataFloat;
    case kDataSymbol:
        return ty == kDataString || ty == kDataObject;
    case kDataString:
        return ty == kDataObject;
    default:
        return false;
    }
    return true;
}

const char *DataNode::DataTypeString(DataType ty) {
    switch (ty) {
    case kDataInt:
        return "kDataInt";
    case kDataFloat:
        return "kDataFloat";
    case kDataVar:
        return "kDataVar";
    case kDataFunc:
        return "kDataFunc";
    case kDataObject:
        return "kDataObject";
    case kDataSymbol:
        return "kDataSymbol";
    case kDataUnhandled:
        return "kDataUnhandled";
    case kDataArray:
        return "kDataArray";
    case kDataCommand:
        return "kDataCommand";
    case kDataString:
        return "kDataString";
    case kDataProperty:
        return "kDataProperty";
    case kDataGlob:
        return "kDataGlob";
    case kDataIfdef:
        return "kDataIfdef";
    case kDataElse:
        return "kDataElse";
    case kDataEndif:
        return "kDataEndif";
    case kDataDefine:
        return "kDataDefine";
    case kDataInclude:
        return "kDataInclude";
    case kDataMerge:
        return "kDataMerge";
    case kDataIfndef:
        return "kDataIfndef";
    case kDataAutorun:
        return "kDataAutorun";
    case kDataUndef:
        return "kDataUndef";
    default:
        return "Unknown data type";
    }
}

DataNode::DataNode(const char *c) {
    mValue.array = new DataArray(c, strlen(c) + 1);
    mType = kDataString;
}

DataNode::DataNode(const String &str) {
    mValue.array = new DataArray(str.c_str(), str.length() + 1);
    mType = kDataString;
}

DataNode::DataNode(const DataArrayPtr &ptr) {
    mValue.array = ptr;
    ptr->AddRef();
    mType = kDataArray;
}

DataNode::DataNode(DataArray *array, DataType type) {
    MILO_ASSERT(array, 0x158);
    mValue.array = array;
    mValue.array->AddRef();
    MILO_ASSERT(type & kDataArray, 0x15B);
    mType = type;
}

DataNode &DataNode::operator=(const DataNode &node) {
    if (&node != this) {
        if (mType & kDataArray)
            mValue.array->Release();
        mValue = node.mValue;
        mType = node.mType;
        if (mType & kDataArray)
            mValue.array->AddRef();
    }
    return *this;
}

const DataNode &UseQueue(const DataNode &node) {
    int i;
    gEvalNode[gEvalIndex] = node;
    i = gEvalIndex;
    gEvalIndex = gEvalIndex + 1 & 7;
    return gEvalNode[i];
}

const DataNode &DataNode::Evaluate() const {
    if (mType == kDataCommand) {
        return UseQueue(mValue.array->Execute());
    } else if (mType == kDataVar) {
        return *mValue.var;
    } else if (mType == kDataProperty) {
        MILO_ASSERT(gDataThis, 0x7A);
        const DataNode *n = gDataThis->Property(mValue.array, true);
#ifdef HX_NATIVE
        if (!n) {
            MILO_WARN("DataNode::Evaluate: property lookup returned null on %s", PathName(gDataThis));
            static DataNode sNullNode(0);
            return sNullNode;
        }
#endif
        return UseQueue(*n);
    } else
        return *this;
}

bool DataNode::NotNull() const {
    const DataNode &n = Evaluate();
    DataType t = n.Type();
    if (t == kDataSymbol) {
        return n.mValue.symbol[0] != 0;
    } else if (t == kDataString) {
        return n.mValue.array->Size() < -1;
    } else if (t == kDataGlob) {
        return n.mValue.array->Size() & -1;
    } else
        return n.mValue.array != 0;
}

bool DataVarExists(Symbol s) { return gDataVars.find(s) != gDataVars.end(); }

const char *DataVarName(const DataNode *var) {
    FOREACH (it, gDataVars) {
        if ((&it->second) == var) {
            return it->first.Str();
        }
    }
    return "<null>";
}

inline bool HasSpace(const char *str) {
    while (*str != '\0') {
        if (*str++ == ' ')
            return true;
    }
    return false;
}

void DataNode::Print(TextStream &ts, bool b) const {
    switch (mType) {
    case kDataUnhandled:
        ts << "kDataUnhandled";
        break;
    case kDataInt:
        ts << mValue.integer;
        break;
    case kDataString:
        if (b)
            ts << mValue.var->mValue.symbol;
        else {
            ts << "\"";
            char *tok = strtok((char *)mValue.var->mValue.symbol, "\"");
            while (tok) {
                ts << tok;
                tok = strtok(0, "\"");
                if (tok) {
                    ts << "\\q";
                    tok[-1] = '\"';
                }
            }
            ts << "\"";
        }
        break;
    case kDataSymbol:
        if (!HasSpace(mValue.symbol))
            ts << mValue.symbol;
        else
            ts << "'" << mValue.symbol << "'";
        break;
    case kDataGlob:
        ts << "<glob " << -mValue.array->Size() << ">";
        break;
    case kDataFloat:
        ts << mValue.real;
        break;
    case kDataArray:
    case kDataCommand:
    case kDataProperty:
        mValue.array->Print(ts, mType, b);
        break;
    case kDataObject:
        ts << mValue.object;
        break;
    case kDataVar:
        ts << '$' << DataVarName(mValue.var);
        break;
    case kDataFunc:
        ts << DataFuncName(mValue.func);
        break;
    case kDataDefine:
        ts << "\n#define " << mValue.symbol << "\n";
        break;
    case kDataUndef:
        ts << "\n#undef " << mValue.symbol << "\n";
        break;
    case kDataIfdef:
        ts << "\n#ifdef " << mValue.symbol << "\n";
        break;
    case kDataIfndef:
        ts << "\n#ifndef " << mValue.symbol << "\n";
        break;
    case kDataAutorun:
        ts << "\n#autorun\n";
        break;
    case kDataElse:
        ts << "\n#else\n";
        break;
    case kDataEndif:
        ts << "\n#endif\n";
        break;
    case kDataInclude:
        ts << "\n#include " << mValue.symbol << "\n";
        break;
    case kDataMerge:
        ts << "\n#merge " << mValue.symbol << "\n";
        break;
    }
}

bool DataNode::PrintUnused(TextStream &, bool) const {
    MILO_NOTIFY("Enable PRINT_UNUSED in Data.h to utilize DataNode::PrintUnused()");
    return false;
}

void DataNode::Save(BinStream &d) const {
    d << mType;
    switch (mType) {
    case kDataSymbol:
    case kDataIfdef:
    case kDataDefine:
    case kDataInclude:
    case kDataMerge:
    case kDataIfndef:
    case kDataUndef:
        d << mValue.symbol;
        break;
    case kDataFloat:
        d << mValue.real;
        break;
    case kDataString:
    case kDataGlob:
        mValue.array->SaveGlob(d, mType == kDataString);
        break;
    case kDataArray:
    case kDataCommand:
    case kDataProperty:
        mValue.array->Save(d);
        break;
    case kDataObject:
        if (mValue.object)
            d << mValue.object->Name();
        else
            d << "";
        break;
    case kDataVar:
        d << DataVarName(mValue.var);
        break;
    case kDataFunc:
        d << DataFuncName(mValue.func);
        break;
    case kDataInt:
    case kDataUnhandled:
    case kDataElse:
    case kDataEndif:
    case kDataAutorun:
        d << mValue.integer;
        break;
    default:
        MILO_FAIL("Unrecognized node type: %x", mType);
        break;
    }
}

int DataNode::Int(const DataArray *source) const {
    const DataNode &n = Evaluate();
// Retail has NO type check here.  0x8274b0f8 is 36 B, fan-in 2385:
// mflr/stw/stwu / bl Evaluate / lwz r3,0(r3) / addi/lwz/mtlr/blr, and nothing
// else -- it is the ONLY accessor in the DataNode cluster without a test.  The
// ones that DO test in retail are testing for real dispatch, not asserting
// (Float tests kDataInt to convert, GetObj tests kDataObject/kDataSymbol to
// look up).  So this block is dev-build-only and retail took the false branch;
// the rb3-Wii source it was ported from spells it `#ifdef MILO_DEBUG`, which
// src/macros.h force-defines here -- hence the HX_NATIVE conjunct.  See
// docs/decomp/patterns/milo-debug-force-define.md for the house pattern.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (n.mType != kDataInt) {
        String s;
        n.Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Int (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Int", s);
    }
#endif
    return n.mValue.integer;
}

int DataNode::LiteralInt(const DataArray *source) const {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (mType != kDataInt) {
        String s;
        Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Int (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Int", s);
    }
#endif
    return mValue.integer;
}

Symbol DataNode::Sym(const DataArray *source) const {
    const DataNode &n = Evaluate();
// Same dev-build-only assert as Int/Array below.  Retail 0x8274af68 is 60 B,
// `bl Evaluate / lwz r11,0(r3) / mr r3,r31 / stw r11,0(r31)` around the sret
// slot -- it evaluates and stores mValue.symbol with NO cmpwi against
// kDataSymbol (= 5) anywhere in the body.  The 88-byte body two slots down
// (0x8274afa8) DOES test 5, but that is ForceSym's symbol-or-string dispatch,
// not an assert.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (n.mType != kDataSymbol) {
        String s;
        n.Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Symbol (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Symbol", s);
        return Symbol("");
    }
#endif
    return STR_TO_SYM(n.mValue.symbol);
}

Symbol DataNode::LiteralSym(const DataArray *source) const {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (mType != kDataSymbol) {
        String s;
        Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Symbol (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Symbol", s);
#ifdef HX_NATIVE
        return Symbol("");
#endif
    }
#endif
    return STR_TO_SYM(mValue.symbol);
}

Symbol DataNode::ForceSym(const DataArray *source) const {
    const DataNode &n = Evaluate();
    if (n.mType == kDataSymbol) {
        return STR_TO_SYM(n.mValue.symbol);
    } else {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (n.mType != kDataString) {
            String s;
            n.Print(s, true);
            if (source)
                MILO_FAIL_DTA(
                    "Data %s is not String (file %s, line %d)",
                    s.c_str(),
                    source->File(),
                    source->Line()
                );
            else
                MILO_FAIL_DTA("Data %s is not String", s);
#ifdef HX_NATIVE
            return Symbol("");
#endif
        }
#endif
        return Symbol(n.mValue.var->mValue.symbol);
    }
}

const char *DataNode::Str(const DataArray *source) const {
    const DataNode &n = Evaluate();
    if (n.mType == kDataSymbol) {
        return n.mValue.symbol;
    } else {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (n.mType != kDataString) {
            String s;
            n.Print(s, true);
            if (source)
                MILO_FAIL_DTA(
                    "Data %s is not String (file %s, line %d)",
                    s.c_str(),
                    source->File(),
                    source->Line()
                );
            else
                MILO_FAIL_DTA("Data %s is not String", s);
#ifdef HX_NATIVE
            return "";
#endif
        }
#endif
        return n.mValue.var->mValue.symbol;
    }
}

const char *DataNode::LiteralStr(const DataArray *source) const {
    if (mType == kDataSymbol) {
        return mValue.symbol;
    } else {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (mType != kDataString) {
            String s;
            Print(s, true);
            if (source)
                MILO_FAIL_DTA(
                    "Data %s is not String (file %s, line %d)",
                    s.c_str(),
                    source->File(),
                    source->Line()
                );
            else
                MILO_FAIL_DTA("Data %s is not String", s);
#ifdef HX_NATIVE
            return "";
#endif
        }
#endif
        return mValue.var->mValue.symbol;
    }
}

float DataNode::Float(const DataArray *source) const {
    const DataNode &n = Evaluate();
    if (n.mType == kDataInt) {
        return n.mValue.integer;
    } else {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (n.mType != kDataFloat) {
            String s;
            n.Print(s, true);
            if (source)
                MILO_FAIL_DTA(
                    "Data %s is not Float (file %s, line %d)",
                    s.c_str(),
                    source->File(),
                    source->Line()
                );
            else
                MILO_FAIL_DTA("Data %s is not Float", s);
        }
#endif
        return n.mValue.real;
    }
}

float DataNode::LiteralFloat(const DataArray *source) const {
    if (mType == kDataInt) {
        return mValue.integer;
    } else {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
        if (mType != kDataFloat) {
            String s;
            Print(s, true);
            if (source)
                MILO_FAIL_DTA(
                    "Data %s is not Float (file %s, line %d)",
                    s.c_str(),
                    source->File(),
                    source->Line()
                );
            else
                MILO_FAIL_DTA("Data %s is not Float", s);
        }
#endif
        return mValue.real;
    }
}

DataFunc *DataNode::Func(const DataArray *source) const {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (mType != kDataFunc) {
        String s;
        Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Func (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Func", s);
#ifdef HX_NATIVE
        return nullptr;
#endif
    }
#endif
    return mValue.func;
}

Hmx::Object *DataNode::GetObj(const DataArray *source) const {
    const DataNode &n = Evaluate();
    if (n.mType == kDataObject)
        return n.mValue.object;
    else {
        const char *str = n.LiteralStr(source);
        Hmx::Object *ret = 0;
        if (*str != '\0') {
            ret = gDataDir->FindObject(str, true);
// Retail (0x8274bf4c, 108 B) has NO not-found path here at all: after
// FindObject it goes straight to the epilogue -- no PathName, no MakeString,
// no Debug::Fail, and not even a `bl LiteralStr` (retail INLINES LiteralStr's
// dispatch: cmpwi 5 -> lwz r4,0(r3), else lwz r11,0(r3) / lwz r4,0(r11)).
// The rb3-Wii source wraps this whole block in `#ifdef MILO_DEBUG`, which
// src/macros.h force-defines here -- hence the HX_NATIVE conjunct.  Gating it
// also shrinks LiteralStr enough for /Ob2 to inline it, which is what makes
// the dispatch above match.  See docs/decomp/patterns/milo-debug-force-define.md.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
            if (!ret) {
                // Native flow lacks many game objects (HUD, score, etc.) that
                // song animations reference. Warn instead of crashing so the
                // LightPreset animation can still drive venue visibility.
                const char *msg =
                    PathName(gDataDir) != nullptr ? PathName(gDataDir) : "**no file**";
                MILO_WARN("GetObj: %s not found in %s\n", str, msg);
            }
#endif
        }
        return ret;
    }
}

DataArray *DataNode::Array(const DataArray *source) const {
    const DataNode &n = Evaluate();
// Same as Int above: no retail type check.  Data.h gives kDataArray = 16, and
// NO retail function in band.exe tests r11 against 0x10 -- so retail's Array
// carries no runtime test and reduces to the identical nine words as Int, which
// is why /OPT:ICF folded the two spellings onto 0x8274b0f8.
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (n.mType != kDataArray) {
        String s;
        n.Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Array (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Array", s);
        return nullptr;
    }
#endif
    return n.mValue.array;
}

DataArray *DataNode::LiteralArray(const DataArray *source) const {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (mType != kDataArray) {
        String s;
        Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Array (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Array", s);
#ifdef HX_NATIVE
        return nullptr;
#endif
    }
#endif
    return mValue.array;
}

DataArray *DataNode::Command(const DataArray *source) const {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (mType != kDataCommand) {
        String s;
        Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Command (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Command", s);
#ifdef HX_NATIVE
        return nullptr;
#endif
    }
#endif
    return mValue.array;
}

DataNode *DataNode::Var(const DataArray *source) const {
#if defined(MILO_DEBUG) && defined(HX_NATIVE)
    if (mType != kDataVar) {
        String s;
        Print(s, true);
        if (source)
            MILO_FAIL_DTA(
                "Data %s is not Var (file %s, line %d)",
                s.c_str(),
                source->File(),
                source->Line()
            );
        else
            MILO_FAIL_DTA("Data %s is not Var", s);
#ifdef HX_NATIVE
        return nullptr;
#endif
    }
#endif
    return mValue.var;
}

bool DataNode::operator>(const DataNode &other) const {
    if ((mType == kDataInt || mType == kDataFloat)
        && (other.mType == kDataInt || other.mType == kDataFloat)) {
        return LiteralFloat() > other.LiteralFloat();
    } else
        return false;
}

bool DataNode::Equal(const DataNode &n, DataArray *a, bool warn) const {
    DataType otherType = n.Type();
    const DataNode &first = mType < n.Type() ? *this : n;
    const DataNode &second = mType < otherType ? n : *this;
    DataType firstType = first.Type();
    DataType secondType = second.Type();
    if (firstType == secondType) {
        bool res;
        if (firstType == kDataString) {
            res = streq(first.UncheckedStr(), second.UncheckedStr());
#ifdef HX_NATIVE
        } else if (firstType == kDataSymbol) {
            // On 64-bit, UncheckedInt() truncates the 8-byte symbol pointer to 4 bytes.
            // Compare the full pointers instead.
            res = first.UncheckedStr() == second.UncheckedStr();
#endif
        } else {
            res = second.UncheckedInt() == first.UncheckedInt();
        }
        return res;
    } else {
        const char *objName = "";
        if (firstType == kDataInt && secondType == kDataFloat) {
            return (float)first.UncheckedInt() == second.UncheckedFloat();
        } else {
            if (firstType == kDataObject) {
                Hmx::Object *obj = first.UncheckedObj();
                if (obj)
                    objName = obj->Name();
                if (secondType == kDataSymbol) {
                    return streq(objName, second.UncheckedStr());
                } else if (secondType == kDataString) {
                    return streq(objName, second.UncheckedStr());
                }
            }
            if (firstType == kDataSymbol) {
                if (secondType == kDataString) {
                    return streq(first.UncheckedStr(), second.UncheckedStr());
                }
            } else if (secondType != kDataString && secondType != kDataSymbol) {
                warn &= secondType != kDataObject; // i dunno lol
            }
        }
        if (firstType == kDataUnhandled || secondType == kDataUnhandled) {
            warn = false;
        }
        if (warn) {
            StackString<32> str1;
            StackString<32> str2;
            first.Print(str1, true);
            second.Print(str2, true);
            MILO_NOTIFY_ONCE(
                "DataNode::Equal: DataNodes %s and %s (%s and %s) are not compatible (file %s, line %d)",
                str1,
                str2,
                DataTypeString(first.Type()),
                DataTypeString(second.Type()),
                a ? a->File() : "",
                a ? a->Line() : -1
            );
        }
    }
    return false;
}

// RB3 retail has NO `DataNode::Equal` — the whole comparison is written inline in
// `operator==` (0x8274ABA0, 464 bytes). `Equal(n, a, warn)` is a LATER Harmonix
// refactor that we inherited from dc3-decomp, and its shape is materially
// different: `Equal` orders the two nodes by type (`first`/`second`) and then
// tests one direction, where retail is symmetric and tests `this`/`other`
// directly. That is not a codegen difference, it is a different function.
//
// House rule for a dc3-newer divergence: reproduce retail in the match build and
// keep the newer/safer behaviour under HX_NATIVE. Retail's version is genuinely
// looser — e.g. object-vs-int falls into the object branch and reinterprets the
// int as a `String*` — which is survivable on ILP32 Xbox but a wild read on LP64,
// so the native port keeps `Equal`.
#ifdef HX_NATIVE
bool DataNode::operator==(const DataNode &other) const {
    return Equal(other, nullptr, true);
}
#else
bool DataNode::operator==(const DataNode &other) const {
    if (mType == other.mType) {
        if (mType == kDataString) {
            // Both types are known to be kDataString here, so retail derefs both
            // String pointers unconditionally. Routing either side through
            // LiteralStr() re-emits a kDataSymbol test: MSVC folds it away for
            // `this` (whose mType it compared directly) but NOT for `other`.
            return streq(mValue.var->mValue.symbol, other.mValue.var->mValue.symbol);
        } else {
            return mValue.integer == other.mValue.integer;
        }
    } else if (mType == kDataObject || other.mType == kDataObject) {
        // Both names are resolved inside ONE if/else rather than two sequential
        // diamonds. The types are known to differ, so exactly one side is the
        // object -- retail emits two fully specialised paths with no second test,
        // and this is the shape that reproduces it. Two sequential
        // `if (mType==obj)` / `if (other.mType==obj)` selections instead emit a
        // redundant test for the second name and let MSVC hoist the shared "".
        const char *name1;
        const char *name2;
        if (mType == kDataObject) {
            Hmx::Object *obj = mValue.object;
            name1 = obj ? obj->Name() : "";
            name2 = other.LiteralStr();
        } else {
            name1 = LiteralStr();
            Hmx::Object *obj = other.mValue.object;
            name2 = obj ? obj->Name() : "";
        }
        return streq(name1, name2);
    } else if (mType == kDataString || other.mType == kDataString) {
        return streq(LiteralStr(), other.LiteralStr());
    } else if (mType == kDataFloat || other.mType == kDataFloat) {
        return LiteralFloat() == other.LiteralFloat();
    } else {
        return false;
    }
}
#endif

// Retail's operator!= is a real out-of-line `bl` to operator== plus a negate
// (0x8274AD78). It only emits that way because operator== is now too big to
// inline — while operator== was a 12-byte forwarder, MSVC inlined it straight
// back and this function was unfixable on its own (lane NEARMISS-1).
bool DataNode::operator!=(const DataNode &other) const {
    return !operator==(other);
}

DataNode &DataVariable(Symbol s) { return gDataVars[s]; }

void DataNode::Load(BinStream &d) {
    static char buf[128];
    d >> (int &)mType;
    switch (mType) {
    case kDataFunc: {
        Symbol sym;
        d >> sym;
        const std::map<Symbol, DataFunc *>::iterator it = gDataFuncs.find(sym);
        if (it == gDataFuncs.end()) {
            MILO_FAIL("Couldn't bind %s", sym);
        }
        mValue.func = it->second;
        break;
    }
    case kDataSymbol:
    case kDataIfdef:
    case kDataDefine:
    case kDataInclude:
    case kDataMerge:
    case kDataIfndef:
    case kDataUndef: {
        Symbol sym;
        d >> sym;
        mValue.symbol = sym.Str();
        break;
    }
    case kDataFloat:
        d >> mValue.real;
        break;
    case kDataString:
    case kDataGlob:
        mValue.array = new DataArray(0);
        mValue.array->LoadGlob(d, mType == kDataString);
        break;
    case kDataArray:
    case kDataCommand:
    case kDataProperty:
        mValue.array = new DataArray(0);
        mValue.array->Load(d);
        break;
    case kDataObject:
        d.ReadString(buf, 0x80);
        mValue.object = gDataDir->FindObject(buf, true);
        if (!mValue.object && *buf) {
            MILO_NOTIFY("Couldn't find %s from %s", buf, gDataDir->Name());
        }
        break;
    case kDataVar: {
        Symbol sym;
        d >> sym;
        Symbol key = sym;
        mValue.var = &gDataVars[key];
        break;
    }
    case kDataUnhandled:
    case kDataInt:
    case kDataElse:
    case kDataEndif:
    case kDataAutorun:
        d >> mValue.integer;
        break;
    default:
        MILO_FAIL("Unrecognized node type: %x", mType);
        break;
    }
}

void DataNode::Load(BinStream &d, ObjectDir *dir) {
    ObjectDir *old = gDataDir;
    gDataDir = dir;
    Load(d);
    gDataDir = old;
}

// RB3 retail linker interleaved DataArray.cpp COMDATs into this TU's .text span.
// Compile its bodies here so objdiff can pair them (sw scatter-scan).
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "obj/DataArray.cpp"
#endif

// sw2 scatter-include (default/DataNode <- band3/meta_band/BandSongMetadata.cpp)
#define gRev gRev_BandSongMetadata
#define gAltRev gAltRev_BandSongMetadata
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "band3/meta_band/BandSongMetadata.cpp"
#endif
#undef gRev
#undef gAltRev
