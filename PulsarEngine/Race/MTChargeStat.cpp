#include <kamek.hpp>

namespace Pulsar {
namespace Race {

// MT Charge Stat [Gaberboo]
asmFunc BikeMTChargeStatReturn() {
    ASM(
        nofralloc;
        blr;
    )
}
kmPatchExitPoint(BikeMTChargeStatReturn, 0x805888A4);

asmFunc MTChargeStatShared() {
    ASM(
        nofralloc;
        stwu r1, -0x10(r1);
        lwz r9, 0x0(r3);
        lwz r9, 0x0(r9);
        lwz r9, 0x14(r9);
        lwz r9, 0x0(r9);
        opword 0xE00900A0; // psq_l f0, 0xA0(r9), 0, 0
        opword 0xF0015008; // psq_st f0, 0x8(r1), 0, 3
        lha r8, 0x8(r1);
        lha r10, 0xA(r1);
        addi r1, r1, 0x10;
        bne cr1, MTChargeStatKartReturn;
        lis r12, BikeMTChargeStatReturn@ha;
        addi r12, r12, BikeMTChargeStatReturn@l;
        mtctr r12;
        bctr;
    MTChargeStatKartReturn:
        blr;
    )
}
kmPatchExitPoint(MTChargeStatShared, 0x8057EE80);

asmFunc KartMTChargeStat() {
    ASM(
        nofralloc;
        crclr 4 * cr1 + eq;
        lis r12, MTChargeStatShared@ha;
        addi r12, r12, MTChargeStatShared@l;
        mtctr r12;
        bctr;
    )
}
kmBranch(0x8057EE7C, KartMTChargeStat);

asmFunc BikeMTChargeStat() {
    ASM(
        nofralloc;
        crset 4 * cr1 + eq;
        lis r12, MTChargeStatShared@ha;
        addi r12, r12, MTChargeStatShared@l;
        mtctr r12;
        bctr;
    )
}
kmBranch(0x805888A0, BikeMTChargeStat);

kmWrite32(0x8057EF2C, 0x7D485378);

} // namespace Race
} // namespace Pulsar
