#include <kamek.hpp>
#include <AutoTrackSelect/ExpFroomMessages.hpp>
#include <MarioKartWii/UI/Page/Other/Globe.hpp>
#include <Settings/Settings.hpp>
#include <Settings/SettingsParam.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <SlotExpansion/UI/ExpansionUIMisc.hpp>
#include <Gamemodes/OnlineTT/OTTRegional.hpp>

namespace Pulsar {
namespace UI {
bool ExpFroomMessages::isOnModeSelection = false;
s32 ExpFroomMessages::clickedButtonIdx = 0;

// Expand message count from 4 to 5 for the start CTs option
static void OnStartButtonFroomMsgActivate() {
    register ExpFroomMessages *msg;
    asm(mr msg, r31;);
    msg->msgCount = 5;  // 4 normal + 1 start CTs option
}
kmCall(0x805dc480, OnStartButtonFroomMsgActivate);

u32 CorrectModeButtonsBMG(const RKNet::ROOMPacket &packet) {
    register u32 rowIdx;
    asm(mr rowIdx, r24;);  // r24 contains the actual message index
    register const ExpFroomMessages *messages;
    asm(mr messages, r19;);
    u32 bmgId;
    bmgId = Pages::FriendRoomManager::GetMessageBmg(packet, 0);

    switch (rowIdx) {
        case 4:
            return BMG_CUSTOM_START_MESSAGE;
    }

    if (rowIdx == 0) {
        const bool isOTT = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_OTT, SETTINGOTT_ONLINE) != OTTSETTING_OFFLINE_DISABLED;
        const bool isKO = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_KO, SETTINGKO_ENABLED) != KOSETTING_DISABLED;

        if (isOTT && isKO) {
            bmgId = BMG_PLAY_OTTKO;
        } else if (isOTT) {
            bmgId = BMG_PLAY_OTT;
        } else if (isKO) {
            bmgId = BMG_PLAY_KO;
        } else {
            bmgId = BMG_PLAY_GP;
        }
    }
    return bmgId;
}
kmCall(0x805dcb74, CorrectModeButtonsBMG);

static void RemapAndStoreSentMessage() {
    register u32 packet;
    register u32 manager;
    asm(mr packet, r30;);
    asm(mr manager, r28;);

    u32 message = (packet >> 8) & 0xFFFF;
    if (message == 4) {
        packet = packet & 0xFF0000FF;
    }

    *(volatile u32 *)((u8 *)manager + 0x2c60) = packet;
}
kmCall(0x805dce38, RemapAndStoreSentMessage);

void CorrectRoomStartButton(Pages::Globe::MessageWindow &control, u32 bmgId, Text::Info *info) {
    Network::SetGlobeMsgColor(control, -1);
    if (bmgId == BMG_PLAY_GP || bmgId == BMG_PLAY_TEAM_GP) {
        const u32 hostContext = System::sInstance->netMgr.hostContext;
        const bool isOTT = hostContext & (1 << PULSAR_MODE_OTT);
        const bool isKO = hostContext & (1 << PULSAR_MODE_KO);
        const bool isStartCT = hostContext & (1 << PULSAR_STARTCTS);

        if (!isStartCT) {
            if (isOTT && isKO) {
                bmgId = BMG_PLAY_OTTKO;
            } else if (isOTT) {
                bmgId = BMG_PLAY_OTT;
            } else if (isKO) {
                bmgId = BMG_PLAY_KO;
            }
        } else {
            bmgId = BMG_CUSTOM_START_MESSAGE;
        }
    }
    control.SetMessage(bmgId, info);
}
kmCall(0x805e4df4, CorrectRoomStartButton);

}  // namespace UI
}  // namespace Pulsar
