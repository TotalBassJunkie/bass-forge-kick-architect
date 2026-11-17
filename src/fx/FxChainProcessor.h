#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <memory>
#include <vector>

#include "core/KickState.h"
#include "fx/FxModule.h"

namespace bfka
{

/**
 * Anzeigedaten eines Rack-Moduls.
 *
 * `pre`  = Signal, wie es **am** Modul ankommt (Eingang, vor dessen DSP).
 * `post` = Signal nach dem Modul, inklusive MIX-Blende und OUT GAIN — also das,
 *          was das Modul an die Kette abgibt.
 *
 * Beide Buffer sind in `FxChainProcessor::prepare()` reserviert und im
 * Audio-Thread allokationsfrei gefuellt. Sie enthalten die **letzten**
 * min(blockSize, 512) Samples des zuletzt verarbeiteten Blocks, linksbuendig
 * beginnend; bei kuerzeren Bloecken ist der Rest genullt.
 *
 * Threading: Der Audio-Thread schreibt, die UI liest ohne Sperre. Ein
 * gleichzeitiger Lesevorgang kann daher einen halb aktualisierten Block sehen —
 * fuer eine Wellenform-Anzeige ist das unkritisch.
 */
struct ModuleDisplay
{
    bool valid = false;
    juce::AudioBuffer<float> pre;
    juce::AudioBuffer<float> post;
};

/**
 * Die Ketten-Engine des FX-Racks (Ticket B-69).
 *
 * Sie haelt die Modulinstanzen in Kettenreihenfolge, ruft sie fuer die Summe auf
 * und uebernimmt alles, was nicht Modul-DSP ist:
 *
 *  1. **MIX**    trocken/nass-Blende je Modul (linear),
 *  2. **OUT GAIN** Pegel nach der Blende,
 *  3. **Bypass**  `enabled == false` und `mix == 0` ueberspringen das Modul,
 *     halten aber seine Latenz aufrecht,
 *  4. **Latenz** Latenzausgleich des Trockenpfads und PDC-Summe,
 *  5. **Display** Wellenform vor/nach jedem Modul.
 *
 * Threading-Vertrag:
 *  - `prepare()`, `setChain()` und `reset()` laufen auf dem Message-Thread und
 *    duerfen allokieren; sie duerfen **nicht** parallel zu `process()` laufen.
 *  - `process()` laeuft auf dem Audio-Thread und allokiert nicht, loggt nicht
 *    und nimmt keine Sperre.
 *  - `setModuleEnabled()` / `setModuleMix()` / `setModuleOutGain()` /
 *    `setBypassAll()` sind atomar und duerfen aus beiden Threads kommen.
 */
class FxChainProcessor
{
public:
    /** Das Rack ist stereo; mehr Kanaele werden unveraendert durchgereicht. */
    static constexpr int kMaxChannels = 2;

    /** Obergrenze des Display-Fensters in Samples. */
    static constexpr int kMaxDisplaySamples = 512;

    FxChainProcessor();
    ~FxChainProcessor();

    // --- Lebenszyklus -----------------------------------------------------
    /** Audio-Vorbereitung; baut die internen Buffer neu auf. */
    void prepare (double sampleRate, int maxBlockSize);

    /** Setzt alle Module und die Latenzleitungen zurueck (kein Allokieren). */
    void reset();

    /**
     * Baut die Modulinstanzen aus dem Zustand neu auf.
     *
     * Fuer jedes Element der Kette wird ein Modul erzeugt, seine Parameter
     * werden aus `FxModuleState::data` uebernommen (Attributname ==
     * `FxParamDesc::id`) und `enabled`/`mix`/`outGain` uebernommen. Unbekannte
     * Typen werden uebersprungen und protokolliert; sie behalten trotzdem
     * ihren Kettenplatz (als `nullptr`), damit Karten-Indizes stimmen.
     *
     * Wurde vorher `prepare()` gerufen, werden die neuen Module sofort
     * vorbereitet.
     */
    void setChain (const FxChain& chain);

    // --- Zugriff ----------------------------------------------------------
    /**
     * Die Modulinstanzen in Kettenreihenfolge. Ein `nullptr`-Eintrag steht fuer
     * einen unbekannten/nicht erzeugbaren Modultyp an dieser Position.
     */
    const std::vector<std::unique_ptr<FxModule>>& modules() const noexcept { return moduleList; }

    /** Anzahl der Kettenplaetze (inklusive nicht erzeugbarer Module). */
    int numModules() const noexcept { return (int) moduleList.size(); }

    /** Summe der Modullatenzen in Samples (fuer die Host-Kompensation). */
    int totalLatencySamples() const noexcept { return cachedLatency; }

    /** Wurde `prepare()` bereits gerufen? */
    bool isPrepared() const noexcept { return prepared; }

    // --- Audio ------------------------------------------------------------
    /**
     * Verarbeitet die Summe in-place durch die Kette.
     * Ohne vorheriges `prepare()` kehrt die Methode sofort zurueck.
     */
    void process (juce::AudioBuffer<float>& buffer);

    // --- Display ----------------------------------------------------------
    /** Anzeigedaten von Kettenplatz `moduleIndex`; ungueltig -> `valid == false`. */
    const ModuleDisplay& displayFor (int moduleIndex) const;

    // --- Live-Bedienung (atomar, beide Threads) ---------------------------
    void setBypassAll (bool shouldBypassAll) noexcept { bypassAll.store (shouldBypassAll, std::memory_order_relaxed); }
    bool isBypassAll() const noexcept { return bypassAll.load (std::memory_order_relaxed); }

    void setModuleEnabled (int moduleIndex, bool shouldBeEnabled) noexcept;
    void setModuleMix (int moduleIndex, float newMix) noexcept;
    void setModuleOutGain (int moduleIndex, float newOutGain) noexcept;

private:
    /** Laufzeitdaten eines Kettenplatzes (im .cpp definiert). */
    struct SlotState;

    /** Module vorbereiten, Buffer/Latenzleitungen aufbauen, zuruecksetzen. */
    void applyPreparation();

    std::vector<std::unique_ptr<FxModule>>  moduleList;
    std::vector<std::unique_ptr<SlotState>> slotList;

    double preparedSampleRate = 48000.0;
    int    preparedBlockSize  = 512;
    int    displayWindow      = 0;
    int    cachedLatency      = 0;
    bool   prepared           = false;

    std::atomic<bool> bypassAll { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FxChainProcessor)
};

} // namespace bfka
