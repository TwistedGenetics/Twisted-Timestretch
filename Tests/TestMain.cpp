/*
    TwistedTimestretchTests — headless checks, no DAW needed.

      * Engine vs. reference vectors rendered by the independent browser port
        of Akaizer (Tests/reference, made from Akaizer.html).
      * File load / save round trips.
      * The instrument: background renders, MIDI playback modes, key tracking,
        choke, preview, tempo sync, state save / restore.

    Also renders the editor to a PNG:  TwistedTimestretchTests --snapshot out.png
*/
#include <JuceHeader.h>
#include "Engine/Akaizer.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <iostream>

static int failures = 0, checks = 0;
static void check (bool ok, const juce::String& what)
{
    ++checks;
    if (! ok) ++failures;
    std::cout << (ok ? "  [PASS] " : "  [FAIL] ") << what << std::endl;
}

static void pump (int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil (ms); }

// Same integer-only generator as Tests/reference/genref.js
static std::vector<int16_t> makeInput (int words, uint32_t seed)
{
    std::vector<int16_t> x ((size_t) words);
    uint32_t s = seed;
    for (int i = 0; i < words; ++i)
    {
        s = s * 1103515245u + 12345u;
        const int noise = (int) ((s >> 16) & 0x3fff) - 0x2000;
        const int saw = (((i * 37) % 2000) - 1000) * 12;
        x[(size_t) i] = (int16_t) juce::jlimit (-32768, 32767, saw + noise);
    }
    return x;
}

static void engineTests()
{
    std::cout << "\n== Engine ==" << std::endl;
    using namespace akaizer;
    check (roundHalfEven (2.5) == 2 && roundHalfEven (3.5) == 4 && roundHalfEven (-2.5) == -2 && roundHalfEven (51.25) == 51,
           "FpI4 rounding: half to even");
    check (linkedFrequency (800) == 51 && linkedFrequency (300) == 137 && linkedFrequency (20) == 2050,
           "linked frequency = 41000 / cycle length (51 at the default 800)");

    const juce::File refDir (TT_REFERENCE_DIR);
    juce::StringArray cases;
    refDir.getChildFile ("cases.txt").readLines (cases);
    int casesRun = 0;
    for (auto& line : cases)
    {
        auto t = juce::StringArray::fromTokens (line, " ", "");
        if (t.size() < 9) continue;
        ++casesRun;
        const int ch = t[1].getIntValue(), words = t[3].getIntValue();
        const double sr = t[2].getDoubleValue(), pct = t[5].getDoubleValue();
        const auto in = makeInput (words, (uint32_t) t[4].getIntValue());
        const auto r = stretch (in.data(), in.size(), ch, sr, Settings { pct, t[6].getIntValue(), t[7].getIntValue() });

        juce::MemoryBlock refData;
        refDir.getChildFile (t[0] + ".raw").loadFileAsData (refData);
        const auto* ref = static_cast<const int16_t*> (refData.getData());
        const size_t nRef = refData.getSize() / 2;

        size_t diff = 0; int maxDiff = 0;
        for (size_t i = 0; i < juce::jmin (nRef, r.samples.size()); ++i)
        {
            const int d = std::abs ((int) ref[i] - (int) r.samples[i]);
            if (d != 0) { ++diff; maxDiff = juce::jmax (maxDiff, d); }
        }
        // sin() may differ by one ulp between maths libraries; that can flip a rounding.
        const bool ok = r.samples.size() == nRef && maxDiff <= 1 && diff * 1000 <= nRef;
        check (ok, t[0] + ": " + juce::String ((int) r.samples.size()) + " samples, "
                       + juce::String ((int) diff) + " differ from the browser port (max " + juce::String (maxDiff) + ")");
    }
    check (casesRun == 6, "all 6 reference cases ran");

    // Repitch and the full chain
    const auto in = makeInput (44100, 9);
    check (repitch (in.data(), in.size(), 1, 0.0) == in, "0 semitones leaves the sample untouched");
    check (std::abs ((int) repitch (in.data(), in.size(), 1, 12.0).size() - 22050) <= 1, "+12 semitones halves the length");
    const auto chain = render (in.data(), in.size(), 1, 44100.0, Settings { 100.0, 800, 51 }, 7.0);
    check (std::abs ((int) chain.samples.size() - 44100) <= 2, "pitch +7 then stretch 100% gives back the original length");
    const auto plain = render (in.data(), in.size(), 1, 44100.0, Settings { 130.0, 800, 51 }, 0.0);
    check (plain.samples == stretch (in.data(), in.size(), 1, 44100.0, Settings { 130.0, 800, 51 }).samples,
           "render() with no pitch is exactly the original algorithm");

    const auto wav = makeWav ({ 1, -1, 32767, -32768 }, 2, 44100);
    check (wav.size() == 52 && std::memcmp (wav.data(), "RIFF", 4) == 0 && std::memcmp (wav.data() + 36, "data", 4) == 0
               && wav[22] == 2 && wav[34] == 16 && wav[44] == 1 && wav[46] == 0xff && wav[47] == 0xff,
           "WAV writer: 44-byte header, 16-bit, little-endian samples");
}

