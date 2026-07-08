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
// Matches Theme::textPrimary in PluginEditor.cpp — was a brighter, separately
// defined 0xffe7dfc8 that read as too white/high-contrast against the dark
// inset list background; using the same warm cream as the rest of the panel
// keeps the preset list visually consistent with everything else.
const auto textCream   = juce::Colour(0xffd9cbb0);
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
    setSize(560, 460);

    listBox.setModel(this);
    listBox.setMultipleSelectionEnabled(false);
    listBox.setColour(juce::ListBox::backgroundColourId, insetBg);
    listBox.setColour(juce::ListBox::outlineColourId,    accentGold.withAlpha(0.55f));
    listBox.setColour(juce::ScrollBar::thumbColourId,    accentGold.withAlpha(0.50f));
    listBox.setOutlineThickness(1);
    listBox.setRowHeight(31); // was 28 — a touch more vertical breathing room per row
    // Register for mouse events inside the list so hover tracking works
    listBox.addMouseListener(this, true);
    addAndMakeVisible(listBox);

    searchBox.setColour(juce::TextEditor::backgroundColourId, insetBg);
    searchBox.setColour(juce::TextEditor::textColourId, textCream);
    searchBox.setColour(juce::TextEditor::outlineColourId, accentGold.withAlpha(0.40f));
    searchBox.setColour(juce::TextEditor::focusedOutlineColourId, accentGold.withAlpha(0.85f));
    searchBox.setColour(juce::TextEditor::highlightColourId, accentGold.withAlpha(0.35f));
    searchBox.setFont(Typography::popupEntry());
    searchBox.setJustification(juce::Justification::centredLeft);
    searchBox.setTextToShowWhenEmpty("Search presets or categories...", textMuted.withAlpha(0.6f));
    // Live filtering — every keystroke rebuilds the (cheap, linear-scan)
    // filtered list; trivially fast even for hundreds of presets.
    searchBox.onTextChange = [this] { rebuildEntries(); };
    searchBox.onEscapeKey = [this] { searchBox.clear(); rebuildEntries(); };
    addAndMakeVisible(searchBox);

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

// ─── Category sidebar ───────────────────────────────────────────────────────

void PresetBrowser::rebuildCategoryTabs()
{
    // Preferred display order for the categories a professional factory
    // library actually uses; anything else discovered in the data still
    // gets a tab, just appended after these. Categories are read from the
    // real PresetData::category field (factory) and each user preset file's
    // own "category" attribute — there is exactly one category system, not
    // a second hardcoded taxonomy layered on top. A user preset saved under
    // "Bass" shows up in the Bass tab right alongside factory Bass presets,
    // not in a separate opaque bucket.
    static const juce::StringArray kPreferredOrder {
        "Bass", "Lead", "Pad", "Keys", "Brass", "Pluck", "Cinematic", "Performance", "FX", "User"
    };

    // Preset-development session folders. Registered here so each one exists
    // as a browser tab (and a valid search/save target) from the moment it's
    // created, even before a single preset has been saved into it — normal
    // category discovery below only finds categories that already have at
    // least one preset. Add new "LAB - ..." folders to this list; do not
    // touch kPreferredOrder, which is reserved for the shipped Factory
    // sound-type taxonomy.
    static const juce::StringArray kRegisteredLabFolders {
        "LAB - Mid Bass Exploration 01",
        "LAB - Leads Exploration 01",
        "LAB - Plucks Exploration 01",
        "LAB - Keys Exploration 01",
        "LAB - FX Exploration 01",
        "LAB - Experimental 01",
    };

    auto& mgr = processor.presetManager;
    juce::StringArray discovered;
    for (int i = 0; i < mgr.getNumFactoryPresets(); ++i) {
        const auto cat = mgr.getFactoryPreset(i).category;
        if (cat.equalsIgnoreCase("Init")) continue; // Init is pinned in "All", not its own tab
        if (!discovered.contains(cat))
            discovered.add(cat);
    }
    for (int i = 0; i < mgr.getNumUserPresets(); ++i) {
        const auto cat = mgr.getUserPresetCategory(i); // defaults to "User" if the file predates categories
        if (!discovered.contains(cat))
            discovered.add(cat);
    }
    for (auto& c : kRegisteredLabFolders)
        if (!discovered.contains(c))
            discovered.add(c);

    categoryNames.clear();
    categoryNames.add("All");
    for (auto& c : kPreferredOrder)
        if (discovered.contains(c)) categoryNames.add(c);

    // LAB session folders are grouped together, sorted alphabetically, and
    // visually separated from the Factory tabs above (see the extra gap +
    // divider drawn in resized()/paint() at firstLabTabIndex) — ad-hoc
    // audition sessions stay out of the curated taxonomy's order while
    // remaining fully browsable/searchable/loadable/savable like any other
    // category.
    juce::StringArray labCategories;
    for (auto& c : discovered)
        if (c.startsWithIgnoreCase("LAB - ") && !categoryNames.contains(c))
            labCategories.add(c);
    labCategories.sort(true); // case-insensitive alphabetical
    firstLabTabIndex = labCategories.isEmpty() ? -1 : categoryNames.size();
    for (auto& c : labCategories)
        categoryNames.add(c);

    // Anything left over (neither a known Factory category nor a LAB session
    // folder) still gets a tab, appended in discovery order.
    for (auto& c : discovered)
        if (!categoryNames.contains(c))
            categoryNames.add(c);

    // selectedCategory may have disappeared (e.g. last preset in it deleted)
    if (!categoryNames.contains(selectedCategory))
        selectedCategory = "All";
}

