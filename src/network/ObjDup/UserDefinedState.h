#pragma once
#include "ObjDup/DataSet.h"

namespace Quazal {
    class UserDefinedState : public DataSet {
    public:
        UserDefinedState();

        unsigned int m_uiUserDefinedState; // 0x0
    };
}
