#include "obj/DataFile.h"

#include <map>
#include "DataFlex.h"
#include "math/Utl.h"
#include "obj/DataUtl.h"
#include "utl/BinStream.h"
#include "utl/Compress.h"
#include "utl/Std.h"
#include "math/FileChecksum.h"
#include "obj/Data.h"
#include "obj/DataFile_Flex.h"
#include "obj/DataUtl.h"
#include "os/CritSec.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/OSFuncs.h"
#include "os/System.h"
#include "os/ThreadCall.h"
#include "utl/BufStream.h"
#include "utl/FileStream.h"
#include "utl/FilePath.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"

CriticalSection gDataReadCrit; // yes these are the bss offsets. this tu sucks
DataArray *gArray; // 0x28
int gNode; // 0x2c
Symbol gFile; // 0x30
BinStream *gBinStream; // 0x34
int gOpenArray; // 0x38
std::list<bool> gConditional; // 0x48
DataType gDataLine; // 0x50
std::map<String, DataNode> gReadFiles; // 0x60

// bool gCompressCached;
bool gCachingFile;
bool gReadingFile;

#ifdef HX_NATIVE
static int gParseDepth = 0;
static const int kMaxParseDepth = 512;
#endif

bool Defined() {
    for (std::list<bool>::iterator it = gConditional.begin();
         it != gConditional.end();
         it++) {
        if (!*it)
            return false;
    }
    return true;
}

void PushBack(const DataNode &n) {
    if (gNode == gArray->Size()) {
        int maxLines = 0x7FFF;
        if (gNode >= 0x7FFF) {
            MILO_FAIL(
                "%s(%d): array size > max %d lines", gArray->File(), gArray->Line(), maxLines
            );
        }
        MemPushTemp();
        int x = gNode << 1;
        if (x > 0x7FFF) x = 0x7FFF;
        gArray->Resize(x);
        MemPopTemp();
    }
    gArray->Node(gNode++) = n;
}

bool ParseNode() {
    int token = yylex();
    if (!Defined() && token != kDataTokenIfdef && token != kDataTokenIfndef
        && token != kDataTokenElse && token != kDataTokenEndif) {
        return true;
    }

    static const char bom[3] = { (char)0xEF, (char)0xBB, (char)0xBF };
    if (gNode == 0 && strncmp(yytext, bom, 3) == 0) {
        if (yyleng > 3)
            MILO_FAIL(
                "%s starts with a ByteOrderMark, put a line return at the top of its file",
                gFile
            );
        else
            return true;
    }

    if (token == kDataTokenFinished) {
        if (gOpenArray != kDataTokenFinished) {
            if (gOpenArray == kDataTokenArrayOpen) {
                MILO_FAIL("Array closed incorrectly (file %s, line %d)", gFile, gDataLine);
            } else if (gOpenArray == kDataTokenCommandOpen) {
                MILO_FAIL("Command closed incorrectly (file %s, line %d)", gFile, gDataLine);
            } else if (gOpenArray == kDataTokenPropertyOpen) {
                MILO_FAIL("Property closed incorrectly (file %s, line %d)", gFile, gDataLine);
            }
        }
        return false;
    } else if (token == kDataTokenArrayClose) {
        if (gOpenArray == kDataTokenArrayOpen) {
            return false;
        } else if (gOpenArray == kDataTokenFinished) {
            MILO_FAIL("File %s ends with open array", gFile);
        } else if (gOpenArray == kDataTokenCommandOpen) {
            MILO_FAIL("Command closed incorrectly (file %s, line %d)", gFile, gDataLine);
        } else if (gOpenArray == kDataTokenPropertyOpen) {
            MILO_FAIL("Property closed incorrectly (file %s, line %d)", gFile, gDataLine);
        }
        return false;
    } else if (token == kDataTokenPropertyClose) {
        if (gOpenArray == kDataTokenPropertyOpen) {
            return false;
        } else if (gOpenArray == kDataTokenFinished) {
            MILO_FAIL("File %s ends with open array", gFile);
        } else if (gOpenArray == kDataTokenArrayOpen) {
            MILO_FAIL("Array closed incorrectly (file %s, line %d)", gFile, gDataLine);
        } else if (gOpenArray == kDataTokenCommandOpen) {
            MILO_FAIL("Command closed incorrectly (file %s, line %d)", gFile, gDataLine);
        }
        return false;
    } else if (token == kDataTokenCommandClose) {
        if (gOpenArray == kDataTokenCommandOpen) {
            return false;
        } else if (gOpenArray == kDataTokenFinished) {
            MILO_FAIL("File %s ends with open array", gFile);
        } else if (gOpenArray == kDataTokenArrayOpen) {
            MILO_FAIL("Array closed incorrectly (file %s, line %d)", gFile, gDataLine);
        } else if (gOpenArray == kDataTokenPropertyOpen) {
            MILO_FAIL("Property closed incorrectly (file %s, line %d)", gFile, gDataLine);
        }
        return false;
    }

    if (token == kDataTokenMerge) {
        if (yylex() != kDataTokenSymbol) {
            MILO_FAIL(
                "DataReadFile: merging a non-symbol (file %s, line %d)", gFile, gDataLine
            );
        }
        if (gCachingFile) {
            PushBack(DataNode(kDataMerge, Symbol(yytext).Str()));
        } else {
            bool usingEmbedded = false;
            DataArray *fileArr = DataGetMacro(yytext);
            if (!fileArr) {
                fileArr = ReadEmbeddedFile(yytext, true);
                usingEmbedded = true;
            }
            if (fileArr && fileArr->Size() == 0) {
                MILO_FAIL("Empty merge file (possibly a re-included file): %s", yytext);
            }
            gArray->Resize(gNode);
            DataMergeTags(gArray, fileArr);
            gNode = gArray->Size();
            if (usingEmbedded) {
                fileArr->Release();
            }
        }
        return true;
    } else if (token == kDataTokenInclude || token == kDataTokenIncludeOptional) {
        bool required = token == kDataTokenInclude;
        if (yylex() != kDataTokenSymbol) {
            MILO_FAIL(
                "DataReadFile: including a non-symbol (file %s, line %d)",
                gFile,
                gDataLine
            );
        }
        if (gCachingFile) {
            PushBack(DataNode(kDataInclude, Symbol(yytext).Str()));
        } else {
            DataArray *fileArr = ReadEmbeddedFile(yytext, required);
            if (fileArr) {
                for (int i = 0; i < fileArr->Size(); i++) {
                    PushBack(fileArr->Node(i));
                }
                fileArr->Release();
            }
        }
        return true;
    }

    if (token == kDataTokenIfdef || token == kDataTokenIfndef) {
        bool positive = token == kDataTokenIfdef;

        int symToken = yylex();
        if (symToken != kDataTokenSymbol && symToken != kDataTokenQuotedSymbol) {
            MILO_FAIL(
                "DataReadFile: not macro symbol (file %s, line %d)", gFile, gDataLine
            );
        }

        char *text;
        if (symToken == kDataTokenQuotedSymbol) {
            yytext[yyleng - 1] = '\0';
            text = yytext + 1;
        } else {
            text = yytext;
        }

        Symbol macro(text);
        if (positive) {
            if (gCachingFile) {
                PushBack(DataNode(kDataIfdef, macro.Str()));
            } else {
                bool defined = DataGetMacro(macro) != 0;
                gConditional.push_back(defined);
            }
        } else {
            if (gCachingFile) {
                PushBack(DataNode(kDataIfndef, macro.Str()));
            } else {
                bool ndefined = DataGetMacro(macro) == 0;
                gConditional.push_back(ndefined);
            }
        }
        return true;
    } else if (token == kDataTokenElse) {
        if (gCachingFile) {
            PushBack(DataNode(kDataElse, 0));
        } else {
            if (gConditional.empty()) {
                MILO_FAIL(
                    "DataReadFile: #else not in conditional (file %s, line %d)",
                    gFile,
                    gDataLine
                );
            }
            gConditional.back() = !gConditional.back();
        }
        return true;
    } else if (token == kDataTokenEndif) {
        if (gCachingFile) {
            PushBack(DataNode(kDataEndif, 0));
        } else {
            if (gConditional.empty()) {
                MILO_FAIL(
                    "DataReadFile: #endif not in conditional (file %s, line %d)",
                    gFile,
                    gDataLine
                );
            }
            gConditional.pop_back();
        }
        return true;
    } else if (token == kDataTokenAutorun) {
        int cmdToken = yylex();
        if (cmdToken != kDataTokenCommandOpen) {
            MILO_FAIL("DataReadFile: not command (file %s, line %d)", gFile, gDataLine);
        }

        int openArray = gOpenArray;
        gOpenArray = cmdToken;
        DataArray *array = ParseArray();
        gOpenArray = openArray;

        DataNode node(array, kDataCommand);
        if (gCachingFile) {
            PushBack(DataNode(kDataAutorun, 0));
            PushBack(node);
        } else {
            node.Command(array)->Execute();
        }

        array->Release();
        return true;
    } else if (token == kDataTokenDefine) {
        if (yylex() != kDataTokenSymbol) {
            MILO_FAIL("DataReadFile: not symbol (file %s, line %d)", gFile, gDataLine);
        }

        Symbol macro(yytext);

        int cmdToken = yylex();
        if (cmdToken != kDataTokenArrayOpen) {
            MILO_FAIL("DataReadFile: not array (file %s, line %d)", gFile, gDataLine);
        }

        int openArray = gOpenArray;
        gOpenArray = cmdToken;
        DataArray *array = ParseArray();
        gOpenArray = openArray;

        if (gCachingFile) {
            PushBack(DataNode(kDataDefine, macro.Str()));
            PushBack(DataNode(array, kDataArray));
        } else {
            DataSetMacro(macro, array);
        }

        array->Release();
        return true;
    } else if (token == kDataTokenUndef) {
        if (yylex() != kDataTokenSymbol) {
            MILO_FAIL("DataReadFile: not synbol (file %s, line %d)", gFile, gDataLine);
        }

        Symbol macro(yytext);
        if (gCachingFile) {
            PushBack(DataNode(kDataUndef, macro.Str()));
        } else {
            DataSetMacro(macro, nullptr);
        }

        return true;
    } else if (token == kDataTokenArrayOpen || token == kDataTokenCommandOpen
               || token == kDataTokenPropertyOpen) {
        int openArray = gOpenArray;
        gOpenArray = token;
        DataArray *array = ParseArray();
        gOpenArray = openArray;

        DataType type;
        if (token == kDataTokenArrayOpen) {
            type = kDataArray;
        } else if (token == kDataTokenCommandOpen) {
            type = kDataCommand;
        } else {
            type = kDataProperty;
        }

        PushBack(DataNode(array, type));
        array->Release();

        return true;
    } else if (token == kDataTokenVar) {
        PushBack(&DataVariable(yytext + 1));
        return true;
    } else if (token == kDataTokenUnhandled) {
        PushBack(DataNode(kDataUnhandled, 0));
        return true;
    } else if (token == kDataTokenInt) {
        PushBack(atoi(yytext));
        return true;
    } else if (token == kDataTokenHex) {
        int i = 0;

        int base = 1;
        for (char *c = yytext + strlen(yytext) - 1; *c != 'x'; --c, base <<= 4) {
            if (*c >= 'a') {
                i += (*c - 'a' + 10) * base;
            } else if (*c > 'A') {
                i += (*c - 'A' + 10) * base;
            } else {
                i += (*c - '0') * base;
            }
        }

        PushBack(i);
        return true;
    } else if (token == kDataTokenFloat) {
        PushBack((float)atof(yytext));
        return true;
    } else if (token == kDataTokenSymbol || token == kDataTokenQuotedSymbol) {
        char *text;
        if (token == kDataTokenQuotedSymbol) {
            yytext[yyleng - 1] = '\0';
            text = yytext + 1;
        } else {
            text = yytext;
        }

        Symbol sym(text);
        DataArray *macro = DataGetMacro(sym);
        bool b = macro && !gCachingFile;
        if (b) {
            for (int i = 0; i < macro->Size(); i++) {
                PushBack(macro->Node(i));
            }
        } else {
            PushBack(sym);
        }

        return true;
    } else if (token == kDataTokenString) {
        yytext[yyleng - 1] = '\0';
        char *text = yytext + 1;

        for (char *c = text; *c != '\0'; c++) {
            bool escaped = false;
            if (*c == '\\') {
                if (c[1] == 'n') {
                    *c = '\n';
                    escaped = true;
                } else if (c[1] == 'q') {
                    *c = '\"';
                    escaped = true;
                } else if (c[1] == 't') {
                    *c = '\t';
                    escaped = true;
                }
            } else if (*c == '\n') {
                gDataLine = (DataType)((int)gDataLine + 1);
            }

            if (escaped) {
                for (char *d = c + 1; *d != '\0'; d++) {
                    *d = *(d + 1);
                }
            }
        }

        PushBack(text);
        return true;
    } else {
        MILO_FAIL(
            "DataReadFile: Unrecognized token %d (file %s, line %d)",
            token,
            gFile,
            gDataLine
        );
        return false;
    }
}

DataArray *ParseArray() {
#ifdef HX_NATIVE
    if (++gParseDepth > kMaxParseDepth) {
        gParseDepth--;
        MILO_FAIL("DTA parse depth exceeded %d (file %s, line %d)", kMaxParseDepth, gFile, gDataLine);
        DataArray *empty = new DataArray(0);
        empty->SetFileLine(gFile, gDataLine);
        return empty;
    }
#endif
    DataArray *sav = gArray;
    int nod = gNode;
    DataArray *da = new DataArray(16);
    gArray = da;
    da->SetFileLine(gFile, gDataLine);
    gNode = 0;
    do
        ;
    while (ParseNode());
    gArray->Resize(gNode);
    da = gArray;
    gArray = sav;
    gNode = nod;
#ifdef HX_NATIVE
    gParseDepth--;
#endif
    return da;
}

int DataInput(void *v, int x) {
    if (gBinStream->Fail()) {
        return 0;
    } else if (gBinStream->Eof()) {
        return 0;
    } else {
        gBinStream->Read(v, x);
        MILO_ASSERT(!gBinStream->Fail(), 0x260);
        return x;
    }
}

void DataWriteFile(const char *file, const DataArray *da, int i) {
    TextStream *stream;
    int idx;
    if (file != 0) {
        stream = new TextFileStream(file, false);
    }
    else {
        stream = new Debug();
    }
    idx = i;
    for (; idx < da->Size(); idx++) {
        da->Node(idx).Print(*stream, false, 0);
        stream->operator<<("\n");
    }
    if (stream) {
        delete stream;
    }
}

