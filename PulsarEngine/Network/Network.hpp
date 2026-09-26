#ifndef _PUL_NETWORK_
#define _PUL_NETWORK_

#include <Config.hpp>

namespace Pulsar {
namespace Network {

//Maximum number of blocked tracks that are synced through the ROOM and SELECT packets
static const u32 MAX_TRACK_BLOCKING = 12;

enum DenyType {
    DENY_TYPE_NORMAL,
    DENY_TYPE_BAD_PACK,
    DENY_TYPE_OTT,
};

class Mgr { //Manages network related stuff within Pulsar
public:
    Mgr() : racesPerGP(3), curBlockingArrayIdx(0) {}
    u32 hostContext;
    DenyType denyType;
    u8 deniesCount;
    u8 ownStatusData;
    u8 statusDatas[30];
    u8 curBlockingArrayIdx;
    u8 racesPerGP;
    u8 padding[2];
    PulsarId* lastTracks;
};

}//namespace Network
}//namespace Pulsar

#endif