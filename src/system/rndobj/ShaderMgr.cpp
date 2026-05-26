#include "rndobj/ShaderMgr.h"
#include "Shader.h"
#include "macros.h"
#include "math/Mtx.h"
#include "obj/Data.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/File.h"
#include "os/Platform.h"
#include "os/System.h"
#include "rndobj/ShaderOptions.h"
#include "rndobj/ShaderProgram.h"
#include "rndobj/Utl.h"
#include "utl/FileStream.h"
#include "utl/Loader.h"
#include "utl/MemMgr.h"

RndShaderMgr::RndShaderMgr()
    : mShaderPoolCount(0), mShaderPoolAlloc(0), mConstantCache(0), mConstantCacheSize(0), mPreInitialized(0),
      mShowShaderErrors(1), mShowMetaMatErrors(0) {}

void RndShaderMgr::PreInit() {
    if (!mPreInitialized) {
        mUseAO = 0;
        mPreInitialized = true;
        mBoneCount = 0;
        unk14 = 1;
        mInDepthVolume = 0;
        unk1c = 0;
        mCullModeOverride = 0;
        unk24 = 0;
        unk25 = 0;
        unk26 = 0;
        unk27 = 0;
        unk28 = 0;
        unk29 = 0;
        unk2b = 0;
        unk2c = 0;
        unk2d = 0;
        unk2e = 0;
        unk2f = 0;
        unk30 = 0;
        unk31 = 0;
        unk34 = 0;
        unk38 = 0;
        unk39 = 0;
        unk3a = 0;
        unk2a = 0;
        unk3b = 0;
        unk3c = 0;
        unk3d = 0;
        unk3e = 0;
        unk3f = 0;
        mAllowPerPixel = 1;
        unk41 = 1;
        mDisplayShaderError = true;
        RELEASE(mWorkMat);
        RELEASE(mPostProcMat);
        RELEASE(mDrawHighlightMat);
        RELEASE(mDrawRectMat);
        mWorkMat = Hmx::Object::New<RndMat>();
        mPostProcMat = Hmx::Object::New<RndMat>();
        mDrawHighlightMat = Hmx::Object::New<RndMat>();
        mDrawRectMat = Hmx::Object::New<RndMat>();
        CreateAndSetMetaMat(mWorkMat);
        CreateAndSetMetaMat(mPostProcMat);
        CreateAndSetMetaMat(mDrawHighlightMat);
        CreateAndSetMetaMat(mDrawRectMat);
        MILO_ASSERT(mConstantCache == NULL, 104);
        mConstantCacheSize = 516;
        {
            MemTemp tmp;
            mConstantCache = new float[mConstantCacheSize];
        }
        LoadShaders("%s_preinit_shaders");
    }
}

void RndShaderMgr::Init() {
    PreInit();
    LoadShaders("%s_shaders");
}

void RndShaderMgr::Terminate() {
    Invalidate(kMaxShaderTypes);
    RELEASE(mConstantCache);
    mConstantCacheSize = 0;
}

void RndShaderMgr::UpdateCache(const Transform &xfm, int idx) {
    // Cache pointer - accessing mConstantCache[idx * 12]
    float *p = &mConstantCache[idx * 12];

    // Load transform components - declaration order affects register allocation
    float xz = xfm.m.x.z;
    float yx = xfm.m.y.x;
    float zx = xfm.m.z.x;
    float yz = xfm.m.y.z;
    float xy = xfm.m.x.y;
    float tz = xfm.v.z;
    float xx = xfm.m.x.x;
    float zy = xfm.m.z.y;
    float ty = xfm.v.y;
    float yy = xfm.m.y.y;
    float tx = xfm.v.x;
    float zz = xfm.m.z.z;

    // Store in column-major order for GPU shader constants (transpose)
    p[0] = xx;
    p[1] = yx;
    p[2] = zx;
    p[3] = tx;
    p[4] = xy;
    p[5] = yy;
    p[6] = zy;
    p[7] = ty;
    p[8] = xz;
    p[9] = yz;
    p[10] = zz;
    p[11] = tz;
}

void RndShaderMgr::ShaderPoolAlloc(int i) { mShaderPoolAlloc = i; }

void RndShaderMgr::SetMeshInfo(int i, bool b) {
    mBoneCount = i;
    mUseAO = b;
}

void RndShaderMgr::SetShaderErrorDisplay(bool disp) { mDisplayShaderError = disp; }
bool RndShaderMgr::GetShaderErrorDisplay() { return mDisplayShaderError; }

unsigned long RndShaderMgr::InitShaders() {
    if (UsingCD() || GetGfxMode() == kOldGfx)
        mCacheShaders = false;
    else {
        DataArray *cfg = SystemConfig("rnd", "cache_shaders");
        mCacheShaders = cfg->Int(1);
    }
    RndShader::Init();
    return RndShaderProgram::InitModTime();
}

