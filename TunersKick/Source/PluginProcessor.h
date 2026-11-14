#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "KickEngine.h"

class TunersKickAudioProcessor : public juce::AudioProcessor
{
public:
    TunersKickAudioProcessor();
    ~TunersKickAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Tuners Kick"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }

    // Presety widoczne w FL Studio jako programy wtyczki.
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    tuners::KickParams readVoiceParams() const;
    float raw (const char* id) const { return apvts.getRawParameterValue (id)->load(); }

    tuners::KickVoice voice;
    juce::dsp::Oversampling<float> oversampling; // 4x, tylko dla distortion/clippera
    juce::AudioBuffer<float> scratch;
    juce::SmoothedValue<float> driveSm, mixSm, outSm;

    int maxBlock = 512;
    int currentProgram = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TunersKickAudioProcessor)
};
