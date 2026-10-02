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
//set while a style-triggered archive reload is in flight
static bool menuReloadOutstanding[4] = {false, false, false, false};
//set once a reload was requested so the row icons re-bind the following frame
static bool iconsRebindPending = false;
//set between requesting the icon re-bind and every icon having its bind flag back
static bool iconsRebound = false;

//ArchiveMgr::allkartsModelsLoaders[hud], seen as the fields the style machinery needs
struct MenuArchiveLoader {
    void* vtable;            //+0x0
    EGG::ExpHeap* mountHeap; //+0x4
    EGG::ExpHeap* dumpHeap;  //+0x8
    s32 state;               //+0xc AllkartArchivesLoader::State
    s32 character;           //+0x10
    s32 mode;                //+0x14 0 for vs, 2 for battle
};
//AllkartArchivesLoader::State
static const s32 LOADER_HAS_REQUEST = 0;
static const s32 LOADER_HAS_LOADED = 4;

//MenuDriverModel states as the game uses them
static const u32 MENU_DRIVER_STATE_CHARSEL = 0;
static const u32 MENU_DRIVER_STATE_ONKART = 2;
//visibility bit of MenuDriverModel::model
static const u32 DRIVER_MODEL_VISIBLE = 0x100000;
//offset of the bind flag inside a vehicle row button
static const u32 BUTTON_BOUND_FLAG = 0x25C;

kmRuntimeUse(0x80542210);
typedef bool (*RequestMenuReloadFunc)(ArchiveMgr*, u8, u32, u32);
static RequestMenuReloadFunc const RequestMenuReload = reinterpret_cast<RequestMenuReloadFunc>(kmRuntimeAddr(0x80542210));

kmRuntimeUse(0x808478F4);
typedef u8 (*ButtonBindFunc)(UIControl*);
static ButtonBindFunc const RealButtonBindTexture = reinterpret_cast<ButtonBindFunc>(kmRuntimeAddr(0x808478F4));

kmRuntimeUse(0x80830c64);
typedef void (*PrepareDriverOnKartAnmsFunc)(MenuDriverModelMgr*, u32);
static PrepareDriverOnKartAnmsFunc const RealPrepareDriverOnKartAnms =
    reinterpret_cast<PrepareDriverOnKartAnmsFunc>(kmRuntimeAddr(0x80830c64));

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

static MenuDriverModelMgr* DriverModels() {
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if(modelMgr == nullptr || modelMgr->driverModels == nullptr) return nullptr;
    return modelMgr->driverModels;
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

    //don't change style while a reload is in flight or its icon rebind hasn't settled yet
    if(menuReloadOutstanding[0] || iconsRebindPending) return;

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

//the kart select menus load their allkart archive into kartModelsHolders[hud]
static u8 HudForMenuArchive(const ArchiveMgr* mgr, const ArchivesHolder* holder) {
    if(mgr == nullptr || holder == nullptr) return 4; //out of range: leave the path alone
    const u8* base = reinterpret_cast<const u8*>(mgr->kartModelsHolders);
    const u8* given = reinterpret_cast<const u8*>(holder);
    if(given < base) return 4;
    const u32 offset = static_cast<u32>(given - base);
    if(offset % sizeof(ArchivesHolder) != 0) return 4;
    const u32 hud = offset / sizeof(ArchivesHolder);
    return hud < 4 ? static_cast<u8>(hud) : 4;
}

//substitutes the styled archive path into menu allkart loads; anything else loads vanilla
static void MenuArchiveLoadHook(ArchivesHolder* holder, char* path, EGG::Heap* mountHeap, EGG::Heap* dumpHeap,
    u32* size) {
    ArchiveMgr* mgr = ArchiveMgr::sInstance;
    const u8 hud = HudForMenuArchive(mgr, holder);
    if(mgr != nullptr && path != nullptr && hud < 4 && !MenuPathIsBattle(path)) {
        const u8 style = menuBoundStyle[hud] & 3;
        //custom archives off: never substitute a styled path
        if(style != 0 && CustomArchivesEnabled()) {
            u32 character = 0;
            if(MenuPathCharacter(path, character) && CharacterStyleArchiveExists(character, style)) {
                const char* postfix = CharacterStylePostfix(character, style);
                if(postfix != nullptr) {
                    snprintf(path, 128, "Scene/Model/Kart/%s-allkart", postfix);
                }
            }
        }
    }
    RealLoadArchives(holder, path, mountHeap, dumpHeap, size);
}
kmCall(0x805411b8, MenuArchiveLoadHook);
kmCall(0x80541FB8, MenuArchiveLoadHook);
//sync variant loader's load call (same register convention)
kmCall(0x80542198, MenuArchiveLoadHook);

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

//---- driver visibility across a reload ----

//a reload hides the driver until the styled models are locked again
static void ShowDriver(u8 hud) {
    MenuDriverModelMgr* drivers = DriverModels();
    if(drivers == nullptr) return;
    MenuDriverModel* driver = drivers->players[hud].playerModel;
    if(driver != nullptr && driver->model != nullptr) driver->model->bitfield |= DRIVER_MODEL_VISIBLE;
    drivers->players[hud].isVisible = true;
}

//stops the old on-kart anms and binds the stable charSel transformator
static void HideDriverForReload(MenuDriverModelMgr* drivers, u8 hud) {
    MenuDriverModel* driver = drivers->players[hud].playerModel;
    driver->onKartTransformator = nullptr;
    driver->SwitchState(hud, static_cast<MenuDriverModel::State>(MENU_DRIVER_STATE_CHARSEL));
    drivers->players[hud].isVisible = false;
    driver->model->ToggleVisible(false);
    driver->model->bitfield &= ~DRIVER_MODEL_VISIBLE;
}

//a reload that will never finish must not leave the driver hidden
static void AbortMenuReload(u8 hud) {
    ShowDriver(hud);
    menuReloadOutstanding[hud] = false;
    iconsRebindPending = false;
}

//restores in the same frame as the rebuild, which ends with prepareDriverOnKartAnms
static void FinishMenuReload(u8 hud) {
    ShowDriver(hud);
    MenuDriverModelMgr* drivers = DriverModels();
    if(drivers == nullptr) return;
    MenuDriverModel* driver = drivers->players[hud].playerModel;
    if(driver != nullptr && driver->onKartTransformator != nullptr && KartSelectTopPage() != nullptr) {
        driver->SwitchState(hud, static_cast<MenuDriverModel::State>(MENU_DRIVER_STATE_ONKART));
    }
    menuReloadOutstanding[hud] = false;
}

static void PrepareDriverOnKartAnmsHook(MenuDriverModelMgr* mgr, u32 hud) {
    RealPrepareDriverOnKartAnms(mgr, hud);
    if(hud < 4 && menuReloadOutstanding[hud]) FinishMenuReload(static_cast<u8>(hud));
}
kmCall(0x80832ea4, PrepareDriverOnKartAnmsHook);

//---- vehicle row icons ----

static bool IsVehicleIconButton(UIControl* control) {
    if(control == nullptr || control->isHidden) return false;
    const char* className = control->GetClassName();
    if(className == nullptr || strstr(className, "Button") == nullptr) return false;
    //the vehicle rows mark their buttons with a "hatena" pane
    return static_cast<LayoutUIControl*>(control)->layout.GetPaneByName("hatena") != nullptr;
}

//re-binds the kart select row icons and reports whether every one of them is bound
static bool PollVehicleSelectIcons(bool rebind) {
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
                if(rebind) {
                    const u8 bound = RealButtonBindTexture(button);
                    //0 lets the game's OnUpdate re-bind the button once its icon has streamed in
                    *(reinterpret_cast<u8*>(button) + BUTTON_BOUND_FLAG) = bound;
                }
                if(*(reinterpret_cast<u8*>(button) + BUTTON_BOUND_FLAG) == 0) allBound = false;
            }
        }
    }
    return allBound;
}

