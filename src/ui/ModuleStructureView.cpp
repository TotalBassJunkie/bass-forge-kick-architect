#include "ui/ModuleStructureView.h"

#include "fx/modules/ConvolverModule.h"
#include "fx/modules/DynamicEqModule.h"
#include "fx/modules/MultibandModule.h"
#include "plugin/PluginProcessor.h"
#include "ui/UiTheme.h"
#include "ui/editors/ConvolverView.h"
#include "ui/editors/DynamicEqView.h"
#include "ui/editors/MultibandView.h"

namespace bfka
{

// ---------------------------------------------------------------------------
// Basisklasse
// ---------------------------------------------------------------------------
ModuleStructureView::ModuleStructureView (PluginProcessor& processor, int slotIndex)
    : proc (processor),
      moduleSlot (slotIndex)
{
    setOpaque (false);
}

void ModuleStructureView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds();

    ui::theme::paintPanel (g, area.toFloat(), ui::theme::panelDeep, 6.0f);

    paintContent (g, area.reduced (1));
}

void ModuleStructureView::paintTitle (juce::Graphics& g, juce::Rectangle<int> area,
                                      const juce::String& text)
{
    g.setFont (ui::theme::sectionFont());
    g.setColour (ui::theme::textDim);
    g.drawText (text, area.reduced (6, 3), juce::Justification::topLeft, false);
}

int ModuleStructureView::findParamIndex (const juce::String& id) const
{
    const auto* module = proc.getModule (moduleSlot);

    if (module == nullptr)
        return -1;

    const auto& descriptors = module->params();

    for (size_t i = 0; i < descriptors.size(); ++i)
        if (descriptors[i].id == id)
            return (int) i;

    return -1;
}

float ModuleStructureView::getParamValue (const juce::String& id) const
{
    const auto index = findParamIndex (id);

    return index >= 0 ? proc.getModuleParam (moduleSlot, index) : 0.0f;
}

void ModuleStructureView::setParamValue (const juce::String& id, float value)
{
    const auto index = findParamIndex (id);

    if (index >= 0)
        proc.setModuleParam (moduleSlot, index, value);
}

// ---------------------------------------------------------------------------
// Fabrik
// ---------------------------------------------------------------------------
std::unique_ptr<ModuleStructureView> createStructureView (PluginProcessor& processor, int slotIndex)
{
    const auto* module = processor.getModule (slotIndex);

    if (module == nullptr)
        return {};

    switch (module->type())
    {
        case FxType::multiband:
            return std::make_unique<MultibandView> (processor, slotIndex);

        case FxType::convolver:
            return std::make_unique<ConvolverView> (processor, slotIndex);

        case FxType::dynamicEq:
            return std::make_unique<DynamicEqView> (processor, slotIndex);

        default:
            break;
    }

    return {};
}

} // namespace bfka