// ─── Entries ────────────────────────────────────────────────────────────────

bool PresetBrowser::matchesSearch(const juce::String& name, const juce::String& category) const
{
    const auto q = searchBox.getText();
    if (q.isEmpty()) return true;
    return name.containsIgnoreCase(q) || category.containsIgnoreCase(q);
}

void PresetBrowser::rebuildEntries()
{
    auto& mgr = processor.presetManager;
    mgr.refreshUserPresets();
    rebuildCategoryTabs();

    entries.clear();

    // One combined view over every preset (factory + user), each tagged
    // with its real category, so category filtering/grouping treats both
    // sources identically — a user preset is a first-class member of
    // whatever category it was saved under, not a separate bucket.
    struct Ref { juce::String name; juce::String category; bool isUser; int index; };
    std::vector<Ref> all;
    all.reserve((size_t)(mgr.getNumFactoryPresets() + mgr.getNumUserPresets()));
    for (int i = 0; i < mgr.getNumFactoryPresets(); ++i) {
        const auto& p = mgr.getFactoryPreset(i);
        all.push_back({p.name, p.category, false, i});
    }
    for (int i = 0; i < mgr.getNumUserPresets(); ++i)
        all.push_back({mgr.getUserPresetName(i), mgr.getUserPresetCategory(i), true, i});

    if (isSearching()) {
        // Global search: flat results across every preset, regardless of
        // the selected category tab — never the grouped/collapsible tree,
        // per the "All"-only-shows-tree rule. Category selection is left
        // untouched so clearing the search restores it.
        for (auto& r : all)
            if (matchesSearch(r.name, r.category))
                entries.push_back({r.name, r.isUser, r.index, false});
        listBox.updateContent();
        resized(); repaint();
        return;
    }

    if (selectedCategory != "All") {
        // A specific sound-type category is selected: flat filtered list,
        // no collapsible headers needed since there's only one group.
        for (auto& r : all)
            if (r.category.equalsIgnoreCase(selectedCategory))
                entries.push_back({r.name, r.isUser, r.index, false});
        listBox.updateContent();
        resized(); repaint();
        return;
    }

    // "All": presets grouped by category as collapsible sections (click a
    // header to open/close it) instead of one long mixed list. First time
    // through: default every section collapsed except the one containing
    // the currently-loaded preset, so the browser opens compact (just
    // section names) but still shows where you are.
    if (!collapsedInitialised) {
        collapsedInitialised = true;
        juce::String currentCategory;
        for (auto& r : all) {
            if (r.name.equalsIgnoreCase(mgr.getCurrentPresetName())) {
                currentCategory = r.category;
                break;
            }
        }
        for (int i = 1; i < categoryNames.size(); ++i) {
            const auto& category = categoryNames[i];
            collapsed[category.toUpperCase()] = !category.equalsIgnoreCase(currentCategory);
        }
    }

    std::vector<int> remaining;
    for (int i = 0; i < (int)all.size(); ++i)
        remaining.push_back(i);

    // Init first, ungrouped, if present.
    for (auto it = remaining.begin(); it != remaining.end(); ) {
        if (all[(size_t)*it].category.equalsIgnoreCase("Init")) {
            const auto& r = all[(size_t)*it];
            entries.push_back({r.name, r.isUser, r.index, false});
            it = remaining.erase(it);
        } else {
            ++it;
        }
    }

    for (int ci = 1; ci < categoryNames.size(); ++ci) {
        const auto& category = categoryNames[ci];

        std::vector<int> inCategory;
        for (int i : remaining)
            if (all[(size_t)i].category.equalsIgnoreCase(category))
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
            const auto& r = all[(size_t)i];
            entries.push_back({r.name, r.isUser, r.index, false});
            remaining.erase(std::find(remaining.begin(), remaining.end(), i));
        }
    }

    listBox.updateContent();
    resized(); repaint();
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
    // A user preset's file can be missing or corrupted (deleted/edited
    // outside the app between refreshUserPresets() and this click) —
    // loadUserPreset() reports that via its return value. Previously this
    // was ignored, so a failed load still updated the displayed name and
    // fired onPresetLoaded as if it had succeeded, leaving the synth's
    // actual sound out of sync with what the UI claimed was loaded.
    const bool loaded = e.isUser
        ? mgr.loadUserPreset(e.sourceIndex, processor.parameters)
        : (mgr.applyPreset(mgr.getFactoryPreset(e.sourceIndex), processor.parameters), true);
    if (!loaded)
        return;

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
    // Same 8 factory sound-type categories offered everywhere else in the
    // browser, plus "User" (the default) first since it's the most common
    // choice for a quick save.
    static const juce::StringArray kSaveCategories {
        "User", "Bass", "Lead", "Pad", "Keys", "Brass", "Cinematic", "Performance", "FX",
        "LAB - Mid Bass Exploration 01", "LAB - Leads Exploration 01", "LAB - Plucks Exploration 01",
        "LAB - Keys Exploration 01", "LAB - FX Exploration 01", "LAB - Experimental 01"
    };

    auto* alertWindow = new juce::AlertWindow("Save Preset",
                                              "Enter a name and choose a category:",
                                              juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("name", processor.presetManager.getCurrentPresetName());
    alertWindow->addComboBox("category", kSaveCategories, "Category");
    if (auto* combo = alertWindow->getComboBoxComponent("category")) {
        const int idx = kSaveCategories.indexOf(processor.presetManager.getLastSaveCategory(), true);
        combo->setSelectedItemIndex(idx >= 0 ? idx : 0, juce::dontSendNotification);
    }
    alertWindow->addButton("Save",   1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, alertWindow](int result) {
            std::unique_ptr<juce::AlertWindow> aw(alertWindow); // deleted on every return path
            if (result != 1) return;

            const auto name = aw->getTextEditorContents("name").trim();
            if (name.isEmpty()) {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                    "Save Preset", "Please enter a preset name.");
                return;
            }
            juce::String category = "User";
            if (auto* combo = aw->getComboBoxComponent("category")) {
                const auto text = combo->getText();
                if (text.isNotEmpty()) category = text;
            }

            auto doSave = [this, name, category] {
                auto& mgr = processor.presetManager;
                mgr.saveUserPreset(name, category, processor.parameters);
                mgr.setCurrentPresetName(name);
                mgr.setLastSaveCategory(category);
                // Browser updates immediately — no restart needed. If the
                // new preset's category isn't the one currently selected
                // (or a search is filtering it out), it still exists and
                // will show up the moment that category/search is chosen;
                // rebuildEntries() alone is enough to make it live right now.
                rebuildEntries();
                for (int i = 0; i < (int)entries.size(); ++i) {
                    if (entries[(size_t)i].name == name) {
                        listBox.selectRow(i, false, true);
                        break;
                    }
                }
                if (onPresetLoaded) onPresetLoaded(name);
            };

            if (processor.presetManager.userPresetExists(name, category)) {
                juce::AlertWindow::showOkCancelBox(
                    juce::MessageBoxIconType::WarningIcon,
                    "Save Preset",
                    "Overwrite existing preset?",
                    "Overwrite", "Cancel",
                    nullptr,
                    juce::ModalCallbackFunction::create([doSave](int overwriteResult) {
                        if (overwriteResult == 1) doSave();
                    }));
            } else {
                doSave();
            }
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
    if (e.eventComponent == this) {
        // Hovering the browser background — check the category sidebar.
        int newHover = -1;
        for (int i = 0; i < (int)categoryTabRects.size(); ++i) {
            if (categoryTabRects[(size_t)i].contains(e.getPosition())) { newHover = i; break; }
        }
        if (newHover != hoveredCategoryIndex) {
            hoveredCategoryIndex = newHover;
            repaint();
        }
        return;
    }

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
    if (e.eventComponent == this && hoveredCategoryIndex >= 0) {
        hoveredCategoryIndex = -1;
        repaint();
    }
}

