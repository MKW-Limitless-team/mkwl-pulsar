#ifndef _CUSTOMVEHICLES_
#define _CUSTOMVehicles_

#include <kamek.hpp>

namespace Pulsar {
namespace UI {

const u32 STYLE_COUNT = 4;

//per local hudSlot chosen vehicle style (0 = vanilla); consumed by stats + model loading
extern u8 playstyles[4];

namespace CustomVehicles {

//poll +/- inputs on kart select pages, cycle playstyles[hud], update label
void ProcessStyleInput();

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif
