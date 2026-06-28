#include "PluginEditor.h"

namespace {
namespace Theme {
constexpr int woodWidth = 90;
constexpr int outerMargin = 18;
constexpr int sectionGap = 16;
constexpr int titleAreaHeight = 82;
constexpr int modulePadding = 12;
constexpr int valueBoxHeight = 22;
constexpr int sectionHeaderHeight = 30;
constexpr auto panelTop = 104;

constexpr float titleFont = 44.0f;
constexpr float subtitleFont = 12.5f;
constexpr float sectionTitleFont = 15.4f;
constexpr float controlLabelFont = 14.8f;
constexpr float compactLabelFont = 13.2f;
constexpr float valueFont = 13.8f;

const auto panelBackground = juce::Colour(0xffbebeb4);
const auto panelTopHighlight = juce::Colour(0xffdeded2);
const auto panelBottomShadow = juce::Colour(0xff9d9d94);
const auto sectionBackground = juce::Colour(0xffaaa99f);
const auto sectionHeader = juce::Colour(0xff939188);
const auto sectionBorder = juce::Colour(0xff5b574b);
const auto sectionSoftBorder = juce::Colour(0xff736e5f);
const auto sectionInnerShadow = juce::Colour(0x332b2b26);
const auto textPrimary = juce::Colour(0xff11110f);
const auto textSecondary = juce::Colour(0xff282823);
const auto textMuted = juce::Colour(0xff373732);
const auto accentGold = juce::Colour(0xff96783c);
const auto valueFill = juce::Colour(0xff514a3c);
const auto valueText = juce::Colour(0xffebe8d7);
const auto woodDark = juce::Colour(0xff170905);
const auto woodMid = juce::Colour(0xff442512);
const auto woodHighlight = juce::Colour(0xff7b4b28);

juce::FontOptions uiFont(float size, int style = juce::Font::plain)
{
    return juce::FontOptions("Segoe UI", size, style);
}

juce::Font trackedLabelFont(float size, int style = juce::Font::bold)
{
    return juce::Font(uiFont(size, style)).withExtraKerningFactor(0.035f);
}
}

namespace Fmt {
    static bool isTime(const juce::String& pid)
    {
        return pid.containsIgnoreCase("attack") || pid.containsIgnoreCase("decay")
            || pid.containsIgnoreCase("release") || pid == "glideTime";
    }

    static juce::String time(double s)
    {
        return s < 1.0 ? juce::String(juce::roundToInt(s * 1000.0)) + " ms"
                       : juce::String(s, 2) + " s";
    }

    static juce::String value(const juce::String& pid, double v)
    {
        if (isTime(pid))                              return time(v);
        if (pid == "filterCutoff")                    return juce::String(juce::roundToInt(v)) + " Hz";
        if (pid.containsIgnoreCase("detune"))         return (v > 0.0 ? juce::String("+") : juce::String()) + juce::String(v, 2) + " st";
        if (pid == "pitchBendRange")                  return juce::String(v, 2) + " st";
        if (pid == "fineTune") {
            const int c = juce::roundToInt(v);
            return (c > 0 ? juce::String("+") : juce::String()) + juce::String(c) + " c";
        }
        if (pid == "lfoRate")                         return juce::String(v, 2) + " Hz";
        if (pid == "osc1PulseWidth")                  return juce::String(juce::roundToInt(v * 100.0)) + "%";
        return juce::String(v, 2);
    }

    static double fromText(const juce::String& pid, const juce::String& text)
    {
        const auto t = text.trim();
        if (isTime(pid)) {
            if (t.endsWithIgnoreCase("ms")) return t.dropLastCharacters(2).trim().getDoubleValue() / 1000.0;
            if (t.endsWithIgnoreCase("s"))  return t.dropLastCharacters(1).trim().getDoubleValue();
            const double v = t.getDoubleValue();
            return v > 10.0 ? v / 1000.0 : v;
        }
        if (pid == "filterCutoff" || pid == "lfoRate")
            return t.endsWithIgnoreCase("hz") ? t.dropLastCharacters(2).trim().getDoubleValue() : t.getDoubleValue();
        if (pid.containsIgnoreCase("detune") || pid == "pitchBendRange")
            return t.endsWithIgnoreCase("st") ? t.dropLastCharacters(2).trim().getDoubleValue() : t.getDoubleValue();
        if (pid == "fineTune")
            return t.endsWithIgnoreCase("c") ? t.dropLastCharacters(1).trim().getDoubleValue() : t.getDoubleValue();
        if (pid == "osc1PulseWidth") {
            const double value = t.endsWithChar('%') ? t.dropLastCharacters(1).trim().getDoubleValue() : t.getDoubleValue();
            return value > 1.0 ? value / 100.0 : value;
        }
        return t.getDoubleValue();
    }
}

void drawScrew(juce::Graphics& g, juce::Point<float> centre)
{
    auto r = juce::Rectangle<float>(10.0f, 10.0f).withCentre(centre);
    g.setColour(juce::Colour(0x55000000));
    g.fillEllipse(r.translated(0.0f, 1.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffd88a), r.getX(), r.getY(),
                                           juce::Colour(0xff4a2b0d), r.getRight(), r.getBottom(), false));
    g.fillEllipse(r);
    g.setColour(juce::Colour(0xff0a0a08));
    g.drawEllipse(r, 1.0f);
    g.setColour(juce::Colour(0xffffefbd));
    g.fillEllipse(juce::Rectangle<float>(1.6f, 1.6f).withCentre({centre.x - 1.8f, centre.y - 2.0f}));
    g.setColour(juce::Colour(0xdd0e0e0b));
    g.drawLine(r.getX() + 2.0f, centre.y + 0.4f, r.getRight() - 2.0f, centre.y - 0.4f, 1.0f);
}

void drawWoodPanel(juce::Graphics& g, juce::Rectangle<int> wood, bool leftSide)
{
    const auto wf = wood.toFloat();
    g.setGradientFill(juce::ColourGradient(Theme::woodDark, wf.getX(), 0.0f,
                                           Theme::woodMid, wf.getRight(), 0.0f, false));
    g.fillRect(wood);

    for (int y = 6; y < wood.getBottom(); y += 13) {
        const auto wobble = static_cast<float>((y / 13) % 7);
        g.setColour(juce::Colour(0x5c130703));
        g.drawLine(static_cast<float>(wood.getX() + 7), static_cast<float>(y),
                   static_cast<float>(wood.getRight() - 7), static_cast<float>(y + 5 + wobble), 1.0f);
        g.setColour(Theme::woodHighlight.withAlpha(0.16f));
        g.drawLine(static_cast<float>(wood.getX() + 14), static_cast<float>(y + 5),
                   static_cast<float>(wood.getRight() - 12), static_cast<float>(y + 10), 0.75f);
    }

    g.setGradientFill(juce::ColourGradient(juce::Colour(0x88000000), leftSide ? wf.getRight() - 12.0f : wf.getX(), 0.0f,
                                           juce::Colour(0x00000000), leftSide ? wf.getX() : wf.getRight(), 0.0f, false));
    g.fillRect(wf);
    g.setColour(juce::Colour(0x44b98b5b));
    g.drawLine(leftSide ? wf.getRight() - 4.0f : wf.getX() + 4.0f, wf.getY(),
               leftSide ? wf.getRight() - 4.0f : wf.getX() + 4.0f, wf.getBottom(), 1.0f);

    g.setColour(juce::Colour(0xff1b0d08));
    g.drawRect(wood, 1);
    drawScrew(g, {wf.getCentreX(), wf.getY() + 28.0f});
    drawScrew(g, {wf.getCentreX(), wf.getBottom() - 28.0f});
}

void drawModuleScrews(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    const auto f = bounds.toFloat();
    drawScrew(g, {f.getX() + 10.0f, f.getY() + 10.0f});
    drawScrew(g, {f.getRight() - 10.0f, f.getY() + 10.0f});
}

void drawInsetRow(juce::Graphics& g, juce::Rectangle<int> row)
{
    const auto r = row.toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x14ffffff), r.getX(), r.getY(),
                                           juce::Colour(0x18000000), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(Theme::sectionSoftBorder.withAlpha(0.22f));
    g.drawRoundedRectangle(r, 4.0f, 0.8f);
}

template <typename ComponentList>
void setComponentBounds(ComponentList& components, int& index, juce::Rectangle<int> bounds)
{
    if (index < 0 || static_cast<size_t>(index) >= components.size()) {
        DBG("LadderVoice editor layout skipped missing component at index " << index);
        ++index;
        return;
    }

    if (auto* component = components[static_cast<size_t>(index)].get())
        component->setBounds(bounds);

    ++index;
}
}

LadderVoiceAudioProcessorEditor::LadderVoiceAudioProcessorEditor(LadderVoiceAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&analogLookAndFeel);

    // CONTROLLERS — buttons[0..2], sliders[0..2], combos[0]
    addToggle("Glide",     "glideEnabled");      // buttons[0]
    addKnob("Glide Time",  "glideTime");         // sliders[0]
    addToggle("Legato",    "legato");            // buttons[1]
    addToggle("Retrigger", "retrigger");         // buttons[2]
    addCombo("Priority", "notePriority", {"Low", "Last", "High"}); // combos[0]
    addKnob("Bend Range",  "pitchBendRange");    // sliders[1]
    addKnob("Fine Tune",   "fineTune");          // sliders[2]

    playModeCombo = std::make_unique<juce::ComboBox>();
    playModeCombo->addItem("MONO", 1);
    playModeCombo->addItem("POLY 4", 2);
    addAndMakeVisible(*playModeCombo);

    playModeLabel = std::make_unique<juce::Label>();
    playModeLabel->setText("MODE", juce::dontSendNotification);
    playModeLabel->setJustificationType(juce::Justification::centred);
    playModeLabel->setColour(juce::Label::textColourId, Theme::textMuted);
    playModeLabel->setFont(Theme::trackedLabelFont(Theme::compactLabelFont));
    playModeLabel->setSize(100, 15);
    addAndMakeVisible(*playModeLabel);
    playModeAttachment = std::make_unique<ComboBoxAttachment>(processor.parameters, "playMode", *playModeCombo);

    // OSC BANK — buttons[3..6], combos[1..6], sliders[3..7]
    const juce::StringArray waveforms {"Tri", "Tri-Saw", "Saw", "Rev Saw", "Square", "Wide", "Narrow"};
    const juce::StringArray ranges {"LO", "32'", "16'", "8'", "4'", "2'"};
    for (int osc = 1; osc <= 3; ++osc) {
        addToggle("Osc " + juce::String(osc), "osc" + juce::String(osc) + "Enabled");
        addCombo("Wave", "osc" + juce::String(osc) + "Waveform", waveforms);
        addCombo("Range", "osc" + juce::String(osc) + "Range", ranges);
        if (osc > 1) addKnob("Detune", "osc" + juce::String(osc) + "Detune");
        if (osc == 3) addToggle("Keyboard", "osc3KeyboardTracking");
        addKnob("Level", "osc" + juce::String(osc) + "Level");
    }
    // OSC extra — placed in osc1 row gap. Analog Drift stays in APVTS but is hidden from the panel.
    addKnob("PW",    "osc1PulseWidth");          // sliders[8]
    auto& analogDrift = addKnob("Analog Drift", "analogDrift"); // sliders[9], hidden for state compatibility
    analogDrift.setVisible(false);
    if (auto it = attachedLabels.find(&analogDrift); it != attachedLabels.end() && it->second != nullptr)
        it->second->setVisible(false);
    if (combos.size() > 1 && combos[1] != nullptr)
        combos[1]->onChange = [this] { updatePulseWidthControlState(); };

    // MIXER — sliders[10..11], buttons[7], combos[7]
    addKnob("Drive", "mixerDrive");              // sliders[10]
    addKnob("Noise Level", "noiseLevel");        // sliders[11]

    // FILTER — sliders[12..15], combos[8]
    addKnob("Cutoff",   "filterCutoff");         // sliders[12]
    addKnob("Emphasis", "filterResonance");      // sliders[13]
    addKnob("Contour",  "filterContour");        // sliders[14]
    addKnob("Drive",    "filterDrive");          // sliders[15]

    // FILTER CONTOUR — sliders[16..19]
    addKnob("Attack",  "filterAttack");          // sliders[16]
    addKnob("Decay",   "filterDecay");           // sliders[17]
    addKnob("Sustain", "filterSustain");         // sliders[18]
    addKnob("Release", "filterRelease");         // sliders[19]

    // MODULATION — buttons[8], sliders[20..22], combos[9]
    addKnob("LFO Rate", "lfoRate");              // sliders[20]
    addKnob("LFO Amt",  "lfoAmount");            // sliders[21]
    addCombo("", "lfoDestination", {"Pitch", "Filter", "Pulse Width"}); // combos[9]
    addKnob("Wheel Depth", "modWheelAmount");    // sliders[22]

    // LOUDNESS CONTOUR — sliders[23..26]
    addKnob("Attack",  "loudnessAttack");        // sliders[23]
    addKnob("Decay",   "loudnessDecay");         // sliders[24]
    addKnob("Sustain", "loudnessSustain");       // sliders[25]
    addKnob("Release", "loudnessRelease");       // sliders[26]

    // OUTPUT — sliders[27..28]
    addKnob("Volume", "masterVolume");           // sliders[27]
    auto& outputDriveKnob = addKnob("Output Drive", "outputDrive"); // sliders[28], hidden for state compatibility
    outputDriveKnob.setVisible(false);
    if (auto it = attachedLabels.find(&outputDriveKnob); it != attachedLabels.end() && it->second != nullptr)
        it->second->setVisible(false);

    // Preset strip (UI shell only — no real preset storage)
    auto stylePresetBtn = [](juce::TextButton& btn, const juce::String& text) {
        btn.setButtonText(text);
        btn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xffd8d5c7));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffc4bea8));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xff11110e));
        btn.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xff11110e));
    };
    stylePresetBtn(presetPrevButton, "<");
    stylePresetBtn(presetNextButton, ">");
    addAndMakeVisible(presetPrevButton);
    addAndMakeVisible(presetNextButton);

    presetNameLabel.setText("Init", juce::dontSendNotification);
    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setColour(juce::Label::textColourId, Theme::textPrimary);
    presetNameLabel.setColour(juce::Label::backgroundColourId, juce::Colour(0xffd4d2c7));
    presetNameLabel.setColour(juce::Label::outlineColourId, Theme::sectionSoftBorder.withAlpha(0.55f));
    presetNameLabel.setFont(Theme::uiFont(12.5f, juce::Font::bold));
    addAndMakeVisible(presetNameLabel);

    setSize(1460, 780);
}

LadderVoiceAudioProcessorEditor::~LadderVoiceAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

bool LadderVoiceAudioProcessorEditor::hasParameter(const juce::String& parameterId) const
{
    if (processor.parameters.getParameter(parameterId) != nullptr)
        return true;

    DBG("LadderVoice editor missing parameter: " << parameterId);
    return false;
}

juce::Slider& LadderVoiceAudioProcessorEditor::addKnob(const juce::String& text, const juce::String& parameterId)
{
    auto slider = std::make_unique<juce::Slider>(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow);
    slider->setName(parameterId);
    int valueWidth = 62;
    if (parameterId == "filterCutoff") {
        valueWidth = 92;
    } else if (parameterId.containsIgnoreCase("attack") || parameterId.containsIgnoreCase("decay")
               || parameterId.containsIgnoreCase("release") || parameterId == "glideTime") {
        valueWidth = 74;
    } else if (parameterId.containsIgnoreCase("detune") || parameterId == "pitchBendRange") {
        valueWidth = 78;
    } else if (parameterId == "lfoRate") {
        valueWidth = 78;
    } else if (parameterId == "analogDrift" || parameterId == "lfoAmount" || parameterId == "modWheelAmount") {
        valueWidth = 70;
    } else if (parameterId == "fineTune") {
        valueWidth = 66;
    }
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, valueWidth, Theme::valueBoxHeight);
    slider->setColour(juce::Slider::textBoxTextColourId, Theme::valueText);
    slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00090a08));
    slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x002c281e));
    addAndMakeVisible(*slider);

    auto label = std::make_unique<juce::Label>();
    label->setText(text, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, Theme::textSecondary);
    label->setFont(Theme::trackedLabelFont(Theme::controlLabelFont));
    label->setSize(juce::jmax(64, juce::jmax(valueWidth + 20, text.length() * 9 + 14)), 16);
    auto* sliderPtr = slider.get();
    auto* labelPtr = label.get();
    addAndMakeVisible(*label);

    if (hasParameter(parameterId)) {
        sliderAttachments.push_back(std::make_unique<SliderAttachment>(processor.parameters, parameterId, *slider));
        // Re-set after attachment — JUCE 8 SliderAttachment overwrites these in its constructor
        slider->textFromValueFunction  = [parameterId](double v)             { return Fmt::value(parameterId, v); };
        slider->valueFromTextFunction  = [parameterId](const juce::String& t){ return Fmt::fromText(parameterId, t); };
    } else {
        slider->setEnabled(false);
        slider->setTooltip("Parameter unavailable");
        slider->textFromValueFunction  = [parameterId](double v)             { return Fmt::value(parameterId, v); };
    }

    auto& ref = *slider;
    sliders.push_back(std::move(slider));
    labels.push_back(std::move(label));
    attachedLabels[sliderPtr] = labelPtr;
    return ref;
}

juce::ToggleButton& LadderVoiceAudioProcessorEditor::addToggle(const juce::String& text, const juce::String& parameterId)
{
    auto button = std::make_unique<juce::ToggleButton>(text);
    addAndMakeVisible(*button);

    if (hasParameter(parameterId)) {
        buttonAttachments.push_back(std::make_unique<ButtonAttachment>(processor.parameters, parameterId, *button));
    } else {
        button->setEnabled(false);
        button->setTooltip("Parameter unavailable");
    }

    auto& ref = *button;
    buttons.push_back(std::move(button));
    return ref;
}

juce::ComboBox& LadderVoiceAudioProcessorEditor::addCombo(const juce::String& text, const juce::String& parameterId, const juce::StringArray& items)
{
    auto combo = std::make_unique<juce::ComboBox>();
    for (int i = 0; i < items.size(); ++i) combo->addItem(items[i], i + 1);
    addAndMakeVisible(*combo);

    auto label = std::make_unique<juce::Label>();
    label->setText(text, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, Theme::textMuted);
    label->setFont(Theme::trackedLabelFont(Theme::controlLabelFont));
    label->setSize(100, 15);
    auto* comboPtr = combo.get();
    auto* labelPtr = label.get();
    addAndMakeVisible(*label);

    if (hasParameter(parameterId)) {
        comboAttachments.push_back(std::make_unique<ComboBoxAttachment>(processor.parameters, parameterId, *combo));
    } else {
        combo->setEnabled(false);
        combo->setTooltip("Parameter unavailable");
    }

    auto& ref = *combo;
    combos.push_back(std::move(combo));
    labels.push_back(std::move(label));
    attachedLabels[comboPtr] = labelPtr;
    return ref;
}

