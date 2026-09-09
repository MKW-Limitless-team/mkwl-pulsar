//Stub of rr-pulsar's CustomCharacters module. This mod does not port custom
//character voices/SFX, so the loose-voice helpers always report not-found and
//the corresponding LooseBRSAROverrides paths stay inert.

#ifndef _CUSTOMCHARACTERS_
#define _CUSTOMCHARACTERS_

#include <kamek.hpp>
#include <core/nw4r/snd/SoundArchive.hpp>

namespace Pulsar {
namespace CustomCharacters {

using nw4r::snd::SoundArchive;

bool FindLooseSoundEffectPath(SoundArchive::FileId fileId, const char* extension, char* path, u32 size);
const char* GetLooseVoicePostfixForGroup(SoundArchive::GroupId groupId, const char*& groupSuffix, const char*& voiceName);

inline bool FindLooseSoundEffectPath(SoundArchive::FileId fileId, const char* extension, char* path, u32 size) {
    return false;
}

inline const char* GetLooseVoicePostfixForGroup(SoundArchive::GroupId groupId, const char*& groupSuffix, const char*& voiceName) {
    return nullptr;
}

}//namespace CustomCharacters
}//namespace Pulsar
#endif