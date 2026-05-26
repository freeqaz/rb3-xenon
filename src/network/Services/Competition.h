#pragma once
#include "Services/CompetitionDDL.h"
#include "Services/Gathering.h"

namespace Quazal {
    class Competition : public Gathering {
    public:
        Competition();
        virtual ~Competition();
    };
}
