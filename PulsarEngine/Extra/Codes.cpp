#include <kamek.hpp>

namespace Pulsar {
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

}
