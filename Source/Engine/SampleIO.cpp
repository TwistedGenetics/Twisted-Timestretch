#include "SampleData.h"
#include "Akaizer.h"

namespace SampleIO
{
    std::unique_ptr<SourceSample> load (const juce::File& file, juce::String& error)
    {
        if (! file.existsAsFile())
        {
            error = "File not found: " + file.getFullPathName();
            return nullptr;
        }

        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
        if (reader == nullptr)
        {
            error = "Can't read " + file.getFileName() + ". Use a WAV, AIFF or FLAC file.";
            return nullptr;
        }

        const auto chans = (int) reader->numChannels;
        const auto frames = (int64_t) reader->lengthInSamples;
        if (chans < 1 || frames < 1)
        {
            error = file.getFileName() + " has no audio in it.";
            return nullptr;
        }
        if (chans > 2)
        {
            error = file.getFileName() + " has " + juce::String (chans) + " channels. Use a mono or stereo file.";
            return nullptr;
        }
        if (frames > (int64_t) reader->sampleRate * 60 * 10)
        {
            error = file.getFileName() + " is longer than 10 minutes.";
            return nullptr;
        }

        juce::AudioBuffer<float> buf (chans, (int) frames);
        reader->read (&buf, 0, (int) frames, 0, true, chans > 1);

        auto s = std::make_unique<SourceSample>();
        s->channels = chans;
        s->sampleRate = reader->sampleRate;
        s->originalBits = (int) reader->bitsPerSample;
        s->originalWas16BitPcm = reader->bitsPerSample == 16 && ! reader->usesFloatingPointData;
        s->name = file.getFileNameWithoutExtension();
        s->path = file.getFullPathName();
        s->pcm.resize ((size_t) frames * (size_t) chans);

        // JUCE reads 16-bit as value / 32768, so this round trip is exact for 16-bit files.
        for (int c = 0; c < chans; ++c)
        {
            const float* src = buf.getReadPointer (c);
            for (int64_t i = 0; i < frames; ++i)
            {
                const double v = std::round ((double) src[i] * 32768.0);
                s->pcm[(size_t) (i * chans + c)] = (int16_t) juce::jlimit (-32768.0, 32767.0, v);
            }
        }
        return s;
    }

    bool saveWav (const RenderedSample& r, const juce::File& file)
    {
        const auto bytes = akaizer::makeWav (r.pcm, r.channels, (int) std::lround (r.sampleRate));
        file.getParentDirectory().createDirectory();
        return file.replaceWithData (bytes.data(), bytes.size());
    }
}