void BeginDataRead() {
    MILO_ASSERT(gReadFiles.size() == 0, 0x29b);
    gReadingFile = 1;
}

void FinishDataRead() {
    gReadingFile = 0;
    std::map<String, DataNode> temp;
    gReadFiles.swap(temp);
}

DataArray *DataReadString(const char *c) {
    BufStream stream((void *)c, strlen(c), true);
    return DataReadStream(&stream);
}

DataArray *ReadCacheStream(BinStream &bs, const char *cc) {
    CritSecTracker cst(&gDataReadCrit); // TODO: may cause IAT thunk issues at runtime
    bs.EnableReadEncryption();
    DataArray::SetFile(cc);
    DataArray *arr;
    bs >> arr;
    bs.DisableEncryption();
    return arr;
}

DataArray *DataReadFile(const char *file, bool warn) {
    char buf[256];
    strcpy(buf, file);
    bool b;
    DataNode *node;
    const char *cached = CachedDataFile(buf, b);
    if (gReadingFile) {
        node = &gReadFiles[cached];
        if (node->Type() == kDataArray) {
            DataArray *arr = node->LiteralArray();
            arr->AddRef();
            return arr;
        }
    } else {
        node = nullptr;
        BeginDataRead();
    }

    FileStream fs(cached, FileStream::kRead, true);
    if (fs.Fail()) {
        if (warn)
            MILO_WARN("DataReadFile: Can't open %s", buf);
        if (!node)
            FinishDataRead();
        return nullptr;
    } else {
        DataArray *ret;
        if (b) {
            if (HasFileChecksumData()) {
                fs.StartChecksum();
            }
            ret = ReadCacheStream(fs, buf);
            if (HasFileChecksumData()) {
                fs.ValidateChecksum();
            }
        } else {
            ret = DataReadStream(&fs);
        }

        if (node) {
            auto dataNode = DataNode(ret, kDataArray);
            *node = dataNode;
        } else {
            FinishDataRead();
        }
        return ret;
    }
}

