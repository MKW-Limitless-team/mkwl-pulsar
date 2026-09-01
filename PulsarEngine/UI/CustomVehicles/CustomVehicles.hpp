#ifndef _CUSTOMVEHICLES_
#define _CUSTOMVehicles_

#include <kamek.hpp>

namespace Pulsar {
namespace UI {

const u32 STYLE_COUNT = 4;

extern u8 playstyles[4];
extern u8 ghostPlaystyles[4];
extern u8 cpuPlaystyles[12];

namespace CustomVehicles {

void ProcessStyleInput();
u8 StyleForPlayer(u8 playerId);
void RandomiseCpuPlaystyles();

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif
