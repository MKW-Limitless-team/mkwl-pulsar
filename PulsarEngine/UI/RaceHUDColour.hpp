#ifndef _RACEHUDCOLOUR_
#define _RACEHUDCOLOUR_
#include <kamek.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

class RaceHUDColour {
public:
    enum Direction {
        Direction_Vertical,
        Direction_Horizontal,
        Direction_Diagonal
    };

    static void GetCurrentGradient(u32& startColour, u32& endColour, Direction& direction);

    static u32 GetColourFromSetting(int colourSetting);

    static void GetGradientFromValues(int preset, int colour1, int colour2, int directionValue,
        u32& startColour, u32& endColour, Direction& direction);

    static void SetPaneGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction);

    static bool IsSplitscreen();

    static void ApplyRaceHUDColours();
    static void WriteThemeToFuchiTable();

    static void WriteFuchiTableColours(u32 startColour, u32 endColour);

    static void UpdateFuchiTableForPlayerMode();

    static void SetPaneGradientKeepAlpha(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction);

    static void SetMaterialWhiteRegs(nw4r::lyt::Pane* pane);

    static void ApplyMenuThemeColours();

};

}//namespace UI
}//namespace Pulsar

#endif
