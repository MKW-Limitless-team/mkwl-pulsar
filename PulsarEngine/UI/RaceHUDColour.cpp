#include <UI/RaceHUDColour.hpp>
#include <UI/UI.hpp>
#include <UI/MenuColour.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Ctrl/UpDown.hpp>
#include <MarioKartWii/UI/Ctrl/RadioButton.hpp>
#include <runtimeWrite.hpp>

namespace Pulsar
{
    namespace UI
    {

        // List of all race HUD panes to colour
        static const char *paneNames[] = {
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
            "Stick", "StickBackdrop"};

        void RaceHUDColour::GetCurrentGradient(u32 &startColour, u32 &endColour, Direction &direction)
        {
            const int presetSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_RADIO_PRESET);
            const int colour1 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_SCROLL_COLOUR1);
            const int colour2 = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_SCROLL_COLOUR2);
            const int dirValue = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_THEME, SETTINGTHEME_RADIO_DIRECTION);
            GetGradientFromValues(presetSetting, colour1, colour2, dirValue, startColour, endColour, direction);
        }

        void RaceHUDColour::GetGradientFromValues(int preset, int colour1, int colour2, int directionValue,
                                                  u32 &startColour, u32 &endColour, Direction &direction)
        {
            switch (preset)
            {
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
            case THEMESETTING_PRESET_CUSTOM:
            {
                startColour = GetColourFromSetting(colour1);
                endColour = GetColourFromSetting(colour2);
                direction = static_cast<Direction>(directionValue);
                break;
            }
            default:
                startColour = 0xffffffff;
                endColour = 0xffffffff;
                direction = Direction_Vertical;
                break;
            }
        }

        u32 RaceHUDColour::GetColourFromSetting(int colourSetting)
        {
            // Convert the colour setting value to actual RGBA colour
            switch (colourSetting)
            {
            case THEMESETTING_COLOUR_BLACK:
                return 0x222222ff;
            case THEMESETTING_COLOUR_BLUE:
                return 0x2222ffff;
            case THEMESETTING_COLOUR_BROWN:
                return 0x8b4513ff;
            case THEMESETTING_COLOUR_CYAN:
                return 0x00ffffff;
            case THEMESETTING_COLOUR_GREY:
                return 0x808080ff;
            case THEMESETTING_COLOUR_GREEN:
                return 0x008000ff;
            case THEMESETTING_COLOUR_LIGHTBLUE:
                return 0x7777ffff;
            case THEMESETTING_COLOUR_LIME:
                return 0x00ff00ff;
            case THEMESETTING_COLOUR_MAGENTA:
                return 0xff00ffff;
            case THEMESETTING_COLOUR_ORANGE:
                return 0xffa500ff;
            case THEMESETTING_COLOUR_PINK:
                return 0xffc0cbff;
            case THEMESETTING_COLOUR_PURPLE:
                return 0x800080ff;
            case THEMESETTING_COLOUR_RED:
                return 0xff0000ff;
            case THEMESETTING_COLOUR_TEAL:
                return 0x008080ff;
            case THEMESETTING_COLOUR_WHITE:
                return 0xffffffff;
            case THEMESETTING_COLOUR_YELLOW:
                return 0xffff00ff;
            default:
                return 0xffffffff;
            }
        }

        void RaceHUDColour::SetPaneGradient(nw4r::lyt::Pane *pane, u32 startColour, u32 endColour, Direction direction)
        {
            if (!pane)
                return;

            nw4r::lyt::Picture *pic = static_cast<nw4r::lyt::Picture *>(pane);
            if (!pic)
                return;

            switch (direction)
            {
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

        bool RaceHUDColour::IsSplitscreen() {
            SectionMgr *mgr = SectionMgr::sInstance;
            if (mgr == nullptr || mgr->sectionParams == nullptr) return false;
            return mgr->sectionParams->localPlayerCount > 1;
        }

        // Apply gradient colours to race HUD panes
        void RaceHUDColour::ApplyRaceHUDColours()
        {

            // Before the control-count cache, which splitscreen must not consume
            if (IsSplitscreen()) return;

            // Get current gradient colours from theme settings
            u32 startColour, endColour;
            Direction direction;
            GetCurrentGradient(startColour, endColour, direction);

            const Section *curSection = SectionMgr::sInstance->curSection;
            if (!curSection)
                return;

            // Get race page containing the race HUD layout
            const Page *racePage = curSection->GetTopLayerPage();
            if (!racePage)
                return;

            // Check if control count has changed
            u32 controlCount = racePage->controlGroup.controlCount;

            if (controlCount == prevControlCount)
                return;
            prevControlCount = controlCount;

            // Search through all controls in the race page
            for (int i = 0; i < controlCount; ++i)
            {
                const UIControl *control = racePage->controlGroup.GetControl(i);
                if (!control)
                    continue;

                // Check if control has a layout (LayoutUIControl)
                const LayoutUIControl *layoutControl = static_cast<const LayoutUIControl *>(control);
                if (!layoutControl)
                    continue;

                // Validate layout before accessing it
                if (!layoutControl->layout.layout.rootPane)
                    continue;

                // Validate memory address range (Wii MEM2 cached: 0x90000000 - 0x93FFFFFF)
                u32 rootPaneAddr = reinterpret_cast<u32>(layoutControl->layout.layout.rootPane);
                if (rootPaneAddr < 0x90000000 || rootPaneAddr > 0x93FFFFFF)
                    continue;

                // Find and colour each target pane in this layout
                for (int j = 0; j < sizeof(paneNames) / sizeof(paneNames[0]); ++j)
                {
                    const char *paneName = paneNames[j];
                    nw4r::lyt::Pane *pane = layoutControl->layout.GetPaneByName(paneName);
                    if (pane)
                    {
                        // Special handling for race_null panes to avoid crashes
                        if (strcmp(pane->name, "race_null") == 0)
                        {
                            // Check if this race_null pane has associated HUD elements
                            bool hasLapElements = (layoutControl->layout.GetPaneByName("lap_riight") != nullptr ||
                                                   layoutControl->layout.GetPaneByName("lap_text") != nullptr);
                            bool hasTimerElements = (layoutControl->layout.GetPaneByName("time_01") != nullptr ||
                                                     layoutControl->layout.GetPaneByName("coron00") != nullptr);

                            // Only draw race_null panes that have associated elements
                            if (hasLapElements || hasTimerElements)
                            {
                                // Apply gradient colour to the found pane
                                SetPaneGradient(pane, startColour, endColour, direction);
                            }
                        }
                        else
                        {
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

        kmRuntimeUse(0x80895CC0);

        // Fuchi colour table: five 0x18-byte entries, one per local player,
        // indexed by ControlInputManager::getFuchiPatternColors
        static volatile s16 *const FuchiTable() {
            return reinterpret_cast<s16 *>(kmRuntimeAddr(0x80895CC0));
        }

        static s16 pristineFuchiTable[5 * (0x18 / sizeof(s16))];
        static bool havePristineFuchiTable = false;

        static void CapturePristineFuchiTable() {
            volatile s16 *table = FuchiTable();
            for (u32 i = 0; i < 5 * (0x18 / sizeof(s16)); ++i) {
                pristineFuchiTable[i] = table[i];
            }
            havePristineFuchiTable = true;
        }

        static void RestorePristineFuchiTable() {
            if (!havePristineFuchiTable) return;
            volatile s16 *table = FuchiTable();
            for (u32 i = 0; i < 5 * (0x18 / sizeof(s16)); ++i) {
                table[i] = pristineFuchiTable[i];
            }
        }

        void RaceHUDColour::WriteFuchiTableColours(u32 startColour, u32 endColour) {
            volatile s16 *table = FuchiTable();
            if (!havePristineFuchiTable) CapturePristineFuchiTable();
            for (u32 i = 0; i < 5; ++i)
            {
                volatile s16 *entry = table + i * (0x18 / sizeof(s16));
                entry[0] = (startColour >> 24) & 0xFF;
                entry[1] = (startColour >> 16) & 0xFF;
                entry[2] = (startColour >> 8) & 0xFF;
                entry[3] = startColour & 0xFF;
                entry[4] = (endColour >> 24) & 0xFF;
                entry[5] = (endColour >> 16) & 0xFF;
                entry[6] = (endColour >> 8) & 0xFF;
                entry[7] = endColour & 0xFF;
            }
        }

        // Neutral two-tone in single player, per-player scheme in splitscreen
        void RaceHUDColour::UpdateFuchiTableForPlayerMode() {
            if (IsSplitscreen()) {
                RestorePristineFuchiTable();
                return;
            }
            WriteFuchiTableColours(0xffffffff, 0x808080ff);
        }

        void RaceHUDColour::WriteThemeToFuchiTable()
        {
            if (!Settings::Mgr::IsCreated())
                return;

            UpdateFuchiTableForPlayerMode();
        }

        SectionLoadHook WriteThemeToFuchiTableHook(RaceHUDColour::WriteThemeToFuchiTable);

        // Apply the theme gradient to a picture pane, preserving each vertex's baked
        // alpha (e.g. the settings underline fade)
        void RaceHUDColour::SetPaneGradientKeepAlpha(nw4r::lyt::Pane *pane, u32 startColour, u32 endColour, RaceHUDColour::Direction direction)
        {
            if (!pane)
                return;
            nw4r::lyt::Picture *pic = static_cast<nw4r::lyt::Picture *>(pane);
            if (!pic)
                return;
            u32 colours[4];
            switch (direction)
            {
            case RaceHUDColour::Direction_Vertical:
                colours[0] = startColour;
                colours[1] = startColour;
                colours[2] = endColour;
                colours[3] = endColour;
                break;
            case RaceHUDColour::Direction_Horizontal:
                colours[0] = startColour;
                colours[1] = endColour;
                colours[2] = startColour;
                colours[3] = endColour;
                break;
            case RaceHUDColour::Direction_Diagonal:
                colours[0] = startColour;
                colours[1] = endColour;
                colours[2] = endColour;
                colours[3] = startColour;
                break;
            default:
                colours[0] = startColour;
                colours[1] = startColour;
                colours[2] = startColour;
                colours[3] = startColour;
                break;
            }
            for (int i = 0; i < 4; ++i)
            {
                pic->vertexColours[i] = (colours[i] & 0xFFFFFF00) | (pic->vertexColours[i] & 0xFF);
            }
        }

        // Neutralise a material's TEV registers to white, preserving each
        // register's file alpha (structural to the blend)
        void RaceHUDColour::SetMaterialWhiteRegs(nw4r::lyt::Pane *pane)
        {
            if (!pane || !pane->material)
                return;
            nw4r::lyt::Material *mat = pane->material;
            mat->tevColours[0].r = 0xFF;
            mat->tevColours[0].g = 0xFF;
            mat->tevColours[0].b = 0xFF;
            mat->tevColours[1].r = 0xFF;
            mat->tevColours[1].g = 0xFF;
            mat->tevColours[1].b = 0xFF;
        }

        // Write a theme gradient into a material's TEV registers, preserving each
        // register's file alpha (structural to the blend)
        static void SetMaterialThemeRegs(nw4r::lyt::Pane *pane, u32 startColour, u32 endColour)
        {
            if (!pane || !pane->material)
                return;
            nw4r::lyt::Material *mat = pane->material;
            mat->tevColours[0].r = (startColour >> 24) & 0xFF;
            mat->tevColours[0].g = (startColour >> 16) & 0xFF;
            mat->tevColours[0].b = (startColour >> 8) & 0xFF;
            mat->tevColours[1].r = (endColour >> 24) & 0xFF;
            mat->tevColours[1].g = (endColour >> 16) & 0xFF;
            mat->tevColours[1].b = (endColour >> 8) & 0xFF;
        }

        // Restore plain white vertex colours (used where the theme lives in the
        // material registers, so the two do not multiply together)
        static void SetPaneWhiteVerts(nw4r::lyt::Pane *pane)
        {
            if (!pane)
                return;
            nw4r::lyt::Picture *pic = static_cast<nw4r::lyt::Picture *>(pane);
            if (!pic)
                return;
            for (int i = 0; i < 4; ++i)
                pic->vertexColours[i] = 0xffffffff;
        }
        extern "C"
        {
            struct CupSelectPage
            {
                u8 padding[0x6c4];
                void *ctrlMenuCupSelectCup;
                u8 padding2[0x15cc - 0x174];
                PushButton cupButtons[8];    // at offset 0x174
            };
        }
        extern "C"
        {
            struct CharacterSelectPage
            {
                u8 padding[0x6c4];
                void *ctrlMenuCharSelect;
            };
        }
        void RaceHUDColour::ApplyMenuThemeColours()
        {
            const Section *curSection = SectionMgr::sInstance->curSection;
            if (!curSection)
                return;
            const Page *curPage = curSection->GetTopLayerPage();
            if (!curPage)
                return;

            MenuColour::ApplyPageTheme(curPage);
        }

        kmRuntimeUse(0x80620b40);

        static void SettingsRowHook(void *saveMgr)
        {
            typedef void (*SaveGhostManagerCalcFunc)(void *saveMgr);
            reinterpret_cast<SaveGhostManagerCalcFunc>(kmRuntimeAddr(0x80620b40))(saveMgr);

            const Section *curSection = SectionMgr::sInstance->curSection;
            if (!curSection)
                return;
            const Page *curPage = curSection->GetTopLayerPage();
            if (!curPage)
                return;

            RaceHUDColour::ApplyMenuThemeColours();
        }
        kmCall(0x80635548, SettingsRowHook);

    } // namespace UI
} // namespace Pulsar
