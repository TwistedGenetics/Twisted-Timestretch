#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/Akaizer.h"

namespace
{
    juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }

    // 4-point Hermite read from one channel; wraps around when looping.
    inline float readHermite (const float* d, int len, double pos, bool wrap) noexcept
    {
        const auto i0 = (int) pos;
        const auto t = (float) (pos - (double) i0);
        auto at = [&] (int i) -> float
        {
            if (wrap) { i %= len; if (i < 0) i += len; return d[i]; }
            return (i < 0 || i >= len) ? 0.0f : d[i];
        };
        const float xm1 = at (i0 - 1), x0 = at (i0), x1 = at (i0 + 1), x2 = at (i0 + 2);
        const float c1 = 0.5f * (x1 - xm1);
        const float c2 = xm1 - 2.5f * x0 + 2.0f * x1 - 0.5f * x2;
        const float c3 = 0.5f * (x2 - xm1) + 1.5f * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout TimestretchProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    NormalisableRange<float> stretchRange (25.0f, 2000.0f, 0.01f);
    stretchRange.setSkewForCentre (150.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::stretch, 1 }, "Stretch Length", stretchRange, 100.0f,
        AudioParameterFloatAttributes().withLabel ("%")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + "%"; })
            .withValueFromStringFunction ([] (const String& s) { return s.retainCharacters ("0123456789.").getFloatValue(); })));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::cycleLen, 1 }, "Cycle Length", 20, 2000, 800,
        AudioParameterIntAttributes().withLabel ("samples")));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::cycleFreq, 1 }, "Cycle Frequency", 20, 2050, 51,
        AudioParameterIntAttributes().withLabel ("cps")));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::freqManual, 1 }, "Manual Frequency", false));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::pitch, 1 }, "Pitch", -24, 24, 0,
        AudioParameterIntAttributes().withLabel ("st")
            .withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v) + " st"; })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::tempoSync, 1 }, "Tempo Sync", false));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::loopBpm, 1 }, "Loop BPM",
        NormalisableRange<float> (40.0f, 300.0f, 0.01f), 136.0f,
        AudioParameterFloatAttributes().withLabel ("BPM")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::playMode, 1 }, "Play Mode",
        StringArray { "One-shot", "Gate", "Loop" }, 0));

    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { ParamIDs::rootNote, 1 }, "Root Note", 0, 127, 60,
        AudioParameterIntAttributes()
            .withStringFromValueFunction ([] (int v, int) { return noteName (v); })
            .withValueFromStringFunction ([] (const String& s)
            {
                if (s.containsOnly ("0123456789")) return s.getIntValue();
                for (int n = 0; n < 128; ++n) if (noteName (n).equalsIgnoreCase (s.trim())) return n;
                return 60;
            })));

    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::keyTrack, 1 }, "Key Tracking", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::choke, 1 }, "Mono Choke", true));
    p.push_back (std::make_unique<AudioParameterBool> (ParameterID { ParamIDs::velocity, 1 }, "Velocity", true));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { ParamIDs::gain, 1 }, "Gain",
        NormalisableRange<float> (-36.0f, 12.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")
            .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

    return { p.begin(), p.end() };
}

//==============================================================================
TimestretchProcessor::TimestretchProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pStretch    = apvts.getRawParameterValue (ParamIDs::stretch);
    pCycleLen   = apvts.getRawParameterValue (ParamIDs::cycleLen);
    pCycleFreq  = apvts.getRawParameterValue (ParamIDs::cycleFreq);
    pFreqManual = apvts.getRawParameterValue (ParamIDs::freqManual);
    pPitch      = apvts.getRawParameterValue (ParamIDs::pitch);
    pTempoSync  = apvts.getRawParameterValue (ParamIDs::tempoSync);
    pLoopBpm    = apvts.getRawParameterValue (ParamIDs::loopBpm);
    pPlayMode   = apvts.getRawParameterValue (ParamIDs::playMode);
    pRootNote   = apvts.getRawParameterValue (ParamIDs::rootNote);
    pKeyTrack   = apvts.getRawParameterValue (ParamIDs::keyTrack);
    pChoke      = apvts.getRawParameterValue (ParamIDs::choke);
    pVelocity   = apvts.getRawParameterValue (ParamIDs::velocity);
    pGain       = apvts.getRawParameterValue (ParamIDs::gain);

    for (auto* id : { ParamIDs::stretch, ParamIDs::cycleLen, ParamIDs::cycleFreq, ParamIDs::freqManual,
                      ParamIDs::pitch, ParamIDs::tempoSync, ParamIDs::loopBpm })
        apvts.addParameterListener (id, this);

    renderThread.startThread (juce::Thread::Priority::low);
}

