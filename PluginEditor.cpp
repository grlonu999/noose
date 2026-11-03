#include "PluginEditor.h"

namespace
{
    juce::Colour ropeBody (float tension)
    {
        return juce::Colour (0xff8a6c43).interpolatedWith (juce::Colour (0xff9a4a36), tension * 0.5f);
    }
}

//==============================================================================
NooseLookAndFeel::NooseLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,    juce::Colour (0xffc9bfae));
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,            juce::Colour (0xffa89f90));
}

void NooseLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float startAngle, float endAngle,
                                         juce::Slider&)
{
    auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (6.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre  = bounds.getCentre();
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    g.setColour (juce::Colour (0xff17151a));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
    g.setColour (juce::Colour (0xff2a252c));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    const float arcRadius = radius - 5.0f;

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff2a2328));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
    g.setColour (juce::Colour (0xffb3261e).interpolatedWith (juce::Colour (0xffff5a3c), sliderPos));
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -radius + 11.0f, 3.0f, radius * 0.36f, 1.5f);
    g.setColour (juce::Colour (0xffd9cdb8));
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
}

//==============================================================================
NooseAudioProcessorEditor::NooseAudioProcessorEditor (NooseAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&laf);

    setupKnob (satSlider,  satLabel,  "SATURATION", " %");
    setupKnob (distSlider, distLabel, "DISTORTION", " %");
    setupKnob (toneSlider, toneLabel, "TONE",       " Hz");
    setupKnob (outSlider,  outLabel,  "OUTPUT",     " dB");
    setupKnob (mixSlider,  mixLabel,  "MIX",        " %");

    satAttach  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::saturation, satSlider);
    distAttach = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::distortion, distSlider);
    toneAttach = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::tone,       toneSlider);
    outAttach  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::output,     outSlider);
    mixAttach  = std::make_unique<SliderAttachment> (proc.apvts, ParamIDs::mix,        mixSlider);

    setSize (640, 520);
    startTimerHz (60);
}

NooseAudioProcessorEditor::~NooseAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NooseAudioProcessorEditor::setupKnob (juce::Slider& s, juce::Label& l,
                                           const juce::String& name, const juce::String& suffix)
{
    addAndMakeVisible (s);
    addAndMakeVisible (l);

    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 84, 16);
    s.setTextValueSuffix (suffix);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                           juce::MathConstants<float>::pi * 2.8f, true);

    l.setText (name, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.attachToComponent (&s, false);
}

//==============================================================================
void NooseAudioProcessorEditor::timerCallback()
{
    const float sat  = proc.apvts.getRawParameterValue (ParamIDs::saturation)->load() * 0.01f;
    const float dist = proc.apvts.getRawParameterValue (ParamIDs::distortion)->load() * 0.01f;

    const float target = 0.5f * (sat + dist);

    // Tightens quickly, loosens slowly.
    shownTension += (target - shownTension) * (target > shownTension ? 0.35f : 0.12f);

    const float lvl = juce::jlimit (0.0f, 1.0f, proc.outputLevel.load());
    shownLevel += (lvl - shownLevel) * (lvl > shownLevel ? 0.6f : 0.15f);

    shake = (rng.nextFloat() - 0.5f) * 0.05f * shownLevel * shownTension;

    repaint();
}

//==============================================================================
void NooseAudioProcessorEditor::drawRope (juce::Graphics& g, const juce::Path& centreline,
                                          float thickness, float tension)
{
    using PST = juce::PathStrokeType;

    g.setColour (juce::Colour (0xff2e2316));
    g.strokePath (centreline, PST (thickness, PST::curved, PST::butt));

    g.setColour (ropeBody (tension));
    g.strokePath (centreline, PST (thickness - 2.0f, PST::curved, PST::butt));

    // Twist bands along the rope; they stretch out a little under tension.
    const float dashes[] = { 4.0f + 3.0f * tension, 5.0f };
    juce::Path bands;
    PST (thickness - 2.0f, PST::curved, PST::butt).createDashedStroke (bands, centreline, dashes, 2);
    g.setColour (juce::Colour (0xffc2a46f).withAlpha (0.45f));
    g.fillPath (bands);
}

