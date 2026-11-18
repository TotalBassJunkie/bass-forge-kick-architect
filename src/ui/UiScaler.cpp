#include "ui/UiScaler.h"

#include <cmath>

// B-89: Fenster-Resize, UIScaler und HiDPI (70..200 %).

namespace bfka
{

namespace
{
    /** Klemmt auf 70..200 und rundet danach auf den naechsten 10er-Schritt. */
    int normalizePercent (int percent) noexcept
    {
        auto p = juce::jlimit (UiScaler::kMinPercent, UiScaler::kMaxPercent, percent);
        p = ((p + 5) / 10) * 10;

        return juce::jlimit (UiScaler::kMinPercent, UiScaler::kMaxPercent, p);
    }
} // namespace

UiScaler::UiScaler() = default;

void UiScaler::setPercent (int percent)
{
    currentPercent = normalizePercent (percent);
}

int UiScaler::percent() const noexcept
{
    return currentPercent;
}

float UiScaler::scaleFactor() const noexcept
{
    return (float) currentPercent / 100.0f;
}

int UiScaler::nextPercent() const noexcept
{
    return juce::jmin (kMaxPercent, currentPercent + kStepPercent);
}

int UiScaler::previousPercent() const noexcept
{
    return juce::jmax (kMinPercent, currentPercent - kStepPercent);
}

juce::Rectangle<int> UiScaler::windowBoundsFor (int percent)
{
    const auto factor = (double) percent / 100.0;

    const auto w = (int) std::lround ((double) kDesignWidth * factor);
    const auto h = (int) std::lround ((double) kDesignHeight * factor);

    return { 0, 0, w, h };
}

bool UiScaler::isReadable (int percent)
{
    // Ausserhalb des erlaubten Rasters ist das Fenster nicht lesbar
    // (z. B. direkt uebergebene 50 %).
    if (percent < kMinPercent || percent > kMaxPercent)
        return false;

    const auto p      = normalizePercent (percent);
    const auto factor = (float) p / 100.0f;

    // Mindestmasse des Layouts bei 100 %, mit dem Faktor skaliert.
    const auto minWidth  = 640.0f * factor;
    const auto minHeight = 400.0f * factor;

    // Das tatsaechliche Fenster darf diese Mindestmasse nicht unterschreiten
    // und muss auf beiden Achsen ganzzahlig mindestens 320 px behalten.
    const auto bounds = windowBoundsFor (p);

    const auto width  = (float) bounds.getWidth();
    const auto height = (float) bounds.getHeight();

    return width >= minWidth && height >= minHeight
           && width >= 320.0f && height >= 320.0f;
}

void UiScaler::applyTo (juce::Component& root) const
{
    root.setTransform (juce::AffineTransform::scale (scaleFactor()));

    const auto bounds = windowBoundsFor (currentPercent);
    root.setSize (bounds.getWidth(), bounds.getHeight());
}

void UiScaler::applyTo (juce::Component& root, int percent)
{
    setPercent (percent);
    applyTo (root);
}

} // namespace bfka
