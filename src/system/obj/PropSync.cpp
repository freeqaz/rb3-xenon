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
    MILO_ASSERT(_i == _prop->Size() - 1 && (_op & (kPropSet|kPropGet|kPropInsert)), 0x4F);
    Symbol sym = _prop->Sym(_i);
    bool ret = false;
    Vector3 euler, scale;
    {
        static Symbol pitch("pitch");
        if (sym == pitch) {
            MakeEulerScale(_m, euler, scale);
            euler.z *= RAD2DEG;
            euler.x *= RAD2DEG;
            euler.y *= RAD2DEG;
            ret = PropSync(euler.x, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol roll("roll");
        if (sym == roll) {
            MakeEulerScale(_m, euler, scale);
            euler.z *= RAD2DEG;
            euler.x *= RAD2DEG;
            euler.y *= RAD2DEG;
            ret = PropSync(euler.y, _val, _prop, _i + 1, _op);
        }
    }
    {
        static Symbol yaw("yaw");
        if (sym == yaw) {
            MakeEulerScale(_m, euler, scale);
            euler.z *= RAD2DEG;
            euler.x *= RAD2DEG;
            euler.y *= RAD2DEG;
            ret = PropSync(euler.z, _val, _prop, _i + 1, _op);
        }
    }
    if (!ret || _op == kPropGet) {
        {
            static Symbol x_scale("x_scale");
            if (sym == x_scale) {
                float len = Length(_m.x);
                float oldLen = len;
                ret = PropSync(len, _val, _prop, _i + 1, _op);
                if (_op != kPropGet) {
                    if (oldLen == 0.0f) {
                        _m.Identity();
                        oldLen = 1.0f;
                    }
                    float ratio = len / oldLen;
                    _m.x.x *= ratio;
                    _m.x.y *= ratio;
                    _m.x.z *= ratio;
                }
            }
        }
        {
            static Symbol y_scale("y_scale");
            if (sym == y_scale) {
                float len = Length(_m.y);
                float oldLen = len;
                ret = PropSync(len, _val, _prop, _i + 1, _op);
                if (_op != kPropGet) {
                    if (oldLen == 0.0f) {
                        _m.Identity();
                        oldLen = 1.0f;
                    }
                    float ratio = len / oldLen;
                    Scale(_m.y, ratio, _m.y);
                }
            }
        }
        {
            static Symbol z_scale("z_scale");
            if (sym == z_scale) {
                float len = Length(_m.z);
                float oldLen = len;
                ret = PropSync(len, _val, _prop, _i + 1, _op);
                if (_op != kPropGet) {
                    if (oldLen == 0.0f) {
                        _m.Identity();
                        oldLen = 1.0f;
                    }
                    float ratio = len / oldLen;
                    Scale(_m.z, ratio, _m.z);
                }
            }
        }
    } else {
        euler.x *= DEG2RAD;
        euler.y *= DEG2RAD;
        euler.z *= DEG2RAD;
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

bool PropSync(Box &box, DataNode &node, DataArray *prop, int i, PropOp op) {
    if (i == prop->Size())
        return true;
    else {
        Symbol sym = prop->Sym(i);
        {
            static Symbol min("min");
            if (sym == min) {
                return PropSync(box.mMin, node, prop, i + 1, op);
            }
        }
        {
            static Symbol max("max");
            if (sym == max) {
                return PropSync(box.mMax, node, prop, i + 1, op);
            }
        }
        return false;
    }
}
