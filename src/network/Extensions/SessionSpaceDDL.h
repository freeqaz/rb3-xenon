#pragma once

namespace Quazal {
    class SessionSpace;

    class _DUPSPACE_SessionSpace {
    public:
        static const char *GetClassNameString();
        static SessionSpace *GetInstance();
    };
}
