#pragma once

#include <JuceHeader.h>

/** Light sampler-front-panel skin: warm casing grey, backlit green LCD with dark text,
    one orange key colour. Big, bold, high-contrast type throughout. */
namespace Palette
{
    inline const juce::Colour panel    { 0xffdcdad3 };
    inline const juce::Colour panel2   { 0xffecebe6 };
    inline const juce::Colour ink      { 0xff15171a };
    inline const juce::Colour inkSoft  { 0xff3d4148 };
    inline const juce::Colour line     { 0xff9c9a92 };
    inline const juce::Colour lcd      { 0xffc7d6a2 };
    inline const juce::Colour lcdInk   { 0xff142008 };
    inline const juce::Colour accent   { 0xffc2410c };
    inline const juce::Colour waveIn   { 0xff2b4a7a };
    inline const juce::Colour waveOut  { 0xffb3360a };
    inline const juce::Colour white    { 0xffffffff };
    inline const juce::Colour bad      { 0xffa3121a };
}

namespace Fonts
{
    juce::Font ui (float size, bool bold = true);
    juce::Font mono (float size, bool bold = true);
}

class AkaiLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AkaiLookAndFeel();

    juce::Font getLabelFont (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool isMouseOverButton, bool isButtonDown) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool isMouseOverButton, bool isButtonDown) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawLabel (juce::Graphics&, juce::Label&) override;
};
