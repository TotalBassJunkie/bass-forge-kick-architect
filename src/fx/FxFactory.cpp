#include "fx/FxFactory.h"

#include "fx/PassThroughModule.h"
#include "fx/modules/ConvolverModule.h"
#include "fx/modules/DistortionModule.h"
#include "fx/modules/DynamicEqModule.h"
#include "fx/modules/EqualizerModule.h"
#include "fx/modules/FiltersModule.h"
#include "fx/modules/MultibandModule.h"
#include "fx/modules/TransientShaperModule.h"

namespace bfka
{

FxFactory& FxFactory::get()
{
    static FxFactory instance;
    return instance;
}

FxFactory::FxFactory()
{
    // Platzhalter-Registrierung aller neun Typen. Die echten Module ersetzen
    // diese Eintraege per registerType(); die Reihenfolge hier ist die
    // Anzeige-Reihenfolge in availableTypes().
    registerType (FxType::filters,         [] { return std::make_unique<FiltersModule>(); });
    registerType (FxType::equalizer,       [] { return std::make_unique<EqualizerModule>(); });
    registerType (FxType::midSideEq,       [] { return std::make_unique<PassThroughModule> (FxType::midSideEq); });
    registerType (FxType::distortion,      [] { return std::make_unique<DistortionModule>(); });
    registerType (FxType::kickShaper,      [] { return std::make_unique<PassThroughModule> (FxType::kickShaper); });
    registerType (FxType::transientShaper, [] { return std::make_unique<TransientShaperModule>(); });
    registerType (FxType::convolver,       [] { return std::make_unique<ConvolverModule>(); });
    registerType (FxType::finalizer,       [] { return std::make_unique<PassThroughModule> (FxType::finalizer); });
    registerType (FxType::multiband,       [] { return std::make_unique<MultibandModule>(); });
    registerType (FxType::dynamicEq,       [] { return std::make_unique<DynamicEqModule>(); });
}

void FxFactory::registerType (FxType type, Creator creator)
{
    for (auto it = entries.begin(); it != entries.end(); ++it)
    {
        if (it->type != type)
            continue;

        if (creator)
            it->creator = std::move (creator);
        else
            entries.erase (it);

        return;
    }

    if (creator)
        entries.push_back ({ type, std::move (creator) });
}

bool FxFactory::isRegistered (FxType type) const
{
    for (const auto& entry : entries)
        if (entry.type == type)
            return true;

    return false;
}

FxFactory::Creator FxFactory::creatorFor (FxType type) const
{
    for (const auto& entry : entries)
        if (entry.type == type)
            return entry.creator;

    return {};
}

std::unique_ptr<FxModule> FxFactory::create (const juce::String& typeString) const
{
    const auto type = fxTypeFromString (typeString);

    if (! type.has_value())
        return {};

    for (const auto& entry : entries)
        if (entry.type == *type)
            return entry.creator ? entry.creator() : nullptr;

    return {};
}

juce::StringArray FxFactory::availableTypes() const
{
    juce::StringArray result;

    for (const auto& entry : entries)
        result.add (toString (entry.type));

    return result;
}

} // namespace bfka
