#pragma once

// Tuners Kick - silnik kicka.
// Czysty C++ (bez JUCE), dzieki temu mozna go testowac osobno.
//
// Warstwy: click (szum), body (sinus + pitch envelope + distortion),
// barrel (rezonator pasmowy), metal (FM, dzwieczacy), rumble (szum w tonacji).

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace tuners
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float dbToLin (float db) noexcept { return std::pow (10.0f, db / 20.0f); }

//==============================================================================
// Parametry pojedynczego uderzenia (zatrzaskiwane przy note-on).
struct KickParams
{
    float tune        = 29.0f;   // nuta MIDI, do ktorej opada body (29 = F1)
    float pitchAmt    = 36.0f;   // poczatek pitch envelope, w polotonach nad tune
    float pitchDecay  = 45.0f;   // ms (do -60 dB)
    float length      = 320.0f;  // ms (obwiednia glosnosci do -60 dB)
    float bodyDrive   = 12.0f;   // dB, distortion tylko na body
    bool  keyTrack    = false;   // true: wysokosc idzie za nuta MIDI (C2 = bez zmiany)

    float clickLevel  = 0.4f;
    float clickFreq   = 3500.0f; // Hz
    float clickDecay  = 4.0f;    // ms

    float barrelAmt   = 0.0f;
    float barrelFreq  = 220.0f;  // Hz
    float barrelQ     = 8.0f;
    float barrelTrack = 0.5f;    // jak mocno rezonans podaza za pitch envelope

    float metalLevel  = 0.0f;
    float metalFreq   = 900.0f;  // Hz, nosna FM
    float metalRatio  = 2.76f;   // stosunek modulator/nosna (nieharmoniczny = metal)
    float metalIndex  = 4.0f;
    float metalDecay  = 120.0f;  // ms

    float rumbleLevel = 0.0f;
    float rumbleDecay = 600.0f;  // ms
    float rumbleTone  = 12.0f;   // poltony nad tune

    float velSens     = 0.5f;    // wrazliwosc na velocity
};

//==============================================================================
// Filtr SVF (topology preserving transform), bezpieczny przy modulacji co probke.
struct SVF
{
    float s1 = 0.0f, s2 = 0.0f, g = 0.0f, r2 = 1.0f, h = 1.0f;

    void reset() noexcept { s1 = s2 = 0.0f; }

    void set (float cutoffHz, float q, float sampleRate) noexcept
    {
        cutoffHz = std::clamp (cutoffHz, 10.0f, 0.45f * sampleRate);
        g  = std::tan (kPi * cutoffHz / sampleRate);
        r2 = 1.0f / std::max (q, 0.1f);
        h  = 1.0f / (1.0f + r2 * g + g * g);
    }

    // Pasmowy, niewyrownany: szczyt wzmocnienia = Q.
    float bandpass (float x) noexcept
    {
        const float hp = (x - (r2 + g) * s1 - s2) * h;
        const float bp = g * hp + s1;
        s1 = g * hp + bp;
        const float lp = g * bp + s2;
        s2 = g * bp + lp;
        return bp;
    }

    // Pasmowy, wyrownany: szczyt wzmocnienia = 1.
    float bandpassNorm (float x) noexcept { return bandpass (x) * r2; }
};

//==============================================================================
// Distortion master: 0 = soft (tanh), 1 = hard clip, 2 = foldback.
inline float shapeSample (float x, int type) noexcept
{
    switch (type)
    {
        case 0:  return std::tanh (x);
        case 1:  return std::clamp (x, -1.0f, 1.0f);
        default:
        {
            float t = x * 0.25f + 0.25f;
            t -= std::floor (t);
            return 1.0f - 4.0f * std::fabs (t - 0.5f);
        }
    }
}

//==============================================================================
// Jeden glos (kick jest monofoniczny; nowe uderzenie ucina poprzednie bez klikniecia).
class KickVoice
{
public:
    void prepare (double newSampleRate)
    {
        sr    = (float) newSampleRate;
        invSr = 1.0f / sr;
        declickCoef = std::exp (-1.0f / (0.002f * sr));
        kill();
    }

    void kill() noexcept
    {
        active = false;
        lastOut = 0.0f;
        declick = 0.0f;
    }

    bool isActive() const noexcept { return active; }

    void trigger (const KickParams& params, int midiNote, float velocity) noexcept
    {
        p = params;

        const float note = p.tune + (p.keyTrack ? (float) midiNote - 36.0f : 0.0f);
        baseHz = std::clamp (440.0f * std::exp2 ((note - 69.0f) / 12.0f), 10.0f, 2000.0f);

        pitchEnv = 1.0f;  pitchCoef  = decayCoef (p.pitchDecay);
        ampEnv   = 1.0f;  ampCoef    = decayCoef (p.length);
        clickEnv = 1.0f;  clickCoef  = decayCoef (p.clickDecay);
        metalEnv = 1.0f;  metalCoef  = decayCoef (p.metalDecay);
        rumbleEnv = 1.0f; rumbleCoef = decayCoef (p.rumbleDecay);
        rumbleAtt = 0.0f; rumbleAttCoef = std::exp (-1.0f / (0.012f * sr));

        bodyDriveLin = std::max (1.0f, dbToLin (p.bodyDrive));
        bodyNorm     = 1.0f / std::tanh (bodyDriveLin);
        barrelGain   = 1.0f / std::sqrt (std::max (p.barrelQ, 1.0f));

        velGain = 1.0f - std::clamp (p.velSens, 0.0f, 1.0f) * (1.0f - std::clamp (velocity, 0.0f, 1.0f));

        bodyPhase = metalCarPhase = metalModPhase = 0.0f;

        clickBP.reset();  clickBP.set (p.clickFreq, 0.8f, sr);
        rumbleBP.reset(); rumbleBP.set (baseHz * std::exp2 (p.rumbleTone / 12.0f), 5.0f, sr);
        barrelBP.reset();

        // Plynne przejscie z poprzedniego uderzenia (bez klikniecia przy retrigerze).
        declick = lastOut;

        float longest = p.length;
        if (p.metalLevel  > 0.001f) longest = std::max (longest, p.metalDecay);
        if (p.rumbleLevel > 0.001f) longest = std::max (longest, p.rumbleDecay);
        totalSamples = std::max (64, (int) (longest * 0.001f * sr * 1.25f));
        fadeSamples  = std::max (32, totalSamples / 5);
        n = 0;
        active = true;
    }

    float getNextSample() noexcept
    {
        if (! active)
            return 0.0f;

        // --- body: sinus z pitch envelope, potem distortion
        const float pitchSemis = p.pitchAmt * pitchEnv;
        const float freq = std::min (baseHz * std::exp2 (pitchSemis / 12.0f), 0.45f * sr);
        bodyPhase += freq * invSr;
        bodyPhase -= std::floor (bodyPhase);

        const float sine = std::sin (kTwoPi * bodyPhase) * ampEnv;
        const float body = std::tanh (sine * bodyDriveLin) * bodyNorm;

        // --- click: przefiltrowany szum
        float click = 0.0f;
        if (p.clickLevel > 0.001f)
            click = clickBP.bandpassNorm (noise()) * clickEnv * p.clickLevel * 3.0f;

        // --- barrel: rezonator pasmowy pobudzany body + click, czestotliwosc podaza za pitch envelope
        float barrel = 0.0f;
        if (p.barrelAmt > 0.001f)
        {
            const float fc = p.barrelFreq * std::exp2 (p.barrelTrack * pitchSemis / 12.0f);
            barrelBP.set (fc, p.barrelQ, sr);
            barrel = barrelBP.bandpass (body + click) * p.barrelAmt * barrelGain;
        }

        // --- metal: FM z nieharmonicznym stosunkiem, dzwiek jak uderzenie w metal
        float metal = 0.0f;
        if (p.metalLevel > 0.001f)
        {
            const float modHz = std::min (p.metalFreq * p.metalRatio, 0.45f * sr);
            metalCarPhase += p.metalFreq * invSr;  metalCarPhase -= std::floor (metalCarPhase);
            metalModPhase += modHz * invSr;        metalModPhase -= std::floor (metalModPhase);
            const float mod = std::sin (kTwoPi * metalModPhase);
            metal = std::sin (kTwoPi * metalCarPhase + p.metalIndex * metalEnv * mod) * metalEnv * p.metalLevel;
        }

        // --- rumble: szum w tonacji kicka, narasta i wolno opada
        float rumble = 0.0f;
        if (p.rumbleLevel > 0.001f)
        {
            rumbleAtt += (1.0f - rumbleAtt) * (1.0f - rumbleAttCoef);
            rumble = rumbleBP.bandpassNorm (noise()) * rumbleEnv * rumbleAtt * p.rumbleLevel * 4.0f;
        }

        pitchEnv *= pitchCoef;   ampEnv *= ampCoef;      clickEnv *= clickCoef;
        metalEnv *= metalCoef;   rumbleEnv *= rumbleCoef;

        float out = (body + click + barrel + metal + rumble) * velGain;
        out += declick;
        declick *= declickCoef;

        // Koncowe wyciszenie, zeby ogon nie byl ucinany.
        const int left = totalSamples - n;
        if (left < fadeSamples)
        {
            const float f = (float) left / (float) fadeSamples;
            out *= f * f;
        }

        if (! std::isfinite (out))
        {
            kill();
            return 0.0f;
        }

        lastOut = out;
        if (++n >= totalSamples)
        {
            active = false;
            lastOut = 0.0f;
        }
        return out;
    }

private:
    float decayCoef (float ms) const noexcept
    {
        return std::exp (-6.9078f / (std::max (ms, 0.1f) * 0.001f * sr));
    }

    float noise() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float) (rng & 0xFFFFFF) * (2.0f / 16777215.0f) - 1.0f;
    }

    KickParams p;
    float sr = 44100.0f, invSr = 1.0f / 44100.0f;
    bool active = false;
    int n = 0, totalSamples = 0, fadeSamples = 0;

    float baseHz = 43.65f;
    float bodyPhase = 0.0f, metalCarPhase = 0.0f, metalModPhase = 0.0f;
    float pitchEnv = 0.0f, ampEnv = 0.0f, clickEnv = 0.0f, metalEnv = 0.0f, rumbleEnv = 0.0f, rumbleAtt = 0.0f;
    float pitchCoef = 0.0f, ampCoef = 0.0f, clickCoef = 0.0f, metalCoef = 0.0f, rumbleCoef = 0.0f, rumbleAttCoef = 0.0f;
    float bodyDriveLin = 1.0f, bodyNorm = 1.0f, barrelGain = 1.0f, velGain = 1.0f;
    float lastOut = 0.0f, declick = 0.0f, declickCoef = 0.99f;

    SVF clickBP, barrelBP, rumbleBP;
    std::uint32_t rng = 22222u;
};

} // namespace tuners
