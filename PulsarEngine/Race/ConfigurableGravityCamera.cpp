#include <kamek.hpp>
#include <MarioKartWii/Kart/KartManager.hpp>
#include <MarioKartWii/KMP/KMPManager.hpp>

namespace Pulsar {
namespace Race {

extern float GetAreaGravityMultiplier(u8 playerId);

typedef void (*CameraFallingFuncType)(int cameraData, float* cameraParams, void* kartProxy, int dirFlag);

static u8 GetPlayerIdFromKartProxy(void* kartProxy) {
    if (kartProxy == nullptr) return 0;
    
    Kart::Manager* kartMgr = Kart::Manager::sInstance;
    if (kartMgr == nullptr) return 0;
    
    for (u8 i = 0; i < kartMgr->playerCount; i++) {
        Kart::Player* player = kartMgr->players[i];
        if (player != nullptr) {
            if ((void*)player == kartProxy) {
                return i;
            }
        }
    }
    
    return 0;
}

// void CameraFallingAdjustment_Hook(int cameraData, float* cameraParams, void* kartProxy, int dirFlag) {
//     u8 playerId = GetPlayerIdFromKartProxy(kartProxy);
//     float gravityMult = GetAreaGravityMultiplier(playerId);
    

//     if (gravityMult != 1.0f) {
//         float interpRate = 0.1f;
//         cameraParams[6] = cameraParams[6] * (1.0f - interpRate);  
//         return;  
//     }
    
//     // Call the original camera falling adjustment function
//     CameraFallingFuncType originalFunc = (CameraFallingFuncType)0x805a463c;
//     originalFunc(cameraData, cameraParams, kartProxy, dirFlag);
// }

// kmCall(0x805a3860, CameraFallingAdjustment_Hook);

} // namespace Race
} // namespace Pulsar