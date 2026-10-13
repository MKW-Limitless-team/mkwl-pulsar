#ifndef _MENUCOLOUR_
#define _MENUCOLOUR_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <UI/RaceHUDColour.hpp>
#include <MarioKartWii/UI/Ctrl/UpDown.hpp>
#include <MarioKartWii/UI/Ctrl/RadioButton.hpp>
#include <UI/UI.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>

namespace Pulsar {
namespace UI {

class MenuColour {
public:
    static void ApplyPageTheme(const Page* page);
};

// Page inline button config (for buttons that are direct Page members, not in controlGroup)
struct PageInlineButton {
    u32 offset;
    const char* paneName;
};

struct PageInlineConfig {
    const PageInlineButton* buttons;
    u32 count;
    u32 stride; // for arrays like friendButtons[5]
};

// All theme application function declarations moved from MenuColour.cpp
static void ApplyThemeToPane(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction);
static void TintInlineButtonArray(const char* base, u32 stride, u32 count,
                                  u32 startColour, u32 endColour,
                                  RaceHUDColour::Direction direction);
static void ApplyButtonTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction);
static void ApplyRowTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                          RaceHUDColour::Direction direction);
static void ApplyValueBoxTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                               RaceHUDColour::Direction direction);
static void ApplyUpDownTheme(const UpDownControl* control, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction);
static void ApplyRadioPanes(const LayoutUIControl* control, u32 startColour, u32 endColour,
                            RaceHUDColour::Direction direction);
static void ApplyRadioTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                            RaceHUDColour::Direction direction);
static void ApplyObiTrimTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                              RaceHUDColour::Direction direction);
static void ApplyNamePlateTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                                RaceHUDColour::Direction direction);
static void ApplyNameListButtons(const LayoutUIControl* control, u32 count,
                                 u32 startColour, u32 endColour,
                                 RaceHUDColour::Direction direction);
static void ApplySheetArrows(const UIControl* control, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction);
static void ApplyPageInlineButtons(const Page* page, const PageInlineConfig* config,
                                   u32 startColour, u32 endColour,
                                   RaceHUDColour::Direction direction);

} // namespace UI
} // namespace Pulsar

#endif
