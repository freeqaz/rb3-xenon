#include "char/FileMergerOrganizer.h"
#include "FileMerger.h"
#include "FileMergerOrganizer.h"
#include "hamobj/SongCollision.h"
#include "math/Rand.h"
#include "obj/Object.h"
#include "os/Debug.h"
#include "os/System.h"

FileMergerOrganizer *TheFileMergerOrganizer;
std::map<Symbol, CatData> gCatPriority;
int gNextCatPriority = 1;
bool gOrganizing;
bool gGenderChirality;

bool FileMergerSort::operator()(const FileMerger::Merger *m1, const FileMerger::Merger *m2) {
    CatData &m1data = gCatPriority[m1->mName];
    CatData &m2data = gCatPriority[m2->mName];
    if (m2data.priority == 0) {
        if (gOrganizing) {
            auto _tmp0 = MakeString("unknown file merger organizer category %s", m2->mName);
            TheDebug.Notify(_tmp0);
        }
        m2data.priority = gNextCatPriority++;
        m2data.mInGenderOrder = false;
    }
    if (m1data.priority == 0) {
        if (gOrganizing) {
            TheDebug.Notify(MakeString("unknown file merger organizer category %s", m1->mName));
        }
        m1data.priority = gNextCatPriority++;
        m1data.mInGenderOrder = false;
    }
    CatData aData = m1data;
    CatData bData = m2data;
    if (m1->loading.empty()) {
        aData.priority -= gNextCatPriority;
        MILO_ASSERT(aData.priority < 0, 0x5f);
    }
    if (m2->loading.empty()) {
        bData.priority -= gNextCatPriority;
        MILO_ASSERT(bData.priority < 0, 0x64);
    }
    if (aData.mInGenderOrder && bData.mInGenderOrder) {
        bool female1 = strstr(m1->loading.c_str(), "female") != 0;
        bool female2 = strstr(m2->loading.c_str(), "female") != 0;
        if (female1 != female2) {
            return gGenderChirality ^ (female1 > female2);
        }
    }
    if (aData.priority == bData.priority) {
        return strcmp(m1->loading.c_str(), m2->loading.c_str()) < 0;
    }
    return aData.priority < bData.priority;
}

void FileMergerOrganizerLoader::PollLoading() { TheFileMergerOrganizer->StartLoad(); }

FileMergerOrganizer::FileMergerOrganizer() : mActiveOrg(nullptr), mStartOrg(nullptr) {
    MILO_ASSERT(TheFileMergerOrganizer == NULL, 0x1BE);
    TheFileMergerOrganizer = this;
}

BEGIN_HANDLERS(FileMergerOrganizer)
    HANDLE_ACTION(add_file_merger, AddFileMerger(_msg->Obj<FileMerger>(2)))
    HANDLE_SUPERCLASS(Hmx::Object)
END_HANDLERS

BEGIN_PROPSYNCS(SongCollision)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_PROPSYNCS(FileMergerOrganizer)
    SYNC_SUPERCLASS(Hmx::Object)
END_PROPSYNCS

BEGIN_SAVES(FileMergerOrganizer)
    MILO_FAIL("Can't save TheFileMergerOrganizer");
END_SAVES

BEGIN_COPYS(FileMergerOrganizer)
    MILO_FAIL("Can't copy TheFileMergerOrganizer");
END_COPYS

BEGIN_LOADS(FileMergerOrganizer)
    MILO_FAIL("Can't load TheFileMergerOrganizer");
END_LOADS

FileMerger::Merger *FileMergerOrganizer::FrontInactiveMerger(OrganizedFileMerger *ofm) {
    std::list<FileMerger::Merger *>::iterator it = ofm->merger->mFilesPending.begin();
    if (ofm->merger->mCurLoader)
        it++;
    if (it == ofm->merger->mFilesPending.end())
        return nullptr;
    else
        return *it;
}

void FileMergerOrganizer::FinishLoading(Loader *l) {
    OrganizedFileMerger *org = mActiveOrg;
    MILO_ASSERT(org->merger->mCurLoader == l, 0x144);
    MILO_ASSERT(org->merger->mFilesPending.front()->loading == l->LoaderFile(), 0x145);
    MILO_ASSERT(org->state == kPendingLoad, 0x146);
    org->state = kFinishLoad;
    mActiveOrg = nullptr;
    if (l->LoaderFile().empty()) {
        if (FrontInactiveMerger(org)) {
            Dispatch(org);
        } else
            RemoveFileMerger(org);
    } else
        CheckDone();
}

