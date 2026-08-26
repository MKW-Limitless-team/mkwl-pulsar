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

}//namespace StatBars
}//namespace UI
}//namespace Pulsar

#endif
