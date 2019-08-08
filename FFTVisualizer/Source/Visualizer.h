/*
  ==============================================================================

    Visualizer.h
    Created: 9 Jul 2019 4:29:48pm
    Author:  Alistair Barker

  ==============================================================================
*/

#pragma once

#include "JuceHeader.h"

class Visualizer : public Component, public Thread
{
public:
    explicit Visualizer (int fftOrder) :
        Thread ("fft"),
        fft (fftOrder),
        windowingFunction (static_cast<size_t> (fft.getSize ()), dsp::WindowingFunction<float>::hamming)
    {
        inputBuffer.setSize (1, 16384, false, true);
        processingBuffer.setSize (1, 2 * fft.getSize (), false, true);

        fftOutputBuffer.setSize (1, fft.getSize () / 2, false, true);
        fftMaxOutputBuffer.setSize (1, fft.getSize () / 2, false, true);

        startThread ();
    }

    ~Visualizer ()
    {
        stopThread (3000);
    }

    void setSampleRate (double fs) {     sampleRate = fs;    }
    double getSampleRate () const {     return sampleRate;    }

    int getNumBins () const
    {
        return fft.getSize () / 2;
    }

    void addSamples (const float* samples, int numSamples)
    {
        jassert (sampleRate > 0.);
        fifo.addToFifo (samples, numSamples);
    }

    void copyCurrentFft (float* samples, int numSamples) const
    {
        jassert (numSamples == fft.getSize () / 2);
        ScopedLock lock (processingLock);
        FloatVectorOperations::copy (samples, fftOutputBuffer.getReadPointer (0), numSamples);
    }

    bool getMaxHasChanged ()
    {
        ScopedLock lock (processingLock);
        if (! maxHasChanged)
            return false;

        maxHasChanged = false;
        return true;
    }

    void resetMax ()
    {
        ScopedLock lock (processingLock);
        fftMaxOutputBuffer.clear ();
        maxHasChanged = true;
    }

    void copyCurrentMax (float* samples, int numSamples) const
    {
        jassert (numSamples == fft.getSize () / 2);
        ScopedLock lock (processingLock);
        FloatVectorOperations::copy (samples, fftMaxOutputBuffer.getReadPointer (0), numSamples);
    }

private:
    void run () override
    {
        while (! threadShouldExit ())
        {
            const auto numReady = fifo.abstractFifo.getNumReady ();
            if (numReady > 0)
            {
                addToInputBuffer (numReady);
                perform ();
            }

            sleep (1);
        }
    }

    void addToInputBuffer (int numSamples)
    {
        const auto bufferSize = inputBuffer.getNumSamples ();
        if (writePointer + numSamples < bufferSize)
        {
            fifo.readFromFifo (inputBuffer.getWritePointer (0, writePointer), numSamples);
            writePointer += numSamples;
        }
        else
        {
            const auto numToCopy1 = bufferSize - writePointer;
            fifo.readFromFifo (inputBuffer.getWritePointer (0, writePointer), numToCopy1);

            const auto numToCopy2 = numSamples - numToCopy1;
            fifo.readFromFifo (inputBuffer.getWritePointer (0), numToCopy2);

            writePointer = numToCopy2;
        }
    }

    void readFromInputBuffer (float* destination, int numSamples)
    {
        const auto bufferSize = inputBuffer.getNumSamples ();

        if (readPointer + numSamples < bufferSize)
        {
            FloatVectorOperations::copy (destination, inputBuffer.getReadPointer (0, readPointer), numSamples);
            readPointer += numSamples;
        }
        else
        {
            const auto numToCopy1 = bufferSize - readPointer;
            FloatVectorOperations::copy (destination, inputBuffer.getReadPointer (0, readPointer), numToCopy1);

            const auto numToCopy2 = numSamples - numToCopy1;
            FloatVectorOperations::copy (destination + numToCopy1, inputBuffer.getReadPointer (0), numToCopy2);

            readPointer = numToCopy2;
        }
    }

    void copyFromFftBufferToInputBuffer ()
    {
        const auto fftSize = fft.getSize ();
        processingBuffer.clear ();
        readFromInputBuffer (processingBuffer.getWritePointer (0), fftSize);
        windowingFunction.multiplyWithWindowingTable (processingBuffer.getWritePointer (0),
                                                      static_cast<size_t> (fftSize));
    }

    void perform ()
    {
        while (getWrappedDistanceBetweenPointers () > fft.getSize ())
        {
            copyFromFftBufferToInputBuffer ();
            fft.performFrequencyOnlyForwardTransform (processingBuffer.getWritePointer (0));
            applyBalisticsAndCopyToOutput ();
        }
    }

    void applyBalisticsAndCopyToOutput ()
    {
        ScopedLock sl (processingLock);
        const auto input = processingBuffer.getWritePointer (0);
        const auto output = fftOutputBuffer.getWritePointer (0);
        const auto maxOutput = fftMaxOutputBuffer.getWritePointer (0);

        const auto decayRate = Decibels::decibelsToGain (-40.f * static_cast<float> (fft.getSize ()) / static_cast<float> (sampleRate));

        for (auto n = 0 ; n < fftOutputBuffer.getNumSamples (); ++n)
        {
            if (input[n] > output[n])
            {
                output[n] = input[n];
            }
            else
                output[n] *= decayRate;


            if (input[n] > maxOutput[n])
            {
                maxOutput[n] = input[n];
                maxHasChanged = true;
            }
        }
    }

    int getWrappedDistanceBetweenPointers () const
    {
        if (writePointer > readPointer)
            return writePointer - readPointer;

        return inputBuffer.getNumSamples () - readPointer + writePointer;
    }


    struct Fifo
    {
        void addToFifo (const float* someData, int numItems)
        {
            int start1, size1, start2, size2;
            abstractFifo.prepareToWrite (numItems, start1, size1, start2, size2);

            if (size1 > 0)
                copySomeData (myBuffer.data () + start1, someData, size1);

            if (size2 > 0)
                copySomeData (myBuffer.data () + start2, someData + size1, size2);

            abstractFifo.finishedWrite (size1 + size2);
        }

        void readFromFifo (float* someData, int numItems)
        {
            int start1, size1, start2, size2;
            abstractFifo.prepareToRead (numItems, start1, size1, start2, size2);

            if (size1 > 0)
                copySomeData (someData, myBuffer.data() + start1, size1);

            if (size2 > 0)
                copySomeData (someData + size1, myBuffer.data() + start2, size2);

            abstractFifo.finishedRead (size1 + size2);
        }

        void copySomeData (float* dest, const float* source, int numItems) const
        {
            FloatVectorOperations::copy (dest, source, numItems);
        }

        AbstractFifo abstractFifo { 4096 };
        std::array<float, 4096> myBuffer{};
    };

    Fifo fifo;

    double sampleRate {0.};

    AudioBuffer<float> processingBuffer;
    AudioBuffer<float> fftOutputBuffer;
    AudioBuffer<float> fftMaxOutputBuffer;
    AudioBuffer<float> inputBuffer;

    dsp::FFT fft;

    dsp::WindowingFunction<float> windowingFunction;

    bool maxHasChanged {false};

    int writePointer {0};
    int readPointer {0};

    CriticalSection processingLock;
};