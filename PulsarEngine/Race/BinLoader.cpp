#include <Settings/Settings.hpp>
#include <MarioKartWii/Archive/ArchiveMgr.hpp>

namespace Pulsar {
    

void *GetCustomKartAIParam(ArchiveMgr *archive, ArchiveSource type, const char *name, u32 *length) {
    bool isEnabled = Settings::Mgr::Get().GetSettingValue(Settings::SETTINGSTYPE_MENU, SETTINGMENU_RADIO_HARDAI) == MENUSETTING_HARDAI_ENABLED;
    
    if (isEnabled) {
        name = "kartAISpdParamMKWL.bin";
    }

    return archive->GetFile(type, name, length);
}
kmCall(0x8073ae9c, GetCustomKartAIParam);
} // namespace Pulsar