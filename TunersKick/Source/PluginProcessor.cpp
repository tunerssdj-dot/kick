#include "PluginProcessor.h"

#include <vector>

//==============================================================================
// Presety: kazdy zaczyna od wartosci domyslnych ("Init"), potem nadpisuje wybrane parametry.
namespace
{
    struct PV     { const char* id; float value; };
    struct Preset { const char* name; std::vector<PV> values; };

    const std::vector<Preset>& getPresets()
    {
        static const std::vector<Preset> presets
        {
            { "Init", {} },

            { "Hard Techno", {
                { "length", 300.f }, { "pitchAmt", 40.f }, { "pitchDecay", 40.f }, { "bodyDrive", 20.f },
                { "clickLevel", 0.45f }, { "rumbleLevel", 0.3f }, { "rumbleDecay", 650.f },
                { "distType", 1.f }, { "drive", 16.f }, { "clip", -3.f } } },

            { "Barrel", {
                { "length", 260.f }, { "pitchAmt", 44.f }, { "pitchDecay", 35.f }, { "bodyDrive", 22.f },
                { "clickLevel", 0.5f }, { "clickFreq", 2800.f },
                { "barrelAmt", 0.8f }, { "barrelFreq", 200.f }, { "barrelQ", 10.f }, { "barrelTrack", 0.6f },
                { "rumbleLevel", 0.25f },
                { "distType", 0.f }, { "drive", 16.f }, { "clip", -4.f } } },

            { "Industrial", {
                { "length", 360.f }, { "pitchAmt", 40.f }, { "pitchDecay", 60.f }, { "bodyDrive", 28.f },
                { "clickLevel", 0.7f }, { "clickFreq", 5500.f }, { "clickDecay", 6.f },
                { "barrelAmt", 0.35f }, { "barrelFreq", 300.f }, { "barrelQ", 6.f },
                { "metalLevel", 0.25f }, { "metalFreq", 700.f }, { "metalRatio", 3.5f },
                { "metalIndex", 6.f }, { "metalDecay", 90.f },
                { "rumbleLevel", 0.45f }, { "rumbleDecay", 750.f },
                { "distType", 2.f }, { "drive", 18.f }, { "clip", -5.f } } },

            { "Metal", {
                { "length", 280.f }, { "pitchAmt", 36.f }, { "pitchDecay", 40.f }, { "bodyDrive", 16.f },
                { "clickLevel", 0.45f }, { "barrelAmt", 0.2f },
                { "metalLevel", 0.6f }, { "metalFreq", 1100.f }, { "metalRatio", 2.76f },
                { "metalIndex", 7.f }, { "metalDecay", 180.f },
                { "rumbleLevel", 0.2f },
                { "distType", 1.f }, { "drive", 14.f }, { "clip", -3.f } } },

            { "Rawstyle", {
                { "tune", 31.f }, { "length", 480.f }, { "pitchAmt", 48.f }, { "pitchDecay", 70.f },
                { "bodyDrive", 26.f }, { "clickLevel", 0.4f },
                { "barrelAmt", 0.5f }, { "barrelFreq", 180.f }, { "barrelQ", 8.f },
                { "rumbleLevel", 0.5f }, { "rumbleDecay", 900.f },
                { "distType", 1.f }, { "drive", 20.f }, { "clip", -4.f } } },
        };
        return presets;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TunersKickAudioProcessor::createLayout()
{
    using namespace juce;
    using Group  = AudioProcessorParameterGroup;
    using FloatP = AudioParameterFloat;

    auto f = [] (const char* id, const char* name, float lo, float hi, float def, float skew, const char* unit)
    {
        return std::make_unique<FloatP> (ParameterID { id, 1 }, name,
                                         NormalisableRange<float> (lo, hi, 0.0f, skew), def,
                                         AudioParameterFloatAttributes().withLabel (unit));
    };

    // Tune: wartosc to nuta MIDI, wyswietlamy nazwe nuty i czestotliwosc.
    auto tune = std::make_unique<FloatP> (
        ParameterID { "tune", 1 }, "Tune",
        NormalisableRange<float> (24.0f, 48.0f, 0.01f), 29.0f,
        AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
        {
            const float hz = 440.0f * std::exp2 ((v - 69.0f) / 12.0f);
            return MidiMessage::getMidiNoteName (roundToInt (v), true, true, 4) + "  " + String (hz, 1) + " Hz";
        }));

    auto body = std::make_unique<Group> ("body", "Body", "|");
    body->addChild (std::move (tune));
    body->addChild (f ("pitchAmt",   "Pitch Amount", 0.0f,  72.0f,  36.0f, 1.0f, " st"));
    body->addChild (f ("pitchDecay", "Pitch Decay",  5.0f,  300.0f, 45.0f, 0.5f, " ms"));
    body->addChild (f ("length",     "Length",       80.0f, 900.0f, 320.0f, 0.6f, " ms"));
    body->addChild (f ("bodyDrive",  "Body Drive",   0.0f,  40.0f,  12.0f, 1.0f, " dB"));
    body->addChild (std::make_unique<AudioParameterBool> (ParameterID { "keyTrack", 1 }, "Key Track", false));

    auto click = std::make_unique<Group> ("click", "Click", "|");
    click->addChild (f ("clickLevel", "Click Level", 0.0f,   1.0f,     0.4f,    1.0f, ""));
    click->addChild (f ("clickFreq",  "Click Freq",  500.0f, 12000.0f, 3500.0f, 0.4f, " Hz"));
    click->addChild (f ("clickDecay", "Click Decay", 0.5f,   20.0f,    4.0f,    0.5f, " ms"));

    auto barrel = std::make_unique<Group> ("barrel", "Barrel", "|");
    barrel->addChild (f ("barrelAmt",   "Barrel Amount", 0.0f,  1.0f,   0.0f,   1.0f, ""));
    barrel->addChild (f ("barrelFreq",  "Barrel Freq",   80.0f, 600.0f, 220.0f, 0.5f, " Hz"));
    barrel->addChild (f ("barrelQ",     "Barrel Q",      1.0f,  30.0f,  8.0f,   0.5f, ""));
    barrel->addChild (f ("barrelTrack", "Barrel Track",  0.0f,  1.0f,   0.5f,   1.0f, ""));

    auto metal = std::make_unique<Group> ("metal", "Metal", "|");
    metal->addChild (f ("metalLevel", "Metal Level", 0.0f,   1.0f,    0.0f,   1.0f, ""));
    metal->addChild (f ("metalFreq",  "Metal Freq",  150.0f, 5000.0f, 900.0f, 0.4f, " Hz"));
    metal->addChild (f ("metalRatio", "Metal Ratio", 0.5f,   9.0f,    2.76f,  1.0f, ""));
    metal->addChild (f ("metalIndex", "Metal FM",    0.0f,   12.0f,   4.0f,   1.0f, ""));
    metal->addChild (f ("metalDecay", "Metal Decay", 10.0f,  600.0f,  120.0f, 0.5f, " ms"));

    auto rumble = std::make_unique<Group> ("rumble", "Rumble", "|");
    rumble->addChild (f ("rumbleLevel", "Rumble Level", 0.0f,   1.0f,    0.0f,   1.0f, ""));
    rumble->addChild (f ("rumbleDecay", "Rumble Decay", 100.0f, 1500.0f, 600.0f, 0.6f, " ms"));
    rumble->addChild (f ("rumbleTone",  "Rumble Tone",  0.0f,   36.0f,   12.0f,  1.0f, " st"));

    auto master = std::make_unique<Group> ("master", "Distortion / Output", "|");
    master->addChild (std::make_unique<AudioParameterChoice> (
        ParameterID { "distType", 1 }, "Dist Type", StringArray { "Soft", "Hard", "Fold" }, 0));
    master->addChild (f ("drive",    "Drive",      0.0f,   48.0f, 12.0f, 1.0f, " dB"));
    master->addChild (f ("distMix",  "Dist Mix",   0.0f,   1.0f,  1.0f,  1.0f, ""));
    master->addChild (f ("clip",     "Clip",      -18.0f,  0.0f,  -3.0f, 1.0f, " dB"));
    master->addChild (f ("output",   "Output",    -24.0f,  0.0f,  -2.0f, 1.0f, " dB"));
    master->addChild (f ("velSens",  "Vel Sens",   0.0f,   1.0f,  0.5f,  1.0f, ""));

    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::move (body));
    layout.add (std::move (click));
    layout.add (std::move (barrel));
    layout.add (std::move (metal));
    layout.add (std::move (rumble));
    layout.add (std::move (master));
    return layout;
}

