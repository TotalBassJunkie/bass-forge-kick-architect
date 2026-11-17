#include "core/Log.h"

namespace bfka::log
{

namespace
{
    std::atomic<bool> debugEnabled { false };
    std::mutex writeMutex;
}

void setDebugEnabled (bool shouldBeEnabled) { debugEnabled.store (shouldBeEnabled); }
bool isDebugEnabled()                        { return debugEnabled.load(); }

void write (Level level, const juce::String& message)
{
    if (level == Level::debug && ! isDebugEnabled())
        return;

    const char* tag = "";
    switch (level)
    {
        case Level::debug: tag = "DEBUG"; break;
        case Level::info:  tag = "INFO "; break;
        case Level::warn:  tag = "WARN "; break;
        case Level::error: tag = "ERROR"; break;
    }

    const std::lock_guard<std::mutex> lock (writeMutex);
    std::fprintf (stderr, "[BFKA][%s] %s\n", tag, message.toRawUTF8());
}

} // namespace bfka::log
