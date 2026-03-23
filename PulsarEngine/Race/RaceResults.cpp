#include <kamek.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Network/GPReport.hpp>

// DWCi declarations for node info
struct DWCiNodeInfo {
    u32 profileId;
    u8 unknown[0x30 - 0x4];
};

extern "C" DWCiNodeInfo* DWCi_NodeInfoList_GetNodeInfoForAid(u8 playerAid);

namespace Pulsar {
namespace Race {

static u32 raceStartTime = 0;
static u32 raceFinishTime = 0;
static bool raceStartTimeRecorded = false;
static bool raceFinishTimeRecorded = false;
static bool localResultReported = false;

static bool IsOnlineRace() {
    GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
    return mode == MODE_PRIVATE_VS || mode == MODE_PUBLIC_VS;
}

static s8 GetLocalPlayerId(const RKNet::Controller& controller) {
    const u8 localAid = controller.subs[controller.currentSub].localAid;
    for (u8 playerId = 0; playerId < 12; ++playerId) {
        if (controller.aidsBelongingToPlayerIds[playerId] == localAid) {
            return static_cast<s8>(playerId);
        }
    }
    return -1;
}

static u32 GetProfileId(const RKNet::Controller& controller, u8 playerId) {
    const u8 aid = controller.aidsBelongingToPlayerIds[playerId];
    DWCiNodeInfo* nodeInfo = DWCi_NodeInfoList_GetNodeInfoForAid(aid);
    return nodeInfo ? nodeInfo->profileId : 0;
}

static bool ShouldSkipReportForHostDC(const Raceinfo& raceInfo, const RacedataScenario& scenario,
                                      const RKNet::Controller& controller) {
    const u8 hostAid = controller.subs[controller.currentSub].hostAid;
    s8 hostPlayerId = -1;
    for (u8 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        if (controller.aidsBelongingToPlayerIds[playerId] == hostAid) {
            hostPlayerId = static_cast<s8>(playerId);
            break;
        }
    }

    if (hostPlayerId < 0) {
        return true;
    }

    const RaceinfoPlayer* hostPlayer = raceInfo.players[hostPlayerId];
    if ((hostPlayer->stateFlags & 0x10) == 0) {
        return false;
    }

    u8 finishedCount = 0;
    u8 disconnectCount = 0;
    for (u8 playerId = 0; playerId < scenario.playerCount; ++playerId) {
        const RaceinfoPlayer* curPlayer = raceInfo.players[playerId];
        if (curPlayer->stateFlags & 0x10) {
            ++disconnectCount;
        } else if (curPlayer->stateFlags & 0x02) {
            ++finishedCount;
        }
    }

    return finishedCount <= (scenario.playerCount / 2) || disconnectCount >= 3;
}

static void SendLocalRaceResult(Raceinfo& raceInfo, u8 playerId) {
    if (!IsOnlineRace() || localResultReported) {
        return;
    }

    Racedata* raceData = Racedata::sInstance;
    RKNet::Controller* controller = RKNet::Controller::sInstance;
    if (raceData == nullptr || controller == nullptr) {
        return;
    }

    RacedataScenario& scenario = raceData->racesScenario;
    if (scenario.playerCount == 0 || scenario.playerCount > 12) {
        OS::Report("PULSAR: error message=\"Invalid player count %u\"\n", scenario.playerCount);
        return;
    }

    if (playerId >= scenario.playerCount) {
        return;
    }

    const s8 localPlayerId = GetLocalPlayerId(*controller);
    if (localPlayerId < 0 || playerId != static_cast<u8>(localPlayerId)) {
        return;
    }

    RacedataPlayer& racePlayer = scenario.players[playerId];
    if (racePlayer.playerType != PLAYER_REAL_LOCAL) {
        return;
    }

    if (ShouldSkipReportForHostDC(raceInfo, scenario, *controller)) {
        return;
    }

    RaceinfoPlayer* infoPlayer = nullptr;
    if (raceInfo.players != nullptr) {
        infoPlayer = raceInfo.players[playerId];
    }
    if (infoPlayer == nullptr) {
        return;
    }

    if (!raceFinishTimeRecorded) {
        raceFinishTime = OS::TicksToMilliseconds(OS::GetTime());
        raceFinishTimeRecorded = true;
        Network::ReportU32("wl:mkw_race_finish_time", raceFinishTime);
        Network::PumpGPI();
    }

    bool hasTime = false;
    u32 finishTimeMs = 0;
    if (infoPlayer->raceFinishTime != nullptr && infoPlayer->raceFinishTime->isActive) {
        const Timer* timer = infoPlayer->raceFinishTime;
        hasTime = true;
        finishTimeMs =
            (static_cast<u32>(timer->minutes) * 60u + static_cast<u32>(timer->seconds)) * 1000u +
            static_cast<u32>(timer->milliseconds);
    }

    char reportJson[256];
    if (hasTime) {
        snprintf(reportJson,
                 sizeof(reportJson),
                 "{\"client_report_version\":\"1.0\",\"player\":{\"pid\":%u,\"finish_time_ms\":%u,"
                 "\"character_id\":%u,\"kart_id\":%u,\"player_count\":%u}}",
                 GetProfileId(*controller, playerId),
                 finishTimeMs,
                 static_cast<u32>(racePlayer.characterId),
                 static_cast<u32>(racePlayer.kartId),
                 static_cast<u32>(scenario.playerCount));
    } else {
        snprintf(reportJson,
                 sizeof(reportJson),
                 "{\"client_report_version\":\"1.0\",\"player\":{\"pid\":%u,\"finish_time_ms\":null,"
                 "\"character_id\":%u,\"kart_id\":%u,\"player_count\":%u}}",
                 GetProfileId(*controller, playerId),
                 static_cast<u32>(racePlayer.characterId),
                 static_cast<u32>(racePlayer.kartId),
                 static_cast<u32>(scenario.playerCount));
    }

    Network::Report("wl:mkw_race_result", reportJson);
    Network::PumpGPI();
    localResultReported = true;
}

static void EndPlayerRaceHook(Raceinfo* raceInfo, u8 playerId) {
    raceInfo->EndPlayerRace(playerId);
    if (raceInfo == nullptr) {
        return;
    }
    SendLocalRaceResult(*raceInfo, playerId);
}

void GetTimestamp() {
    if (!IsOnlineRace()) {
        return;
    }

    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (raceInfo == nullptr) {
        return;
    }

    if (raceInfo->stage == RACESTAGE_INTRO) {
        raceStartTime = 0;
        raceFinishTime = 0;
        raceStartTimeRecorded = false;
        raceFinishTimeRecorded = false;
        localResultReported = false;
    }

    if (raceInfo->stage == RACESTAGE_COUNTDOWN && !raceStartTimeRecorded) {
        raceStartTime = OS::TicksToMilliseconds(OS::GetTime());
        raceStartTimeRecorded = true;
        Network::ReportU32("wl:mkw_race_start_time", raceStartTime);
        Network::PumpGPI();
    }
}

kmCall(0x8053491c, EndPlayerRaceHook);
RaceFrameHook raceTimestampsHook(GetTimestamp);

}  // namespace Race
}  // namespace Pulsar
