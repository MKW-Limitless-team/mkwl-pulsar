#ifndef PULSAR_ITEM_DISTRIBUTION_HPP
#define PULSAR_ITEM_DISTRIBUTION_HPP

#include <kamek.hpp>
#include <PulsarSystem.hpp>
#include <MarioKartWii/Item/ItemSlot.hpp>
#include <MarioKartWii/System/Identifiers.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>

#include <core/rvl/OS/OS.hpp>

namespace Pulsar {
namespace Race {

// Item distribution configuration
#define NUM_ROOM_SIZES 11    // 2-12 players
#define NUM_POSITIONS 12      // Position count
#define NUM_ITEMS 19          // Item count

struct ItemDistributionConfig {
    u8 roomSizes[NUM_ROOM_SIZES][NUM_POSITIONS][NUM_ITEMS]; // [roomSize][position][item] probabilities
};

// Function declarations
void InitItemDistribution();
void ApplyRoomSizeDistribution(Item::ItemSlotData::Probabilities* probabilities, bool isPlayerTable);
bool LoadItemDistributionFromFile();
void ParsePULItemSlotFile(const u8* fileData);
} // namespace Race
} // namespace Pulsar

#endif // PULSAR_ITEM_DISTRIBUTION_HPP