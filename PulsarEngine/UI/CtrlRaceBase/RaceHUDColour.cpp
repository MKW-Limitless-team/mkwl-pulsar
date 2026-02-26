#include <UI/CtrlRaceBase/GradientUtils.hpp>
#include <MarioKartWii/Race/RaceInfo/RaceInfo.hpp>
#include <MarioKartWii/UI/Page/RaceHUD/RaceHUD.hpp>
#include <MarioKartWii/UI/Layout/Layout.hpp>

namespace Pulsar {
namespace UI {

// Apply gradient colours to race HUD panes to match speedometer and input display
void ApplyRaceHUDColours() {
    
    // Get current gradient colours from Input Display settings
    u32 startColour, endColour;
    int direction;
    GradientUtils::GetCurrentGradient(startColour, endColour, direction);
    
    const Section* curSection = SectionMgr::sInstance->curSection;
    if (!curSection) return;
    
    // List of all race HUD panes to colour
    const char* paneNames[] = {
        "race_null", "lap_riight", "slash", "lap_text",
        "time_01", "time_02", "time_03", "time_04",
        "time_05", "time_06", "coron00", "coron01",
        "set_p", "picture_00", "picture_01", "picture_02", 
        "picture_03", "hilight_next", "item_next", 
        "hilight_curr", "item_curr"
    };
    
    // Get race page containing the race HUD layout
    const Page* racePage = curSection->GetTopLayerPage();
    if (!racePage) return;
    
    // Search through all controls in the race page
    for (int i = 0; i < racePage->controlGroup.controlCount; ++i) {
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
                // Apply gradient colour to the found pane
                GradientUtils::SetPaneGradient(pane, startColour, endColour, 
                    static_cast<GradientUtils::Direction>(direction));
            }
        }
    }
}

// Register hook to apply race HUD colours every frame during race
RaceFrameHook RaceHUDColourHook(ApplyRaceHUDColours);

} // namespace UI
} // namespace Pulsar

