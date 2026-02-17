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

// Race frame hook for timestamps
// Variables to store timestamps
static u64 raceStartTime = 0;
static u64 raceFinishTime = 0;
static u32 raceDurationMs = 0;

// Flags to ensure timestamps are only captured once per race stage
static bool raceStartTimeRecorded = false;
static bool raceFinishRecorded = false;

// Helper function to check if the local player has finished the race
static bool hasFinished() {
    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (!raceInfo) {
        return false;
    }
    
    u8 localPlayerId = 0;
    if (RKNet::Controller::sInstance) {
        u8 localAid = RKNet::Controller::sInstance->subs[RKNet::Controller::sInstance->currentSub].localAid;
        for (u8 pid = 0; pid < 12; ++pid) {
            if (RKNet::Controller::sInstance->aidsBelongingToPlayerIds[pid] == localAid) {
                localPlayerId = pid;
                break;
            }
        }
    }
    
    return raceInfo->players && raceInfo->players[localPlayerId] && 
           (raceInfo->players[localPlayerId]->stateFlags & 0x2);
}

// Hook RacedataScenario::UpdatePoints at 0x8052e950
static void UpdatePoints_Hook() {

    RacedataScenario& racesscenario = Racedata::sInstance->racesScenario;

    // Get local player ID to report only our own result
    u8 localPlayerId = 0;
    if (RKNet::Controller::sInstance) {
        u8 localAid = RKNet::Controller::sInstance->subs[RKNet::Controller::sInstance->currentSub].localAid;
        for (u8 pid = 0; pid < 12; ++pid) {
            if (RKNet::Controller::sInstance->aidsBelongingToPlayerIds[pid] == localAid) {
                localPlayerId = pid;
                break;
            }
        }
    }

    // Get Raceinfo for finish times
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (!raceinfo) {
        OS::Report("PULSAR: error message=\"Raceinfo::sInstance is NULL\"\n");
        return;
    }

    // Only log for online VS races (MODE_PRIVATE_VS=6 or MODE_PUBLIC_VS=7)
    GameMode mode = racesscenario.settings.gamemode;
    if (mode != MODE_PRIVATE_VS && mode != MODE_PUBLIC_VS) {
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
    int pos = snprintf(reportJson, sizeof(reportJson), "{\"client_report_version\":\"1.1\",\"player\":");
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

        // Resolve an authoritative profile id (pid) when possible.
        u32 pid = 0;
        if (RKNet::Controller::sInstance) {
            // Map player index -> aid
            u8 aid = RKNet::Controller::sInstance->aidsBelongingToPlayerIds[i];
            DWCiNodeInfo* nodeInfo = DWCi_NodeInfoList_GetNodeInfoForAid(aid);
            if (nodeInfo) {
                pid = nodeInfo->profileId;
            }
        }
        
        u32 character = static_cast<u32>(rdPlayer.characterId);
        u32 kart = static_cast<u32>(rdPlayer.kartId);

        // Get player's finishing position and room size
        u8 position = 0;
        if (raceinfo->players && raceinfo->players[i]) {
            position = raceinfo->players[i]->position;
        }

        // Build JSON
        if (hasTime) {
            u32 true_finishTime = OS::TicksToMilliseconds(raceFinishTime - raceStartTime);
            snprintf(reportJson + strlen(reportJson), sizeof(reportJson) - strlen(reportJson),
                     "{\"pid\":%u,\"finish_time_ms\":%u,\"position\":[%u,%u],\"character_id\":%u,\"kart_id\":%u",
                     pid, finishTime, position, playerCount, character, kart);
        } else {
            snprintf(reportJson + strlen(reportJson), sizeof(reportJson) - strlen(reportJson),
                     "{\"pid\":%u,\"finish_time_ms\":null,\"position\":[%u,%u],\"character_id\":%u,\"kart_id\":%u",
                     pid, position, playerCount, character, kart);
        }
    }

    snprintf(reportJson + strlen(reportJson), sizeof(reportJson) - strlen(reportJson), "}}");

    // Send under the key the server expects
    Network::Report("wl:mkw_race_result", reportJson);
}

// Hook the blr instruction at 0x8052ed14 at the end of RacedataScenario::UpdatePoints
kmBranch(0x8052ed14, UpdatePoints_Hook);

// The race frame hook function
void GetTimestamp() {
    // Ensure Raceinfo instance is valid
    Raceinfo* raceInfo = Raceinfo::sInstance;
    if (!raceInfo) {
        OS::Report("PULSAR: error message=\"Raceinfo::sInstance is NULL\"\n");
        return; // Should not happen, but good practice
    }

    // Reset timing variables when entering a new race (RACESTAGE_INTRO)
    if (raceInfo->stage == RACESTAGE_INTRO) {
        raceStartTime = 0;
        raceFinishTime = 0;
        raceDurationMs = 0;
        raceStartTimeRecorded = false;
        raceFinishRecorded = false;
    }

    if (raceInfo->stage == RACESTAGE_RACE && !raceStartTimeRecorded) {
        raceStartTime = OS::GetTime();
        raceStartTime = OS::TicksToMilliseconds(raceStartTime);
        raceStartTimeRecorded = true;
        OS::Report("PULSAR: stage_timing race_start_time=%llu ms\n", raceStartTime);

        // Report timestamp
        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp), "%llu", raceStartTime);
        Network::Report("wl:mkw_race_start_time", timestamp);
    }

    if (!raceFinishRecorded && raceStartTime > 0) {
        if (hasFinished()) {
            raceFinishTime = OS::GetTime();
            raceFinishTime = OS::TicksToMilliseconds(raceFinishTime);
            raceFinishRecorded = true;
            OS::Report("PULSAR: stage_timing race_finish_time=%llu ms\n", raceFinishTime);

            // Report timestamp
            char timestamp[32];
            snprintf(timestamp, sizeof(timestamp), "%llu", raceFinishTime);
            Network::Report("wl:mkw_race_finish_time", timestamp);
        }
    }
}

RaceFrameHook raceTimestampsHook(GetTimestamp);

} // namespace Race
} // namespace Pulsar
