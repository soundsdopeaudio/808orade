#include "DescriptorWindow.h"

using namespace juce;

DescriptorWindow::DescriptorWindow(PluginProcessor& ownerProcessor)
    : DocumentWindow("Descriptor / Tagging",
        Colours::transparentBlack,
        DocumentWindow::allButtons),
    owner(ownerProcessor)
{
    setUsingNativeTitleBar(false);
    setResizable(true, true);
    setResizeLimits(600, 400, 2500, 1500);

    // create and attach content component that owns children
    content.reset(new Component());
    setContentOwned(content.get(), true);

    // Left column categories
    const char* categories[] = { "Tone", "Length", "Character", "Texture", "Harmonic" };
    for (auto c : categories)
    {
        auto* btn = new TextButton(c);
        btn->setColour(TextButton::buttonColourId, Colour(0xFF1E1F21));
        btn->setColour(TextButton::textColourOnId, Colour(0xFFD6DCE0));
        content->addAndMakeVisible(btn);
        categoryButtons.add(btn);
        btn->addListener(this);
    }

    // populate keywords (IDs follow naming rule)
    const std::vector<std::pair<const char*, const char*>> keywords = {
        { "subBtn", "sub" }, { "boomyBtn", "boomy" }, { "shortBtn", "short" }, { "longBtn", "long" },
        { "lowBtn", "low" }, { "midBtn", "mid" }, { "highBtn", "high" }, { "punchyBtn", "punchy" },
        { "growlBtn", "growl" }, { "detunedBtn", "detuned" }, { "analogBtn", "analog" }, { "cleanBtn", "clean" },
        { "softBtn", "soft" }, { "hardBtn", "hard" }, { "thumpBtn", "thump" }, { "snappyBtn", "snappy" },
        { "roundedBtn", "rounded" }, { "harshBtn", "harsh" }, { "deepBtn", "deep" }, { "boingBtn", "boing" },
        { "saturatedBtn", "saturated" }, { "filteredBtn", "filtered" }, { "vintageBtn", "vintage" }, { "biteBtn", "bite" }
    };

    for (auto& kp : keywords)
        addKeyword(kp.first, kp.second);

    // default intensities
    for (auto& kv : keywordButtonMap)
        keywordIntensity[kv.first] = 0.6f;

    // Right column controls
    promptEditor.setMultiLine(false);
    promptEditor.setReturnKeyStartsNewLine(false);
    promptEditor.setTextToShowWhenEmpty("e.g. deep boomy with slight distortion", Colours::grey);
    promptEditor.addListener(this);
    content->addAndMakeVisible(promptEditor);

    content->addAndMakeVisible(applyPromptBtn);
    applyPromptBtn.addListener(this);

    content->addAndMakeVisible(savePresetBtn);
    savePresetBtn.addListener(this);

    content->addAndMakeVisible(resetBtn);
    resetBtn.addListener(this);

    content->addAndMakeVisible(generateBtn);
    generateBtn.addListener(this);

    content->addAndMakeVisible(randomizeBtn);
    randomizeBtn.addListener(this);

    intensityLabel.setText("Intensity", dontSendNotification);
    intensityLabel.setColour(Label::textColourId, Colour(0xff9aa0a6));
    content->addAndMakeVisible(intensityLabel);

    intensityKnob.setSliderStyle(Slider::RotaryVerticalDrag);
    intensityKnob.setRange(0.0, 1.0, 0.01);
    intensityKnob.setValue(0.65);
    content->addAndMakeVisible(intensityKnob);

    content->addAndMakeVisible(keywordSummaryBox);

    // show window centered but hidden by default
    centreWithSize(1000, 700);
    setVisible(false);
}

DescriptorWindow::~DescriptorWindow()
{
    applyPromptBtn.removeListener(this);
    savePresetBtn.removeListener(this);
    resetBtn.removeListener(this);
    generateBtn.removeListener(this);
    randomizeBtn.removeListener(this);
    promptEditor.removeListener(this);
    // OwnedArray will delete buttons automatically
}

void DescriptorWindow::open()
{
    setVisible(true);
    toFront(true);
}

void DescriptorWindow::closeWindow()
{
    setVisible(false);
}
void DescriptorWindow::closeButtonPressed()
{
    setVisible(false);
    if (onCloseCallback) onCloseCallback();
}

void DescriptorWindow::addKeyword(const juce::String& id, const juce::String& labelText)
{
    // Create toggle button and add to content
    auto* tb = new ToggleButton(labelText);
    tb->setToggleState(false, dontSendNotification);
    tb->setColour(ToggleButton::textColourId, Colour(0xFFD6DCE0));
    tb->setColour(ToggleButton::tickColourId, Colour(0xFF4DB6A9));
    content->addAndMakeVisible(tb);
    keywordButtons.add(tb);
    keywordButtonMap[id] = tb;
    // default intensity
    keywordIntensity[id] = 0.6f;
    tb->addListener(this);
}

std::vector<std::pair<juce::String, float>> DescriptorWindow::getSelectedKeywords() const
{
    std::vector<std::pair<juce::String, float>> out;
    for (auto& kv : keywordButtonMap)
    {
        if (kv.second->getToggleState())
        {
            float val = 0.6f;
            auto it = keywordIntensity.find(kv.first);
            if (it != keywordIntensity.end()) val = it->second;
            out.emplace_back(kv.first, val);
        }
    }
    return out;
}

