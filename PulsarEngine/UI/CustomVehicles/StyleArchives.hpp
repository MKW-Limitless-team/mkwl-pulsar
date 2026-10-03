#ifndef _STYLEARCHIVES_
#define _STYLEARCHIVES_

#include <UI/CustomVehicles/CustomVehicles.hpp>

namespace Pulsar {
namespace UI {
namespace CustomVehicles {

//vanilla name tables; the race-side archive hook patches entries of these
extern "C" {
extern const char* VEHICLE_NAMES[VEHICLE_COUNT];
extern const char* CHARACTER_NAMES[CHARACTER_COUNT];
}

//the custom archives carry custom models and textures; the theme setting can suppress them entirely
bool CustomArchivesEnabled();

//"<vehicleName>-<style>"; null for style 0 or an invalid vehicle
const char* VehicleStylePostfix(u32 kart, u32 style);
//true when /Race/Kart/<vehicleName>-<style>-<characterName>.szs exists
bool VehicleStyleArchiveExists(u32 kart, u32 style, CharacterId character);

//"<characterName>-<style>"; null for style 0 or an invalid character
const char* CharacterStylePostfix(u32 character, u32 style);
//true when /Scene/Model/Kart/<characterName>-<style>-allkart.szs exists
bool CharacterStyleArchiveExists(u32 character, u32 style);

//rewrites "Race/Kart/<vehicle>..." in place to that player's styled archive; true when it changed
bool RewriteRaceArchivePath(char* path, u8 playerId, u32 capacity);

//sets playstyles[hud] and latches the menu archive that the new style needs
void NoteComboRandomisedStyle(u8 hud, u32 character, u8 style);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif