#pragma once

#include <juce_graphics/juce_graphics.h>

// Single source of truth for every text style in the interface.
//
// Three embedded families, one role each (see Resources/Fonts/README.md):
//   Title — Archivo Expanded Bold (brand/logo text only)
//   Label — Satoshi (every panel/knob/section label)
//   Mono  — IBM Plex Mono (numeric readouts/displays only)
//
// Falls back to a platform system font per family if its embedded binaries
// are ever unavailable, so the build and app always work either way.
namespace Typography
{
    enum class Weight
    {
        Regular,
        Medium,
        SemiBold,
        Bold,
        Italic
    };

    enum class Family
    {
        Title,  // Archivo Expanded Bold
        Label,  // Satoshi
        Mono    // IBM Plex Mono
    };

    // Generic building blocks -------------------------------------------------

    // Font at the given family/weight/size, no added letter-spacing.
    juce::Font font (Family family, Weight weight, float pointSize);

    // Font at the given family/weight/size with additional letter-spacing,
    // in juce::Font::withExtraKerningFactor units. Positive opens the
    // tracking, negative tightens it.
    juce::Font trackedFont (Family family, Weight weight, float pointSize, float tracking);

    // Measured rendered width of `text` set in `font` (honours the font's own
    // tracking), in pixels.
    float measuredWidth (const juce::Font& font, const juce::String& text);

    // Returns a font derived from `base` that is guaranteed to fit `text`
    // within `maxWidth` pixels: (1) unchanged if it already fits, (2)
    // tracking relaxed toward zero and then slightly negative (condensed,
    // down to -0.08) if still needed, (3) point size trimmed by at most 3%
    // as a last resort. Never enlarges size, never changes family/weight.
    juce::Font fitToWidth (const juce::Font& base, const juce::String& text, float maxWidth);

    // Draws `text` with an engraved/debossed look — as if cut into the
    // panel material rather than printed flat on top of it: a dark shadow
    // offset up-left (the groove wall facing away from the light) and a
    // faint light highlight offset down-right (the wall catching it),
    // with `baseColour` drawn on top at the exact position. `g` must
    // already have `font` set as the current font before calling.
    void drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                       juce::Colour baseColour, juce::Justification justification);

    // Named roles ---------------------------------------------------------
    // Point sizes/tracking preserved from the prior approved pass unless
    // noted; only typeface family/weight change here.

    juce::Font mainTitle();
    juce::Font subtitle();
    juce::Font sectionTitle();
    juce::Font controlLabel();
    juce::Font compactLabel();
    juce::Font comboLabel();
    juce::Font comboBoxText();
    juce::Font toggleLabelLarge();
    juce::Font toggleLabelMedium();
    juce::Font buttonLabel();
    juce::Font valueDisplay();
    juce::Font popupTitle();
    juce::Font popupGroupHeader();
    juce::Font popupEntry();
    juce::Font popupEntryItalic();
    juce::Font footer();
}
