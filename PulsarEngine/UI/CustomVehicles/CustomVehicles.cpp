#include <UI/CustomVehicles/CustomVehicles.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <core/rvl/dvd/dvd.hpp>

namespace Pulsar {
namespace UI {

u8 playstyles[4] = {0, 0, 0, 0};

namespace CustomVehicles {

//vanilla data tables
static const char** vehicleNames = reinterpret_cast<const char**>(0x808b3b50);
static const char* const* characterNames = reinterpret_cast<const char* const*>(0x808b3a90);

//"<vanillaName>-<style>" postfixes, generated once
static char generatedPostfixes[36][STYLE_COUNT][16];

//tri-state existence cache: 0 unknown, 1 missing, 2 exists; keyed kart/style/character
static u8 styleExists[36][STYLE_COUNT][0x30];

//kart select pages on which cycling is active
static bool IsStylePageId(u32 id) {
    return id == PAGE_KART_SELECT || id == PAGE_BATTLE_KART_SELECT || id == PAGE_MULTIPLAYER_KART_SELECT;
}

static bool IsStyleSelectActive(const SectionMgr& mgr) {
    if(mgr.curSection == nullptr) return false;
    Page* top = mgr.curSection->GetTopLayerPage();
    if(top == nullptr || !IsStylePageId(top->pageId)) return false;
    return top->currentState == 4 && !top->updateState; //STATE_ACTIVE
}

//"ma_bike-2" etc.; null for style 0 / invalid input
static const char* GeneratedVehiclePostfix(u32 kart, u32 style) {
    if(kart >= 36 || style == 0 || style >= STYLE_COUNT) return nullptr;
    char* postfix = generatedPostfixes[kart][style];
    if(postfix[0] != '\0') return postfix;
    const char* base = vehicleNames[kart];
    if(base == nullptr) return nullptr;
    if(snprintf(postfix, 16, "%s-%u", base, style) <= 0) {
        postfix[0] = '\0';
        return nullptr;
    }
    return postfix;
}

//probes the race archive for this (vehicle, style, character) combo
static bool VehicleStyleFileExists(u32 kart, u32 style, CharacterId character) {
    if(kart >= 36 || character >= 0x30 || style == 0 || style >= STYLE_COUNT) return false;
    u8& cached = styleExists[kart][style][character];
    if(cached != 0) return cached == 2;
    const char* postfix = GeneratedVehiclePostfix(kart, style);
    const char* charName = characterNames[character];
    bool exists = false;
    if(postfix != nullptr && charName != nullptr) {
        char path[0x60];
        if(snprintf(path, sizeof(path), "/Race/Kart/%s-%s.szs", postfix, charName) > 0) {
            DVD::FileInfo info;
            if(DVD::Open(path, &info)) {
                exists = info.length != 0;
                DVD::Close(&info);
            }
        }
    }
    cached = exists ? 2 : 1;
    return exists;
}

//style to use for a race player; vanilla when unset or files missing
static u8 RaceStyleForPlayer(u8 playerId, u32 kart, CharacterId character) {
    if(playerId >= 4) return 0;
    const u8 style = playstyles[playerId];
    if(style == 0) return 0;
    if(!VehicleStyleFileExists(kart, style, character)) return 0;
    return style;
}

static ControllerType ControllerForHud(const SectionMgr& mgr, u8 hud) {
    if(hud >= 4) return GCN;
    const Input::RealControllerHolder* holder = mgr.pad.padInfos[hud].controllerHolder;
    if(holder == nullptr || holder->curController == nullptr) return GCN;
    const ControllerType type = holder->curController->GetType();
    return type == WHEEL || type == NUNCHUCK || type == CLASSIC || type == GCN ? type : GCN;
}

//is this hudSlot currently the one picking its vehicle? byte inside ControlsManipulatorManager
static bool IsHudChoosingVehicle(Page* page, u8 hud) {
    if(hud >= 4) return false;
    enum { PLAYER_STATE_SIZE = 0x5c, IS_PER_CONTROL_OFFSET = 0xa4 };
    const u8* manager = reinterpret_cast<const u8*>(page) + 0x430 + IS_PER_CONTROL_OFFSET;
    return manager[hud * PLAYER_STATE_SIZE] != 0;
}

//raw +/- buttons, edge detected, eaten so menus never see them
static void ToggleInputs(ControllerType type, u16& prevButton, u16& nextButton, u16& prevAction, u16& nextAction) {
    prevAction = 0;
    nextAction = 0;
    switch(type) {
        case WHEEL:
            prevButton = WPAD::WPAD_BUTTON_B;
            nextButton = WPAD::WPAD_BUTTON_A;
            prevAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case NUNCHUCK:
            prevButton = WPAD::WPAD_BUTTON_1;
            nextButton = WPAD::WPAD_BUTTON_2;
            prevAction = static_cast<u16>(1 << BACK_PRESS);
            nextAction = static_cast<u16>(1 << FORWARD_PRESS);
            break;
        case CLASSIC:
            prevButton = WPAD::WPAD_CL_TRIGGER_L;
            nextButton = WPAD::WPAD_CL_TRIGGER_R;
            break;
        case GCN:
        default:
            prevButton = PAD::PAD_BUTTON_L;
            nextButton = PAD::PAD_BUTTON_R;
            break;
    }
}

static void EatButton(Input::RealControllerHolder& holder, u16 button, u16 action) {
    holder.inputStates[0].buttonRaw &= static_cast<u16>(~button);
    holder.uiinputStates[0].rawButtons &= static_cast<u16>(~button);
    holder.uiinputStates[0].buttonActions &= static_cast<u16>(~action);
}

static void ShowStyleLabel(Pages::Menu& page, u8 hud, u32 kart, u32 style) {
    if(page.bottomText == nullptr) return;
    if(kart < 36) page.bottomText->SetMessage(BMG_PLAYSTYLE_NAMES + kart * 4 + style);
}

//vehicle currently being previewed on the kart select page (updates while browsing,
//unlike sectionParams->karts[] which only updates on confirm)
static u32 GetHoveredKart(const SectionMgr& mgr, u8 hud) {
    const u32 confirmed = mgr.sectionParams->karts[hud];
    const MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if(modelMgr == nullptr || !modelMgr->isActive || modelMgr->kartModels == nullptr) return confirmed;

    //MenuKartModelMgr layout: players[hud] @ +hud*0x10 {idx @ +0xc, readyFlag @ +0x11};
    //modelCount byte @ +0x4; models array ptr @ +0x8, entries 0x2c apart, kartId @ +0x8
    const u8* kartModels = reinterpret_cast<const u8*>(modelMgr->kartModels);
    if(*(const u8*)(kartModels + hud * 0x10 + 0x11) == 0) return confirmed; //previews not built yet
    const u32 idx = *reinterpret_cast<const u32*>(kartModels + hud * 0x10 + 0xc);
    const u32 count = *(const u8*)(kartModels + 4);
    if(idx >= count) return confirmed;
    const u8* modelsArray = *reinterpret_cast<const u8* const*>(kartModels + 8);
    if(modelsArray == nullptr) return confirmed;
    const u32 kartId = *reinterpret_cast<const u32*>(modelsArray + idx * 0x2c + 8);
    return kartId < 36 ? kartId : confirmed;
}

void ProcessStyleInput() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr || mgr->sectionParams == nullptr) return;

