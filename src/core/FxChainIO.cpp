#include "core/FxChainIO.h"

namespace bfka
{

namespace
{
    const juce::Identifier kFxType     { "FxChain" };
    const juce::Identifier kModuleType { "Module" };
}

juce::ValueTree fxChainToValueTree (const FxChain& chain)
{
    juce::ValueTree fx (kFxType);
    fx.setProperty ("bypassAll", chain.bypassAll, nullptr);

    for (const auto& m : chain.modules)
    {
        juce::ValueTree module (kModuleType);
        module.setProperty ("type", m.type, nullptr);
        module.setProperty ("on", m.enabled, nullptr);
        module.setProperty ("mix", m.mix, nullptr);
        module.setProperty ("outGain", m.outGain, nullptr);

        if (m.data.isValid())
            module.appendChild (m.data.createCopy(), nullptr);

        fx.appendChild (module, nullptr);
    }

    return fx;
}

std::optional<FxChain> fxChainFromValueTree (const juce::ValueTree& tree)
{
    if (! tree.isValid())
        return std::nullopt;

    const auto fx = tree.hasType (kFxType) ? tree : tree.getChildWithName (kFxType);

    if (! fx.isValid())
        return std::nullopt;

    FxChain chain;
    chain.bypassAll = (bool) fx.getProperty ("bypassAll", false);

    for (int i = 0; i < fx.getNumChildren(); ++i)
    {
        const auto m = fx.getChild (i);

        if (! m.hasType (kModuleType))
            continue;

        FxModuleState module;
        module.type    = m.getProperty ("type", "").toString();
        module.enabled = (bool) m.getProperty ("on", true);
        module.mix     = (float) (double) m.getProperty ("mix", 1.0);
        module.outGain = (float) (double) m.getProperty ("outGain", 1.0);

        // Der Struktur-Zustand ist das erste Kind, das nicht selbst ein
        // Kettenmodul ist (so schreibt es fxChainToValueTree()).
        for (int c = 0; c < m.getNumChildren(); ++c)
        {
            const auto child = m.getChild (c);

            if (child.hasType (kModuleType))
            {
                module.data = child.createCopy();
                break;
            }
        }

        // Ein Modul ohne Typ hat keinen Kettenplatz; es kann nicht entstehen.
        if (module.type.isEmpty())
            continue;

        chain.modules.push_back (std::move (module));
    }

    return chain;
}

} // namespace bfka
