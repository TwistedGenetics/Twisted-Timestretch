#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <functional>
#include <vector>

/** A waveform strip with a caption. The akaized one can be dragged out into a DAW. */
class WaveformView : public juce::Component
{
public:
    WaveformView (juce::String caption, juce::Colour colour);

    void setData (const int16_t* interleaved, size_t numWords, int channels, double sampleRate);
    void clear();
    void setRightCaption (const juce::String& s) { rightCaption = s; repaint(); }
    void setPlayhead (float fraction);          // < 0 hides it
    void setEmptyText (const juce::String& s) { emptyText = s; repaint(); }

    /** Called once per drag gesture when the user drags out of the waveform. */
    std::function<void()> onDragOut;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    juce::String caption, rightCaption, emptyText;
    juce::Colour colour;
    std::vector<float> mins, maxs;   // 1024 buckets, -1..1
    float playhead = -1.0f;
    bool dragStarted = false;
};