void FileMergerOrganizer::FailedLoading(Loader *l) {
    OrganizedFileMerger *org = nullptr;
    for (std::list<OrganizedFileMerger>::iterator it = mOrganizedFileMergers.begin();
         it != mOrganizedFileMergers.end();
         ++it) {
        if (it->merger->mCurLoader && it->merger->mCurLoader == l) {
            org = &*it;
            break;
        }
    }
    MILO_ASSERT(org, 0x173);
    MILO_ASSERT(org->merger->mFilesPending.front()->loading == l->LoaderFile(), 0x176);
    MILO_ASSERT(org->merger->mCurLoader == l, 0x177);
    MILO_ASSERT(org->state == kPendingLoad || (org->state == kFinishLoad && mActiveOrg != org), 0x179);
    org->state = kFailedLoad;
    OrganizedFileMerger *oldOrg = mActiveOrg;
    mActiveOrg = nullptr;
    Dispatch(org);
    MILO_ASSERT(org->merger->mCurLoader == NULL, 0x184);
    org->state = (OrganizedState)0;
    if (oldOrg == org)
        oldOrg = nullptr;
    mActiveOrg = oldOrg;
    if (!mActiveOrg && !mStartOrg) {
        FOREACH (it, mOrganizedFileMergers) {
            OrganizedFileMerger *curr = &*it;
            MILO_ASSERT(curr->state != kPendingLoad, 0x193);
        }
        mStartOrg = new FileMergerOrganizerLoader();
    }
}

void FileMergerOrganizer::Init() {
    MILO_ASSERT(gNextCatPriority == 1, 0x19E);
    REGISTER_OBJ_FACTORY(FileMergerOrganizer)
    DataArray *cfg = SystemConfig("file_merger_organizer");
    DataArray *catArr = cfg->FindArray("category_order", false);
    DataArray *genderArr = cfg->FindArray("gender_order", false);
    if (catArr) {
        for (; gNextCatPriority < catArr->Size(); gNextCatPriority++) {
            DataArray *curCatArr = catArr->Array(gNextCatPriority);
            for (int i = 0; i < curCatArr->Size(); i++) {
                Symbol key = curCatArr->Sym(i);
                CatData &val = gCatPriority[key];
                val.priority = gNextCatPriority;
                val.mInGenderOrder = genderArr && genderArr->Contains(key);
            }
            gOrganizing = true;
        }
    }
    FileMergerOrganizer::NewObject(); // ???
}

void FileMergerOrganizer::StartLoad() {
    gGenderChirality = RandomInt() & 1;
    RELEASE(mStartOrg);
    CheckDone();
}

void FileMergerOrganizer::Dispatch(FileMergerOrganizer::OrganizedFileMerger *ofm) {
    MILO_ASSERT(mActiveOrg == NULL, 0xC4);
    mActiveOrg = ofm;
    if (mActiveOrg->state != kFailedLoad) {
        FOREACH (it, mOrganizedFileMergers) {
            OrganizedFileMerger *curr = &*it;
            MILO_ASSERT(curr->state != kPendingLoad, 0xCD);
        }
    }
    if (ofm->state == kFinishLoad) {
        ofm->merger->FinishLoading(ofm->merger->mCurLoader);
    } else if (ofm->state == kFailedLoad) {
        ofm->merger->FailedLoading(ofm->merger->mCurLoader);
    } else if (ofm->state == 0) {
        ofm->merger->LaunchNextLoader();
    } else {
        MILO_FAIL("Unknown dispatch state %d\n", ofm->state);
    }
    ofm->state = kPendingLoad;
}

void FileMergerOrganizer::RemoveFileMerger(FileMergerOrganizer::OrganizedFileMerger *ofm
) {
    MILO_ASSERT(!mActiveOrg, 0x116);
    if (ofm->merger->mCurLoader) {
        Dispatch(ofm);
    }
    if (!ofm->merger->mCurLoader) {
        ofm->merger->mOrganizer = ofm->merger;
        FOREACH (it, mOrganizedFileMergers) {
            if (&*it == ofm) {
                mOrganizedFileMergers.erase(it);
                break;
            }
        }
        mActiveOrg = nullptr;
        CheckDone();
    }
}

void FileMergerOrganizer::CheckDone() {
    MILO_ASSERT(mActiveOrg == NULL, 0x97);
    MILO_ASSERT(mStartOrg == NULL, 0x98);
    OrganizedFileMerger *bestOrg = nullptr;
    FileMerger::Merger *bestFront = nullptr;
    FOREACH (it, mOrganizedFileMergers) {
        OrganizedFileMerger *curr = &*it;
        MILO_ASSERT(curr->state != kPendingLoad, 0xA3);
        FileMerger::Merger *front = FrontInactiveMerger(curr);
        if (!front) {
            RemoveFileMerger(curr);
            return;
        }
        if (!bestOrg || !FileMergerSort()(bestFront, front)) {
            bestOrg = curr;
            bestFront = front;
        }
    }
    if (bestOrg)
        Dispatch(bestOrg);
}

void FileMergerOrganizer::AddFileMerger(FileMerger *fm) {
    if (!gOrganizing)
        fm->LaunchNextLoader();
    else {
        MILO_ASSERT(fm->mOrganizer == fm, 0xFD);
        MILO_ASSERT(fm->mCurLoader == NULL, 0xFE);
        FOREACH (it, mOrganizedFileMergers) {
            OrganizedFileMerger *curr = &*it;
            MILO_ASSERT(curr->merger != fm, 0x102);
        }
        fm->mOrganizer = this;
        OrganizedFileMerger ofm;
        ofm.merger = fm;
        ofm.state = (OrganizedState)0;
        mOrganizedFileMergers.push_back(ofm);
        if (mActiveOrg == 0 && !mStartOrg) {
            mStartOrg = new FileMergerOrganizerLoader();
        }
    }
}
