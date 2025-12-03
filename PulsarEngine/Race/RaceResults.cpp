#include <kamek.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
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

// Hook RacedataScenario::UpdatePoints at 0x8052e950
// Fires once per race after all players finish, ensuring finalized data

// Original function typedef
static void UpdatePoints_Hook() {

    RacedataScenario& racesscenario = Racedata::sInstance->racesScenario;

    // Only log for online VS races (MODE_PRIVATE_VS=6 or MODE_PUBLIC_VS=7)
    GameMode mode = racesscenario.settings.gamemode;
    if (mode != MODE_PRIVATE_VS && mode != MODE_PUBLIC_VS) {
        OS::Report("PULSAR: info message=\"Not an online race, skipping report\"\n");
        return;
    }
    
    // Get player count
    u8 playerCount = racesscenario.playerCount;
    if (playerCount == 0 || playerCount > 12) {
        OS::Report("PULSAR: error message=\"Invalid player count %u\"\n", playerCount);
        return;
    }
    
    // Get Raceinfo for finish times
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (!raceinfo) {
        OS::Report("PULSAR: error message=\"Raceinfo::sInstance is NULL\"\n");
        return;
    }
    
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

    // Send under the key the server expects
    Network::Report("wl:mkw_race_result", reportJson);
}

// Hook the blr instruction at 0x8052ed14 at the end of RacedataScenario::UpdatePoints
kmBranch(0x8052ed14, UpdatePoints_Hook);

} // namespace Race
} // namespace Pulsar



