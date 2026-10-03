#include <UI/CustomVehicles/StyleArchives.hpp>
#include <UI/UI.hpp>
#include <UI/PlaystyleStatBars.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuKartModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Page/Other/ModelRenderer.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <core/egg/mem/Heap.hpp>
#include <include/c_string.h>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace UI {
namespace CustomVehicles {

//style whose menu archive is bound per hud slot; 0 loads vanilla
static u8 menuBoundStyle[4] = {0, 0, 0, 0};
//set when a reload is requested, until the rebuilt icons are bound again
static bool iconRebindPending = false;
//set once the icon bind latches were cleared, so the wait does not clear them every frame
static bool iconLatchesCleared = false;

//ArchiveMgr::allkartsModelsLoaders[hud], seen as the fields the style machinery needs
struct MenuArchiveLoader {
    void* vtable;            //+0x0
    EGG::ExpHeap* mountHeap; //+0x4
    EGG::ExpHeap* dumpHeap;  //+0x8
    s32 state;               //+0xc AllkartArchivesLoader::State
    s32 character;           //+0x10
};
//AllkartArchivesLoader::State
static const s32 LOADER_HAS_LOADED = 4;

//offset of the icon bind latch inside a ButtonMachine vehicle row button
static const u32 ICON_BOUND_LATCH = 0x25C;

//the game's own "rebuild this slot's menu models for this character" (0x805f570c)
typedef void (*LoadKartModelsByCharacterFunc)(Pages::ModelRenderer*, u32, CharacterId);
kmRuntimeUse(0x805f570c);
static LoadKartModelsByCharacterFunc const LoadKartModelsByCharacter =
    reinterpret_cast<LoadKartModelsByCharacterFunc>(kmRuntimeAddr(0x805f570c));

static MenuArchiveLoader* MenuLoaderForHud(u8 hud) {
    ArchiveMgr* mgr = ArchiveMgr::sInstance;
    if(mgr == nullptr || hud >= 4) return nullptr;
    return reinterpret_cast<MenuArchiveLoader*>(&mgr->allkartsModelsLoaders[hud]);
}

//character the menu models of this hud slot are loaded with, -1 when there is none yet
static s32 LoadedCharacter(u8 hud) {
    const MenuArchiveLoader* loader = MenuLoaderForHud(hud);
    if(loader == nullptr || loader->character < 0 || loader->character >= CHARACTER_COUNT) return -1;
    return loader->character;
}

static MenuDriverModel* DriverModelForHud(u8 hud) {
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if(modelMgr == nullptr || modelMgr->driverModels == nullptr || hud >= 4) return nullptr;
    return modelMgr->driverModels->players[hud].playerModel;
}

//---- page and player context ----

//kart select pages on which style cycling is active; battle is excluded
static bool IsKartSelectPageId(u32 id) {
    return id == PAGE_KART_SELECT || id == PAGE_MULTIPLAYER_KART_SELECT;
}

//top page of the kart select screen, null on every other page
static Page* KartSelectTopPage() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return nullptr;
    Page* top = mgr->curSection->GetTopLayerPage();
    if(top == nullptr || !IsKartSelectPageId(top->pageId)) return nullptr;
    return top;
}

static bool IsStyleSelectActive(const Page* top) {
    return top->currentState == 4 && !top->updateState; //STATE_ACTIVE
}

//local multiplayer picks styles on its own page, so the vehicle select screen never changes style there
static bool UsesPlaystyleSelectPage() {
    const ExpSection* section = ExpSection::GetSection();
    return section != nullptr
        && section->pulPages[PULPAGE_PLAYSTYLESELECT - PULPAGE_INITIAL] != nullptr;
}

//the vehicle select screen only changes style for a single local player; splitscreen does not
static bool IsSinglePlayerScreen(const SectionMgr& mgr) {
    return mgr.sectionParams->localPlayerCount < 2;
}

kmRuntimeUse(0x80830c64);
typedef void (*PrepareDriverOnKartAnmsFunc)(MenuDriverModelMgr*, u32);
static PrepareDriverOnKartAnmsFunc const RealPrepareDriverOnKartAnms =
    reinterpret_cast<PrepareDriverOnKartAnmsFunc>(kmRuntimeAddr(0x80830c64));

