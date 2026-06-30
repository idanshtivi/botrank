#include "PresetBrowser.h"
#include "PluginProcessor.h"

namespace {
// Shared palette — mirrors Theme namespace in PluginEditor.cpp
const auto panel       = juce::Colour(0xffbcbbb1);
const auto panelTop    = juce::Colour(0xffd8d7cc);
const auto sectionBdr  = juce::Colour(0xff5b574b);
const auto insetBg     = juce::Colour(0xff16130e);
const auto accentGold  = juce::Colour(0xff96783c);
const auto textCream   = juce::Colour(0xffe7dfc8);
const auto textMuted   = juce::Colour(0xff8f8772);
const auto selBg       = juce::Colour(0xff5a3f18); // warm amber selection
const auto hoverBg     = juce::Colour(0xff251c0e); // very dark warm brown — subtle hover
const auto sepBg       = juce::Colour(0xff201d16);

juce::FontOptions uiFont(float size, int style = juce::Font::plain)
{
    return juce::FontOptions("Segoe UI", size, style);
}

// Draws text with explicit per-character pixel tracking using GlyphArrangement.
// JUCE 8 removed Font::getStringWidth; GlyphArrangement is the correct measurement API.
static void drawTrackedText(juce::Graphics& g, const juce::String& text,
                             float x, float y, float w, float h,
                             float trackPx, juce::Justification just)
{
    if (text.isEmpty()) return;
    const juce::Font font = g.getCurrentFont();

    juce::GlyphArrangement ga;
    ga.addLineOfText(font, text, 0.0f, 0.0f);

    const int n = ga.getNumGlyphs();
    if (n == 0) return;

    // Push each glyph right by accumulated tracking (i * trackPx)
    for (int i = 1; i < n; ++i)
        ga.moveRangeOfGlyphs(i, -1, trackPx, 0.0f);

    const float totalW = ga.getBoundingBox(0, n, false).getWidth();

    float startX = x;
    if (just.testFlags(juce::Justification::horizontallyCentred))
        startX = x + (w - totalW) * 0.5f;
    else if (just.testFlags(juce::Justification::right))
        startX = x + w - totalW;

    // Baseline = top of text area + vertical centre offset + ascent
    const float baselineY = y + (h - font.getHeight()) * 0.5f + font.getAscent();
    ga.moveRangeOfGlyphs(0, -1, startX, baselineY);
    ga.draw(g);
}
} // namespace

// Header height constant — shared between paint() and resized()
static constexpr float kHeaderH = 36.0f; // was 30; extra 6px gives title breathing room

PresetBrowser::PresetBrowser(LadderVoiceAudioProcessor& proc,
                             std::function<void(const juce::String&)> cb)
    : processor(proc), onPresetLoaded(std::move(cb))
{
    setSize(330, 430);

    listBox.setModel(this);
    listBox.setMultipleSelectionEnabled(false);
    listBox.setColour(juce::ListBox::backgroundColourId, insetBg);
    listBox.setColour(juce::ListBox::outlineColourId,    accentGold.withAlpha(0.55f));
    listBox.setColour(juce::ScrollBar::thumbColourId,    accentGold.withAlpha(0.50f));
    listBox.setOutlineThickness(1);
    listBox.setRowHeight(28);
    // Register for mouse events inside the list so hover tracking works
    listBox.addMouseListener(this, true);
    addAndMakeVisible(listBox);

    loadBtn.setButtonText("Load");
    saveBtn.setButtonText("Save As...");
    deleteBtn.setButtonText("Delete");
    loadBtn.onClick   = [this] { loadSelected(); };
    saveBtn.onClick   = [this] { saveAs(); };
    deleteBtn.onClick = [this] { deleteSelected(); };
    addAndMakeVisible(loadBtn);
    addAndMakeVisible(saveBtn);
    addAndMakeVisible(deleteBtn);

    rebuildEntries();
    listBox.selectRow(0);

    // Pre-select the currently active preset
    const auto& mgr = processor.presetManager;
    for (int i = 0; i < (int)entries.size(); ++i) {
        if (entries[(size_t)i].name.equalsIgnoreCase(mgr.getCurrentPresetName())) {
            listBox.selectRow(i, false, true);
            break;
        }
    }
    updateDeleteState();
}

