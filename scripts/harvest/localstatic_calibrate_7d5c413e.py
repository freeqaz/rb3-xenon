import sys, os, subprocess
sys.path.insert(0,'scripts/harvest')
import localstatic_patch_gen as G
wt='.'
img=G.Image(os.path.join(wt,'orig/45410914/band.exe'))
_t,rev=G.load_target_map(wt); tell=G.build_tellname(rev)
# ground truth: what commit 7d5c413e ADDED (identifier, string) per function
GT = {
 ('band3/meta_band/NewAwardPanel','?PopAndShowFirstAward@NewAwardPanel@@QAAXXZ'):
   [('Message','handle_continue'),('Message','update_provider'),('Message','update_all')],
 ('BandStarDisplay','?SyncProperty@BandStarDisplay@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z'):
   [('Symbol','num_stars'),('Symbol','star_type')],
 ('PatchDir','?SyncProperty@PatchLayer@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z'):
   [('Symbol','sticker_category'),('Symbol','sticker_idx'),('Symbol','color_idx')],
 ('TrackPanelDirBase','?SyncProperty@TrackPanelDirBase@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z'):
   [('Symbol','view_time_easy'),('Symbol','view_time_expert'),('Symbol','net_track_alpha'),('Symbol','configuration'),('Symbol','configurable_objects')],
 ('band3/meta_band/SigninScreen','?Enter@SigninScreen@@UAAXPAVUIScreen@@@Z'):
   [('Symbol','limit_user_signin'),('Symbol','must_not_be_a_guest'),('Symbol','must_be_online'),('Symbol','must_be_multiplayer_capable'),('Symbol','handle_sign_outs')],
 ('band3/meta_band/SigninScreen','?GetUser@SigninScreen@@QAAPAVLocalBandUser@@XZ'):
   [('Symbol','signing_in_user')],
 ('band3/meta_band/SigninScreen','?ReEvaluateState@SigninScreen@@QAAXXZ'):
   [('Message','on_signed_in')],
 ('band3/meta_band/SigninScreen','?OnMsg@SigninScreen@@QAA?AVDataNode@@ABVUIChangedMsg@@@Z'):
   [('Message','on_signed_out')],
 ('system/bandobj/BandTrack','?EnterCoda@BandTrack@@UAAXXZ'):[('Message','reset')],
 ('system/bandobj/BandTrack','?PlayIntro@BandTrack@@UAAXXZ'):
   [('Message','intro'),('Message','intro_remote')],
 ('system/bandobj/BandTrack','?SavePlayer@BandTrack@@UAAXXZ'):
   [('Message','icon_hide'),('Message','saved')],
 ('band3/game/RGTrainerPanel','?InitFretSteps@RGTrainerPanel@@QAAXABVGameGem@@@Z'):
   [('Symbol','rg_chordbook_left_hand_doesnt_matter'),('Symbol','rg_chordbook_step_strum'),('Symbol','rg_chordbook_step_strum')],
 ('band3/game/RGTrainerPanel','?NewDifficulty@RGTrainerPanel@@UAAXHH@Z'):[('Message','end_chord_legend_no_rollback')],
 ('band3/meta_band/SongSelectPanel','?ResultFailure@SongSelectPanel@@UAAXXZ'):[('Message','lb_failure')],
 # partial climbers
 ('ChordbookPanel','?SetFret@ChordbookPanel@@QAAXHH@Z'):
   [('Message','set_finger_fret'),('Message','play_correct_fret')],
 ('band3/bandtrack/TrackPanel','?Poll@TrackPanel@@UAAXXZ'):[('Message','hide')],
 ('band3/meta_band/SigninScreen','?OnMsg@SigninScreen@@QAA?AVDataNode@@ABVSigninChangedMsg@@@Z'):
   [('Message','on_signed_in'),('Message','on_not_multiplayer_capable'),('Message','on_not_online'),('Message','on_signed_into_guest')],
 ('BandSongMetadata','?HasPart@BandSongMetadata@@UBA_NVSymbol@@_N@Z'):
   [('Symbol','real_guitar'),('Symbol','real_bass')],
}
ok=bad=0
for (unit,sym),want in GT.items():
    p=os.path.join(wt,'build/45410914/obj',unit+'.obj')
    got=G.scan_obj(p,img,tell,{sym})
    if sym not in got: print("MISS-OBJ",unit,sym); bad+=1; continue
    size,sites=got[sym][0],got[sym][1]
    have=[(s['kind'],s['string']) for s in sites if s['form']=='LOCAL_STATIC']
    if have==want: ok+=1; print("OK   %-30s %s"%(unit,sym[:52]))
    else:
        bad+=1
        print("DIFF %-30s %s\n     want=%s\n     got =%s"%(unit,sym[:52],want,have))
print("\n%d/%d exact"%(ok,ok+bad))
