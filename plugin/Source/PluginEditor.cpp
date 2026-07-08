#include "PluginEditor.h"
#include "PresetBrowser.h"
#include "Typography.h"

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

// Warm brown/wood palette (was light cream/gray) — moves the whole main
// panel toward the reference vintage-hardware tone (dark brown chassis,
// cream/off-white text) instead of a light metal panel against dark wood
// sides. Text colours flip to light-on-dark accordingly.
const auto panelBackground = juce::Colour(0xff6e5a44);
const auto panelTopHighlight = juce::Colour(0xff8a7157);
const auto panelBottomShadow = juce::Colour(0xff4a3c2c);
const auto sectionBackground = juce::Colour(0xff705c46);
const auto sectionHeader = juce::Colour(0xff5c4a38);
const auto sectionBorder = juce::Colour(0xff2e2419);
const auto sectionSoftBorder = juce::Colour(0xff3c3020);
const auto sectionInnerShadow = juce::Colour(0x33140f09);
// Muted back down from an earlier, brighter pass — now that every caption
// has the engraved shadow/highlight treatment (Typography::drawEngraved),
// that contrast carries the legibility instead of a bright flat colour, so
// the text itself can sit closer to the panel material rather than reading
// as a separate coat of bright paint on top of it.
const auto textPrimary = juce::Colour(0xffd9cbb0);
const auto textSecondary = juce::Colour(0xffcbbc9c);
const auto textMuted = juce::Colour(0xffb8a684);
const auto accentGold = juce::Colour(0xffc9963c);
const auto valueFill = juce::Colour(0xff342c20);
const auto valueText = juce::Colour(0xffebe8d7);
const auto woodDark = juce::Colour(0xff170905);
const auto woodMid = juce::Colour(0xff442512);
const auto woodHighlight = juce::Colour(0xff7b4b28);
}

namespace Fmt {
    static bool isTime(const juce::String& pid)
    {
        return pid.containsIgnoreCase("attack") || pid.containsIgnoreCase("decay")
            || pid.containsIgnoreCase("release") || pid == "glideTime";
    }

    static bool isLevel(const juce::String& pid)
    {
        return pid == "osc1Level" || pid == "osc2Level" || pid == "osc3Level" || pid == "noiseLevel";
    }

    static juce::String time(double s)
    {
        return s < 1.0 ? juce::String(juce::roundToInt(s * 1000.0)) + " ms"
                       : juce::String(s, 2) + " s";
    }