static void PrepareDriverOnKartAnmsHook(MenuDriverModelMgr* mgr, u32 hud) {
    RealPrepareDriverOnKartAnms(mgr, hud);
    if(hud >= 4) return;
    MenuDriverModel* driver = mgr->players[hud].playerModel;
    if(driver == nullptr) return;
    //the original leaves the driver on its character select transformator, so put it back on the kart
    if(driver->state != MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT && !iconRebindPending) return;
    driver->state = MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT; //SwitchState ignores an unchanged state
    driver->SwitchState(static_cast<u8>(hud), MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT);
}
kmCall(0x80832ea4, PrepareDriverOnKartAnmsHook);

//---- style input on the vehicle select screen ----

static ControllerType ControllerForHud(const SectionMgr& mgr, u8 hud) {
    if(hud >= 4) return GCN;
    const Input::RealControllerHolder* holder = mgr.pad.padInfos[hud].controllerHolder;
    if(holder == nullptr || holder->curController == nullptr) return GCN;
    const ControllerType type = holder->curController->GetType();
    return type <= GCN ? type : GCN;
}

//raw +/- buttons per controller type; indexed by ControllerType (WHEEL, NUNCHUCK, CLASSIC, GCN)
struct ToggleButtons {
    u16 prev;
    u16 next;
    u16 prevAction;
    u16 nextAction;
};
static const ToggleButtons TOGGLE_BUTTONS[4] = {
    {WPAD::WPAD_BUTTON_B, WPAD::WPAD_BUTTON_A, 1 << BACK_PRESS, 1 << FORWARD_PRESS},
    {WPAD::WPAD_BUTTON_1, WPAD::WPAD_BUTTON_2, 1 << BACK_PRESS, 1 << FORWARD_PRESS},
    {WPAD::WPAD_CL_TRIGGER_L, WPAD::WPAD_CL_TRIGGER_R, 0, 0},
    {PAD::PAD_BUTTON_L, PAD::PAD_BUTTON_R, 0, 0},
};

static void EatButton(Input::RealControllerHolder& holder, u16 button, u16 action) {
    holder.inputStates[0].buttonRaw &= static_cast<u16>(~button);
    holder.uiinputStates[0].rawButtons &= static_cast<u16>(~button);
    holder.uiinputStates[0].buttonActions &= static_cast<u16>(~action);
}

//state of player 1 on the vehicle select screen
struct KartSelectUi {
    u32 shownKart; //STYLE_TOOLTIP_STALE forces a tooltip refresh
    u8 shownStyle;
    u16 heldButtons; //toggle buttons currently held, for edge detection
};
static const u32 STYLE_TOOLTIP_STALE = 0xFFFFFFFF;
static KartSelectUi kartSelectUi = {STYLE_TOOLTIP_STALE, 0, 0};

//keep the style tooltip in sync with the hovered vehicle and the current style
static void SetStyleLabel(Pages::Menu& page, u32 kart, u8 style) {
    if(page.bottomText == nullptr || kart >= VEHICLE_COUNT) return;
    page.bottomText->SetMessage(BMG_PLAYSTYLE_NAMES + kart * STYLE_COUNT + style);
}

//cycles the style of player 1 on the shoulder buttons and mirrors it in the bottom text
static void ProcessStyleInput(const SectionMgr& mgr, Page* top) {
    if(!IsStyleSelectActive(top)) {
        //force a tooltip refresh on the next active frame
        kartSelectUi.shownKart = STYLE_TOOLTIP_STALE;
        return;
    }

    const u32 kart = StatBars::GetHoveredVehicle(0, mgr.sectionParams->karts[0]);
    if(kartSelectUi.shownKart != kart || kartSelectUi.shownStyle != playstyles[0]) {
        kartSelectUi.shownKart = kart;
        kartSelectUi.shownStyle = playstyles[0];
        SetStyleLabel(*reinterpret_cast<Pages::Menu*>(top), kart, playstyles[0]);
    }

    Input::RealControllerHolder* holder = mgr.pad.padInfos[0].controllerHolder;
    if(holder == nullptr || holder->curController == nullptr) {
        kartSelectUi.heldButtons = 0;
        return;
    }

    const ToggleButtons& toggle = TOGGLE_BUTTONS[ControllerForHud(mgr, 0)];
    const u16 inputs = holder->inputStates[0].buttonRaw;
    const u16 pressed = static_cast<u16>((inputs & (toggle.prev | toggle.next)) & ~kartSelectUi.heldButtons);
    kartSelectUi.heldButtons = static_cast<u16>(inputs & (toggle.prev | toggle.next));
    if((inputs & toggle.prev) != 0) EatButton(*holder, toggle.prev, toggle.prevAction);
    if((inputs & toggle.next) != 0) EatButton(*holder, toggle.next, toggle.nextAction);

    int step = 0;
    if((pressed & toggle.prev) != 0) step = -1;
    else if((pressed & toggle.next) != 0) step = 1;
    else return;

    //don't change style while a reload is in flight or its icons have not been bound again
    if(iconRebindPending) return;

    //cycle through all styles; vehicles without a style archive fall back to vanilla
    const u8 style = playstyles[0];
    const u32 next = (style + STYLE_COUNT + step) % STYLE_COUNT;
    if(next == style) return;

    playstyles[0] = static_cast<u8>(next);
    Audio::RSARPlayer::PlaySoundById(step > 0 ? SOUND_ID_RIGHT_ARROW_PRESS : SOUND_ID_LEFT_ARROW_PRESS, 0, nullptr);
    //stat bars reflect the newly selected playstyle immediately
    StatBars::RefreshGridBars(0, static_cast<KartId>(kart));
}

