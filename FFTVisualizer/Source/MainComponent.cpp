/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize (800, 600);

    // Some platforms require permissions to open input channels so request that here
    if (RuntimePermissions::isRequired (RuntimePermissions::recordAudio)
        && ! RuntimePermissions::isGranted (RuntimePermissions::recordAudio))
    {
        RuntimePermissions::request (RuntimePermissions::recordAudio,
                                     [&] (bool granted) { if (granted)  setAudioChannels (2, 2); });
    }
    else
    {
        // Specify the number of input and output channels that we want to open
        setAudioChannels (2, 2);
    }

    addAndMakeVisible (visualizerComponent);
}

MainComponent::~MainComponent()
{
    // This shuts down the audio device and clears the audio source.
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    visualizer.setSampleRate (sampleRate);
    summingBuffer.setSize(1, samplesPerBlockExpected);
}

void MainComponent::getNextAudioBlock(const AudioSourceChannelInfo& bufferToFill)
{
    const auto numChannels = bufferToFill.buffer->getNumChannels();
    const auto startSample = bufferToFill.startSample;
    const auto numSamples = bufferToFill.buffer->getNumSamples();

    summingBuffer.clear();
    for (auto i = 0; i < numChannels; ++i)
        summingBuffer.addFrom(0, startSample, *bufferToFill.buffer, i, startSample, numSamples);
    const auto gain = 1.f / static_cast<float> (numChannels);
    FloatVectorOperations::multiply(summingBuffer.getWritePointer(0, startSample), gain, numSamples);

    visualizer.addSamples(summingBuffer.getReadPointer(0), numSamples);

    bufferToFill.buffer->clear();
}

void MainComponent::releaseResources()
{
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    visualizerComponent.setBounds (getLocalBounds ());
}
