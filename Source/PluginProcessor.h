#pragma once

#include <JuceHeader.h>
#include "Engine/SampleData.h"
#include <array>
#include <atomic>
#include <memory>

namespace ParamIDs
{
    inline constexpr const char* stretch    = "stretch";
    inline constexpr const char* cycleLen   = "cycleLength";
    inline constexpr const char* cycleFreq  = "cycleFreq";
    inline constexpr const char* freqManual = "freqManual";
    inline constexpr const char* pitch      = "pitch";
    inline constexpr const char* tempoSync  = "tempoSync";
    inline constexpr const char* loopBpm    = "loopBpm";
    inline constexpr const char* playMode   = "playMode";
    inline constexpr const char* rootNote   = "rootNote";
    inline constexpr const char* keyTrack   = "keyTrack";
    inline constexpr const char* choke      = "choke";
    inline constexpr const char* velocity   = "velocity";
    inline constexpr const char* gain       = "gain";
}

enum class PlayMode { oneShot = 0, gate = 1, loop = 2 };

/** Akaizer as an instrument: load a sample and it is timestretched with the original
    Akaizer algorithm in the background whenever a setting changes; MIDI notes play
    the result (repitched across the keyboard like a sampler, or at one pitch). */
class TimestretchProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    TimestretchProcessor();
    ~TimestretchProcessor() override;

    // --- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // --- Sample and render (message thread)
    bool loadFile (const juce::File& file);                 // decode + render now
    void setSource (std::unique_ptr<SourceSample> s, bool guessTempo);
    std::shared_ptr<const SourceSample> getSource() const;
    RenderedSample::Ptr getRendered() const;                 // latest finished render
    bool renderNow();                                        // synchronous render with current settings
    bool saveRenderedWav (const juce::File& file) const;
    juce::File writeDragFile() const;                        // temp WAV for dragging into a DAW

    // --- Settings helpers
    double getEffectiveStretch() const;                      // stretch % after tempo sync
    int getEffectiveFrequency() const;                       // manual value or 41000 / cycle
    bool isTempoSyncActive() const;
    double getHostBpm() const noexcept { return hostBpm.load(); }
    static double guessLoopBpm (double seconds);

    // --- Preview from the UI (picked up by the audio thread)
    void startPreview() noexcept { previewRequest.store (1); }
    void stopPreview()  noexcept { previewRequest.store (2); }
    bool isAnyVoicePlaying() const noexcept { return voicesPlaying.load() > 0; }
    int getActiveVoiceCount() const noexcept { return voicesPlaying.load(); }
    float getPlayheadFraction() const noexcept { return playhead.load(); }

    juce::String getStatus() const;
    void setStatus (const juce::String& s);
    uint32_t getSourceVersion() const noexcept { return sourceVersion.load(); }
    uint32_t getRenderVersion() const noexcept { return renderVersion.load(); }

    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    void parameterChanged (const juce::String& id, float newValue) override;
    void markDirty() noexcept;
    void handleNoteOn (int note, float velocity, bool isPreview);
    void handleNoteOff (int note, bool isPreview);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int num);

    struct Voice
    {
        RenderedSample::Ptr smp;
        double pos = 0.0, inc = 1.0;
        float level = 1.0f, env = 0.0f, attackStep = 0.0f, releaseStep = 0.0f;
        int note = -1;
        bool active = false, releasing = false, preview = false, looping = false, oneShot = false;
        uint64_t startedAt = 0;
    };

    class RenderThread : public juce::Thread
    {
    public:
        explicit RenderThread (TimestretchProcessor& o) : juce::Thread ("Akaizer render"), owner (o) {}
        void run() override;
    private:
        TimestretchProcessor& owner;
    };

    // Parameters (raw pointers into the APVTS)
    std::atomic<float>* pStretch = nullptr;
    std::atomic<float>* pCycleLen = nullptr;
    std::atomic<float>* pCycleFreq = nullptr;
    std::atomic<float>* pFreqManual = nullptr;
    std::atomic<float>* pPitch = nullptr;
    std::atomic<float>* pTempoSync = nullptr;
    std::atomic<float>* pLoopBpm = nullptr;
    std::atomic<float>* pPlayMode = nullptr;
    std::atomic<float>* pRootNote = nullptr;
    std::atomic<float>* pKeyTrack = nullptr;
    std::atomic<float>* pChoke = nullptr;
    std::atomic<float>* pVelocity = nullptr;
    std::atomic<float>* pGain = nullptr;

    // Source sample
    mutable juce::CriticalSection sourceLock;
    std::shared_ptr<const SourceSample> source;
    std::atomic<uint32_t> sourceVersion { 0 };

    // Renders
    juce::CriticalSection renderLock;                 // one render at a time; guards pool
    juce::ReferenceCountedArray<RenderedSample> pool; // keeps every render alive until unused
    mutable juce::SpinLock currentLock;
    RenderedSample::Ptr current;
    std::atomic<uint32_t> renderVersion { 0 };
    std::atomic<bool> dirty { false };
    std::atomic<double> dirtySince { 0.0 };
    std::atomic<double> renderedSyncBpm { 0.0 };
    RenderThread renderThread { *this };

    // Audio thread
    RenderedSample::Ptr audioCurrent;
    std::array<Voice, 16> voices;
    uint64_t noteCounter = 0;
    double currentSampleRate = 44100.0;
    juce::SmoothedValue<float> gainSmoothed;
    std::atomic<double> hostBpm { 0.0 };
    std::atomic<int> previewRequest { 0 };
    std::atomic<int> voicesPlaying { 0 };
    std::atomic<float> playhead { -1.0f };

    mutable juce::CriticalSection statusLock;
    juce::String status { "Load a sample: drop a WAV or AIFF here or press LOAD." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimestretchProcessor)
};
