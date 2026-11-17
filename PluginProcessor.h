#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Dsp.h"

class MemoAudioProcessor : public juce::AudioProcessor
{
public:
    MemoAudioProcessor();
    ~MemoAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Memo"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;
    juce::String promptText;   // ultimul text scris de utilizator (se salveaza in proiect)

private:
    struct Params
    {
        std::atomic<float> *noise, *upward, *peak, *reson, *hpf, *boxy, *clarity, *air, *sat, *deess,
                           *c1176, *la2a, *dlyTime, *dlyFb, *dlyMix, *revSize, *revMix, *boost;
    } p {};

    memo::NoiseSuppressor noiseSup;
    memo::UpwardComp upward;
    memo::Compressor peakComp, fetComp, optoComp;
    memo::EqChain eq;
    memo::Saturator sat;
    memo::DeEsser deesser;
    memo::Delay delay;
    juce::Reverb reverb;
    bool reverbWasOn = false;
    float prevGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoAudioProcessor)
};
