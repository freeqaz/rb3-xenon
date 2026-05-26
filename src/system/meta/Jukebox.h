#pragma once
#include <vector>

struct JukeboxItem {
    JukeboxItem(int x, int y) : mName(x), mLastPlayed(y) {}
    ~JukeboxItem() {}
    bool operator==(int x) const { return mName == x; }
    int mName;
    int mLastPlayed;
};

class Jukebox {
private:
    void AddItem(int, int);

    std::vector<JukeboxItem> mJukeboxItems; // 0x0
    int mPlayCounter; // 0xc

public:
    Jukebox(int);
    void Play(int);
    int Pick(const std::vector<int> &);
};
