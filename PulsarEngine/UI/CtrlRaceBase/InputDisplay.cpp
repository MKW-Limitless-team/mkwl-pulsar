/*
Copyright 2021-2023 Pablo Stebler

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <UI/CtrlRaceBase/InputDisplay.hpp>

namespace Pulsar {
namespace UI {

//Input Display by MKW-SP Team ported by Rambo
//Taken from https://github.com/mkw-sp/mkw-sp/blob/main/payload/game/ui/ctrl/CtrlRaceInputDisplay.cc
const s8 CtrlRaceInputViewer::DPAD_HOLD_FOR_N_FRAMES = 10;
void CtrlRaceInputViewer::Init() {
    char name[32];
    bool isBrakedriftToggled = true;
    RacedataScenario& raceScenario = Racedata::sInstance->racesScenario;
    
    for (int i = 0; i < (int)DpadState_Count; ++i) {
        DpadState state = static_cast<DpadState>(i);
        const char* stateName = CtrlRaceInputViewer::DpadStateToName(state);
        
        snprintf(name, 32, "Dpad%.*s", strlen(stateName), stateName);
        nw4r::lyt::Pane* pane = this->layout.GetPaneByName(name);
        this->SetPaneVisibility(name, state == DpadState_Off);
        this->m_dpadPanes[i] = pane;
        
        this->HudSlotColorEnable(name, true);
    }
    
    // Check if this is a nunchuck controller to handle positioning differently
    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const ControllerType controllerType = SectionMgr::sInstance->pad.padInfos[0].controllerHolder->curController->GetType();
    const int inputSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_INPUT);
    const bool isGhostRace = (sectionId >= SECTION_WATCH_GHOST_FROM_CHANNEL && sectionId <= SECTION_WATCH_GHOST_FROM_MENU);
    bool isNunchuck = (controllerType == NUNCHUCK) && !(inputSetting == INPUTSETTING_INPUT_FORCED || isGhostRace);

    for (int i = 0; i < (int)AccelState_Count; ++i) {
        AccelState state = static_cast<AccelState>(i);
        const char* stateName = CtrlRaceInputViewer::AccelStateToName(state);

        snprintf(name, 32, "Accel%.*s", strlen(stateName), stateName);
        nw4r::lyt::Pane* pane = this->layout.GetPaneByName(name);
        this->SetPaneVisibility(name, state == AccelState_Off);

        if (isBrakedriftToggled && !isNunchuck) {
            pane->trans.x += pane->scale.x * 15.0f;
            pane->trans.y += pane->scale.z * 15.0f;
        }
        this->m_accelPanes[i] = pane;

        this->HudSlotColorEnable(name, true);
    }
    
    for (int i = 0; i < (int)Trigger_Count; ++i) {
        Trigger trigger = static_cast<Trigger>(i);
        const char* triggerName = CtrlRaceInputViewer::TriggerToName(trigger);
        
        for (int j = 0; j < (int)TriggerState_Count; ++j) {
            TriggerState state = static_cast<TriggerState>(j);
            const char* stateName = CtrlRaceInputViewer::TriggerStateToName(state);
            
            snprintf(name, 32, "Trigger%.*s%.*s", strlen(triggerName), triggerName, strlen(stateName), stateName);
            nw4r::lyt::Pane* pane = this->layout.GetPaneByName(name);
            this->SetPaneVisibility(name, state == TriggerState_Off);
            if (!isBrakedriftToggled && trigger == Trigger_BD) {
                this->SetPaneVisibility(name, false);
            }
            this->m_triggerPanes[i][j] = pane;

            this->HudSlotColorEnable(name, true);
        }
    }
    
    this->m_stickPane = this->layout.GetPaneByName("Stick");
    this->m_stickOrigin = this->m_stickPane->trans;
    this->m_playerId = this->GetPlayerId();

    this->HudSlotColorEnable("Stick", true);
    this->HudSlotColorEnable("StickBackdrop", true);

    this->ApplyButtonColours();

    LayoutUIControl::Init();
}

void CtrlRaceInputViewer::OnUpdate() {
    this->UpdatePausePosition();
    u8 playerId = this->GetPlayerId();
    if (playerId != m_playerId) {
        m_dpadTimer = 0;
        m_playerId = playerId;
    }
    RacedataScenario& raceScenario = Racedata::sInstance->racesScenario;
    if (playerId < raceScenario.playerCount) {
        RaceinfoPlayer* player = Raceinfo::sInstance->players[playerId];
        if (player) {
            Input::State* input = &player->realControllerHolder->inputStates[0];
            DpadState dpadState = (DpadState)input->motionControlFlick;
            Vec2 stick = input->stick;
            // Check mirror mode
            if (raceScenario.settings.modeFlags & 1) {
                stick.x = -stick.x;
                if (input->motionControlFlick == DpadState_Left) {
                    dpadState = DpadState_Right;
                } else if (input->motionControlFlick == DpadState_Right) {
                    dpadState = DpadState_Left;
                }
            }
            
            bool accel = input->buttonActions & 0x1;
            bool L = input->buttonActions & 0x4;
            bool R = input->buttonActions & 0x2;
            bool BD = input->buttonActions & 0x10;

            setDpad(dpadState);
            setAccel(accel ? AccelState_Pressed : AccelState_Off);
            setTrigger(Trigger_L, L ? TriggerState_Pressed : TriggerState_Off);
            setTrigger(Trigger_R, R ? TriggerState_Pressed : TriggerState_Off);
            setStick(stick);
            setTrigger(Trigger_BD, BD ? TriggerState_Pressed : TriggerState_Off);
        }
    }
}
u32 CtrlRaceInputViewer::Count() {
    if(Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_INPUT) == INPUTSETTING_INPUT_DISABLED)
        return 0;
    else if(Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_INPUT) == INPUTSETTING_INPUT_ENABLED || 
    Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_INPUT) == INPUTSETTING_INPUT_FORCED) {
        // Declare and initialize scenario here
        const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
        u32 localPlayerCount = scenario.localPlayerCount;
        const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
        if(sectionId >= SECTION_WATCH_GHOST_FROM_CHANNEL && sectionId <= SECTION_WATCH_GHOST_FROM_MENU) 
            localPlayerCount += 1;
        if(localPlayerCount == 0 && (scenario.settings.gametype & GAMETYPE_ONLINE_SPECTATOR)) 
            localPlayerCount = 1;
        return localPlayerCount;
    }
    return 0; 
}
void CtrlRaceInputViewer::Create(Page& page, u32 index, u32 count) {
    u8 variantId = (count == 3) ? 4 : count;
    for(int i = 0; i < count; ++i) {
        CtrlRaceInputViewer* inputViewer = new CtrlRaceInputViewer;
        page.AddControl(index + i, *inputViewer, 0);
        char variant[0x20];
        int pos = i;
        snprintf(variant, 0x20, "InputDisplay_%u_%u", variantId, pos);
        inputViewer->Load(variant, i);
    }
}
static CustomCtrlBuilder INPUTVIEWER(CtrlRaceInputViewer::Count, CtrlRaceInputViewer::Create);
void CtrlRaceInputViewer::Load(const char* variant, u8 id) {
    this->hudSlotId = id;
    ControlLoader loader(this);
    const char* groups[] = { nullptr, nullptr };

    const SectionId sectionId = SectionMgr::sInstance->curSection->sectionId;
    const ControllerType controllerType = SectionMgr::sInstance->pad.padInfos[0].controllerHolder->curController->GetType();
    const int inputSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_INPUT);
    const bool isGhostRace = (sectionId >= SECTION_WATCH_GHOST_FROM_CHANNEL && sectionId <= SECTION_WATCH_GHOST_FROM_MENU);

    if (inputSetting == INPUTSETTING_INPUT_FORCED || isGhostRace) {
        loader.Load(UI::raceFolder, "PULInputViewer", variant, groups);
        return;
    }

    if (controllerType == NUNCHUCK && inputSetting == INPUTSETTING_INPUT_ENABLED && !isGhostRace) {
        loader.Load(UI::raceFolder, "PULInputViewerNunchuck", variant, groups);
        return;
    }

    if (controllerType == WHEEL || controllerType == CLASSIC || controllerType == GCN) {
        loader.Load(UI::raceFolder, "PULInputViewer", variant, groups);
    }

        loader.Load(UI::raceFolder, "PULInputViewer", variant, groups);
}
void CtrlRaceInputViewer::setDpad(DpadState state) {
    if (state == m_dpadState) {
        return;
    }
    // Only hold for off press
    if (state == DpadState_Off && m_dpadTimer != 0 && --m_dpadTimer) {
        return;
    }
    m_dpadPanes[static_cast<u32>(m_dpadState)]->flag &= ~1;
    m_dpadPanes[static_cast<u32>(state)]->flag |= 1;
    m_dpadState = state;
    m_dpadTimer = DPAD_HOLD_FOR_N_FRAMES;
}
void CtrlRaceInputViewer::setAccel(AccelState state) {
    if (state == m_accelState) {
        return;
    }
    m_accelPanes[static_cast<u32>(m_accelState)]->flag &= ~1;
    m_accelPanes[static_cast<u32>(state)]->flag |= 1;
    m_accelState = state;
}
void CtrlRaceInputViewer::setTrigger(Trigger trigger, TriggerState state) {
    u32 t = static_cast<u32>(trigger);
    if (state == m_triggerStates[t]) {
        return;
    }
    m_triggerPanes[t][static_cast<u32>(m_triggerStates[t])]->flag &= ~1;
    m_triggerPanes[t][static_cast<u32>(state)]->flag |= 1;
    m_triggerStates[t] = state;
}
void CtrlRaceInputViewer::setStick(Vec2 state) {
    if (state.x == m_stickState.x && state.z == m_stickState.z) {
        return;
    }
    // Map range [-1, 1] -> [-width * 5 / 19, width * 5 / 19]
    f32 scale = 5.0f / 19.0f;
    m_stickPane->trans.x =
            m_stickOrigin.x + scale * state.x * m_stickPane->scale.x * m_stickPane->size.x;
    m_stickPane->trans.y =
            m_stickOrigin.y + scale * state.z * m_stickPane->scale.z * m_stickPane->size.z;
    m_stickState = state;
}

void CtrlRaceInputViewer::SetButtonGradient(nw4r::lyt::Pane* pane, u32 startColour, u32 endColour, Direction direction ) {
    nw4r::lyt::Picture* pic = reinterpret_cast<nw4r::lyt::Picture*>(pane);
    
    // Vertex layout for a quad:
    // 0 (top-left)    1 (top-right)    <- Left vertices: start colour
    // 2 (bottom-left) 3 (bottom-right)  <- Right vertices: end colour


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

void CtrlRaceInputViewer::ApplyButtonColours() {
    // Get the current preset setting
    const int presetSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_PRESET);
    
    u32 startColour, endColour;
    Direction direction;
    
    // Switch case for different presets
    switch (presetSetting) {
        case INPUTSETTING_PRESET_LIMITLESS:
            startColour = 0x1b28e7ff;  // #1b28e7
            endColour = 0xdb62ddff;    // #db62dd
            direction = Direction_Vertical;
            break;
            
        case INPUTSETTING_PRESET_UNLIMITED:
            startColour = 0xff0000ff;  // #ff0000
            endColour = 0x420000ff;    // #420000
            direction = Direction_Diagonal;
            break;
            
        case INPUTSETTING_PRESET_BLAZINGCOLD:
            startColour = 0xff000099;  // #ff0000
            endColour = 0x0000ff99;    // #0000ff
            direction = Direction_Horizontal;
            break;

        case INPUTSETTING_PRESET_CUSTOM: {
            // Custom preset - use the selected colors
            const int colour1Setting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_SCROLL_COLOUR1);
            const int colour2Setting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_SCROLL_COLOUR2);
            const int directionSetting = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_INPUT, SETTINGINPUT_RADIO_DIRECTION);

            startColour = GetColourFromSetting(colour1Setting);
            endColour = GetColourFromSetting(colour2Setting);
            direction = static_cast<Direction>(directionSetting);
            break;
        }
            
        default:
            // Fallback to white
            startColour = 0xffffffff;
            endColour = 0xffffffff;
            direction = Direction_Vertical;
            break;
    }

    // Apply gradient to all button states
    for (int i = 0; i < (int)AccelState_Count; ++i) {
        this->SetButtonGradient(this->m_accelPanes[i], startColour, endColour, direction);
    }

    for (int i = 0; i < (int)Trigger_Count; ++i) {
        for (int j = 0; j < (int)TriggerState_Count; ++j) {
            this->SetButtonGradient(this->m_triggerPanes[i][j], startColour, endColour, direction);
        }
    }

    for (int i = 0; i < (int)DpadState_Count; ++i) {
        this->SetButtonGradient(this->m_dpadPanes[i], startColour, endColour, direction);
    }

    // Apply vertical gradient to stick and backdrop
    this->SetButtonGradient(this->m_stickPane, startColour, endColour, direction);
    nw4r::lyt::Pane* stickBackdrop = this->layout.GetPaneByName("StickBackdrop");
    if (stickBackdrop) {
        this->SetButtonGradient(stickBackdrop, startColour, endColour, direction);
    }
}

u32 CtrlRaceInputViewer::GetColourFromSetting(int colourSetting) {
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
}//namespace UI
}//namespace Pulsar
