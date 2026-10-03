#include <UI/CustomVehicles/CustomVehicles.hpp>
#include <UI/CustomVehicles/StyleArchives.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <MarioKartWii/Archive/ArchiveFile.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/RKNet/RKNetController.hpp>
#include <MarioKartWii/System/Random.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <core/egg/Decomp.hpp>
#include <core/egg/mem/Heap.hpp>
#include <core/RK/RKSystem.hpp>
#include <core/rvl/OS/OS.hpp>
#include <core/rvl/os/OSCache.hpp>
#include <include/c_string.h>
#include <runtimeWrite.hpp>

namespace Pulsar {
namespace UI {

//style currently selected by each local player
u8 playstyles[4] = {0, 0, 0, 0};
//style of each ghost slot, read from the ghost's RKG header
u8 ghostPlaystyles[4] = {0, 0, 0, 0};
//style of each cpu slot
u8 cpuPlaystyles[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

namespace CustomVehicles {

//style (0-3) for a race player: ghost -> ghostPlaystyles, cpu -> cpuPlaystyles, local/remote -> playstyles/remoteStyles
u8 StyleForPlayer(u8 playerId) {
    const Racedata* raceData = Racedata::sInstance;
    if(raceData == nullptr) return 0;
    const RacedataScenario& scenario = raceData->racesScenario;
    const GameMode gamemode = scenario.settings.gamemode;
    if(IsBattleMode(gamemode)) return 0;

    if(playerId < 4 && scenario.players[playerId].playerType == PLAYER_GHOST) {
        return ghostPlaystyles[playerId] & 3;
    }
    if(playerId < 12 && scenario.players[playerId].playerType == PLAYER_CPU) {
        return cpuPlaystyles[playerId] & 3;
    }

    const RKNet::Controller* controller = RKNet::Controller::sInstance;
    const bool isOnlineRace = gamemode == MODE_PUBLIC_VS || gamemode == MODE_PRIVATE_VS;
    if(!isOnlineRace || controller == nullptr) return playerId < 4 ? (playstyles[playerId] & 3) : 0;

    const RKNet::ControllerSub& sub = controller->subs[controller->currentSub];
    const u8 rawAid = controller->aidsBelongingToPlayerIds[playerId];
    const u8 aid = rawAid & 0xF; //AI/offline seats hold 0xFF or garbage
    if(aid >= 12 || rawAid == 0xFF) return 0;

    //two players of one online console are the two race players sharing this aid
    const bool isLocal = aid == sub.localAid;
    const u8 playersAtConsole = isLocal ? sub.localPlayerCount
                                        : sub.connectionUserDatas[aid].playersAtConsole;
    u8 slot = 0;
    if(playersAtConsole == 2 && playerId > 0
        && (controller->aidsBelongingToPlayerIds[playerId - 1] & 0xF) == aid) slot = 1;
    if(isLocal) return playstyles[slot] & 3;

    const System* system = System::sInstance;
    if(system == nullptr) return 0;
    return static_cast<u8>(system->remoteStyles[aid][slot] & 3);
}

//assign each cpu race player a random style 1-3, re-rolled from the race seed
void RandomiseCpuPlaystyles() {
    const Racedata* raceData = Racedata::sInstance;
    if(raceData == nullptr) return;
    const RacedataScenario& scenario = raceData->racesScenario;
    const GameMode gamemode = scenario.settings.gamemode;
    if(gamemode != MODE_GRAND_PRIX && gamemode != MODE_VS_RACE) return;
    Random random(scenario.settings.randomSeed);
    for(u8 i = 0; i < 12; ++i) {
        //the style is not checked against the archives here; a miss falls back to vanilla in the race
        cpuPlaystyles[i] = scenario.players[i].playerType == PLAYER_CPU
            ? static_cast<u8>(1 + random.NextLimited(STYLE_COUNT - 1)) : 0;
    }
}

//randomise the style of each local player, latching the menu archive of the new combo
void RandomiseLocalPlaystyles() {
    SectionMgr* sectionMgr = SectionMgr::sInstance;
    if(sectionMgr == nullptr || sectionMgr->curSection == nullptr) return;
    SectionParams* sectionParams = sectionMgr->sectionParams;
    if(sectionParams == nullptr) return;
    const Racedata* raceData = Racedata::sInstance;
    if(raceData != nullptr && IsBattleMode(raceData->menusScenario.settings.gamemode)) return;

    u32 count = sectionParams->localPlayerCount;
    if(count == 0 || count > 4) count = 1;
    Random random;
    for(u32 hud = 0; hud < count; ++hud) {
        const u8 style = static_cast<u8>(random.NextLimited(STYLE_COUNT));
        NoteComboRandomisedStyle(static_cast<u8>(hud), sectionParams->characters[hud], style);
    }
}


//kart archives decode onto a root heap, which survives between races, unlike the system heap
static EGG::Heap* KartArchiveRootHeap(u32 requiredSize) {
    EGG::Heap* systemHeap = nullptr;
    if(System::sInstance != nullptr) systemHeap = static_cast<EGG::Heap*>(System::sInstance->heap);
    EGG::Heap* candidates[3];
    candidates[0] = RKSystem::mInstance.EGGRootMEM2;
    candidates[1] = RKSystem::mInstance.EGGRootMEM1;
    candidates[2] = systemHeap;
    for(u32 i = 0; i < 3; ++i) {
        if(candidates[i] == nullptr) continue;
        if(candidates[i]->getAllocatableSize(0x20) >= requiredSize) return candidates[i];
    }
    return nullptr;
}

//kart-only scoping, and MKW formats these paths without a leading slash, so the prefix must match
static bool IsRaceKartArchivePath(const char* path) {
    static const char prefix[] = "Race/Kart/";
    return path != nullptr && strncmp(path, prefix, sizeof(prefix) - 1) == 0;
}

static void ArchiveFileDecompressHook(ArchiveFile* file, const char* path, EGG::Heap* heap,
    EGG::Archive::FileInfo* info) {
    u8* compressed = static_cast<u8*>(file->compressedArchive);
    u32 size = EGG::Decomp::getExpandSize(compressed);
    EGG::Heap* target = heap;
    //kart archives decode onto a root heap; every other archive keeps the caller's mountHeap
    if(IsRaceKartArchivePath(path)) {
        EGG::Heap* rootHeap = KartArchiveRootHeap(size);
        if(rootHeap != nullptr) target = rootHeap;
    }
    u8* dest = EGG::Heap::alloc<u8>(size, 0x20, target);
    EGG::Decomp::decodeSZS(compressed, dest);
    file->archiveSize = size;
    file->rawArchive = dest;
    file->archiveHeap = target;
    OS::DCStoreRange(dest, size);
    file->status = ARCHIVE_STATUS_DECOMPRESSED;
}
kmBranch(0x80519508, ArchiveFileDecompressHook);

extern "C" void SetModelColorsImpl(void*, void*);

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