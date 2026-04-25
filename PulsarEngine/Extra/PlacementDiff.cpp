#include <kamek.hpp>

namespace Pulsar {

// Show Everyone's Placement Difference From the Previous Race [QB22]
extern "C" void sInstance__8Racedata(void*);
extern "C" void sInstance__10SectionMgr(void*);
extern "C" void SetTextBoxMessage__15LayoutUIControlFPCcUiPCQ24Text4Info(void*);
extern "C" const char PlacementComparePane[] = "position_compare";

asmFunc ShowPlacementDiffHook() {
    ASM(
        nofralloc;
        mr r30, r3;

        lis r11, sInstance__8Racedata @ha;
        lwz r12, sInstance__8Racedata @l(r11);
        cmpwi r12, 0x0;
        beq end;

        lis r11, sInstance__10SectionMgr @ha;
        lwz r11, sInstance__10SectionMgr @l(r11);
        cmpwi r11, 0x0;
        beq end;

        lwz r11, 0x98(r11);
        cmpwi r11, 0x0;
        beq end;

        lwz r5, 0xb70(r12);
        cmpwi r5, 7;
        beq gameModeFriendRoom;
        bgt end;
        cmpwi r5, 0;
        beq gameModeGP;
        lwz r4, 0x60(r11);
        cmpwi r4, 1;
        beq end;
        b processPositions;

    gameModeGP:
        lbz r4, 0xb8c(r12);
        cmpwi r4, 0;
        beq end;
        b processPositions;

    gameModeFriendRoom:
        lwz r4, 0x2d0(r11);
        cmpwi r4, 0;
        beq end;

    processPositions:
        mulli r0, r31, 240;
        addi r6, r12, 40;
        add r8, r6, r0;

        cmpwi r5, 7;
        cmpwi cr1, r5, 0;
        cror 4 * cr0 + eq, 4 * cr0 + eq, 4 * cr1 + eq;
        beq getHexRaceNumber;

        cmpwi r4, 2;
        bne loadFinalPosition;
        b loadPreviousPosition;

    getHexRaceNumber:
        cmpwi r4, 1;
        bne loadFinalPosition;

    loadPreviousPosition:
        lbz r12, 0xE1(r8);
        b comparePlacements;

    loadFinalPosition:
        lbz r12, 0xE0(r8);

    comparePlacements:
        cmpw r24, r12;
        blt raceDiffImprove;
        bgt raceDiffRegress;
        li r5, 0x5e2;
        b getPaneName;

    raceDiffImprove:
        li r5, 0x5e3;
        b getPaneName;

    raceDiffRegress:
        li r5, 0x5e4;

    getPaneName:
        mr r3, r30;
        lis r4, PlacementComparePane @ha;
        addi r4, r4, PlacementComparePane @l;
        li r6, 0;
        lis r12, SetTextBoxMessage__15LayoutUIControlFPCcUiPCQ24Text4Info @ha;
        addi r12, r12, SetTextBoxMessage__15LayoutUIControlFPCcUiPCQ24Text4Info @l;
        mtctr r12;
        bctrl;

    end:
        blr;
    )
}

kmBranch(0x807F530C, ShowPlacementDiffHook);
kmPatchExitPoint(ShowPlacementDiffHook, 0x807F5310);

}//namespace Pulsar