// ─── Entries ────────────────────────────────────────────────────────────────

void PresetBrowser::rebuildEntries()
{
    entries.clear();
    auto& mgr = processor.presetManager;
    mgr.refreshUserPresets();

    entries.push_back({"FACTORY PRESETS", false, -1});
    for (int i = 0; i < mgr.getNumFactoryPresets(); ++i)
        entries.push_back({mgr.getFactoryPreset(i).name, false, i});

    if (mgr.getNumUserPresets() > 0) {
        entries.push_back({"USER PRESETS", true, -1});
        for (int i = 0; i < mgr.getNumUserPresets(); ++i)
            entries.push_back({mgr.getUserPresetName(i), true, i});
    }

    listBox.updateContent();
}

// ─── Preset actions (functional — unchanged) ─────────────────────────────────

void PresetBrowser::loadSelected()
{
    const int row = listBox.getSelectedRow();
    if (row < 0 || row >= (int)entries.size()) return;
    auto& e = entries[(size_t)row];
    if (e.sourceIndex < 0) return;

    auto& mgr = processor.presetManager;
    if (e.isUser)
        mgr.loadUserPreset(e.sourceIndex, processor.parameters);
    else
        mgr.applyPreset(mgr.getFactoryPreset(e.sourceIndex), processor.parameters);

    mgr.setCurrentPresetName(e.name);
    if (onPresetLoaded) onPresetLoaded(e.name);
}

void PresetBrowser::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    listBox.selectRow(row);
    loadSelected();
}

void PresetBrowser::saveAs()
{
    auto* alertWindow = new juce::AlertWindow("Save Preset",
                                              "Enter a name for your preset:",
                                              juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("name", processor.presetManager.getCurrentPresetName());
    alertWindow->addTextEditor("category", "User");
    alertWindow->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result) {
            if (result == 1) {
                auto name = alertWindow->getTextEditorContents("name").trim();
                auto cat  = alertWindow->getTextEditorContents("category").trim();
                if (name.isEmpty()) name = "User Preset";
                if (cat.isEmpty())  cat  = "User";
                processor.presetManager.saveUserPreset(name, cat, processor.parameters);
                processor.presetManager.setCurrentPresetName(name);
                rebuildEntries();
                for (int i = 0; i < (int)entries.size(); ++i) {
                    if (entries[(size_t)i].name == name) {
                        listBox.selectRow(i, false, true);
                        break;
                    }
                }
                if (onPresetLoaded) onPresetLoaded(name);
            }
            delete alertWindow;
        }), false);
}

void PresetBrowser::deleteSelected()
{
    const int row = listBox.getSelectedRow();
    if (row < 0 || row >= (int)entries.size()) return;
    auto& e = entries[(size_t)row];
    if (!e.isUser || e.sourceIndex < 0) return;

    auto confirmCb = [this, name = e.name, userIdx = e.sourceIndex](int result) {
        if (result == 1) {
            processor.presetManager.deleteUserPreset(userIdx);
            if (processor.presetManager.getCurrentPresetName() == name)
                processor.presetManager.setCurrentPresetName("Init");
            rebuildEntries();
            updateDeleteState();
            if (onPresetLoaded) onPresetLoaded(processor.presetManager.getCurrentPresetName());
        }
    };

    juce::AlertWindow::showOkCancelBox(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Delete \"" + e.name + "\"?",
        "Delete", "Cancel",
        nullptr,
        juce::ModalCallbackFunction::create(confirmCb));
}

// ─── Delete enable/disable ───────────────────────────────────────────────────

void PresetBrowser::updateDeleteState()
{
    const int row = listBox.getSelectedRow();
    const bool canDelete = row >= 0
        && row < (int)entries.size()
        && entries[(size_t)row].isUser
        && entries[(size_t)row].sourceIndex >= 0;
    deleteBtn.setEnabled(canDelete);
}

