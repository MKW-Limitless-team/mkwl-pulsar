#include <UI/MenuColour.hpp>
#include <UI/RaceHUDColour.hpp>
#include <UI/UI.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWII/UI/Ctrl/UpDown.hpp>
#include <MarioKartWII/UI/Ctrl/RadioButton.hpp>

namespace Pulsar {
namespace UI {

// CtrlMenu* name lists
const u32 kNameListOffset = 0x174;

// SheetSelectControl arrows
const u32 kSheetArrowOffset = 0xa8;
const u32 kSheetArrowStride = 0x248;
const u32 kSheetArrowCount  = 2;

// RadioButtonControl option buttons
const u32 kOptionButtonStride = 0x17c;
const u32 kMaxRadioOptions    = 4;

// UpDownControl arrows
const u32 kUpDownArrowCount = 2;

// Walk limits
const u32 kMaxControlDepth = 4;
const u32 kMaxChildren     = 64;

// Pointer sanity range; the structural check in WalkControl is the real guard
static bool IsSanePtr(const void* p) {
    const u32 a = reinterpret_cast<u32>(p);
    return a >= 0x80800000u && a <= 0x93FFFFFFu;
}

// Pane primitives

static void ApplyThemeToPane(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction) {
    if (!pane) return;
    RaceHUDColour::SetPaneGradientKeepAlpha(pane, startColour, endColour, direction);
    if (pane->material) UI::UnbindRLMC(pane->material);
}

static nw4r::lyt::Pane* FindPaneByNames(const LayoutUIControl* control,
                                        const char* const* names, u32 count) {
    for (u32 i = 0; i < count; ++i) {
        nw4r::lyt::Pane* pane = control->layout.GetPaneByName(names[i]);
        if (pane) return pane;
    }
    return nullptr;
}

static void TintInlineButtonArray(const char* base, u32 stride, u32 count,
                                  u32 startColour, u32 endColour,
                                  RaceHUDColour::Direction direction) {
    for (u32 k = 0; k < count; ++k) {
        const LayoutUIControl* button =
            reinterpret_cast<const LayoutUIControl*>(base + k * stride);
        if (!IsSanePtr(button->layout.layout.rootPane)) continue;
        nw4r::lyt::Pane* pane = button->layout.GetPaneByName("fuchi_pattern");
        ApplyThemeToPane(pane, startColour, endColour, direction);
    }
}

// Per-element themes

static void ApplyButtonTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction) {
    ApplyThemeToPane(control->layout.GetPaneByName("fuchi_pattern"),
                     startColour, endColour, direction);
}

static void ApplyRowTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                          RaceHUDColour::Direction direction) {
    ApplyButtonTheme(control, startColour, endColour, direction); // underline

    const UpDownControl* upDown = static_cast<const UpDownControl*>(control);
    for (u32 b = 0; b < kUpDownArrowCount; ++b) {
        const UpDownControl::UpDownButton* arrow = &upDown->arrowButtons[b];
        if (!IsSanePtr(arrow->layout.layout.rootPane)) continue;
        ApplyButtonTheme(arrow, startColour, endColour, direction);
    }
}

static void ApplyValueBoxTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                               RaceHUDColour::Direction direction) {
    static const char* valuePaneNames[] = { "fuchi_black", "shadow", "fuchi_pattern" };
    ApplyThemeToPane(FindPaneByNames(control, valuePaneNames, 3),
                     startColour, endColour, direction);
}

static void ApplyRadioPanes(const LayoutUIControl* control, u32 startColour, u32 endColour,
                            RaceHUDColour::Direction direction) {
    static const char* radioPaneNames[] = { "fuchi_black", "fuchi_pattern" };
    for (u32 j = 0; j < 2; ++j) {
        nw4r::lyt::Pane* pane = control->layout.GetPaneByName(radioPaneNames[j]);
        if (!pane) continue;
        ApplyThemeToPane(pane, startColour, endColour, direction);
        if (strcmp(radioPaneNames[j], "fuchi_pattern") == 0) {
            RaceHUDColour::SetMaterialWhiteRegs(pane);
        }
    }
}