//==============================================================================
TunersKickAudioProcessor::TunersKickAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout()),
      oversampling (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true)
{
}

tuners::KickParams TunersKickAudioProcessor::readVoiceParams() const
{
    tuners::KickParams k;
    k.tune        = raw ("tune");
    k.pitchAmt    = raw ("pitchAmt");
    k.pitchDecay  = raw ("pitchDecay");
    k.length      = raw ("length");
    k.bodyDrive   = raw ("bodyDrive");
    k.keyTrack    = raw ("keyTrack") > 0.5f;

    k.clickLevel  = raw ("clickLevel");
    k.clickFreq   = raw ("clickFreq");
    k.clickDecay  = raw ("clickDecay");

    k.barrelAmt   = raw ("barrelAmt");
    k.barrelFreq  = raw ("barrelFreq");
    k.barrelQ     = raw ("barrelQ");
    k.barrelTrack = raw ("barrelTrack");

    k.metalLevel  = raw ("metalLevel");
    k.metalFreq   = raw ("metalFreq");
    k.metalRatio  = raw ("metalRatio");
    k.metalIndex  = raw ("metalIndex");
    k.metalDecay  = raw ("metalDecay");

    k.rumbleLevel = raw ("rumbleLevel");
    k.rumbleDecay = raw ("rumbleDecay");
    k.rumbleTone  = raw ("rumbleTone");

    k.velSens     = raw ("velSens");
    return k;
}