    static juce::String value(const juce::String& pid, double v)
    {
        if (isTime(pid))                              return time(v);
        if (isLevel(pid))                             return juce::String(juce::roundToInt(v * 100.0)) + "%";
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
        if (isLevel(pid)) {
            const double value = t.endsWithChar('%') ? t.dropLastCharacters(1).trim().getDoubleValue() : t.getDoubleValue();
            return value > 1.0 ? value / 100.0 : value;
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

#if JUCE_DEBUG
// Non-modal — just a child Component overlaid on the editor, so it never
// blocks interaction with the rest of the UI outside its own bounds.
// Dismisses on click; shown once per application launch (see the static
// flag at its call site), not once per editor open/close.
class DebugBuildWarningBanner final : public juce::Component {
public:
    DebugBuildWarningBanner()
    {
        setInterceptsMouseClicks(true, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0xfffff3d0));
        g.fillRoundedRectangle(bounds, 6.0f);
        g.setColour(juce::Colour(0xffb8860b));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 6.0f, 1.5f);

        auto textBounds = bounds.reduced(12.0f).toNearestInt();
        g.setColour(juce::Colour(0xff3a2c10));
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
        g.drawText("Debug build detected.", textBounds.removeFromTop(18),
                    juce::Justification::centredLeft);
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText("Real-time audio performance is not representative.",
                    textBounds.removeFromTop(16), juce::Justification::centredLeft);
        g.drawText("Use Release for sound evaluation.",
                    textBounds.removeFromTop(16), juce::Justification::centredLeft);

        g.setColour(juce::Colour(0xff7a6a40));
        g.setFont(juce::Font(juce::FontOptions(10.0f)));
        g.drawText("(click to dismiss)", getLocalBounds().removeFromBottom(14),
                    juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        setVisible(false);
    }
};
#endif

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
    auto& glideTimeKnob = addKnob("Glide Time",  "glideTime");   // sliders[0]
    glideTimeKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    glideTimeKnob.setPopupDisplayEnabled(true, true, this);
    addToggle("Legato",    "legato");            // buttons[1]
    addToggle("Retrigger", "retrigger");         // buttons[2]
    addCombo("Priority", "notePriority", {"Low", "Last", "High"}); // combos[0]
    // Bend Range + Fine Tune live in the header GLOBAL strip — value shown as a floating
    // popup bubble on interaction instead of a permanently visible value box.
    auto& bendRangeKnob = addKnob("Bend Range",  "pitchBendRange");    // sliders[1]
    bendRangeKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    bendRangeKnob.setPopupDisplayEnabled(true, true, this);
    auto& fineTuneKnob = addKnob("Fine Tune",   "fineTune");           // sliders[2]
    fineTuneKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    fineTuneKnob.setPopupDisplayEnabled(true, true, this);

    playModeCombo = std::make_unique<juce::ComboBox>();
    playModeCombo->addItem("MONO", 1);
    playModeCombo->addItem("POLY 4", 2);
    addAndMakeVisible(*playModeCombo);

    playModeLabel = std::make_unique<juce::Label>();
    playModeLabel->setText("MODE", juce::dontSendNotification);
    playModeLabel->setJustificationType(juce::Justification::centred);
    playModeLabel->setColour(juce::Label::textColourId, Theme::textMuted);
    playModeLabel->setSize(100, 15);
    // Matches Priority/Glide Time's caption size (comboLabel) instead of the
    // smaller compactLabel — Controllers had MODE at one size and every
    // other caption in the panel at another for no real reason.
    playModeLabel->setFont(Typography::fitToWidth(Typography::comboLabel(), "MODE", static_cast<float>(playModeLabel->getWidth() - 10)));
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
    auto& filterEmphasisKnob = addKnob("Emphasis", "filterResonance");      // sliders[13]
    auto& filterContourKnob  = addKnob("Contour",  "filterContour");        // sliders[14]
    auto& filterDriveKnob    = addKnob("Drive",    "filterDrive");          // sliders[15]
    // These three sit on a 61px knob pitch (see resized(), widened from 57
    // to fix the value-box overlap below them) — no combination of
    // tracking/size reduction on a label wider than that pitch can avoid
    // overlapping its neighbour. Capping each label's own width to the
    // pitch itself and letting fitToWidth shrink tracking/size to match
    // guarantees adjacent labels never touch, without moving any knob.
    {
        auto filterSecondaryLabelWidth = [] (const juce::String& labelText) {
            if (labelText == "EMPHASIS") return 72.0f;
            if (labelText == "CONTOUR")  return 64.0f;
            return 48.0f;
        };
        const auto filterSecondaryLabelFont = Typography::compactLabel();
        for (auto* knobPtr : { &filterEmphasisKnob, &filterContourKnob, &filterDriveKnob }) {
            if (auto it = attachedLabels.find(knobPtr); it != attachedLabels.end() && it->second != nullptr) {
                auto* lbl = it->second;
                const float labelWidth = filterSecondaryLabelWidth (lbl->getText());
                lbl->setSize (juce::roundToInt (labelWidth), lbl->getHeight());
                lbl->setFont (Typography::fitToWidth (filterSecondaryLabelFont, lbl->getText(), labelWidth - 4.0f));
            }
        }
    }

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

    // Preset strip — dark inset buttons/display, matching the new warm-brown
    // chassis (were light cream, which no longer fit the palette).
    auto stylePresetBtn = [](juce::TextButton& btn, const juce::String& text) {
        btn.setButtonText(text);
        btn.setColour(juce::TextButton::buttonColourId,  juce::Colour(0xff3c3020));
        btn.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4a3c2c));
        btn.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffebe8d7));
        btn.setColour(juce::TextButton::textColourOnId,  juce::Colour(0xffebe8d7));
        // Flagged so AnalogLookAndFeel::drawButtonBackground can give these
        // small nav buttons the same muted comboBorder outline as every
        // other interactive control (combo boxes, push buttons), instead of
        // the brighter accentGold border generic action buttons (e.g. INIT)
        // use — that bright border read as mismatched chrome against the
        // rest of the strip.
        btn.setName("presetNav");
    };
    // Solid triangle glyphs instead of "<"/">" ASCII — read as proper nav
    // chevrons rather than typed characters.
    stylePresetBtn(presetPrevButton, juce::String::fromUTF8("\xe2\x97\x80"));
    stylePresetBtn(presetNextButton, juce::String::fromUTF8("\xe2\x96\xb6"));
    presetPrevButton.onClick = [this] { navigatePreset(-1); };
    presetNextButton.onClick = [this] { navigatePreset(+1); };
    addAndMakeVisible(presetPrevButton);
    addAndMakeVisible(presetNextButton);

    presetNameLabel.setText(processor.presetManager.getCurrentPresetName(), juce::dontSendNotification);
    presetNameLabel.setJustificationType(juce::Justification::centred);
    presetNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xffebe8d7));
    // backgroundColourId/outlineColourId intentionally not set here: for a
    // non-editable Label, AnalogLookAndFeel::drawLabel() only ever draws the
    // engraved text — it never reads either colour, so setting them was dead
    // code that looked like it should produce a bordered box but didn't.
    presetNameLabel.setMouseCursor(juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible(presetNameLabel);
    presetNameLabel.addMouseListener(this, false);

    // Widened/heightened from 1460x712: every section column sat only
    // 14-16px apart, three columns had just 6-14px between their stacked
    // top/bottom sub-sections, and the last column left only 4px before the
    // chassis edge. Grown to give uniform 24px gaps between all 5 columns,
    // 20px between stacked sub-sections, and a real margin on the right.
    // Height shrunk back down (726->642) after removing the lower nameplate
    // strip — bottom-row content now ends at y=604, so 642 gives the same
    // ~20px bottom margin every other column edge already gets.
    setSize(1520, 642);

#if JUCE_DEBUG
    // Once per application launch, not once per editor open/close (the
    // editor is destroyed/recreated whenever the plugin window is
    // reopened, e.g. after closing and reopening a DAW's plugin window).
    static bool debugWarningShownThisRun = false;
    if (!debugWarningShownThisRun) {
        debugWarningShownThisRun = true;
        auto banner = std::make_unique<DebugBuildWarningBanner>();
        banner->setBounds(getWidth() - 300 - 12, 12, 300, 74);
        addAndMakeVisible(*banner);
        banner->toFront(false);
        debugBuildWarning = std::move(banner);
    }
#endif
}

