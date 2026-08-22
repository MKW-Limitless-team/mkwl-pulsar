#include <UI/PlaystyleSelect/PlaystyleSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/CupSelect.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Section/SectionParams.hpp>
#include <MarioKartWii/GlobalFunctions.hpp>
#include <SlotExpansion/CupsConfig.hpp>
#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace UI {

u8 playstyles[4] = {0, 0, 0, 0};

PlaystyleSelect::PlaystyleSelect() : playstyleCount(4), totalButtons(0), playstyleButtons(nullptr) {
    OS::Report("PlaystyleSelect::Constructor called\n");
    this->onButtonClickHandler.subject = this;
    this->onButtonClickHandler.ptmf = &PlaystyleSelect::OnButtonClick;
    this->onButtonDeselectHandler.subject = this;
    this->onButtonDeselectHandler.ptmf = &PlaystyleSelect::OnButtonDeselect;
    this->onBackPressHandler.subject = this;
    this->onBackPressHandler.ptmf = &PlaystyleSelect::OnBackPress;
    this->onStartPressHandler.subject = this;
    this->onStartPressHandler.ptmf = &PlaystyleSelect::OnStartPress;

    hasBackButton = true;
    externControlCount = 0;
    internControlCount = 0; //set dynamically in OnInit, before Menu::OnInit runs
    extraControlNumber = 0;
    controlSources = 2;
    titleBmg = 10547; //Select Playstyle
    nextPageId = PAGE_CUP_SELECT;
    prevPageId = PAGE_DRIFT_SELECT;
    nextSection = SECTION_NONE;
    movieStartFrame = -1;
    isLocked = false;
    activePlayerBitfield = 1;

    this->controlsManipulatorManager.Init(1, false);
    this->SetManipulatorManager(controlsManipulatorManager);
    this->controlsManipulatorManager.SetGlobalHandler(START_PRESS, onStartPressHandler, false, false);
    this->controlsManipulatorManager.SetGlobalHandler(BACK_PRESS, onBackPressHandler, false, false);
}

PlaystyleSelect::~PlaystyleSelect() {
    if(this->playstyleButtons != nullptr) delete[] this->playstyleButtons;
}

void PlaystyleSelect::OnInit() {
    OS::Report("PlaystyleSelect::OnInit called\n");
    const SectionParams* sectionParams = SectionMgr::sInstance->sectionParams;
    u32 localPlayerCount = sectionParams->localPlayerCount;
    if(localPlayerCount == 0 || localPlayerCount > 4) localPlayerCount = 1;
    this->totalButtons = localPlayerCount * this->playstyleCount;
    this->internControlCount = this->totalButtons;
    this->activePlayerBitfield = (1 << localPlayerCount) - 1;
    this->playstyleButtons = new PushButton[this->totalButtons];
    this->controlCount = 0;

    Menu::OnInit();
    OS::Report("PlaystyleSelect: Menu::OnInit done, totalButtons=%d\n", this->totalButtons);
}

void PlaystyleSelect::OnActivate() {
    OS::Report("PlaystyleSelect::OnActivate called\n");
    MenuInteractable::OnActivate();

    SectionParams* sectionParams = SectionMgr::sInstance->sectionParams;
    u32 localPlayerCount = sectionParams->localPlayerCount;
    if(localPlayerCount == 0 || localPlayerCount > 4) localPlayerCount = 1;

    for(u32 i = 0; i < this->totalButtons; ++i) {
        u32 hudSlot = i / playstyleCount;
        u32 styleIdx = i % playstyleCount;
        u8 currentStyle = playstyles[hudSlot];

        OS::Report("PlaystyleSelect: Button %d (hudSlot=%d, styleIdx=%d), currentStyle=%d\n", i, hudSlot, styleIdx, currentStyle);

        //per-vehicle playstyle names
        const u32 kartId = sectionParams->karts[hudSlot];
        if(kartId < 36) this->playstyleButtons[i].SetMessage(UI::BMG_PLAYSTYLE_NAMES + kartId * 4 + styleIdx);
        else OS::Report("PlaystyleSelect: invalid kart %d, keeping default label\n", kartId);

        if(hudSlot >= localPlayerCount) continue;
        if(styleIdx == currentStyle) {
            this->playstyleButtons[i].Select(hudSlot);
        }
        else {
            this->playstyleButtons[i].HandleDeselect(hudSlot, -1);
        }
    }
    OS::Report("PlaystyleSelect::OnActivate complete\n");
}

