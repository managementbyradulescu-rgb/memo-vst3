#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <algorithm>
#include <cmath>

// Lantul de efecte "Memo": toate modulele lucreaza pe max. 2 canale si sunt
// relative la nivelul mediu al vocii (threshold-urile se adapteaza singure).
namespace memo
{
constexpr float kPi = 3.14159265358979f;

inline float dbToLin (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
inline float linToDb (float lin) noexcept { return 20.0f * std::log10 (std::max (lin, 1.0e-7f)); }
inline float coefFromMs (float ms, double sr) noexcept
{
    return std::exp (-1.0f / (std::max (ms, 0.01f) * 0.001f * (float) sr));
}

struct Chans
{
    float* d[2] { nullptr, nullptr };
    int n = 0, numSamples = 0;
    explicit Chans (juce::AudioBuffer<float>& b)
    {
        n = std::min (b.getNumChannels(), 2);
        numSamples = b.getNumSamples();
        for (int c = 0; c < n; ++c)
            d[c] = b.getWritePointer (c);
    }
};

//==============================================================================
struct Biquad
{
    float b0 = 1.f, b1 = 0.f, b2 = 0.f, a1 = 0.f, a2 = 0.f;
    float z1[2] { 0.f, 0.f }, z2[2] { 0.f, 0.f };

    void reset() noexcept { z1[0] = z1[1] = z2[0] = z2[1] = 0.f; }

    float process (int ch, float x) noexcept
    {
        const float y = b0 * x + z1[ch];
        z1[ch] = b1 * x - a1 * y + z2[ch];
        z2[ch] = b2 * x - a2 * y;
        return y;
    }

    void setRaw (float B0, float B1, float B2, float A0, float A1, float A2) noexcept
    {
        const float inv = 1.0f / A0;
        b0 = B0 * inv; b1 = B1 * inv; b2 = B2 * inv; a1 = A1 * inv; a2 = A2 * inv;
    }

    static float clampFreq (double sr, float f) noexcept { return juce::jlimit (10.0f, (float) (sr * 0.45), f); }

    void makeHighPass (double sr, float f, float q) noexcept
    {
        f = clampFreq (sr, f);
        const float w = 2.f * kPi * f / (float) sr, c = std::cos (w), s = std::sin (w), al = s / (2.f * q);
        setRaw ((1.f + c) * 0.5f, -(1.f + c), (1.f + c) * 0.5f, 1.f + al, -2.f * c, 1.f - al);
    }

    void makePeak (double sr, float f, float q, float gainDb) noexcept
    {
        f = clampFreq (sr, f);
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w = 2.f * kPi * f / (float) sr, c = std::cos (w), s = std::sin (w), al = s / (2.f * q);
        setRaw (1.f + al * A, -2.f * c, 1.f - al * A, 1.f + al / A, -2.f * c, 1.f - al / A);
    }

    void makeHighShelf (double sr, float f, float gainDb) noexcept
    {
        f = clampFreq (sr, f);
        const float A = std::pow (10.f, gainDb / 40.f);
        const float w = 2.f * kPi * f / (float) sr, c = std::cos (w), s = std::sin (w);
        const float beta = 2.f * std::sqrt (A) * (s * 0.70710678f);
        setRaw (A * ((A + 1.f) + (A - 1.f) * c + beta),
                -2.f * A * ((A - 1.f) + (A + 1.f) * c),
                A * ((A + 1.f) + (A - 1.f) * c - beta),
                (A + 1.f) - (A - 1.f) * c + beta,
                2.f * ((A - 1.f) - (A + 1.f) * c),
                (A + 1.f) - (A - 1.f) * c - beta);
    }
};

// Nivel mediu lent (~1 s) al vocii; ignora tacerea ca sa nu "alunece" in jos.
struct LevelTracker
{
    float ms = 0.003f, coef = 0.9999f;
    void prepare (double sr) { coef = std::exp (-1.0f / (float) sr); ms = 0.003f; }
    void update (float p) noexcept { if (p > 1.0e-6f) ms = coef * ms + (1.f - coef) * p; }
    float db() const noexcept { return 10.f * std::log10 (std::max (ms, 1.0e-9f)); }
};

//==============================================================================
// 1) Noise suppressor: expander downward cu sidechain filtrat
struct NoiseSuppressor
{
    double sr = 44100.0;
    Biquad sc;
    LevelTracker avg;
    float env = 0.f, gainDb = 0.f;

    void prepare (double s)
    {
        sr = s; sc.reset(); sc.makeHighPass (sr, 150.f, 0.7071f);
        avg.prepare (sr); env = 0.f; gainDb = 0.f;
    }

    void process (juce::AudioBuffer<float>& b, float amount)
    {
        if (amount < 0.001f) return;
        Chans c (b);
        const float thrRel = -26.f - 10.f * amount;
        const float range = 6.f + 18.f * amount;
        const float aOpen = coefFromMs (3.f, sr), aClose = coefFromMs (120.f, sr);
        const float eA = coefFromMs (0.5f, sr), eR = coefFromMs (30.f, sr);

        for (int i = 0; i < c.numSamples; ++i)
        {
            float det = 0.f, p = 0.f;
            for (int ch = 0; ch < c.n; ++ch)
            {
                const float x = c.d[ch][i];
                det = std::max (det, std::abs (sc.process (ch, x)));
                p = std::max (p, x * x);
            }
            avg.update (p);
            env = det > env ? eA * env + (1.f - eA) * det : eR * env + (1.f - eR) * det;

            const float lev = linToDb (env);
            const float thr = avg.db() + thrRel;
            const float target = lev < thr ? -std::min ((thr - lev) * 1.5f, range) : 0.f;
            const float a = target > gainDb ? aOpen : aClose;
            gainDb = a * gainDb + (1.f - a) * target;

            const float g = dbToLin (gainDb);
            for (int ch = 0; ch < c.n; ++ch)
                c.d[ch][i] *= g;
        }
    }
};

//==============================================================================
// 2) Upward compression: ridica partile line, cu protectie pentru zgomotul de fond
struct UpwardComp
{
    double sr = 44100.0;
    LevelTracker avg;
    float env = 0.f, gainDb = 0.f;

    void prepare (double s) { sr = s; avg.prepare (sr); env = 0.f; gainDb = 0.f; }

    void process (juce::AudioBuffer<float>& b, float amount)
    {
        if (amount < 0.001f) return;
        Chans c (b);
        const float maxGain = 10.f * amount, ratio = 3.f, slope = 1.f - 1.f / ratio;
        const float eA = coefFromMs (5.f, sr), eR = coefFromMs (60.f, sr);
        const float aUp = coefFromMs (120.f, sr), aDown = coefFromMs (8.f, sr);

        for (int i = 0; i < c.numSamples; ++i)
        {
            float det = 0.f;
            for (int ch = 0; ch < c.n; ++ch)
                det = std::max (det, std::abs (c.d[ch][i]));
            avg.update (det * det);
            env = det > env ? eA * env + (1.f - eA) * det : eR * env + (1.f - eR) * det;

            const float lev = linToDb (env);
            const float avgDb = avg.db();
            const float thr = avgDb - 4.f;
            float target = 0.f;
            if (lev < thr)
            {
                const float fade = juce::jlimit (0.f, 1.f, (lev - (avgDb - 48.f)) / 10.f);
                target = std::min ((thr - lev) * slope, maxGain) * fade;
            }
            const float a = target > gainDb ? aUp : aDown;
            gainDb = a * gainDb + (1.f - a) * target;

            const float g = dbToLin (gainDb);
            for (int ch = 0; ch < c.n; ++ch)
                c.d[ch][i] *= g;
        }
    }
};

//==============================================================================
// Compresor generic, threshold relativ la nivelul mediu (merge pe orice volum de intrare)
struct Compressor
{
    double sr = 44100.0;
    LevelTracker avg;
    float ms = 0.f, gr = 0.f;

    // setari (se pun inainte de process)
    float thrRelDb = 0.f, ratio = 4.f, kneeDb = 6.f, attackMs = 5.f, releaseMs = 100.f, makeupDb = 0.f;
    bool opto = false, rms = false;

    void prepare (double s) { sr = s; avg.prepare (sr); ms = 0.f; gr = 0.f; }

    void process (juce::AudioBuffer<float>& b)
    {
        Chans c (b);
        const float cAtt = coefFromMs (attackMs, sr), cRel = coefFromMs (releaseMs, sr);
        const float rmsC = 1.f - coefFromMs (10.f, sr);
        const float slope = 1.f - 1.f / std::max (ratio, 1.f);
        const float knee = std::max (kneeDb, 0.01f);

        for (int i = 0; i < c.numSamples; ++i)
        {
            float pk = 0.f;
            for (int ch = 0; ch < c.n; ++ch)
                pk = std::max (pk, std::abs (c.d[ch][i]));
            avg.update (pk * pk);

            float level = pk;
            if (rms) { ms += (pk * pk - ms) * rmsC; level = std::sqrt (ms); }

            const float over = linToDb (level) - (avg.db() + thrRelDb);
            float red = 0.f;
            if (2.f * over > -knee)
            {
                if (2.f * over < knee) { const float t = over + knee * 0.5f; red = slope * t * t / (2.f * knee); }
                else red = slope * over;
            }

            float a;
            if (red > gr) a = cAtt;
            else if (opto) a = coefFromMs (60.f + 840.f * juce::jlimit (0.f, 1.f, gr / 10.f), sr);
            else a = cRel;
            gr = a * gr + (1.f - a) * red;

            const float g = dbToLin (-gr + makeupDb);
            for (int ch = 0; ch < c.n; ++ch)
                c.d[ch][i] *= g;
        }
    }
};

//==============================================================================
// Saturatie + exciter pe armonici (claritate)
struct Saturator
{
    Biquad hp;
    void prepare (double sr) { hp.reset(); hp.makeHighPass (sr, 2500.f, 0.7071f); }

    void process (juce::AudioBuffer<float>& b, float amount)
    {
        if (amount < 0.001f) return;
        Chans c (b);
        const float drive = 1.f + 3.f * amount, bias = 0.12f;
        const float off = std::tanh (drive * bias);
        for (int i = 0; i < c.numSamples; ++i)
            for (int ch = 0; ch < c.n; ++ch)
            {
                const float x = c.d[ch][i];
                const float y = (std::tanh (drive * (x + bias)) - off) / drive;
                const float harm = y - x;
                c.d[ch][i] = x + amount * (0.7f * harm + 0.9f * hp.process (ch, harm));
            }
    }
};

//==============================================================================
// De-esser pe banda (split: doar inaltele sunt reduse)
struct DeEsser
{
    double sr = 44100.0;
    Biquad hp;
    LevelTracker avg;
    float env = 0.f, gr = 0.f;

    void prepare (double s)
    {
        sr = s; hp.reset(); hp.makeHighPass (sr, 5500.f, 0.8f);
        avg.prepare (sr); env = 0.f; gr = 0.f;
    }

    void process (juce::AudioBuffer<float>& b, float amount)
    {
        if (amount < 0.001f) return;
        Chans c (b);
        const float thrRel = -8.f - 10.f * amount, maxRed = 4.f + 14.f * amount;
        const float eA = coefFromMs (0.3f, sr), eR = coefFromMs (30.f, sr);
        const float gA = coefFromMs (0.5f, sr), gR = coefFromMs (40.f, sr);

        for (int i = 0; i < c.numSamples; ++i)
        {
            float band[2] { 0.f, 0.f }, x[2] { 0.f, 0.f };
            float det = 0.f, p = 0.f;
            for (int ch = 0; ch < c.n; ++ch)
            {
                x[ch] = c.d[ch][i];
                band[ch] = hp.process (ch, x[ch]);
                det = std::max (det, std::abs (band[ch]));
                p = std::max (p, x[ch] * x[ch]);
            }
            avg.update (p);
            env = det > env ? eA * env + (1.f - eA) * det : eR * env + (1.f - eR) * det;

            const float over = linToDb (env) - (avg.db() + thrRel);
            const float red = juce::jlimit (0.f, maxRed, over * 0.8f);
            const float a = red > gr ? gA : gR;
            gr = a * gr + (1.f - a) * red;

            const float g = dbToLin (-gr);
            for (int ch = 0; ch < c.n; ++ch)
                c.d[ch][i] = x[ch] - band[ch] + band[ch] * g;
        }
    }
};

//==============================================================================
// EQ-urile: resonance cleaner + low cut + boxy cut + presence + air
struct EqChain
{
    double sr = 44100.0;
    Biquad res1, res2, res3, hpA, hpB, boxy, pres, air;

    void prepare (double s)
    {
        sr = s;
        for (auto* f : { &res1, &res2, &res3, &hpA, &hpB, &boxy, &pres, &air }) f->reset();
    }

    void update (float resAmt, float hpfHz, float boxyAmt, float clarity, float airAmt)
    {
        res1.makePeak (sr, 240.f, 3.0f, -3.5f * resAmt);
        res2.makePeak (sr, 520.f, 4.0f, -2.5f * resAmt);
        res3.makePeak (sr, 3100.f, 3.5f, -3.0f * resAmt);
        hpA.makeHighPass (sr, hpfHz, 0.5412f);
        hpB.makeHighPass (sr, hpfHz, 1.3066f);
        boxy.makePeak (sr, 350.f, 1.1f, -6.f * boxyAmt);
        pres.makePeak (sr, 3400.f, 0.8f, 5.f * clarity);
        air.makeHighShelf (sr, 10000.f, 6.f * airAmt);
    }

    void process (juce::AudioBuffer<float>& b)
    {
        Chans c (b);
        for (int i = 0; i < c.numSamples; ++i)
            for (int ch = 0; ch < c.n; ++ch)
            {
                float x = c.d[ch][i];
                x = res1.process (ch, x);
                x = res2.process (ch, x);
                x = res3.process (ch, x);
                x = hpA.process (ch, x);
                x = hpB.process (ch, x);
                x = boxy.process (ch, x);
                x = pres.process (ch, x);
                x = air.process (ch, x);
                c.d[ch][i] = x;
            }
    }
};

//==============================================================================
// Delay stereo cu feedback filtrat (ton intunecat)
struct Delay
{
    juce::AudioBuffer<float> buf;
    int size = 0, pos = 0;
    double sr = 44100.0;
    float lp[2] { 0.f, 0.f };
    juce::SmoothedValue<float> timeSmooth;

    void prepare (double s)
    {
        sr = s;
        size = (int) (sr * 2.2) + 4;
        buf.setSize (2, size);
        buf.clear();
        pos = 0; lp[0] = lp[1] = 0.f;
        timeSmooth.reset (sr, 0.08);
        timeSmooth.setCurrentAndTargetValue (0.25f * (float) sr);
    }

    void process (juce::AudioBuffer<float>& b, float timeMs, float fb, float mix)
    {
        if (mix < 0.001f)
        {
            buf.clear(); lp[0] = lp[1] = 0.f;
            return;
        }
        Chans c (b);
        timeSmooth.setTargetValue (timeMs * 0.001f * (float) sr);
        fb = juce::jlimit (0.f, 0.92f, fb);

        for (int i = 0; i < c.numSamples; ++i)
        {
            const float d = timeSmooth.getNextValue();
            for (int ch = 0; ch < c.n; ++ch)
            {
                float* line = buf.getWritePointer (ch);
                float rp = (float) pos - d * (ch == 1 ? 1.07f : 1.0f);
                while (rp < 0.f) rp += (float) size;
                const int i0 = (int) rp % size;
                const int i1 = (i0 + 1) % size;
                const float fr = rp - std::floor (rp);
                const float del = line[i0] + fr * (line[i1] - line[i0]);

                lp[ch] += 0.45f * (del - lp[ch]);
                const float in = c.d[ch][i];
                line[pos] = in + lp[ch] * fb;
                c.d[ch][i] = in + mix * lp[ch];
            }
            if (++pos >= size) pos = 0;
        }
    }
};

inline float softLimit (float x) noexcept
{
    const float t = 0.89f, a = std::abs (x);
    if (a < t) return x;
    const float y = t + (1.f - t) * std::tanh ((a - t) / (1.f - t));
    return x < 0.f ? -y : y;
}
} // namespace memo
