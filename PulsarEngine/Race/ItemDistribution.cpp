#include <Race/ItemDistribution.hpp>

namespace Pulsar {
namespace Race {

// Static variables
static ItemDistributionConfig sDistributionConfig;
static u8 sCurrentPlayerCount = 0;
static bool sIsInitialised = false;
static bool sHasCustomDistributions = false;

// Function to load item distributions from file
bool LoadItemDistributionFromFile() {
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

    return false;
}

// Function to parse PULItemSlot.bin file
void ParsePULItemSlotFile(const u8* fileData) {
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
                    sDistributionConfig.roomSizes[tableIdx][position][item] = tableData[item * columns + position];
                }
            }
        }

        dataPtr += columns * rows * sizeof(u8);
    }
}

// Function to initialise item distribution
void InitItemDistribution() {
    if (sIsInitialised) return;

    // Get current player count from race data
    if (Racedata::sInstance) {
        sCurrentPlayerCount = Racedata::sInstance->racesScenario.playerCount;
    }

    // Load custom item distributions from PULItemSlot.bin file if available
    sHasCustomDistributions = LoadItemDistributionFromFile();

    sIsInitialised = true;
}

// Function to apply room-size-based item distribution
void ApplyRoomSizeDistribution(Item::ItemSlotData::Probabilities* probabilities, bool isPlayerTable) {
    if (!probabilities) {
        OS::Report("ItemDistribution: ERROR - ApplyRoomSizeDistribution called with invalid parameters!\n");
        return;
    }

    // Ensure the instance is initialised
    if (!sIsInitialised) {
        InitItemDistribution();
    }

    // Only apply custom distributions if we successfully loaded them
    if (sHasCustomDistributions) {
        // Convert room size to index (2->0, 3->1, ..., 12->10)
        u8 roomSizeIndex = sCurrentPlayerCount - 2;

        // Apply the custom distribution for this room size
        for (u8 position = 0; position < probabilities->rowCount && position < 12; position++) {
            for (int itemId = 0; itemId < 19; itemId++) {
                u8 prob = sDistributionConfig.roomSizes[roomSizeIndex][position][itemId];
                probabilities->probabilities[position * 19 + itemId] = prob;
            }
        }
    }
}

// Hook function to apply room-size-based item distribution
void ApplyRoomSizeDistributionHook(Item::ItemSlotData* itemSlotData, Item::ItemSlotData::Probabilities* probabilities) {
    if (!itemSlotData || !probabilities || !probabilities->probabilities || probabilities->rowCount == 0) {
        OS::Report("ItemDistribution: ERROR - Invalid parameters in hook!\n");
        return;
    }

    u8 currentCount = sCurrentPlayerCount;
    if (currentCount != itemSlotData->playerCount) {
        sCurrentPlayerCount = static_cast<u8>(itemSlotData->playerCount);
    }

    ApplyRoomSizeDistribution(probabilities, false);
}

// Hook the CALL to PostProcessVSTable
kmCall(0x807bb5c0, ApplyRoomSizeDistributionHook);
kmCall(0x807bb1ac, ApplyRoomSizeDistributionHook);
kmCall(0x807bb0b4, ApplyRoomSizeDistributionHook);

// Initialise item distribution when the race loads
void InitItemDistributionOnLoad() {
    OS::Report("ItemDistribution: Initialising item distribution on load...\n");
    InitItemDistribution();
    OS::Report("ItemDistribution: Item distribution initialised.\n");
}

RaceLoadHook initItemDistHook(InitItemDistributionOnLoad);

} // namespace Race
} // namespace Pulsar