void NooseAudioProcessorEditor::paint (juce::Graphics& g)
{
    using PST = juce::PathStrokeType;

    const float t = juce::jlimit (0.0f, 1.0f, shownTension);
    const float cx = (float) getWidth() * 0.5f;

    const float beamY = 30.0f;
    const float knotTop = 104.0f, knotBottom = 140.0f;
    const float rMax = 86.0f, rMin = 11.0f;
    const float r = juce::jmax (rMin, juce::jmap (t, rMax, rMin) - shownLevel * t * 5.0f);

    // Background, with a red glow that grows as the noose tightens.
    g.fillAll (juce::Colour (0xff0b0a0d));
    {
        juce::ColourGradient glow (juce::Colour (0xffa31f16).withAlpha (0.06f + 0.30f * t), cx, 215.0f,
                                   juce::Colour (0xff0b0a0d).withAlpha (0.0f), cx, 515.0f, true);
        g.setGradientFill (glow);
        g.fillRect (getLocalBounds());
    }

    // Beam
    g.setColour (juce::Colour (0xff1c1712));
    g.fillRoundedRectangle (cx - 150.0f, 14.0f, 300.0f, 18.0f, 3.0f);
    g.setColour (juce::Colour (0xff32291f));
    g.drawLine (cx - 148.0f, 16.0f, cx + 148.0f, 16.0f, 1.0f);

    {
        juce::Graphics::ScopedSaveState state (g);

        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const float sway = (float) std::sin (now * 1.1) * 0.012f * (1.0f - 0.75f * t) + shake;
        g.addTransform (juce::AffineTransform::rotation (sway, cx, beamY));

        // Rope from the beam down to the knot
        juce::Path rope;
        rope.startNewSubPath (cx, beamY);
        rope.lineTo (cx, knotTop + 4.0f);
        drawRope (g, rope, 10.0f, t);

        g.setColour (juce::Colour (0xff4a4a4f));
        g.drawEllipse (cx - 7.0f, beamY - 7.0f, 14.0f, 14.0f, 2.5f);

        // Glow inside the loop (what's being squeezed)
        {
            juce::ColourGradient inner (juce::Colour (0xffff3b24).withAlpha (0.10f + 0.55f * t + 0.20f * shownLevel),
                                        cx, knotBottom + r,
                                        juce::Colour (0xffff3b24).withAlpha (0.0f),
                                        cx + r, knotBottom + r, true);
            g.setGradientFill (inner);
            g.fillEllipse (cx - r, knotBottom, r * 2.0f, r * 2.0f);
        }

        // The loop itself
        juce::Path loop;
        loop.addEllipse (cx - r, knotBottom, r * 2.0f, r * 2.0f);
        drawRope (g, loop, 10.0f, t);

        // Knot: a stack of diagonal wraps
        const int wraps = 6;
        const float step = (knotBottom - knotTop) / (float) wraps;
        for (int i = 0; i < wraps; ++i)
        {
            const float y0 = knotTop + step * (float) i + step * 0.5f;

            juce::Path w;
            w.startNewSubPath (cx - 14.0f, y0 - 2.5f);
            w.lineTo (cx + 14.0f, y0 + 2.5f);

            g.setColour (juce::Colour (0xff2e2316));
            g.strokePath (w, PST (step + 1.5f, PST::curved, PST::rounded));
            g.setColour (ropeBody (t));
            g.strokePath (w, PST (step - 1.0f, PST::curved, PST::rounded));
            g.setColour (juce::Colour (0xffc2a46f).withAlpha (0.35f));
            g.strokePath (w, PST (1.5f, PST::curved, PST::rounded));
        }
    }

    // Text
    g.setColour (juce::Colour (0xffd9cdb8));
    g.setFont (g.getCurrentFont().boldened().withHeight (22.0f));
    g.drawText ("NOOSE", 24, 16, 140, 28, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xff6f675c));
    g.setFont (11.0f);
    g.drawText ("SATURATION + DISTORTION", 24, 42, 160, 16, juce::Justification::centredLeft, false);

    g.setColour (juce::Colour (0xffa89f90).interpolatedWith (juce::Colour (0xffff4a30), t));
    g.setFont (13.0f);
    g.drawText ("TENSION  " + juce::String (juce::roundToInt (t * 100.0f)) + "%",
                (int) cx - 100, 316, 200, 18, juce::Justification::centred, false);

    g.setColour (juce::Colour (0x14ffffff));
    g.drawHorizontalLine (340, 30.0f, (float) getWidth() - 30.0f);
}

void NooseAudioProcessorEditor::resized()
{
    const int bigW = 120, smallW = 100;
    const int total = bigW * 2 + smallW * 3;
    int x = (getWidth() - total) / 2;

    satSlider.setBounds  (x, 370, bigW, 134);  x += bigW;
    distSlider.setBounds (x, 370, bigW, 134);  x += bigW;
    toneSlider.setBounds (x, 384, smallW, 120); x += smallW;
    outSlider.setBounds  (x, 384, smallW, 120); x += smallW;
    mixSlider.setBounds  (x, 384, smallW, 120);
}
