#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/MultiPlayer.hpp>
#include <MarioKartWii/Item/ItemManager.hpp>
#include <PulsarSystem.hpp>
#include <UI/UI.hpp>
#include <Settings/UI/SettingsPanel.hpp>
#include <Settings/SettingsParam.hpp>

namespace Pulsar {
namespace UI {

void CorrectButtonCount(Pages::MultiPlayer* page) {
    const System* system = System::sInstance;
    page->externControlCount = 3;
    new (page) Page;
}
kmCall(0x8062b260, CorrectButtonCount);
kmWrite32(0x8062b27C, 0x60000000);

UIControl* CreateExternalControls(Pages::MultiPlayer* page, u32 id) {
    if(id == page->externControlCount - 1) {
        PushButton* button = new(PushButton);
        page->AddControl(page->controlCount++, *button, 0);
        const char* name =  "Settings1P";
        button->Load(UI::buttonFolder, name, name, page->activePlayerBitfield, 0, false);
        return button;
    }
    return page->Pages::MultiPlayer::CreateExternalControl(id);

}
kmWritePointer(0x808D9D8C, CreateExternalControls);

void OnButtonSelect(Pages::MultiPlayer* page, PushButton& button, u32 hudSlotId) {
    const s32 id = button.buttonId;
    u32 bmgId;
    if(id == 2) bmgId = BMG_SETTINGSBUTTON_BOTTOM;
    else bmgId = 0xbed + id; 
    page->bottomText->SetMessage(bmgId);
}
kmWritePointer(0x808D9D6C, &OnButtonSelect);

void OnButtonClick(Pages::MultiPlayer* page, PushButton& button, u32 hudSlotId) {
    const u32 id = button.buttonId;
    if(id == page->externControlCount - 1) {
        ExpSection::GetSection()->GetPulPage<SettingsPanel>()->prevPageId = PAGE_MULTIPLAYER_MENU;
        page->nextPageId = static_cast<PageId>(SettingsPanel::id);
        page->EndStateAnimated(0, button.GetAnimationFrameSize());
        return;
    }
    page->Pages::MultiPlayer::OnButtonClick(button, hudSlotId);
}
kmWritePointer(0x808BC638, OnButtonClick);

//Disable Corrupt THP from playing
kmWrite8(0x808ADDE0, 0x73);
}//namespace UI
}//namespace Pulsar