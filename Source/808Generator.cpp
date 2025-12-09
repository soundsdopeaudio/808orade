#include "808Generator.h"

void Generator808::fillOsc(double phaseInc, double& phase, float* dest, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        dest[i] = (float)std::sin(phase);
        phase += phaseInc;
        if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
    }
}

juce::AudioBuffer<float> Generator808::renderToBuffer(const GeneratorParams& params)
{
    int numSamples = (int)std::lround(params.lengthSeconds * params.sampleRate);
    juce::AudioBuffer<float> buf(2, numSamples);
    buf.clear();
    render(params, buf);
    return buf;
}

void Generator808::render(const GeneratorParams& params, juce::AudioBuffer<float>& outBuffer)
{
    // seed
    rng.seed((uint64_t)params.seed ^ 0x9E3779B97F4A7C15ULL);

    // create a mono buffer first
    int numSamples = outBuffer.getNumSamples();
    juce::AudioBuffer<float> mono(1, numSamples);
    mono.clear();

    // generate raw waveform in mono
    generateWaveform(params, mono);

    // filtering / saturation / tone shaping
    applyFilterAndSaturation(mono, params);

    // write into stereo with optional width processing
    outBuffer.clear();
    outBuffer.addFrom(0, 0, mono, 0, 0, numSamples);
    outBuffer.addFrom(1, 0, mono, 0, 0, numSamples);

    // stereo width (detune/chorus / keep below 120Hz mono-summed)
    applyStereoWidth(outBuffer, params);

    // apply master gain and final limiter-ish normalization
    float gain = GeneratorVoiceUtils::dBToGain(params.masterGainDb);
    outBuffer.applyGain(gain);

    // quick soft clip to avoid hard digital clipping
    for (int ch = 0; ch < outBuffer.getNumChannels(); ++ch)
    {
        float* data = outBuffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            // soft clip using tanh-like curve
            float x = data[i];
            // simple soft clip
            float clipped = std::tanh(x * 1.2f);
            data[i] = clipped;
        }
    }
}

void Generator808::generateWaveform(const GeneratorParams& p, juce::AudioBuffer<float>& bufMono)
{
    int ns = bufMono.getNumSamples();
    float* dst = bufMono.getWritePointer(0);

    // pick a base MIDI note low in the 808 range: prefer 28-45 (~35–70 Hz)
    double baseMidi = 32.0 + (random01() * 10.0); // 32..42
    baseMidi += p.tuneSemitones;
    double freq = midiNoteToFreq(baseMidi);

    // apply keyword bias for "sub" and "deep"
    double subBias = (double)p.subAmount * -2.0; // lower by up to -2 semitones
    freq *= std::pow(2.0, subBias / 12.0);

    // oscillator phases
    double phase = 0.0;
    double phase2 = 0.0;

    double sr = p.sampleRate;
    double phi = juce::MathConstants<double>::twoPi * freq / sr;
    double phi2 = juce::MathConstants<double>::twoPi * freq * 2.0 / sr; // 2nd harmonic

    // envelopes
    // base durations (in seconds)
    double baseDecay = 0.8;
    baseDecay *= (1.0 + 0.8 * (double)p.boomAmount); // boomy → longer
    baseDecay *= (0.4 + 0.6 * (1.0 - (double)p.shortness)); // shortness reduces decay
    double attack = 0.002;
    double release = baseDecay * 0.5;

    // pitch pitch glide for punch (fast downward)
    double pitchGlideSec = 0.015 + 0.010 * random01();
    double maxPitchDrop = 0.24 + 1.0 * p.punch; // in semitones downward

    // amplitude envelope immediate values
    for (int i = 0; i < ns; ++i)
    {
        double t = (double)i / sr;

        // amp env (simple one-pole ADSR-ish)
        double env;
        if (t < attack) env = t / attack;
        else env = std::exp(-(t - attack) / baseDecay);

        // pitch envelope: exponential drop
        double pitchMult = 1.0;
        if (t < pitchGlideSec)
        {
            double frac = t / pitchGlideSec;
            double drop = maxPitchDrop * (1.0 - frac); // semitone drop that decays to 0
            pitchMult = std::pow(2.0, -drop / 12.0);
        }

        // main osc
        double curPhi = juce::MathConstants<double>::twoPi * freq * pitchMult / sr;
        double s1 = std::sin(phase);
        phase += curPhi;
        if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;

        // second harmonic for character
        double s2 = std::sin(phase2);
        phase2 += phi2;
        if (phase2 > juce::MathConstants<double>::twoPi) phase2 -= juce::MathConstants<double>::twoPi;

        // optional FM/growl (modulator simple)
        double fm = 0.0;
        if (p.growl > 0.001f)
        {
            double modFreq = freq * (1.8 + 1.2 * random01());
            double modPhase = std::sin(phase2 * 0.5 + 0.3);
            fm = p.growl * 0.25 * modPhase;
        }

        // simple body: fundamental + harmonic scaled by keywords
        double body = (1.0 - (0.25 * p.growl)) * s1 + 0.25 * s2 + fm;

        // sub-component: low sine added to beef up sub (phase locked)
        double sub = 0.0;
        if (p.subAmount > 0.001f)
        {
            double subFreq = freq * 0.5; // 1 octave below partial
            double subPhase = std::sin(juce::MathConstants<double>::twoPi * subFreq * ((double)i / sr));
            sub = p.subAmount * 0.8 * subPhase;
        }

        // combined
        double sample = (body * (1.0 - p.subAmount * 0.5)) + sub;

        // random micro-analog noise
        if (p.analog > 0.001f)
            sample += ((random01() - 0.5) * 0.002 * p.analog);

        // apply amplitude env and a bit of compression by saturating follow
        double out = sample * env;

        dst[i] = (float)out;
    }
}

