#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace ParamIDs
{
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* distortion = "distortion";
    inline constexpr const char* tone       = "tone";
    inline constexpr const char* output     = "output";
    inline constexpr const char* mix        = "mix";
}

class NooseAudioProcessor : public juce::AudioProcessor
{
public:
    NooseAudioProcessor();
    ~NooseAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Noose"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Read by the editor to make the rope react to the signal.
    std::atomic<float> outputLevel { 0.0f };

private:
    std::atomic<float>* satParam  = nullptr;
    std::atomic<float>* distParam = nullptr;
    std::atomic<float>* toneParam = nullptr;
    std::atomic<float>* outParam  = nullptr;
    std::atomic<float>* mixParam  = nullptr;

    // 2x oversampling around the nonlinear stages to keep aliasing down.
    juce::dsp::Oversampling<float> oversampling { 2, 1,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false };

    juce::dsp::DryWetMixer<float> mixer;
    juce::dsp::StateVariableTPTFilter<float> toneFilter, dcBlock;

    juce::SmoothedValue<float> satDrive, satBlend, distDrive, distBlend;
    float lastOutGain = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NooseAudioProcessor)
};
