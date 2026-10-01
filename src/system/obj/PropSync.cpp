#include "obj/PropSync.h"
#include "math/Color.h"
#include "math/Geo.h"
#include "math/Mtx.h"
#include "math/Rot.h"
#include "math/Sphere.h"
#include "obj/Data.h"
#include "os/Debug.h"
#include "os/File.h"
#include "utl/FilePath.h"

bool PropSync(class String &str, DataNode &node, DataArray *prop, int i, PropOp op) {
    MILO_ASSERT(i == prop->Size() && (op & (kPropSet|kPropGet|kPropInsert)), 0x12);
    if (op == kPropGet)
        node = str.c_str();
    else
        str = node.Str();
    return true;
}

bool PropSync(FilePath &fp, DataNode &node, DataArray *prop, int i, PropOp op) {
    MILO_ASSERT(i == prop->Size() && (op & (kPropSet|kPropGet|kPropInsert)), 0x1C);
    if (op == kPropGet)
        node = FileRelativePath(FilePath::Root().c_str(), fp.c_str());
    else {
        const char *str = node.Str();
        fp.Set(FilePath::Root().c_str(), str);
    }
    return true;
}

bool PropSync(Hmx::Color &color, DataNode &node, DataArray *prop, int i, PropOp op) {
    MILO_ASSERT(i == prop->Size() && (op & (kPropSet|kPropGet|kPropInsert)), 0x26);
    if (op == kPropGet)
        node = (int)color.Pack();
    else
        color.Unpack(node.Int());
    return true;
}

bool PropSync(Vector2 &vec, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        {
            static Symbol x("x");
            if (sym == x) {
                return PropSync(vec.x, node, prop, i + 1, op);
            }
        }
        {
            static Symbol y("y");
            if (sym == y) {
                return PropSync(vec.y, node, prop, i + 1, op);
            }
        }
        return false;
    }
}

bool PropSync(Vector3 &vec, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        {
            static Symbol x("x");
            if (sym == x) {
                return PropSync(vec.x, node, prop, i + 1, op);
            }
        }
        {
            static Symbol y("y");
            if (sym == y) {
                return PropSync(vec.y, node, prop, i + 1, op);
            }
        }
        {
            static Symbol z("z");
            if (sym == z) {
                return PropSync(vec.z, node, prop, i + 1, op);
            }
        }
        return false;
    }
}

bool PropSync(Hmx::Matrix3 &_m, DataNode &_val, DataArray *_prop, int _i, PropOp _op) {
    // Retail calls MakeEulerScale once per property (6 sites) and only rebuilds
    // the matrix after a successful non-get sync.
    MILO_ASSERT(_i == _prop->Size() - 1 && (_op & (kPropSet|kPropGet|kPropInsert)), 0x4F);
    Symbol sym = _prop->Sym(_i);
    Vector3 euler, scale;
    bool ret = false;
    {
        static Symbol pitch("pitch");
        if (sym == pitch) {
            MakeEulerScale(_m, euler, scale);
            Scale(euler, RAD2DEG, euler);
            ret = PropSync(euler.x, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol roll("roll");
        if (sym == roll) {
            MakeEulerScale(_m, euler, scale);
            Scale(euler, RAD2DEG, euler);
            ret = PropSync(euler.y, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol yaw("yaw");
        if (sym == yaw) {
            MakeEulerScale(_m, euler, scale);
            Scale(euler, RAD2DEG, euler);
            ret = PropSync(euler.z, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol x_scale("x_scale");
        if (sym == x_scale) {
            MakeEulerScale(_m, euler, scale);
            Scale(euler, RAD2DEG, euler);
            ret = PropSync(scale.x, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol y_scale("y_scale");
        if (sym == y_scale) {
            MakeEulerScale(_m, euler, scale);
            Scale(euler, RAD2DEG, euler);
            ret = PropSync(scale.y, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol z_scale("z_scale");
        if (sym == z_scale) {
            MakeEulerScale(_m, euler, scale);
            Scale(euler, RAD2DEG, euler);
            ret = PropSync(scale.z, _val, _prop, _i + 1, _op);
        }
    }
    if (ret && _op != kPropGet) {
        Scale(euler, DEG2RAD, euler);
        MakeRotMatrix(euler, _m, true);
        Scale(scale, _m, _m);
    }
    return ret;
}

bool PropSync(Transform &tf, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        {
            static Symbol x("x");
            if (sym == x) {
                return PropSync(tf.v.x, node, prop, i + 1, op);
            }
        }
        {
            static Symbol y("y");
            if (sym == y) {
                return PropSync(tf.v.y, node, prop, i + 1, op);
            }
        }
        {
            static Symbol z("z");
            if (sym == z) {
                return PropSync(tf.v.z, node, prop, i + 1, op);
            }
        }
        return PropSync(tf.m, node, prop, i, op) != false;
    }
}

bool PropSync(Sphere &sphere, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        {
            static Symbol x("x");
            if (sym == x) {
                return PropSync(sphere.center.x, node, prop, i + 1, op);
            }
        }
        {
            static Symbol y("y");
            if (sym == y) {
                return PropSync(sphere.center.y, node, prop, i + 1, op);
            }
        }
        {
            static Symbol z("z");
            if (sym == z) {
                return PropSync(sphere.center.z, node, prop, i + 1, op);
            }
        }
        {
            static Symbol radius("radius");
            if (sym == radius) {
                return PropSync(sphere.radius, node, prop, i + 1, op);
            }
        }
        return false;
    }
}

bool PropSync(Hmx::Rect &rect, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        {
            static Symbol x("x");
            if (sym == x) {
                return PropSync(rect.x, node, prop, i + 1, op);
            }
        }
        {
            static Symbol y("y");
            if (sym == y) {
                return PropSync(rect.y, node, prop, i + 1, op);
            }
        }
        {
            static Symbol w("w");
            if (sym == w) {
                return PropSync(rect.w, node, prop, i + 1, op);
            }
        }
        {
            static Symbol h("h");
            if (sym == h) {
                return PropSync(rect.h, node, prop, i + 1, op);
            }
        }
        return false;
    }
}

// Retail 0x8276A438: per-component, with six function-scope static Symbols on
// one guard word, each dispatching to PropSync(float &).
bool PropSync(Box &box, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        static Symbol min_x("min_x");
        if (sym == min_x)
            return PropSync(box.mMin.x, node, prop, i + 1, op);
        static Symbol max_x("max_x");
        if (sym == max_x)
            return PropSync(box.mMax.x, node, prop, i + 1, op);
        static Symbol min_y("min_y");
        if (sym == min_y)
            return PropSync(box.mMin.y, node, prop, i + 1, op);
        static Symbol max_y("max_y");
        if (sym == max_y)
            return PropSync(box.mMax.y, node, prop, i + 1, op);
        static Symbol min_z("min_z");
        if (sym == min_z)
            return PropSync(box.mMin.z, node, prop, i + 1, op);
        static Symbol max_z("max_z");
        if (sym == max_z)
            return PropSync(box.mMax.z, node, prop, i + 1, op);
        return false;
    }
}


// COMDAT-scatter owner-TU includes (sw scatter-scan): retail linker
// interleaved these owners' COMDATs into this TU's .text span.
#define gRev gRev_Dir
#define gAltRev gAltRev_Dir
#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing include
#include "world/Dir.cpp"
#endif
#undef gRev
#undef gAltRev