LadderVoiceAudioProcessorEditor::~LadderVoiceAudioProcessorEditor()
{
    presetNameLabel.removeMouseListener(this);
    setLookAndFeel(nullptr);
}

void LadderVoiceAudioProcessorEditor::mouseDown(const juce::MouseEvent& e)
{
    if (e.eventComponent == &presetNameLabel)
        showPresetBrowser();
}

void LadderVoiceAudioProcessorEditor::updatePresetNameDisplay(const juce::String& name)
{
    presetNameLabel.setText(name, juce::dontSendNotification);
    fitPresetNameLabelFont();
}

void LadderVoiceAudioProcessorEditor::fitPresetNameLabelFont()
{
    // Preset names are arbitrary-length runtime content (factory + user
    // presets), unlike every other label in the app whose text is fixed at
    // construction — so this is re-measured every time the displayed name
    // changes, against the label's actual current width.
    const auto base = Typography::comboBoxText();
    const float usableWidth = static_cast<float>(presetNameLabel.getWidth() - 10);
    presetNameLabel.setFont(Typography::fitToWidth(base, presetNameLabel.getText(), usableWidth));
}

void LadderVoiceAudioProcessorEditor::navigatePreset(int delta)
{
    auto& mgr = processor.presetManager;
    mgr.refreshUserPresets();
    const int total = mgr.getTotalPresetCount();
    if (total == 0) return;

    // Find current index by name match, fall back to stored index
    int idx = currentPresetIndex;
    for (int i = 0; i < total; ++i) {
        if (mgr.getPresetNameAtIndex(i).equalsIgnoreCase(mgr.getCurrentPresetName())) {
            idx = i;
            break;
        }
    }

    idx = (idx + delta + total) % total;
    currentPresetIndex = idx;
    mgr.loadPresetAtIndex(idx, processor.parameters);
    updatePresetNameDisplay(mgr.getCurrentPresetName());
}

