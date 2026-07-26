
#include <JuceHeader.h>
#include "Spectrogram.h"


Spectrogram::Spectrogram() :
    forwardFFT(fftOrder),
    spectrogramImage(juce::Image::RGB, 512, 512, true),
    fifoIndex(0),
    nextFFTBlockReady(false)
{
    // Zero-fill the fifo and fftData arrays
    std::fill_n(fifo, fftSize, 0.0f);
    std::fill_n(fftData, 2 * fftSize, 0.0f);  // <-- Here it is
    setOpaque(true);
    startTimerHz(30);
    setSize(700, 500);
}


Spectrogram::~Spectrogram()
{
    // You may need custom cleanup code here
}

void Spectrogram::prepareToPlay(int /*samplesPerBlockExpected*/, double /*newSampleRate*/)
{
    // (nothing to do here)
}

void Spectrogram::releaseResources()
{
    // (nothing to do here)
}

void Spectrogram::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer->getNumChannels() > 0)
    {
        const auto* channelData = bufferToFill.buffer->getReadPointer(0, bufferToFill.startSample);

        for (auto i = 0; i < bufferToFill.numSamples; ++i)
            pushNextSampleIntoFifo(channelData[i]);

        // Removed the line that clears the buffer
    }
}


void Spectrogram::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);
    g.setOpacity(1.0f);
    g.drawImage(spectrogramImage, getLocalBounds().toFloat());
}

void Spectrogram::timerCallback()
{
    if (nextFFTBlockReady)
    {
        drawNextLineOfSpectrogramWithStreaking();
        nextFFTBlockReady = false;
        repaint();
    }
}

void Spectrogram::pushNextSampleIntoFifo(float sample) noexcept
{
    if (fifoIndex == fftSize)
    {
        if (!nextFFTBlockReady)
        {
            std::memset(fftData, 0, sizeof(fftData));
            memcpy(fftData, fifo, sizeof(fifo));
            nextFFTBlockReady = true;
        }

        fifoIndex = 0;
    }

    fifo[fifoIndex++] = sample;
}


void Spectrogram::drawNextLineOfSpectrogramWithStreaking()
{
    auto rightHandEdge = spectrogramImage.getWidth() - 1;
    auto imageHeight = spectrogramImage.getHeight();

    // Move the image upwards by 1 pixel to make space for the new line at the bottom
    spectrogramImage.moveImageSection(0, 0, 0, 1, rightHandEdge, imageHeight - 1);

    // Perform FFT
    forwardFFT.performFrequencyOnlyForwardTransform(fftData);

    // Pre-calculate log limits
    double logMin = std::log10(20);  // 20 Hz
    double logMax = std::log10(20000);  // 20000 Hz

    // Loop through each pixel in the new bottom row
    for (int x = 0; x <= rightHandEdge; ++x)
    {
        // Logarithmic frequency mapping
        double logFreq = logMin + (logMax - logMin) * x / static_cast<double>(rightHandEdge);
        auto fftDataIndex = static_cast<int>(std::pow(10.0, logFreq));

        // Make sure the index is within bounds
        fftDataIndex = std::clamp(fftDataIndex, 0, fftSize / 2 - 1);

        // Get the level for this frequency
        auto level = fftData[fftDataIndex];

        // Create color based on level
        juce::Colour startColor = juce::Colour::fromHSV(0.5f, 1.0f, 1.0f, 1.0f);
        juce::Colour endColor = juce::Colour::fromHSV(0.8f, 1.0f, 1.0f, 1.0f);
        juce::Colour blendedColor = startColor.interpolatedWith(endColor, level);

        // Update the pixels along the new bottom row
        spectrogramImage.setPixelAt(x, imageHeight - 1, blendedColor);
    }
}


