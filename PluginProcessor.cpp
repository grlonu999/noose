#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
NooseAudioProcessor::NooseAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    satParam  = apvts.getRawParameterValue (ParamIDs::saturation);
    distParam = apvts.getRawParameterValue (ParamIDs::distortion);
    toneParam = apvts.getRawParameterValue (ParamIDs::tone);
    outParam  = apvts.getRawParameterValue (ParamIDs::output);
    mixParam  = apvts.getRawParameterValue (ParamIDs::mix);
}

juce::AudioProcessorValueTreeState::ParameterLayout NooseAudioProcessor::createLayout()
{
    using APF = juce::AudioParameterFloat;
    using NR  = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::saturation, 1 }, "Saturation",
                                       NR (0.0f, 100.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::distortion, 1 }, "Distortion",
                                       NR (0.0f, 100.0f, 0.01f), 0.0f));
    layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::tone, 1 }, "Tone",
                                       NR (200.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::output, 1 }, "Output",
                                       NR (-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<APF> (juce::ParameterID { ParamIDs::mix, 1 }, "Mix",
                                       NR (0.0f, 100.0f, 0.01f), 100.0f));
    return layout;
}

//==============================================================================
bool NooseAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return in == out;
}

void NooseAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double osRate = sampleRate * 2.0;

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) samplesPerBlock, 2 };
    juce::dsp::ProcessSpec osSpec { osRate, (juce::uint32) samplesPerBlock * 2, 2 };

    oversampling.initProcessing ((size_t) samplesPerBlock);
    oversampling.reset();

    const float latency = oversampling.getLatencyInSamples();
    setLatencySamples (juce::roundToInt (latency));

    mixer.prepare (spec);
    mixer.setMixingRule (juce::dsp::DryWetMixingRule::linear);
    mixer.setWetLatency (latency);
    mixer.reset();

    toneFilter.prepare (osSpec);
    toneFilter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    toneFilter.reset();

    dcBlock.prepare (osSpec);
    dcBlock.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    dcBlock.setCutoffFrequency (20.0f);
    dcBlock.reset();

    for (auto* s : { &satDrive, &satBlend, &distDrive, &distBlend })
        s->reset (osRate, 0.03);

    const float sat  = satParam->load()  * 0.01f;
    const float dist = distParam->load() * 0.01f;
    satDrive.setCurrentAndTargetValue  (1.0f + sat * 8.0f);
    satBlend.setCurrentAndTargetValue  (juce::jmin (1.0f, sat * 4.0f));
    distDrive.setCurrentAndTargetValue (1.0f + dist * 40.0f);
    distBlend.setCurrentAndTargetValue (juce::jmin (1.0f, dist * 4.0f));

    lastOutGain = juce::Decibels::decibelsToGain (outParam->load());
}

//==============================================================================
void NooseAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int i = getTotalNumInputChannels(); i < getTotalNumOutputChannels(); ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const float sat     = satParam->load()  * 0.01f;
    const float dist    = distParam->load() * 0.01f;
    const float toneHz  = toneParam->load();
    const float outGain = juce::Decibels::decibelsToGain (outParam->load());
    const float mix     = mixParam->load() * 0.01f;

    satDrive.setTargetValue  (1.0f + sat * 8.0f);
    satBlend.setTargetValue  (juce::jmin (1.0f, sat * 4.0f));
    distDrive.setTargetValue (1.0f + dist * 40.0f);
    distBlend.setTargetValue (juce::jmin (1.0f, dist * 4.0f));
    toneFilter.setCutoffFrequency (toneHz);
    mixer.setWetMixProportion (mix);

    juce::dsp::AudioBlock<float> block (buffer);
    mixer.pushDrySamples (block);

    auto osBlock = oversampling.processSamplesUp (block);
    const size_t numSamples  = osBlock.getNumSamples();
    const size_t numChannels = juce::jmin ((size_t) 2, osBlock.getNumChannels());

    float* ch[2] = { nullptr, nullptr };
    for (size_t c = 0; c < numChannels; ++c)
        ch[c] = osBlock.getChannelPointer (c);

    for (size_t i = 0; i < numSamples; ++i)
    {
        const float sd = satDrive.getNextValue();
        const float sb = satBlend.getNextValue();
        const float dd = distDrive.getNextValue();
        const float db = distBlend.getNextValue();

        const float satNorm  = 1.0f / (std::tanh (sd) * std::sqrt (sd));
        const float distNorm = 1.0f / std::pow (dd, 0.3f);

        for (size_t c = 0; c < numChannels; ++c)
        {
            float x = ch[c][i];

            // Stage 1: saturation (smooth tanh curve)
            const float s = std::tanh (x * sd) * satNorm;
            x += sb * (s - x);

            // Stage 2: distortion (harder clipping with a little asymmetry)
            const float v      = x * dd + 0.2f * db;
            const float soft   = std::tanh (v);
            const float hard   = juce::jlimit (-1.0f, 1.0f, v);
            const float shaped = (soft * (1.0f - 0.6f * db) + hard * (0.6f * db)) * distNorm;
            x += db * (shaped - x);

            ch[c][i] = x;
        }
    }

    juce::dsp::ProcessContextReplacing<float> osContext (osBlock);
    toneFilter.process (osContext);
    dcBlock.process (osContext);

    oversampling.processSamplesDown (block);
    mixer.mixWetSamples (block);

    buffer.applyGainRamp (0, buffer.getNumSamples(), lastOutGain, outGain);
    lastOutGain = outGain;

    const float mag  = juce::jmin (1.0f, buffer.getMagnitude (0, buffer.getNumSamples()));
    const float prev = outputLevel.load();
    outputLevel.store (mag > prev ? mag : prev * 0.85f + mag * 0.15f);
}

//==============================================================================
juce::AudioProcessorEditor* NooseAudioProcessor::createEditor()
{
    return new NooseAudioProcessorEditor (*this);
}

void NooseAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NooseAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NooseAudioProcessor();
}
