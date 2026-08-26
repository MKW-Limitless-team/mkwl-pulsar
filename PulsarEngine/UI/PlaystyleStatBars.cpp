#include <UI/PlaystyleStatBars.hpp>
#include <UI/CustomVehicles/CustomVehicles.hpp>
#include <MarioKartWii/UI/Ctrl/Menu/CtrlMenuMachineGraph.hpp>
#include <MarioKartWii/UI/Section/SectionMgr.hpp>
#include <MarioKartWii/System/Identifiers.hpp>

namespace Pulsar {
namespace UI {
namespace StatBars {

//Vehicle-select stat bars sourced from kartParam.bin (including our playstyle tables),
//normalized with P5..P95 percentile clamping.  ability_graph shows the vanilla/base
//stat value.  Two overlay pic1 panes in the BRLYT show playstyle differences:
//ps_red (red, in front of base) appears when a stat is nerfed.
//ps_blue (blue, behind base) appears when a stat is buffed.
//Both are hidden when the style is vanilla or the stat is unchanged.

struct PageGraphInfo {
    PageId id;
    u32 graphOffset;
    u32 graphCount;
};

static const PageGraphInfo pageGraphs[3] = {
    {PAGE_KART_SELECT, 0x87c, 1},
    {PAGE_BATTLE_KART_SELECT, 0x9cc, 1},
    {PAGE_MULTIPLAYER_KART_SELECT, 0x3F78, 2},
};
static const u32 abilityStride = 0x17C;
static const u32 abilityPaneOffset = 0x174;

static const char* STYLE_PANE_LOSS = "loss";
static const char* STYLE_PANE_GAIN = "gain";

static const u32 COLOR_BASE_LEFT  = 0x1B28E7FF;
static const u32 COLOR_BASE_RIGHT = 0xDB62DDFF;
static const u32 COLOR_LOSS_LEFT  = 0xFF6464FF;
static const u32 COLOR_LOSS_RIGHT = 0xFF0000FF;
static const u32 COLOR_GAIN_LEFT  = 0x00FF88FF;
static const u32 COLOR_GAIN_RIGHT = 0x00FF11FF;

static const char* KART_PARAM_PATH = "/Pulsar/UI/playstyle_kartparam.bin";
static const u32 ENTRY_SIZE = 396;
static const u32 VEHICLE_COUNT = 36;
static const u32 STYLE_COUNT_REAL = 4;
static const u8* kartParamEntries = nullptr;
static bool kartParamLoadAttempted = false;

// Icon classification thresholds
static const float DRIFT_HYBRID_MAX = 40.0f;
static const float DRIFT_OUTSIDE_MAX = 55.0f;
static const u32 CORSA_MT_THRESHOLD = 10;
static const float PERF_TRANS_THRESHOLD = 0.0f;
static const float QC_THRESHOLD = 270.0f;

// Icon material names in BRLYT mat1 section (must match JSON5)
// Index maps: 0=inside, 1=hybrid, 2=outside, 3=wide, 4=corsa, 5=perf_trans, 6=qc
static const char* ICON_MAT_SOURCE_NAMES[7] = {
    "icon_slot_0",    // material 7 = mat_icon_inside
    "icon_slot_1",    // material 8 = mat_icon_hybrid
    "icon_slot_2",    // material 9 = mat_icon_outside
    "mat_src_wide",   // material 10 = mat_icon_wide
    "mat_src_corsa",  // material 11 = mat_icon_corsa
    "mat_src_perf",   // material 12 = mat_icon_perf_trans
    "mat_src_qc"      // material 13 = mat_icon_qc
};

// 3 fixed-position icon slot panes in BRLYT (pre-positioned under mini turbo bar)
static const char* ICON_SLOT_NAMES[3] = {
    "icon_slot_0", "icon_slot_1", "icon_slot_2"
};
static const u32 NUM_ICON_SLOTS = 3;

// Cached material pointers (filled at init from source panes)
static nw4r::lyt::Material* iconMaterials[7] = {};
static bool iconMatsCached = false;

enum StatIndex {
    STAT_SPEED,
    STAT_WEIGHT,
    STAT_ACCEL,
    STAT_HANDLING,
    STAT_DRIFT,
    STAT_OFFROAD,
    STAT_MINITURBO,
    STAT_COUNT
};

static bool rangesComputed = false;
static float rangeLo[STAT_COUNT];
static float rangeHi[STAT_COUNT];

static float ReadEntryFloat(const u8* entry, u32 offset) {
    return *reinterpret_cast<const float*>(entry + offset);
}

static float ReadAcceleration(const u8* entry) {
    float a[4] = {
        ReadEntryFloat(entry, 0x24),
        ReadEntryFloat(entry, 0x28),
        ReadEntryFloat(entry, 0x2C),
        ReadEntryFloat(entry, 0x30),
    };
    float t[3] = {
        ReadEntryFloat(entry, 0x34),
        ReadEntryFloat(entry, 0x38),
        ReadEntryFloat(entry, 0x3C),
    };
    for(u32 i = 0; i < 3; ++i) {
        for(u32 j = i + 1; j < 3; ++j) {
            if(t[j] < t[i]) {
                float tmp = t[i];
                t[i] = t[j];
                t[j] = tmp;
            }
        }
        if(t[i] < 0.0f) t[i] = 0.0f;
        if(t[i] > 1.0f) t[i] = 1.0f;
    }
    float ranges[5] = {0.0f, t[0], t[1], t[2], 1.0f};
    float weightedSum = 0.0f;
    for(u32 i = 0; i < 4; ++i) {
        weightedSum += a[i] * (ranges[i + 1] - ranges[i]);
    }
    return weightedSum;
}

static float ExtractStat(const u8* entry, u32 stat) {
    switch(stat) {
        case STAT_SPEED: return ReadEntryFloat(entry, 0x18);
        case STAT_WEIGHT: return ReadEntryFloat(entry, 0x10);
        case STAT_ACCEL: return ReadAcceleration(entry);
        case STAT_HANDLING: return ReadEntryFloat(entry, 0x4C);
        case STAT_DRIFT: return ReadEntryFloat(entry, 0x58);
        case STAT_OFFROAD:
            return ReadEntryFloat(entry, 0x78) + ReadEntryFloat(entry, 0x7C)
                 + ReadEntryFloat(entry, 0x80);
        case STAT_MINITURBO: return static_cast<float>(*reinterpret_cast<const u32*>(entry + 0x6C));
        default: return 0.0f;
    }
}

static void Sort(float* values, u32 count) {
    for(u32 i = 1; i < count; ++i) {
        const float key = values[i];
        u32 j = i;
        while(j > 0 && values[j - 1] > key) {
            values[j] = values[j - 1];
            --j;
        }
        values[j] = key;
    }
}

static void ComputeRanges() {
    if(kartParamEntries == nullptr) return;
    const u32 total = VEHICLE_COUNT * STYLE_COUNT_REAL;
    float values[VEHICLE_COUNT * STYLE_COUNT_REAL];
    for(u32 stat = 0; stat < STAT_COUNT; ++stat) {
        for(u32 e = 0; e < total; ++e) {
            values[e] = ExtractStat(kartParamEntries + e * ENTRY_SIZE, stat);
        }
        Sort(values, total);
        rangeLo[stat] = values[(5 * (total - 1)) / 100];
        rangeHi[stat] = values[(95 * (total - 1)) / 100];
    }
    rangesComputed = true;
}

static void SetVisibleBit(nw4r::lyt::Pane* pane, bool visible) {
    if(visible) pane->flag |= 0x1;
    else pane->flag &= static_cast<u8>(~0x1);
}

static u8 ClassifyDriftType(const u8* entry) {
    float angle = ReadEntryFloat(entry, 0x64);
    if(angle <= 0.0f) return 0;
    if(angle < DRIFT_HYBRID_MAX) return 1;
    if(angle < DRIFT_OUTSIDE_MAX) return 2;
    return 3;
}

static u32 ClassifySpecialIcons(const u8* entry, u32* outTypes) {
    u32 count = 0;
    u32 mt = *reinterpret_cast<const u32*>(entry + 0x6C);
    if(mt < CORSA_MT_THRESHOLD) outTypes[count++] = 4;
    float da1 = ReadEntryFloat(entry, 0x44);
    if(da1 < PERF_TRANS_THRESHOLD) outTypes[count++] = 5;
    float ws = ReadEntryFloat(entry, 0xA0);
    if(ws < QC_THRESHOLD) outTypes[count++] = 6;
    return count;
}

static void CacheIconMaterials(nw4r::lyt::Pane* parent) {
    if(parent == nullptr || iconMatsCached) return;
    for(u32 i = 0; i < 7; ++i) {
        nw4r::lyt::Pane* src = parent->FindPaneByName(ICON_MAT_SOURCE_NAMES[i], true);
        if(src != nullptr) {
            iconMaterials[i] = src->material;
        }
    }
    iconMatsCached = true;
}

static void UpdateIcons(nw4r::lyt::Pane* parent, KartId kart, u8 style) {
    if(parent == nullptr || kartParamEntries == nullptr || kart >= VEHICLE_COUNT) return;

    CacheIconMaterials(parent);

    const u8* styledEntry = kartParamEntries + (kart + VEHICLE_COUNT * style) * ENTRY_SIZE;

    u8 driftType = ClassifyDriftType(styledEntry);
    u32 specialTypes[3];
    u32 specialCount = ClassifySpecialIcons(styledEntry, specialTypes);

    u32 totalCount = specialCount + 1;
    if(totalCount > 3) totalCount = 3;

    u32 slotIconIdx[3] = {0, 0, 0};
    u32 slotIdx = 0;
    slotIconIdx[slotIdx++] = driftType;
    for(u32 i = 0; i < specialCount && slotIdx < 3; ++i) {
        slotIconIdx[slotIdx++] = specialTypes[i];
    }

    for(u32 i = 0; i < NUM_ICON_SLOTS; ++i) {
        nw4r::lyt::Pane* pane = parent->FindPaneByName(ICON_SLOT_NAMES[i], true);
        if(pane == nullptr) continue;

        if(i < totalCount) {
            nw4r::lyt::Material* mat = iconMaterials[slotIconIdx[i]];
            if(mat != nullptr) {
                pane->material = mat;
            }
            u32 paneAddr = reinterpret_cast<u32>(pane);
            u32 white = 0xFFFFFFFF;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 0) = white;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 4) = white;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 8) = white;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 12) = white;
            SetVisibleBit(pane, true);
        } else {
            u32 paneAddr = reinterpret_cast<u32>(pane);
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 0) = 0;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 4) = 0;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 8) = 0;
            *reinterpret_cast<u32*>(paneAddr + 0xD8 + 12) = 0;
            SetVisibleBit(pane, false);
        }
    }
}

