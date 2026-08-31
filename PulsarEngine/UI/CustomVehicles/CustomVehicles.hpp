#ifndef _CUSTOMVEHICLES_
#define _CUSTOMVehicles_

#include <kamek.hpp>

namespace Pulsar {
namespace UI {

const u32 STYLE_COUNT = 4;

//per local hudSlot chosen vehicle style (0 = vanilla); consumed by stats + model loading
extern u8 playstyles[4];

//per race-player-slot playstyle loaded from ghost RKG headers; used by StyleForPlayer during ghost replay
extern u8 ghostPlaystyles[4];

namespace CustomVehicles {

//poll +/- inputs on kart select pages, cycle playstyles[hud], update label
void ProcessStyleInput();

//effective playstyle (0-3) for a race player by global index; local->playstyles, remote->received
u8 StyleForPlayer(u8 playerId);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar

#endif
