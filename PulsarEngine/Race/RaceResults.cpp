#include <kamek.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/Driver/DriverManager.hpp>
#include <core/rvl/OS/OS.hpp>
#include <Network/GPReport.hpp>

#define TIMESTAMP_INTERVAL 20 //frames
// DWCi declarations for node info
struct DWCiNodeInfo {
    u32 profileId;
    u8 unknown[0x30 - 0x4];
};

extern "C" DWCiNodeInfo* DWCi_NodeInfoList_GetNodeInfoForAid(u8 playerAid);


namespace Pulsar {
namespace Race {

// Variables to store timestamps
static u32 raceStartTime = 0;
static u32 raceProgressTime = 0;
static u32 raceFinishTime = 0;

// Flags to ensure timestamps are only captured once per race stage
static bool raceStartTimeRecorded = false;
static bool raceFinishRecorded = false;
static u8 frameCount = 0;

static bool isOnlineRace() {
    GameMode mode = Racedata::sInstance->racesScenario.settings.gamemode;
    return (mode == MODE_PRIVATE_VS || mode == MODE_PUBLIC_VS);
}

// Hook RacedataScenario::UpdatePoints at 0x8052e950
static void UpdatePoints_Hook() {

    // Only log for online VS races 
    if (!isOnlineRace()) {
        return;
    }

    RacedataScenario& racesscenario = Racedata::sInstance->racesScenario;

    // Get local player ID to report only our own result
    u8 localPlayerId = -1;
    if (RKNet::Controller::sInstance) {
        u8 localAid = RKNet::Controller::sInstance->subs[RKNet::Controller::sInstance->currentSub].localAid;
        for (u8 pid = 0; pid < 12; ++pid) {
            if (RKNet::Controller::sInstance->aidsBelongingToPlayerIds[pid] == localAid) {
                localPlayerId = pid;
                break;
            }
        }
    }
    if (localPlayerId == -1) return;

    // Get Raceinfo for finish times
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (!raceinfo) {
        OS::Report("PULSAR: error message=\"Raceinfo::sInstance is NULL\"\n");
        return;
    }
    
    // Get player count
    u8 playerCount = racesscenario.playerCount;
    if (playerCount == 0 || playerCount > 12) {
        OS::Report("PULSAR: error message=\"Invalid player count %u\"\n", playerCount);
        return;
    }

    // Build JSON matching server schema (only for local player)
    char reportJson[4096];
    int pos = snprintf(reportJson, sizeof(reportJson), "{\"client_report_version\":\"1.0\",\"player\":");
    if (pos < 0) return;

    // Only process the local player
    u8 i = localPlayerId;
    {
        RacedataPlayer& rdPlayer = racesscenario.players[i];

        bool hasTime = false;
        u32 finishTime = 0;
        if (raceinfo->players && raceinfo->players[i]) {
            RaceinfoPlayer* riPlayer = raceinfo->players[i];
            if (riPlayer->raceFinishTime && riPlayer->raceFinishTime->isActive) {
                const Timer* t = riPlayer->raceFinishTime;
                hasTime = true;
                finishTime = (static_cast<u32>(t->minutes) * 60u + static_cast<u32>(t->seconds)) * 1000u + static_cast<u32>(t->milliseconds);
            }
        }

        u32 pid = 0;
        if (RKNet::Controller::sInstance) {
            u8 aid = RKNet::Controller::sInstance->aidsBelongingToPlayerIds[i];
            DWCiNodeInfo* nodeInfo = DWCi_NodeInfoList_GetNodeInfoForAid(aid);
            if (nodeInfo) {
                pid = nodeInfo->profileId;
            }
        }
        
        u32 character = static_cast<u32>(rdPlayer.characterId);
        u32 kart = static_cast<u32>(rdPlayer.kartId);

        // Build JSON
        if (hasTime) {
            snprintf(reportJson + strlen(reportJson), sizeof(reportJson) - strlen(reportJson),
                     "{\"pid\":%u,\"finish_time_ms\":%u,\"character_id\":%u,\"kart_id\":%u",
                     pid, finishTime, character, kart);
        } else {
            snprintf(reportJson + strlen(reportJson), sizeof(reportJson) - strlen(reportJson),
                     "{\"pid\":%u,\"finish_time_ms\":null,\"character_id\":%u,\"kart_id\":%u",
                     pid, character, kart);
        }
    }

    snprintf(reportJson + strlen(reportJson), sizeof(reportJson) - strlen(reportJson), "}}");
    Network::Report("wl:mkw_race_result", reportJson);
}

// Hook the blr instruction at 0x8052ed14 at the end of RacedataScenario::UpdatePoints
kmBranch(0x8052ed14, UpdatePoints_Hook);

void GetTimestamp() {
    if (!isOnlineRace()) {
        return;
    }
    Raceinfo* raceInfo = Raceinfo::sInstance;

    // Reset timing variables when entering a new race
    if (raceInfo->stage == RACESTAGE_INTRO) {
        raceStartTime = 0;
        raceFinishTime = 0;
        raceStartTimeRecorded = false;
        raceFinishRecorded = false;
    }

    if (raceInfo->stage == RACESTAGE_COUNTDOWN && !raceStartTimeRecorded) {
        raceStartTime = OS::TicksToMilliseconds(OS::GetTime());
        raceStartTimeRecorded = true;
        Network::ReportU32("wl:mkw_race_start_time", raceStartTime);
    }

    if(!raceFinishRecorded && raceStartTimeRecorded) {
        // if(frameCount == TIMESTAMP_INTERVAL) {
        //     raceProgressTime = OS::TicksToMilliseconds(OS::GetTime());
        //     Network::ReportU32("wl:mkw_race_progress_time", raceProgressTime);
        //     frameCount = 0;
        // }
        // else frameCount++;

        if (raceInfo->stage == RACESTAGE_INTRO) {
            raceFinishTime = OS::TicksToMilliseconds(OS::GetTime());
            raceFinishRecorded = true;
            Network::ReportU32("wl:mkw_race_finish_time", raceFinishTime);
        }
    }

    Network::PumpGPI(); // Force flush the buffer every frame for accurate timing
}

RaceFrameHook raceTimestampsHook(GetTimestamp);

} // namespace Race
} // namespace Pulsar
