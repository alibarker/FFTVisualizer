/*
  ==============================================================================

    VisualizerComponent.h
    Created: 10 Jul 2019 1:31:21pm
    Author:  Alistair Barker

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"
#include "Visualizer.h"
#include "Utilities.h"

class VisualizerComponent : public Component
{
public:
    VisualizerComponent (Visualizer& visualizer) : Component ("FFTDisplay"), visualizer (visualizer)
    {
        fftInputBuffer.setSize (1, visualizer.getNumBins (), false, true);
        maxInputBuffer.setSize (1, visualizer.getNumBins (), false, true);

        redrawTimer.setCallback ([this] () { update (); });
        redrawTimer.startTimerHz (60);

        maxResetTimer.setCallback ([this] () { resetMax (); });

        addAndMakeVisible (fftGraph);
        addAndMakeVisible (maxGraph);
    }

    void resized () override
    {
        maxGraph.setBounds (getLocalBounds ());
        fftGraph.setBounds (getLocalBounds ());
    }

    void resetMax ()
    {
        visualizer.resetMax ();
        maxResetTimer.stopTimer ();
    }

private:
    Visualizer& visualizer;
    AudioBuffer<float> fftInputBuffer;
    AudioBuffer<float> maxInputBuffer;

    class MaxGraph : public Component
    {
    public:
        MaxGraph ()
        {
            setBufferedToImage (true);
        }

        void paint (Graphics& g) override
        {
            const auto numSegments = 50;
            const auto numPixelsPerSegment = static_cast<int> (std::ceil (static_cast<float> (getWidth ()) / numSegments));

            for (int i = 0; i < numSegments; ++i)
            {
                const auto start = i * numPixelsPerSegment;
                const auto end = jmin (start + numPixelsPerSegment + 1, getWidth ());

                drawLineSegment (g, start, end);
            }
        }

        void drawLineSegment (Graphics& g, int start, int end)
        {
            Path path;

            auto buffer = renderBuffer.getReadPointer (0);

            const auto height = getHeight ();

            auto prevX = static_cast<float> (start);
            auto prevY = height - buffer[start] * height;

            path.startNewSubPath ({prevX, prevY});

            for (int i = start + 1; i < end; ++i)
            {
                const auto x = static_cast<float> (i);
                const auto y = height - buffer [i] * height;

                path.addLineSegment ({{prevX, prevY}, {x, y}}, 1.f);
                prevX = x;
                prevY = y;
            }

            g.setColour (Colours::whitesmoke);
            g.fillPath (path);
        }

        void resized () override
        {
            renderBuffer.setSize (1, getWidth ());
        }

        AudioBuffer<float> renderBuffer;
    };

    class FftGraph : public Component
    {
    public:
        FftGraph ()
        {
            setOpaque (true);
            setPaintingIsUnclipped (true);
        }

        void paint (Graphics& g) override
        {
            g.setColour (Colours::black);
            g.fillRect (getLocalBounds ());
            g.setColour (Colours::whitesmoke.withAlpha (0.2f));
            const auto height = static_cast<float> (getHeight ());

            auto buffer = renderBuffer.getReadPointer (0);

            for (int i = 0; i < getWidth (); ++i)
            {
                const auto top = getHeight () - buffer[i] * height;
                g.drawVerticalLine (i, top, height);
            }
        }

        void resized () override
        {
            renderBuffer.setSize (1, getWidth ());
        }

        AudioBuffer<float> renderBuffer;
    };

    FftGraph fftGraph;
    MaxGraph maxGraph;

    LambdaTimer redrawTimer;
    LambdaTimer maxResetTimer;

    void update ()
    {
        if (isVisible ())
        {
            visualizer.copyCurrentFft (fftInputBuffer.getWritePointer (0), visualizer.getNumBins ());
            updateRenderBuffer (fftGraph.renderBuffer, fftInputBuffer, getWidth (), visualizer.getNumBins ());
            fftGraph.repaint ();

            if (visualizer.getMaxHasChanged ())
            {
                visualizer.copyCurrentMax (maxInputBuffer.getWritePointer (0), visualizer.getNumBins ());
                updateRenderBuffer (maxGraph.renderBuffer, maxInputBuffer, getWidth (), visualizer.getNumBins ());
                maxGraph.repaint ();
                maxResetTimer.startTimer (5000);
            }
        }
    }

    static void updateRenderBuffer (AudioBuffer<float>& dest, const AudioBuffer<float>& source, int width, int numBins)
    {
        const auto fft = source.getReadPointer (0);
        auto destination = dest.getWritePointer (0);

        auto previousValue = getRelativeDbValue (fft[0], numBins);

        for (int i = 0; i < width; ++i)
        {
            const auto normPos = static_cast<float> (i) / width;

            const auto binPos = RangeUtils::normalizedToLogRange (normPos, 1.f, numBins);
            const auto bin = static_cast<int> (std::floor (binPos));
            const auto nextBin = bin + 1 < numBins ? bin + 1 : bin;
            const auto posInBin = binPos - bin;

            const auto lower = getRelativeDbValue (fft[bin], numBins);
            const auto upper = getRelativeDbValue (fft[nextBin], numBins);

            const auto interpolatedValue = lower - posInBin * (lower - upper);
            const auto smoothedValue = 0.5f * (previousValue + interpolatedValue);

            previousValue = smoothedValue;

            destination [i] = smoothedValue;
        }
    }

    static float getRelativeDbValue (float fftValue, int numBins)
    {
        const auto scaledValue = fftValue / (2 * numBins);
        const auto dBValue = Decibels::gainToDecibels (scaledValue, -100.f);

        return dBValue / 100.f + 1.f;
    }
};