void PresetBrowser::selectedRowsChanged(int /*lastRowSelected*/)
{
    updateDeleteState();
}

// ─── Hover tracking ──────────────────────────────────────────────────────────

void PresetBrowser::mouseMove(const juce::MouseEvent& e)
{
    if (&listBox != e.eventComponent && !listBox.isParentOf(e.eventComponent)) {
        if (hoveredRow >= 0) { listBox.repaintRow(hoveredRow); hoveredRow = -1; }
        return;
    }
    const auto pos = e.getEventRelativeTo(&listBox).getPosition();
    const int row = listBox.getRowContainingPosition(pos.x, pos.y);
    if (row != hoveredRow) {
        const int prev = hoveredRow;
        hoveredRow = row;
        if (prev >= 0) listBox.repaintRow(prev);
        if (row  >= 0) listBox.repaintRow(row);
    }
}

void PresetBrowser::mouseExit(const juce::MouseEvent& e)
{
    if (e.eventComponent == &listBox) {
        if (hoveredRow >= 0) { listBox.repaintRow(hoveredRow); hoveredRow = -1; }
    }
}

// ─── ListBoxModel painting ───────────────────────────────────────────────────

int PresetBrowser::getNumRows()
{
    return (int)entries.size();
}

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int)entries.size()) return;
    const auto& e = entries[(size_t)row];
    const bool isSep = (e.sourceIndex < 0);

    if (isSep) {
        // Section header row — slightly darker background, gold accent line on left
        g.setColour(sepBg);
        g.fillAll();
        // Gold left accent bar
        g.setColour(accentGold.withAlpha(0.55f));
        g.fillRect(0, 3, 2, h - 6);
        // Hairline borders
        g.setColour(sectionBdr.withAlpha(0.40f));
        g.drawLine(0, 0, (float)w, 0, 0.7f);
        g.drawLine(0, (float)h - 0.7f, (float)w, (float)h - 0.7f, 0.7f);
        // Label — manual tracking for visible hardware-style letter spacing
        g.setColour(textMuted.brighter(0.08f));
        g.setFont(juce::Font(uiFont(12.0f, juce::Font::bold)));
        drawTrackedText(g, e.name, 10.0f, 0.0f, (float)(w - 10), (float)h, 1.1f,
                        juce::Justification::centredLeft);
        return;
    }

    // Background: selected > hover > alternating subtle tint
    if (selected) {
        g.setGradientFill(juce::ColourGradient(selBg.brighter(0.10f), 0.0f, 0.0f,
                                               selBg.darker(0.08f), 0.0f, (float)h, false));
        g.fillAll();
        // Gold left marker
        g.setColour(accentGold.withAlpha(0.85f));
        g.fillRect(0, 2, 3, h - 4);
        // Subtle top highlight
        g.setColour(juce::Colour(0x33ffffff));
        g.drawLine(3.0f, 0.7f, (float)w, 0.7f, 0.7f);
    } else if (row == hoveredRow) {
        // Very subtle warm brown — clearly weaker than selected
        g.setColour(hoverBg);
        g.fillAll();
    } else {
        // Alternating barely-visible row tint
        g.setColour(row % 2 == 0 ? juce::Colour(0x08ffffff) : juce::Colour(0x00000000));
        g.fillAll();
    }

    const int indent = selected ? 10 : (e.isUser ? 16 : 10);
    const float textAlpha = selected ? 1.0f : (row == hoveredRow ? 0.92f : 0.82f);
    g.setColour(selected ? textCream.brighter(0.10f) : textCream.withAlpha(textAlpha));
    g.setFont(juce::Font(uiFont(14.0f, e.isUser ? juce::Font::italic : juce::Font::plain)).withExtraKerningFactor(0.008f));
    g.drawText(e.name, indent, 0, w - indent - 6, h, juce::Justification::centredLeft);

    // Subtle bottom hairline
    g.setColour(sectionBdr.withAlpha(0.18f));
    g.drawLine(6.0f, (float)h - 0.7f, (float)w - 6.0f, (float)h - 0.7f, 0.7f);
}

