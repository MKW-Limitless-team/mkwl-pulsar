#include <Race/ItemDistribution.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <PulsarSystem.hpp>
#include <kamek.hpp>
#include <core/rvl/OS/OS.hpp>

// Hook function to apply room-size-based item distribution
// This function will be called by the kmBranch hook below
// Based on the assembly analysis, the scaleTable function has ItemSlotData* as its first parameter (in r3)
static void ApplyRoomSizeDistributionHook(Item::ItemSlotData* itemSlotData) {
    OS::Report("ItemDistribution: Hook activated\n");

    // Debug: Show what's in the parameter
    OS::Report("ItemDistribution: itemSlotData parameter value: 0x%08x\n", reinterpret_cast<u32>(itemSlotData));

    if (!itemSlotData) {
        OS::Report("ItemDistribution: itemSlotData is null\n");
        return;
    }

    OS::Report("ItemDistribution: itemSlotData valid, playerChances rowCount=%d\n", itemSlotData->playerChances.rowCount);

    // Apply our room-size-based distribution to player probabilities
    Pulsar::Race::ItemDistributionManager* instance = Pulsar::Race::ItemDistributionManager::GetInstance();
    if (instance) {
        OS::Report("ItemDistribution: Instance found, applying distribution\n");
        // Apply to player probabilities (address 0x10 in ItemSlotData)
        instance->ApplyRoomSizeDistribution(&itemSlotData->playerChances, true);
    } else {
        OS::Report("ItemDistribution: Instance is null, creating it\n");
        // Debug: Instance is null, try to create it
        Pulsar::Race::ItemDistributionManager::CreateInstance();
        instance = Pulsar::Race::ItemDistributionManager::GetInstance();
        if (instance) {
            OS::Report("ItemDistribution: Instance created, applying distribution\n");
            instance->ApplyRoomSizeDistribution(&itemSlotData->playerChances, true);
        } else {
            OS::Report("ItemDistribution: Failed to create instance\n");
        }
    }
}

// Hook the blr instruction at 0x807ba810 where ItemSlotData::CreateInstance() returns
// At this point, the ItemSlotData::sInstance should be available and valid
kmBranch(0x807ba810, ApplyRoomSizeDistributionHook);

// Create and initialise the ItemDistributionManager when the race loads
static void InitItemDistribution() {
    Pulsar::Race::ItemDistributionManager::CreateInstance();
}

// Clean up when the race ends
static void CleanupItemDistribution() {
    Pulsar::Race::ItemDistributionManager::DestroyInstance();
}

// Register our init and cleanup functions with the race load hooks
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

    // Initialize default distributions
    InitializeDefaultDistributions();

    isInitialised = true;
}

void ItemDistributionManager::InitializeDefaultDistributions() {
    // Copy the hardcoded distribution table into our configuration
    for (int roomSizeIndex = 0; roomSizeIndex < 11; roomSizeIndex++) {
        for (int position = 0; position < 12; position++) {
            for (int itemId = 0; itemId < 19; itemId++) {
                distributionConfig.roomSizes[roomSizeIndex].positions[position].probabilities[itemId] =
                    DEFAULT_ITEM_DISTRIBUTION_TABLE[roomSizeIndex][position][itemId];
            }
        }
    }
}

void ItemDistributionManager::ApplyCustomItemDistribution(Item::ItemSlotData::Probabilities* probabilities, int roomSize) {
    if (!probabilities || !probabilities->probabilities) return;

    // Convert room size to index (2->0, 3->1, ..., 12->10)
    int roomSizeIndex = roomSize - 2;

    // Apply the custom distribution for this room size
    for (u32 position = 0; position < probabilities->rowCount && position < 12; position++) {
        // Copy probabilities from our configuration to the game's probability table
        for (int itemId = 0; itemId < 19; itemId++) {
            probabilities->probabilities[position * 19 + itemId] =
                distributionConfig.roomSizes[roomSizeIndex].positions[position].probabilities[itemId];
        }
    }
}

void ItemDistributionManager::ApplyRoomSizeDistribution(Item::ItemSlotData::Probabilities* probabilities, bool isPlayerTable) {
    OS::Report("ItemDistribution: ApplyRoomSizeDistribution called, isPlayerTable=%d\n", isPlayerTable);

    if (!probabilities) {
        OS::Report("ItemDistribution: probabilities is null\n");
        return;
    }

    if (!isInitialised) {
        OS::Report("ItemDistribution: Not initialised\n");
        return;
    }

    OS::Report("ItemDistribution: Room size=%d\n", currentPlayerCount);

    // Apply custom item distribution based on room size
    ApplyCustomItemDistribution(probabilities, currentPlayerCount);
}


} // namespace Race
} // namespace Pulsar
