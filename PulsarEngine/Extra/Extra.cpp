
#include <kamek.hpp>

namespace Pulsar {
    // Instant Voting Roulette Decide [Ro]
    kmWrite32(0x80643BC4, 0x60000000);
    kmWrite32(0x80643C2C, 0x60000000);

    // Placement Difference mod [QuestionBlock22]
    asmFunc PlacementDifference() {
        ASM(
            nofralloc;
            lis r11, 0x809C;
            lwz r12, 0x0B70(r12);
            cmpwi r0, 0;
            bne gameModeGP;
            lwz r12, 0x1E38(r11);
            lwz r11, 0x0098(r12);
            lwz r0, 0x0060(r11);
            cmpwi r0, 1;
            bne processPositions;
            b end;
        gameModeGP:;
            lbz r0, 0x0B8C(r12);
            cmpwi r0, 0;
            bne processPositions;
            b end;
        processPositions:;
            lwz r11, 0xD728(r8);
            mulli r0, r31, 240;
            addi r6, r11, 40;
            add r8, r6, r0;
            lbz r12, 0x00E1(r8);
            cmpw r24, r12;
            blt raceDiffImprove;
            bgt raceDiffRegress;
            li r5, 0x5E2;
            b getPaneName;
        raceDiffImprove:;
            li r5, 0x5E3;
            b getPaneName;
        raceDiffRegress:;
            li r5, 0x5E4;
        getPaneName:;
            lis r3, 0x706F;
            ori r3, r3, 0x7369;
            lis r4, 0x7469;
            ori r4, r4, 0x6F6E;
            lis r5, 0x5F63;
            ori r5, r5, 0x6F6D;
            lis r6, 0x7061;
            ori r6, r6, 0x7265;
            lis r11, 0x8063;
            li r6, 0;
            ori r11, r11, 0xDCBC;
            blr;
            nop;
            nop;
        end:;
        )
    }
    kmCall(0x807f530c, PlacementDifference);
}

