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

// Retail ctor (0x8246bcc8, 68 B; called from DxShaderMgr's global initializer)
// stores only 0x58/0x5c/0x64/0x68/0x6d -- it never touches mShowShaderErrors
// or mShowMetaMatErrors, which DC3 initializes here.
RndShaderMgr::RndShaderMgr()
    : mShaderPoolCount(0), mShaderPoolAlloc(0), mConstantCache(0), mConstantCacheSize(0), mPreInitialized(0) {}

// Retail RB3 (0x8246b9a0, 256 B) differs from DC3's body in four ways, all read
// off retail bytes: the fields are stored in address order (unk2a between
// unk29 and unk2b), mDisplayShaderError (0x42) is cleared rather than set,
// there are no RELEASEs of the four mats (and no CreateAndSetMetaMat, which is
// DC3-era), and the constant cache is a plain operator new[] with no temp-
// allocation scope.
void RndShaderMgr::PreInit() {
    if (!mPreInitialized) {
        mUseAO = 0;
        mBoneCount = 0;
        mPreInitialized = true;
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
        unk2a = 0;
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
        unk3b = 0;
        unk3c = 0;
        unk3d = 0;
        unk3e = 0;
        unk3f = 0;
        mAllowPerPixel = 1;
        unk41 = 1;
        mDisplayShaderError = false;
        mWorkMat = Hmx::Object::New<RndMat>();
        mPostProcMat = Hmx::Object::New<RndMat>();
        mDrawHighlightMat = Hmx::Object::New<RndMat>();
        mDrawRectMat = Hmx::Object::New<RndMat>();
        MILO_ASSERT(mConstantCache == NULL, 104);
        mConstantCacheSize = 516;
        mConstantCache = new float[mConstantCacheSize];
        LoadShaders("%s_preinit_shaders");
    }
}

void RndShaderMgr::Init() {
    PreInit();
    RndShaderMgr::LoadShaders("%s_shaders");
}

void RndShaderMgr::Terminate() {
    Invalidate(kMaxShaderTypes);
    RELEASE(mConstantCache);
    mConstantCacheSize = 0;
}

void RndShaderMgr::UpdateCache(const Transform &xfm, int idx) {
    float t[12] = { xfm.m.x.x, xfm.m.y.x, xfm.m.z.x, xfm.v.x,
                    xfm.m.x.y, xfm.m.y.y, xfm.m.z.y, xfm.v.y,
                    xfm.m.x.z, xfm.m.y.z, xfm.m.z.z, xfm.v.z };
    float *p = &mConstantCache[idx * 12];
    p[0] = t[0];
    p[1] = t[1];
    p[2] = t[2];
    p[3] = t[3];
    p[4] = t[4];
    p[5] = t[5];
    p[6] = t[6];
    p[7] = t[7];
    p[8] = t[8];
    p[9] = t[9];
    p[10] = t[10];
    p[11] = t[11];
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
    mCacheShaders = false;
    RndShader::Init();
    unsigned long shaders = RndShaderProgram::InitModTime();
    String str(MakeString(cc, PlatformSymbol(kPlatformXBox)));
    FileStat stat;
    if (!mCacheShaders || !FileGetStat(str.c_str(), &stat) && stat.st_mtime > shaders || strstr(cc, "preinit")) {
            FileStream stream(str.c_str(), FileStream::kRead, true);
            if (!stream.Fail()) {
                LoadShaderFile(stream);
            }
        }
}

void RndShaderMgr::SetTransform(const Transform &xfm) {
    mBoneCount = 0;
    SetVConstant4x3(kVS_WorldTransform, Hmx::Matrix4(xfm));
}

void RndShaderMgr::Invalidate(ShaderType t) {
    bool all = t == kMaxShaderTypes;
    for (std::list<ShaderTree>::iterator it = mShaderTrees.begin();
         it != mShaderTrees.end();) {
        if (!all && it->shaderType != t) {
            ++it;
        } else {
            delete it->obj;
            it = mShaderTrees.erase(it);
        }
    }
    RndShaderProgram::InitModTime();
}

void RndShaderMgr::LoadShaderFile(FileStream &fs) {
    // retail Xbox 360 build has no PS3-platform branch here at all (verified
    // against dtk target asm at fn_8246BD70 -- no TheLoadMgr/GetPlatform
    // check, no RndSplasherResume/Suspend, no fileType/fileVersion reads).
    // dc3-decomp (also Xbox-only) carries the same dead PS3 block and shows
    // the identical ~81% mismatch shape there, so this isn't RB3-specific
    // drift -- it's inherited dead code that never applied to either title's
    // shipped platform.
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

// Retail (0x8246b680, 192 B): no UsingCD()/"allocating dynamically" notify
// branch, and the three members are stored count, alloc, pool.
void *RndShaderMgr::AllocShader() {
    if (mShaderPoolCount == 0 && mShaderPoolAlloc > 0) {
        mShaderPoolCount = mShaderPoolAlloc;
        mShaderPoolAlloc = 0;
        mShaderPool = MemAlloc(mShaderSize * mShaderPoolCount, __FILE__, 0x11c, "ShaderPool");
    }
    if (mShaderPoolCount <= 0) {
        mShaderPoolAlloc = 0;
        mShaderPoolCount = 0x100;
        mShaderPool = MemAlloc(mShaderSize << 8, __FILE__, 0x127, "ShaderPool");
    }
    MILO_ASSERT(mShaderPoolCount-- > 0, 0x12A);
    mShaderPoolAlloc--;
    void *old = mShaderPool;
    mShaderPool = (char *)old + mShaderSize;
    return old;
}

// Retail shape (0x8246bbc8): the found-path is entered straight from the
// type test inside the list walk, and falling off the list goes to the
// not-found path, which picks begin()/end() and makes ONE list::insert call.
RndShaderProgram &RndShaderMgr::FindShader(ShaderType t, const ShaderOptions &opts) {
    u64 flags = opts.flags;
    for (std::list<ShaderTree>::iterator it = mShaderTrees.begin(); it != mShaderTrees.end();
         ++it) {
        if (it->shaderType == t) {
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
    }
    ShaderTree tree;
    tree.shaderType = t;
    RndShaderProgram *p = NewShaderProgram();
    tree.obj = p;
    p->mFlags = flags;
    if (t == kStandardShader) {
        mShaderTrees.push_front(tree);
    } else {
        mShaderTrees.push_back(tree);
    }
    return *p;
}
