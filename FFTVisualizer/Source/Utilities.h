/*
  ==============================================================================

    Utilities.h
    Created: 8 Aug 2019 10:27:11am
    Author:  Alistair Barker

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

struct LambdaTimer : public juce::Timer
{
    LambdaTimer () = default;

    void timerCallback () override
    {
        if (callbackFunction)
            callbackFunction ();
    }

    LambdaTimer& setCallback (std::function<void()> callback)
    {
        callbackFunction = std::move (callback);
        return *this;
    }

    std::function<void()> callbackFunction;
};

namespace RangeUtils
{
    static float normalizedToLogRange (float normVal, float logRangeMin, float logRangeMax)
    {
        const auto logVal = std::exp (normVal * std::log (logRangeMax / logRangeMin)) * logRangeMin;
        return jlimit (logRangeMin, logRangeMax, logVal);
    }
}
