#include <kamek.hpp>
#include <UI/UI.hpp>
#include <MarioKartWii/RKNet/USER.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
namespace Pulsar {
    // Select any Mii in Character Selection Screen (Single Player Menus) [B_squo] https://mariokartwii.com/showthread.php?tid=2434

    extern "C" SectionMgr* sInstance__10SectionMgr;
    extern "C" RKNet::USERHandler* sInstance__Q25RKNet11USERHandler;

    // Small function that adds the Mii Selection page. Accessed from the branches in the lines above. Uses addresses 0x800007B0 thru 0x800007E0 in the original Gecko code.
    // In C++, this'd be the same as having a single function that only has this -> `Section_addPage(section, 0x60);`
    extern "C" void CreateAndInitPage__7SectionF6PageId(void* self, u32 id);
    asmFunc addMiiSelectPage() {
        ASM(
            nofralloc;
            stwu    r1,-16(r1);
            mflr    r0;
            stw     r0,20(r1);
            mr      r3,r31;
            li      r4,0x60;		// PageID 0x60: MiiSelectPage
            bl      CreateAndInitPage__7SectionF6PageId;
            mr      r3,r31;
            lwz     r0,20(r1);
            mtlr    r0;
            addi    r1,r1,16;
            blr;
        )
    }

    // Add Mii Selection page to the sections that have the Character Select page
    kmCall(0x8062D32C, addMiiSelectPage);  // MenuSingle (from Main)
    kmCall(0x8062D470, addMiiSelectPage);  // MenuSingle (from Time Trial - Change Character)
    kmCall(0x8062D5B4, addMiiSelectPage);  // MenuSingle (from Time Trial - Change Course)
    kmCall(0x8062D800, addMiiSelectPage);  // MenuSingle (from unused Mission Mode)
    kmCall(0x8062D86C, addMiiSelectPage);  // MenuSingle (from Mario Kart Channel - Challenge Ghost)
    kmCall(0x8062D8D8, addMiiSelectPage);  // MenuSingle (from Mario Kart Channel - Leaderboard - Challenge Ghost)
    kmCall(0x8062D944, addMiiSelectPage);  // MenuSingle (from Mario Kart Channel - Downloaded Ghost Data List - Challenge Ghost)
    kmCall(0x8062DCF8, addMiiSelectPage);  // Wi-Fi (1P)
    kmCall(0x8062DE9C, addMiiSelectPage);  // Wi-Fi (1P) (from disconnection)
    kmCall(0x8062E040, addMiiSelectPage);  // Wi-Fi (1P) (from Friends)
    kmCall(0x8062E364, addMiiSelectPage);  // Wi-Fi (2P)
    kmCall(0x8062E508, addMiiSelectPage);  // Wi-Fi (2P) (from disconnection)
    kmCall(0x8062E6A0, addMiiSelectPage);  // Wi-Fi (2P) (from Friends)
    kmCall(0x8062F684, addMiiSelectPage);  // Ghost Race (ghost data ready)
    kmCall(0x8062F768, addMiiSelectPage);  // Ghost Race (no ghost data ready)
    kmCall(0x8062F840, addMiiSelectPage);  // Ghost Race (next race)
    kmCall(0x8062FA2C, addMiiSelectPage);  // Friends List (from Mario Kart Channel)
    kmCall(0x8062FBC4, addMiiSelectPage);  // Friends List (from Mario Kart Channel) 2
    kmCall(0x8062FC3C, addMiiSelectPage);  // Competition (from Mario Kart Channel)
    kmCall(0x8062FCB4, addMiiSelectPage);  // Competition (from Change Character) 
    // In-between-races online sections are handled by ChangeCombo.cpp's page bundle.

    // Don't add extra buttons if going to the License settings / Mii Select on create / Mii Select on deleted Mii pages. This avoids a crash due to missing resources in MenuOther.szs, used by said pages.
    // C2847E84 -> Inject this ASM somewhere in memory, then call it from 0x80847e84
    asmFunc removeExtraButtons() {
        ASM(
            nofralloc;
            cmpwi   r0,69;
            beq     end;
            cmpwi   r0,70;
            beq     end;
            cmpwi   r0,71;
        end:
            blr;
        )
    }
    kmBranch(0x80847E84, removeExtraButtons);
    kmPatchExitPoint(removeExtraButtons, 0x80847E88);

    kmWrite32(0x80847E88, 0x41820020);					// beq 0x80847ec0

    // Remove local multiplayer-only checks so that they can pass if accessing other sections.
    kmWrite32(0x80847ED4, 0x60000000);	// nop
    kmWrite32(0x808315B8, 0x60000000);	// nop
    kmWrite32(0x80848858, 0x48000044);	// b 0x8084889c
    kmWrite32(0x80848950, 0x480000F4);	// b 0x80848a44
    kmWrite32(0x8059E3B0, 0x60000000);	// nop

    // After choosing a Mii, copy it into the outgoing local-player group immediately so the next USER packet
    // can advertise it without waiting for the next race transition.
    asmFunc SyncSelectedMiiToNetwork() {
        ASM(
            nofralloc;
            lis     r12, sInstance__10SectionMgr@ha;
            lwz     r3, sInstance__10SectionMgr@l(r12);
            lwz     r3, 0x98(r3);
            addi    r3, r3, 0x238;
            lwz     r4, 0x44(r28);
            lwz     r5, 0xd94(r28);
            mr      r6, r30;

            lis     r12, 0x805f;
            ori     r12, r12, 0xaf34;
            mtctr   r12;
            bctrl;

            lis     r12, sInstance__10SectionMgr@ha;
            lwz     r3, sInstance__10SectionMgr@l(r12);
            lwz     r3, 0x98(r3);
            addi    r3, r3, 0x238;
            mr      r4, r30;
            li      r5, 1;

            lis     r12, 0x805f;
            ori     r12, r12, 0xa940;
            mtctr   r12;
            bctrl;

            lis     r12, sInstance__Q25RKNet11USERHandler@ha;
            lwz     r3, sInstance__Q25RKNet11USERHandler@l(r12);
            cmpwi   r3, 0;
            beq     end;
            li      r4, 0;
            stb     r4, 0x0(r3);

        end:
            lis     r12, sInstance__10SectionMgr@ha;
            lwz     r3, sInstance__10SectionMgr@l(r12);
            blr;
        )
    }
    kmBranch(0x80848A8C, SyncSelectedMiiToNetwork);
    kmPatchExitPoint(SyncSelectedMiiToNetwork, 0x80848A90);

    // The game crashes when selecting a Mii here because of a missing pane named ok_text_Xp (where X is a local player number). I wasn't able to fix this on time, so at the moment I skip this to avoid the crash, but has the side effect of not displaying the small "OK" text in in front of the Mii icon
    kmWrite32(0x807E3BB0, 0x60000000);	// nop

    // Allow loading the Mii Selection page instead of simply choosing the Mii when in single player modes
    kmWrite32(0x807E3928, 0x60000000);	// nop
    kmWrite32(0x807E3944, 0x60000000);	// nop

    // Fixes an issue where selecting a Mii would then load the License's Mii data back
    kmWrite32(0x8083E354, 0x38600000);	// li r3, 0
}
