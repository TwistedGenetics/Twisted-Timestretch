#include "AkaiLookAndFeel.h"

namespace Fonts
{
    juce::Font ui (float size, bool bold)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    juce::Font mono (float size, bool bold)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size,
                                              bold ? juce::Font::bold : juce::Font::plain));
    }
}

AkaiLookAndFeel::AkaiLookAndFeel()
{
    using namespace juce;
    setColour (ResizableWindow::backgroundColourId, Palette::panel);
    setColour (Label::textColourId, Palette::ink);
    setColour (TextButton::buttonColourId, Palette::white);
    setColour (TextButton::buttonOnColourId, Palette::accent);
    setColour (TextButton::textColourOffId, Palette::ink);
    setColour (TextButton::textColourOnId, Palette::white);
    setColour (ToggleButton::textColourId, Palette::ink);
    setColour (ToggleButton::tickColourId, Palette::white);
    setColour (Slider::textBoxTextColourId, Palette::ink);
    setColour (Slider::textBoxBackgroundColourId, Palette::white);
    setColour (Slider::textBoxOutlineColourId, Palette::ink);
    setColour (Slider::textBoxHighlightColourId, Palette::lcd);
    setColour (ComboBox::backgroundColourId, Palette::white);
    setColour (ComboBox::textColourId, Palette::ink);
    setColour (ComboBox::outlineColourId, Palette::ink);
    setColour (ComboBox::arrowColourId, Palette::ink);
    setColour (PopupMenu::backgroundColourId, Palette::white);
    setColour (PopupMenu::textColourId, Palette::ink);
    setColour (PopupMenu::highlightedBackgroundColourId, Palette::accent);
    setColour (PopupMenu::highlightedTextColourId, Palette::white);
    setColour (TextEditor::backgroundColourId, Palette::white);
    setColour (TextEditor::textColourId, Palette::ink);
    setColour (TextEditor::highlightColourId, Palette::lcd);
    setColour (TextEditor::highlightedTextColourId, Palette::ink);
    setColour (TextEditor::outlineColourId, Palette::ink);
    setColour (TextEditor::focusedOutlineColourId, Palette::accent);
    setColour (CaretComponent::caretColourId, Palette::ink);
    setColour (AlertWindow::backgroundColourId, Palette::panel2);
    setColour (AlertWindow::textColourId, Palette::ink);
    setColour (AlertWindow::outlineColourId, Palette::ink);
    setColour (DocumentWindow::textColourId, Palette::ink);

    // Standalone settings dialogs read these.
    getCurrentColourScheme().setUIColour (ColourScheme::windowBackground, Palette::panel);
    getCurrentColourScheme().setUIColour (ColourScheme::widgetBackground, Palette::white);
    getCurrentColourScheme().setUIColour (ColourScheme::menuBackground, Palette::white);
    getCurrentColourScheme().setUIColour (ColourScheme::outline, Palette::ink);
    getCurrentColourScheme().setUIColour (ColourScheme::defaultText, Palette::ink);
    getCurrentColourScheme().setUIColour (ColourScheme::defaultFill, Palette::accent);
    getCurrentColourScheme().setUIColour (ColourScheme::highlightedText, Palette::white);
    getCurrentColourScheme().setUIColour (ColourScheme::highlightedFill, Palette::accent);
    getCurrentColourScheme().setUIColour (ColourScheme::menuText, Palette::ink);
}

juce::Font AkaiLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

juce::Font AkaiLookAndFeel::getTextButtonFont (juce::TextButton&, int h)
{
    return Fonts::ui (juce::jmin (22.0f, (float) h * 0.5f));
}

juce::Font AkaiLookAndFeel::getComboBoxFont (juce::ComboBox&) { return Fonts::ui (21.0f); }
juce::Font AkaiLookAndFeel::getPopupMenuFont() { return Fonts::ui (20.0f); }

void AkaiLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                            bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    r.removeFromBottom (3.0f);
    const bool on = b.getToggleState();
    auto fill = on ? Palette::accent : b.findColour (juce::TextButton::buttonColourId);
    if (! b.isEnabled()) fill = Palette::panel2;
    else if (over && ! down) fill = fill.interpolatedWith (Palette::lcd, on ? 0.0f : 0.25f);

    const float drop = down ? 1.0f : 3.0f;
    g.setColour (Palette::ink);
    g.fillRoundedRectangle (r.translated (0.0f, 3.0f), 5.0f);
    auto face = r.translated (0.0f, 3.0f - drop);
    g.setColour (fill);
    g.fillRoundedRectangle (face, 5.0f);
    g.setColour (b.isEnabled() ? Palette::ink : Palette::line);
    g.drawRoundedRectangle (face, 5.0f, 2.0f);
}

void AkaiLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat();
    const float box = juce::jmin (28.0f, r.getHeight() - 4.0f);
    juce::Rectangle<float> tick (r.getX() + 2.0f, r.getCentreY() - box * 0.5f, box, box);

    g.setColour (b.getToggleState() ? Palette::accent : (over ? Palette::panel2 : Palette::white));
    g.fillRoundedRectangle (tick, 4.0f);
    g.setColour (b.isEnabled() ? Palette::ink : Palette::line);
    g.drawRoundedRectangle (tick, 4.0f, 2.0f);
    if (b.getToggleState())
    {
        juce::Path p;
        auto t = tick.reduced (box * 0.22f);
        p.startNewSubPath (t.getX(), t.getCentreY());
        p.lineTo (t.getX() + t.getWidth() * 0.4f, t.getBottom());
        p.lineTo (t.getRight(), t.getY());
        g.setColour (Palette::white);
        g.strokePath (p, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour (b.isEnabled() ? Palette::ink : Palette::inkSoft);
    g.setFont (Fonts::ui (juce::jmin (21.0f, r.getHeight() * 0.62f)));
    g.drawFittedText (b.getButtonText(), r.withTrimmedLeft (box + 12.0f).toNearestInt(),
                      juce::Justification::centredLeft, 1);
}

void AkaiLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float pos, float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float trackH = 12.0f;
    auto track = r.withSizeKeepingCentre (r.getWidth(), trackH);
    const bool en = s.isEnabled();

    g.setColour (Palette::white);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (en ? Palette::accent : Palette::line);
    g.fillRoundedRectangle (track.withRight (pos), 3.0f);
    g.setColour (en ? Palette::ink : Palette::line);
    g.drawRoundedRectangle (track, 3.0f, 2.0f);

    const float thumbW = 18.0f, thumbH = juce::jmin (36.0f, r.getHeight() - 2.0f);
    juce::Rectangle<float> thumb (pos - thumbW * 0.5f, r.getCentreY() - thumbH * 0.5f, thumbW, thumbH);
    g.setColour (en ? Palette::panel2 : Palette::panel);
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (en ? Palette::ink : Palette::line);
    g.drawRoundedRectangle (thumb, 3.0f, 2.0f);
    g.drawLine (thumb.getCentreX(), thumb.getY() + 6.0f, thumb.getCentreX(), thumb.getBottom() - 6.0f, 2.0f);
}

juce::Label* AkaiLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (Fonts::mono (26.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, Palette::ink);
    l->setColour (juce::Label::backgroundColourId, Palette::white);
    l->setColour (juce::Label::outlineColourId, Palette::ink);
    l->setColour (juce::Label::textWhenEditingColourId, Palette::ink);
    l->setColour (juce::TextEditor::textColourId, Palette::ink);
    l->setColour (juce::TextEditor::backgroundColourId, Palette::white);
    l->setColour (juce::TextEditor::highlightColourId, Palette::lcd);
    l->setColour (juce::TextEditor::outlineColourId, Palette::accent);
    l->setColour (juce::TextEditor::focusedOutlineColourId, Palette::accent);
    return l;
}

void AkaiLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& c)
{
    auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (1.0f);
    g.setColour (Palette::white);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (c.isEnabled() ? Palette::ink : Palette::line);
    g.drawRoundedRectangle (r, 4.0f, 2.0f);
    juce::Path arrow;
    const float ax = (float) w - 26.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 8.0f, ay - 5.0f, ax + 8.0f, ay - 5.0f, ax, ay + 6.0f);
    g.fillPath (arrow);
}

void AkaiLookAndFeel::positionComboBoxText (juce::ComboBox& c, juce::Label& l)
{
    l.setBounds (10, 1, c.getWidth() - 46, c.getHeight() - 2);
    l.setFont (getComboBoxFont (c));
}

// Like the stock label, but a disabled value box keeps dark, readable text
// (a linked or synced value is still the value in use).
void AkaiLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.fillAll (label.findColour (juce::Label::backgroundColourId));
    if (! label.isBeingEdited())
    {
        const auto font = getLabelFont (label);
        g.setColour (label.isEnabled() ? label.findColour (juce::Label::textColourId) : Palette::inkSoft);
        g.setFont (font);
        auto area = getLabelBorderSize (label).subtractedFrom (label.getLocalBounds());
        g.drawFittedText (label.getText(), area, label.getJustificationType(),
                          juce::jmax (1, (int) ((float) area.getHeight() / font.getHeight())), label.getMinimumHorizontalScale());
        g.setColour (label.isEnabled() ? label.findColour (juce::Label::outlineColourId) : Palette::line);
    }
    else
    {
        g.setColour (label.findColour (juce::Label::outlineColourId));
    }
    g.drawRect (label.getLocalBounds(), label.findColour (juce::Label::outlineColourId).isTransparent() ? 0 : 2);
}