//---- menu archive binding ----

//menu style whose archive should actually be bound; 0 when custom archives are disabled or the style is missing
static u8 BoundMenuStyle(u32 character, u8 style) {
    if(style == 0 || !CustomArchivesEnabled()) return 0;
    return CharacterStyleArchiveExists(character, style) ? style : 0;
}

//latches an out-of-band selection (the multiplayer picker) into the bound style
void NoteMenuStyleSelected(u8 hud) {
    if(hud >= 4) return;
    const s32 character = LoadedCharacter(hud);
    if(character < 0) return;
    menuBoundStyle[hud] = BoundMenuStyle(static_cast<u32>(character), playstyles[hud] & 3);
}

//sets a randomised playstyle and latches the bound style of the new character
void NoteComboRandomisedStyle(u8 hud, u32 character, u8 style) {
    if(hud >= 4 || character >= CHARACTER_COUNT) return;
    playstyles[hud] = style & 3;
    menuBoundStyle[hud] = BoundMenuStyle(character, playstyles[hud]);
}

static bool MenuPathIsBattle(const char* path) {
    u32 len = 0;
    while(len < 128 && path[len] != '\0') ++len;
    return len >= 3 && path[len - 3] == '_' && path[len - 2] == 'B' && path[len - 1] == 'T';
}

//match the "<name>" in "Scene/Model/Kart/<name>-allkart[_BT]" back to a character id
static bool MenuPathCharacter(const char* path, u32& character) {
    static const char prefix[] = "Scene/Model/Kart/";
    static const char suffix[] = "-allkart";
    static const u32 prefixLen = sizeof(prefix) - 1;
    u32 i = 0;
    while(i < prefixLen && path[i] == prefix[i]) ++i;
    if(i != prefixLen) return false;
    for(u32 c = 0; c < CHARACTER_COUNT; ++c) {
        const char* name = CHARACTER_NAMES[c];
        if(name == nullptr) continue;
        u32 n = 0;
        while(name[n] != '\0' && path[prefixLen + n] == name[n]) ++n;
        if(name[n] != '\0') continue;
        u32 s = 0;
        while(s < 8 && path[prefixLen + n + s] == suffix[s]) ++s;
        if(s != 8) continue;
        character = c;
        return true;
    }
    return false;
}

kmRuntimeUse(0x8052A954);
typedef void (*LoadArchivesFunc)(ArchivesHolder*, const char*, EGG::Heap*, EGG::Heap*, u32*);
static LoadArchivesFunc const RealLoadArchives = reinterpret_cast<LoadArchivesFunc>(kmRuntimeAddr(0x8052A954));

//race kart archives are mounted from a job context path buffer of this size (mkw-pal.c strncpy 0x40)
static const u32 RACE_PATH_CAPACITY = 0x40;

//which race player a kart archive holder belongs to, in either holder array; NO_HOLDER if neither
#define NO_HOLDER 0xFF
static u8 PlayerIdForKartArchive(const ArchiveMgr* mgr, const ArchivesHolder* holder) {
    if(mgr == nullptr || holder == nullptr) return NO_HOLDER;
    const u8* given = reinterpret_cast<const u8*>(holder);
    const u8* base = reinterpret_cast<const u8*>(mgr->kartModelsHolders);
    const u32 arraySize = sizeof(mgr->kartModelsHolders);
    if(given >= base && given < base + arraySize) {
        const u32 offset = static_cast<u32>(given - base);
        if(offset % sizeof(ArchivesHolder) == 0) return static_cast<u8>(offset / sizeof(ArchivesHolder));
    }
    const u8* base2 = reinterpret_cast<const u8*>(mgr->kartModelsHolders2);
    if(given >= base2 && given < base2 + arraySize) {
        const u32 offset = static_cast<u32>(given - base2);
        if(offset % sizeof(ArchivesHolder) == 0) return static_cast<u8>(offset / sizeof(ArchivesHolder));
    }
    return NO_HOLDER;
}