void RndShaderMgr::LoadShaders(const char *cc) {
    unsigned long shaders = InitShaders();
    if (TheLoadMgr.GetPlatform() != kPlatformNone) {
        String str(MakeString(cc, PlatformSymbol(TheLoadMgr.GetPlatform())));
        FileStat stat;
        if (!mCacheShaders || !FileGetStat(str.c_str(), &stat) && stat.st_mtime > shaders || strstr(cc, "preinit")) {
                FileStream stream(str.c_str(), FileStream::kRead, true);
                if (!stream.Fail()) {
                    // this check is made somewhere in here according to the asm
                    // TheLoadMgr.GetPlatform() == kPlatformXBox;
                    LoadShaderFile(stream);
                } else {
                    if (UsingCD() && GetGfxMode() == kNewGfx) {
                        MILO_NOTIFY("Can't load shader file %s!!", str.c_str());
                    }
                }
            }
    }
}

void RndShaderMgr::SetTransform(const Transform &xfm) {
    mBoneCount = 0;
    SetVConstant4x3(kVS_WorldTransform, Hmx::Matrix4(xfm));
}

void RndShaderMgr::Invalidate(ShaderType t) {
    for (std::list<ShaderTree>::iterator it = mShaderTrees.begin();
         it != mShaderTrees.end();) {
        if (it == mShaderTrees.begin() && it->shaderType != t) {
            ++it;
        } else {
            delete it->obj;
            it = mShaderTrees.erase(it);
        }
    }
    RndShaderProgram::InitModTime();
}

void RndShaderMgr::LoadShaderFile(FileStream &fs) {
    if (TheLoadMgr.GetPlatform() == kPlatformPS3) {
        RndSplasherResume();
        unsigned int fileType, fileVersion;
        fs >> fileType;
        fs >> fileVersion;
        MILO_ASSERT(fileType == PS3_SHADERS_TYPE, 0xBF);
        MILO_ASSERT(fileVersion == PS3_SHADERS_VERSION, 0xC0);
        RndSplasherSuspend();
    }
    int num;
    fs >> num;
    while (num--) {
        Symbol name;
        fs >> name;
        ShaderType shaderType = ShaderTypeFromName(name.Str());
        int alloc;
        fs >> alloc;
        mShaderPoolAlloc = alloc;
        while (alloc--) {
            u64 u50;
            fs >> u50;
            RndShaderProgram &program = FindShader(shaderType, ShaderOptions(u50));
            int i6c;
            fs >> i6c;
            RndShaderBuffer *buf1;
            program.LoadShaderBuffer(fs, i6c, buf1);
            fs >> i6c;
            RndShaderBuffer *buf2;
            program.LoadShaderBuffer(fs, i6c, buf2);
            program.Cache(shaderType, ShaderOptions(u50), buf1, buf2);
            delete buf1;
            delete buf2;
            RndSplasherPoll();
        }
    }
}

void *RndShaderMgr::AllocShader() {
    if (mShaderPoolCount == 0 && mShaderPoolAlloc > 0) {
        mShaderPoolCount = mShaderPoolAlloc;
        mShaderPoolAlloc = 0;
        mShaderPool = MemAlloc(mShaderSize * mShaderPoolCount, __FILE__, 0x11c, "ShaderPool");
    }
    if (mShaderPoolCount <= 0) {
        if (UsingCD()) {
            MILO_NOTIFY_ONCE("Shader Pool is allocating dynamically");
        }
        mShaderPoolAlloc = 0;
        mShaderPoolCount = 0x100;
        mShaderPool = MemAlloc(mShaderSize << 8, __FILE__, 0x127, "ShaderPool");
    }
    MILO_ASSERT(mShaderPoolCount-- > 0, 0x12A);
    // increment mShaderPool by mShaderSize
    void *old = mShaderPool;
    char *pool = (char *)mShaderPool;
    pool += mShaderSize;
    mShaderPool = pool;
    mShaderPoolAlloc--;
    return old;
}

RndShaderProgram &RndShaderMgr::FindShader(ShaderType t, const ShaderOptions &opts) {
    u64 flags = opts.flags;
    std::list<ShaderTree>::iterator it;

    for (it = mShaderTrees.begin(); it != mShaderTrees.end(); ++it) {
        if (it->shaderType == t) {
            break;
        }
    }

    if (it == mShaderTrees.end()) {
        ShaderTree tree;
        tree.shaderType = t;
        RndShaderProgram *p = NewShaderProgram();
        p->mFlags = flags;
        tree.obj = p;
        if (t == kStandardShader) {
            mShaderTrees.push_front(tree);
        } else {
            mShaderTrees.push_back(tree);
        }
        return *p;
    }

    // Found the tree, now binary search in it
    RndShaderProgram *node = it->obj;
    while (true) {
        if (flags < node->mFlags) {
            RndShaderProgram *left = (RndShaderProgram *)node->unk10;
            if (left == NULL) {
                RndShaderProgram *newNode = NewShaderProgram();
                node->unk10 = (Hmx::Object *)newNode;
                newNode->mFlags = flags;
                return *newNode;
            }
            node = left;
        } else if (flags > node->mFlags) {
            RndShaderProgram *right = (RndShaderProgram *)node->unk14;
            if (right == NULL) {
                RndShaderProgram *newNode = NewShaderProgram();
                node->unk14 = (Hmx::Object *)newNode;
                newNode->mFlags = flags;
                return *newNode;
            }
            node = right;
        } else {
            return *node;
        }
    }
}
