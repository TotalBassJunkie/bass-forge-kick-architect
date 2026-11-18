// B-27: Tests fuer den Hintergrund-Analyse-Job (Fortschritt/Abbruch).
// Die Wartezeiten sind bewusst kurz; die Tests pruefen nur Zustand, nicht Timing.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <thread>

#include <juce_events/juce_events.h>

#include "library/analysis/AnalysisJob.h"
#include "library/analysis/AnalysisCache.h"

using namespace bfka;
using Catch::Matchers::WithinAbs;

namespace
{
    /** Aufgabe mit leerem Audio; nur der Schluessel wird ausgewertet. */
    AnalysisTask makeTask (const juce::String& key)
    {
        AnalysisTask task;
        task.sampleKey = key;
        task.sampleRate = 48000.0;
        return task;
    }

    std::vector<AnalysisTask> makeTasks (int count, const juce::String& prefix)
    {
        std::vector<AnalysisTask> tasks;
        tasks.reserve ((size_t) juce::jmax (0, count));

        for (int i = 0; i < count; ++i)
            tasks.push_back (makeTask (prefix + juce::String (i)));

        return tasks;
    }
} // namespace

TEST_CASE ("Analyse-Job: alle Aufgaben werden verarbeitet", "[library][job]")
{
    AnalysisJob job ([] (const AnalysisTask& task)
    {
        SampleAnalysis a;
        a.keyNote = task.sampleKey.hashCode() % 12;
        juce::Thread::sleep (1);
        return a;
    });

    job.start (makeTasks (5, "sample-"));

    job.waitForCompletion();

    REQUIRE_FALSE (job.isRunning());
    REQUIRE (job.completedCount() == 5);
    REQUIRE (job.results().size() == 5);
    REQUIRE_THAT (job.progress(), WithinAbs (1.0, 1e-9));
}

TEST_CASE ("Analyse-Job: Abbruch stoppt frueh", "[library][job]")
{
    AnalysisJob job ([] (const AnalysisTask&)
    {
        juce::Thread::sleep (20);
        return SampleAnalysis {};
    });

    job.start (makeTasks (50, "cancel-"));

    juce::Thread::sleep (30);

    job.cancel();
    job.waitForCompletion();

    REQUIRE (job.wasCancelled());
    REQUIRE (job.completedCount() < 50);
}

TEST_CASE ("Analyse-Job: leere Liste ist sofort fertig", "[library][job]")
{
    AnalysisJob job ([] (const AnalysisTask&) { return SampleAnalysis {}; });

    job.start ({});

    job.waitForCompletion();

    REQUIRE (job.completedCount() == 0);
    REQUIRE_THAT (job.progress(), WithinAbs (0.0, 1e-9));   // leer -> kein Fortschritt gesetzt
}

TEST_CASE ("Analyse-Job: Neustart nach Abschluss", "[library][job]")
{
    AnalysisJob job ([] (const AnalysisTask&) { return SampleAnalysis {}; });

    job.start (makeTasks (3, "first-"));
    job.waitForCompletion();
    REQUIRE (job.completedCount() == 3);

    job.start (makeTasks (7, "second-"));
    job.waitForCompletion();
    REQUIRE (job.completedCount() == 7);
    REQUIRE (job.results().size() == 7);
}

TEST_CASE ("Analyse-Job: Destruktor bricht ab", "[library][job]")
{
    const auto slowAnalyser = [] (const AnalysisTask&)
    {
        juce::Thread::sleep (20);
        return SampleAnalysis {};
    };

    REQUIRE_NOTHROW (
        [&]
        {
            AnalysisJob job (slowAnalyser);
            job.start (makeTasks (50, "dtor-"));
        }());
}
