#include <kamek.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <core/rvl/OS/OS.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {

// Hook RacedataScenario::UpdatePoints at 0x8052e950
// This fires ONCE per race, about 3 seconds after the last player finishes
// Perfect timing: all data is finalized, no need for flags or stage checks

// Simple, safe wchar_t (UTF-16) to ASCII converter (best-effort)
static void WStrToAscii(const wchar_t* w, char* out, size_t outSize) {
    if (!out || outSize == 0) return;
    out[0] = '\0';
    if (!w) return;
    size_t j = 0;
    for (; j + 1 < outSize && w[j] != 0 && j < 10; ++j) {
        wchar_t c = w[j];
        if (c >= 32 && c < 127) out[j] = (char)c; else out[j] = '?';
    }
    out[j] = '\0';
}

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
    
    // Log race header with comprehensive settings
    const RacedataSettings& settings = self->settings;
    OS::Report("PULSAR: ===== Online VS Race Results =====\n");
    OS::Report("PULSAR: Course: %u | Engine: %u | Players: %u | Laps: %u | Seed: %08X\n",
               settings.courseId, settings.engineClass, playerCount, settings.lapCount, settings.randomSeed);
    OS::Report("PULSAR: GameMode: %u | ItemMode: %u | ModeFlags: %08X\n",
               settings.gamemode, settings.itemMode, settings.modeFlags);
    
    // Get Raceinfo for finish times
    Raceinfo* raceinfo = Raceinfo::sInstance;
    if (!raceinfo) {
        OS::Report("PULSAR: Warning - Raceinfo::sInstance is NULL\n");
        return;
    }
    
    // Log results for each player
    for (u8 i = 0; i < playerCount; ++i) {
        RacedataPlayer& rdPlayer = self->players[i];
        
        // Get position (finishPos is set by UpdatePoints)
        u8 pos = rdPlayer.finishPos;
        if (pos == 0 || pos > playerCount) {
            pos = i + 1; // Fallback to player index + 1
        }
        
        // Get finish time from Raceinfo
        u16 mm = 0, msec = 0; 
        u8 ss = 0; 
        bool hasTime = false;
        
        if (raceinfo->players && raceinfo->players[i]) {
            RaceinfoPlayer* riPlayer = raceinfo->players[i];
            if (riPlayer->raceFinishTime && riPlayer->raceFinishTime->isActive) {
                const Timer* t = riPlayer->raceFinishTime;
                hasTime = true;
                mm = t->minutes;
                ss = t->seconds;
                msec = t->milliseconds;
            }
        }
        
        // Get player name from Mii
        char name[24];
        WStrToAscii(rdPlayer.mii.info.name, name, sizeof(name));
        
        // Get character and vehicle info
        u8 character = rdPlayer.characterId;
        u8 vehicle = rdPlayer.kartId;
        
        // Get player type (real/CPU) and controller
        u8 playerType = rdPlayer.playerType;
        u8 controller = rdPlayer.controllerType;
        
        // Get team info if teams are enabled
        u8 team = rdPlayer.team;
        
        // Get rating (VR/BR)
        u16 rating = rdPlayer.rating.points;
        
        // Get score/points
        u16 prevScore = rdPlayer.previousScore;
        u16 newScore = rdPlayer.score;
        s16 scoreChange = (s16)newScore - (s16)prevScore;
        
        // Log comprehensive result
        if (hasTime) {
            OS::Report("PULSAR: Player[%u] pos=%u time=%02u:%02u.%03u name=\"%s\" char=%u veh=%u type=%u ctrl=%u team=%u rating=%u score=%d->%d (%+d)\n",
                       i, pos, mm, ss, msec, name, character, vehicle, playerType, controller, team, rating, prevScore, newScore, scoreChange);
        } else {
            OS::Report("PULSAR: Player[%u] pos=%u time=DNF name=\"%s\" char=%u veh=%u type=%u ctrl=%u team=%u rating=%u score=%d->%d (%+d)\n",
                       i, pos, name, character, vehicle, playerType, controller, team, rating, prevScore, newScore, scoreChange);
        }
    }
    
    OS::Report("PULSAR: ===== End Race Results =====\n");
}

// Hook the bl instruction at 0x803a7724 that calls RacedataScenario::UpdatePoints
// This is inside a function called from the leaderboard update (0x8085c878 -> 0x803a769c -> 0x803a7724)
// Using kmCall because this is a bl (branch and link) instruction, not a function entry point
kmCall(0x803a7724, UpdatePoints_Hook);

} // namespace Race
} // namespace Pulsar



