#ifndef _RACEHUDCOLOUR_
#define _RACEHUDCOLOUR_
#include <kamek.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

// Utility class for gradient colour management
class RaceHUDColour {
public:
    enum Direction {
        Direction_Vertical,
        Direction_Horizontal,
        Direction_Diagonal
    };

    // Get current gradient colours and direction from Input Display settings
    static void GetCurrentGradient(u32& startColour, u32& endColour, Direction& direction);
    
    // Convert setting value to RGBA colour
    static u32 GetColourFromSetting(int colourSetting);
    
    // Apply gradient to a picture pane
    static void SetPaneGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction);

    static void ApplyRaceHUDColours();

};

}//namespace UI
}//namespace Pulsar

#endif