// ─── Panel painting ──────────────────────────────────────────────────────────

void PresetBrowser::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Drop shadow
    g.setColour(juce::Colour(0x60000000));
    g.fillRoundedRectangle(bounds.translated(0.0f, 2.5f), 7.0f);

    // Main panel — warm aged cream gradient
    g.setGradientFill(juce::ColourGradient(panelTop, bounds.getX(), bounds.getY(),
                                           panel.darker(0.06f), bounds.getX(), bounds.getBottom(), false));
    g.fillRoundedRectangle(bounds, 7.0f);

    // Subtle texture lines
    g.setColour(juce::Colour(0x0a000000));
    for (float y = bounds.getY() + 10.0f; y < bounds.getBottom() - 8.0f; y += 5.5f)
        g.drawLine(bounds.getX() + 8.0f, y, bounds.getRight() - 8.0f, y, 0.30f);

    // Outer border + inner bevel
    g.setColour(sectionBdr.withAlpha(0.70f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), 7.0f, 1.0f);
    g.setColour(juce::Colour(0x18ffffff));
    g.drawRoundedRectangle(bounds.reduced(2.5f), 5.5f, 0.60f);

    // Header strip — matches drawSection() style, now taller for breathing room
    auto headerArea = bounds.withHeight(kHeaderH + 6.0f).reduced(1.0f, 1.0f);
    headerArea.setBottom(kHeaderH + 5.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffaaa99f), bounds.getX(), bounds.getY(),
                                           juce::Colour(0xff939188), bounds.getX(), headerArea.getBottom(), false));
    g.fillRoundedRectangle(headerArea, 6.0f);
    g.setColour(juce::Colour(0xffaaa99f));
    g.fillRect(headerArea.withTrimmedTop(headerArea.getHeight() - 7.0f));

    // Gold divider under header
    g.setColour(accentGold.withAlpha(0.35f));
    g.drawLine(bounds.getX() + 10.0f, kHeaderH + 5.0f, bounds.getRight() - 10.0f, kHeaderH + 5.0f, 0.85f);

    // "PRESETS" title — manually tracked for visible hardware label feel
    g.setColour(juce::Colour(0xff11110f));
    g.setFont(juce::Font(uiFont(15.0f, juce::Font::bold)));
    drawTrackedText(g, "PRESETS", bounds.getX() + 26.0f, 4.0f, 110.0f, kHeaderH, 1.5f,
                    juce::Justification::centredLeft);

    // Corner screws
    const float screwY = kHeaderH * 0.5f + 2.5f;
    auto drawScrew = [&](float cx, float cy) {
        auto r = juce::Rectangle<float>(8.0f, 8.0f).withCentre({cx, cy});
        g.setColour(juce::Colour(0x55000000));
        g.fillEllipse(r.translated(0.0f, 0.8f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffd88a), r.getX(), r.getY(),
                                               juce::Colour(0xff4a2b0d), r.getRight(), r.getBottom(), false));
        g.fillEllipse(r);
        g.setColour(juce::Colour(0xff0a0a08));
        g.drawEllipse(r, 0.85f);
        g.setColour(juce::Colour(0xdddddddd));
        g.drawLine(r.getX() + 1.5f, cy + 0.3f, r.getRight() - 1.5f, cy - 0.3f, 0.85f);
    };
    drawScrew(bounds.getX() + 14.0f, screwY);
    drawScrew(bounds.getRight() - 14.0f, screwY);
}

// ─── Layout ──────────────────────────────────────────────────────────────────

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(7);
    area.removeFromTop((int)kHeaderH + 5 + 4); // header strip + divider + gap
    auto btnRow = area.removeFromBottom(36);
    area.removeFromBottom(5);

    listBox.setBounds(area);

    const int btnW = (btnRow.getWidth() - 8) / 3;
    loadBtn.setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(4);
    saveBtn.setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(4);
    deleteBtn.setBounds(btnRow);
}