void PresetBrowser::mouseUp(const juce::MouseEvent& e)
{
    if (e.eventComponent != this) return;
    for (int i = 0; i < (int)categoryTabRects.size(); ++i) {
        if (categoryTabRects[(size_t)i].contains(e.getPosition())) {
            selectedCategory = categoryNames[i];
            // A search in progress would otherwise hide the effect of
            // picking a new tab (search results ignore the category
            // selection) — clear it so the tab click is immediately visible.
            if (isSearching()) searchBox.clear();
            rebuildEntries();
            listBox.selectRow(0);
            updateDeleteState();
            return;
        }
    }
}

bool PresetBrowser::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('f', juce::ModifierKeys::ctrlModifier, 0)) {
        searchBox.grabKeyboardFocus();
        searchBox.selectAll();
        return true;
    }
    if (key == juce::KeyPress::escapeKey && isSearching()) {
        searchBox.clear();
        rebuildEntries();
        return true;
    }
    return false;
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

    // Fixed left padding regardless of selection/user state — the row no
    // longer visibly shifts horizontally when it becomes selected, and text
    // reads as sitting inside a selectable item rather than glued to the edge.
    constexpr int indent = 14;
    // Selected text gets only a small brightness lift (never pure white);
    // normal rows stay softer, hover a touch brighter than resting state.
    const float textAlpha = selected ? 1.0f : (row == hoveredRow ? 0.88f : 0.78f);
    g.setColour(selected ? textCream.brighter(0.04f) : textCream.withAlpha(textAlpha));
    const auto entryBase = e.isUser ? Typography::popupEntryItalic() : Typography::popupEntry();
    const int entryWidth = w - indent - 10;
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

    // Category sidebar — click a name to filter the list on the right to
    // just that sound type; "All" (and "User", if present) show everything
    // in their respective scope.
    for (int i = 0; i < categoryNames.size() && i < (int)categoryTabRects.size(); ++i) {
        const auto r = categoryTabRects[(size_t)i].toFloat();
        const bool isSelected = categoryNames[i] == selectedCategory;
        if (isSelected) {
            g.setColour(selBg.brighter(0.05f));
            g.fillRoundedRectangle(r, 4.0f);
            g.setColour(accentGold.withAlpha(0.85f));
            g.fillRect(r.getX(), r.getY() + 2.0f, 3.0f, r.getHeight() - 4.0f);
        } else if (i == hoveredCategoryIndex) {
            g.setColour(hoverBg);
            g.fillRoundedRectangle(r, 4.0f);
        }
        // Resting tabs have no fill at all — they sit directly on the light
        // cream panel background, not the dark list — so they need dark
        // text, not the light cream used for selected/hover (which do have
        // a dark fill behind them). Using a light colour here was reading
        // as near-invisible white-on-cream.
        const bool onDarkFill = isSelected || i == hoveredCategoryIndex;
        g.setColour(onDarkFill ? textCream.brighter(isSelected ? 0.04f : 0.0f).withAlpha(isSelected ? 1.0f : 0.90f)
                               : juce::Colour(0xff2a2318).withAlpha(0.82f));
        g.setFont(Typography::fitToWidth(Typography::popupEntry(), categoryNames[i], r.getWidth() - 20.0f));
        g.drawText(categoryNames[i], r.reduced(14, 0).toNearestInt(), juce::Justification::centredLeft);
    }

    // Divider between the Factory sound-type tabs and the LAB session-folder
    // tabs, sitting in the extra gap resized() left above firstLabTabIndex.
    if (firstLabTabIndex > 0 && firstLabTabIndex < (int)categoryTabRects.size()) {
        const auto& r = categoryTabRects[(size_t)firstLabTabIndex];
        const float lineY = (float)r.getY() - 5.0f;
        g.setColour(sectionBdr.withAlpha(0.45f));
        g.drawLine((float)r.getX() + 2.0f, lineY, (float)r.getRight() - 2.0f, lineY, 0.8f);
    }

    // Vertical divider between the category sidebar and the preset list
    if (!categoryTabRects.empty()) {
        const float dividerX = listBox.getX() - 6.0f;
        g.setColour(sectionBdr.withAlpha(0.35f));
        g.drawLine(dividerX, (float)listBox.getY(), dividerX, (float)listBox.getBottom(), 0.8f);
    }
}