TimestretchProcessor::~TimestretchProcessor()
{
    renderThread.stopThread (2000);
    for (auto* id : { ParamIDs::stretch, ParamIDs::cycleLen, ParamIDs::cycleFreq, ParamIDs::freqManual,
                      ParamIDs::pitch, ParamIDs::tempoSync, ParamIDs::loopBpm })
        apvts.removeParameterListener (id, this);
}

bool TimestretchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet().isDisabled();
}

void TimestretchProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    gainSmoothed.reset (currentSampleRate, 0.02);
    gainSmoothed.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    for (auto& v : voices) { v.active = false; v.smp = nullptr; }
    voicesPlaying.store (0);
}

//==============================================================================
void TimestretchProcessor::parameterChanged (const juce::String&, float) { markDirty(); }

void TimestretchProcessor::markDirty() noexcept
{
    dirtySince.store (juce::Time::getMillisecondCounterHiRes());
    dirty.store (true);
}

void TimestretchProcessor::RenderThread::run()
{
    while (! threadShouldExit())
    {
        wait (20);
        if (threadShouldExit()) break;

        // Follow the host's tempo while sync is on.
        if (owner.isTempoSyncActive() && std::abs (owner.hostBpm.load() - owner.renderedSyncBpm.load()) > 0.001)
        {
            owner.dirtySince.store (0.0);
            owner.dirty.store (true);
        }

        // Short debounce so dragging a slider doesn't queue up renders.
        if (owner.dirty.load() && juce::Time::getMillisecondCounterHiRes() - owner.dirtySince.load() > 35.0)
        {
            owner.dirty.store (false);
            owner.renderNow();
        }
    }
}

bool TimestretchProcessor::isTempoSyncActive() const
{
    return pTempoSync->load() > 0.5f && hostBpm.load() > 0.0 && pLoopBpm->load() > 0.0f;
}

double TimestretchProcessor::getEffectiveStretch() const
{
    if (isTempoSyncActive())
        return juce::jlimit (25.0, 2000.0, (double) pLoopBpm->load() / hostBpm.load() * 100.0);
    return (double) pStretch->load();
}

int TimestretchProcessor::getEffectiveFrequency() const
{
    if (pFreqManual->load() > 0.5f)
        return (int) std::lround (pCycleFreq->load());
    return akaizer::linkedFrequency ((int) std::lround (pCycleLen->load()));
}

double TimestretchProcessor::guessLoopBpm (double seconds)
{
    if (seconds <= 0.2) return 0.0;
    double best = 0.0, bestDist = 1.0e9;
    for (double bars : { 0.5, 1.0, 2.0, 4.0, 8.0, 16.0 })
    {
        const double bpm = 240.0 * bars / seconds;
        if (bpm < 80.0 || bpm > 180.0) continue;
        const double dist = std::abs (bpm - 140.0);
        if (dist < bestDist) { best = bpm; bestDist = dist; }
    }
    return best;
}

//==============================================================================
std::shared_ptr<const SourceSample> TimestretchProcessor::getSource() const
{
    const juce::ScopedLock l (sourceLock);
    return source;
}

void TimestretchProcessor::setSource (std::unique_ptr<SourceSample> s, bool guessTempo)
{
    double seconds = s != nullptr ? s->lengthSeconds() : 0.0;
    {
        const juce::ScopedLock l (sourceLock);
        source = std::shared_ptr<const SourceSample> (s.release());
    }
    ++sourceVersion;

    if (guessTempo)
        if (auto bpm = guessLoopBpm (seconds); bpm > 0.0)
            if (auto* prm = apvts.getParameter (ParamIDs::loopBpm))
                prm->setValueNotifyingHost (prm->convertTo0to1 ((float) bpm));
}

