#ifndef _PLAYSTYLESELECT_
#define _PLAYSTYLESELECT_

#include <kamek.hpp>
#include <MarioKartWii/UI/Page/Menu/Menu.hpp>
#include <MarioKartWii/UI/Ctrl/PushButton.hpp>
#include <MarioKartWii/UI/Ctrl/SheetSelect.hpp>
#include <UI/UI.hpp>

namespace Pulsar {
namespace UI {

//per-local-player selected playstyle index; defined in CustomVehicles.cpp
extern u8 playstyles[4];

class PlaystyleSelect;

//A-click gate installed as the plate's FORWARD_PRESS action handler: swallows the
//press entirely while the player's selection is confirmed, otherwise forwards
struct PlateClickGate : PtmfHolder_2A<LayoutUIControl, void, u32, u32> {
    PlaystyleSelect* page;
    u32 hud;
    void operator()(u32 hudSlotId, u32 r5) const override;
};

//local multiplayer style picker: per player an "arrow cycler" widget (left arrow |
//style name | right arrow); A confirms, B undoes; all confirmed -> cup select
class PlaystyleSelect : public Pages::MenuInteractable {
public:
    static const PageId id = PULPAGE_PLAYSTYLESELECT;

    PlaystyleSelect();
    ~PlaystyleSelect() override;

    void OnInit() override;
    void OnActivate() override;

    void SetButtonHandlers(PushButton& button) override;
    UIControl* CreateControl(u32 id) override;
    UIControl* CreateExternalControl(u32 id) override;
    bool IsConfirmed(u32 hudSlotId) const { return this->confirmed[hudSlotId]; }
    PushButton& GetStyleButton(u32 hudSlotId) { return this->styleButtons[hudSlotId]; }
    void ClearPlayerSelection(u8 hud);
    void RestorePlayerSelection(u8 hud);

    ManipulatorManager& GetManipulatorManager() override;
    int GetPlayerBitfield() const override;
    int GetActivePlayerBitfield() const override;
    const ut::detail::RuntimeTypeInfo* GetRuntimeTypeInfo() const override;
    void OnExternalButtonSelect(PushButton& button, u32 r5) override;

    void OnButtonClick(PushButton& button, u32 hudSlotId);
    void OnButtonDeselect(PushButton& button, u32 hudSlotId);
    void OnRightArrow(SheetSelectControl& control, u32 hudSlotId);
    void OnLeftArrow(SheetSelectControl& control, u32 hudSlotId);
    void OnBackPress(u32 hudSlotId) override;
    void OnStartPress(u32 hudSlotId) override;

private:
    void CycleStyle(u8 hud, int step);
    void UpdateStyleDisplay(u8 hud);
    bool AllConfirmed() const;
    bool AnyConfirmed() const;

    u32 playerCount;
    PushButton* styleButtons;      //one display plate per player
    SheetSelectControl* arrows;    //one arrow pair per player
    PlateClickGate clickGates[4];  //per-plate A-press gates (persistent holders)
    const PtmfHolder_2A<LayoutUIControl, void, u32, u32>* savedScrollHandlers[4];
    const PtmfHolder_2A<LayoutUIControl, void, u32, u32>* savedRightHandlers[4];
    bool confirmed[4];
    bool transitionPending;        //a delayed page load is scheduled; ignore all input
    PtmfHolder_2A<Page, void, PushButton&, u32> onButtonClickHandler;
    PtmfHolder_2A<Page, void, PushButton&, u32> onButtonDeselectHandler;
    PtmfHolder_2A<Page, void, SheetSelectControl&, u32> onRightArrowHandler;
    PtmfHolder_2A<Page, void, SheetSelectControl&, u32> onLeftArrowHandler;
    PtmfHolder_1A<MenuInteractable, void, u32> onBackPressHandler;
};

}//namespace UI
}//namespace Pulsar

#endif