DataArray *DataReadStream(BinStream *bs) {
    gDataReadCrit.Enter(); // TODO: may cause IAT thunk issues at runtime
    Symbol stream(bs->Name());
    gBinStream = bs;
    gNode = 0;
    gOpenArray = 0;
    gDataLine = (DataType)1;
    unsigned int conds1 = 0;
    gFile = stream;
    FOREACH(it, gConditional) {
        conds1++;
    }
    DataArray *parse = ParseArray();
    unsigned int conds2 = 0;
    FOREACH(it, gConditional) {
        conds2++;
    }
    if (conds2 != conds1) {
        MILO_FAIL("DataReadFile: conditional block not closed (file %s)", gFile);
    }
    gDataReadCrit.Exit(); // TODO: may cause IAT thunk issues at runtime
    return parse;
}

DataArray *LoadDtz(const char *c, int i) {
    int decompSize;
    decompSize = 0;
    ((unsigned char *)&decompSize)[0] = c[i - 1];
    ((unsigned char *)&decompSize)[1] = c[i - 2];
    ((unsigned char *)&decompSize)[2] = c[i - 3];
    ((unsigned char *)&decompSize)[3] = c[i - 4];
    MILO_ASSERT(decompSize > 0, 0x456);
    void *pDecompBuf = MemAlloc(decompSize, __FILE__, 0x459, "LoadDtz", 0);
    MILO_ASSERT(pDecompBuf, 0x45b);
    DecompressMem(c, i - 4, pDecompBuf, decompSize, 0);
    BufStream buf_stream = BufStream(pDecompBuf, decompSize, true);
    DataArray *da;
    da = 0;
    buf_stream >> da;
    if (pDecompBuf) {
        MemFree(pDecompBuf, __FILE__, 0x46a, "unknown");
    }
    return da;
}

const char *CachedDataFile(const char *file, bool &b) {
    bool isLocal = FileIsLocal(file);
    if (strstr(file, ".dtb")) {
        b = true;
        return file;
    }
    if (UsingCD() && !isLocal) {
        b = true;
        const char *filebase = FileGetBase(file);
        const char *filepath = FileGetPath(file);
        const char *result = MakeString("%s/gen/%s.dtb", filepath, filebase);
        return result;
    }
    b = false;
    return file;
}

void DataFail(const char *msg) {
    MILO_FAIL("%s (file %s, line %d)", msg, gFile, gDataLine);
}

DataArray *ReadEmbeddedFile(const char *file, bool b) {
    CritSecTracker cst(&gDataReadCrit);
    const char *madePath = FileMakePath(FileGetPath(gFile.Str()), file);
    Symbol localfile = gFile;

    BinStream *bs = gBinStream;
    DataType savedDataLine = gDataLine;
    DataArray *savedArray = gArray;
    int savedOpenArray = gOpenArray;
#ifdef HX_NATIVE
    int savedNode = gNode;
    char savedHoldChar = yyGetHoldChar();
#endif

    yyrestart(nullptr);
    DataArray *ret = DataReadFile(madePath, b);
    if (b && !ret) {
        MILO_FAIL("Couldn\'t open embedded file: %s (file %s, line %d)", madePath, savedArray->File(), savedArray->Line());
    }
    gBinStream = bs;
    gDataLine = savedDataLine;
    gFile = localfile;
    gArray = savedArray;
    gOpenArray = savedOpenArray;
#ifdef HX_NATIVE
    gNode = savedNode;
#endif

    yyrestart(nullptr);
#ifdef HX_NATIVE
    yySetHoldChar(savedHoldChar);
#endif
    return ret;
}

