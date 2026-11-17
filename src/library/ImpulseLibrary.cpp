#include "library/ImpulseLibrary.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace bfka
{

namespace
{
    /** Endungen, die der IR-Browser anbietet (Reihenfolge = Scan-Reihenfolge). */
    static const char* const extensions[] = { "*.wav", "*.aif", "*.aiff", "*.flac" };

    constexpr int kNumExtensions = (int) (sizeof (extensions) / sizeof (extensions[0]));

    /** Sammelt die passenden Dateien eines Ordners; doppelte Pfade fallen weg. */
    void collectFiles (const juce::File& directory, bool factory,
                       std::vector<ImpulseInfo>& target, juce::StringArray& seen)
    {
        if (directory == juce::File {} || ! directory.isDirectory())
            return;

        for (int e = 0; e < kNumExtensions; ++e)
        {
            juce::Array<juce::File> found;
            directory.findChildFiles (found, juce::File::findFiles, true, extensions[e]);

            for (const auto& file : found)
            {
                const auto path = file.getFullPathName();

                if (seen.contains (path, true))
                    continue;

                seen.add (path);

                ImpulseInfo info;
                info.name    = file.getFileNameWithoutExtension();
                info.file    = file;
                info.factory = factory;

                double seconds = 0.0;

                if (ImpulseLibrary::readLengthSeconds (file, seconds))
                    info.seconds = seconds;

                target.push_back (std::move (info));
            }
        }
    }
} // namespace

juce::File ImpulseLibrary::defaultDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
               .getChildFile ("BassForge")
               .getChildFile ("KickArchitect")
               .getChildFile ("IRs");
}

void ImpulseLibrary::setFactoryDirectory (const juce::File& directory)
{
    factoryDir = directory;
}

void ImpulseLibrary::setUserDirectory (const juce::File& directory)
{
    userDir = directory;
}

int ImpulseLibrary::scan()
{
    items.clear();

    if (userDir == juce::File {})
        userDir = defaultDirectory();

    juce::StringArray seen;

    // User zuerst, damit die eigene Sammlung oben steht.
    collectFiles (userDir, false, items, seen);
    collectFiles (factoryDir, true, items, seen);

    std::stable_sort (items.begin(), items.end(),
                      [] (const ImpulseInfo& a, const ImpulseInfo& b)
                      {
                          if (a.factory != b.factory)
                              return ! a.factory;

                          return a.name.compareIgnoreCase (b.name) < 0;
                      });

    return (int) items.size();
}

bool ImpulseLibrary::readLengthSeconds (const juce::File& file, double& secondsOut)
{
    if (! file.existsAsFile())
        return false;

    juce::AudioFormatManager manager;
    manager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (manager.createReaderFor (file));

    if (reader == nullptr)
        return false;

    if (reader->sampleRate <= 0.0)
        return false;

    secondsOut = (double) reader->lengthInSamples / reader->sampleRate;

    return true;
}

} // namespace bfka
