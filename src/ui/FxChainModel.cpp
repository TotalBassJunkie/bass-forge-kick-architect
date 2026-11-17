#include "ui/FxChainModel.h"

#include <utility>

#include "fx/FxFactory.h"

namespace bfka
{

FxChainModel::FxChainModel (FxChain initialChain)
    : chainData (std::move (initialChain))
{
}

bool FxChainModel::isValidIndex (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, (int) chainData.modules.size());
}

FxModuleState* FxChainModel::module (int index) noexcept
{
    return isValidIndex (index) ? &chainData.modules[(size_t) index] : nullptr;
}

const FxModuleState* FxChainModel::module (int index) const noexcept
{
    return isValidIndex (index) ? &chainData.modules[(size_t) index] : nullptr;
}

int FxChainModel::addModule (FxType type)
{
    const auto typeString = toString (type);

    if (typeString.isEmpty())
        return -1;

    FxModuleState state;
    state.type    = typeString;
    state.enabled = true;
    state.mix     = 1.0f;
    state.outGain = 1.0f;

    // Kein `data`: die Modul-Defaults gelten (Werkszustand des Moduls).
    chainData.modules.push_back (std::move (state));

    return (int) chainData.modules.size() - 1;
}

bool FxChainModel::removeModule (int index)
{
    if (! isValidIndex (index))
        return false;

    chainData.modules.erase (chainData.modules.begin() + index);
    return true;
}

bool FxChainModel::moveModule (int from, int to)
{
    if (! isValidIndex (from))
        return false;

    const int last = (int) chainData.modules.size() - 1;
    const int target = juce::jlimit (0, last, to);

    if (target == from)
        return false;

    auto moved = std::move (chainData.modules[(size_t) from]);
    chainData.modules.erase (chainData.modules.begin() + from);
    chainData.modules.insert (chainData.modules.begin() + target, std::move (moved));

    return true;
}

int FxChainModel::duplicateModule (int index)
{
    if (! isValidIndex (index))
        return -1;

    FxModuleState copy = chainData.modules[(size_t) index];

    if (copy.data.isValid())
        copy.data = copy.data.createCopy();

    const auto insertAt = chainData.modules.begin() + index + 1;
    chainData.modules.insert (insertAt, std::move (copy));

    return index + 1;
}

void FxChainModel::clear()
{
    chainData.modules.clear();
}

void FxChainModel::setChainBypass (bool shouldBypassAll)
{
    chainData.bypassAll = shouldBypassAll;
}

} // namespace bfka
