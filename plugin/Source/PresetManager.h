#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

struct PresetData {
    juce::String name;
    juce::String category;
    std::vector<std::pair<juce::String, float>> params;
    // Snapshot of `name` as originally built/embedded, taken once at
    // startup before any rename override is applied — stays fixed across
    // however many times the preset gets renamed in-app, so the persisted
    // rename record always has a stable key to update in place rather than
    // growing a rename chain. Not used for anything except that lookup.
    juce::String originalName;
};

class PresetManager {
public:
    PresetManager();

    int getNumFactoryPresets() const { return (int)factoryPresets.size(); }
    const PresetData& getFactoryPreset(int i) const { return factoryPresets[(size_t)i]; }

    // Removes a factory (compiled-into-the-binary) preset from the live
    // list immediately and persists the hide across restarts — same
    // pattern as hideLabFolder(), just at individual-preset granularity.
    // Does not touch the shipped binary: a fresh install of the plugin
    // always has the full library; only this local install remembers the
    // hide. Returns false if no matching factory preset was found.
    bool hideFactoryPreset(const juce::String& name, const juce::String& category);

    // Renames a factory (compiled-into-the-binary) preset in place and
    // persists it across restarts — same idea as hideFactoryPreset(), just
    // changing the name instead of removing it. Fails without changing
    // anything if index is out of range or newName collides with another
    // preset (factory or user) already in the same category.
    bool renameFactoryPreset(int index, const juce::String& newName);

    juce::File getUserPresetFolder() const;
    void refreshUserPresets();
    int getNumUserPresets() const { return (int)userPresetFiles.size(); }
    juce::String getUserPresetName(int i) const;
    // Reads just the "category" attribute out of a user preset file, without
    // touching APVTS. Defaults to "User" if the file predates the category
    // field or the attribute is otherwise missing/blank — old presets with
    // no category are automatically treated as "User".
    juce::String getUserPresetCategory(int i) const;
    juce::File getUserPresetFile(int i) const { return userPresetFiles[(size_t)i]; }