void Generator808::applyFilterAndSaturation(juce::AudioBuffer<float>& bufMono, const GeneratorParams& p)
{
    using Filter = juce::dsp::IIR::Filter<float>;
    juce::dsp::IIR::Coefficients<float>::Ptr lowpassCoeffs =
        juce::dsp::IIR::Coefficients<float>::makeLowPass((float)p.sampleRate, 1400.0f, 0.7f);

    Filter lp;
    lp.coefficients = lowpassCoeffs;

    // process in place
    juce::dsp::AudioBlock<float> block(bufMono);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    lp.reset();
    lp.process(ctx);

    // mild EQ boosts for 'boomy' and 'punchy' simulated as simple shelf & band gain via hand-coded processing
    float* data = bufMono.getWritePointer(0);
    int ns = bufMono.getNumSamples();

    float lowShelfCenter = 60.0f;
    float lowShelfGain = 1.0f + p.boomAmount * 0.5f; // linear multiplier
    float clickBoostGain = 1.0f + p.punch * 0.5f;

    // crude low-shelf: multiply low freq content by lowShelfGain
    // simple approach: apply sample-by-sample running lowpass to isolate lows and amplify
    double a = 0.0;
    double rc = 1.0 / (2.0 * juce::MathConstants<double>::pi * lowShelfCenter);
    double dt = 1.0 / p.sampleRate;
    double alpha = dt / (rc + dt);
    float prevLow = 0.0f;
    for (int i = 0; i < ns; ++i)
    {
        float s = data[i];
        prevLow = (float)(prevLow + alpha * (s - prevLow));
        // Boost lows
        data[i] += (prevLow * (lowShelfGain - 1.0f));
    }

    // very light soft saturation to taste
    for (int i = 0; i < ns; ++i)
    {
        float x = data[i];
        // soft clip curve
        data[i] = (float)((x < -1.0f) ? -1.0f : (x > 1.0f ? 1.0f : std::tanh(x * (1.0 + p.analog * 0.5f))));
    }
}

void Generator808::applyStereoWidth(juce::AudioBuffer<float>& bufStereo, const GeneratorParams& p)
{
    // Keep sub below 120Hz mono. We will add a small stereo high-frequency component if detune > 0
    int ns = bufStereo.getNumSamples();
    auto left = bufStereo.getWritePointer(0);
    auto right = bufStereo.getWritePointer(1);

    if (p.detune < 0.001f)
    {
        // mono content already duplicated; nothing else
        return;
    }

    // create tiny phase-shifted copy for right channel to simulate detune/width
    double detuneCents = 5.0 + 15.0 * p.detune; // 5..20 cents
    double detuneFactor = std::pow(2.0, detuneCents / 1200.0);

    for (int i = 0; i < ns; ++i)
    {
        // crude approach: mix a delayed, slightly pitch-shifted version into right channel
        // We'll apply a simple small-sample delay modulation for illusion of detune (cheap chorus)
        double mod = 0.0005 * std::sin(2.0 * juce::MathConstants<double>::pi * 0.8 * i / (double)ns); // tiny LFO
        int delaySamples = (int)std::round(mod * p.sampleRate * p.detune * 20.0);
        int idx = juce::jlimit(0, ns - 1, i - delaySamples);
        right[i] = 0.6f * right[i] + 0.4f * left[idx];
    }
}