static void ApplyRadioTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                            RaceHUDColour::Direction direction) {
    ApplyRadioPanes(control, startColour, endColour, direction);

    const RadioButtonControl* radio = static_cast<const RadioButtonControl*>(control);
    const char* optionsBase = reinterpret_cast<const char*>(radio->optionButtonsArray);
    const u32 count = (radio->buttonsCount < kMaxRadioOptions) ? radio->buttonsCount
                                                              : kMaxRadioOptions;
    for (u32 k = 0; k < count; ++k) {
        const LayoutUIControl* option =
            reinterpret_cast<const LayoutUIControl*>(optionsBase + k * kOptionButtonStride);
        if (!IsSanePtr(option->layout.layout.rootPane)) continue;
        ApplyRadioPanes(option, startColour, endColour, direction);
    }
}

static void ApplyNamePlateTheme(const LayoutUIControl* control, u32 startColour, u32 endColour,
                                RaceHUDColour::Direction direction) {
    nw4r::lyt::Pane* pane = control->layout.GetPaneByName("black_parts_t");
    if (!pane) return;
    ApplyThemeToPane(pane, startColour, endColour, direction);
    RaceHUDColour::SetMaterialWhiteRegs(pane);
}

// Container member arrays

static void ApplyNameListButtons(const LayoutUIControl* control, u32 count,
                                 u32 startColour, u32 endColour,
                                 RaceHUDColour::Direction direction) {
    TintInlineButtonArray(reinterpret_cast<const char*>(control) + kNameListOffset,
                          sizeof(LayoutUIControl), count,
                          startColour, endColour, direction);
}

// SheetSelectControl is a plain UIControl, not a LayoutUIControl
static void ApplySheetArrows(const UIControl* control, u32 startColour, u32 endColour,
                             RaceHUDColour::Direction direction) {
    TintInlineButtonArray(reinterpret_cast<const char*>(control) + kSheetArrowOffset,
                          kSheetArrowStride, kSheetArrowCount,
                          startColour, endColour, direction);
}

// Class dispatch

enum ThemeAction {
    ThemeAction_None,
    ThemeAction_Button,
    ThemeAction_Row,
    ThemeAction_ValueBox,
    ThemeAction_Radio,
    ThemeAction_NamePlate,
    ThemeAction_SheetArrows,
    ThemeAction_NameList
};

// A row also grants the cast to LayoutUIControl*, so there is no separate allowlist
struct ClassTheme {
    const char* className;
    ThemeAction action;
    u32 count; // only read for ThemeAction_NameList
};

static const ClassTheme kClassThemes[] = {
    // plain UIControl, dispatched before any rootPane check
    { "SheetSelectControl",                    ThemeAction_SheetArrows, 0 },

    // containers, own layout is never built
    { "CtrlMenuCupSelectCourse",               ThemeAction_NameList,    4 },
    { "CtrlMenuBattleCupSelectStage",          ThemeAction_NameList,    5 },

    // settings page
    { "UpDownControl",                         ThemeAction_Row,         0 },
    { "TextUpDownValueControl",                ThemeAction_ValueBox,    0 },
    { "RadioButtonControl",                    ThemeAction_Radio,       0 },

    // menu content
    { "CharaName",                             ThemeAction_NamePlate,   0 },
    { "CtrlMenuSingleTopMovieButton",          ThemeAction_Button,      0 },
    { "CtrlMenuMovieButton",                   ThemeAction_Button,      0 },
    { "CtrlMenuMachineGraph",                  ThemeAction_Button,      0 },
    { "VehicleModelControl",                   ThemeAction_Button,      0 },

    // PushButton subclasses whose class name lacks the prefix
    { "ButtonMachine",                         ThemeAction_Button,      0 },
    { "CtrlMenuCharacterSelect::ButtonDriver", ThemeAction_Button,      0 },
    { "CourseButton",                          ThemeAction_Button,      0 },
    { "StageButton",                           ThemeAction_Button,      0 }
};

static const u32 kClassThemeCount = sizeof(kClassThemes) / sizeof(kClassThemes[0]);

// Prefix match rather than an exact one
static const ClassTheme kPushButtonTheme = { "PushButton", ThemeAction_Button, 0 };

