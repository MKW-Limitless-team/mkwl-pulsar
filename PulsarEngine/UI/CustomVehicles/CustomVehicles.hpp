#ifndef _CUSTOMVEHICLES_
#define _CUSTOMVEHICLES_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace UI {

const u32 STYLE_COUNT = 4;
const u32 VEHICLE_COUNT = 36;
const u32 CHARACTER_COUNT = 0x30;

extern u8 playstyles[4];
extern u8 ghostPlaystyles[4];
extern u8 cpuPlaystyles[12];

bool IsBattleMode(GameMode gamemode);

namespace CustomVehicles {

u8 StyleForPlayer(u8 playerId);
void RandomiseCpuPlaystyles();
void RandomiseLocalPlaystyles();
void NoteMenuStyleSelected(u8 hud);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif