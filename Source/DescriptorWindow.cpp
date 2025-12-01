#include "DescriptorWindow.h"

DescriptorWindow::DescriptorWindow(juce::AudioProcessor& ownerProcessor)
    : juce::DocumentWindow ("Descriptor / Tagging",
                           juce::Colours::transparentBlack,
                           DocumentWindow::allButtons),
      owner(ownerProcessor)
{
    setUsingNativeTitleBar(false);
    setResizable(true, true);
    setResizeLimits(600, 400, 2500, 1500);

    intensityLabel.setText("Intensity", juce::dontSendNotification);
    intensityLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9aa0a6));
    intensityKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    intensityKnob.setRange(0.0, 1.0, 0.01);
    intensityKnob.setValue(0.65);

    addAndMakeVisible(promptEditor);
    promptEditor.setMultiLine(false);
    promptEditor.setReturnKeyStartsNewLine(false);
    promptEditor.setTextToShowWhenEmpty("e.g. deep boomy with slight distortion", juce::Colours::grey);
    promptEditor.addListener(this);

    addAndMakeVisible(applyPromptBtn);
    addAndMakeVisible(savePresetBtn);
    addAndMakeVisible(resetBtn);
    addAndMakeVisible(generateBtn);
    addAndMakeVisible(randomizeBtn);
    addAndMakeVisible(intensityLabel);
    addAndMakeVisible(intensityKnob);
    addAndMakeVisible(keywordSummaryBox);

    applyPromptBtn.addListener(this);
    savePresetBtn.addListener(this);
    resetBtn.addListener(this);
    generateBtn.addListener(this);
    randomizeBtn.addListener(this);

    // categories (left column)
    static const char* categories[] = { "Tone", "Length", "Character", "Texture", "Harmonic" };
    for (auto c : categories)
    {
        auto* b = new juce::TextButton(c);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xFF1E1F21));
        b->setColour(juce::TextButton::textColourOnId, juce::Colour(0xFFD6DCE0));
        addAndMakeVisible(b);
        categoryButtons.add(b);
        b->addListener(this);
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

    // make content component
    setContentNonOwned(new juce::Component(), true);
    setVisible(false);
    centreWithSize(1000, 700);
}

DescriptorWindow::~DescriptorWindow()
{
    applyPromptBtn.removeListener(this);
    savePresetBtn.removeListener(this);
    resetBtn.removeListener(this);
    generateBtn.removeListener(this);
    randomizeBtn.removeListener(this);
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

void DescriptorWindow::addKeyword(const juce::String& id, const juce::String& labelText)
{
    auto* tb = new juce::ToggleButton(labelText);
    tb->setToggleState(false, juce::dontSendNotification);
    tb->setColour(juce::ToggleButton::textColourId, juce::Colour(0xFFD6DCE0));
    tb->setColour(juce::ToggleButton::tickColourId, juce::Colour(0xFF4DB6A9));
    addAndMakeVisible(tb);
    tb->addListener(this);

    // store both in arrays/maps
    keywordButtons.add(tb);
    keywordButtonMap[id] = tb;
}

std::vector<std::pair<juce::String, float>> DescriptorWindow::getSelectedKeywords() const
{
    std::vector<std::pair<juce::String, float>> out;
    for (auto& kv : keywordButtonMap)
    {
        if (kv.second->getToggleState())
        {
            auto it = keywordIntensity.find(kv.first);
            float val = (it != keywordIntensity.end()) ? it->second : 0.6f;
            out.emplace_back(kv.first, val);
        }
    }
    return out;
}

float DescriptorWindow::getKeywordIntensity(const juce::String& id) const
{
    auto it = keywordIntensity.find(id);
    if (it != keywordIntensity.end()) return it->second;
    return 0.0f;
}

void DescriptorWindow::applyPrompt(const juce::String& text)
{
    auto lower = text.toLowerCase();
    // naive tokenization on spaces + punctuation
    juce::StringArray tokens;
    tokens.addTokens(lower, " ,.-;:()[]{}", "");
    tokens.trim();
    tokens.removeEmptyStrings();

    // try to match tokens to available keyword labels
    for (auto& kv : keywordButtonMap)
    {
        juce::String label = kv.second->getButtonText().toLowerCase();
        bool matched = false;
        for (auto& t : tokens)
        {
            if (label.contains(t) || t.contains(label) || label.startsWith(t) || t.startsWith(label))
            {
                matched = true;
                break;
            }
        }
        kv.second->setToggleState(matched, juce::sendNotification);
        if (matched)
            keywordIntensity.at(kv.first) = juce::jlimit(0.0f, 1.0f, keywordIntensity[kv.first] + 0.15f);
    }
}

void DescriptorWindow::buttonClicked(juce::Button* b)
{
    // category buttons do nothing fancy for now (could filter grid)
    for (auto* cat: categoryButtons)
        if (b == cat)
            return;

    if (b == &applyPromptBtn)
    {
        applyPrompt(promptEditor.getText());
    }
    else if (b == &resetBtn)
    {
        for (auto& kv : keywordButtonMap)
            kv.second->setToggleState(false, juce::sendNotification);
    }
    else if (b == &randomizeBtn)
    {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> d(0.35f, 1.0f);
        for (auto& kv : keywordButtonMap)
        {
            bool on = (d(gen) > 0.7f);
            kv.second->setToggleState(on, juce::sendNotification);
            keywordIntensity[kv.first] = d(gen);
        }
    }
    else if (b == &generateBtn)
    {
        // generation is triggered from main editor; here we simply close (user will press Generate in main or we can signal)
        // For convenience we close window and leave selections in place
        closeWindow();
    }
    else if (b == &savePresetBtn)
    {
        juce::FileChooser fc("Save descriptor preset", juce::File::getSpecialLocation(juce::File::userDocumentsDirectory), "*.json");
        if (fc.browseForFileToSave(true))
        {
            juce::File out = fc.getResult();
            if (!out.hasFileExtension("json")) out = out.withFileExtension(".json");

            juce::var root = juce::DynamicObject::create();
            juce::Array<juce::var> arr;
            for (auto& kv : keywordButtonMap)
            {
                if (kv.second->getToggleState())
                {
                    juce::DynamicObject::Ptr o = new juce::DynamicObject();
                    o->setProperty("id", kv.first);
                    o->setProperty("label", kv.second->getButtonText());
                    o->setProperty("intensity", (double)keywordIntensity.at(kv.first));
                    arr.add(o.get());
                }
            }
            root->setProperty("keywords", arr);
            juce::String json = juce::JSON::toString(root.get(), true);
            out.replaceWithText(json);
        }
    }
}

void DescriptorWindow::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    if (&editor == &promptEditor)
        applyPrompt(promptEditor.getText());
}

void DescriptorWindow::layoutComponents()
{
    // This function intentionally left blank; we rely on DocumentWindow's content component and parent
    // The editor will size it; but we need to place internal children.
    auto area = getLocalBounds().reduced(12);

    // Header not used here — we draw content
    // left column for categories
    auto left = area.removeFromLeft(160);
    int y = left.getY();
    for (int i = 0; i < categoryButtons.size(); ++i)
    {
        categoryButtons[i]->setBounds(left.getX() + 8, y + 12 + i * 56, left.getWidth() - 16, 44);
    }

    // center: keyword grid
    auto center = area.removeFromLeft(getWidth() - 160 - 320).reduced(12);
    int cols = 3; // columns in grid
    int tileW = (center.getWidth() - 16) / cols;
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
    auto right = getLocalBounds().removeFromRight(320).reduced(12);
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

void DescriptorWindow::closeButtonPressed()
{
    setVisible(false);
}
