#include "math/FileChecksum.h"
#include "os/File.h"
#include "utl/Std.h"
#include <vector>

std::vector<ChecksumData> gChecksumData;

void SetFileChecksumData(FileChecksum *fc, int i) {
    gChecksumData.push_back(ChecksumData(fc, &fc[i]));
}
void ClearFileChecksumData() { gChecksumData.clear(); }
bool HasFileChecksumData() { return !gChecksumData.empty(); }

const unsigned char *GetFileChecksum(const char *cc, bool b) {
    const char *filename = b ? FileGetName(cc) : FileLocalize(cc, nullptr);
    FOREACH (it, gChecksumData) {
        for (FileChecksum *sum = it->start; sum != it->end; sum++) {
            if (streq(sum->file, filename)) {
                return sum->checksum;
            }
        }
    }
    return nullptr;
}