//---- per-frame driver ----

//finishes in-flight reloads on every page, so one that survives a page change still restores its driver
static void ProcessMenuRebindRestore() {
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    const bool modelsLoaded = modelMgr != nullptr && modelMgr->driverModels != nullptr
        && modelMgr->kartModels != nullptr;
    for(u8 hud = 0; hud < 4; ++hud) {
        if(!menuReloadOutstanding[hud] || !modelsLoaded) continue;
        const MenuArchiveLoader* loader = MenuLoaderForHud(hud);
        if(loader == nullptr) continue;
        //no request in flight: abandon the restore
        if(loader->state == LOADER_HAS_REQUEST) {
            AbortMenuReload(hud);
            continue;
        }
        if(loader->state != LOADER_HAS_LOADED || !modelMgr->kartModels->players[hud].isLocked) continue;
        FinishMenuReload(hud);
    }

    //no reload to settle: drop the one-shot rebind flag again
    if(!iconsRebindPending) {
        iconsRebound = false;
        return;
    }
    //re-link the row icons once the regenerated models are locked, then keep gating style input until they are bound
    if(!modelsLoaded || !modelMgr->kartModels->players[0].isLocked) return;
    const MenuArchiveLoader* loader = MenuLoaderForHud(0);
    if(loader == nullptr || loader->state != LOADER_HAS_LOADED) return;
    if(!iconsRebound) {
        PollVehicleSelectIcons(true);
        iconsRebound = true;
    }
    if(PollVehicleSelectIcons(false)) {
        iconsRebindPending = false;
        iconsRebound = false;
    }
}

//requests a reload whenever the style to load no longer matches the bound one
static void ProcessMenuRebindRequests() {
    ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    MenuDriverModelMgr* drivers = DriverModels();
    if(archiveMgr == nullptr || drivers == nullptr || modelMgr == nullptr || modelMgr->kartModels == nullptr) return;
    const MenuArchiveLoader* loader = MenuLoaderForHud(0);
    const s32 character = LoadedCharacter(0);
    if(loader == nullptr || loader->mountHeap == nullptr || character < 0) return;
    if(drivers->players[0].playerModel == nullptr) return;
    //missing styles and disabled custom archives load vanilla; reload when the loaded archive differs
    const u8 loadStyle = BoundMenuStyle(static_cast<u32>(character), playstyles[0] & 3);
    if(loadStyle == menuBoundStyle[0] || iconsRebindPending) return;

    HideDriverForReload(drivers, 0);
    menuReloadOutstanding[0] = true;
    modelMgr->ResetKartModels(0);
    if(!RequestMenuReload(archiveMgr, 0, static_cast<u32>(character), static_cast<u32>(loader->mode))) {
        AbortMenuReload(0);
        return;
    }
    menuBoundStyle[0] = loadStyle;
    iconsRebindPending = true;
}

void MenuStyleUpdate() {
    //the restore poll runs on every page so a reload can never leave a driver hidden
    ProcessMenuRebindRestore();
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