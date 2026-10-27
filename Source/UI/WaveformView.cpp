#include "WaveformView.h"
#include "AkaiLookAndFeel.h"

WaveformView::WaveformView (juce::String c, juce::Colour col)
    : caption (std::move (c)), colour (col)
{
    setOpaque (false);
}

void WaveformView::setData (const int16_t* pcm, size_t numWords, int channels, double)
{
    constexpr int buckets = 1024;
    mins.assign (buckets, 0.0f);
    maxs.assign (buckets, 0.0f);
    if (pcm == nullptr || channels <= 0 || numWords == 0) { repaint(); return; }

    const size_t frames = numWords / (size_t) channels;
    for (int b = 0; b < buckets; ++b)
    {
        const size_t f0 = frames * (size_t) b / buckets;
        const size_t f1 = juce::jmax (f0 + 1, frames * (size_t) (b + 1) / buckets);
        int mn = 0, mx = 0;
        const size_t step = juce::jmax<size_t> (1, (f1 - f0) / 256);
        for (size_t f = f0; f < f1 && f < frames; f += step)
            for (int c = 0; c < channels; ++c)
            {
                const int v = pcm[f * (size_t) channels + (size_t) c];
                mn = juce::jmin (mn, v); mx = juce::jmax (mx, v);
            }
        mins[(size_t) b] = (float) mn / 32768.0f;
        maxs[(size_t) b] = (float) mx / 32768.0f;
    }
    repaint();
}

void WaveformView::clear() { mins.clear(); maxs.clear(); repaint(); }

void WaveformView::setPlayhead (float f)
{
    if (std::abs (f - playhead) > 0.0005f) { playhead = f; repaint(); }
}

void WaveformView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::panel2);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (Palette::ink);
    g.drawRoundedRectangle (r.reduced (1.0f), 6.0f, 2.0f);

    auto inner = getLocalBounds().reduced (12, 8);
    auto cap = inner.removeFromTop (26);
    g.setFont (Fonts::mono (19.0f));
    g.setColour (Palette::ink);
    g.drawText (caption, cap, juce::Justification::centredLeft);
    g.drawText (rightCaption, cap, juce::Justification::centredRight);
    inner.removeFromTop (4);

    auto wave = inner.toFloat();
    g.setColour (Palette::white);
    g.fillRect (wave);
    g.setColour (Palette::line);
    g.drawRect (wave, 1.0f);
    g.drawHorizontalLine ((int) wave.getCentreY(), wave.getX(), wave.getRight());

    if (maxs.empty())
    {
        g.setColour (Palette::inkSoft);
        g.setFont (Fonts::ui (19.0f, false));
        g.drawText (emptyText, wave.toNearestInt(), juce::Justification::centred);
        return;
    }

    g.setColour (colour);
    const int w = (int) wave.getWidth();
    const float mid = wave.getCentreY(), half = wave.getHeight() * 0.5f - 2.0f;
    const auto n = maxs.size();
    for (int x = 0; x < w; ++x)
    {
        const size_t b0 = n * (size_t) x / (size_t) w;
        const size_t b1 = juce::jmax (b0 + 1, n * (size_t) (x + 1) / (size_t) w);
        float mn = 0.0f, mx = 0.0f;
        for (size_t b = b0; b < b1 && b < n; ++b) { mn = juce::jmin (mn, mins[b]); mx = juce::jmax (mx, maxs[b]); }
        const float y1 = mid - mx * half, y2 = mid - mn * half;
        g.fillRect (wave.getX() + (float) x, y1, 1.0f, juce::jmax (1.0f, y2 - y1));
    }

    if (playhead >= 0.0f && playhead <= 1.0f)
    {
        g.setColour (Palette::ink);
        const float px = wave.getX() + playhead * wave.getWidth();
        g.fillRect (px - 1.0f, wave.getY(), 2.0f, wave.getHeight());
    }
}

void WaveformView::mouseDown (const juce::MouseEvent&) { dragStarted = false; }

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragStarted && onDragOut && ! maxs.empty() && e.getDistanceFromDragStart() > 6)
    {
        dragStarted = true;
        onDragOut();
    }
}