static bool IsMultiplayerPage(const CtrlMenuMachineGraph* graph);
static const char* BUTTON_PROMPT_PANE = "button_prompt";
static const u32 BMG_BUTTON_WHEEL   = 0x8800;
static const u32 BMG_BUTTON_NUNCHUK = 0x8801;
static const u32 BMG_BUTTON_CLASSIC = 0x8802;
static const u32 BMG_BUTTON_GCN     = 0x8803;

static ControllerType GetControllerType(u8 hud) {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return GCN;
    if(hud >= 4) return GCN;
    const Input::RealControllerHolder* holder = mgr->pad.padInfos[hud].controllerHolder;
    if(holder == nullptr || holder->curController == nullptr) return GCN;
    const ControllerType type = holder->curController->GetType();
    if(type == WHEEL || type == NUNCHUCK || type == CLASSIC || type == GCN) return type;
    return GCN;
}

static u32 ControllerToBmgId(ControllerType type) {
    switch(type) {
        case WHEEL:    return BMG_BUTTON_WHEEL;
        case NUNCHUCK: return BMG_BUTTON_NUNCHUK;
        case CLASSIC:  return BMG_BUTTON_GCN;
        case GCN:
        default:       return BMG_BUTTON_CLASSIC;
    }
}

static void UpdateButtonPrompt(CtrlMenuMachineGraph* graph, u8 hud) {
    if(IsMultiplayerPage(graph)) return;
    MachineAbility* abArray = graph->abilityArray;
    if(abArray == nullptr) return;

    nw4r::lyt::Pane* promptPane = abArray[0].layout.GetPaneByName(BUTTON_PROMPT_PANE);
    if(promptPane == nullptr) return;

    promptPane->flag |= 0x1;

    ControllerType ctrlType = GetControllerType(hud);
    u32 bmgId = ControllerToBmgId(ctrlType);

    Text::Info info;
    info.playerId[0] = hud;
    abArray[0].SetTextBoxMessage(BUTTON_PROMPT_PANE, bmgId, &info);
}

