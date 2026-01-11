#include <kamek.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/Effect/EffectMgr.hpp> 
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>
#include <Race/200ccParams.hpp>
#include <PulsarSystem.hpp>

namespace Pulsar {
namespace Race {


static const u8 AREA_TYPE_GRAVITY = 0x0b; // AREA type for configurable gravity

float GetAreaGravityMultiplier(u8 playerId) {
    float gravityMultiplier = 1.0f;
    
    Kart::Manager* kartMgr = Kart::Manager::sInstance;
    KMP::Manager* kmpMgr = KMP::Manager::sInstance;
    
    if (kartMgr == nullptr || kmpMgr == nullptr || kmpMgr->areaSection == nullptr) {
        return gravityMultiplier;
    }
    
    if (playerId >= kartMgr->playerCount) {
        return gravityMultiplier;
    }
    
    Kart::Player* player = kartMgr->players[playerId];
    if (player == nullptr) {
        return gravityMultiplier;
    }
    
    const Vec3& position = player->GetPhysics().position;
    
    s16 foundIdx = kmpMgr->FindAREA(position, -1, AREA_TYPE_GRAVITY);
    
    if (foundIdx >= 0) {
        AREA* area = kmpMgr->areaSection->holdersArray[foundIdx]->raw;
        float* settingAsFloat = reinterpret_cast<float*>(&area->setting1);
        gravityMultiplier = *settingAsFloat;
    }
    
    return gravityMultiplier;
}

static void ApplyAreaGravityMultiplier(Kart::Physics& physics, float dt, float maxSpeed, int air) {
    float gravityMultiplier = 1.0f;
    
    KMP::Manager* kmpMgr = KMP::Manager::sInstance;
    if (kmpMgr != nullptr && kmpMgr->areaSection != nullptr) {
        const Vec3& position = physics.position;
        
        s16 foundIdx = kmpMgr->FindAREA(position, -1, AREA_TYPE_GRAVITY);
        
        if (foundIdx >= 0) {
            AREA* area = kmpMgr->areaSection->holdersArray[foundIdx]->raw;
            float* settingAsFloat = reinterpret_cast<float*>(&area->setting1);
            gravityMultiplier = *settingAsFloat;
        }
    }
    
    physics.gravity *= gravityMultiplier;
    
    physics.Update(false, dt, maxSpeed);
}
kmCall(0x8059fb5c, ApplyAreaGravityMultiplier);

}
}