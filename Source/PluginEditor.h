#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
// forward-declare window types to avoid include cycles
class DescriptorWindow;
class BatchWindow;
class ResynthesisWindow;
#include <memory>
#include <functional>

// Minimal WaveformComponent used by the editor.
// Replace with your real waveform drawing implementation later.
class WaveformComponent : public juce::Component
{
public:
    WaveformComponent() = default;
    void setBuffer(juce::AudioBuffer<float>* b) { buf = b; repaint(); }
    void paint(juce::Graphics& g) override
    {
        // draw a simple waveform without an opaque background rectangle
        if (!buf || buf->getNumSamples() == 0) return;
        g.setColour(juce::Colour(0xFF4DB6A9));
        auto bounds = getLocalBounds().toFloat().reduced(4.0f);
        const int w = bounds.getWidth();
        const int h = bounds.getHeight();
        const int numSamples = buf->getNumSamples();

        juce::Path p;
        p.startNewSubPath(bounds.getX(), bounds.getCentreY());
        for (int x = 0; x < w; ++x)
        {
            int sampleIndex = juce::jlimit(0, numSamples - 1, (int)((double)x / (double)w * (double)numSamples));
            float v = buf->getSample(0, sampleIndex);
            float y = bounds.getCentreY() - v * (h * 0.45f);
            p.lineTo(bounds.getX() + (float)x, y);
        }
        g.strokePath(p, juce::PathStrokeType(1.2f));
    }

private:
    juce::AudioBuffer<float>* buf { nullptr };
};

// RegeneratingSlider wraps juce::Slider and notifies when mouseUp occurs.
// Minimal implementation used by the editor. Keep signature compatible with uses.
class RegeneratingSlider : public juce::Slider
{
public:
    using MouseUpCallback = std::function<void()>;

    RegeneratingSlider() = default;

    void setMouseUpCallback(MouseUpCallback cb) { mouseUpCb = std::move(cb); }

    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseUp(e);
        if (mouseUpCb) mouseUpCb();
    }

private:
    MouseUpCallback mouseUpCb;
};

//==============================================================================
// The main plugin editor
class PluginEditor  : public juce::AudioProcessorEditor,
                      private juce::Button::Listener,
                      private juce::Slider::Listener
{
public:
    PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
   
private:
    // UI controls (kept from previous implementation)
    WaveformComponent waveform;
    juce::Image logoImage;
    juce::TextButton generateButton { "GENERATE 808" };
    juce::TextButton exportButton { "EXPORT" };
    juce::ToggleButton previewToggle { "Preview" };
    juce::Label noteLabel;
    juce::Label seedLabel;
    juce::TextButton copySeedButton { "Copy Seed" };
    RegeneratingSlider tuneSlider;
    juce::Label tuneLabel;
    // ownership for popup windows (add to your editor class members)
    std::unique_ptr<ResynthesisWindow> resynthesisWindow;

    // logo things
    class LogoComponent : public juce::Component
    {
    public:
        void setImage(const juce::Image& img) { image = img; repaint(); }
        void paint(juce::Graphics& g) override
        {
            if (image.isValid())
                g.drawImageWithin(image, 0, 0, getWidth(), getHeight(),
                    juce::RectanglePlacement::centred);
        }
    private:
        juce::Image image;
    };

    LogoComponent logoComponent;
    juce::Image logoImage_;


    // new: main menu button (hamburger)
    juce::TextButton mainMenuBtn { "≡" }; // small icon-like button

    PluginProcessor& processor;

    // windows
    std::unique_ptr<DescriptorWindow> descriptorWindow;
    std::unique_ptr<BatchWindow> batchWindow;

    // we keep a shared_ptr to the generated buffer so its memory remains valid
    std::shared_ptr<juce::AudioBuffer<float>> currentGeneratedBufferPtr;

    // lastParams display
    void updateWaveformFromProcessor();

    // regenerate helper (collects UI values -> params -> generate)
    void regenerateFromCurrentUI();

    // handlers
    void buttonClicked(juce::Button* b) override;
    void sliderValueChanged(juce::Slider* s) override;

    // helper to show main menu
    void showMainMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