static CtrlMenuMachineGraph* LocateGraphForHud(u8 hud) {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return nullptr;
    for(u32 i = 0; i < 3; ++i) {
        const PageGraphInfo& info = pageGraphs[i];
        if(hud >= info.graphCount) continue;
        Page* page = mgr->curSection->pages[info.id];
        if(page == nullptr) continue;
        return reinterpret_cast<CtrlMenuMachineGraph*>(reinterpret_cast<u32>(page)
            + info.graphOffset + hud * 0x184);
    }
    return nullptr;
}

static u8 HudForGraph(const CtrlMenuMachineGraph* graph) {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return 0;
    const u32 g = reinterpret_cast<u32>(graph);
    for(u32 i = 0; i < 3; ++i) {
        const PageGraphInfo& info = pageGraphs[i];
        Page* page = mgr->curSection->pages[info.id];
        if(page == nullptr) continue;
        const u32 base = reinterpret_cast<u32>(page) + info.graphOffset;
        if(g >= base && g < base + info.graphCount * 0x184) {
            return static_cast<u8>((g - base) / 0x184);
        }
    }
    return 0;
}

static bool IsMultiplayerPage(const CtrlMenuMachineGraph* graph) {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return false;
    const u32 g = reinterpret_cast<u32>(graph);
    for(u32 i = 0; i < 3; ++i) {
        const PageGraphInfo& info = pageGraphs[i];
        Page* page = mgr->curSection->pages[info.id];
        if(page == nullptr) continue;
        const u32 base = reinterpret_cast<u32>(page) + info.graphOffset;
        if(g >= base && g < base + info.graphCount * 0x184) {
            return info.graphCount > 1;
        }
    }
    return false;
}

