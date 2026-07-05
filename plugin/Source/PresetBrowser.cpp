#include "PresetBrowser.h"
#include "PluginProcessor.h"
#include "Typography.h"
#include <algorithm>

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
    auto& mgr = processor.presetManager;
    mgr.refreshUserPresets();

    // Factory presets grouped by category as collapsible sections (click a
    // header to open/close it) instead of one long mixed list — the fixed
    // order here matches how the bank was designed (Bass -> Lead -> Brass ->
    // Glide Lead -> Keys -> Pluck -> Pad -> Sweep). "Init" has no header of
    // its own since it's a single utility entry, not a sound category.
    static const juce::StringArray kCategoryOrder {
        "Bass", "Lead", "Brass", "Glide Lead", "Keys", "Pluck", "Pad", "Sweep"
    };

    // First time through: default every section collapsed except the one
    // containing the currently-loaded preset, so the browser opens compact
    // (just section names) but still shows where you are.
    if (!collapsedInitialised) {
        collapsedInitialised = true;
        juce::String currentCategory;
        for (int i = 0; i < mgr.getNumFactoryPresets(); ++i) {
            const auto& p = mgr.getFactoryPreset(i);
            if (p.name.equalsIgnoreCase(mgr.getCurrentPresetName())) {
                currentCategory = p.category;
                break;
            }
        }
        for (auto& category : kCategoryOrder)
            collapsed[category.toUpperCase()] = !category.equalsIgnoreCase(currentCategory);
        // If the current preset wasn't found among factory presets, it must be
        // a user preset — expand USER PRESETS by default in that case.
        collapsed["USER PRESETS"] = !currentCategory.isEmpty();
    }

    entries.clear();

    std::vector<int> remaining;
    for (int i = 0; i < mgr.getNumFactoryPresets(); ++i)
        remaining.push_back(i);

    // Init first, ungrouped, if present.
    for (auto it = remaining.begin(); it != remaining.end(); ) {
        if (mgr.getFactoryPreset(*it).category.equalsIgnoreCase("Init")) {
            entries.push_back({mgr.getFactoryPreset(*it).name, false, *it, false});
            it = remaining.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& category : kCategoryOrder) {
        std::vector<int> inCategory;
        for (int i : remaining)
            if (mgr.getFactoryPreset(i).category.equalsIgnoreCase(category))
                inCategory.push_back(i);
        if (inCategory.empty())
            continue;

        const auto headerName = category.toUpperCase();
        entries.push_back({headerName, false, -1, true});
        if (collapsed[headerName]) {
            for (int i : inCategory)
                remaining.erase(std::find(remaining.begin(), remaining.end(), i));
            continue;
        }
        for (int i : inCategory) {
            entries.push_back({mgr.getFactoryPreset(i).name, false, i, false});
            remaining.erase(std::find(remaining.begin(), remaining.end(), i));
        }
    }

    // Anything with a category outside the known list still shows up, rather
    // than silently disappearing if a future preset's category is renamed.
    if (!remaining.empty()) {
        entries.push_back({"OTHER", false, -1, true});
        if (!collapsed["OTHER"])
            for (int i : remaining)
                entries.push_back({mgr.getFactoryPreset(i).name, false, i, false});
    }

    if (mgr.getNumUserPresets() > 0) {
        entries.push_back({"USER PRESETS", true, -1, true});
        if (!collapsed["USER PRESETS"])
            for (int i = 0; i < mgr.getNumUserPresets(); ++i)
                entries.push_back({mgr.getUserPresetName(i), true, i, false});
    }

    listBox.updateContent();
}

void PresetBrowser::toggleSection(const juce::String& header)
{
    collapsed[header] = !collapsed[header];
    rebuildEntries();
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

void PresetBrowser::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    if (row < 0 || row >= (int)entries.size()) return;
    const auto& e = entries[(size_t)row];
    if (e.isHeader)
        toggleSection(e.name);
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

    if (e.isHeader) {
        // Section header row — click to open/close. Slightly darker
        // background, gold accent line on left, chevron shows current state.
        const bool isOpen = !collapsed[e.name];
        g.setColour(row == hoveredRow ? sepBg.brighter(0.06f) : sepBg);
        g.fillAll();
        // Gold left accent bar
        g.setColour(accentGold.withAlpha(0.55f));
        g.fillRect(0, 3, 2, h - 6);
        // Hairline borders
        g.setColour(sectionBdr.withAlpha(0.40f));
        g.drawLine(0, 0, (float)w, 0, 0.7f);
        g.drawLine(0, (float)h - 0.7f, (float)w, (float)h - 0.7f, 0.7f);
        // Chevron: ▸ collapsed, ▾ open
        g.setColour(accentGold.withAlpha(0.85f));
        g.setFont(Typography::popupGroupHeader());
        g.drawText(isOpen ? juce::String::fromUTF8("\xe2\x96\xbe") : juce::String::fromUTF8("\xe2\x96\xb8"),
                   10, 0, 16, h, juce::Justification::centred);
        // Label — manual tracking for visible hardware-style letter spacing
        g.setColour(textMuted.brighter(0.08f));
        constexpr float groupTrackPx = 0.45f;
        constexpr float labelIndent = 26.0f;
        const float availableW = (float)(w - labelIndent - 8);
        const float groupManualWidth = juce::jmax(0, e.name.length() - 1) * groupTrackPx;
        g.setFont(Typography::fitToWidth(Typography::popupGroupHeader(), e.name, availableW - groupManualWidth));
        drawTrackedText(g, e.name, labelIndent, 0.0f, availableW, (float)h, groupTrackPx,
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
    const auto entryBase = e.isUser ? Typography::popupEntryItalic() : Typography::popupEntry();
    const int entryWidth = w - indent - 6;
    g.setFont(Typography::fitToWidth(entryBase, e.name, static_cast<float>(entryWidth)));
    g.drawText(e.name, indent, 0, entryWidth, h, juce::Justification::centredLeft);

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

    // Header — flat treatment matching PluginEditor's drawSection(): no
    // filled bar and no corner screws (those read as a separate raised
    // module, which the main panel moved away from this session). Just the
    // title text and a thin engraved-groove rule underneath.
    g.setColour(juce::Colour(0xff11110f));
    constexpr float titleTrackPx = 0.55f;
    constexpr float titleBoxWidth = 110.0f;
    const float titleManualWidth = juce::jmax(0, juce::String("PRESETS").length() - 1) * titleTrackPx;
    g.setFont(Typography::fitToWidth(Typography::popupTitle(), "PRESETS", titleBoxWidth - titleManualWidth));
    drawTrackedText(g, "PRESETS", bounds.getX() + 18.0f, 4.0f, titleBoxWidth, kHeaderH, titleTrackPx,
                    juce::Justification::centredLeft);

    g.setColour(juce::Colour(0x30000000));
    g.drawLine(bounds.getX() + 10.0f, kHeaderH + 5.0f, bounds.getRight() - 10.0f, kHeaderH + 5.0f, 0.8f);
    g.setColour(juce::Colour(0x20ffffff));
    g.drawLine(bounds.getX() + 10.0f, kHeaderH + 6.0f, bounds.getRight() - 10.0f, kHeaderH + 6.0f, 0.8f);
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