bool TimestretchProcessor::loadFile (const juce::File& file)
{
    juce::String err;
    auto s = SampleIO::load (file, err);
    if (s == nullptr) { setStatus (err); return false; }

    juce::String msg = "Loaded " + file.getFileName();
    if (! s->originalWas16BitPcm)
        msg << " (converted from " << s->originalBits << "-bit to 16-bit, as Akaizer works in 16-bit)";
    setSource (std::move (s), true);
    renderNow();
    setStatus (msg);
    return true;
}

RenderedSample::Ptr TimestretchProcessor::getRendered() const
{
    const juce::SpinLock::ScopedLockType l (currentLock);
    return current;
}

bool TimestretchProcessor::renderNow()
{
    const juce::ScopedLock rl (renderLock);
    auto src = getSource();
    if (src == nullptr || src->pcm.empty())
        return false;

    akaizer::Settings s;
    s.stretchPercent = getEffectiveStretch();
    s.cycleLength    = (int) std::lround (pCycleLen->load());
    s.cycleFrequency = getEffectiveFrequency();
    const double semis = std::round (pPitch->load());
    const double syncBpm = isTempoSyncActive() ? hostBpm.load() : 0.0;

    auto res = akaizer::render (src->pcm.data(), src->pcm.size(), src->channels, src->sampleRate, s, semis);

    RenderedSample::Ptr r (new RenderedSample());
    r->channels = src->channels;
    r->sampleRate = src->sampleRate;
    r->name = src->name + "_akaized";
    r->stretchPercent = s.stretchPercent;
    r->cycleLength = s.cycleLength;
    r->cycleFrequency = s.cycleFrequency;
    r->semitones = semis;
    r->grainWords = res.grainWords;
    r->outHopWords = res.outHopWords;
    r->inHopWords = res.inHopWords;
    r->pcm = std::move (res.samples);

    const auto frames = (int) (r->pcm.size() / (size_t) r->channels);
    r->audio.setSize (r->channels, juce::jmax (1, frames));
    r->audio.clear();
    for (int c = 0; c < r->channels; ++c)
    {
        auto* d = r->audio.getWritePointer (c);
        for (int i = 0; i < frames; ++i)
            d[i] = (float) r->pcm[(size_t) i * (size_t) r->channels + (size_t) c] / 32768.0f;
    }
    r->version = renderVersion.load() + 1;

    pool.add (r);
    {
        const juce::SpinLock::ScopedLockType l (currentLock);
        current = r;
    }
    renderedSyncBpm.store (syncBpm);

    // Free renders nothing plays any more (only the pool still refers to them).
    for (int i = pool.size(); --i >= 0;)
        if (auto* o = pool.getObjectPointerUnchecked (i); o != r.get() && o->getReferenceCount() == 1)
            pool.remove (i);

    ++renderVersion;
    return true;
}

bool TimestretchProcessor::saveRenderedWav (const juce::File& file) const
{
    auto r = getRendered();
    return r != nullptr && SampleIO::saveWav (*r, file);
}

juce::File TimestretchProcessor::writeDragFile() const
{
    auto r = getRendered();
    if (r == nullptr) return {};
    auto f = juce::File::getSpecialLocation (juce::File::tempDirectory)
                 .getChildFile ("TwistedTimestretch")
                 .getChildFile (juce::File::createLegalFileName (r->name) + ".wav");
    return SampleIO::saveWav (*r, f) ? f : juce::File();
}

juce::String TimestretchProcessor::getStatus() const { const juce::ScopedLock l (statusLock); return status; }
void TimestretchProcessor::setStatus (const juce::String& s) { const juce::ScopedLock l (statusLock); status = s; }