void DescriptorWindow::applyPrompt(const juce::String& text)
{
    auto lower = text.toLowerCase();
    StringArray tokens;
    tokens.addTokens(lower, " ,.-;:()[]{}", "");
    tokens.trim();
    tokens.removeEmptyStrings();

    for (auto& kv : keywordButtonMap)
    {
        String label = kv.second->getButtonText().toLowerCase();
        bool matched = false;
        for (auto& t : tokens)
        {
            if (label.contains(t) || t.contains(label) || label.startsWith(t) || t.startsWith(label))
            {
                matched = true; break;
            }
        }
        kv.second->setToggleState(matched, sendNotification);
        if (matched)
            keywordIntensity[kv.first] = jlimit(0.0f, 1.0f, keywordIntensity[kv.first] + 0.15f);
    }
}

void DescriptorWindow::buttonClicked(juce::Button* b)
{
    // category buttons do nothing fancy currently
    for (auto* cat : categoryButtons)
    {
        if (b == cat)
            return;
    }

    if (b == &applyPromptBtn)
    {
        applyPrompt(promptEditor.getText());
    }
    else if (b == &resetBtn)
    {
        for (auto& kv : keywordButtonMap)
            kv.second->setToggleState(false, sendNotification);
    }
    else if (b == &randomizeBtn)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> d(0.35f, 1.0f);
        for (auto& kv : keywordButtonMap)
        {
            bool on = (d(gen) > 0.7f);
            kv.second->setToggleState(on, sendNotification);
            keywordIntensity[kv.first] = d(gen);
        }
    }
    else if (b == &generateBtn)
    {
        // user could close and rely on main window to read selections
        setVisible(false);
    }
    else if (b == &savePresetBtn)
    {
        juce::FileChooser fc("Save descriptor preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
        // use async with explicit flags
        fc.launchAsync(juce::FileBrowserComponent::saveMode, [this](const juce::FileChooser& c)
        {
            juce::File out = c.getResult();
            if (! out.existsAsFile())
                return; // cancelled

            if (!out.hasFileExtension("json")) out = out.withFileExtension(".json");

            juce::DynamicObject::Ptr rootObj = new juce::DynamicObject();
            juce::Array<juce::var> arr;
            for (auto& kv : keywordButtonMap)
            {
                if (kv.second->getToggleState())
                {
                    juce::DynamicObject::Ptr o = new juce::DynamicObject();
                    o->setProperty("id", kv.first);
                    o->setProperty("label", kv.second->getButtonText());
                    o->setProperty("intensity", (double)keywordIntensity.at(kv.first));
                    arr.add(juce::var(o.get()));
                }
            }

            rootObj->setProperty("keywords", juce::var(arr));
            juce::var rootVar = juce::var(rootObj.get());
            juce::String json = juce::JSON::toString(rootVar, true);
            out.replaceWithText(json);
        });
    }
}

void DescriptorWindow::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &promptEditor)
        applyPrompt(promptEditor.getText());
}

void DescriptorWindow::resized()
{
    if (!getContentComponent()) return;

    auto area = getContentComponent()->getLocalBounds().reduced(12);

    // left column
    auto left = area.removeFromLeft(160);
    int y = left.getY();
    for (int i = 0; i < categoryButtons.size(); ++i)
    {
        categoryButtons[i]->setBounds(left.getX() + 8, y + 12 + i * 56, left.getWidth() - 16, 44);
    }

    // center: keyword grid
    auto center = area.removeFromLeft(getWidth() - 160 - 320).reduced(12);
    int cols = 3;
    int tileW = (center.getWidth() - (cols - 1) * 8) / cols;
    int tileH = 48;
    int idx = 0;
    int cx = center.getX();
    int cy = center.getY() + 8;
    for (auto* tb : keywordButtons)
    {
        int col = idx % cols;
        int row = idx / cols;
        tb->setBounds(cx + col * (tileW + 8), cy + row * (tileH + 8), tileW, tileH);
        ++idx;
    }

    // right: prompt + controls
    auto right = getContentComponent()->getLocalBounds().removeFromRight(320).reduced(12);
    promptEditor.setBounds(right.removeFromTop(48));
    applyPromptBtn.setBounds(right.removeFromTop(40).reduced(0, 8));
    keywordSummaryBox.setBounds(right.removeFromTop(80).reduced(0, 8));
    intensityLabel.setBounds(right.removeFromTop(22));
    intensityKnob.setBounds(right.removeFromTop(120).withSizeKeepingCentre(120, 120));
    randomizeBtn.setBounds(right.removeFromBottom(40).withWidth(120).withX(right.getX() + 12));
    generateBtn.setBounds(right.removeFromBottom(48).withSizeKeepingCentre(200, 44));
    savePresetBtn.setBounds(right.removeFromBottom(36).withWidth(120).withX(right.getX() + 12));
    resetBtn.setBounds(right.removeFromBottom(36).withWidth(120).withX(right.getX() + 12));
}