static void ioTests()
{
    std::cout << "\n== File load / save ==" << std::endl;
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("tt-tests");
    dir.createDirectory();

    const auto pcm = makeInput (2 * 5000, 11);
    auto f16 = dir.getChildFile ("in16.wav");
    const auto bytes = akaizer::makeWav (pcm, 2, 44100);
    f16.replaceWithData (bytes.data(), bytes.size());
    juce::String err;
    auto s = SampleIO::load (f16, err);
    check (s != nullptr && s->pcm == pcm && s->channels == 2 && s->originalWas16BitPcm,
           "16-bit WAV loads bit-exact (stereo)");

    // 24-bit file through JUCE's writer
    auto f24 = dir.getChildFile ("in24.wav");
    f24.deleteFile();
    {
        juce::AudioBuffer<float> b (1, 1000);
        for (int i = 0; i < 1000; ++i) b.setSample (0, i, 0.5f * std::sin ((float) i * 0.05f));
        juce::WavAudioFormat wf;
        std::unique_ptr<juce::AudioFormatWriter> w (wf.createWriterFor (new juce::FileOutputStream (f24), 48000.0, 1, 24, {}, 0));
        w->writeFromAudioSampleBuffer (b, 0, 1000);
    }
    auto s24 = SampleIO::load (f24, err);
    check (s24 != nullptr && s24->originalBits == 24 && ! s24->originalWas16BitPcm && s24->pcm.size() == 1000
               && std::abs (s24->pcm[100] - (int) std::lround (0.5 * std::sin (5.0) * 32768.0)) <= 1,
           "24-bit WAV is converted to 16-bit");

    auto bad = dir.getChildFile ("notaudio.wav");
    bad.replaceWithText ("hello");
    err.clear();
    const bool rejected = SampleIO::load (bad, err) == nullptr;
    check (rejected && err.isNotEmpty(), "a broken file gives an error message: " + err);
}

struct FixedTempoPlayHead : public juce::AudioPlayHead
{
    double bpm = 170.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setIsPlaying (true);
        return p;
    }
};

static float blockRms (TimestretchProcessor& p, juce::MidiBuffer midi = {})
{
    juce::AudioBuffer<float> b (2, 512);
    p.processBlock (b, midi);
    return b.getRMSLevel (0, 0, 512);
}

static bool waitForRender (TimestretchProcessor& p, uint32_t after, int ms = 2000)
{
    for (int t = 0; t < ms; t += 10)
    {
        if (p.getRenderVersion() > after) return true;
        pump (10);
    }
    return false;
}

static std::unique_ptr<SourceSample> testSource (int frames, int channels, double rate)
{
    auto s = std::make_unique<SourceSample>();
    s->pcm = makeInput (frames * channels, 21);
    s->channels = channels;
    s->sampleRate = rate;
    s->name = "testloop";
    return s;
}

