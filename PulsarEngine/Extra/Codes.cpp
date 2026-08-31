#include <kamek.hpp>
#include <MarioKartWii/Race/RaceData.hpp>
#include <MarioKartWii/Race/Raceinfo/Raceinfo.hpp>

namespace Pulsar {
    // Points Distribution Modifier [Gaberboo]
    static const u8 customPointsRoom[12][12] = {
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1},
        {15, 13, 11,  9,  8,  7,  6,  5,  4,  3,  2,  1}
    };

    kmOnLoadDefCpp() {
        memcpy((void*)Racedata::pointsRoom, customPointsRoom, sizeof(customPointsRoom));
        return 0;
    }

    // Instant Voting Roulette Decide [Ro] https://mariokartwii.com/showthread.php?tid=2123
    kmWrite32(0x80643BC4, 0x60000000);
    kmWrite32(0x80643C2C, 0x60000000);

    // No item drop in TT/OTT [Ro]
    kmWrite32(0x80790C48, 0x38600001);

    // Item Damage Type Modifier [CLF78, Skullface, Supastarrio] https://mariokartwii.com/showthread.php?tid=1638
    kmWrite32(0x80573188, 0x38600000); // Banana
    kmWrite32(0x805731CC, 0x38600009); // Bob-omb explosion
    kmWrite32(0x805731D8, 0x38600000); // Bob-omb spinout
    kmWrite32(0x805731B4, 0x38600002); // Blue Shell explosionk
    kmWrite32(0x805731C0, 0x38600000); // Blue Shell spinout
    kmWrite32(0x805733A4, 0x38600002); // Fake Item Box
    kmWrite32(0x805731A8, 0x38600002); // Green/Red Shell
    kmWrite32(0x805811A4, 0x3880000B); // POW Block
    kmWrite32(0x805808A4, 0x3880000A); // Shock
    kmWrite32(0x805808BC, 0x3880000A); // Thunder Cloud 

    // Use the Intended Mii Stats [B_squo] https://mariokartwii.com/showthread.php?tid=2250
    kmWrite8(0x80592163, 0x18);
    kmWrite8(0x80592143, 0xE8);

    // Remove Worldwide Option [Chadderz] https://mariokartwii.com/showthread.php?tid=994
    kmWrite16(0x8064B982, 0x0005);
    kmWrite32(0x8064BA10, 0x60000000);
    kmWrite32(0x8064BA38, 0x60000000);
    kmWrite32(0x8064BA50, 0x60000000);
    kmWrite32(0x8064BA5C, 0x60000000);
    kmWrite16(0x8064BC12, 0x0001);
    kmWrite16(0x8064BC3E, 0x0484);
    kmWrite16(0x8064BC4E, 0x10D7);
    kmWrite16(0x8064BCB6, 0x0484);
    kmWrite16(0x8064BCC2, 0x10D7);

    // Instant Squash Recovery [Nutmeg] https://mariokartwii.com/showthread.php?tid=176
    kmWrite32(0x8057982C, 0x38000000);

    // Replace Mushroom When No Item Available; Offline Only [Vega] https://mariokartwii.com/showthread.php?tid=2056
    kmWrite32(0x807BA194, 0x38000009);

    // Disable Item Poof [CLF78] https://mariokartwii.com/showthread.php?tid=1716
    kmWrite32(0x807965C0, 0x60000000);

    // Allow Mega in a Mega [Unnamed] https://mariokartwii.com/showthread.php?tid=1939
    kmWrite32(0x807BB764, 0x60000000);

    // Allow Pausing Before Race Starts [Sponge] https://mariokartwii.com/showthread.php?tid=2374
    kmWrite32(0x80856A28, 0x40810050);

    // Allow Looking Backwards During the Countdown [Gaberboo] https://mariokartwii.com/showthread.php?tid=2109
    kmWrite32(0x805A225C, 0x38800001);

    // Cones Don't Slow [LucioWins] https://mariokartwii.com/showthread.php?tid=2382
    kmWrite32(0x80573778, 0xD0230020);

    // Prediction Removal [Stebler] https://mariokartwii.com/showthread.php?tid=1929
    kmWrite32(0x80891B28, 0x3F800000);

    // WiiLink Compatibility for Wiimmfi Patched Games [mkwcat]
    kmWrite32(0x800EE3A0, 0x2C030000);
    kmWrite32(0x800ECAAC, 0x7C7E1B78);

    // Fix Offroad Affecting Star After Cannon Glitch [Ro] https://mariokartwii.com/showthread.php?tid=2307
    asmFunc FixOffroadAffectingStarAfterCannonGlitch() {
        ASM(
            nofralloc;
            andi. r11, r0, 0x80;
            andis. r12, r0, 0x8000;
            or. r0, r11, r12;
            blr;
        )
    }
    kmBranch(0x8057C3F8, FixOffroadAffectingStarAfterCannonGlitch);
    kmPatchExitPoint(FixOffroadAffectingStarAfterCannonGlitch, 0x8057C3FC);

    // Blue Shell Cooldown [Gaberboo] https://mariokartwii.com/showthread.php?tid=2180
    extern "C" void ItemHolderPlayer_useBlooper();
    asmFunc BlueShellCooldownHook() {
        ASM(
            nofralloc;
            lis r10, (ItemHolderPlayer_useBlooper + 0x17a)@ha;
            lhzu r9, (ItemHolderPlayer_useBlooper + 0x17a)@l(r10);
            lha r8, 0x4(r10);
            rlwinm r9, r9, 16, 0, 15;
            lwzx r7, r9, r8;
            li r10, 0x4b0;
            stw r10, 0x38(r7);
            stwu r1, -0x20(r1);
            blr;
        )
    }
    kmBranch(0x807AC634, BlueShellCooldownHook);
    kmPatchExitPoint(BlueShellCooldownHook, 0x807AC638);

    // Item Box Respawn Timer Modifier [Unnamed] https://mariokartwii.com/showthread.php?tid=2206
    asmFunc ItemBoxRespawnTimerModifier() {
        ASM(
            nofralloc;
            li r12, 0x69;
            stw r12, 0xb8(r27);
            stw r0, 0xb0(r27);
            blr;
        )
    }
    kmBranch(0x80828EDC, ItemBoxRespawnTimerModifier);
    kmPatchExitPoint(ItemBoxRespawnTimerModifier, 0x80828EE0);

    // Anti Lag/Late Start Online [Ro] https://mariokartwii.com/showthread.php?tid=2318
    extern "C" Racedata* sInstance__8Racedata;

    asmFunc AntiLagLateStartOnline() {
        ASM(
            nofralloc;
            lis r12, sInstance__8Racedata@ha;
            lwz r12, sInstance__8Racedata@l(r12);
            lwz r12, 0xB70(r12);
            cmpwi r12, 7;
            blt AntiLagLateStartOnlineEnd;
            li r3, 1;
        AntiLagLateStartOnlineEnd:
            cmpwi r3, 0;
            blr;
        )
    }
    kmBranch(0x80533430, AntiLagLateStartOnline);
    kmPatchExitPoint(AntiLagLateStartOnline, 0x80533434);
}
