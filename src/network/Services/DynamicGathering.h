#pragma once
#include "Platform/String.h"
#include "Plugins/Buffer.h"
#include "Services/DynamicGatheringDDL.h"

namespace Quazal {
    class DynamicGathering : public _DDL_DynamicGathering {
    public:
        DynamicGathering() : m_strClass("DynamicGathering") {}
        virtual ~DynamicGathering();

        String m_strClass; // 0x3c
    };
}
