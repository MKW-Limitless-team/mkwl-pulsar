#include <UI/CtrlRaceBase/GradientUtils.hpp>

namespace Pulsar {
namespace UI {

void GradientUtils::GetCurrentGradient(u32& startColour, u32& endColour, int& direction) {
    const int presetSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_PRESET);

    switch (presetSetting) {
        case INPUTSETTING_PRESET_LIMITLESS:
            startColour = 0x1b28e7ff;
            endColour = 0xdb62ddff;
            direction = 0; // vertical
            break;
        case INPUTSETTING_PRESET_UNLIMITED:
            startColour = 0xff0000ff;
            endColour = 0x420000ff;
            direction = 2; // diagonal
            break;
        case INPUTSETTING_PRESET_BLAZINGCOLD:
            startColour = 0xff2222ff;
            endColour = 0x2222ffff;
            direction = 1; // horizontal
            break;
        case INPUTSETTING_PRESET_CUSTOM: {
            const int colour1 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_SCROLL_COLOUR1);
            const int colour2 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_SCROLL_COLOUR2);
            const int dir = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_DIRECTION);
            startColour = GetColourFromSetting(colour1);
            endColour = GetColourFromSetting(colour2);
            direction = dir;
            break;
        }
        default:
            startColour = 0xffffffff;
            endColour = 0xffffffff;
            direction = 0;
            break;
    }
}

u32 GradientUtils::GetColourFromSetting(int colourSetting) {
    // Convert the colour setting value to actual RGBA colour
    switch (colourSetting) {
        case INPUTSETTING_COLOUR_BLACK:     return 0x000000ff;  // Black
        case INPUTSETTING_COLOUR_BLUE:      return 0x7979ffff;  // Blue
        case INPUTSETTING_COLOUR_BROWN:     return 0x8b4513ff;  // Brown
        case INPUTSETTING_COLOUR_CYAN:      return 0x00ffffff;  // Cyan
        case INPUTSETTING_COLOUR_GREY:      return 0x808080ff;  // Grey
        case INPUTSETTING_COLOUR_GREEN:     return 0x008000ff;  // Green
        case INPUTSETTING_COLOUR_LIME:      return 0x00ff00ff;  // Lime
        case INPUTSETTING_COLOUR_MAGENTA:   return 0xff00ffff;  // Magenta
        case INPUTSETTING_COLOUR_ORANGE:    return 0xffa500ff;  // Orange
        case INPUTSETTING_COLOUR_PINK:      return 0xffc0cbff;  // Pink
        case INPUTSETTING_COLOUR_PURPLE:    return 0x800080ff;  // Purple
        case INPUTSETTING_COLOUR_RED:       return 0xff0000ff;  // Red
        case INPUTSETTING_COLOUR_VIOLET:    return 0xee82eeff;  // Violet
        case INPUTSETTING_COLOUR_WHITE:     return 0xffffffff;  // White
        case INPUTSETTING_COLOUR_YELLOW:    return 0xffff00ff;  // Yellow
        default:                            return 0x1b28e7ff;  // Default to Limitless blue
    }
}

void GradientUtils::SetPaneGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction) {
    if (!pane) return;
    nw4r::lyt::Picture* pic = static_cast<nw4r::lyt::Picture*>(pane);
    if (!pic) return;
    
    // direction: 0=vertical, 1=horizontal, 2=diagonal
    switch (direction) {
        case Direction_Vertical:
            pic->vertexColours[0] = startColour; // TL
            pic->vertexColours[1] = startColour; // TR
            pic->vertexColours[2] = endColour;   // BL
            pic->vertexColours[3] = endColour;   // BR
            break;
        case Direction_Horizontal:
            pic->vertexColours[0] = startColour; // TL
            pic->vertexColours[1] = endColour;   // TR
            pic->vertexColours[2] = startColour; // BL
            pic->vertexColours[3] = endColour;   // BR
            break;
        case Direction_Diagonal:
            pic->vertexColours[0] = startColour; // TL
            pic->vertexColours[1] = endColour;   // TR
            pic->vertexColours[2] = endColour;   // BL
            pic->vertexColours[3] = startColour; // BR
            break;
        default:
            pic->vertexColours[0] = startColour;
            pic->vertexColours[1] = startColour;
            pic->vertexColours[2] = startColour;
            pic->vertexColours[3] = startColour;
            break;
    }
}

}//namespace UI
}//namespace Pulsar