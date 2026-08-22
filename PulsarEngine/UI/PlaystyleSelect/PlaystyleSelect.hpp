#ifndef _PLAYSTYLESELECT_
#define _PLAYSTYLESELECT_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

//per-local-player selected playstyle index (0 = balanced); consumed by the stats hook
extern u8 playstyles[4];

class PlaystyleSelect : public Pages::MenuInteractable {
public:
    static const PageId id = PULPAGE_PLAYSTYLESELECT;

    PlaystyleSelect();
    ~PlaystyleSelect() override;

    void OnInit() override;
    void OnActivate() override;
    void AfterControlUpdate() override;

    void SetButtonHandlers(PushButton& button) override;

    UIControl* CreateControl(u32 id) override;
    UIControl* CreateExternalControl(u32 id) override;

    ManipulatorManager& GetManipulatorManager() override;
    int GetPlayerBitfield() const override;
    int GetActivePlayerBitfield() const override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    void OnExternalButtonSelect(PushButton& button, u32 r5) override;

    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonDeselect(PushButton& button, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId) override;
    void OnStartPress(u32 hudSlotId) override;

    static Page* GetPageById(PulPageId id);

private:
    u32 playstyleCount;
    u32 totalButtons;
    PushButton* playstyleButtons;
    PtmfHolder_2A<Page, void, PushButton&, u32> onButtonClickHandler;
    PtmfHolder_2A<Page, void, PushButton&, u32> onButtonDeselectHandler;
    PtmfHolder_1A<MenuInteractable, void, u32> onBackPressHandler;
    PtmfHolder_1A<MenuInteractable, void, u32> onStartPressHandler;
};

}//namespace UI
}//namespace Pulsar

#endif