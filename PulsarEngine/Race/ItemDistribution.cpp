#include <Race/ItemDistribution.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <PulsarSystem.hpp>
#include <kamek.hpp>
#include <core/rvl/OS/OS.hpp>

// Hook function to apply room-size-based item distribution
static void ApplyRoomSizeDistributionHook(Item::ItemSlotData* itemSlotData, Item::ItemSlotData::Probabilities* probabilities) {
    if (!itemSlotData || !probabilities || !probabilities->probabilities || probabilities->rowCount == 0) {
        OS::Report("ItemDistribution: ERROR - Invalid parameters in hook!\n");
        return;
    }

    Pulsar::Race::ItemDistributionManager* instance = Pulsar::Race::ItemDistributionManager::GetInstance();
    if (!instance) {
        Pulsar::Race::ItemDistributionManager::CreateInstance();
        instance = Pulsar::Race::ItemDistributionManager::GetInstance();
        if (!instance) {
            OS::Report("ItemDistribution: ERROR - Failed to create instance!\n");
            return;
        }
    }

    u32 currentCount = instance->GetCurrentPlayerCount();
    if (currentCount != itemSlotData->playerCount) {
        instance->UpdatePlayerCount(itemSlotData->playerCount);
        OS::Report("ItemDistribution: Player count updated from %d to %d\n", currentCount, itemSlotData->playerCount);
    }

    instance->ApplyRoomSizeDistribution(probabilities, false);
}

// Hook the CALL to PostProcessVSTable
kmCall(0x807bb5c0, ApplyRoomSizeDistributionHook);
kmCall(0x807bb1ac, ApplyRoomSizeDistributionHook);
kmCall(0x807bb0b4, ApplyRoomSizeDistributionHook);

// Create and initialise the ItemDistributionManager when the race loads
static void InitItemDistribution() {
    Pulsar::Race::ItemDistributionManager::CreateInstance();
}

static void CleanupItemDistribution() {
    Pulsar::Race::ItemDistributionManager::DestroyInstance();
}

static RaceLoadHook initItemDistHook(InitItemDistribution);
static RaceFrameHook cleanupItemDistHook(CleanupItemDistribution);

namespace Pulsar {
namespace Race {

// Static instance
ItemDistributionManager* ItemDistributionManager::sInstance = nullptr;

ItemDistributionManager* ItemDistributionManager::CreateInstance() {
    if (!sInstance) {
        sInstance = new ItemDistributionManager();
        sInstance->Init();
    }
    return sInstance;
}

ItemDistributionManager* ItemDistributionManager::GetInstance() {
    return sInstance;
}

void ItemDistributionManager::DestroyInstance() {
    if (sInstance) {
        delete sInstance;
        sInstance = nullptr;
    }
}

ItemDistributionManager::ItemDistributionManager()
    : currentPlayerCount(0), isInitialised(false) {
}

ItemDistributionManager::~ItemDistributionManager() {}

void ItemDistributionManager::Init() {
    if (isInitialised) return;

    // Get current player count from race data
    if (Racedata::sInstance) {
        currentPlayerCount = Racedata::sInstance->racesScenario.playerCount;
    }

    // Load custom item distributions from PULItemSlot.bin file if available
    hasCustomDistributions = LoadItemDistributionFromFile();

    isInitialised = true;
}

void ItemDistributionManager::ApplyRoomSizeDistribution(Item::ItemSlotData::Probabilities* probabilities, bool isPlayerTable) {
    if (!probabilities || !isInitialised) {
        OS::Report("ItemDistribution: ERROR - ApplyRoomSizeDistribution called with invalid parameters!\n");
        return;
    }

    // Only apply custom distributions if we successfully loaded them
    if (hasCustomDistributions) {
        // Convert room size to index (2->0, 3->1, ..., 12->10)
        int roomSizeIndex = currentPlayerCount - 2;

        // Apply the custom distribution for this room size
        for (u32 position = 0; position < probabilities->rowCount && position < 12; position++) {
            for (int itemId = 0; itemId < 19; itemId++) {
                u16 prob = distributionConfig.roomSizes[roomSizeIndex][position][itemId];
                probabilities->probabilities[position * 19 + itemId] = prob;
            }
        }
    }
}

bool ItemDistributionManager::LoadItemDistributionFromFile() {
    // Check if ArchiveMgr is available
    if (!ArchiveMgr::sInstance) {
        OS::Report("ItemDistribution: ERROR - ArchiveMgr not available!\n");
        return false;
    }

    // Try to get the file from CommonAssets archive
    if (const void* fileData = ArchiveMgr::sInstance->GetFile(ARCHIVE_HOLDER_COMMON, "PULItemSlot.bin")) {
        // Parse the file directly from the archive
        ParsePULItemSlotFile(static_cast<const u8*>(fileData));
        return true;
    }

    OS::Report("ItemDistribution: INFO - PULItemSlot.bin not found, using default distributions\n");
    return false;
}

void ItemDistributionManager::ParsePULItemSlotFile(const u8* fileData) {
    // PULItemSlot.bin format: 12 tables, each with 12 positions (NUM_POSITIONS) × 19 items (NUM_ITEMS)
    // Data is stored in column-major order (transposed): [item][position]

    u8 tableCount = *fileData;
    const u8* dataPtr = fileData + sizeof(u8);

    // Process each table (tableIdx 0 = 2 players, tableIdx 10 = 12 players)
    for (u8 tableIdx = 0; tableIdx < tableCount; tableIdx++) {
        u8 columns = *dataPtr++;  // positions (12)
        u8 rows = *dataPtr++;     // items (19)

        if (tableIdx < NUM_ROOM_SIZES && rows <= NUM_ITEMS && columns <= NUM_POSITIONS) {
            const u8* tableData = dataPtr;

            // Convert from file format [item][position] to [position][item]
            for (u8 item = 0; item < rows; item++) {
                for (u8 position = 0; position < columns; position++) {
                    distributionConfig.roomSizes[tableIdx][position][item] = tableData[item * columns + position];
                }
            }
        }

        dataPtr += columns * rows * sizeof(u8);
    }
}


} // namespace Race
} // namespace Pulsar