//==============================================================================
void TimestretchProcessor::handleNoteOn (int note, float vel, bool isPreview)
{
    if (audioCurrent == nullptr || audioCurrent->audio.getNumSamples() < 2)
        return;

    const auto mode = (PlayMode) juce::jlimit (0, 2, (int) pPlayMode->load());
    const int root = (int) pRootNote->load();

    if (pChoke->load() > 0.5f || isPreview)
        for (auto& v : voices)
            if (v.active && ! v.releasing)
            {
                v.releasing = true;
                v.releaseStep = 1.0f / (float) (0.004 * currentSampleRate);
            }

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }
    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.startedAt < target->startedAt) target = &v;
    }

    auto& v = *target;
    v.smp = audioCurrent;
    v.pos = 0.0;
    double ratio = v.smp->sampleRate / currentSampleRate;
    if (! isPreview && pKeyTrack->load() > 0.5f)
        ratio *= std::pow (2.0, (double) (note - root) / 12.0);
    v.inc = ratio;
    v.level = (pVelocity->load() > 0.5f && ! isPreview) ? vel : 1.0f;
    v.env = 0.0f;
    v.attackStep = 1.0f / (float) (0.0005 * currentSampleRate);
    v.releaseStep = 1.0f / (float) (0.008 * currentSampleRate);
    v.note = isPreview ? -1 : note;
    v.preview = isPreview;
    v.looping = mode == PlayMode::loop;
    v.oneShot = mode == PlayMode::oneShot;
    v.active = true;
    v.releasing = false;
    v.startedAt = ++noteCounter;
}

void TimestretchProcessor::handleNoteOff (int note, bool isPreview)
{
    for (auto& v : voices)
    {
        if (! v.active || v.releasing) continue;
        const bool match = isPreview ? v.preview : (! v.preview && v.note == note && ! v.oneShot);
        if (match) v.releasing = true;
    }
}

void TimestretchProcessor::renderVoices (juce::AudioBuffer<float>& out, int start, int num)
{
    if (num <= 0) return;
    const int outCh = out.getNumChannels();

    for (auto& v : voices)
    {
        if (! v.active) continue;
        const auto& a = v.smp->audio;
        const int len = a.getNumSamples();
        const int sc = a.getNumChannels();
        const float* d0 = a.getReadPointer (0);
        const float* d1 = a.getReadPointer (sc > 1 ? 1 : 0);

        for (int i = 0; i < num; ++i)
        {
            if (v.releasing)
            {
                v.env -= v.releaseStep;
                if (v.env <= 0.0f) { v.active = false; break; }
            }
            else if (v.env < 1.0f)
            {
                v.env = juce::jmin (1.0f, v.env + v.attackStep);
            }

            const float l = readHermite (d0, len, v.pos, v.looping);
            const float r = sc > 1 ? readHermite (d1, len, v.pos, v.looping) : l;
            const float g = v.level * v.env;
            if (outCh >= 2)
            {
                out.addSample (0, start + i, l * g);
                out.addSample (1, start + i, r * g);
            }
            else if (outCh == 1)
            {
                out.addSample (0, start + i, 0.5f * (l + r) * g);
            }

            v.pos += v.inc;
            if (v.pos >= (double) len)
            {
                if (v.looping) v.pos = std::fmod (v.pos, (double) len);
                else { v.active = false; break; }
            }
        }
        if (! v.active) v.smp = nullptr; // the pool still holds it: never freed here
    }
}

void TimestretchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm(); bpm.hasValue() && *bpm > 0.0)
                hostBpm.store (*bpm);

    {
        const juce::SpinLock::ScopedTryLockType l (currentLock);
        if (l.isLocked() && audioCurrent != current)
            audioCurrent = current;
    }

    switch (previewRequest.exchange (0))
    {
        case 1: handleNoteOn ((int) pRootNote->load(), 1.0f, true); break;
        case 2: handleNoteOff (-1, true); break;
        default: break;
    }

    int cursor = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const int pos = juce::jlimit (0, buffer.getNumSamples(), meta.samplePosition);
        renderVoices (buffer, cursor, pos - cursor);
        cursor = pos;

        if (m.isNoteOn())
            handleNoteOn (m.getNoteNumber(), m.getFloatVelocity(), false);
        else if (m.isNoteOff())
            handleNoteOff (m.getNoteNumber(), false);
        else if (m.isAllNotesOff() || m.isAllSoundOff())
            for (auto& v : voices) if (v.active) v.releasing = true;
    }
    renderVoices (buffer, cursor, buffer.getNumSamples() - cursor);
    midi.clear();

    gainSmoothed.setTargetValue (juce::Decibels::decibelsToGain (pGain->load()));
    if (gainSmoothed.isSmoothing())
    {
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float g = gainSmoothed.getNextValue();
            for (int c = 0; c < buffer.getNumChannels(); ++c)
                buffer.setSample (c, i, buffer.getSample (c, i) * g);
        }
    }
    else
    {
        buffer.applyGain (gainSmoothed.getTargetValue());
    }

    int playing = 0;
    const Voice* newest = nullptr;
    for (auto& v : voices)
        if (v.active)
        {
            ++playing;
            if (newest == nullptr || v.startedAt > newest->startedAt) newest = &v;
        }
    voicesPlaying.store (playing);
    playhead.store (newest != nullptr ? (float) (newest->pos / (double) newest->smp->audio.getNumSamples()) : -1.0f);
}

//==============================================================================
void TimestretchProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree root ("TwistedTimestretch");
    root.setProperty ("formatVersion", 1, nullptr);
    root.appendChild (apvts.copyState(), nullptr);

    if (auto src = getSource())
    {
        juce::ValueTree s ("Sample");
        s.setProperty ("name", src->name, nullptr);
        s.setProperty ("path", src->path, nullptr);
        s.setProperty ("channels", src->channels, nullptr);
        s.setProperty ("rate", src->sampleRate, nullptr);
        s.setProperty ("bits", src->originalBits, nullptr);
        s.setProperty ("was16", src->originalWas16BitPcm, nullptr);
        const auto bytes = src->pcm.size() * sizeof (int16_t);
        if (bytes <= (size_t) 64 * 1024 * 1024) // embed so projects open anywhere
            s.setProperty ("pcm", juce::var (juce::MemoryBlock (src->pcm.data(), bytes)), nullptr);
        root.appendChild (s, nullptr);
    }

    juce::MemoryOutputStream mos (destData, false);
    root.writeToStream (mos);
}

void TimestretchProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto root = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (! root.isValid() || ! root.hasType ("TwistedTimestretch"))
        return;

    auto params = root.getChildWithName (apvts.state.getType());
    if (params.isValid())
        apvts.replaceState (params);

    auto s = root.getChildWithName ("Sample");
    if (! s.isValid())
        return;

    std::unique_ptr<SourceSample> src;
    if (auto* mb = s.getProperty ("pcm").getBinaryData(); mb != nullptr && mb->getSize() >= 2)
    {
        src = std::make_unique<SourceSample>();
        src->channels = juce::jlimit (1, 2, (int) s.getProperty ("channels", 1));
        src->sampleRate = (double) s.getProperty ("rate", 44100.0);
        src->originalBits = (int) s.getProperty ("bits", 16);
        src->originalWas16BitPcm = (bool) s.getProperty ("was16", true);
        src->name = s.getProperty ("name").toString();
        src->path = s.getProperty ("path").toString();
        src->pcm.resize (mb->getSize() / sizeof (int16_t));
        std::memcpy (src->pcm.data(), mb->getData(), src->pcm.size() * sizeof (int16_t));
    }
    else
    {
        const juce::File f (s.getProperty ("path").toString());
        juce::String err;
        src = SampleIO::load (f, err);
        if (src == nullptr)
        {
            setStatus ("Missing sample: " + f.getFullPathName() + ". Load it again.");
            return;
        }
    }

    setSource (std::move (src), false);
    renderNow();
    setStatus ("Restored " + getSource()->name + ".");
}

//==============================================================================
juce::AudioProcessorEditor* TimestretchProcessor::createEditor()
{
    return new TimestretchEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TimestretchProcessor();
}