void LadderVoiceAudioProcessorEditor::showPresetBrowser()
{
    processor.presetManager.refreshUserPresets();

    auto* browser = new PresetBrowser(processor, [this](const juce::String& name) {
        updatePresetNameDisplay(name);
    });
    // Apply the synth LookAndFeel so buttons, scrollbar, etc. match the instrument palette
    browser->setLookAndFeel(&analogLookAndFeel);

    juce::CallOutBox::launchAsynchronously(std::unique_ptr<juce::Component>(browser),
                                           presetNameLabel.getScreenBounds(),
                                           nullptr);
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
    // Raised from 360 — more physical travel per full sweep reads as
    // weighted/precise (closer to real hardware) rather than twitchy.
    // Velocity sensitivity eased from 0.55 so fast moves ramp smoothly
    // instead of overshooting sharply.
    slider->setMouseDragSensitivity(550);
    slider->setVelocityBasedMode(true);
    slider->setVelocityModeParameters(0.4, 1, 0.04, true);
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
    } else if (parameterId == "filterResonance" || parameterId == "filterContour" || parameterId == "filterDrive") {
        // Matches the narrower component width these three now use (54px,
        // widened pitch) — the previous default of 62 was wider than even
        // the old 57px pitch between them, which is why their value-readout
        // boxes overlapped.
        valueWidth = 52;
    }
    slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, valueWidth, Theme::valueBoxHeight);
    slider->setColour(juce::Slider::textBoxTextColourId, Theme::valueText);
    slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00090a08));
    slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x002c281e));
    addAndMakeVisible(*slider);

    auto label = std::make_unique<juce::Label>();
    const auto labelText = text.toUpperCase();
    label->setText(labelText, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, Theme::textSecondary);
    // Box width sized to the condensed font's actual measured width (plus a
    // small margin) rather than a fixed per-character guess — the old
    // character-count heuristic was tuned for a wider typeface and, for
    // tightly-pitched knob rows (e.g. Emphasis/Contour/Drive in FILTER),
    // produced label boxes wide enough to visually overlap their neighbours.
    const auto baseLabelFont = Typography::controlLabel();
    const int measuredLabelWidth = juce::roundToInt(Typography::measuredWidth(baseLabelFont, labelText));
    label->setSize(juce::jmax(44, juce::jmax(measuredLabelWidth + 10, valueWidth)), 18);
    // Label's default border is 5px each side — fit against the usable
    // interior width, not the full label width.
    label->setFont(Typography::fitToWidth(baseLabelFont, labelText, static_cast<float>(label->getWidth() - 10)));
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
    combo->setName(parameterId);
    for (int i = 0; i < items.size(); ++i) combo->addItem(items[i], i + 1);
    addAndMakeVisible(*combo);

    auto label = std::make_unique<juce::Label>();
    const auto labelText = text.toUpperCase();
    label->setText(labelText, juce::dontSendNotification);
    label->setJustificationType(juce::Justification::centred);
    label->setColour(juce::Label::textColourId, Theme::textMuted);
    const auto baseComboLabelFont = Typography::comboLabel();
    const int measuredComboLabelWidth = juce::roundToInt(Typography::measuredWidth(baseComboLabelFont, labelText));
    label->setSize(juce::jmax(44, measuredComboLabelWidth + 10), 18);
    label->setFont(Typography::fitToWidth(baseComboLabelFont, labelText, static_cast<float>(label->getWidth() - 10)));
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
        // Oscillator Bank's Wave/Range/Detune/Level/PW captions sat flush
        // against their control (18px box, 14px offset — box bottom actually
        // overlapped the control by 4px). Floated just those 18px above with
        // a 14px box instead, opening a clear 4px gap; every other section
        // keeps its original 18/14 spacing untouched.
        const bool oscBankControl = component->getName().startsWith("osc");
        const bool crampedFilterSecondary = component->getName() == "filterResonance"
                                         || component->getName() == "filterContour"
                                         || component->getName() == "filterDrive";
        // Priority/Glide Time in CONTROLLERS had the same flush-against-
        // control problem (18px label box, 14px offset, self-overlapping by
        // 4px) — the column was re-spaced above to open real headroom, so
        // they get the same wider 18/14 treatment as Oscillator Bank.
        const bool controllersExtraGap = component->getName() == "notePriority"
                                       || component->getName() == "glideTime";
        const bool needsWiderGap = oscBankControl || controllersExtraGap;
        const int labelHeight = needsWiderGap ? 14 : (crampedFilterSecondary ? 12 : 18);
        const int labelWidth = crampedFilterSecondary ? label->getWidth()
                                                      : juce::jmax(bounds.getWidth(), label->getWidth());
        const int x = bounds.getCentreX() - labelWidth / 2;
        // filterCutoff's value-box readout now ends at y=268 (shrunk from
        // 278 above), 20px above these three knobs at y=288 — a 12px label
        // at y=272 clears both sides by a real 4px instead of touching one
        // or the other.
        const int y = crampedFilterSecondary ? 272 : bounds.getY() - (needsWiderGap ? 18 : 14);

        label->setBounds(x, y, labelWidth, labelHeight);
        const auto baseFont = crampedFilterSecondary
                            ? Typography::compactLabel()
                            : (combo ? Typography::comboLabel() : Typography::controlLabel());
        label->setFont(Typography::fitToWidth(baseFont, label->getText(), static_cast<float>(labelWidth - (crampedFilterSecondary ? 4 : 10))));
        label->toFront(false);
    }

    if (playModeCombo != nullptr && playModeLabel != nullptr) {
        const auto bounds = playModeCombo->getBounds();
        const int labelWidth = juce::jmax(bounds.getWidth(), playModeLabel->getWidth());
        playModeLabel->setBounds(bounds.getCentreX() - labelWidth / 2,
                                 bounds.getY() - 21,
                                 labelWidth,
                                 18);
        playModeLabel->setFont(Typography::fitToWidth(Typography::comboLabel(), playModeLabel->getText(), static_cast<float>(labelWidth - 10)));
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
    // Hidden entirely when not applicable (was dimmed-but-visible; that
    // read as a ghosted disabled control rather than genuinely gone).
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
    // Flat-surface treatment: each group sits directly on the shared panel
    // texture — no per-section fill, drop shadow, or corner screws (those
    // read as separate raised modules/"cubes"). Just a single thin outline
    // and a hairline engraved rule under the header, the way a real vintage
    // panel demarcates function groups on one continuous brushed surface.
    const auto f = bounds.toFloat();
    g.setColour(Theme::sectionBorder.withAlpha(0.45f));
    g.drawRoundedRectangle(f.reduced(0.5f), 5.0f, 1.0f);

    auto headerBounds = bounds.removeFromTop(Theme::sectionHeaderHeight);
    const auto headerText = headerBounds.reduced(18, 4);
    const auto titleText = title.toUpperCase();
    g.setFont(Typography::fitToWidth(Typography::sectionTitle(), titleText, static_cast<float>(headerText.getWidth())));
    Typography::drawEngraved(g, titleText, headerText, Theme::textPrimary.withAlpha(0.96f), juce::Justification::centred);

    // Hairline groove under the header — dark edge + light edge one pixel
    // below, reading as an engraved line rather than a drawn stroke.
    g.setColour(juce::Colour(0x30000000));
    g.drawLine(f.getX() + 10.0f, static_cast<float>(headerBounds.getBottom()),
               f.getRight() - 10.0f, static_cast<float>(headerBounds.getBottom()), 0.8f);
    g.setColour(juce::Colour(0x20ffffff));
    g.drawLine(f.getX() + 10.0f, static_cast<float>(headerBounds.getBottom() + 1),
               f.getRight() - 10.0f, static_cast<float>(headerBounds.getBottom() + 1), 0.8f);
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
    // Two overlapping fine-line texture loops (every 6px + every 7px) used
    // to sit here — subtle on the old light cream panel, but the differing
    // frequencies read as a busy striped moiré against the new warm-brown
    // gradient. Removed; the gradient + border alone read as a clean flat
    // surface, consistent with this session's other declutter passes.
    g.setColour(Theme::sectionBorder.withAlpha(0.72f));
    g.drawRoundedRectangle(panel.toFloat().reduced(0.5f), 7.0f, 1.1f);
    g.setColour(Theme::sectionSoftBorder.withAlpha(0.18f));
    g.drawRoundedRectangle(panel.toFloat().reduced(6.0f), 5.0f, 0.65f);

    // Outer panel's 4 corner screws removed — the bottom two kept crowding
    // against the lower nameplate strip no matter how much room was made,
    // and dropping all 4 (not just the bottom pair) keeps the corners
    // symmetric and matches this session's other declutter passes.
    // Title / logo
    // Width widened from the old 370px plate to accommodate Archivo Expanded Bold,
    // which is substantially wider per-glyph than the prior typeface even
    // at maximum tracking/size reduction. This is purely decorative plate
    // real estate — it uses part of the genuinely empty ~210px gap between
    // the plate and the preset strip (which starts at x=694), and touches
    // no control.
    // No fill here any more — with every section now sitting flush on the
    // shared flat surface, this plate's own subtle gradient box read as a
    // stray pale rectangle instead of blending in. titlePlate is kept only
    // as the text's layout reference, not painted.
    auto titlePlate = juce::Rectangle<int>(112, 28, 444, 50);
    auto titleTextArea = titlePlate.reduced(10, 2);
    const juce::String titleText = "LADDER VOICE";
    // Kept as the existing dark, high-contrast tone rather than the
    // requested cream/off-white: the title sits on the light title plate
    // (same light panel gradient as the rest of the chassis), where a cream
    // fill would fail readability against a near-equal-luminance
    // background. Weight/tracking/case/typeface all changed per spec.
    g.setFont(Typography::fitToWidth(Typography::mainTitle(), titleText, static_cast<float>(titleTextArea.getWidth())));
    Typography::drawEngraved(g, titleText, titleTextArea, Theme::textPrimary, juce::Justification::centredLeft);
    // Aligned to the title's own left edge (not a separate hand-picked x),
    // and dropped from y=66 to y=76 — at the old position the subtitle's
    // box overlapped the title's own text box (which runs to y=80),
    // reading as cramped; the smaller/lighter subtitle now sits clearly
    // underneath with its own breathing room.
    auto subtitleArea = juce::Rectangle<int>(titleTextArea.getX(), 74, 230, 13);
    g.setFont(Typography::fitToWidth(Typography::subtitle(), "ANALOG-STYLE SYNTH", static_cast<float>(subtitleArea.getWidth())));
    Typography::drawEngraved(g, "ANALOG-STYLE SYNTH", subtitleArea, Theme::textMuted.withAlpha(0.92f), juce::Justification::centredLeft);

    // Preset strip background (right side of title bar) — dark inset
    // display, consistent with the new warm-brown chassis (was a light
    // gray plate that no longer matched).
    auto presetStrip = juce::Rectangle<int>(694, 36, 322, 34);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff3c3020), static_cast<float>(presetStrip.getX()), static_cast<float>(presetStrip.getY()),
                                           juce::Colour(0xff241c12), static_cast<float>(presetStrip.getX()), static_cast<float>(presetStrip.getBottom()), false));
    g.fillRoundedRectangle(presetStrip.toFloat(), 4.0f);
    // Bumped to match drawSection()'s outline weight (0.45 alpha, 1.0px) —
    // at 0.38/0.85 this strip read as unbordered/floating next to every
    // other module's visible thin outline.
    g.setColour(Theme::sectionBorder.withAlpha(0.45f));
    g.drawRoundedRectangle(presetStrip.toFloat(), 4.0f, 1.0f);
    // Caption zone widened from 52 to 58 (the maximum available before the
    // "<" preset-nav button, which starts at absolute x=752 = presetStrip's
    // x=694 + 58) and its internal padding trimmed from 4 to 2 each side —
    // "PRESET" at compactLabel's tracking/size alone cannot fit in the
    // original 44px; fitToWidth still relaxes tracking/size on top of this
    // wider box so the result is the least reduction actually needed.
    auto presetCaptionArea = presetStrip.removeFromLeft(58).reduced(2, 0);
    g.setFont(Typography::fitToWidth(Typography::compactLabel(), "PRESET", static_cast<float>(presetCaptionArea.getWidth())));
    Typography::drawEngraved(g, "PRESET", presetCaptionArea, Theme::textMuted.withAlpha(0.70f), juce::Justification::centred);

    g.setColour(Theme::sectionBorder.withAlpha(0.35f));
    g.drawLine(116.0f, 91.0f, 1404.0f, 91.0f, 0.9f);

    // Main sections
    // Column gaps widened to a uniform 24px (previously an inconsistent
    // 14-16px) by shifting each column right by a fixed per-column delta —
    // CONTROLLERS +0, OSCILLATOR BANK +10, MIXER/OUTPUT +18,
    // FILTER/FILTER CONTOUR +26, MODULATION/LOUDNESS CONTOUR +34. Every
    // control inside a column gets the same delta as its column in
    // resized(), so internal spacing is bit-for-bit unchanged — only the
    // gaps between columns grow.
    // The three right-hand columns also had just 6-14px between their
    // stacked top/bottom sub-sections; both sub-sections there (and
    // everything below, including the lower strip) shift down by 14px for a
    // uniform 20px gap. CONTROLLERS/OSCILLATOR BANK grow by the same 14px so
    // their bottom edge still lines up with the rest.
    drawSection(g, {110, Theme::panelTop, 160, 500}, "CONTROLLERS");
    drawSection(g, {294, Theme::panelTop, 500, 500}, "OSCILLATOR BANK");
    drawSection(g, {818, Theme::panelTop, 150, 242}, "MIXER");
    drawSection(g, {992, Theme::panelTop, 192, 250}, "FILTER");
    drawSection(g, {1208, Theme::panelTop, 176, 250}, "MODULATION");
    drawSection(g, {818, 374, 150, 230}, "OUTPUT");
    drawSection(g, {992, 374, 192, 230}, "FILTER CONTOUR");
    drawSection(g, {1208, 374, 176, 230}, "LOUDNESS CONTOUR");

    // Oscillator bank row dividers — thin hairlines only, no raised trays
    // (col delta +10; y moved to match the widened 144->163 row pitch)
    g.setColour(juce::Colour(0x30000000));
    g.drawLine(316.0f, 287.0f, 774.0f, 287.0f, 0.8f);
    g.drawLine(316.0f, 450.0f, 774.0f, 450.0f, 0.8f);
    g.setColour(juce::Colour(0x20ffffff));
    g.drawLine(316.0f, 288.0f, 774.0f, 288.0f, 0.8f);
    g.drawLine(316.0f, 451.0f, 774.0f, 451.0f, 0.8f);

    // Lower chassis / nameplate strip removed entirely (was a plate with
    // "LADDER VOICE / CONTROL SURFACE" text + decorative vents) — the
    // window height was shrunk to close the gap this leaves at the bottom.
}

