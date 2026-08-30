#ifndef _PLAYSTYLESTATBARS_
#define _PLAYSTYLESTATBARS_

#include <kamek.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace UI {
namespace StatBars {

//re-runs the vehicle-select stat bar update for one player's graph, applying the
//player's current playstyle row (from /Pulsar/UI/playstyle_para.bin deltas) and
//per-stat nerf/buff coloring
void RefreshGridBars(u8 hud, KartId hoveredKart);

//the vehicle the game is currently previewing for this hud (fed to the hooked graph
//update on every grid selection change); fallback is used before the first game call
KartId GetHoveredVehicle(u8 hud, KartId fallback);

}//namespace StatBars
}//namespace UI
}//namespace Pulsar

#endif
