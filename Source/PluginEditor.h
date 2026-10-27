#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/AkaiLookAndFeel.h"
#include "UI/WaveformView.h"

/** Backlit LCD readout: sample name, format, what the last render did, status. */
class LcdDisplay : public juce::Component
{
public:
    void setLines (juce::StringArray l, bool statusIsError);
    void paint (juce::Graphics&) override;
private:
    juce::StringArray lines;
    bool error = false;
};

/** A labelled section of the front panel. Children are positioned by the editor. */
class PanelBlock : public juce::Component
{
public:
    explicit PanelBlock (juce::String t) : title (std::move (t)) {}
    void setTitle (const juce::String& t) { if (t != title) { title = t; repaint(); } }
    void paint (juce::Graphics&) override;
    juce::Rectangle<int> content() const { return getLocalBounds().reduced (14, 10).withTrimmedTop (34); }
    juce::Rectangle<int> titleArea() const { return getLocalBounds().reduced (14, 10).withHeight (30); }
private:
    juce::String title;
};

class TimestretchEditor : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit TimestretchEditor (TimestretchProcessor&);
    ~TimestretchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dropHover = true; panel.repaint(); }
    void fileDragExit (const juce::StringArray&) override { dropHover = false; panel.repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override;

    static constexpr int designW = 1040, designH = 864;

private:
    void timerCallback() override;
    void refreshDisplays (bool force);
    void layoutPanel();
    void chooseFileToLoad();
    void chooseFileToSave();

    class Panel : public juce::Component
    {
    public:
        explicit Panel (TimestretchEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        void paintOverChildren (juce::Graphics&) override;
        void resized() override { owner.layoutPanel(); }
    private:
        TimestretchEditor& owner;
    };

    TimestretchProcessor& proc;
    AkaiLookAndFeel lnf;
    Panel panel { *this };
    bool dropHover = false;

    LcdDisplay lcd;
    juce::TextButton loadButton { "LOAD" }, saveButton { "SAVE WAV" }, previewButton { "PREVIEW" };
    juce::Label dragHint;
    WaveformView inputView  { "INPUT", Palette::waveIn };
    WaveformView outputView { "AKAIZED  (DRAG INTO YOUR DAW)", Palette::waveOut };

    PanelBlock stretchBlock { "STRETCH LENGTH" }, cycleBlock { "CYCLE LENGTH" }, freqBlock { "CYCLE FREQ" };
    PanelBlock pitchBlock { "PITCH" }, gainBlock { "GAIN" }, tempoBlock { "LOOP BPM" };
    PanelBlock modeBlock { "PLAY MODE" }, rootBlock { "ROOT NOTE" }, keysBlock { "" };

    juce::Slider stretchSlider, cycleSlider, freqSlider, pitchSlider, gainSlider, bpmSlider, rootSlider, targetSlider;
    juce::ToggleButton manualToggle { "MANUAL" }, syncToggle { "SYNC" };
    juce::ToggleButton keyTrackToggle { "KEY TRACK" }, chokeToggle { "CHOKE" }, velocityToggle { "VELOCITY" };
    juce::ComboBox modeBox;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SA>> sliderAttachments;
    std::vector<std::unique_ptr<BA>> buttonAttachments;
    std::unique_ptr<CA> modeAttachment;

    std::unique_ptr<juce::FileChooser> chooser;
    uint32_t shownSourceVersion = 0xffffffff, shownRenderVersion = 0xffffffff;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TimestretchEditor)
};
