#include "PluginEditor.h"
#include "Engine/Akaizer.h"

//==============================================================================
void LcdDisplay::setLines (juce::StringArray l, bool statusIsError)
{
    if (l != lines || statusIsError != error) { lines = std::move (l); error = statusIsError; repaint(); }
}

void LcdDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (r, 7.0f);
    auto glass = r.reduced (4.0f);
    g.setColour (Palette::lcd);
    g.fillRoundedRectangle (glass, 4.0f);
    g.setColour (Palette::lcdInk.withAlpha (0.10f));
    g.drawHorizontalLine ((int) glass.getY() + 2, glass.getX() + 4.0f, glass.getRight() - 4.0f);

    auto text = glass.reduced (16.0f, 8.0f).toNearestInt();
    const int lineH = text.getHeight() / 4;
    for (int i = 0; i < 4; ++i)
    {
        auto row = text.removeFromTop (lineH);
        g.setFont (Fonts::mono (i == 0 ? 27.0f : 21.0f));
        g.setColour (i == 3 && error ? Palette::bad : Palette::lcdInk);
        g.drawFittedText (lines[i], row, juce::Justification::centredLeft, 1, 0.8f);
    }
}

void PanelBlock::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Palette::panel2);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (r, 6.0f, 2.0f);
    g.setFont (Fonts::ui (21.0f));
    g.drawFittedText (title, titleArea(), juce::Justification::centredLeft, 1, 0.85f);
}

//==============================================================================
void TimestretchEditor::Panel::paint (juce::Graphics& g)
{
    g.fillAll (Palette::panel);

    auto header = getLocalBounds().reduced (24, 0).withTrimmedTop (16).withHeight (52);
    g.setColour (Palette::ink);
    g.setFont (Fonts::ui (40.0f));
    g.drawText ("TWISTED", header, juce::Justification::centredLeft);
    const int tw = juce::GlyphArrangement::getStringWidthInt (Fonts::ui (40.0f), "TWISTED ");
    g.setColour (Palette::accent);
    g.drawText ("TIMESTRETCH", header.withTrimmedLeft (tw), juce::Justification::centredLeft);

    g.setColour (Palette::inkSoft);
    g.setFont (Fonts::mono (17.0f));
    g.drawText ("AKAIZER ENGINE  /  AFTER BEN BURCHETT'S AKAIZER v1.2", header, juce::Justification::centredRight);
}

void TimestretchEditor::Panel::paintOverChildren (juce::Graphics& g)
{
    if (owner.dropHover)
    {
        g.setColour (Palette::accent);
        g.drawRect (getLocalBounds(), 6);
        auto r = getLocalBounds().withSizeKeepingCentre (520, 80);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (Palette::white);
        g.setFont (Fonts::ui (28.0f));
        g.drawText ("DROP TO LOAD", r, juce::Justification::centred);
    }
}

//==============================================================================
TimestretchEditor::TimestretchEditor (TimestretchProcessor& p)
    : AudioProcessorEditor (p), proc (p)
{
    addAndMakeVisible (panel);

    panel.addAndMakeVisible (lcd);
    for (auto* b : { &loadButton, &saveButton, &previewButton })
        panel.addAndMakeVisible (b);
    previewButton.setColour (juce::TextButton::buttonColourId, Palette::accent);
    previewButton.setColour (juce::TextButton::textColourOffId, Palette::white);

    dragHint.setText ("Drop a WAV or AIFF anywhere on the panel to load it.", juce::dontSendNotification);
    dragHint.setFont (Fonts::ui (18.0f, false));
    dragHint.setColour (juce::Label::textColourId, Palette::inkSoft);
    dragHint.setJustificationType (juce::Justification::centredRight);
    panel.addAndMakeVisible (dragHint);

    panel.addAndMakeVisible (inputView);
    panel.addAndMakeVisible (outputView);
    inputView.setEmptyText ("No sample loaded");
    outputView.setEmptyText ("Load a sample to hear it akaized");
    outputView.setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    outputView.onDragOut = [this]
    {
        auto f = proc.writeDragFile();
        if (f.existsAsFile())
            juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, &outputView);
    };

    for (auto* b : { &stretchBlock, &cycleBlock, &freqBlock, &pitchBlock, &gainBlock, &tempoBlock,
                     &modeBlock, &rootBlock, &keysBlock })
        panel.addAndMakeVisible (b);

    auto setupSlider = [this] (juce::Slider& s, PanelBlock& block, const char* id, int textW)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, textW, 44);
        s.setScrollWheelEnabled (true);
        block.addAndMakeVisible (s);
        sliderAttachments.push_back (std::make_unique<SA> (proc.apvts, id, s));
    };
    setupSlider (stretchSlider, stretchBlock, ParamIDs::stretch, 150);
    setupSlider (cycleSlider, cycleBlock, ParamIDs::cycleLen, 110);
    setupSlider (freqSlider, freqBlock, ParamIDs::cycleFreq, 110);
    setupSlider (pitchSlider, pitchBlock, ParamIDs::pitch, 120);
    setupSlider (gainSlider, gainBlock, ParamIDs::gain, 140);
    setupSlider (bpmSlider, tempoBlock, ParamIDs::loopBpm, 120);
    setupSlider (rootSlider, rootBlock, ParamIDs::rootNote, 100);
    pitchSlider.setDoubleClickReturnValue (true, 0.0);
    gainSlider.setDoubleClickReturnValue (true, 0.0);
    stretchSlider.setDoubleClickReturnValue (true, 100.0);

    auto setupToggle = [this] (juce::ToggleButton& t, PanelBlock& block, const char* id)
    {
        block.addAndMakeVisible (t);
        buttonAttachments.push_back (std::make_unique<BA> (proc.apvts, id, t));
    };
    setupToggle (manualToggle, freqBlock, ParamIDs::freqManual);
    setupToggle (syncToggle, tempoBlock, ParamIDs::tempoSync);
    setupToggle (keyTrackToggle, keysBlock, ParamIDs::keyTrack);
    setupToggle (chokeToggle, keysBlock, ParamIDs::choke);
    setupToggle (velocityToggle, keysBlock, ParamIDs::velocity);

    modeBox.addItemList ({ "One-shot", "Gate", "Loop" }, 1);
    modeBlock.addAndMakeVisible (modeBox);
    modeAttachment = std::make_unique<CA> (proc.apvts, ParamIDs::playMode, modeBox);

    loadButton.onClick = [this] { chooseFileToLoad(); };
    saveButton.onClick = [this] { chooseFileToSave(); };
    previewButton.onClick = [this]
    {
        if (proc.isAnyVoicePlaying()) proc.stopPreview();
        else proc.startPreview();
    };

    // Disabled sliders (linked frequency, synced stretch) show the value actually in use.
    freqSlider.textFromValueFunction = [this] (double v)
    {
        return manualToggle.getToggleState() ? juce::String (juce::roundToInt (v)) : juce::String (proc.getEffectiveFrequency());
    };
    stretchSlider.textFromValueFunction = [this] (double v)
    {
        return juce::String (proc.isTempoSyncActive() ? proc.getEffectiveStretch() : v, 2) + "%";
    };

    // Applied last so every child (and every slider's value box) picks up the skin.
    setLookAndFeel (&lnf);

    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio ((double) designW / (double) designH);
        c->setSizeLimits (designW * 6 / 10, designH * 6 / 10, designW * 3 / 2, designH * 3 / 2);
    }
    setSize (designW * 9 / 10, designH * 9 / 10);

    refreshDisplays (true);
    startTimerHz (30);
}

TimestretchEditor::~TimestretchEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void TimestretchEditor::paint (juce::Graphics& g) { g.fillAll (Palette::panel); }

