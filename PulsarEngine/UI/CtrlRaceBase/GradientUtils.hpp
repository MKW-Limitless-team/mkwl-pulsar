#ifndef _GRADIENT_UTILS_
#define _GRADIENT_UTILS_
#include <kamek.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

// Utility class for gradient colour management
class GradientUtils {
public:
    enum Direction {
        Direction_Vertical,
        Direction_Horizontal,
        Direction_Diagonal
    };

    // Get current gradient colours and direction from Input Display settings
    static void GetCurrentGradient(u32& startColour, u32& endColour, int& direction);
    
    // Convert setting value to RGBA colour
    static u32 GetColourFromSetting(int colourSetting);
    
    // Apply gradient to a picture pane
    static void SetPaneGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction);
};

}//namespace UI
}//namespace Pulsar

#endif