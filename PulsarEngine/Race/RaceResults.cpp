#include <kamek.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <core/rvl/OS/OS.hpp>
#include <PulsarSystem.hpp>
#include <SlotExpansion/CupsConfig.hpp>

namespace Pulsar {
namespace Race {

// Hook RacedataScenario::UpdatePoints at 0x8052e950
// Fires once per race after all players finish, ensuring finalized data

// Original function typedef
typedef void (*UpdatePoints_t)(RacedataScenario* self);
static const UpdatePoints_t UpdatePoints_orig = (UpdatePoints_t)0x8052e950;

static void UpdatePoints_Hook(RacedataScenario* self) {
    // Call original function first
    UpdatePoints_orig(self);
    
    // Only log for online VS races (MODE_PRIVATE_VS=6 or MODE_PUBLIC_VS=7)
    GameMode mode = self->settings.gamemode;
    if (mode != MODE_PRIVATE_VS && mode != MODE_PUBLIC_VS) {
        return;
    }
    
    // Get player count
    u8 playerCount = self->playerCount;
    if (playerCount == 0 || playerCount > 12) {
        return;
    }
    
    // Get Raceinfo for finish times
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (!raceinfo) {
        OS::Report("PULSAR: WL:error message=\"Raceinfo::sInstance is NULL\"\n");
        return;
    }
    
    // Get timestamp in milliseconds since boot (server converts to proper timestamp)
    u64 timestamp_ticks = OS::GetTime();
    u32 timestamp_ms = OS::TicksToMilliseconds(timestamp_ticks);
    
    // Get race settings
    const RacedataSettings& settings = self->settings;
    
    // Determine course/track ID using Pulsar's backwards-compatible ID system
    // Prefer Pulsar's winning track (PulsarId), fallback to RacedataSettings::courseId
    u32 courseIdOut = settings.courseId;
    Pulsar::PulsarId pulsarId = Pulsar::PULSARID_NONE;
    if (Pulsar::CupsConfig::sInstance) {
        pulsarId = Pulsar::CupsConfig::sInstance->GetWinning();
        if (pulsarId != Pulsar::PULSARID_NONE) {
            courseIdOut = static_cast<u32>(pulsarId);
        }
    }

    // Emit race report header with minimal race metadata
    OS::Report("PULSAR: WL:race_start client_report_version=\"1.0\" timestamp_client_ms=%u player_count=%u course_id=%u\n",
               timestamp_ms, playerCount, courseIdOut);
    
    // Log results for each player
    for (u8 i = 0; i < playerCount; ++i) {
        RacedataPlayer& rdPlayer = self->players[i];
        
        // Get position (finishPos is set by UpdatePoints)
        u8 finishPos = rdPlayer.finishPos;
        if (finishPos == 0 || finishPos > playerCount) {
            finishPos = i + 1; // Fallback to player index + 1
        }
        
        // Get finish time from Raceinfo
        u32 finishTimeMs = 0;
        bool hasTime = false;
        
        if (raceinfo->players && raceinfo->players[i]) {
            RaceinfoPlayer* riPlayer = raceinfo->players[i];
            if (riPlayer->raceFinishTime && riPlayer->raceFinishTime->isActive) {
                const Timer* t = riPlayer->raceFinishTime;
                hasTime = true;
                // Convert to milliseconds
                finishTimeMs = (static_cast<u32>(t->minutes) * 60u + static_cast<u32>(t->seconds)) * 1000u 
                             + static_cast<u32>(t->milliseconds);
            }
        }
        
        // Get character and vehicle info
        u8 characterId = rdPlayer.characterId;
        u8 kartId = rdPlayer.kartId;
        
        // Use player array index as pid (player ID in race session)
        u32 pid = i;
        
        // Emit player data matching client schema
        // Required: pid; Optional: finish_position, finish_time_ms, character_id, kart_id
        if (hasTime) {
            OS::Report("PULSAR: WL:player pid=%u finish_position=%u finish_time_ms=%u character_id=%u kart_id=%u\n",
                       pid, finishPos, finishTimeMs, characterId, kartId);
        } else {
            // DNF - omit finish_time_ms (server interprets as DNF)
            OS::Report("PULSAR: WL:player pid=%u finish_position=%u character_id=%u kart_id=%u\n",
                       pid, finishPos, characterId, kartId);
        }
    }
    
    OS::Report("PULSAR: WL:race_end\n");
}

// Hook the bl instruction at 0x803a7724 that calls RacedataScenario::UpdatePoints
// This is inside a function called from the leaderboard update
kmCall(0x803a7724, UpdatePoints_Hook);

} // namespace Race
} // namespace Pulsar