void TimestretchEditor::resized()
{
    const float s = (float) getWidth() / (float) designW;
    panel.setBounds (0, 0, designW, designH);
    panel.setTransform (juce::AffineTransform::scale (s));
}

void TimestretchEditor::layoutPanel()
{
    auto area = juce::Rectangle<int> (0, 0, designW, designH).reduced (24, 0);
    area.removeFromTop (16 + 52 + 8);

    lcd.setBounds (area.removeFromTop (124));
    area.removeFromTop (12);

    auto row = area.removeFromTop (56);
    loadButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (12);
    saveButton.setBounds (row.removeFromLeft (190));
    row.removeFromLeft (12);
    previewButton.setBounds (row.removeFromLeft (180));
    row.removeFromLeft (12);
    dragHint.setBounds (row);
    area.removeFromTop (12);

    inputView.setBounds (area.removeFromTop (92));
    area.removeFromTop (8);
    outputView.setBounds (area.removeFromTop (124));
    area.removeFromTop (14);

    auto threeCols = [] (juce::Rectangle<int> r, std::initializer_list<PanelBlock*> blocks)
    {
        const int gap = 14;
        const int w = (r.getWidth() - gap * 2) / 3;
        for (auto* b : blocks) { b->setBounds (r.removeFromLeft (w)); r.removeFromLeft (gap); }
    };
    threeCols (area.removeFromTop (100), { &stretchBlock, &cycleBlock, &freqBlock });
    area.removeFromTop (12);
    threeCols (area.removeFromTop (100), { &pitchBlock, &gainBlock, &tempoBlock });
    area.removeFromTop (12);
    threeCols (area.removeFromTop (100), { &modeBlock, &rootBlock, &keysBlock });

    for (auto [s, b] : { std::pair { &stretchSlider, &stretchBlock }, { &cycleSlider, &cycleBlock },
                         { &freqSlider, &freqBlock }, { &pitchSlider, &pitchBlock }, { &gainSlider, &gainBlock },
                         { &bpmSlider, &tempoBlock }, { &rootSlider, &rootBlock } })
        s->setBounds (b->content().withSizeKeepingCentre (b->content().getWidth(), 46));

    manualToggle.setBounds (freqBlock.titleArea().removeFromRight (122));
    syncToggle.setBounds (tempoBlock.titleArea().removeFromRight (100));
    modeBox.setBounds (modeBlock.content().withSizeKeepingCentre (modeBlock.content().getWidth(), 44));

    auto keys = keysBlock.getLocalBounds().reduced (14, 10);
    auto k1 = keys.removeFromTop (keys.getHeight() / 2);
    keyTrackToggle.setBounds (k1.removeFromLeft (k1.getWidth() * 55 / 100));
    chokeToggle.setBounds (k1);
    velocityToggle.setBounds (keys.removeFromLeft (keys.getWidth() * 55 / 100));
}

//==============================================================================
void TimestretchEditor::timerCallback() { refreshDisplays (false); }

