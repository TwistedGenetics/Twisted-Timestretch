#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <vector>

/** A loaded source sample, held the way Akaizer works on it: interleaved 16-bit. */
struct SourceSample
{
    std::vector<int16_t> pcm;      // interleaved
    int channels = 0;
    double sampleRate = 0.0;
    int originalBits = 16;
    bool originalWas16BitPcm = true;
    juce::String name;             // file name without extension
    juce::String path;             // full path it was loaded from (may be missing later)

    size_t numFrames() const noexcept { return channels > 0 ? pcm.size() / (size_t) channels : 0; }
    double lengthSeconds() const noexcept { return sampleRate > 0.0 ? (double) numFrames() / sampleRate : 0.0; }
};

/** One finished Akaizer render, shared between the renderer, the audio thread and the UI.
    Never deleted on the audio thread: the renderer keeps every live one in a pool and
    frees it only once nothing else refers to it. */
struct RenderedSample : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RenderedSample>;

    juce::AudioBuffer<float> audio;   // de-interleaved, for playback
    std::vector<int16_t> pcm;         // interleaved 16-bit, exactly what gets saved
    int channels = 0;
    double sampleRate = 0.0;
    juce::String name;                // e.g. "amen_akaized"

    // What produced it (for the display)
    double stretchPercent = 100.0;    // effective, after tempo sync
    int cycleLength = 0;
    int cycleFrequency = 0;
    double semitones = 0.0;
    long long grainWords = 0, outHopWords = 0, inHopWords = 0;
    uint32_t version = 0;
};

namespace SampleIO
{
    /** Decodes any format JUCE knows (WAV, AIFF, FLAC, Ogg, MP3 on macOS) into 16-bit. */
    std::unique_ptr<SourceSample> load (const juce::File& file, juce::String& error);

    /** Writes the 16-bit WAV bytes exactly as Akaizer did. */
    bool saveWav (const RenderedSample& r, const juce::File& file);
}
