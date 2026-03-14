#include <UI/RaceHUDColour.hpp>

namespace Pulsar {
namespace UI {

// List of all race HUD panes to colour
static const char* paneNames[] = {
    // Original Race HUD panes
    "race_null",
    "lap_riight", "slash", "lap_text",
    "time_01", "time_02", "time_03", "time_04",
    "time_05", "time_06", "coron00", "coron01",
    "set_p", "picture_00", "picture_01", "picture_02", 
    "picture_03", "hilight_next", "item_next", 
    "hilight_curr", "item_curr",
    
    // Speedometer panes
    "speed0", "speed1", "speed2", "speed3",
    "speed4", "speed5", "speed6", "kmh",
    
    // Input Display panes (all states)
    "DpadOff", "DpadUp", "DpadDown", "DpadLeft", "DpadRight",
    "AccelOff", "AccelPressed",
    "TriggerLOff", "TriggerLPressed",
    "TriggerROff", "TriggerRPressed", 
    "TriggerBDOff", "TriggerBDPressed",
    "Stick", "StickBackdrop"
};

void RaceHUDColour::GetCurrentGradient(u32& startColour, u32& endColour, Direction& direction) {
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
            endColour = 0x8888ffff; // #8888ff
            direction = Direction_Horizontal;
            break;
        case THEMESETTING_PRESET_CUSTOM: {
            const int colour1 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_SCROLL_COLOUR1);
            const int colour2 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_SCROLL_COLOUR2);
            const Direction dir = static_cast<Direction>(Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_RADIO_DIRECTION));
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

u32 RaceHUDColour::GetColourFromSetting(int colourSetting) {
    // Convert the colour setting value to actual RGBA colour
    switch (colourSetting) {
        case THEMESETTING_COLOUR_BLACK:     return 0x222222ff;
        case THEMESETTING_COLOUR_BLUE:      return 0x0000ffff;
        case THEMESETTING_COLOUR_BROWN:     return 0x8b4513ff;
        case THEMESETTING_COLOUR_CYAN:      return 0x00ffffff;
        case THEMESETTING_COLOUR_GREY:      return 0x808080ff;
        case THEMESETTING_COLOUR_GREEN:     return 0x008000ff;
        case THEMESETTING_COLOUR_LIGHTBLUE: return 0x6969ffff;
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

void RaceHUDColour::SetPaneGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction){
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

static u32 prevControlCount = 0;

// Apply gradient colours to race HUD panes
void RaceHUDColour::ApplyRaceHUDColours() {
    
    // Get current gradient colours from theme settings
    u32 startColour, endColour;
    Direction direction;
    GetCurrentGradient(startColour, endColour, direction);
    
    const Section* curSection = SectionMgr::sInstance->curSection;
    if (!curSection) return;
    
    // Get race page containing the race HUD layout
    const Page* racePage = curSection->GetTopLayerPage();
    if (!racePage) return;
    
    // Check if control count has changed
    u32 controlCount = racePage->controlGroup.controlCount;

    if (controlCount == prevControlCount) return;
    prevControlCount = controlCount;
    
    // Search through all controls in the race page
    for (int i = 0; i < controlCount; ++i) {
        const UIControl* control = racePage->controlGroup.GetControl(i);
        if (!control) continue;
        
        // Check if control has a layout (LayoutUIControl)
        const LayoutUIControl* layoutControl = static_cast<const LayoutUIControl*>(control);
        if (!layoutControl) continue;

        // Validate layout before accessing it
        if (!layoutControl->layout.layout.rootPane) continue;

        // Validate memory address range (Wii MEM2 cached: 0x90000000 - 0x93FFFFFF)
        u32 rootPaneAddr = reinterpret_cast<u32>(layoutControl->layout.layout.rootPane);
        if (rootPaneAddr < 0x90000000 || rootPaneAddr > 0x93FFFFFF) continue;
        
        // Find and colour each target pane in this layout
        for (int j = 0; j < sizeof(paneNames) / sizeof(paneNames[0]); ++j) {
            const char* paneName = paneNames[j];
            nw4r::lyt::Pane* pane = layoutControl->layout.GetPaneByName(paneName);
            if (pane) {
                // Special handling for race_null panes to avoid crashes
                if (strcmp(pane->name, "race_null") == 0) {
                    // Check if this race_null pane has associated HUD elements
                    bool hasLapElements = (layoutControl->layout.GetPaneByName("lap_riight") != nullptr ||
                                          layoutControl->layout.GetPaneByName("lap_text") != nullptr);
                    bool hasTimerElements = (layoutControl->layout.GetPaneByName("time_01") != nullptr ||
                                            layoutControl->layout.GetPaneByName("coron00") != nullptr);
                    
                    // Only draw race_null panes that have associated elements
                    if (hasLapElements || hasTimerElements) {
                        // Apply gradient colour to the found pane
                        SetPaneGradient(pane, startColour, endColour, direction);
                    }
                } else {
                    // Apply gradient colour to the found pane
                    SetPaneGradient(pane, startColour, endColour, direction);
                }
            }
        }
    }
}

// Register hook to apply race HUD colours every frame during race
RaceFrameHook RaceHUDColourHook(RaceHUDColour::ApplyRaceHUDColours);

// kmBranch(0x80857ac4, RaceHUDColour::ApplyRaceHUDColours); // end of RaceHUD::AfterControlUpdate

void ResetCount() { prevControlCount = 0; }
RaceLoadHook ResetCountHook(ResetCount);

}//namespace UI
}//namespace Pulsar