void GraphUpdateHook(CtrlMenuMachineGraph* graph, CharacterId character, KartId kart) {
    (void)character;
    if(graph == nullptr || kart >= VEHICLE_COUNT) return;

    if(kartParamEntries == nullptr && !kartParamLoadAttempted) {
        kartParamLoadAttempted = true;
        DVD::FileInfo info;
        if(DVD::Open(KART_PARAM_PATH, &info)) {
            const s32 size = static_cast<s32>(info.length);
            DVD::Close(&info);
            const s32 readSize = (size + 31) & ~31;
            static u8 fileBuffer[VEHICLE_COUNT * STYLE_COUNT_REAL * ENTRY_SIZE + 4 + 32];
            u8* alignedBuf = reinterpret_cast<u8*>(
                (reinterpret_cast<u32>(fileBuffer) + 31) & ~31u);
            if(size > 0 && static_cast<u32>(readSize) <= sizeof(fileBuffer)
                && DVD::Open(KART_PARAM_PATH, &info)) {
                DVD::ReadPrio(&info, alignedBuf, readSize, 0, 2);
                DVD::Close(&info);
                kartParamEntries = alignedBuf + 4;
            }
        }
    }
    if(kartParamEntries == nullptr) return;
    if(!rangesComputed) ComputeRanges();
    if(!rangesComputed) return;

    const u8 hud = HudForGraph(graph);
    const u8 style = hud < 4 ? playstyles[hud] : 0;

    u8* abilities = *reinterpret_cast<u8**>(
        reinterpret_cast<u32>(graph) + 0x174);
    if(abilities == nullptr) return;

    for(u32 stat = 0; stat < STAT_COUNT; ++stat) {
        u8* ab = abilities + stat * abilityStride;
        nw4r::lyt::Pane* basePane = *reinterpret_cast<nw4r::lyt::Pane**>(ab + abilityPaneOffset);
        if(basePane == nullptr) continue;

        const float lo = rangeLo[stat];
        const float hi = rangeHi[stat];
        const u8* baseEntry = kartParamEntries + kart * ENTRY_SIZE;
        const u8* styledEntry = kartParamEntries + (kart + VEHICLE_COUNT * style) * ENTRY_SIZE;
        const float baseValue = ExtractStat(baseEntry, stat);
        const float styledValue = ExtractStat(styledEntry, stat);

        float clampedBase = baseValue;
        if(clampedBase < lo) clampedBase = lo;
        if(clampedBase > hi) clampedBase = hi;
        const float baseNorm = hi > lo ? (clampedBase - lo) / (hi - lo) : 0.5f;

        float clampedStyled = styledValue;
        if(clampedStyled < lo) clampedStyled = lo;
        if(clampedStyled > hi) clampedStyled = hi;
        const float styledNorm = hi > lo ? (clampedStyled - lo) / (hi - lo) : 0.5f;

        const bool nerfed = style != 0 && styledValue < baseValue;
        const bool buffed = style != 0 && styledValue > baseValue;

        const float purpleNorm = nerfed ? styledNorm : baseNorm;
        *reinterpret_cast<float*>(reinterpret_cast<u32>(basePane) + 0x44) = purpleNorm;
        *reinterpret_cast<float*>(reinterpret_cast<u32>(basePane) + 0x48) = 1.0f;

        *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xD8) = COLOR_BASE_LEFT;
        *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xDC) = COLOR_BASE_RIGHT;
        *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xE0) = COLOR_BASE_LEFT;
        *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xE4) = COLOR_BASE_RIGHT;

        nw4r::lyt::Pane* parent = basePane->parent;
        nw4r::lyt::Pane* lossPane = nullptr;
        nw4r::lyt::Pane* gainPane = nullptr;
        if(parent != nullptr) {
            lossPane = parent->FindPaneByName(STYLE_PANE_LOSS, true);
            gainPane = parent->FindPaneByName(STYLE_PANE_GAIN, true);
        }

        if(lossPane != nullptr) {
            SetVisibleBit(lossPane, nerfed);
            if(nerfed) {
                *reinterpret_cast<float*>(reinterpret_cast<u32>(lossPane) + 0x44) = baseNorm;
                *reinterpret_cast<float*>(reinterpret_cast<u32>(lossPane) + 0x48) = 1.0f;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xD8) = COLOR_LOSS_LEFT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xDC) = COLOR_LOSS_RIGHT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xE0) = COLOR_LOSS_LEFT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xE4) = COLOR_LOSS_RIGHT;
            }
        }

        if(gainPane != nullptr) {
            SetVisibleBit(gainPane, buffed);
            if(buffed) {
                *reinterpret_cast<float*>(reinterpret_cast<u32>(gainPane) + 0x44) = styledNorm;
                *reinterpret_cast<float*>(reinterpret_cast<u32>(gainPane) + 0x48) = 1.0f;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xD8) = COLOR_GAIN_LEFT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xDC) = COLOR_GAIN_RIGHT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xE0) = COLOR_GAIN_LEFT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xE4) = COLOR_GAIN_RIGHT;
            }
        }
    }

    u8* lastAb = abilities + STAT_MINITURBO * abilityStride;
    nw4r::lyt::Pane* lastBase = *reinterpret_cast<nw4r::lyt::Pane**>(
        reinterpret_cast<u32>(lastAb) + abilityPaneOffset);
    if(!IsMultiplayerPage(graph) && lastBase != nullptr && lastBase->parent != nullptr) {
        UpdateIcons(lastBase->parent, kart, style);
        UpdateButtonPrompt(graph, hud);
    }
}
kmBranch(0x807e7e20, GraphUpdateHook);

