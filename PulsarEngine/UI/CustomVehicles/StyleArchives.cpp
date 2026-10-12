#include <UI/CustomVehicles/StyleArchives.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <Settings/Settings.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <include/c_string.h>

namespace Pulsar {
namespace UI {

bool IsBattleMode(GameMode gamemode) {
    return gamemode == MODE_BATTLE || gamemode == MODE_PUBLIC_BATTLE || gamemode == MODE_PRIVATE_BATTLE;
}

namespace CustomVehicles {

extern "C" const char* VEHICLE_NAMES[VEHICLE_COUNT];
extern "C" const char* CHARACTER_NAMES[CHARACTER_COUNT];

bool CustomArchivesEnabled() {
    if(!Settings::Mgr::IsCreated()) return true;
    return Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME,
        SETTINGTHEME_RADIO_CUSTOMTEXTURES) == THEMESETTING_CUSTOMTEXTURES_ENABLED;
}

//---- archive naming and existence ----

static const u32 POSTFIX_LENGTH = 32;
static char characterPostfixes[CHARACTER_COUNT][STYLE_COUNT][POSTFIX_LENGTH];

static const u8 STYLE_UNKNOWN = 0;
static const u8 STYLE_MISSING = 1;
static const u8 STYLE_EXISTS = 2;
static u8 characterStyleExists[CHARACTER_COUNT][STYLE_COUNT];

//probes an szs on disc; the answer cannot change for the life of the process
static bool SzsFileExists(const char* path) {
    DVD::FileInfo info;
    if(!DVD::Open(path, &info)) return false;
    const bool exists = info.length != 0;
    DVD::Close(&info);
    return exists;
}

static const char* GeneratePostfix(char* cache, const char* vanillaName, u32 style) {
    if(vanillaName == nullptr) return nullptr;
    if(cache[0] != '\0') return cache;
    if(snprintf(cache, POSTFIX_LENGTH, "%s-%u", vanillaName, style) <= 0) {
        cache[0] = '\0';
        return nullptr;
    }
    return cache;
}

const char* CharacterStylePostfix(u32 character, u32 style) {
    if(character >= CHARACTER_COUNT || style == 0 || style >= STYLE_COUNT) return nullptr;
    return GeneratePostfix(characterPostfixes[character][style], CHARACTER_NAMES[character], style);
}

bool CharacterStyleArchiveExists(u32 character, u32 style) {
    if(character >= CHARACTER_COUNT || style == 0 || style >= STYLE_COUNT) return false;
    u8& cached = characterStyleExists[character][style];
    if(cached != 0) return cached == STYLE_EXISTS;
    bool exists = false;
    const char* postfix = CharacterStylePostfix(character, style);
    if(postfix != nullptr) {
        char path[0x80];
        if(snprintf(path, sizeof(path), "/Scene/Model/Kart/%s-allkart.szs", postfix) > 0) {
            exists = SzsFileExists(path);
        }
    }
    cached = exists ? STYLE_EXISTS : STYLE_MISSING;
    return exists;
}

//the game builds "Race/Kart/<vehicle><teamSuffix>-<character><modeSuffix>"
static const char* const RACE_ARCHIVE_PREFIX = "Race/Kart/";
static const u32 RACE_ARCHIVE_PREFIX_LENGTH = sizeof("Race/Kart/") - 1;

//a rewritten path stays inside the 0x40 job context buffer; this leaves room for the slash and .szs
static const u32 RACE_PROBE_PATH_LENGTH = 0x50;

//keyed by full path, since the tail's suffixes are what a (kart, style, character) key drops
static const u32 RACE_PROBE_SLOTS = 32;
struct RaceStyleProbe {
    char path[RACE_PROBE_PATH_LENGTH];
    u8 result;
};
static RaceStyleProbe raceStyleProbes[RACE_PROBE_SLOTS];
static u32 raceProbeCursor = 0;

static bool RaceStyleArchiveExists(const char* candidate) {
    char probe[RACE_PROBE_PATH_LENGTH];
    const int written = snprintf(probe, sizeof(probe), "/%s.szs", candidate);
    if(written <= 0 || static_cast<u32>(written) >= sizeof(probe)) return false;

    for(u32 i = 0; i < RACE_PROBE_SLOTS; ++i) {
        if(raceStyleProbes[i].result == STYLE_UNKNOWN) continue;
        if(strcmp(raceStyleProbes[i].path, probe) == 0) return raceStyleProbes[i].result == STYLE_EXISTS;
    }

    const bool exists = SzsFileExists(probe);
    u32 slot = RACE_PROBE_SLOTS;
    for(u32 i = 0; i < RACE_PROBE_SLOTS; ++i) {
        if(raceStyleProbes[i].result == STYLE_UNKNOWN) { slot = i; break; }
    }
    if(slot == RACE_PROBE_SLOTS) { //answers never change, so evicting is safe
        slot = raceProbeCursor;
        raceProbeCursor = (raceProbeCursor + 1) % RACE_PROBE_SLOTS;
    }
    const u32 copied = static_cast<u32>(snprintf(raceStyleProbes[slot].path, RACE_PROBE_PATH_LENGTH, "%s", probe));
    raceStyleProbes[slot].path[copied] = '\0';
    raceStyleProbes[slot].result = exists ? STYLE_EXISTS : STYLE_MISSING;
    return exists;
}

//only the vehicle name is parsed; everything the game appended to it is kept as it is
bool RewriteRaceArchivePath(char* path, u8 playerId, u32 capacity) {
    //runs on the archive task thread, where the race scenario is still frozen
    const Racedata* raceData = Racedata::sInstance;
    if(path == nullptr || raceData == nullptr || playerId >= 12) return false;
    if(strncmp(path, RACE_ARCHIVE_PREFIX, RACE_ARCHIVE_PREFIX_LENGTH) != 0) return false;

    const RacedataPlayer& player = raceData->racesScenario.players[playerId];
    const u8 style = StyleForPlayer(playerId);
    if(style == 0 || !CustomArchivesEnabled()) return false;
    if(player.kartId >= VEHICLE_COUNT) return false;
    const char* vehicleName = VEHICLE_NAMES[player.kartId];
    if(vehicleName == nullptr) return false;
    const u32 nameLength = strlen(vehicleName);

    char* vehicle = path + RACE_ARCHIVE_PREFIX_LENGTH;
    if(strncmp(vehicle, vehicleName, nameLength) != 0) return false; //the game named another vehicle

    //the styled archive sits next to the vanilla one: "<vehicleName>-<style><tail>"
    const char* tail = vehicle + nameLength;
    const u32 tailLength = strlen(tail);
    char styleSuffix[4];
    const u32 suffixLength = snprintf(styleSuffix, sizeof(styleSuffix), "-%u", style);
    if(suffixLength <= 0 || (RACE_ARCHIVE_PREFIX_LENGTH + nameLength + tailLength + suffixLength) >= capacity) {
        return false;
    }

    //probe the name this rewrite would produce, so a request the mod has no asset for keeps vanilla
    char candidate[RACE_PROBE_PATH_LENGTH];
    const int built = snprintf(candidate, sizeof(candidate), "%s%s%s%s", RACE_ARCHIVE_PREFIX, vehicleName,
        styleSuffix, tail);
    if(built <= 0 || static_cast<u32>(built) >= sizeof(candidate)) return false;
    if(!RaceStyleArchiveExists(candidate)) return false;

    memmove(vehicle + nameLength + suffixLength, tail, tailLength + 1);
    memcpy(vehicle + nameLength, styleSuffix, suffixLength);
    return true;
}

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar