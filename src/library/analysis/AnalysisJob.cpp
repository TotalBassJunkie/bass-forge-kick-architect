/*
 * B-27: Hintergrund-Analyse-Thread mit Fortschritt und Abbruch.
 *
 * Umsetzung: ein juce::Thread-Ableger ruft `AnalysisJob::run()` auf. Alle
 * Zustaende, die der Aufrufer sehen darf, sind atomar; `queue` und `results_`
 * werden nur vom Worker beschrieben bzw. erst nach `waitForCompletion()`
 * gelesen.
 */
#include "library/analysis/AnalysisJob.h"

namespace bfka
{

namespace
{
    /** Kleiner Worker, der die Arbeit des Besitzers ausfuehrt (B-27). */
    class AnalysisJobThread final : public juce::Thread
    {
    public:
        explicit AnalysisJobThread (std::function<void()> workToDo)
            : juce::Thread ("bfka AnalysisJob"),
              work (std::move (workToDo))
        {
        }

        void run() override
        {
            if (work != nullptr)
                work();
        }

    private:
        std::function<void()> work;
    };
} // namespace

AnalysisJob::AnalysisJob (Analyser analyserToUse)
    : analyser (std::move (analyserToUse))
{
}

AnalysisJob::~AnalysisJob()
{
    cancel();
    waitForCompletion();
}

void AnalysisJob::start (std::vector<AnalysisTask> tasks)
{
    // Ein noch laufender (oder noch nicht abgeraeumter) Job wird erst
    // abgeschlossen, dann neu gestartet.
    if (thread != nullptr)
        waitForCompletion();

    queue = std::move (tasks);
    results_.assign (queue.size(), SampleAnalysis {});

    cancelRequested.store (false);
    cancelled.store (false);
    doneCount.store (0);
    progressValue.store (0.0);
    running.store (true);

    thread = std::make_unique<AnalysisJobThread> ([this] { run(); });

    if (! thread->startThread())
    {
        // Thread konnte nicht angelegt werden: der Job gilt als beendet.
        thread.reset();
        running.store (false);
    }
}

void AnalysisJob::cancel()
{
    cancelRequested.store (true);
}

void AnalysisJob::waitForCompletion()
{
    if (thread == nullptr)
        return;

    // Ohne Abbruch-Signal warten: der Lauf endet nach der letzten Aufgabe.
    thread->waitForThreadToExit (-1);
    thread.reset();
}

bool AnalysisJob::isRunning() const noexcept
{
    return running.load();
}

bool AnalysisJob::wasCancelled() const noexcept
{
    return cancelled.load();
}

double AnalysisJob::progress() const noexcept
{
    return progressValue.load();
}

int AnalysisJob::completedCount() const noexcept
{
    return doneCount.load();
}

const std::vector<SampleAnalysis>& AnalysisJob::results() const noexcept
{
    return results_;
}

void AnalysisJob::run()
{
    const int total = (int) queue.size();

    for (int i = 0; i < total; ++i)
    {
        if (cancelRequested.load())
        {
            cancelled.store (true);
            break;
        }

        try
        {
            if (analyser != nullptr)
                results_[(size_t) i] = analyser (queue[(size_t) i]);
        }
        catch (...)
        {
            // Fehler in der Analyse: Ergebnis bleibt Standard, kein Abbruch.
        }

        doneCount.store (i + 1);
        progressValue.store ((double) (i + 1) / (double) total);
    }

    running.store (false);
}

} // namespace bfka