void LadderVoiceAudioProcessorEditor::positionControlLabels()
{
    for (const auto& item : attachedLabels) {
        auto* component = item.first;
        auto* label = item.second;
        if (component == nullptr || label == nullptr || ! component->isVisible())
            continue;

        const auto bounds = component->getBounds();
        if (bounds.isEmpty())
            continue;

        bool combo = false;
        for (const auto& comboComponent : combos) {
            if (comboComponent.get() == component) {
                combo = true;
                break;
            }
        }
        const int labelHeight = combo ? 15 : 16;
        const int gap = 1;
        const int labelWidth = juce::jmax(bounds.getWidth(), label->getWidth());
        const int x = bounds.getCentreX() - labelWidth / 2;
        int y = bounds.getY() - labelHeight - gap;
        if (component->getName() == "filterCutoff")
            y = bounds.getY() - 4;

        label->setBounds(x, y, labelWidth, labelHeight);
        label->toFront(false);
    }

    if (playModeCombo != nullptr && playModeLabel != nullptr) {
        const auto bounds = playModeCombo->getBounds();
        const int labelWidth = juce::jmax(bounds.getWidth(), playModeLabel->getWidth());
        playModeLabel->setBounds(bounds.getCentreX() - labelWidth / 2,
                                 bounds.getY() - 17,
                                 labelWidth,
                                 15);
        playModeLabel->toFront(false);
    }
}

void LadderVoiceAudioProcessorEditor::updatePulseWidthControlState()
{
    if (combos.size() <= 1 || sliders.size() <= 8 || combos[1] == nullptr || sliders[8] == nullptr)
        return;

    const int waveformIndex = combos[1]->getSelectedItemIndex();
    const bool pulseCapable = waveformIndex >= 4 && waveformIndex <= 6;
    auto* pulseWidthSlider = sliders[8].get();
    pulseWidthSlider->setVisible(pulseCapable);
    pulseWidthSlider->setEnabled(pulseCapable);
    pulseWidthSlider->setAlpha(1.0f);

    if (auto it = attachedLabels.find(pulseWidthSlider); it != attachedLabels.end() && it->second != nullptr) {
        it->second->setVisible(pulseCapable);
        it->second->setEnabled(pulseCapable);
        it->second->setAlpha(1.0f);
    }
}

void LadderVoiceAudioProcessorEditor::drawSection(juce::Graphics& g, juce::Rectangle<int> bounds, const juce::String& title)
{
    const auto f = bounds.toFloat();
    g.setColour(juce::Colour(0x24000000));
    g.fillRoundedRectangle(f.translated(0.0f, 2.0f), 6.0f);
    g.setGradientFill(juce::ColourGradient(Theme::sectionBackground.brighter(0.13f), f.getX(), f.getY(),
                                           Theme::sectionBackground.darker(0.04f), f.getX(), f.getBottom(), false));
    g.fillRoundedRectangle(f, 6.0f);
    g.setColour(juce::Colour(0x0affffff));
    for (int y = bounds.getY() + 12; y < bounds.getBottom() - 10; y += 8)
        g.drawLine(static_cast<float>(bounds.getX() + 10), static_cast<float>(y),
                   static_cast<float>(bounds.getRight() - 10), static_cast<float>(y), 0.35f);
    g.setColour(Theme::sectionBorder.withAlpha(0.34f));
    g.drawRoundedRectangle(f.reduced(0.5f), 6.0f, 0.75f);
    g.setColour(juce::Colour(0x20ffffff));
    g.drawRoundedRectangle(f.reduced(2.0f), 4.5f, 0.55f);
    drawModuleScrews(g, bounds);

    auto headerBounds = bounds.removeFromTop(Theme::sectionHeaderHeight + 4);
    const auto headerText = headerBounds.reduced(18, 4);
    g.setColour(juce::Colour(0x18000000));
    g.drawLine(static_cast<float>(headerText.getX()),
               static_cast<float>(headerText.getBottom() - 1),
               static_cast<float>(headerText.getRight()),
               static_cast<float>(headerText.getBottom() - 1),
               0.8f);
    g.setColour(Theme::textPrimary.withAlpha(0.96f));
    g.setFont(Theme::uiFont(Theme::sectionTitleFont, juce::Font::bold));
    g.drawFittedText(title.toUpperCase(), headerText, juce::Justification::centred, 1, 0.88f);
    g.setColour(Theme::accentGold.withAlpha(0.24f));
    g.drawLine(f.getX() + 12.0f, static_cast<float>(headerBounds.getBottom() - 1),
               f.getRight() - 12.0f, static_cast<float>(headerBounds.getBottom() - 1), 0.85f);
}

void LadderVoiceAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff15130f));
    auto leftWood = getLocalBounds().removeFromLeft(Theme::woodWidth);
    auto rightWood = getLocalBounds().removeFromRight(Theme::woodWidth);

    drawWoodPanel(g, leftWood, true);
    drawWoodPanel(g, rightWood, false);

    auto panel = getLocalBounds().withTrimmedLeft(Theme::woodWidth).withTrimmedRight(Theme::woodWidth).reduced(16, 18);
    g.setGradientFill(juce::ColourGradient(Theme::panelTopHighlight, static_cast<float>(panel.getX()), static_cast<float>(panel.getY()),
                                           Theme::panelBottomShadow, static_cast<float>(panel.getX()), static_cast<float>(panel.getBottom()), false));
    g.fillRoundedRectangle(panel.toFloat(), 7.0f);
    g.setColour(juce::Colour(0x20ffffff));
    for (int y = panel.getY() + 8; y < panel.getBottom() - 8; y += 6)
        g.drawLine(static_cast<float>(panel.getX() + 10), static_cast<float>(y),
                   static_cast<float>(panel.getRight() - 10), static_cast<float>(y), 0.35f);
    g.setColour(Theme::sectionBorder.withAlpha(0.72f));
    g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 7.0f, 1.1f);
    g.setColour(Theme::sectionSoftBorder.withAlpha(0.18f));
    g.drawRoundedRectangle(panel.toFloat().reduced(6.0f), 5.0f, 0.65f);

    drawScrew(g, {static_cast<float>(panel.getX() + 18), static_cast<float>(panel.getY() + 18)});
    drawScrew(g, {static_cast<float>(panel.getRight() - 18), static_cast<float>(panel.getY() + 18)});
    drawScrew(g, {static_cast<float>(panel.getX() + 18), static_cast<float>(panel.getBottom() - 18)});
    drawScrew(g, {static_cast<float>(panel.getRight() - 18), static_cast<float>(panel.getBottom() - 18)});

    for (int y = panel.getY() + 56; y < panel.getBottom() - 20; y += 7) {
        g.setColour(juce::Colour(0x0a000000));
        g.drawLine(static_cast<float>(panel.getX() + 12), static_cast<float>(y),
                   static_cast<float>(panel.getRight() - 12), static_cast<float>(y), 1.0f);
    }

    // Title / logo
    auto titlePlate = juce::Rectangle<int>(112, 26, 370, 56);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x11ffffff), static_cast<float>(titlePlate.getX()), static_cast<float>(titlePlate.getY()),
                                           juce::Colour(0x08000000), static_cast<float>(titlePlate.getRight()), static_cast<float>(titlePlate.getBottom()), false));
    g.fillRoundedRectangle(titlePlate.toFloat(), 4.0f);
    g.setFont(Theme::uiFont(Theme::titleFont, juce::Font::bold));
    g.setColour(juce::Colour(0x33000000));
    g.drawText("Ladder Voice", titlePlate.reduced(12, 2).translated(1, 1), juce::Justification::centredLeft);
    g.setColour(Theme::textPrimary);
    g.drawText("Ladder Voice", titlePlate.reduced(10, 2), juce::Justification::centredLeft);
    g.setColour(Theme::textPrimary.withAlpha(0.90f));
    g.setColour(Theme::textMuted.withAlpha(0.92f));
    g.setFont(Theme::uiFont(Theme::subtitleFont, juce::Font::bold));
    g.drawText("ANALOG-STYLE SYNTH", 120, 66, 260, 18, juce::Justification::centredLeft);

    // Preset strip background (right side of title bar)
    auto presetStrip = juce::Rectangle<int>(694, 36, 322, 34);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffc7c5ba), static_cast<float>(presetStrip.getX()), static_cast<float>(presetStrip.getY()),
                                           juce::Colour(0xffaaa89d), static_cast<float>(presetStrip.getX()), static_cast<float>(presetStrip.getBottom()), false));
    g.fillRoundedRectangle(presetStrip.toFloat(), 4.0f);
    g.setColour(Theme::sectionBorder.withAlpha(0.38f));
    g.drawRoundedRectangle(presetStrip.toFloat(), 4.0f, 0.85f);
    g.setColour(Theme::textMuted.withAlpha(0.70f));
    g.setFont(Theme::uiFont(Theme::compactLabelFont, juce::Font::bold));
    g.drawText("PRESET", presetStrip.removeFromLeft(52).reduced(4, 0), juce::Justification::centred);

    g.setColour(Theme::sectionBorder.withAlpha(0.35f));
    g.drawLine(116.0f, 90.0f, 1344.0f, 90.0f, 0.9f);

    // Main sections
    drawSection(g, {110, Theme::panelTop, 160, 500}, "CONTROLLERS");
    drawSection(g, {284, Theme::panelTop, 500, 486}, "OSCILLATOR BANK");
    drawSection(g, {800, Theme::panelTop, 150, 242}, "MIXER");
    drawSection(g, {966, Theme::panelTop, 192, 250}, "FILTER");
    drawSection(g, {1174, Theme::panelTop, 176, 250}, "MODULATION");
    drawSection(g, {800, 360, 150, 230}, "OUTPUT");
    drawSection(g, {966, 360, 192, 230}, "FILTER CONTOUR");
    drawSection(g, {1174, 360, 176, 230}, "LOUDNESS CONTOUR");

    // Oscillator bank row insets and dividers
    drawInsetRow(g, {302, 140, 462, 132});
    drawInsetRow(g, {302, 284, 462, 132});
    drawInsetRow(g, {302, 428, 462, 132});
    g.setColour(Theme::sectionBorder.withAlpha(0.24f));
    g.drawLine(306.0f, 281.0f, 764.0f, 281.0f, 0.9f);
    g.drawLine(306.0f, 421.0f, 764.0f, 421.0f, 0.9f);

    // Lower chassis / nameplate strip
    auto lower = juce::Rectangle<int>(110, 608, 1240, 66);
    g.setColour(juce::Colour(0x70000000));
    g.fillRoundedRectangle(lower.toFloat().translated(0.0f, 3.0f), 5.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffc8c7bd), static_cast<float>(lower.getX()), static_cast<float>(lower.getY()),
                                           juce::Colour(0xff929188), static_cast<float>(lower.getX()), static_cast<float>(lower.getBottom()), false));
    g.fillRoundedRectangle(lower.toFloat(), 5.0f);
    g.setColour(juce::Colour(0x88fffff8));
    g.drawLine(static_cast<float>(lower.getX() + 8), static_cast<float>(lower.getY() + 3),
               static_cast<float>(lower.getRight() - 8), static_cast<float>(lower.getY() + 3), 1.0f);
    g.setColour(Theme::sectionBorder.withAlpha(0.55f));
    g.drawRoundedRectangle(lower.toFloat(), 5.0f, 1.0f);
    auto namePlate = lower.reduced(18, 10).removeFromLeft(280);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffbdbbb1), static_cast<float>(namePlate.getX()), static_cast<float>(namePlate.getY()),
                                           juce::Colour(0xff9d9b91), static_cast<float>(namePlate.getX()), static_cast<float>(namePlate.getBottom()), false));
    g.fillRoundedRectangle(namePlate.toFloat(), 3.0f);
    drawScrew(g, {static_cast<float>(namePlate.getX() + 10), static_cast<float>(namePlate.getCentreY())});
    drawScrew(g, {static_cast<float>(namePlate.getRight() - 10), static_cast<float>(namePlate.getCentreY())});
    g.setColour(Theme::sectionBorder.withAlpha(0.45f));
    g.drawRoundedRectangle(namePlate.toFloat(), 3.0f, 0.85f);
    g.setColour(Theme::textMuted.withAlpha(0.88f));
    g.setFont(Theme::uiFont(Theme::compactLabelFont, juce::Font::bold));
    g.drawText("LADDER VOICE  /  CONTROL SURFACE", namePlate.reduced(10, 0), juce::Justification::centredLeft);
    for (int i = 0; i < 64; ++i) {
        const int x = lower.getX() + 330 + i * 14;
        auto vent = juce::Rectangle<int>(x, lower.getY() + 22, 7, 22);
        g.setColour(juce::Colour(0x26000000));
        g.fillRoundedRectangle(vent.expanded(1, 2).toFloat(), 2.0f);
        g.setColour(juce::Colour(0xff10100e));
        g.fillRoundedRectangle(vent.toFloat(), 2.0f);
        g.setColour(juce::Colour(0x38ffffff));
        g.drawRoundedRectangle(vent.toFloat(), 2.0f, 0.7f);
    }
}

