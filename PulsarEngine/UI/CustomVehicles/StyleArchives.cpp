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

//"<vanillaName>-<style>" postfixes, generated once
static const u32 POSTFIX_LENGTH = 32;
static char vehiclePostfixes[VEHICLE_COUNT][STYLE_COUNT][POSTFIX_LENGTH];
static char characterPostfixes[CHARACTER_COUNT][STYLE_COUNT][POSTFIX_LENGTH];

//tri-state existence cache: 0 unknown, STYLE_MISSING, STYLE_EXISTS
static const u8 STYLE_MISSING = 1;
static const u8 STYLE_EXISTS = 2;
static u8 vehicleStyleExists[VEHICLE_COUNT][STYLE_COUNT][CHARACTER_COUNT];
static u8 characterStyleExists[CHARACTER_COUNT][STYLE_COUNT];

//probes an szs on disc; the answer cannot change for the life of the process
static bool SzsFileExists(const char* path) {
    DVD::FileInfo info;
    if(!DVD::Open(path, &info)) return false;
    const bool exists = info.length != 0;
    DVD::Close(&info);
    return exists;
}

//"<vanillaName>-<style>", written into the given cache slot the first time it is asked for
static const char* GeneratePostfix(char* cache, const char* vanillaName, u32 style) {
    if(vanillaName == nullptr) return nullptr;
    if(cache[0] != '\0') return cache;
    if(snprintf(cache, POSTFIX_LENGTH, "%s-%u", vanillaName, style) <= 0) {
        cache[0] = '\0';
        return nullptr;
    }
    return cache;
}

const char* VehicleStylePostfix(u32 kart, u32 style) {
    if(kart >= VEHICLE_COUNT || style == 0 || style >= STYLE_COUNT) return nullptr;
    return GeneratePostfix(vehiclePostfixes[kart][style], VEHICLE_NAMES[kart], style);
}

const char* CharacterStylePostfix(u32 character, u32 style) {
    if(character >= CHARACTER_COUNT || style == 0 || style >= STYLE_COUNT) return nullptr;
    return GeneratePostfix(characterPostfixes[character][style], CHARACTER_NAMES[character], style);
}

bool VehicleStyleArchiveExists(u32 kart, u32 style, CharacterId character) {
    if(kart >= VEHICLE_COUNT || character >= CHARACTER_COUNT || style == 0 || style >= STYLE_COUNT) return false;
    u8& cached = vehicleStyleExists[kart][style][character];
    if(cached != 0) return cached == STYLE_EXISTS;
    const char* postfix = VehicleStylePostfix(kart, style);
    const char* characterName = CHARACTER_NAMES[character];
    bool exists = false;
    if(postfix != nullptr && characterName != nullptr) {
        char path[0x60];
        if(snprintf(path, sizeof(path), "/Race/Kart/%s-%s.szs", postfix, characterName) > 0) {
            exists = SzsFileExists(path);
        }
    }
    cached = exists ? STYLE_EXISTS : STYLE_MISSING;
    return exists;
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
//sizeof the literal, not the pointer
static const u32 RACE_ARCHIVE_PREFIX_LENGTH = sizeof("Race/Kart/") - 1;

//only the vehicle name is parsed; everything the game appended to it is kept as it is
bool RewriteRaceArchivePath(char* path, u8 playerId, u32 capacity) {
    //runs on the archive task thread, where the race scenario is still frozen
    const Racedata* raceData = Racedata::sInstance;
    if(path == nullptr || raceData == nullptr || playerId >= 12) return false;
    if(strncmp(path, RACE_ARCHIVE_PREFIX, RACE_ARCHIVE_PREFIX_LENGTH) != 0) return false;

    const RacedataPlayer& player = raceData->racesScenario.players[playerId];
    const u8 style = StyleForPlayer(playerId);
    if(style == 0 || !CustomArchivesEnabled()) return false;
    if(player.kartId >= VEHICLE_COUNT || player.characterId >= CHARACTER_COUNT) return false;
    const char* vehicleName = VEHICLE_NAMES[player.kartId];
    if(vehicleName == nullptr) return false;
    const u32 nameLength = strlen(vehicleName);

    char* vehicle = path + RACE_ARCHIVE_PREFIX_LENGTH;
    if(strncmp(vehicle, vehicleName, nameLength) != 0) return false; //the game named another vehicle
    if(!VehicleStyleArchiveExists(player.kartId, style, player.characterId)) return false;

    //the styled archive sits next to the vanilla one: "<vehicleName>-<style>"
    const char* tail = vehicle + nameLength;
    const u32 tailLength = strlen(tail);
    char styleSuffix[4];
    const u32 suffixLength = snprintf(styleSuffix, sizeof(styleSuffix), "-%u", style);
    if(suffixLength <= 0 || (RACE_ARCHIVE_PREFIX_LENGTH + nameLength + tailLength + suffixLength) >= capacity) {
        return false;
    }
    memmove(vehicle + nameLength + suffixLength, tail, tailLength + 1);
    memcpy(vehicle + nameLength, styleSuffix, suffixLength);
    return true;
}

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar