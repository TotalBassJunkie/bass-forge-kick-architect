/*
 * B-27: Hintergrund-Analyse-Thread mit Fortschritt und Abbruch.
 *
 * Der Job nimmt eine Liste von Analyse-Aufgaben entgegen und arbeitet sie in
 * einem einzigen Hintergrund-Thread ab. Fortschritt (0..1) und Abbruch sind
 * thread-sicher; die Ergebnisse liegen in derselben Reihenfolge wie die
 * Aufgaben vor. Der Analyser laeuft immer im Worker-Thread und darf den
 * Message-Thread nicht blockieren.
 *
 * Lebenszeit: der Destruktor bricht ab und wartet auf das Ende des Threads.
 */
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "library/analysis/AnalysisCache.h"

namespace bfka
{

/** Eine Analyse-Aufgabe (Ticket B-27). */
struct AnalysisTask
{
    juce::String sampleKey;                 ///< Pfad oder stabile Kennung
    juce::AudioBuffer<float> audio;
    double sampleRate = 48000.0;
};

/**
 * Fuehrt Analyse-Aufgaben in einem Hintergrund-Thread aus (B-27).
 *
 * Fortschritt 0..1 und Abbruch sind thread-sicher. Ergebnisse kommen in
 * derselben Reihenfolge wie die Aufgaben. Der Job wird im Destruktor
 * gestoppt und beendet; `waitForCompletion()` blockiert bis fertig.
 */
class AnalysisJob
{
public:
    /** `analyser` wird fuer jede Aufgabe aufgerufen (Worker-Thread). */
    using Analyser = std::function<SampleAnalysis (const AnalysisTask&)>;

    explicit AnalysisJob (Analyser analyserToUse);
    ~AnalysisJob();

    /** Startet die Liste im Hintergrund (vorheriger Lauf muss fertig sein). */
    void start (std::vector<AnalysisTask> tasks);

    /** Bittet um Abbruch; bereits fertige Ergebnisse bleiben erhalten. */
    void cancel();

    /** Blockiert bis der Thread fertig ist. */
    void waitForCompletion();

    bool isRunning() const noexcept;
    bool wasCancelled() const noexcept;
    double progress() const noexcept;                  ///< 0..1
    int  completedCount() const noexcept;
    const std::vector<SampleAnalysis>& results() const noexcept;

private:
    void run();
    Analyser analyser;
    std::vector<AnalysisTask> queue;
    std::vector<SampleAnalysis> results_;
    std::unique_ptr<juce::Thread> thread;
    std::atomic<bool> cancelRequested { false };
    std::atomic<bool> running { false };
    std::atomic<bool> cancelled { false };
    std::atomic<double> progressValue { 0.0 };
    std::atomic<int> doneCount { 0 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AnalysisJob)
};

} // namespace bfka
