#include <MarioKartWii/System/Ghost.hpp>
#include <MarioKartWii/UI/Ctrl/GhostInfoControl.hpp>
#include <MarioKartWii/UI/Ctrl/Animation.hpp>
#include <MarioKartWii/UI/Text/Text.hpp>
#include <MarioKartWii/Mii/MiiGroup.hpp>
#include <UI/CustomVehicles/CustomVehicles.hpp>
#include <UI/UI.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>

const char* GetCharacterIconPaneName(CharacterId characterId);

namespace Pulsar {
namespace UI {

static void UpdateInfoPlaystyle(GhostInfoControl* control, GhostData* data, u32 isNew) {
    LayoutUIControl* base = (LayoutUIControl*)control;

    if (!data->isValid) {
        control->miiGroup.DeleteMii(0);
        base->SetPaneVisibility("nationality", false);
        base->SetPaneVisibility("nintendo", false);
        nw4r::lyt::Pane* pane = base->layout.GetPaneByName("chara");
        pane->flag &= 0xFE;
        pane = base->layout.GetPaneByName("machine");
        pane->flag &= 0xFE;
        AnimationGroup& group2 = base->animator.GetAnimationGroupById(2);
        group2.PlayAnimationAtFrame(1, 0.0f);
        return;
    }

    base->SetTextBoxMessage("course_name", GetCurTrackBMG());

    Text::Info timeInfo;
    memset(&timeInfo, 0, sizeof(timeInfo));
    u32 minutes = (u16)data->finishTime.minutes;
    timeInfo.intToPass[0] = 99;
    if (minutes < 100) {
        timeInfo.intToPass[0] = minutes;
    }
    timeInfo.intToPass[0] = timeInfo.intToPass[0] & 0xFF;
    if (minutes < 100) {
        timeInfo.intToPass[1] = (int)data->finishTime.seconds;
    } else {
        timeInfo.intToPass[1] = 0x3B;
    }
    if (minutes < 100) {
        timeInfo.intToPass[2] = (int)data->finishTime.milliseconds;
    } else {
        timeInfo.intToPass[2] = 999;
    }
    base->SetTextBoxMessage("time", 0x17A4, &timeInfo);

    control->miiGroup.LoadMii(0, &data->miiData);
    Text::Info playerInfo;
    memset(&playerInfo, 0, sizeof(playerInfo));
    playerInfo.miis[0] = control->miiGroup.GetMii(0);
    base->SetTextBoxMessage("player", 0x251D, &playerInfo);

    if (data->type == EASY_STAFF_GHOST || data->type == EXPERT_STAFF_GHOST) {
        base->SetPaneVisibility("nationality", false);
        base->SetPaneVisibility("nintendo", true);
    } else {
        char flagBuf[8];
        snprintf(flagBuf, 7, "%03d", data->nationality >> 24);
        if (!base->PicturePaneExists(flagBuf)) {
            base->SetPaneVisibility("nationality", false);
        } else {
            base->SetPaneVisibility("nationality", true);
            base->SetPicturePane("nationality", flagBuf);
        }

        u8 playstyle = (u8)(data->userData[0] >> 8);
        if (playstyle < STYLE_COUNT) {
            u32 bmgId = BMG_PLAYSTYLE_NAMES + data->kartId * STYLE_COUNT + playstyle;
            base->SetTextBoxMessage("nintendo", bmgId);
            base->SetPaneVisibility("nintendo", true);
        } else {
            base->SetPaneVisibility("nintendo", false);
        }
    }

    const char* charaPane = GetCharacterIconPaneName(data->characterId);
    base->SetPicturePane("chara", charaPane);
    nw4r::lyt::Pane* pane = base->layout.GetPaneByName("chara");
    pane->flag = (pane->flag & 0xFE) | 1;

    char vehicleBuf[16];
    snprintf(vehicleBuf, 15, "Vehicle%02d", data->kartId);
    base->SetPicturePane("machine", vehicleBuf);

    base->SetPaneVisibility("handle", data->controllerType == 0);

    u32 animIdx = !isNew;
    AnimationGroup& group2 = base->animator.GetAnimationGroupById(2);
    group2.PlayAnimationAtFrame(animIdx, 0.0f);
}

kmBranch(0x805e296c, UpdateInfoPlaystyle);

}//namespace UI
}//namespace Pulsar