static void WriteAllBarColors() {
    SectionMgr* mgr = SectionMgr::sInstance;
    if(mgr == nullptr || mgr->curSection == nullptr) return;

    for(u32 i = 0; i < 3; ++i) {
        const PageGraphInfo& info = pageGraphs[i];
        Page* page = mgr->curSection->pages[info.id];
        if(page == nullptr) continue;

        for(u8 hud = 0; hud < info.graphCount; ++hud) {
            u8* graph = reinterpret_cast<u8*>(reinterpret_cast<u32>(page)
                + info.graphOffset + hud * 0x184);
            u8* abilities = *reinterpret_cast<u8**>(graph + 0x174);
            if(abilities == nullptr) continue;

            for(u32 stat = 0; stat < STAT_COUNT; ++stat) {
                u8* ab = abilities + stat * abilityStride;
                nw4r::lyt::Pane* basePane = *reinterpret_cast<nw4r::lyt::Pane**>(ab + abilityPaneOffset);
                if(basePane == nullptr) continue;

                *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xD8) = COLOR_BASE_LEFT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xDC) = COLOR_BASE_RIGHT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xE0) = COLOR_BASE_LEFT;
                *reinterpret_cast<u32*>(reinterpret_cast<u32>(basePane) + 0xE4) = COLOR_BASE_RIGHT;

                nw4r::lyt::Pane* parent = basePane->parent;
                if(parent == nullptr) continue;

                nw4r::lyt::Pane* lossPane = parent->FindPaneByName(STYLE_PANE_LOSS, true);
                if(lossPane != nullptr) {
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xD8) = COLOR_LOSS_LEFT;
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xDC) = COLOR_LOSS_RIGHT;
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xE0) = COLOR_LOSS_LEFT;
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(lossPane) + 0xE4) = COLOR_LOSS_RIGHT;
                }

                nw4r::lyt::Pane* gainPane = parent->FindPaneByName(STYLE_PANE_GAIN, true);
                if(gainPane != nullptr) {
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xD8) = COLOR_GAIN_LEFT;
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xDC) = COLOR_GAIN_RIGHT;
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xE0) = COLOR_GAIN_LEFT;
                    *reinterpret_cast<u32*>(reinterpret_cast<u32>(gainPane) + 0xE4) = COLOR_GAIN_RIGHT;
                }
            }
        }
    }
}

void PostInitSelfHook() {
    WriteAllBarColors();
}
kmBranch(0x807e8114, PostInitSelfHook);

void RefreshGridBars(u8 hud, KartId hoveredKart) {
    CtrlMenuMachineGraph* graph = LocateGraphForHud(hud);
    if(graph == nullptr) return;
    GraphUpdateHook(graph, CHARACTER_NONE, hoveredKart);
}

}//namespace StatBars
}//namespace UI
}//namespace Pulsar