    void applyPreset(const PresetData& p, juce::AudioProcessorValueTreeState& apvts) const;
    bool loadUserPreset(int index, juce::AudioProcessorValueTreeState& apvts);
    bool loadPresetFromFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts) const;

    // True if a user preset with this exact name already exists in this
    // exact category (the two together determine the on-disk filename, so
    // the same name in a different category is a different preset, not a
    // collision).
    bool userPresetExists(const juce::String& name, const juce::String& category) const;

    juce::File saveUserPreset(const juce::String& name, const juce::String& category,
                              juce::AudioProcessorValueTreeState& apvts);
    bool deleteUserPreset(int index);

    // Deletes every user preset filed under `category` (case-insensitive) —
    // used by the sidebar's right-click "Delete Category". Factory presets
    // sharing that category name are untouched (they're compiled into the
    // app, not files), so the tab only disappears once none are left in it.
    // Returns how many files were deleted.
    int deleteUserPresetsInCategory(const juce::String& category);

    // A handful of "LAB - ..." session folders are pinned in the sidebar by
    // PresetBrowser even while empty (so they exist as a save target before
    // any preset lives in them yet). That pinning is otherwise permanent, so
    // deleting every preset in one wouldn't make its tab disappear like a
    // normal category does — these two persist which of those pinned names
    // the user has explicitly deleted, so PresetBrowser can stop force-
    // showing them. A category reappears on its own (via normal discovery)
    // the moment a preset is saved into it again, hidden or not.
    juce::StringArray getHiddenLabFolders() const;
    void hideLabFolder(const juce::String& name);

    // Moves every user preset filed under oldCategory (case-insensitive) to
    // newCategory — keeps each preset's own name, just its category/bank.
    // A preset whose name would collide with one already in newCategory is
    // left where it was rather than overwritten. Returns how many were moved.
    int renameUserPresetsCategory(const juce::String& oldCategory, const juce::String& newCategory);

    // Renames a user preset in place (keeps its category and parameters,
    // changes only the "name" attribute and the on-disk filename that
    // encodes it). Fails without changing anything if newName is empty or
    // collides with a different preset already in the same category.
    bool renameUserPreset(int index, const juce::String& newName);

    // Imports an externally-created .ladderpreset file (e.g. hand-written or
    // generated by another tool) into the user presets folder. Validates the
    // file is a well-formed LadderVoicePreset before copying anything. If a
    // preset with the same name+category already exists, the import is not
    // rejected or silently overwritten — it's saved under an auto-numbered
    // name ("Foo (2)", "Foo (3)", ...) so batch-importing several files never
    // stops partway through on a single collision. On failure, errorOut is
    // set to a user-facing reason and the folder is left untouched.
    bool importPresetFile(const juce::File& sourceFile, juce::String& errorOut);
    bool importPresetFile(const juce::File& sourceFile, const juce::String& destinationCategory, juce::String& errorOut);

    // Imports every .ladderpreset file directly inside `folder` as one named
    // bank: the category becomes the folder's own name (overriding whatever,
    // if anything, each file's own category attribute says), so an entire
    // externally-created preset folder shows up as its own tab in the
    // browser, grouped together exactly as it was organized on disk. Returns
    // how many files were imported; per-file failures are appended to errors
    // rather than aborting the rest of the folder.
    int importPresetFolder(const juce::File& folder, juce::StringArray& errors);

    const juce::String& getCurrentPresetName() const { return currentPresetName; }
    void setCurrentPresetName(const juce::String& n) { currentPresetName = n; }

    // Remembered for the lifetime of the plugin instance (not just one
    // popup open) so re-saving into the same category doesn't require
    // reselecting it every time the Save dialog is opened.
    const juce::String& getLastSaveCategory() const { return lastSaveCategory; }
    void setLastSaveCategory(const juce::String& c) { lastSaveCategory = c; }

    // Combined navigation: factory presets come first, then user presets
    int getTotalPresetCount() const { return getNumFactoryPresets() + getNumUserPresets(); }
    void loadPresetAtIndex(int combinedIndex, juce::AudioProcessorValueTreeState& apvts);
    juce::String getPresetNameAtIndex(int combinedIndex) const;

private:
    std::vector<PresetData> factoryPresets;
    std::vector<juce::File> userPresetFiles;
    juce::String currentPresetName { "Init" };
    juce::String lastSaveCategory { "User" };
    int currentPresetIndex = 0;

    void buildFactoryPresets();
    // Removes any factory preset previously hidden via hideFactoryPreset()
    // (persisted to hidden_factory_presets.txt) from the in-memory list.
    // Called once at startup, after every factory preset (hand-written and
    // embedded) has been loaded.
    void applyHiddenFactoryPresetsFilter();
    // Applies any persisted factory-preset renames (from
    // factory_preset_renames.txt) to the in-memory list. Called once at
    // startup, after originalName has been snapshotted for every entry and
    // before applyHiddenFactoryPresetsFilter() runs.
    void applyFactoryPresetRenames();
    // Appends every embedded .ladderpreset resource (compiled into the binary
    // from the top-level presets/ tree — see plugin/CMakeLists.txt) to
    // factoryPresets, right after the hand-written ones built by
    // buildFactoryPresets(). No-op if the plugin was built without any
    // presets/ files present (LADDERVOICE_HAS_EMBEDDED_PRESETS undefined).
    void loadEmbeddedFactoryPresets();
    static juce::String sanitizeFilename(const juce::String& name);
    // Category folded into the filename so "Bass/My Lead" and "Pad/My Lead"
    // are different files, not a collision.
    juce::File userPresetFile(const juce::String& name, const juce::String& category) const;
    static PresetData parsePresetXml(const juce::XmlElement& xml);
    static std::unique_ptr<juce::XmlElement> presetToXml(const juce::String& name,
                                                          const juce::String& category,
                                                          juce::AudioProcessorValueTreeState& apvts);
    friend class LadderVoiceAudioProcessor;
    int currentCombinedIndex = 0;
};
