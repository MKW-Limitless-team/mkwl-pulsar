#ifndef PULSAR_ITEM_DISTRIBUTION_HPP
#define PULSAR_ITEM_DISTRIBUTION_HPP

#include <kamek.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <IO/IO.hpp>

namespace Pulsar {
namespace Race {

// Item distribution configuration
#define NUM_ROOM_SIZES 11    // 2-12 players
#define NUM_POSITIONS 12      // Position count
#define NUM_ITEMS 19          // Item count

struct ItemDistributionConfig {
    u16 roomSizes[NUM_ROOM_SIZES][NUM_POSITIONS][NUM_ITEMS]; // [roomSize][position][item] probabilities
};

class ItemDistributionManager {
public:
    static ItemDistributionManager* sInstance;
    static ItemDistributionManager* CreateInstance();
    static void DestroyInstance();
    ItemDistributionManager();
    ~ItemDistributionManager();
    void Init();
    void ApplyRoomSizeDistribution(Item::ItemSlotData::Probabilities* probabilities, bool isPlayerTable);
    static ItemDistributionManager* GetInstance();

    // Public method to update player count
    void UpdatePlayerCount(u32 newPlayerCount) {
        currentPlayerCount = newPlayerCount;
    }

    // Public method to get current player count
    u32 GetCurrentPlayerCount() const {
        return currentPlayerCount;
    }

private:
    u32 currentPlayerCount;
    bool isInitialised;
    bool hasCustomDistributions;
    ItemDistributionConfig distributionConfig;
    bool LoadItemDistributionFromFile();
    void ParsePULItemSlotFile(const u8* fileData);
};

} // namespace Race
} // namespace Pulsar

#endif // PULSAR_ITEM_DISTRIBUTION_HPP