void PlaystyleSelect::AfterControlUpdate() {
    // Update instruction text based on selected button
}

void PlaystyleSelect::SetButtonHandlers(PushButton& button) {
    button.SetOnClickHandler(this->onButtonClickHandler, 0);
    button.SetOnSelectHandler(this->onButtonSelectHandler);
    button.SetOnDeselectHandler(this->onButtonDeselectHandler);
}

UIControl* PlaystyleSelect::CreateControl(u32 id) {
    if(id >= this->totalButtons || this->playstyleButtons == nullptr) return nullptr;
    const u32 count = this->controlCount;
    this->controlCount++;

    const SectionParams* sectionParams = SectionMgr::sInstance->sectionParams;
    u32 localPlayerCount = sectionParams->localPlayerCount;
    if(localPlayerCount == 0 || localPlayerCount > 4) localPlayerCount = 1;
    u32 hudSlot = id / this->playstyleCount;
    u32 styleIdx = id % this->playstyleCount;

    char layoutName[0x20];
    u32 layoutIdx;
    if(localPlayerCount >= 3) {
        snprintf(layoutName, sizeof(layoutName), "playstyle4");
        layoutIdx = 4;
    }
    else if(localPlayerCount == 2) {
        snprintf(layoutName, sizeof(layoutName), "playstyle2");
        layoutIdx = 2;
    }
    else {
        snprintf(layoutName, sizeof(layoutName), "playstyle1");
        layoutIdx = 1;
    }

    char variantName[0x20];
    snprintf(variantName, sizeof(variantName), "playstyle_%d_%d", layoutIdx, styleIdx);

    PushButton& button = this->playstyleButtons[id];
    this->AddControl(count, button, 0);
    OS::Report("PlaystyleSelect: Loading control %d (hudSlot=%d, styleIdx=%d, layout=%s, variant=%s)\n", id, hudSlot, styleIdx, layoutName, variantName);
    button.Load(UI::buttonFolder, layoutName, variantName, 1 << hudSlot, 0, false);
    button.buttonId = styleIdx;
    button.SetOnClickHandler(this->onButtonClickHandler, hudSlot);
    return &button;
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
    // Not used for this page
}

void PlaystyleSelect::OnButtonClick(PushButton& button, u32 hudSlotId) {
    if(button.buttonId == -100) { //on-screen back button
        OS::Report("PlaystyleSelect: Back button clicked\n");
        this->LoadPrevPageById(PAGE_DRIFT_SELECT, button);
        return;
    }

    OS::Report("PlaystyleSelect::OnButtonClick called, buttonId=%d, hudSlotId=%d\n", button.buttonId, hudSlotId);
    u32 styleIdx = button.buttonId;

    playstyles[hudSlotId] = static_cast<u8>(styleIdx);
    OS::Report("PlaystyleSelect: Set playstyle=%d for hudSlot=%d\n", styleIdx, hudSlotId);

    // Continue to cup select
    bool isBattle = IsBattle();
    PageId nextPage = isBattle ? PAGE_BATTLE_CUP_SELECT : PAGE_CUP_SELECT;
    OS::Report("PlaystyleSelect: isBattle=%d, nextPage=%d\n", isBattle, nextPage);

    this->LoadNextPageById(nextPage, button);
}

void PlaystyleSelect::OnButtonDeselect(PushButton& button, u32 hudSlotId) {
}

void PlaystyleSelect::OnBackPress(u32 hudSlotId) {
    OS::Report("PlaystyleSelect::OnBackPress called, hudSlotId=%d\n", hudSlotId);
    if(hudSlotId == 0) {
        this->nextPageId = PAGE_DRIFT_SELECT;
        this->EndStateAnimated(1, this->backButton.GetAnimationFrameSize());
        OS::Report("PlaystyleSelect: Going back to DriftSelect\n");
    }
}

void PlaystyleSelect::OnStartPress(u32 hudSlotId) {
    // Not used for this page
}

Page* PlaystyleSelect::GetPageById(PulPageId id) {
    ExpSection* section = ExpSection::GetSection();
    return section->GetPulPage<PlaystyleSelect>(id);
}

} // namespace UI
} // namespace Pulsar
