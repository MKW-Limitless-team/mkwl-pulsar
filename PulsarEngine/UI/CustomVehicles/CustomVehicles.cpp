#include <UI/CustomVehicles/CustomVehicles.hpp>
#include <UI/UI.hpp>
#include <UI/PlaystyleStatBars.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuModelMgr.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuKartModel.hpp>
#include <MarioKartWii/3D/Model/Menu/MenuDriverModel.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/UI/Page/Page.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/Input/ControllerHolder.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Audio/RSARPlayer.hpp>
#include <core/rvl/dvd/dvd.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/egg/mem/Heap.hpp>

namespace Pulsar {
namespace UI {

u8 playstyles[4] = {0, 0, 0, 0};
u8 ghostPlaystyles[4] = {0, 0, 0, 0};
u8 cpuPlaystyles[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

namespace CustomVehicles {

//vanilla data tables
extern "C" const char* VEHICLE_NAMES[36];
extern "C" const char* CHARACTER_NAMES[48];
extern "C" void SetModelColorsImpl(void*, void*);

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

//player count for the current context: sectionParams offline, room sub online (1 there even with 2 local players)
static u32 GetEffectiveLocalPlayerCount(const SectionMgr& mgr) {
    u32 count = mgr.sectionParams->localPlayerCount;
    if(count == 0 || count > 4) count = 1;
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller != nullptr) {
        const u8 onlineCount = controller->subs[controller->currentSub].localPlayerCount;
        if(onlineCount >= 1 && onlineCount <= 4 && static_cast<u32>(onlineCount) > count) {
            count = onlineCount;
        }
    }
    return count;
}

//"ma_bike-2" etc.; null for style 0 / invalid input
static const char* GeneratedVehiclePostfix(u32 kart, u32 style) {
    if(kart >= 36 || style == 0 || style >= STYLE_COUNT) return nullptr;
    char* postfix = generatedPostfixes[kart][style];
    if(postfix[0] != '\0') return postfix;
    const char* base = VEHICLE_NAMES[kart];
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
    const char* charName = CHARACTER_NAMES[character];
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

//playstyle (0-3) for a race player (0-11): ghost -> ghostPlaystyles, cpu -> cpuPlaystyles, local -> playstyles, remote -> remoteStyles
u8 StyleForPlayer(u8 playerId) {
    if(playerId < 12 && Racedata::sInstance != nullptr) {
        const RacedataPlayer& player = Racedata::sInstance->racesScenario.players[playerId];
        if(player.playerType == PLAYER_GHOST) {
            return ghostPlaystyles[playerId] & 3;
        }
        if(player.playerType == PLAYER_CPU) {
            return cpuPlaystyles[playerId] & 3;
        }
    }
    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    if(controller == nullptr) return playerId < 4 ? (playstyles[playerId] & 3) : 0; //offline: ids are local huds
    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 rawAid = controller->aidsBelongingToPlayerIds[playerId];
    const u8 aid = rawAid & 0xF; //AI/offline seats hold 0xFF or garbage
    if(aid >= 12 || rawAid == 0xFF) { return 0; }
    const bool isLocal = (aid == sub.localAid);
    const u8 playersAtConsole = isLocal ? sub.localPlayerCount
                                        : sub.connectionUserDatas[aid].playersAtConsole;
    u8 slot = 0;
    if(playersAtConsole == 2 && playerId > 0
        && (controller->aidsBelongingToPlayerIds[playerId - 1] & 0xF) == aid) slot = 1;
    if(isLocal) return playstyles[slot] & 3;
    const System* system = System::sInstance;
    if(system == nullptr) { return 0; }
    const u8 s = system->remoteStyles[aid][slot] & 3;
    return s;
}

//assign each CPU race player a random playstyle 1-3, stable for the whole session
void RandomiseCpuPlaystyles() {
    if(Racedata::sInstance == nullptr) return;
    const RacedataScenario& scenario = Racedata::sInstance->racesScenario;
    const GameMode mode = scenario.settings.gamemode;
    if(mode != MODE_GRAND_PRIX && mode != MODE_VS_RACE) return;
    Random random(scenario.settings.randomSeed);
    for(u8 i = 0; i < 12; ++i) {
        cpuPlaystyles[i] = 0;
        if(scenario.players[i].playerType != PLAYER_CPU) continue;
        
        //randomise style order then pick any - NO archive validation
        u8 order[3] = {1, 2, 3};
        for(u32 j = 3; j > 1; --j) {
            const u32 r = random.NextLimited(j);
            const u8 tmp = order[j - 1]; order[j - 1] = order[r]; order[r] = tmp;
        }
        u8 chosen = order[random.NextLimited(3)]; // Pure random selection
        
        cpuPlaystyles[i] = chosen;
    }
}

//style to use for a race player; vanilla when unset or files missing
static u8 RaceStyleForPlayer(u8 playerId, u32 kart, CharacterId character) {
    const u8 style = StyleForPlayer(playerId);
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

void ProcessStyleInput() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr || mgr->sectionParams == nullptr) return;

    //on local multiplayer the PlaystyleSelect page is the only style picker
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
        const u32 hudCount = GetEffectiveLocalPlayerCount(*mgr);
        for(u8 hud = 0; hud < hudCount; ++hud) {
            //in multiplayer the shared bottom text follows the currently choosing player
            if(hudCount > 1 && !IsHudChoosingVehicle(top, hud)) continue;
            const u32 kart = StatBars::GetHoveredVehicle(hud, mgr->sectionParams->karts[hud]);
            const u8 style = playstyles[hud];
            if(kart == shownKarts[hud] && style == shownStyles[hud]) continue;
            shownKarts[hud] = kart;
            shownStyles[hud] = style;
            ShowStyleLabel(page, hud, kart, style);
        }
    }

    static u16 heldToggleButtons[4] = {0, 0, 0, 0};

    const u32 count = GetEffectiveLocalPlayerCount(*mgr);

    for(u8 hud = 0; hud < count; ++hud) {
        const u32 kart = StatBars::GetHoveredVehicle(hud, mgr->sectionParams->karts[hud]);
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

        //cycle through all styles; vehicles without a style file fall back to vanilla
        u32 style = playstyles[hud];
        style = (style + STYLE_COUNT + step) % STYLE_COUNT;
        if(style == playstyles[hud]) continue;

        playstyles[hud] = static_cast<u8>(style);
        Audio::RSARPlayer::PlaySoundById(step > 0 ? SOUND_ID_RIGHT_ARROW_PRESS : SOUND_ID_LEFT_ARROW_PRESS, 0, nullptr);
        //stat bars reflect the newly selected playstyle immediately
        StatBars::RefreshGridBars(hud, static_cast<KartId>(kart));
    }
}

//---- menu model styling: archives are per character+style "<char>-<style>-allkart" ----
struct MenuCharManager {
    void* vtable;
    EGG::ExpHeap* archiveHeap;
    EGG::ExpHeap* fileHeap;
    s32 state;
    s32 character;
    s32 team;
};
static const u32 MENU_MANAGERS_OFFSET = 0x5AC; //menuCharacterManagers within ResourceManager/ArchiveMgr
static const u32 MENU_ARCHIVES_OFFSET = 0x8;   //kartArchives within ResourceManager/ArchiveMgr
static const u32 MENU_ARCHIVE_STRIDE = 28;     //sizeof MultiDvdArchive

//style of the archive currently loaded per hud; 0 = vanilla (missing styles load vanilla)
static u8 menuBoundStyle[4] = {0, 0, 0, 0};
//set while a style-triggered archive reload is in flight
static bool menuReloadOutstanding[4] = {false, false, false, false};
//game-accurate MenuDriverModel state ids (the header's 1/2 are wrong)
static const u32 MENU_DRIVER_STATE_CHARSEL = 0;
static const u32 MENU_DRIVER_STATE_ONKART = 2;
//tri-state existence cache per character/style: 0 unknown, 1 missing, 2 exists
static u8 menuStyleExists[48][STYLE_COUNT] = {};
//"<char>-<style>" postfixes, generated once
static char generatedMenuPostfixes[48][STYLE_COUNT][32];

typedef void (*MenuArchiveLoadFunc)(void* archive, char* path, EGG::Heap* archiveHeap, EGG::Heap* fileHeap, u32 unused);
static MenuArchiveLoadFunc const RealMenuArchiveLoad = reinterpret_cast<MenuArchiveLoadFunc>(0x8052A954);
typedef bool (*RequestMenuReloadFunc)(ArchiveMgr* mgr, u8 hud, u32 character, u32 gamemode);
static RequestMenuReloadFunc const RequestMenuReload = reinterpret_cast<RequestMenuReloadFunc>(0x80542210);
typedef void (*PrepareDriverOnKartAnmsFunc)(MenuDriverModelMgr* mgr, u32 hud);
static PrepareDriverOnKartAnmsFunc const RealPrepareDriverOnKartAnms = reinterpret_cast<PrepareDriverOnKartAnmsFunc>(0x80830c64);

static MenuCharManager* MenuManagerForHud(u8 hud) {
    ArchiveMgr* mgr = ArchiveMgr::sInstance;
    if(mgr == nullptr || hud >= 4) return nullptr;
    return reinterpret_cast<MenuCharManager*>(reinterpret_cast<u8*>(mgr) + MENU_MANAGERS_OFFSET + hud * sizeof(MenuCharManager));
}

static const char* GeneratedMenuPostfix(u32 character, u32 style) {
    if(character >= 48 || style == 0 || style >= STYLE_COUNT) return nullptr;
    char* postfix = generatedMenuPostfixes[character][style];
    if(postfix[0] != '\0') return postfix;
    const char* base = CHARACTER_NAMES[character];
    if(base == nullptr) return nullptr;
    if(snprintf(postfix, 32, "%s-%u", base, style) <= 0) {
        postfix[0] = '\0';
        return nullptr;
    }
    return postfix;
}

//probe the styled menu archive, mirroring the race-side VehicleStyleFileExists pattern
static bool MenuStyleFileExists(u32 character, u32 style) {
    if(character >= 48 || style == 0 || style >= STYLE_COUNT) return false;
    u8& cached = menuStyleExists[character][style];
    if(cached != 0) return cached == 2;
    bool exists = false;
    const char* postfix = GeneratedMenuPostfix(character, style);
    if(postfix != nullptr) {
        char path[0x80];
        if(snprintf(path, sizeof(path), "/Scene/Model/Kart/%s-allkart.szs", postfix) > 0) {
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

//latches an out-of-band selection (multiplayer picker) into the bound style
void NoteMenuStyleSelected(u8 hud) {
    if(hud >= 4) return;
    MenuCharManager* mm = MenuManagerForHud(hud);
    if(mm == nullptr || mm->character < 0 || mm->character >= 0x30) return;
    const u8 style = playstyles[hud] & 3;
    menuBoundStyle[hud] = (style != 0 && MenuStyleFileExists(static_cast<u32>(mm->character), style)) ? style : 0;
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
    for(u32 c = 0; c < 48; ++c) {
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

//substitutes the styled archive path into menu allkart loads; anything else loads vanilla
static void MenuArchiveLoadHook(void* archiveCountPtr, char* path, EGG::Heap* archiveHeap, EGG::Heap* fileHeap, u32 unused) {
    ArchiveMgr* mgr = ArchiveMgr::sInstance;
    //HUD slot from the archive pointer; call sites disagree on the exact offset
    u32 hud = 4;
    if(mgr != nullptr && path != nullptr) {
        const u8* base = reinterpret_cast<const u8*>(&mgr->kartModelsHolders[0]);
        const u8* given = reinterpret_cast<const u8*>(archiveCountPtr);
        if(given >= base) {
            const u32 off0 = static_cast<u32>(given - base);
            if(off0 % MENU_ARCHIVE_STRIDE == 0) {
                hud = off0 / MENU_ARCHIVE_STRIDE;
            }
            else if(given >= base + MENU_ARCHIVES_OFFSET) {
                const u32 off8 = static_cast<u32>(given - MENU_ARCHIVES_OFFSET - base);
                if(off8 % MENU_ARCHIVE_STRIDE == 0) hud = off8 / MENU_ARCHIVE_STRIDE;
            }
        }
    }
    if(mgr != nullptr && path != nullptr && hud < 4 && !MenuPathIsBattle(path)) {
        const u8 style = menuBoundStyle[hud] & 3;
        if(style != 0) {
            u32 character = 0;
            if(MenuPathCharacter(path, character) && MenuStyleFileExists(character, style)) {
                const char* postfix = GeneratedMenuPostfix(character, style);
                if(postfix != nullptr) {
                    snprintf(path, 128, "Scene/Model/Kart/%s-allkart", postfix);
                }
            }
        }
    }
    RealMenuArchiveLoad(archiveCountPtr, path, archiveHeap, fileHeap, unused);
}
kmCall(0x805411b8, MenuArchiveLoadHook);
kmCall(0x80541FB8, MenuArchiveLoadHook);
//sync variant loader's load call (same register convention)
kmCall(0x80542198, MenuArchiveLoadHook);

//re-shows the driver on the kart after a reload's rebuild; visibility-only off a style page
static void RestoreDriverAfterRebuild(u8 hud) {
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    if(modelMgr == nullptr || modelMgr->driverModels == nullptr) return;
    MenuDriverModel* liveDriver = modelMgr->driverModels->players[hud].playerModel;
    if(liveDriver == nullptr || liveDriver->model == nullptr || liveDriver->onKartTransformator == nullptr) return;
    SectionMgr* mgr = SectionMgr::sInstance;
    bool onStylePage = false;
    if(mgr != nullptr && mgr->curSection != nullptr) {
        Page* top = mgr->curSection->GetTopLayerPage();
        onStylePage = top != nullptr && IsStylePageId(top->pageId) && top->pageId != PAGE_BATTLE_KART_SELECT;
    }
    liveDriver->model->bitfield |= 0x100000;
    modelMgr->driverModels->players[hud].isVisible = true;
    if(onStylePage) {
        liveDriver->SwitchState(hud, static_cast<MenuDriverModel::State>(MENU_DRIVER_STATE_ONKART));
    }
    menuReloadOutstanding[hud] = false;
}

//restores in the same frame as the rebuild, which ends with prepareDriverOnKartAnms
static void PrepareDriverOnKartAnmsHook(MenuDriverModelMgr* mgr, u32 hud) {
    RealPrepareDriverOnKartAnms(mgr, hud);
    if(hud < 4 && menuReloadOutstanding[hud]) {
        RestoreDriverAfterRebuild(static_cast<u8>(hud));
    }
}
kmCall(0x80832ea4, PrepareDriverOnKartAnmsHook);

//reloads the menu archive when the selected style changed under it
void ProcessMenuRebinds() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return;

    Page* top = mgr->curSection->GetTopLayerPage();
    const bool onStylePage = top != nullptr && IsStylePageId(top->pageId) && top->pageId != PAGE_BATTLE_KART_SELECT;

    //restore poll; runs before the page gate so a mid-reload page change still restores
    MenuModelMgr* modelMgr = MenuModelMgr::sInstance;
    for(u8 hud = 0; hud < 4; ++hud) {
        if(!menuReloadOutstanding[hud]) continue;
        MenuCharManager* mm = MenuManagerForHud(hud);
        if(mm == nullptr || modelMgr == nullptr || modelMgr->driverModels == nullptr || modelMgr->kartModels == nullptr) continue;
        if(mm->state == 0) {
            //reload failed: re-show the driver and abort
            MenuDriverModel* liveDriver = modelMgr->driverModels->players[hud].playerModel;
            if(liveDriver != nullptr && liveDriver->model != nullptr) {
                liveDriver->model->bitfield |= 0x100000;
            }
            modelMgr->driverModels->players[hud].isVisible = true;
            menuReloadOutstanding[hud] = false;
            continue;
        }
        if(mm->state != 4 || !modelMgr->kartModels->players[hud].isLocked) continue;
        RestoreDriverAfterRebuild(hud);
    }

    if(top == nullptr || !onStylePage) return;

    //first pass only synchronises the bound styles with the playstyles
    static bool menuBoundsSynced = false;
    if(!menuBoundsSynced) {
        for(u8 hud = 0; hud < 4; ++hud) {
            MenuCharManager* mm = MenuManagerForHud(hud);
            const u8 style = playstyles[hud] & 3;
            if(mm == nullptr || mm->character < 0 || mm->character >= 0x30) {
                menuBoundStyle[hud] = 0;
                continue;
            }
            menuBoundStyle[hud] = (style != 0 && MenuStyleFileExists(static_cast<u32>(mm->character), style)) ? style : 0;
        }
        menuBoundsSynced = true;
        return;
    }

    ArchiveMgr* archiveMgr = ArchiveMgr::sInstance;
    if(archiveMgr == nullptr) return;
    const u32 count = GetEffectiveLocalPlayerCount(*mgr);

    //detect style changes and rebind the menu archive in the same frame
    for(u8 hud = 0; hud < count; ++hud) {
        const u8 style = playstyles[hud] & 3;
        MenuCharManager* mm = MenuManagerForHud(hud);
        if(mm == nullptr || mm->archiveHeap == nullptr) continue;
        if(mm->character < 0 || mm->character >= 0x30) continue;
        //missing styles load vanilla; reload when the loaded archive differs
        const bool styled = style != 0 && MenuStyleFileExists(static_cast<u32>(mm->character), style);
        const u8 loadStyle = styled ? style : 0;
        if(loadStyle == menuBoundStyle[hud]) continue;
        if(modelMgr == nullptr || modelMgr->driverModels == nullptr || modelMgr->kartModels == nullptr) continue;
        MenuDriverModel* liveDriver = modelMgr->driverModels->players[hud].playerModel;
        if(liveDriver == nullptr || liveDriver->model == nullptr) continue;
        //stops the old on-kart anms and binds the stable charSel transformator
        liveDriver->onKartTransformator = nullptr;
        liveDriver->SwitchState(hud, static_cast<MenuDriverModel::State>(MENU_DRIVER_STATE_CHARSEL));
        //hides the driver from the render gather until the restore re-shows it
        modelMgr->driverModels->players[hud].isVisible = false;
        liveDriver->model->ToggleVisible(false);
        liveDriver->model->bitfield &= ~0x100000;
        menuReloadOutstanding[hud] = true;
        modelMgr->ResetKartModels(hud);
        if(!RequestMenuReload(archiveMgr, hud, static_cast<u32>(mm->character), static_cast<u32>(mm->team))) {
            //reload request failed: re-show the driver immediately
            liveDriver->model->bitfield |= 0x100000;
            modelMgr->driverModels->players[hud].isVisible = true;
            menuReloadOutstanding[hud] = false;
        } else {
            menuBoundStyle[hud] = loadStyle;
        }
    }
}

}//namespace CustomVehicles

//menu update wrappers: poll style input just before the menu pipeline runs
void MenuSceneUpdateHook(SectionMgr& mgr) {
    CustomVehicles::ProcessStyleInput();
    CustomVehicles::ProcessMenuRebinds();
    mgr.MenuUpdate();
}
kmCall(0x805552e8, MenuSceneUpdateHook);
kmCall(0x80553b30, MenuSceneUpdateHook);

namespace CustomVehicles {

//race model loading: swap the vehicle name entry so vanilla builds the styled archive paths itself
static ArchivesHolder* LoadKartArchiveHook(ArchiveMgr* archiveMgr, u8 playerId, KartId kart, CharacterId character,
    u32 color, u32 type, EGG::Heap* decompressedHeap, EGG::Heap* archiveHeap) {
    const u8 style = RaceStyleForPlayer(playerId, kart, character);
    const char** entry = &VEHICLE_NAMES[kart];
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
    const char** entry = &VEHICLE_NAMES[kart];
    const char* old = *entry;
    if(style != 0) *entry = GeneratedVehiclePostfix(kart, style);
    ArchivesHolder* holder = archiveMgr->LoadKartArchiveHolder2(playerId, kart, character, color, 0, decompressedHeap, archiveHeap);
    *entry = old;
    return holder;
}
kmCall(0x80554198, LoadBackupKartArchiveHook);

//only applies star colours when the styled archive actually provides them
static void SetModelColorsIfReady(void* starAnm, void* drawMdl) {
    if(drawMdl == nullptr) return;
    const u8* model = static_cast<const u8*>(drawMdl);
    if(*reinterpret_cast<void* const*>(model + 0x10) == nullptr) return;
    for(u32 i = 0; i < 2; ++i) {
        if(*reinterpret_cast<void* const*>(model + 0x14 + i * sizeof(void*)) == nullptr) return;
    }
    SetModelColorsImpl(starAnm, drawMdl);
}
kmCall(0x80592e24, SetModelColorsIfReady);
kmCall(0x80592e40, SetModelColorsIfReady);

}//namespace CustomVehicles
}//namespace UI
}//namespace Pulsar