static void processorTests()
{
    std::cout << "\n== Instrument ==" << std::endl;
    TimestretchProcessor p;
    p.prepareToPlay (44100.0, 512);
    check (p.getParameters().size() == 13, "13 automatable parameters");
    check (p.getEffectiveFrequency() == 51, "default cycle frequency is linked: 51");

    p.setSource (testSource (44100, 1, 44100.0), true);
    check (std::abs (p.apvts.getRawParameterValue (ParamIDs::loopBpm)->load() - 120.0f) < 0.01f,
           "loop BPM guessed from the length (1 s = half a bar at 120 BPM)");
    check (p.renderNow() && p.getRendered() != nullptr, "renders as soon as a sample is set");
    check (p.getRendered()->pcm.size() == 44100, "100% keeps the length");

    // A parameter change re-renders in the background.
    auto v = p.getRenderVersion();
    auto* stretch = p.apvts.getParameter (ParamIDs::stretch);
    stretch->setValueNotifyingHost (stretch->convertTo0to1 (200.0f));
    check (waitForRender (p, v) && p.getRendered()->pcm.size() == 88200, "moving STRETCH to 200% re-renders in the background");

    // Bit-exact against the engine
    {
        auto src = p.getSource();
        auto direct = akaizer::stretch (src->pcm.data(), src->pcm.size(), 1, 44100.0, akaizer::Settings { 200.0, 800, 51 });
        check (direct.samples == p.getRendered()->pcm, "plugin render is exactly the Akaizer algorithm");
    }

    // Playback: one-shot ignores note off
    blockRms (p);
    juce::MidiBuffer on;  on.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 127), 0);
    juce::MidiBuffer off; off.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
    check (blockRms (p, on) > 0.01f, "MIDI note plays the akaized sample");
    blockRms (p, off);
    for (int i = 0; i < 4; ++i) blockRms (p);
    check (blockRms (p) > 0.01f && p.isAnyVoicePlaying(), "ONE-SHOT keeps playing after note off");

    // Gate
    auto* mode = p.apvts.getParameter (ParamIDs::playMode);
    mode->setValueNotifyingHost (mode->convertTo0to1 (1.0f));
    blockRms (p, on);
    blockRms (p, off);
    for (int i = 0; i < 3; ++i) blockRms (p);
    check (blockRms (p) < 1.0e-6f && ! p.isAnyVoicePlaying(), "GATE stops on note off");

    // Loop keeps going past the end
    mode->setValueNotifyingHost (mode->convertTo0to1 (2.0f));
    blockRms (p, on);
    for (int i = 0; i < 88200 / 512 + 20; ++i) blockRms (p);
    check (p.isAnyVoicePlaying(), "LOOP keeps playing past the end of the sample");
    blockRms (p, off);
    for (int i = 0; i < 4; ++i) blockRms (p);
    check (! p.isAnyVoicePlaying(), "LOOP stops on note off");

    // Key tracking: an octave up plays twice as fast
    mode->setValueNotifyingHost (mode->convertTo0to1 (0.0f));
    auto countBlocks = [&] (int note)
    {
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        blockRms (p, m);
        int n = 1;
        while (p.isAnyVoicePlaying() && n < 2000) { blockRms (p); ++n; }
        return n;
    };
    const int atRoot = countBlocks (60), octaveUp = countBlocks (72);
    check (std::abs (atRoot - 2 * octaveUp) <= 2, "KEY TRACK: C4 plays an octave up, twice as fast ("
                                                   + juce::String (atRoot) + " vs " + juce::String (octaveUp) + " blocks)");

    // Choke: a second note cuts the first
    {
        juce::MidiBuffer two;
        two.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
        two.addEvent (juce::MidiMessage::noteOn (1, 64, (juce::uint8) 100), 100);
        blockRms (p, two);
        for (int i = 0; i < 2; ++i) blockRms (p);
        juce::MidiBuffer none;
        juce::AudioBuffer<float> b (2, 512);
        p.processBlock (b, none);
        // Two notes in one block: with CHOKE on only one voice is left sounding.
        check (p.getActiveVoiceCount() == 1, "CHOKE: the second note cuts the first");
        juce::MidiBuffer allOff; allOff.addEvent (juce::MidiMessage::allNotesOff (1), 0);
        // one-shot ignores note off but all-notes-off releases
        blockRms (p, allOff);
        for (int i = 0; i < 3; ++i) blockRms (p);
        check (! p.isAnyVoicePlaying(), "All Notes Off silences everything");
    }

    // Preview
    p.startPreview();
    check (blockRms (p) > 0.01f, "PREVIEW plays without MIDI");
    p.stopPreview();
    for (int i = 0; i < 3; ++i) blockRms (p);
    check (! p.isAnyVoicePlaying(), "STOP ends the preview");

    // Tempo sync: 120 BPM loop at host 170 BPM -> 70.59%
    FixedTempoPlayHead ph;
    p.setPlayHead (&ph);
    auto* sync = p.apvts.getParameter (ParamIDs::tempoSync);
    v = p.getRenderVersion();
    sync->setValueNotifyingHost (1.0f);
    blockRms (p);
    const double expected = 120.0 / 170.0 * 100.0;
    check (std::abs (p.getEffectiveStretch() - expected) < 0.01, "SYNC: stretch follows host tempo ("
                                                                 + juce::String (p.getEffectiveStretch(), 2) + "%)");
    check (waitForRender (p, v) && std::abs ((int) p.getRendered()->pcm.size() - (int) std::lround (44100 * expected / 100.0)) <= 1,
           "SYNC: render length matches the host tempo");
    v = p.getRenderVersion();
    ph.bpm = 160.0;
    blockRms (p);
    check (waitForRender (p, v) && std::abs (p.getRendered()->stretchPercent - 75.0) < 0.01, "SYNC: re-renders when the host tempo changes");
    p.setPlayHead (nullptr);
    sync->setValueNotifyingHost (0.0f);

    // Save WAV
    auto out = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("tt-tests/out.wav");
    check (p.saveRenderedWav (out), "SAVE WAV writes a file");
    {
        juce::MemoryBlock mb; out.loadFileAsData (mb);
        const auto expect = akaizer::makeWav (p.getRendered()->pcm, 1, 44100);
        check (mb.getSize() == expect.size() && std::memcmp (mb.getData(), expect.data(), expect.size()) == 0,
               "saved WAV is the exact render");
    }
    check (p.writeDragFile().existsAsFile(), "drag-out file is written");

    // State round trip
    auto* pitch = p.apvts.getParameter (ParamIDs::pitch);
    pitch->setValueNotifyingHost (pitch->convertTo0to1 (5.0f));
    auto* cyc = p.apvts.getParameter (ParamIDs::cycleLen);
    cyc->setValueNotifyingHost (cyc->convertTo0to1 (333.0f));
    p.renderNow();
    juce::MemoryBlock state;
    p.getStateInformation (state);

    TimestretchProcessor q;
    q.prepareToPlay (48000.0, 512);
    q.setStateInformation (state.getData(), (int) state.getSize());
    check (q.getSource() != nullptr && q.getSource()->pcm == p.getSource()->pcm, "state restores the sample (embedded)");
    check ((int) q.apvts.getRawParameterValue (ParamIDs::pitch)->load() == 5
               && (int) q.apvts.getRawParameterValue (ParamIDs::cycleLen)->load() == 333,
           "state restores the settings");
    check (q.getRendered() != nullptr && q.getRendered()->pcm == p.getRendered()->pcm, "restored instance renders the same audio");
    check (blockRms (q, on) > 0.01f, "restored instance plays at a different sample rate");
}

static int snapshot (const juce::String& path)
{
    TimestretchProcessor p;
    p.prepareToPlay (44100.0, 512);
    auto s = testSource (88200, 2, 44100.0);
    s->name = "think_break_136";
    p.setSource (std::move (s), true);
    p.renderNow();
    p.setStatus ("Loaded think_break_136.wav");
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (TimestretchEditor::designW, TimestretchEditor::designH);
    pump (200);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    juce::File f (juce::File::getCurrentWorkingDirectory().getChildFile (path));
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    const bool ok = png.writeImageToStream (img, os);
    std::cout << (ok ? "wrote " : "could not write ") << f.getFullPathName() << std::endl;
    return ok ? 0 : 1;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc >= 3 && juce::String (argv[1]) == "--snapshot")
        return snapshot (argv[2]);

    engineTests();
    ioTests();
    processorTests();

    std::cout << "\n" << (checks - failures) << " / " << checks << " checks passed" << std::endl;
    return failures == 0 ? 0 : 1;
}
