#include "game/ChordPreview.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

void ChordPreview::StreamData::Reset(bool releaseFader) {
    RELEASE(stream);
    state = 0;
    if (releaseFader) {
        RELEASE(fader);
    }
}

void ChordPreview::Start(Symbol song) {
    MILO_ASSERT(mGuitarFader && mSilenceFader, 0x65);
    if (song == mSong)
        return;
    if (!song.Null()) {
        if (!TheSongMgr.HasSong(song, true))
            return;
        BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(
            TheSongMgr.GetSongIDFromShortName(song, true)
        );
        if (data && !data->IsVersionOK()) {
            song = gNullStr;
        }
        if (!mRegisteredWithCM) {
            TheContentMgr.RegisterCallback(this, false);
            mRegisteredWithCM = true;
        }
    }
    mSong = song;
    mGuitarFader->SetVal(0);
    for (int i = 0; i < 3; i++) {
        StreamData &data = mStreamData[i];
        MILO_ASSERT(data.fader, 0x84);
        switch (data.state) {
        case 0:
        case 1:
            data.Reset(false);
            break;
        case 2:
            data.state = 3;
            break;
        case 5:
            data.fader->DoFade(-48.0f, mFadeMs);
            data.state = 7;
            break;
        default:
            break;
        }
    }
}
