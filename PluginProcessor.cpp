#include "PluginProcessor.h"
#include "PluginEditor.h"

MemoAudioProcessor::MemoAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    p.noise = get ("noise");   p.upward = get ("upward"); p.peak = get ("peak");   p.reson = get ("reson");
    p.hpf = get ("hpf");       p.boxy = get ("boxy");     p.clarity = get ("clarity"); p.air = get ("air");
    p.sat = get ("sat");       p.deess = get ("deess");   p.c1176 = get ("c1176"); p.la2a = get ("la2a");
    p.dlyTime = get ("dlytime"); p.dlyFb = get ("dlyfb"); p.dlyMix = get ("dlymix");
    p.revSize = get ("revsize"); p.revMix = get ("revmix"); p.boost = get ("boost");
}

juce::AudioProcessorValueTreeState::ParameterLayout MemoAudioProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    auto add = [&layout] (const char* id, const char* name, float lo, float hi, float def)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                                 juce::NormalisableRange<float> (lo, hi), def));
    };

    add ("noise",   "Noise Suppressor",  0.f, 1.f, 0.30f);
    add ("upward",  "Upward Comp",       0.f, 1.f, 0.25f);
    add ("peak",    "Peak Tamer",        0.f, 1.f, 0.35f);
    add ("reson",   "Resonance Clean",   0.f, 1.f, 0.30f);
    add ("hpf",     "Low Cut",           60.f, 200.f, 100.f);
    add ("boxy",    "Boxy Cut",          0.f, 1.f, 0.30f);
    add ("clarity", "Clarity",           0.f, 1.f, 0.40f);
    add ("air",     "Air",               0.f, 1.f, 0.35f);
    add ("sat",     "Saturation",        0.f, 1.f, 0.25f);
    add ("deess",   "De-Esser",          0.f, 1.f, 0.40f);
    add ("c1176",   "FET Comp (1176)",   0.f, 1.f, 0.40f);
    add ("la2a",    "Opto Comp (LA-2A)", 0.f, 1.f, 0.40f);
    add ("dlytime", "Delay Time",        20.f, 1000.f, 250.f);
    add ("dlyfb",   "Delay Feedback",    0.f, 0.9f, 0.25f);
    add ("dlymix",  "Delay Mix",         0.f, 1.f, 0.f);
    add ("revsize", "Reverb Size",       0.f, 1.f, 0.40f);
    add ("revmix",  "Reverb Mix",        0.f, 1.f, 0.f);
    add ("boost",   "Volume Boost",      0.f, 12.f, 0.f);

    return layout;
}

bool MemoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void MemoAudioProcessor::prepareToPlay (double sr, int)
{
    noiseSup.prepare (sr);
    upward.prepare (sr);
    peakComp.prepare (sr);
    fetComp.prepare (sr);
    optoComp.prepare (sr);
    eq.prepare (sr);
    sat.prepare (sr);
    deesser.prepare (sr);
    delay.prepare (sr);
    reverb.setSampleRate (sr);
    reverb.reset();
    reverbWasOn = false;
    prevGain = 1.0f;
}

void MemoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels(), numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();
    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);
    if (n == 0 || buffer.getNumChannels() == 0)
        return;

    // 1-2) curatare zgomot + upward compression
    noiseSup.process (buffer, p.noise->load());
    upward.process (buffer, p.upward->load());

    // 3) compresor care potoleste peak-urile
    if (const float a = p.peak->load(); a > 0.001f)
    {
        peakComp.thrRelDb = 8.f - 6.f * a;  peakComp.ratio = 4.f + 6.f * a;
        peakComp.kneeDb = 4.f;  peakComp.attackMs = 1.f;  peakComp.releaseMs = 60.f;
        peakComp.makeupDb = 0.f; peakComp.opto = false; peakComp.rms = false;
        peakComp.process (buffer);
    }

    // 4-6) EQ: rezonante, low cut, boxy, claritate, aer
    eq.update (p.reson->load(), p.hpf->load(), p.boxy->load(), p.clarity->load(), p.air->load());
    eq.process (buffer);

    // 7) saturatie
    sat.process (buffer, p.sat->load());

    // 8) de-esser
    deesser.process (buffer, p.deess->load());

    // 9) compresor stil 1176 (peak-uri rapide)
    if (const float a = p.c1176->load(); a > 0.001f)
    {
        fetComp.thrRelDb = 4.f - 8.f * a;  fetComp.ratio = 4.f + 8.f * a;
        fetComp.kneeDb = 3.f;  fetComp.attackMs = 0.4f;  fetComp.releaseMs = 70.f;
        fetComp.makeupDb = 3.f * a; fetComp.opto = false; fetComp.rms = false;
        fetComp.process (buffer);
    }

    // 10) compresor stil LA-2A (caracter, opto)
    if (const float a = p.la2a->load(); a > 0.001f)
    {
        optoComp.thrRelDb = 2.f - 6.f * a;  optoComp.ratio = 3.f + a;
        optoComp.kneeDb = 12.f;  optoComp.attackMs = 10.f;  optoComp.releaseMs = 300.f;
        optoComp.makeupDb = 2.5f * a; optoComp.opto = true; optoComp.rms = true;
        optoComp.process (buffer);
    }

    // 11) delay
    delay.process (buffer, p.dlyTime->load(), p.dlyFb->load(), p.dlyMix->load());

    // 12) reverb
    const float revMix = p.revMix->load();
    if (revMix > 0.001f)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.25f + 0.7f * p.revSize->load();
        rp.damping = 0.5f;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        rp.dryLevel = 0.5f;
        rp.wetLevel = revMix * 0.33f;
        reverb.setParameters (rp);

        if (buffer.getNumChannels() >= 2)
            reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), n);
        else
            reverb.processMono (buffer.getWritePointer (0), n);
        reverbWasOn = true;
    }
    else if (reverbWasOn)
    {
        reverb.reset();
        reverbWasOn = false;
    }

    // 13) boost de volum + limitator soft de siguranta
    const float gain = memo::dbToLin (p.boost->load());
    buffer.applyGainRamp (0, n, prevGain, gain);
    prevGain = gain;

    for (int ch = 0; ch < std::min (buffer.getNumChannels(), 2); ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
            d[i] = memo::softLimit (d[i]);
    }
}

juce::AudioProcessorEditor* MemoAudioProcessor::createEditor()
{
    return new MemoAudioProcessorEditor (*this);
}

void MemoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("prompt", promptText, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void MemoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto vt = juce::ValueTree::fromXml (*xml);
            promptText = vt.getProperty ("prompt").toString();
            apvts.replaceState (vt);
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MemoAudioProcessor();
}
