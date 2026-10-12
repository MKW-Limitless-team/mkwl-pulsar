#ifndef _PLAYSTYLESTATBARS_
#define _PLAYSTYLESTATBARS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace UI {
namespace StatBars {

void RefreshGridBars(u8 hud, KartId hoveredKart);
KartId GetHoveredVehicle(u8 hud, KartId fallback);

}//namespace StatBars
}//namespace UI
}//namespace Pulsar

#endif