//==============================================================================
void TunersKickAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    maxBlock = juce::jmax (samplesPerBlock, 64);

    voice.prepare (sampleRate);
    scratch.setSize (1, maxBlock);

    oversampling.initProcessing ((size_t) maxBlock);
    oversampling.reset();
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));

    driveSm.reset (sampleRate, 0.02);
    mixSm.reset   (sampleRate, 0.02);
    outSm.reset   (sampleRate, 0.02);
    driveSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw ("drive")));
    mixSm.setCurrentAndTargetValue   (raw ("distMix"));
    outSm.setCurrentAndTargetValue   (juce::Decibels::decibelsToGain (raw ("output")));
}

bool TunersKickAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void TunersKickAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (scratch.getNumSamples() < numSamples)
        scratch.setSize (1, numSamples, false, false, true);

    float* mono = scratch.getWritePointer (0);

    // --- 1) glos kicka, z dokladnoscia do probki dla MIDI
    const auto voiceParams = readVoiceParams();

    int pos = 0;
    auto renderTo = [&] (int end)
    {
        for (int i = pos; i < end; ++i)
            mono[i] = voice.getNextSample();
        pos = end;
    };

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        renderTo (juce::jlimit (pos, numSamples, meta.samplePosition));

        if (msg.isNoteOn())
            voice.trigger (voiceParams, msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isAllSoundOff())
            voice.kill();
    }
    renderTo (numSamples);

    // --- 2) distortion + clipper w 4x oversamplingu (mniej aliasingu)
    const int distType = (int) std::lround (raw ("distType"));
    const float clipT  = juce::Decibels::decibelsToGain (raw ("clip"));
    const float invClip = 1.0f / clipT;

    driveSm.setTargetValue (juce::Decibels::decibelsToGain (raw ("drive")));
    mixSm.setTargetValue   (raw ("distMix"));
    outSm.setTargetValue   (juce::Decibels::decibelsToGain (raw ("output")));

    for (int start = 0; start < numSamples; start += maxBlock)
    {
        const int count = juce::jmin (maxBlock, numSamples - start);

        juce::dsp::AudioBlock<const float> inBlock (scratch.getArrayOfReadPointers(), 1, (size_t) start, (size_t) count);
        juce::dsp::AudioBlock<float> outBlock (scratch.getArrayOfWritePointers(), 1, (size_t) start, (size_t) count);

        auto up = oversampling.processSamplesUp (inBlock);
        float* x = up.getChannelPointer (0);

        for (int i = 0; i < count; ++i)
        {
            const float drive = driveSm.getNextValue();
            const float mix   = mixSm.getNextValue();

            for (int j = 0; j < 4; ++j)
            {
                float& s = x[i * 4 + j];
                const float dry = s;
                const float wet = tuners::shapeSample (dry * drive, distType);
                const float y   = dry + mix * (wet - dry);
                s = juce::jlimit (-clipT, clipT, y) * invClip;
            }
        }

        oversampling.processSamplesDown (outBlock);
    }

    // --- 3) glosnosc wyjsciowa i kopia na kanaly
    for (int i = 0; i < numSamples; ++i)
        mono[i] *= outSm.getNextValue();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono, numSamples);
}

//==============================================================================
juce::AudioProcessorEditor* TunersKickAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

int TunersKickAudioProcessor::getNumPrograms()    { return (int) getPresets().size(); }
int TunersKickAudioProcessor::getCurrentProgram() { return currentProgram; }

const juce::String TunersKickAudioProcessor::getProgramName (int index)
{
    const auto& presets = getPresets();
    return (index >= 0 && index < (int) presets.size()) ? presets[(size_t) index].name : juce::String();
}

void TunersKickAudioProcessor::setCurrentProgram (int index)
{
    const auto& presets = getPresets();
    if (index < 0 || index >= (int) presets.size())
        return;

    currentProgram = index;

    // Najpierw wartosci domyslne (poza glosnoscia wyjsciowa), potem nadpisania presetu.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID != "output")
                rp->setValueNotifyingHost (rp->getDefaultValue());

    for (const auto& pv : presets[(size_t) index].values)
        if (auto* rp = apvts.getParameter (pv.id))
            rp->setValueNotifyingHost (rp->convertTo0to1 (pv.value));
}

void TunersKickAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void TunersKickAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

// Punkt wejscia dla JUCE
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TunersKickAudioProcessor();
}