static bool StartsWith(const char* path, const char* prefix) {
    return strncmp(path, prefix, strlen(prefix)) == 0;
}

//menu archives live in the first holder array only, one per local player
static void MenuArchivePathHook(char* path, u8 hud) {
    if(!StartsWith(path, "Scene/Model/Kart/")) return;
    if(MenuPathIsBattle(path)) return;
    const u8 style = menuBoundStyle[hud] & 3;
    //a stale bound style can never leak a styled path while custom archives are off
    if(style == 0 || !CustomArchivesEnabled()) return;
    u32 character = 0;
    if(!MenuPathCharacter(path, character) || !CharacterStyleArchiveExists(character, style)) return;
    const char* postfix = CharacterStylePostfix(character, style);
    if(postfix != nullptr) snprintf(path, 128, "Scene/Model/Kart/%s-allkart", postfix);
}

//every kart archive the game mounts passes here, for the menus and for the race
static void KartArchiveLoadHook(ArchivesHolder* holder, char* path, EGG::Heap* mountHeap, EGG::Heap* fileHeap,
    u32* size) {
    ArchiveMgr* mgr = ArchiveMgr::sInstance;
    if(mgr != nullptr && path != nullptr) {
        const u8 playerId = PlayerIdForKartArchive(mgr, holder);
        if(playerId != NO_HOLDER) {
            if(StartsWith(path, "Race/Kart/")) {
                RewriteRaceArchivePath(path, playerId, RACE_PATH_CAPACITY);
            }
            else if(playerId < 4 && holder == &mgr->kartModelsHolders[playerId]) {
                MenuArchivePathHook(path, playerId);
            }
        }
    }
    RealLoadArchives(holder, path, mountHeap, fileHeap, size);
}
kmCall(0x805411b8, KartArchiveLoadHook);
kmCall(0x80541FB8, KartArchiveLoadHook);
//sync variant loader's load call (same register convention)
kmCall(0x80542198, KartArchiveLoadHook);
//job 5 (the kart archives) ends in a tail branch, so this one must stay a branch
kmBranch(0x80540084, KartArchiveLoadHook);

//the styled menu archives are larger than the vanilla ones, so the loaders get a bigger heap
kmRuntimeUse(0x80226AC8);
typedef EGG::ExpHeap* (*ExpHeapCreateFunc)(int size, EGG::Heap* parent, u16 flags);
static ExpHeapCreateFunc const RealExpHeapCreate = reinterpret_cast<ExpHeapCreateFunc>(kmRuntimeAddr(0x80226AC8));
static EGG::ExpHeap* ExpHeapCreateHook(int size, EGG::Heap* parent, u16 flags) {
    if(size == 0xc8000 || size == 0xe1000) {
        size = 0x180000;
    }
    return RealExpHeapCreate(size, parent, flags);
}
kmCall(0x80542304, ExpHeapCreateHook);
kmCall(0x8054233c, ExpHeapCreateHook);

//---- vehicle row icons ----

static bool IsVehicleIconButton(UIControl* control) {
    if(control == nullptr || control->isHidden) return false;
    const char* className = control->GetClassName();
    if(className == nullptr || strstr(className, "Button") == nullptr) return false;
    //the vehicle rows mark their buttons with a "hatena" pane
    return static_cast<LayoutUIControl*>(control)->layout.GetPaneByName("hatena") != nullptr;
}

enum IconWalk {
    ICON_CLEAR_LATCH, //a reload frees the icon textures, so the game has to bind them again
    ICON_CHECK_BOUND
};

