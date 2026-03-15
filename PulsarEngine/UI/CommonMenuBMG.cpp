#include <kamek.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>
#include <Settings/Settings.hpp>

namespace Pulsar {
namespace UI {

namespace {
static const char commonBmgPath[] = "message/Common.bmg";
static const char commonMenuBmgPath[] = "message/CommonMenu.bmg";

static bool StringsEqual(const char* lhs, const char* rhs) {
    if(lhs == nullptr || rhs == nullptr) return false;
    while(*lhs != '\0' && *rhs != '\0') {
        if(*lhs != *rhs) return false;
        ++lhs;
        ++rhs;
    }
    return *lhs == *rhs;
}

static bool UseCommonMenuBMG() {
    const u8 value = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_MENU, SETTINGMENU_RADIO_COMMONBMG);
    return value == MENUSETTING_COMMONBMG_ENABLED;
}
}

static void* CommonMenuBMGGetFile(const ArchiveMgr& archiveMgr, ArchiveSource source, const char* path, u32* size) {
    if(source == ARCHIVE_HOLDER_UI && UseCommonMenuBMG() && StringsEqual(path, commonBmgPath)) {
        void* replacement = archiveMgr.GetFile(source, commonMenuBmgPath, size);
        if(replacement != nullptr) return replacement;
    }
    return archiveMgr.GetFile(source, path, size);
}
kmCall(0x805f8bdc, CommonMenuBMGGetFile);

}//namespace UI
}//namespace Pulsar