void LadderVoiceAudioProcessorEditor::resized()
{
    int s = 0, b = 0, c = 0;

    // CONTROLLERS — section {110,104,160,486}
    setComponentBounds(buttons, b, {130, 144, 122, 28});       // [b0] glideEnabled
    if (playModeCombo != nullptr) playModeCombo->setBounds({126, 190, 124, 26});
    setComponentBounds(sliders, s, {138, 234, 98, 76});        // [s0] glideTime      → render 66px
    setComponentBounds(buttons, b, {130, 318, 122, 26});       // [b1] legato
    setComponentBounds(buttons, b, {130, 350, 122, 26});       // [b2] retrigger
    setComponentBounds(combos, c, {126, 394, 124, 24});        // [c0] notePriority
    setComponentBounds(sliders, s, {138, 436, 98, 76});        // [s1] pitchBendRange → render 66px
    setComponentBounds(sliders, s, {138, 532, 98, 62});        // [s2] fineTune
    // OSC BANK — columns: Enable@298 Wave@398 Range@500 PW/Detune@574 Level@680
    // Knob slots w=102 h=124 → knobBox 98×102 → render 98px, medium strip (98≥76)
    for (int osc = 0; osc < 3; ++osc) {
        const int rowY = 154 + osc * 144;
        setComponentBounds(buttons, b, {300, rowY,      88, 26});            // [b3,4,5] oscEnabled
        setComponentBounds(combos,  c, {398, rowY + 2,  96, 26});            // [c1,3,5] wave
        setComponentBounds(combos,  c, {500, rowY + 2,  74, 26});            // [c2,4,6] range
        if (osc > 0) setComponentBounds(sliders, s, {578, rowY + 8, 92, 100}); // [s4,6] detune
        if (osc == 2) setComponentBounds(buttons, b, {300, rowY + 54, 88, 26}); // [b6] keyboard
        setComponentBounds(sliders, s, {674, rowY + 8, 92, 100});           // [s3,5,7] level
    }
    setComponentBounds(sliders, s, {578, 162, 92, 100});      // [s8] osc1PulseWidth
    setComponentBounds(sliders, s, {0, 0, 0, 0});              // [s9] analogDrift hidden, APVTS kept

    // MIXER — section {800,104,150,240}
    // mixerDrive: knobBox 140×90 → render 90px, large strip (140≥100)
    setComponentBounds(sliders, s, {812, 156, 126,  88});      // [s10] mixerDrive
    setComponentBounds(sliders, s, {814, 280, 118,  64});      // [s11] noiseLevel

    // FILTER — section {966,104,192,260} — big Cutoff + secondary controls
    // Cutoff: knobBox 132×144 → render 132px large (~35% > 98px OSC knobs)
    setComponentBounds(sliders, s, {990,  140, 144, 132});     // [s12] filterCutoff
    setComponentBounds(sliders, s, {970,  288,  70,  60});     // [s13] filterResonance
    setComponentBounds(sliders, s, {1027, 288,  70,  60});     // [s14] filterContour
    setComponentBounds(sliders, s, {1084, 288,  70,  60});     // [s15] filterDrive

    // FILTER CONTOUR — section {966,366,192,224}
    // Row 1: knobBox 84×80 → render 80px medium; Row 2: knobBox 84×56 → render 56px
    setComponentBounds(sliders, s, {980,  414, 76,  78});      // [s16] filterAttack
    setComponentBounds(sliders, s, {1066, 414, 76,  78});      // [s17] filterDecay
    setComponentBounds(sliders, s, {980,  506, 76,  78});      // [s18] filterSustain
    setComponentBounds(sliders, s, {1066, 506, 76,  78});      // [s19] filterRelease

    // MODULATION — section {1174,104,176,336}
    setComponentBounds(sliders, s, {1186, 158,  72, 74});      // [s20] lfoRate
    setComponentBounds(sliders, s, {1266, 158,  72, 74});      // [s21] lfoAmount
    setComponentBounds(combos, c, {1186, 242, 152, 26});       // [c9]  lfoDestination
    setComponentBounds(sliders, s, {1222, 284,  80, 64});      // [s22] modWheelAmount

    // LOUDNESS CONTOUR — section {1174,456,176,134}
    // compact (loudness*): no reduction; w=76 → render min(76,86)=76px, medium strip (76≥76) ✓
    setComponentBounds(sliders, s, {1184, 414, 76,  78});      // [s23] loudnessAttack
    setComponentBounds(sliders, s, {1262, 414, 76,  78});      // [s24] loudnessDecay
    setComponentBounds(sliders, s, {1184, 506, 76,  78});      // [s25] loudnessSustain
    setComponentBounds(sliders, s, {1262, 506, 76,  78});      // [s26] loudnessRelease

    // OUTPUT — section {800,360,150,230}
    // Volume is the only visible control — centered vertically in the section content area.
    setComponentBounds(sliders, s, {804, 444, 142,  96});      // [s27] masterVolume — centered
    setComponentBounds(sliders, s, {0, 0, 0, 0});               // [s28] outputDrive hidden, APVTS kept

    // Preset strip
    presetPrevButton.setBounds(752, 42, 28, 26);
    presetNameLabel.setBounds(784, 42, 162, 26);
    presetNextButton.setBounds(950, 42, 28, 26);

    positionControlLabels();
    updatePulseWidthControlState();
}
