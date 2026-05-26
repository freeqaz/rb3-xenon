#pragma once

class MicClientID {
public:
    MicClientID(int i = -1, int j = -1) : mClientID(i), mPlayerID(j) {}
    MicClientID(const MicClientID &id) : mClientID(id.mClientID), mPlayerID(id.mPlayerID) {}

    MicClientID &operator=(const MicClientID &id) {
        if (this != &id) {
            mClientID = id.mClientID;
            mPlayerID = id.mPlayerID;
        }
        return *this;
    }
    int mClientID; // mic id - 0, 1 or 2
    int mPlayerID; // 0x4
};

class MicManagerInterface {
public:
    MicManagerInterface() {}
    virtual ~MicManagerInterface() {}
    virtual void HandleMicsChanged() = 0;
    virtual void SetPlayback(bool) = 0;
    virtual float GetEnergyForMic(const MicClientID &) = 0;
};
