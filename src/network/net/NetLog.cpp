#include "net/NetLog.h"

// Retail link order is SessionSearcher.cpp -> NetLog.cpp -> NetworkEmulator.cpp:
// NetLog's static-init thunk at 0x82c3ec00 constructs 0x82cbfc80 with
// "netlog-%05d.txt" (0x820574e0), and this TU's only .text$mn contribution is
// the ICF survivor of the header-inline LogFile::~LogFile COMDAT at 0x823eb8e0
// (76 B + 40 B EH funclet), which is why that COMDAT sits between the two
// network TUs rather than in system/utl/LogFile.cpp's span (lane W16-AE).
LogFile NetLog = LogFile("netlog-%05d.txt");
