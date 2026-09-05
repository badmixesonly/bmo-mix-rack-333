#include "DspCore.h"
#include "DriveTables.h"
#include <algorithm>

namespace bmo::sat
{

namespace
{
    float dbToGain (float db) noexcept { return std::pow (10.0f, db * 0.05f); }
}

//==============================================================================
float DspCore::driveFor (float amountPercent) noexcept
{
    const auto t = std::clamp (amountPercent, 0.0f, 100.0f) * 0.01f;

    return tables::kDriveMin * std::pow (tables::kDriveMax / tables::kDriveMin, t);
}

float DspCore::makeupGainDb (float amountPercent) noexcept
{
    return tables::makeupDb (amountPercent);
}

//==============================================================================
void DspCore::Channel::prepare (double rate, const Character& c) noexcept
{
    character = &c;

    for (auto& pole : bodyInput)
        pole.setCutoff (c.bodySourceHz, rate);

    bodySplit.setCutoff (c.bodySourceHz, rate);

    for (auto& pole : sheenInput)
        pole.setCutoff (c.sheenSourceHz, rate);

    sheenSplit.setCutoff (c.residualSplitHz, rate);
    sheenTilt.set (c.sheenHz, c.sheenTilt, rate);

    highPass.set (c.highPassHz, rate);
    setTone (100.0f, rate);

    dc.prepare (rate);
    reset();
}

void DspCore::Channel::reset() noexcept
{
    shaper.reset();
    bodyShaper.reset();
    sheenShaper.reset();
    bell.reset();
    highPass.reset();

    for (auto& pole : bodyInput)
        pole.reset();

    for (auto& pole : sheenInput)
        pole.reset();

    bodySplit.reset();
    sheenSplit.reset();
    sheenTilt.reset();
    dc.reset();
    oversampler.reset();
}

void DspCore::Channel::setTone (float amountPercent, double rate) noexcept
{
    // The amount scales the fitted shape in decibels, so half of TONE is half
    // the voicing rather than half the gain -- the shape stays the shape.
    const auto t = std::clamp (amountPercent, 0.0f, 100.0f) * 0.01f;
    const auto gain = std::pow (10.0f, character->bellGainDb * t * 0.05f);

    bell.set (character->bellHz, character->bellQ, gain, rate);

    // The high-pass is blended rather than switched, so that TONE at zero is
    // the signal untouched rather than the signal with a filter still on it.
    // Anything the voicing does has to leave when the voicing leaves.
    toneAmount = t;
}

void DspCore::Channel::setDrive (float drive) noexcept
{
    shaper.setDrive (drive);
    bodyShaper.setDrive (drive);
    sheenShaper.setDrive (drive);
}

float DspCore::Channel::process (float x) noexcept
{
    // The fitted curve, exactly: x + (shape(x) - x), with the residual
    // anti-aliased. Everything the calibration in Shaper.h says about the
    // positive and negative average gains is a statement about this line.
    auto y = x + shaper.processResidual (x);

    // One generator per band that needs filling. Each is fed the signal below
    // its corner, and only what it makes above that corner is kept.
    y += character->bodyGain  * generate (bodyShaper,  bodyInput,  bodySplit,  x);
    y += character->sheenGain * sheenTilt.process (generate (sheenShaper, sheenInput, sheenSplit, x));

    // The voicing goes after the saturation: the reference's bell sits on the
    // finished sound, and putting it before would drive the curve with a shape
    // the reference never fed it.
    const auto filtered = y + toneAmount * (highPass.process (y) - y);
    return dc.process (bell.process (filtered));
}

float DspCore::Channel::generate (AsymmetricShaper& generator, OnePole (&input)[2],
                                  OnePole& split, float x) noexcept
{
    auto source = x;

    for (auto& pole : input)
        source = pole.process (OnePole::Output::lowpass, source);

    float below = 0.0f, above = 0.0f;
    split.process (generator.processResidual (source), below, above);
    return above;
}

//==============================================================================
void DspCore::prepare (double newSampleRate, int maxBlockSize, int numChannels,
                       int oversampleFactor)
{
    sampleRate  = newSampleRate > 0.0 ? newSampleRate : 44100.0;
    maxBlock    = std::max (maxBlockSize, 1);
    maxChannels = std::clamp (numChannels, 1, (int) channels.size());

    // Sized for the highest factor, so a change of oversampling at run time
    // never has to allocate on the audio thread.
    dryDelay.assign ((size_t) ((Oversampler::kMaxLatency + 2) * (int) channels.size()), 0.0f);
    dryStride = Oversampler::kMaxLatency + 2;

    const auto controlRate = sampleRate / (double) kSubBlock;

    for (auto* s : { &inputGainSm, &outputLevelSm, &mixSm, &makeupSm, &toneSm })
        s->prepare (controlRate, 20.0);

    // Drive is smoothed more slowly than a gain. It moves the shape of the
    // curve rather than a level, and a curve that changes shape quickly under
    // an automation ramp is audible as a flutter on sustained material.
    driveSm.prepare (controlRate, 40.0);

    // Auto Gain's detector. 1.5 seconds: slower than any phrase, so it cannot
    // act on the programme's dynamics.
    autoGainCoeff = (float) (1.0 - std::exp (-1.0 / (controlRate * 1.5)));
    inputEnergy = processedEnergy = 0.0;

    applyOversampling (oversampleFactor);

    primed = false;
    reset();
}

void DspCore::applyOversampling (int factor)
{
    factor = factor >= 8 ? 8 : factor >= 4 ? 4 : factor >= 2 ? 2 : 1;

    for (auto& c : channels)
        c.oversampler.setFactor (factor);

    currentFactor  = factor;
    latencySamples = Oversampler::latencyForFactor (factor);
    effectiveRate  = sampleRate * (double) factor;
    dryLength      = latencySamples + 1;

    // None of this allocates; it only recomputes coefficients, so it is safe
    // to call from the audio thread when the factor changes.
    for (auto& c : channels)
        c.prepare (effectiveRate, character);
}

void DspCore::reset() noexcept
{
    for (auto& c : channels)
        c.reset();

    std::fill (dryDelay.begin(), dryDelay.end(), 0.0f);
    dryWrite = 0;
}

//==============================================================================
void DspCore::setParams (const Params& p) noexcept
{
    params = p;

    const auto drive = driveFor (p.driveAmount);

    inputGainSm  .setTarget (dbToGain (p.inputGainDb));
    outputLevelSm.setTarget (dbToGain (p.outputLevelDb));
    mixSm        .setTarget (std::clamp (p.mixPercent, 0.0f, 100.0f) * 0.01f);
    driveSm      .setTarget (drive);
    toneSm       .setTarget (std::clamp (p.toneAmount, 0.0f, 100.0f));

    if (! p.autoGain)
        makeupSm.setTarget (1.0f);

    if (primed)
        return;

    toneSm.snap (std::clamp (p.toneAmount, 0.0f, 100.0f));
    makeupSm.snap (1.0f);

    inputGainSm  .snap (dbToGain (p.inputGainDb));
    outputLevelSm.snap (dbToGain (p.outputLevelDb));
    mixSm        .snap (std::clamp (p.mixPercent, 0.0f, 100.0f) * 0.01f);
    driveSm      .snap (drive);
    primed = true;
}

//==============================================================================
void DspCore::process (float* const* channelData, int numChannels, int numSamples) noexcept
{
    const auto activeChannels = std::clamp (numChannels, 0, maxChannels);

    if (activeChannels == 0 || numSamples <= 0)
        return;

    if (params.oversampling != currentFactor)
        applyOversampling (params.oversampling);

    const auto polarity = params.phaseInvert ? -1.0f : 1.0f;
    const auto factor   = currentFactor;

    for (int start = 0; start < numSamples; start += kSubBlock)
    {
        const auto n = std::min (kSubBlock, numSamples - start);

        const auto inGain   = inputGainSm.tick();
        const auto outGain  = outputLevelSm.tick();
        const auto makeup   = makeupSm.tick();
        const auto wet      = mixSm.tick();
        const auto dryLevel = 1.0f - wet;
        const auto drive    = driveSm.tick();
        const auto tone     = toneSm.tick();

        for (int ch = 0; ch < activeChannels; ++ch)
        {
            channels[(size_t) ch].setDrive (drive);
            channels[(size_t) ch].setTone (tone, effectiveRate);
        }

        double blockInput = 0.0, blockProcessed = 0.0;

        // Samples outermost so the shared dry-delay cursor advances once per
        // frame rather than once per channel.
        for (int i = 0; i < n; ++i)
        {
            const auto readIndex = (dryWrite + 1) % dryLength;

            for (int ch = 0; ch < activeChannels; ++ch)
            {
                auto& channel = channels[(size_t) ch];
                auto* data = channelData[ch] + start;
                auto* dry  = dryDelay.data() + (size_t) ch * (size_t) dryStride;

                const auto input = data[i];

                dry[(size_t) dryWrite] = input;
                const auto delayed = dry[(size_t) readIndex];

                const auto driven = input * inGain;

                float buffer[Oversampler::kMaxFactor] {};
                channel.oversampler.upsample (driven, buffer);

                for (int j = 0; j < factor; ++j)
                    buffer[j] = params.saturationIn ? channel.process (buffer[j]) : buffer[j];

                const auto shaped = channel.oversampler.downsample (buffer);

                // Measured before the makeup is applied, so the detector reads
                // what the stage did rather than what it and its own
                // compensation did together -- a loop that would take a while
                // to settle and could be made to oscillate.
                blockInput     += (double) driven * driven;
                blockProcessed += (double) shaped * shaped;

                // Polarity is the last thing that happens to the signal, and
                // Output the last thing after that. Both apply to the blend
                // rather than to the wet path alone, which is what makes the
                // button mean "flip what leaves the plugin" in every state.
                //
                // It used to be applied to the input instead, and that was
                // wrong twice over. The dry path of the Mix control never saw
                // it, so at Mix 0 the button did nothing at all. And because
                // the curve is asymmetric, -f(-x) is not f(x): flipping ahead
                // of the shaper changed which harmonics came out, so engaging
                // a polarity switch altered the sound. A polarity control has
                // one job and it is not that.
                const auto blended = shaped * makeup * wet + delayed * dryLevel;

                data[i] = blended * polarity * outGain;
            }

            dryWrite = (dryWrite + 1) % dryLength;
        }

        updateAutoGain (blockInput, blockProcessed, n * activeChannels);
    }
}

//==============================================================================
void DspCore::updateAutoGain (double blockInput, double blockProcessed, int samples) noexcept
{
    if (samples <= 0)
        return;

    const auto in  = blockInput  / (double) samples;
    const auto out = blockProcessed / (double) samples;

    // Silence carries no information about the gain and would drag both
    // averages towards zero, so the detector holds its reading through it.
    constexpr double kFloor = 1.0e-9;          // about -90 dBFS

    if (in > kFloor && out > kFloor)
    {
        inputEnergy     += (double) autoGainCoeff * (in  - inputEnergy);
        processedEnergy += (double) autoGainCoeff * (out - processedEnergy);
    }

    if (! params.autoGain || inputEnergy <= kFloor || processedEnergy <= kFloor)
        return;

    // Clamped, because a level match is a convenience and should never be
    // capable of a surprise: at most 12 dB either way.
    const auto wanted = std::sqrt (inputEnergy / processedEnergy);
    makeupSm.setTarget ((float) std::clamp (wanted, 0.25, 4.0));
}

} // namespace bmo::sat