static const ClassTheme* FindClassTheme(const char* className) {
    for (u32 i = 0; i < kClassThemeCount; ++i) {
        if (strcmp(className, kClassThemes[i].className) == 0) return &kClassThemes[i];
    }
    if (strncmp(className, "PushButton", 10) == 0) return &kPushButtonTheme;
    return nullptr;
}

// Control walk

static void ApplyToControl(const UIControl* control, ThemeAction action, u32 count,
                           u32 startColour, u32 endColour,
                           RaceHUDColour::Direction direction) {
    const LayoutUIControl* layoutControl = static_cast<const LayoutUIControl*>(control);

    // NameList containers have a null rootPane; their members are validated separately
    if (action != ThemeAction_NameList &&
        !IsSanePtr(layoutControl->layout.layout.rootPane)) {
        return;
    }

    switch (action) {
        case ThemeAction_Button:     ApplyButtonTheme(layoutControl, startColour, endColour, direction);     break;
        case ThemeAction_Row:        ApplyRowTheme(layoutControl, startColour, endColour, direction);        break;
        case ThemeAction_ValueBox:   ApplyValueBoxTheme(layoutControl, startColour, endColour, direction);   break;
        case ThemeAction_Radio:      ApplyRadioTheme(layoutControl, startColour, endColour, direction);      break;
        case ThemeAction_NamePlate:  ApplyNamePlateTheme(layoutControl, startColour, endColour, direction);  break;
        case ThemeAction_NameList:   ApplyNameListButtons(layoutControl, count, startColour, endColour, direction); break;
        default: break;
    }
}

static void WalkControl(const UIControl* control, u32 startColour, u32 endColour,
                        RaceHUDColour::Direction direction, u32 depth) {
    if (!control || depth > kMaxControlDepth) return;
    const char* className = control->GetClassName();
    if (!className) return;

    const ClassTheme* theme = FindClassTheme(className);
    if (theme) {
        if (theme->action == ThemeAction_SheetArrows) {
            ApplySheetArrows(control, startColour, endColour, direction);
        } else {
            ApplyToControl(control, theme->action, theme->count,
                          startColour, endColour, direction);
        }
    }

    // Recurse into nested children (UIControl::childrenGroup at +0x68)
    const ControlGroup* children =
        reinterpret_cast<const ControlGroup*>(reinterpret_cast<const char*>(control) + 0x68);

    // parentControl must point back at us, which uninitialised memory cannot satisfy
    if (children->parentControl != control) return;

    const u32 childCount = children->controlCount;
    if (childCount == 0 || childCount > kMaxChildren) return;
    if (!IsSanePtr(children->controlArray)) return;
    for (u32 i = 0; i < childCount; ++i) {
        const UIControl* child = children->controlArray[i];
        if (IsSanePtr(child)) {
            WalkControl(child, startColour, endColour, direction, depth + 1);
        }
    }
}

// Entry point

void MenuColour::ApplyPageTheme(const Page* page) {
    if (!page || !page->controlGroup.controlArray) return;

    // Must run before the splitscreen return below so the restore happens there too
    RaceHUDColour::UpdateFuchiTableForPlayerMode();

    if (RaceHUDColour::IsSplitscreen()) return;

    u32 startColour, endColour;
    RaceHUDColour::Direction direction;
    SettingsPanel* panel = nullptr;
    bool live = false;
    if (page->pageId == PULPAGE_SETTINGS) {
        panel = static_cast<SettingsPanel*>(const_cast<Page*>(page));
        live = panel->GetLiveThemeGradient(startColour, endColour, direction);
    }
    if (!live) RaceHUDColour::GetCurrentGradient(startColour, endColour, direction);

    const u32 controlCount = page->controlGroup.controlCount;
    for (u32 i = 0; i < controlCount; ++i) {
        WalkControl(page->controlGroup.GetControl(i), startColour, endColour, direction, 0);
    }

    // Settings value boxes sit in a member array, not a childrenGroup
    if (panel) {
        for (u32 i = 0; i < Settings::Params::maxScrollerCount; ++i) {
            const TextUpDownValueControl* valueControl = panel->GetTextUpDown(i);
            if (!valueControl) continue;
            if (!IsSanePtr(valueControl->layout.layout.rootPane)) continue;
            ApplyValueBoxTheme(valueControl, startColour, endColour, direction);
        }
    }
}

} // namespace UI
} // namespace Pulsar