// ─── Layout ──────────────────────────────────────────────────────────────────

void PresetBrowser::resized()
{
    auto area = getLocalBounds().reduced(7);
    area.removeFromTop((int)kHeaderH + 5 + 4); // header strip + divider + gap

    searchBox.setBounds(area.removeFromTop(28));
    area.removeFromTop(6); // gap below search field

    auto btnRow = area.removeFromBottom(36);
    area.removeFromBottom(5);

    auto sidebar = area.removeFromLeft(128);
    area.removeFromLeft(10); // gap + divider line drawn in paint()

    categoryTabRects.clear();
    constexpr int tabH = 25;
    constexpr int tabGap = 2;
    int y = sidebar.getY();
    for (int i = 0; i < categoryNames.size(); ++i) {
        if (i == firstLabTabIndex)
            y += 10; // extra gap separating Factory tabs from LAB session folders
        categoryTabRects.push_back({sidebar.getX(), y, sidebar.getWidth(), tabH});
        y += tabH + tabGap;
    }

    listBox.setBounds(area);

    const int btnW = (btnRow.getWidth() - 8) / 3;
    loadBtn.setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(4);
    saveBtn.setBounds(btnRow.removeFromLeft(btnW));
    btnRow.removeFromLeft(4);
    deleteBtn.setBounds(btnRow);
}