DataLoader::DataLoader(const FilePath &fp, LoaderPos pos, bool b3)
    : Loader(fp, pos), mFilename(""), mData(nullptr), mFile(nullptr), mBufLen(0),
      mBuffer(nullptr), mDtb(b3), mThreadObj(nullptr) {
    const char *new_str = fp.c_str();
    if (b3) {
        new_str = CachedDataFile(new_str, mDtb);
    }
    mFilename = new_str;
    mState = &DataLoader::OpenFile;
}

DataLoader::~DataLoader() {
    if (mState != &DataLoader::DoneLoading) {
        delete mFile;
        MemFree(mBuffer);
    } else if (mData) {
        mData->Release();
    }
}

bool DataLoader::IsLoaded() const { return mState == &DataLoader::DoneLoading; }
void DataLoader::PollLoading() { (this->*mState)(); }
void DataLoader::DoneLoading() {}

void DataLoader::OpenFile() {
    mFile = NewFile(mFilename.c_str(), 2);
    if (mFile && !mFile->Fail()) {
        mBufLen = mFile->Size();
        mBuffer = (char *)MemAlloc(mBufLen, __FILE__, 0x366, "Resource");
        mFile->ReadAsync(mBuffer, mBufLen);
        mState = &DataLoader::LoadFile;
    } else {
        if (!mFilename.empty()) {
            MILO_NOTIFY("Could not load: %s", FileLocalize(LoaderFile().c_str(), nullptr));
        }
        mState = &DataLoader::DoneLoading;
    }
}

DataArray *DataLoader::Data() {
    MILO_ASSERT(IsLoaded(), 0x3A5);
    return mData;
}

void DataLoader::ThreadDone(DataArray *arr) {
    MILO_ASSERT(MainThread(), 0x3B5);
    mData = arr;
    RELEASE(mThreadObj);
    if (mBuffer) {
        MemFree(mBuffer, __FILE__, 0x3BA);
        mBuffer = nullptr;
    }
    RELEASE(mFile);
    mState = &DataLoader::DoneLoading;
}

void DataLoader::LoadFile() {
    if (mThreadObj) {
        Timer::Sleep(0);
        TheLoadMgr.SetCurrentPeriod(0.0f);
    } else {
        int x;
        if (mFile->ReadDone(x)) {
            if (mFile->Fail()) {
                ThreadDone(nullptr);
            } else {
                mThreadObj = new DataLoaderThreadObj(
                    this, mFile, (char *)mBuffer, mBufLen, mDtb, mFilename.c_str()
                );
                ThreadCall(mThreadObj);
            }
        }
    }
}

#pragma endregion
#pragma region DataLoaderThreadObj

DataLoaderThreadObj::DataLoaderThreadObj(
    DataLoader *dl, File *file, char *buffer, int bufSize, bool dtb, const char *filename
)
    : mLoader(dl), mResult(nullptr), mFile(file), mBufLen(bufSize), mBuffer(buffer),
      mFilename(filename), mDtb(dtb), mLocal(FileIsLocal(filename)) {}

int DataLoaderThreadObj::ThreadStart() {
    BufStream bs(mBuffer, mBufLen, true);
    bs.SetName(FileLocalize(mLoader->LoaderFile().c_str(), nullptr));
    if (mDtb) {
        bool runChecksum = HasFileChecksumData() && !mLocal;
        if (runChecksum) {
            bs.StartChecksum(mFilename);
        }
        mResult = ReadCacheStream(bs, mLoader->LoaderFile().c_str());
        if (runChecksum) {
            bs.ValidateChecksum();
        }
    } else {
        mResult = DataReadStream(&bs);
    }
    return 0;
}

void DataLoaderThreadObj::ThreadDone(int) { mLoader->ThreadDone(mResult); }
