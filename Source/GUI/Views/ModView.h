#pragma once

#include "GUI/Components/ParamWidgets.h"

namespace aetherfall
{

class AetherfallProcessor;

/** MOD view placeholder: the full modulation editor (LFO editors, matrix with
    drag & drop, modulation rings) arrives in phase F8. */
class ModView final : public juce::Component
{
public:
    explicit ModView (AetherfallProcessor&) {}
    void paint (juce::Graphics&) override;
};

} // namespace aetherfall
