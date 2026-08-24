#include <UI/PlaystyleSelect/PlaystyleSelect.hpp>
#include <UI/CustomVehicles/CustomVehicles.hpp>
#include <MarioKartWii/UI/Page/Menu/CupSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Section/SectionParams.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>

namespace Pulsar {
namespace UI {

PlaystyleSelect::PlaystyleSelect() : playerCount(1), styleButtons(nullptr), arrows(nullptr) {
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &PlaystyleSelect::OnButtonClick;
    this->onButtonDeselectHandler.subject = this;
    this->onButtonDeselectHandler.ptmf = &PlaystyleSelect::OnButtonDeselect;
    this->onRightArrowHandler.subject = this;
    this->onRightArrowHandler.ptmf = &PlaystyleSelect::OnRightArrow;
    this->onLeftArrowHandler.subject = this;
    this->onLeftArrowHandler.ptmf = &PlaystyleSelect::OnLeftArrow;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &PlaystyleSelect::OnBackPress;

    const SectionParams* sectionParams = SectionMgr::sInstance->sectionParams;
    u32 localPlayerCount = 1;
    if(sectionParams != nullptr) {
        localPlayerCount = sectionParams->localPlayerCount;
        if(localPlayerCount == 0 || localPlayerCount > 4) localPlayerCount = 1;
    }
    this->playerCount = localPlayerCount;

    hasBackButton = true;
    externControlCount = 0;
    internControlCount = this->playerCount * 2; //one plate + one arrow pair per player
    extraControlNumber = 0;
    controlSources = 2;
    titleBmg = 10547; //Select Playstyle
    nextPageId = PAGE_CUP_SELECT;
    prevPageId = PAGE_MULTIPLAYER_DRIFT_SELECT;
    nextSection = SECTION_NONE;
    movieStartFrame = -1;
    isLocked = false;
    activePlayerBitfield = (1 << localPlayerCount) - 1;
    this->transitionPending = false;

    for(u32 i = 0; i < 4; ++i) confirmed[i] = false;

    //without it Page::Init derefs a null control group
    this->InitControlGroup(this->playerCount * 2);

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

PlaystyleSelect::~PlaystyleSelect() {
    if(this->styleButtons != nullptr) delete[] this->styleButtons;
    if(this->arrows != nullptr) delete[] this->arrows;
}

void PlaystyleSelect::OnInit() {
    this->styleButtons = new PushButton[this->playerCount];
    this->arrows = new SheetSelectControl[this->playerCount];
    for(u8 hud = 0; hud < 4; ++hud) {
        this->savedScrollHandlers[hud] = nullptr;
        this->savedRightHandlers[hud] = nullptr;
    }
    this->controlCount = 0;

    Menu::OnInit();
}

void PlaystyleSelect::OnActivate() {
    MenuInteractable::OnActivate();

    this->backButton.isHidden = false;
    this->transitionPending = false;
    for(u8 hud = 0; hud < this->playerCount; ++hud) {
        this->confirmed[hud] = false;
        this->UpdateStyleDisplay(hud);
        this->arrows[hud].rightArrow.Toggle(true);
        this->arrows[hud].leftArrow.Toggle(true);
        this->RestorePlayerSelection(hud);
        this->styleButtons[hud].HandleDeselect(hud, 0);
    }
}

UIControl* PlaystyleSelect::CreateControl(u32 id) {
    const u32 count = this->controlCount;
    const u32 pairIdx = id / 2;
    if(pairIdx >= this->playerCount) return nullptr;
    const bool isArrows = (id % 2) != 0;
    this->controlCount++;

    //per-player positions come from BRCTR variants (P1 top, P2 bottom)
    char rightVariant[0x20];
    char leftVariant[0x20];
    snprintf(rightVariant, sizeof(rightVariant), "arrowR_pos_%u_%u", this->playerCount, pairIdx);
    snprintf(leftVariant, sizeof(leftVariant), "arrowL_pos_%u_%u", this->playerCount, pairIdx);

    UIControl* result = nullptr;
    if(!isArrows) {
        PushButton& button = this->styleButtons[pairIdx];
        this->AddControl(count, button, 0);
        char plateVariant[0x20];
        snprintf(plateVariant, sizeof(plateVariant), "playstyle_pos_%u_%u", this->playerCount, pairIdx);
        button.Load(UI::buttonFolder, "playstyle1", plateVariant, 1 << pairIdx, 0, false);
        button.buttonId = pairIdx;
        button.SetOnClickHandler(this->onButtonClickHandler, pairIdx);
        //swallows A presses while this player is confirmed
        this->clickGates[pairIdx].page = this;
        this->clickGates[pairIdx].hud = pairIdx;
        button.manipulator.actionHandlers[FORWARD_PRESS] = &this->clickGates[pairIdx];
        result = &button;
    }
    else {
        SheetSelectControl& arrowPair = this->arrows[pairIdx];
        this->AddControl(count, arrowPair, 0);
        arrowPair.SetRightArrowHandler(this->onRightArrowHandler);
        arrowPair.SetLeftArrowHandler(this->onLeftArrowHandler);
        arrowPair.Load(UI::buttonFolder, "CupsArrowR", rightVariant, "CupsArrowL", leftVariant,
            1 << pairIdx, 0, false);
        result = &arrowPair;
    }
    return result;
}

UIControl* PlaystyleSelect::CreateExternalControl(u32 id) {
    return nullptr;
}

ManipulatorManager& PlaystyleSelect::GetManipulatorManager() {
    return this->controlsManipulatorManager;
}

int PlaystyleSelect::GetPlayerBitfield() const {
    return this->playerBitfield;
}

int PlaystyleSelect::GetActivePlayerBitfield() const {
    return this->activePlayerBitfield;
}

const ut::detail::RuntimeTypeInfo* PlaystyleSelect::GetRuntimeTypeInfo() const {
    return Page::GetRuntimeTypeInfo();
}

void PlaystyleSelect::OnExternalButtonSelect(PushButton& button, u32 r5) {
}

void PlateClickGate::operator()(u32 hudSlotId, u32 r5) const {
    if(this->page == nullptr || this->hud >= 4) return;
    PlaystyleSelect& page = *this->page;
    //swallow A presses while confirmed so HandleClick never runs
    if(!page.IsConfirmed(this->hud)) page.GetStyleButton(this->hud).HandleClick(hudSlotId, r5);
}

//no-op handler muting the held arrow's scroll path while confirmed
struct MutedScrollGate : PtmfHolder_2A<LayoutUIControl, void, u32, u32> {
    void operator()(u32 hudSlotId, u32 r5) const override {}
};
static MutedScrollGate mutedScrollGate;

//CheckActions plays the decide-SE before dispatching and nothing suppresses it per
//button, so confirmed players get their selection pointed at a handler-less target.
//Never use null here: CheckActions derefs curManipulator + offset unguarded.
void PlaystyleSelect::ClearPlayerSelection(u8 hud) {
    ControlManipulator& man = this->arrows[hud].rightArrow.manipulator;
    this->savedScrollHandlers[hud] = man.actionHandlers[FORWARD_PRESS];
    this->savedRightHandlers[hud] = man.actionHandlers[RIGHT_PRESS];
    man.actionHandlers[FORWARD_PRESS] = &mutedScrollGate;
    man.actionHandlers[RIGHT_PRESS] = &mutedScrollGate;
    this->controlsManipulatorManager.holders[hud].curManipulator = &man;
}

void PlaystyleSelect::RestorePlayerSelection(u8 hud) {
    ControlManipulator& man = this->arrows[hud].rightArrow.manipulator;
    if(this->savedScrollHandlers[hud] != nullptr) {
        man.actionHandlers[FORWARD_PRESS] = this->savedScrollHandlers[hud];
    }
    if(this->savedRightHandlers[hud] != nullptr) {
        man.actionHandlers[RIGHT_PRESS] = this->savedRightHandlers[hud];
    }
    this->controlsManipulatorManager.holders[hud].curManipulator =
        &this->styleButtons[hud].manipulator;
}

bool PlaystyleSelect::AllConfirmed() const {
    for(u8 hud = 0; hud < this->playerCount; ++hud) {
        if(!this->confirmed[hud]) return false;
    }
    return true;
}

bool PlaystyleSelect::AnyConfirmed() const {
    for(u8 hud = 0; hud < this->playerCount; ++hud) {
        if(this->confirmed[hud]) return true;
    }
    return false;
}

void PlaystyleSelect::UpdateStyleDisplay(u8 hud) {
    const u32 kartId = SectionMgr::sInstance->sectionParams->karts[hud];
    if(kartId < 36 && playstyles[hud] < STYLE_COUNT) {
        this->styleButtons[hud].SetMessage(UI::BMG_PLAYSTYLE_NAMES + kartId * 4 + playstyles[hud]);
    }
}

void PlaystyleSelect::CycleStyle(u8 hud, int step) {
    if(this->transitionPending || this->confirmed[hud]) return;
    u32 style = (playstyles[hud] + STYLE_COUNT + step) % STYLE_COUNT;
    playstyles[hud] = static_cast<u8>(style);
    this->UpdateStyleDisplay(hud);
    Audio::RSARPlayer::PlaySoundById(step > 0 ? SOUND_ID_RIGHT_ARROW_PRESS : SOUND_ID_LEFT_ARROW_PRESS, 0, nullptr);
}

void PlaystyleSelect::OnRightArrow(SheetSelectControl& control, u32 hudSlotId) {
    for(u8 hud = 0; hud < this->playerCount; ++hud) {
        if(&this->arrows[hud] == &control) {
            this->CycleStyle(hud, 1);
            return;
        }
    }
}

void PlaystyleSelect::OnLeftArrow(SheetSelectControl& control, u32 hudSlotId) {
    for(u8 hud = 0; hud < this->playerCount; ++hud) {
        if(&this->arrows[hud] == &control) {
            this->CycleStyle(hud, -1);
            return;
        }
    }
}

void PlaystyleSelect::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

void PlaystyleSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if(hudSlotId >= this->playerCount) return;
    if(this->transitionPending) return;
    //A only confirms; undoing is B's job
    if(this->confirmed[hudSlotId]) return;

    this->confirmed[hudSlotId] = true;
    this->arrows[hudSlotId].rightArrow.Toggle(false);
    this->arrows[hudSlotId].leftArrow.Toggle(false);
    //keep the plate highlighted; re-clicks are blocked by the gate + cleared selection
    this->styleButtons[hudSlotId].Select(hudSlotId);
    Audio::RSARPlayer::PlaySoundById(SOUND_ID_BUTTON_SELECT, 0, nullptr);
    this->ClearPlayerSelection(hudSlotId);
    this->backButton.isHidden = this->AnyConfirmed();

    if(this->AllConfirmed()) {
        //isLocked must stay false: it gates the deferred load itself
        this->transitionPending = true;
        const PageId nextPage = IsBattle() ? PAGE_BATTLE_CUP_SELECT : PAGE_CUP_SELECT;
        this->LoadNextPageWithDelayById(nextPage, 30.0f);
    }
}

void PlaystyleSelect::OnButtonDeselect(PushButton& button, u32 hudSlotId) {
}

void PlaystyleSelect::OnBackPress(u32 hudSlotId) {
    if(hudSlotId >= this->playerCount) return;
    if(this->transitionPending) return;

    //B undoes this player's selection first
    if(this->confirmed[hudSlotId]) {
        this->confirmed[hudSlotId] = false;
        this->arrows[hudSlotId].rightArrow.Toggle(true);
        this->arrows[hudSlotId].leftArrow.Toggle(true);
        this->styleButtons[hudSlotId].HandleDeselect(hudSlotId, 0);
        this->RestorePlayerSelection(hudSlotId);
        this->backButton.isHidden = false;
        Audio::RSARPlayer::PlaySoundById(SOUND_ID_BACK_PRESS, 0, nullptr);
        return;
    }

    //only player 1 may back out (vanilla local multiplayer convention); the section
    //contains MultiDriftSelect, not DriftSelect
    if(hudSlotId != 0) return;
    this->nextPageId = PAGE_MULTIPLAYER_DRIFT_SELECT;
    this->EndStateAnimated(0, this->backButton.GetAnimationFrameSize());
}

void PlaystyleSelect::OnStartPress(u32 hudSlotId) {
}

}//namespace UI
}//namespace Pulsar
