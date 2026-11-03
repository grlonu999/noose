#pragma once

#include "PluginProcessor.h"

//==============================================================================
class NooseLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NooseLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;
};

//==============================================================================
class NooseAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::Timer
{
public:
    explicit NooseAudioProcessorEditor (NooseAudioProcessor&);
    ~NooseAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, juce::Label&, const juce::String& name, const juce::String& suffix);
    static void drawRope (juce::Graphics&, const juce::Path& centreline, float thickness, float tension);

    NooseAudioProcessor& proc;
    NooseLookAndFeel laf;

    juce::Slider satSlider, distSlider, toneSlider, outSlider, mixSlider;
    juce::Label  satLabel,  distLabel,  toneLabel,  outLabel,  mixLabel;

    std::unique_ptr<SliderAttachment> satAttach, distAttach, toneAttach, outAttach, mixAttach;

    juce::Random rng;
    float shownTension = 0.0f; // 0 = loose, 1 = fully tightened
    float shownLevel   = 0.0f; // output level, used to make the rope twitch
    float shake        = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NooseAudioProcessorEditor)
};
