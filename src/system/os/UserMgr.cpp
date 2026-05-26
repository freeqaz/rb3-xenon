#include "os/UserMgr.h"
#include "Joypad.h"
#include "obj/Dir.h"

UserMgr *TheUserMgr;

void SetTheUserMgr(UserMgr *mgr) {
    TheUserMgr = mgr;
    JoypadReset();
}

UserMgr::UserMgr() : unk2c(true) { SetName("user_mgr", ObjectDir::Main()); }

void UserMgr::GetLocalUsers(std::vector<LocalUser *> &users) const {
    std::vector<User *> allUsers;
    GetUsers(allUsers);
    for (int i = 0; i < allUsers.size(); i++) {
        if (allUsers[i]->IsLocal()) {
            users.push_back(allUsers[i]->GetLocalUser());
        }
    }
}

LocalUser *UserMgr::GetLocalUserFromPadNum(int padnum) const {
    return JoypadGetUserFromPadNum(padnum);
}

BEGIN_HANDLERS(UserMgr)
    HANDLE_EXPR(get_user_from_pad_num, JoypadGetUserFromPadNum(_msg->Int(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS
