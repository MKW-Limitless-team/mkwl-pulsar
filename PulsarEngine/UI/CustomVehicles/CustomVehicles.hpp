#ifndef _CUSTOMVEHICLES_
#define _CUSTOMVEHICLES_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace UI {

//style 0 is vanilla, 1 to STYLE_COUNT - 1 are the custom playstyles
const u32 STYLE_COUNT = 4;
const u32 VEHICLE_COUNT = 36;
const u32 CHARACTER_COUNT = 0x30;

//style currently selected by each local player
extern u8 playstyles[4];
//style of each ghost slot, read from the ghost's RKG header
extern u8 ghostPlaystyles[4];
//style of each cpu slot
extern u8 cpuPlaystyles[12];

//battles always use the vanilla vehicles
bool IsBattleMode(GameMode gamemode);

namespace CustomVehicles {

//style to use for a race player (0-11)
u8 StyleForPlayer(u8 playerId);

//rolls a style for every cpu of a grand prix or vs race
void RandomiseCpuPlaystyles();
//rolls a style for every local player of the combo that is being randomised
void RandomiseLocalPlaystyles();

//latches a style picked outside the vehicle select screen into the menu archive
void NoteMenuStyleSelected(u8 hud);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif