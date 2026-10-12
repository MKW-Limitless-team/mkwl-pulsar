#ifndef _STYLEARCHIVES_
#define _STYLEARCHIVES_

#include <UI/CustomVehicles/CustomVehicles.hpp>

namespace Pulsar {
namespace UI {
namespace CustomVehicles {

extern "C" {
extern const char* VEHICLE_NAMES[VEHICLE_COUNT];
extern const char* CHARACTER_NAMES[CHARACTER_COUNT];
}

bool CustomArchivesEnabled();

const char* CharacterStylePostfix(u32 character, u32 style);
bool CharacterStyleArchiveExists(u32 character, u32 style);
bool RewriteRaceArchivePath(char* path, u8 playerId, u32 capacity);
void NoteComboRandomisedStyle(u8 hud, u32 character, u8 style);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif