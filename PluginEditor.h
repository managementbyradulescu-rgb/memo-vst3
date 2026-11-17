#pragma once
#include "PluginProcessor.h"

namespace memo_ui
{
const juce::Colour cream    { 0xfff6efe7 };
const juce::Colour cream2   { 0xffe9dfd3 };
const juce::Colour pink     { 0xffff4fa3 };
const juce::Colour pinkDark { 0xffd1236b };
const juce::Colour pinkSoft { 0xffffa3cf };
const juce::Colour screenBg { 0xff1b0f17 };
const juce::Colour ink      { 0xff3a2a33 };
}

class PinkLook : public juce::LookAndFeel_V4
{
public:
    PinkLook();
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override;
};

class MemoAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit MemoAudioProcessorEditor (MemoAudioProcessor&);
    ~MemoAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    void applyPrompt();
    static void drawDevil (juce::Graphics&, juce::Rectangle<float>);

    MemoAudioProcessor& proc;
    PinkLook look;
    juce::TextEditor promptBox;
    juce::TextButton mixButton { "MIX" };
    juce::Label status;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Rectangle<int> screenArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MemoAudioProcessorEditor)
};