void TimestretchEditor::refreshDisplays (bool force)
{
    auto src = proc.getSource();
    auto r = proc.getRendered();

    if (force || shownSourceVersion != proc.getSourceVersion())
    {
        shownSourceVersion = proc.getSourceVersion();
        if (src != nullptr)
        {
            inputView.setData (src->pcm.data(), src->pcm.size(), src->channels, src->sampleRate);
            inputView.setRightCaption (juce::String (src->lengthSeconds(), 3) + " S");
        }
        else
        {
            inputView.clear();
            inputView.setRightCaption ({});
        }
    }

    if (force || shownRenderVersion != proc.getRenderVersion())
    {
        shownRenderVersion = proc.getRenderVersion();
        if (r != nullptr)
        {
            outputView.setData (r->pcm.data(), r->pcm.size(), r->channels, r->sampleRate);
            outputView.setRightCaption (juce::String ((double) (r->pcm.size() / (size_t) r->channels) / r->sampleRate, 3) + " S");
        }
        else
        {
            outputView.clear();
            outputView.setRightCaption ({});
        }
    }
    outputView.setPlayhead (proc.getPlayheadFraction());

    // LCD
    juce::StringArray lines;
    if (src != nullptr)
    {
        lines.add (src->name.toUpperCase());
        const juce::String chans = src->channels == 1 ? "MONO" : "STEREO";
        lines.add (juce::String ((int) std::lround (src->sampleRate)) + " HZ  " + juce::String (src->originalBits) + "-BIT  "
                   + chans + "  " + juce::String (src->lengthSeconds(), 3) + " S");
    }
    else
    {
        lines.add ("NO SAMPLE LOADED");
        lines.add ("DROP A WAV OR AIFF ON THE PANEL, OR PRESS LOAD");
    }
    if (r != nullptr)
    {
        const double hop = (double) r->outHopWords / (double) r->channels;
        juce::String line = "GRAIN " + juce::String (r->cycleLength * 2) + " SMP   HOP "
                            + juce::String (hop, (r->outHopWords % r->channels) != 0 ? 1 : 0) + " SMP   "
                            + juce::String (r->stretchPercent, 2) + "%";
        if (std::abs (r->semitones) > 0.5) line << "   " << (r->semitones > 0 ? "+" : "") << juce::String ((int) r->semitones) << " ST";
        if (proc.isTempoSyncActive()) line << "   SYNC " << juce::String (proc.getHostBpm(), 1) << " BPM";
        lines.add (line);
    }
    else
    {
        lines.add ("GRAIN --   HOP --");
    }
    const auto status = proc.getStatus();
    const bool err = status.startsWith ("Can't") || status.startsWith ("Missing") || status.startsWith ("File not found")
                     || status.contains ("has no audio") || status.contains ("channels.") || status.startsWith ("Couldn't");
    lines.add (status);
    lcd.setLines (lines, err);

    // Block titles and enable states
    const bool sync = proc.isTempoSyncActive();
    stretchBlock.setTitle (sync ? "STRETCH (SYNCED)" : "STRETCH LENGTH");
    if (stretchSlider.isEnabled() == sync) stretchSlider.setEnabled (! sync);
    stretchSlider.updateText();

    const bool manual = manualToggle.getToggleState();
    if (freqSlider.isEnabled() != manual) freqSlider.setEnabled (manual);
    freqSlider.updateText();

    saveButton.setEnabled (r != nullptr);
    previewButton.setEnabled (r != nullptr);
    previewButton.setButtonText (proc.isAnyVoicePlaying() ? "STOP" : "PREVIEW");
}

//==============================================================================
bool TimestretchEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;wave;aif;aiff;aifc;flac;mp3;ogg"))
            return true;
    return false;
}

void TimestretchEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dropHover = false;
    panel.repaint();
    for (auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;wave;aif;aiff;aifc;flac;mp3;ogg"))
        {
            proc.loadFile (juce::File (f));
            refreshDisplays (true);
            return;
        }
}

void TimestretchEditor::chooseFileToLoad()
{
    juce::File start = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (auto src = proc.getSource(); src != nullptr && juce::File (src->path).existsAsFile())
        start = juce::File (src->path).getParentDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Load a sample to akaize", start, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f.existsAsFile()) { proc.loadFile (f); refreshDisplays (true); }
                          });
}

void TimestretchEditor::chooseFileToSave()
{
    auto r = proc.getRendered();
    if (r == nullptr) return;

    juce::File dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (auto src = proc.getSource(); src != nullptr && juce::File (src->path).existsAsFile())
        dir = juce::File (src->path).getParentDirectory();

    chooser = std::make_unique<juce::FileChooser> ("Save the akaized WAV",
                                                   dir.getChildFile (juce::File::createLegalFileName (r->name) + ".wav"), "*.wav");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File()) return;
                              if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                              proc.setStatus (proc.saveRenderedWav (f) ? "Saved " + f.getFileName()
                                                                       : "Couldn't save " + f.getFullPathName());
                          });
}
