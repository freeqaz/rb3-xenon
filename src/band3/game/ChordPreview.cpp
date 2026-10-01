#include "game/ChordPreview.h"
#include "meta_band/BandSongMetadata.h"
#include "meta_band/BandSongMgr.h"
#include "os/ContentMgr.h"
#include "os/Debug.h"
#include "utl/Symbol.h"

// Fade target for a preview stream being stopped (retail 0x820F395C).
extern const float kChordPreviewSilenceDb = -48.0f;

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
        int songID = TheSongMgr.GetSongIDFromShortName(song, true);
        BandSongMetadata *data = (BandSongMetadata *)TheSongMgr.Data(songID);
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
            data.fader->DoFade(kChordPreviewSilenceDb, mFadeMs);
            data.state = 7;
            break;
        default:
            break;
        }
    }
}