//walks every vehicle icon of the kart select rows, clearing or reading their bind latch
static bool WalkVehicleSelectIcons(IconWalk walk) {
    Page* top = KartSelectTopPage();
    if(top == nullptr) return true;
    bool allBound = true;
    for(u8 col = 1; col <= 2; ++col) {
        UIControl* column = top->controlGroup.GetControl(col);
        if(column == nullptr || column->isHidden) continue;
        const ControlGroup& rows = column->childrenGroup;
        for(u32 slot = 0; slot < rows.controlCount; ++slot) {
            UIControl* row = rows.GetControl(slot);
            if(row == nullptr || row->isHidden) continue;
            const ControlGroup& buttons = row->childrenGroup;
            for(u32 b = 0; b < buttons.controlCount; ++b) {
                UIControl* button = buttons.GetControl(b);
                if(button == nullptr || !IsVehicleIconButton(button)) continue;
                u8* latch = reinterpret_cast<u8*>(button) + ICON_BOUND_LATCH;
                if(walk == ICON_CLEAR_LATCH) *latch = 0;
                else if(*latch == 0) allBound = false;
            }
        }
    }
    return allBound;
}

//---- per-frame driver ----

//waits for the game's own per-frame rebuild, then lets it bind the row icons again
static void ProcessIconRebind() {
    if(!iconRebindPending) {
        iconLatchesCleared = false;
        return;
    }
    //MenuKartModelMgr::Load locks the models once the archive is mounted and the rebuild is done
    const MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if(modelMgr == nullptr || modelMgr->kartModels == nullptr) return;
    const MenuArchiveLoader* loader = MenuLoaderForHud(0);
    if(loader == nullptr || loader->state != LOADER_HAS_LOADED) return;
    const MenuKartModelMgr::Player& slot = modelMgr->kartModels->players[0];
    if(slot.hasLoadRequest || !slot.isLocked) return;

    if(!iconLatchesCleared) {
        //the game binds a row icon in OnUpdate only while its latch is clear, and never clears it itself
        WalkVehicleSelectIcons(ICON_CLEAR_LATCH);
        iconLatchesCleared = true;
    }
    if(WalkVehicleSelectIcons(ICON_CHECK_BOUND)) {
        iconRebindPending = false;
        iconLatchesCleared = false;
    }
}

//requests a reload whenever the style to load no longer matches the bound one
static void ProcessMenuRebindRequests() {
    const SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return;
    Pages::ModelRenderer* renderer = mgr->curSection->Get<Pages::ModelRenderer>();
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    MenuDriverModel* driver = DriverModelForHud(0);
    if(renderer == nullptr || driver == nullptr || modelMgr == nullptr || modelMgr->kartModels == nullptr) return;
    const s32 character = LoadedCharacter(0);
    if(character < 0) return;
    //missing styles and disabled custom archives load vanilla; reload when the loaded archive differs
    const u8 loadStyle = BoundMenuStyle(static_cast<u32>(character), playstyles[0] & 3);
    if(loadStyle == menuBoundStyle[0] || iconRebindPending) return;

    //the reload frees the archive heap on the task thread, so move the driver off its on-kart
    //transformator, which lives in that heap, before asking for it
    ModelTransformator* onKartTransformator = driver->onKartTransformator;
    driver->onKartTransformator = nullptr;
    driver->SwitchState(0, MenuDriverModel::MENUDRIVERMODEL_STATE_ONCHARSELECT);

    //the game's own entry point keeps the page params in sync, so the kart is re-selected afterwards
    LoadKartModelsByCharacter(renderer, 0, static_cast<CharacterId>(character));
    if(!modelMgr->kartModels->players[0].hasLoadRequest) {
        //it declined, e.g. a request is already in flight: leave the driver on its kart
        driver->onKartTransformator = onKartTransformator;
        driver->SwitchState(0, MenuDriverModel::MENUDRIVERMODEL_STATE_ONKARTSELECT);
        return;
    }

    menuBoundStyle[0] = loadStyle;
    iconRebindPending = true;
}

void MenuStyleUpdate() {
    //runs on every page so a reload that survives a page change still settles its icons
    ProcessIconRebind();
    //local multiplayer picks its styles on the PlaystyleSelect page: no menu asset switching there
    if(UsesPlaystyleSelectPage()) return;
    const SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->sectionParams == nullptr) return;
    if(!IsSinglePlayerScreen(*mgr)) return;
    Page* top = KartSelectTopPage();
    if(top == nullptr) return;
    ProcessStyleInput(*mgr, top);
    ProcessMenuRebindRequests();
}

}//namespace CustomVehicles

//menu update wrapper: poll the style input just before the menu pipeline runs
void MenuSceneUpdateHook(SectionMgr& mgr) {
    CustomVehicles::MenuStyleUpdate();
    mgr.MenuUpdate();
}
kmCall(0x805552e8, MenuSceneUpdateHook);
kmCall(0x80553b30, MenuSceneUpdateHook);

}//namespace UI
}//namespace Pulsar