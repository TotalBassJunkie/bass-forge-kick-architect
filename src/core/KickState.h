// B-16 -- Zustands-Migration: Schema-Version am Kick-Zustand.
#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <array>
#include <optional>
#include <vector>

namespace bfka
{

/** Rolle einer Schicht. Entspricht den Pool-Rollen der Style-Regeln. */
enum class Role
{
    none = 0,
    punch,
    crunch,
    rumble,
    click,
    top,
    punchEnhancer,
    rumbleEnhancer,
    schranzEnhancer
};

/** Rollen-Kennung als Text, wie sie in Presets steht ("punch", "rumble", ...). */
juce::String roleToString (Role role);
Role         roleFromString (const juce::String& text);

/** Verweis auf ein Sample. */
struct SampleRef
{
    juce::String relPath;   ///< Pfad relativ zum Library-Root
    juce::String absPath;   ///< absoluter Pfad, wenn ausserhalb der Library
    juce::String fileName;  ///< reiner Dateiname
    juce::String roleId;    ///< Pool-Rollen-Kennung

    bool isValid() const noexcept { return relPath.isNotEmpty() || absPath.isNotEmpty(); }
};

/** Zustand eines FX-Moduls in der Kette. */
struct FxModuleState
{
    juce::String type;      ///< "eq", "mseq", "dist", "shaper", "trans", "reverb", "filt", "final", "ott"
    bool  enabled = true;
    float mix     = 1.0f;   ///< MIX-Regler (0..1)
    float outGain = 1.0f;   ///< Ausgangsverstaerkung (linear)

    juce::ValueTree data;   ///< modulspezifische Parameter und Kurven
};

/** Die FX-Kette in Reihenfolge. */
struct FxChain
{
    bool bypassAll = false;
    std::vector<FxModuleState> modules;
};

/** Einstellungen fuer ALIGN/PHASE ALIGN. */
enum class AlignMode   { gentle, normal, tight };
enum class AlignMethod { moveCrunch, stretchPunch };

struct AlignSettings
{
    bool        enabled     = true;
    AlignMethod method      = AlignMethod::moveCrunch;
    AlignMode   tightness   = AlignMode::normal;
    bool        phaseAlign  = true;
    bool        stretchCrunch = true;
};

/** Tempo-Verhalten der Schichten. */
enum class StretchMode { keepPitch, repitch };
enum class BpmMode     { syncBpm, native };

/** Der vollstaendige Kick-Zustand. */
class KickState
{
public:
    static constexpr int kNumSlots = 12;

    KickState();

    // --- Slot-Inhalt -------------------------------------------------------
    const std::optional<SampleRef>& getSample (int slot) const noexcept;
    void setSample (int slot, std::optional<SampleRef> sample);
    void clearSample (int slot);

    Role getRole (int slot) const noexcept;
    void setRole (int slot, Role role) noexcept;

    bool isLocked (int slot) const noexcept;
    void setLocked (int slot, bool shouldBeLocked) noexcept;

    bool isKeyTracked (int slot) const noexcept;
    void setKeyTracked (int slot, bool shouldTrack) noexcept;

    // --- Parameter (immer normalisiert 0..1) -------------------------------
    float getParam (const juce::String& id) const noexcept;
    void  setParam (const juce::String& id, float normalisedValue) noexcept;

    const std::vector<float>& values() const noexcept { return paramValues; }
    std::vector<float>&       values()       noexcept { return paramValues; }

    // --- Globals -----------------------------------------------------------
    FxChain&       fxChain()       noexcept { return chain; }
    const FxChain& fxChain() const noexcept { return chain; }

    AlignSettings&       align()       noexcept { return alignSettings; }
    const AlignSettings& align() const noexcept { return alignSettings; }

    StretchMode& stretchMode()       noexcept { return stretch; }
    StretchMode  stretchMode() const noexcept { return stretch; }
    BpmMode&      bpmMode()      noexcept { return bpm; }
    BpmMode       bpmMode()      const noexcept { return bpm; }

    double getBpm() const noexcept { return bpmValue; }
    void   setBpm (double newBpm) noexcept { bpmValue = newBpm; }

    // --- Meta --------------------------------------------------------------
    const juce::String& getStyleId() const noexcept { return styleId; }
    void setStyleId (juce::String newId) { styleId = std::move (newId); }

    const juce::String& getPresetName() const noexcept { return presetName; }
    void setPresetName (juce::String name) { presetName = std::move (name); }

    bool isDirty() const noexcept { return dirty; }
    void setDirty (bool shouldBeDirty) noexcept { dirty = shouldBeDirty; }

    // --- Serialisierung ----------------------------------------------------
    juce::ValueTree toValueTree() const;
    static std::optional<KickState> fromValueTree (const juce::ValueTree& tree, juce::String& error);

    // --- Schema-Version und Migration (B-16) -------------------------------
    /** Hoechste bekannte Schema-Version des Kick-Zustands (B-16). */
    static constexpr int currentSchemaVersion = 2;

    /** Die Schema-Version eines Zustandsbaums (0, wenn keine gesetzt ist). */
    static int schemaVersionOf (const juce::ValueTree& tree) noexcept;

    /**
     * Migriert `tree` auf `targetVersion` (B-16).
     * Unbekannte Properties und Kinder bleiben unangetastet; jede Stufe
     * ergaenzt nur, was ihr fehlt. Nie eine Exception.
     */
    static juce::ValueTree migrateTo (juce::ValueTree tree, int targetVersion);

    /** Setzt die Schema-Version im Baum. */
    static void setSchemaVersion (juce::ValueTree& tree, int version);

private:
    std::array<std::optional<SampleRef>, (size_t) kNumSlots> samples;
    std::array<Role, (size_t) kNumSlots>  roles;
    std::array<bool, (size_t) kNumSlots>  locked;
    std::array<bool, (size_t) kNumSlots>  keyTracked;
    std::vector<float> paramValues;

    FxChain       chain;
    AlignSettings alignSettings;
    StretchMode   stretch = StretchMode::keepPitch;
    BpmMode       bpm     = BpmMode::syncBpm;
    double        bpmValue = 150.0;

    juce::String styleId = "free_build";
    juce::String presetName;
    bool         dirty = false;
};

} // namespace bfka
