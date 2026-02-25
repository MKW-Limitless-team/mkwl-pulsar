#include <UI/CtrlRaceBase/GradientUtils.hpp>

namespace Pulsar {
namespace UI {

void GradientUtils::GetCurrentGradient(u32& startColour, u32& endColour, int& direction) {
    const int presetSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_RADIO_PRESET);

    switch (presetSetting) {
        case THEMESETTING_PRESET_LIMITLESS:
            startColour = 0x1b28e7ff;
            endColour = 0xdb62ddff;
            direction = Direction_Vertical;
            break;
        case THEMESETTING_PRESET_UNLIMITED:
            startColour = 0xff0000ff;
            endColour = 0x420000ff;
            direction = Direction_Diagonal;
            break;
        case THEMESETTING_PRESET_BLAZINGCOLD:
            startColour = 0xff2222ff;
            endColour = 0x2222ffff;
            direction = Direction_Horizontal;
            break;
        case THEMESETTING_PRESET_CUSTOM: {
            const int colour1 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_SCROLL_COLOUR1);
            const int colour2 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_SCROLL_COLOUR2);
            const int dir = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_RADIO_DIRECTION);
            startColour = GetColourFromSetting(colour1);
            endColour = GetColourFromSetting(colour2);
            direction = dir;
            break;
        }
        default:
            startColour = 0xffffffff;
            endColour = 0xffffffff;
            direction = Direction_Vertical;
            break;
    }
}

u32 GradientUtils::GetColourFromSetting(int colourSetting) {
    // Convert the colour setting value to actual RGBA colour
    switch (colourSetting) {
        case THEMESETTING_COLOUR_BLACK:     return 0x000000ff;
        case THEMESETTING_COLOUR_BLUE:      return 0x7979ffff;
        case THEMESETTING_COLOUR_BROWN:     return 0x8b4513ff;
        case THEMESETTING_COLOUR_CYAN:      return 0x00ffffff;
        case THEMESETTING_COLOUR_GREY:      return 0x808080ff;
        case THEMESETTING_COLOUR_GREEN:     return 0x008000ff;
        case THEMESETTING_COLOUR_INDIGO:    return 0x4b0082ff;
        case THEMESETTING_COLOUR_LIME:      return 0x00ff00ff;
        case THEMESETTING_COLOUR_MAGENTA:   return 0xff00ffff;
        case THEMESETTING_COLOUR_ORANGE:    return 0xffa500ff;
        case THEMESETTING_COLOUR_PINK:      return 0xffc0cbff;
        case THEMESETTING_COLOUR_PURPLE:    return 0x800080ff;
        case THEMESETTING_COLOUR_RED:       return 0xff0000ff;
        case THEMESETTING_COLOUR_TEAL:      return 0x008080ff;
        case THEMESETTING_COLOUR_WHITE:     return 0xffffffff;
        case THEMESETTING_COLOUR_YELLOW:    return 0xffff00ff;
        default:                            return 0xffffffff;  
    }
}

void GradientUtils::SetPaneGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction) {
    if (!pane) return;
    nw4r::lyt::Picture* pic = static_cast<nw4r::lyt::Picture*>(pane);
    if (!pic) return;
    
    switch (direction) {
        case Direction_Vertical:
            pic->vertexColours[0] = startColour;
            pic->vertexColours[1] = startColour;
            pic->vertexColours[2] = endColour;
            pic->vertexColours[3] = endColour;
            break;
        case Direction_Horizontal:
            pic->vertexColours[0] = startColour;
            pic->vertexColours[1] = endColour;
            pic->vertexColours[2] = startColour;
            pic->vertexColours[3] = endColour;
            break;
        case Direction_Diagonal:
            pic->vertexColours[0] = startColour;
            pic->vertexColours[1] = endColour;
            pic->vertexColours[2] = endColour;
            pic->vertexColours[3] = startColour;
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