    //where the dedicated PlaystyleSelect page exists (local multiplayer), R/L cycling
    //on the kart grid is disabled: the page is the only style picker there
    const ExpSection* section = ExpSection::GetSection();
    if(section != nullptr
        && section->pulPages[PULPAGE_PLAYSTYLESELECT - PULPAGE_INITIAL] != nullptr) return;

    Page* top = mgr->curSection->GetTopLayerPage();
    if(top == nullptr || !IsStylePageId(top->pageId)) return;

    Pages::Menu& page = *reinterpret_cast<Pages::Menu*>(top);
    const bool active = IsStyleSelectActive(*mgr);

    //keep the style tooltip in sync with the hovered vehicle and current style
    static u32 shownKarts[4] = {0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF};
    static u8 shownStyles[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    if(!active) {
        for(u8 i = 0; i < 4; ++i) {
            shownKarts[i] = 0xFFFFFFFF;
            shownStyles[i] = 0xFF;
        }
    }
    else {
        u32 hudCount = mgr->sectionParams->localPlayerCount;
        if(hudCount == 0 || hudCount > 4) hudCount = 1;
        for(u8 hud = 0; hud < hudCount; ++hud) {
            //in multiplayer the shared bottom text follows the currently choosing player
            if(hudCount > 1 && !IsHudChoosingVehicle(top, hud)) continue;
            const u32 kart = GetHoveredKart(*mgr, hud);
            const u8 style = playstyles[hud];
            if(kart == shownKarts[hud] && style == shownStyles[hud]) continue;
            shownKarts[hud] = kart;
            shownStyles[hud] = style;
            ShowStyleLabel(page, hud, kart, style);
        }
    }

    static u16 heldToggleButtons[4] = {0, 0, 0, 0};

    u32 count = mgr->sectionParams->localPlayerCount;
    if(count == 0 || count > 4) count = 1;

    for(u8 hud = 0; hud < count; ++hud) {
        const u32 kart = GetHoveredKart(*mgr, hud);
        Input::RealControllerHolder* holder = mgr->pad.padInfos[hud].controllerHolder;
        if(holder == nullptr || holder->curController == nullptr) {
            heldToggleButtons[hud] = 0;
            continue;
        }
        //in multiplayer only the currently choosing player may cycle
        if(count > 1 && !IsHudChoosingVehicle(top, hud)) {
            heldToggleButtons[hud] = 0;
            continue;
        }

        u16 prevButton = 0;
        u16 nextButton = 0;
        u16 prevAction = 0;
        u16 nextAction = 0;
        ToggleInputs(ControllerForHud(*mgr, hud), prevButton, nextButton, prevAction, nextAction);

        const u16 inputs = holder->inputStates[0].buttonRaw;
        const u16 pressed = static_cast<u16>((inputs & (prevButton | nextButton)) & ~heldToggleButtons[hud]);
        heldToggleButtons[hud] = static_cast<u16>(inputs & (prevButton | nextButton));
        if((inputs & prevButton) != 0) EatButton(*holder, prevButton, prevAction);
        if((inputs & nextButton) != 0) EatButton(*holder, nextButton, nextAction);

        int step = 0;
        if((pressed & prevButton) != 0) step = -1;
        else if((pressed & nextButton) != 0) step = 1;
        else continue;

        //cycle through all styles unconditionally; vehicles without custom files
        //for a style simply fall back to vanilla looks (RaceStyleForPlayer)
        u32 style = playstyles[hud];
        style = (style + STYLE_COUNT + step) % STYLE_COUNT;
        if(style == playstyles[hud]) continue;

        playstyles[hud] = static_cast<u8>(style);
        Audio::RSARPlayer::PlaySoundById(step > 0 ? SOUND_ID_RIGHT_ARROW_PRESS : SOUND_ID_LEFT_ARROW_PRESS, 0, nullptr);
    }
}

}//namespace CustomVehicles

//menu update wrappers: poll style input just before the menu pipeline runs
void MenuSceneUpdateHook(SectionMgr& mgr) {
    CustomVehicles::ProcessStyleInput();
    mgr.MenuUpdate();
}
kmCall(0x805552e8, MenuSceneUpdateHook);
kmCall(0x80553b30, MenuSceneUpdateHook);

namespace CustomVehicles {

//race model loading: temporarily swap the vehicle name entry so vanilla builds
//the "<vehicle>-<style>" archive paths itself, then restore
static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character,
    u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    const u8 style = RaceStyleForPlayer(playerId, kart, character);
    const char** entry = &vehicleNames[kart];
    const char* old = *entry;
    if(style != 0) *entry = GeneratedVehiclePostfix(kart, style);
    ArchivesHolder* holder = archiveMgr->LoadKartArchive(playerId, kart, character, color, type, decompressedHeap, archiveHeap);
    *entry = old;
    return holder;
}
kmCall(0x805540f4, LoadKartArchiveHook);

static ArchivesHolder* LoadBackupKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character,
    u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    const u8 style = RaceStyleForPlayer(playerId, kart, character);
    const char** entry = &vehicleNames[kart];
    const char* old = *entry;
    if(style != 0) *entry = GeneratedVehiclePostfix(kart, style);
    ArchivesHolder* holder = archiveMgr->LoadKartArchiveHolder2(playerId, kart, character, color, 0, decompressedHeap, archiveHeap);
    *entry = old;
    return holder;
}
kmCall(0x80554198, LoadBackupKartArchiveHook);

//styled archives can lack the star-color material data vanilla karts have; applying
//them derefs null during race init. Ported from rr-pulsar: only apply when loaded.
static void SetModelColorsIfReady(void* starAnm, void* drawMdl) {
    if(drawMdl == nullptr) return;
    const u8* model = static_cast<const u8*>(drawMdl);
    if(*reinterpret_cast<void* const*>(model + 0x10) == nullptr) return;
    for(u32 i = 0; i < 2; ++i) {
        if(*reinterpret_cast<void* const*>(model + 0x14 + i * sizeof(void*)) == nullptr) return;
    }
    reinterpret_cast<void (*)(void*, void*)>(0x8056be20)(starAnm, drawMdl);
}
kmCall(0x80592e24, SetModelColorsIfReady);
kmCall(0x80592e40, SetModelColorsIfReady);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar
