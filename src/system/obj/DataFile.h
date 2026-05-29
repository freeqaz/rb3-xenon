#pragma once
#include "obj/Data.h"
#include "os/ThreadCall.h"
#include "utl/BinStream.h"
#include "utl/Loader.h"

class DataLoader;
typedef void (DataLoader::*DataLoaderStateFunc)(void);

class DataLoader : public Loader {
public:
    DataLoader(const FilePath &, LoaderPos, bool);
    virtual ~DataLoader();
    virtual const char *DebugText() {
        return MakeString("DataLoader: %s", LoaderFile().c_str());
    }
    virtual bool IsLoaded() const;
    virtual const char *StateName() const { return "DataLoader"; }

    void ThreadDone(DataArray *);
    DataArray *Data();

protected:
    virtual void PollLoading();
    virtual void OpenFile();

private:
    void LoadFile();
    void DoneLoading();

    String mFilename; // 0x18
    DataArray *mData; // 0x24
    File *mFile; // 0x28
    int mBufLen; // 0x2C
    char *mBuffer; // 0x30
    bool mDtb; // 0x34
    class DataLoaderThreadObj *mThreadObj; // 0x38
    DataLoaderStateFunc mState; // 0x3C
};

class DataLoaderThreadObj : public ThreadCallback {
public:
    DataLoaderThreadObj(DataLoader *, File *, char *, int, bool, const char *);
    virtual ~DataLoaderThreadObj() {}
    virtual int ThreadStart();
    virtual void ThreadDone(int);

private:
    DataLoader *mLoader; // 0x4
    DataArray *mResult; // 0x8
    File *mFile; // 0xc
    int mBufLen; // 0x10
    char *mBuffer; // 0x14
    const char *mFilename; // 0x18
    bool mDtb; // 0x1c
    bool mLocal; // 0x1d
};

DataArray *DataReadString(const char *);
DataArray *ReadCacheStream(BinStream &, const char *);
DataArray *ReadEmbeddedFile(const char *, bool);
DataArray *DataReadFile(const char *, bool);
DataArray *DataReadStream(BinStream *);
bool ParseNode();
DataArray *ParseArray();
void PushBack(const DataNode &);
void DataWriteFile(const char *, const DataArray *, int);
DataArray *LoadDtz(const char *, int);

void BeginDataRead();
void FinishDataRead();
const char *CachedDataFile(const char *, bool &);