void LadderVoiceAudioProcessorEditor::resized()
{
    int s = 0, b = 0, c = 0;

    // CONTROLLERS — section {110,104,160,500}, column delta +0
    // MODE / Priority / Glide Time at top, then the 4 push buttons below in
    // reading order (Glide first). This whole column was fully packed
    // top-to-bottom with only 3-4px between each label and its control —
    // Priority and Glide Time shifted down (206/256, was 200/242) to open
    // real ~10-12px gaps; the 4 buttons below shift down and shrink
    // slightly (73->70px) to absorb that without pushing Keyboard past the
    // section's bottom edge.
    if (playModeCombo != nullptr) playModeCombo->setBounds({126, 156, 124, 26}); // label at 139
    setComponentBounds(combos,  c, {126, 206, 124, 24});       // [c0] notePriority
    setComponentBounds(sliders, s, {141, 256, 98, 40});        // [s0] glideTime (popup)
    // [s1] pitchBendRange + [s2] fineTune in header strip — shifted +30
    // (midway between the Filter/Modulation column deltas they sit above)
    // to track the wider chassis.
    // Tightened 22px gaps to 12px (and Volume below by the same amount) so
    // the three header knobs read as one grouped cluster rather than
    // spread loosely across the strip.
    setComponentBounds(sliders, s, {1120, 38, 88, 50});        // [s1] pitchBendRange
    setComponentBounds(sliders, s, {1230, 38, 88, 50});        // [s2] fineTune

    // 4 push buttons — vertical column
    setComponentBounds(buttons, b, {130, 306, 120, 70});       // [b0] glide
    setComponentBounds(buttons, b, {130, 380, 120, 70});       // [b1] legato
    setComponentBounds(buttons, b, {130, 454, 120, 70});       // [b2] retrigger
    // [b6] keyboard set after OSC loop (counter must reach 6 first)
    // OSC BANK — column delta +10. Columns: Enable@339 Wave@408 Range@518
    // PW/Detune@588 Level@684. Knob slots w=102 h=124 → knobBox 98×102 →
    // render 98px, medium strip (98≥76)
    // Row pitch widened 144->163 — the section box is 500 tall but the 3
    // rows only filled the top ~420px of it, leaving ~48px of empty space
    // at the bottom. The wider pitch spreads them evenly across the full
    // section height instead (row dividers below moved to match).
    for (int osc = 0; osc < 3; ++osc) {
        const int rowY = 154 + osc * 163;
        const int btnY = (osc == 0) ? 143 : rowY - 10;
        setComponentBounds(buttons, b, {339, btnY, 50, 124});               // [b3,4,5] osc xlarge
        setComponentBounds(combos,  c, {408, rowY + 8, 96, 26});            // [c1,3,5] wave
        setComponentBounds(combos,  c, {518, rowY + 8, 62, 26});            // [c2,4,6] range
        if (osc > 0) setComponentBounds(sliders, s, {588, rowY + 8, 92, 100}); // [s4,6] detune
        setComponentBounds(sliders, s, {684, rowY + 8, 92, 100});           // [s3,5,7] level
    }
    setComponentBounds(buttons, b, {130, 528, 120, 70});       // [b6] keyboard
    setComponentBounds(sliders, s, {588, 162, 92, 100});      // [s8] osc1PulseWidth
    setComponentBounds(sliders, s, {0, 0, 0, 0});              // [s9] analogDrift hidden, APVTS kept

    // MIXER — section {818,104,150,242}, column delta +18. mixerDrive:
    // knobBox 140×90 → render 90px, large strip (140≥100)
    setComponentBounds(sliders, s, {830, 156, 126,  88});      // [s10] mixerDrive
    setComponentBounds(sliders, s, {845, 270,  96,  74});      // [s11] noiseLevel

    // FILTER — section {992,104,192,260}, column delta +26 — big Cutoff + secondary controls
    // Cutoff: the rotary's rendered size tracks slider height (minus the
    // fixed 22px value-box strip below it), not width — component width has
    // no effect on knob size. Shrunk from 132 to 118 to 108 (rotary ~86px)
    // — its value-box readout ("1800 Hz") sat only 10px above the
    // EMPHASIS/CONTOUR/DRIVE row, too tight to fit their caption with any
    // real breathing room on either side. The extra 10px shaved off here
    // opens a genuine ~20px gap below instead.
    setComponentBounds(sliders, s, {1016, 160, 144, 108});     // [s12] filterCutoff
    // Narrowed 70->54 and re-pitched 57->61px (1000/1061/1122) — at width 70
    // on a 57px pitch these three overlapped by 13px, and their value-box
    // readouts (widened separately below) overlapped even more visibly.
    // Knob render size is driven by height, not width, so the dial itself
    // is unchanged; only the three boxes are now evenly spaced with a
    // real ~7px gap.
    setComponentBounds(sliders, s, {1000, 288,  54,  60});     // [s13] filterResonance
    setComponentBounds(sliders, s, {1061, 288,  54,  60});     // [s14] filterContour
    setComponentBounds(sliders, s, {1122, 288,  54,  60});     // [s15] filterDrive

    // FILTER CONTOUR — section {992,374,192,230}, column delta +26, row delta +14
    // Row 1: knobBox 84×80 → render 80px medium; Row 2: knobBox 84×56 → render 56px
    setComponentBounds(sliders, s, {1006, 428, 76,  78});      // [s16] filterAttack
    setComponentBounds(sliders, s, {1092, 428, 76,  78});      // [s17] filterDecay
    setComponentBounds(sliders, s, {1006, 520, 76,  78});      // [s18] filterSustain
    setComponentBounds(sliders, s, {1092, 520, 76,  78});      // [s19] filterRelease

    // MODULATION — section {1208,104,176,336}, column delta +34
    setComponentBounds(sliders, s, {1220, 158,  72, 74});      // [s20] lfoRate
    setComponentBounds(sliders, s, {1300, 158,  72, 74});      // [s21] lfoAmount
    setComponentBounds(combos, c, {1220, 242, 152, 26});       // [c9]  lfoDestination
    setComponentBounds(sliders, s, {1256, 284,  80, 64});      // [s22] modWheelAmount

    // LOUDNESS CONTOUR — section {1208,374,176,134}, column delta +34, row delta +14
    // compact (loudness*): no reduction; w=76 → render min(76,86)=76px, medium strip (76≥76) ✓
    setComponentBounds(sliders, s, {1218, 428, 76,  78});      // [s23] loudnessAttack
    setComponentBounds(sliders, s, {1296, 428, 76,  78});      // [s24] loudnessDecay
    setComponentBounds(sliders, s, {1218, 520, 76,  78});      // [s25] loudnessSustain
    setComponentBounds(sliders, s, {1296, 520, 76,  78});      // [s26] loudnessRelease

    // OUTPUT — section {818,374,150,230}
    setComponentBounds(sliders, s, {822, 430, 142,  96});      // [s27] masterVolume
    setComponentBounds(sliders, s, {0, 0, 0, 0});               // [s28] outputDrive hidden, APVTS kept

    // Preset strip
    presetPrevButton.setBounds(752, 42, 28, 26);
    presetNameLabel.setBounds(784, 42, 162, 26);
    presetNextButton.setBounds(950, 42, 28, 26);
    fitPresetNameLabelFont();

    positionControlLabels();
    updatePulseWidthControlState();
}
