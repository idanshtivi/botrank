#include "Typography.h"
#include <map>
#include <tuple>

#if LADDERVOICE_HAS_EMBEDDED_FONTS
 #include "FontBinaryData.h"
#endif

namespace Typography
{
namespace
{
    juce::Typeface::Ptr loadEmbedded (const void* data, int size)
    {
        if (data == nullptr || size <= 0)
            return nullptr;

        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    // ---- Title family: Big Shoulders Display ExtraBold — a condensed,
    // industrial slab with the stamped-metal-nameplate feel of real vintage
    // hardware synth panels, rather than the more sci-fi/digital read of the
    // other candidates tried here. Static wght=800 instance produced with
    // `fonttools varLib.instancer` from Google Fonts' variable
    // BigShouldersDisplay[wght].ttf. Archivo Expanded Bold, then Archivo
    // Black, remain as static fallbacks if it ever fails to load. ----
    juce::Typeface::Ptr getTitleTypeface()
    {
#if LADDERVOICE_HAS_EMBEDDED_FONTS
        static const juce::Typeface::Ptr bigShoulders  = loadEmbedded (FontData::BigShouldersDisplayExtraBold_ttf, FontData::BigShouldersDisplayExtraBold_ttfSize);
        static const juce::Typeface::Ptr expandedBold  = loadEmbedded (FontData::ArchivoExpandedBold_ttf, FontData::ArchivoExpandedBold_ttfSize);
        static const juce::Typeface::Ptr black         = loadEmbedded (FontData::ArchivoBlackRegular_ttf, FontData::ArchivoBlackRegular_ttfSize);
        if (bigShoulders != nullptr) return bigShoulders;
        if (expandedBold != nullptr) return expandedBold;
        return black;
#else
        return nullptr;
#endif
    }

    // ---- Label family: Satoshi (Regular/Medium/Bold, plus a genuine Italic). ----
    // Satoshi has no dedicated SemiBold cut, so SemiBold falls back to Bold.
    juce::Typeface::Ptr getLabelTypeface (Weight weight)
    {
#if LADDERVOICE_HAS_EMBEDDED_FONTS
        static const juce::Typeface::Ptr regular = loadEmbedded (FontData::SatoshiRegular_otf, FontData::SatoshiRegular_otfSize);
        static const juce::Typeface::Ptr medium   = loadEmbedded (FontData::SatoshiMedium_otf,  FontData::SatoshiMedium_otfSize);
        static const juce::Typeface::Ptr bold     = loadEmbedded (FontData::SatoshiBold_otf,    FontData::SatoshiBold_otfSize);
        static const juce::Typeface::Ptr italic   = loadEmbedded (FontData::SatoshiItalic_otf,  FontData::SatoshiItalic_otfSize);

        switch (weight)
        {
            case Weight::Regular:  return regular;
            case Weight::Medium:   return medium;
            case Weight::SemiBold: return bold;
            case Weight::Bold:     return bold;
            case Weight::Italic:   return italic; // genuine italic cut, no synthesis needed
        }
#else
        juce::ignoreUnused (weight);
#endif
        return nullptr;
    }

    // ---- Mono family: IBM Plex Mono (Regular/Medium/SemiBold/Bold). ----
    juce::Typeface::Ptr getMonoTypeface (Weight weight)
    {
#if LADDERVOICE_HAS_EMBEDDED_FONTS
        static const juce::Typeface::Ptr regular  = loadEmbedded (FontData::IBMPlexMonoRegular_ttf,  FontData::IBMPlexMonoRegular_ttfSize);
        static const juce::Typeface::Ptr medium   = loadEmbedded (FontData::IBMPlexMonoMedium_ttf,   FontData::IBMPlexMonoMedium_ttfSize);
        static const juce::Typeface::Ptr semibold = loadEmbedded (FontData::IBMPlexMonoSemiBold_ttf, FontData::IBMPlexMonoSemiBold_ttfSize);
        static const juce::Typeface::Ptr bold     = loadEmbedded (FontData::IBMPlexMonoBold_ttf,     FontData::IBMPlexMonoBold_ttfSize);

        switch (weight)
        {
            case Weight::Regular:  return regular;
            case Weight::Medium:   return medium;
            case Weight::SemiBold: return semibold;
            case Weight::Bold:     return bold;
            case Weight::Italic:   return regular;
        }
#else
        juce::ignoreUnused (weight);
#endif
        return nullptr;
    }

    // Platform-default fonts — active only if a family's embedded binaries
    // are unavailable. Segoe UI for Title/Label (matches the app's prior
    // system-font baseline); Consolas for Mono (Windows' standard monospace).
    juce::Font fallbackFont (Family family, Weight weight, float pointSize)
    {
        int styleFlags = juce::Font::plain;
        switch (weight)
        {
            case Weight::Regular:                break;
            case Weight::Medium:
            case Weight::SemiBold:
            case Weight::Bold:   styleFlags = juce::Font::bold;   break;
            case Weight::Italic: styleFlags = juce::Font::italic; break;
        }

        const auto name = (family == Family::Mono) ? "Consolas" : "Segoe UI";
        return juce::Font (juce::FontOptions().withName (name).withPointHeight (pointSize)).withStyle (styleFlags);
    }

    juce::Font systemMonoFont (Weight weight, float pointSize)
    {
        int styleFlags = juce::Font::plain;
        if (weight == Weight::Bold || weight == Weight::SemiBold)
            styleFlags = juce::Font::bold;
        else if (weight == Weight::Italic)
            styleFlags = juce::Font::italic;

        return juce::Font (juce::FontOptions().withName ("Consolas").withPointHeight (pointSize)).withStyle (styleFlags);
    }
}

juce::Font font (Family family, Weight weight, float pointSize)
{
    juce::Typeface::Ptr typeface;
    switch (family)
    {
        case Family::Title: typeface = getTitleTypeface();        break;
        case Family::Label: typeface = getLabelTypeface (weight); break;
        case Family::Mono:  typeface = getMonoTypeface (weight);  break;
    }

    if (typeface == nullptr)
        return fallbackFont (family, weight, pointSize);

    auto result = juce::Font (juce::FontOptions().withTypeface (typeface).withPointHeight (pointSize));
    if (weight == Weight::Italic && family == Family::Mono)
        result = result.italicised(); // no dedicated italic file embedded for Mono; Label uses a genuine italic cut
    return result;
}

juce::Font trackedFont (Family family, Weight weight, float pointSize, float tracking)
{
    return font (family, weight, pointSize).withExtraKerningFactor (tracking);
}

float measuredWidth (const juce::Font& font, const juce::String& text)
{
    if (text.isEmpty())
        return 0.0f;

    juce::GlyphArrangement ga;
    ga.addLineOfText (font, text, 0.0f, 0.0f);
    return ga.getBoundingBox (0, -1, false).getWidth();
}

static juce::Font fitToWidthUncached (const juce::Font& base, const juce::String& text, float maxWidth)
{
    if (measuredWidth (base, text) <= maxWidth)
        return base;

    // Step 1 — relax tracking, first toward zero and then, only if that
    // still isn't enough, slightly negative (condensed).
    const float startTracking = base.getExtraKerningFactor();
    const float minTracking = juce::jmin (0.0f, startTracking);
    if (startTracking > minTracking)
    {
        for (float t = startTracking; t >= minTracking; t -= 0.005f)
        {
            auto candidate = base.withExtraKerningFactor (t);
            if (measuredWidth (candidate, text) <= maxWidth)
                return candidate;
        }
    }

    // Step 2 — tracking alone wasn't enough. Reduce point size by at most
    // 3% of the original, tracking held at the condensed floor.
    auto tight = base.withExtraKerningFactor (minTracking);
    const float originalPoints = base.getHeightInPoints();
    const float minPoints = originalPoints * 0.88f;
    for (float pts = originalPoints; pts >= minPoints; pts -= 0.05f)
    {
        auto candidate = tight.withPointHeight (pts);
        if (measuredWidth (candidate, text) <= maxWidth)
            return candidate;
    }

    return tight.withPointHeight (minPoints);
}

juce::Font fitToWidth (const juce::Font& base, const juce::String& text, float maxWidth)
{
    if (text.isEmpty() || maxWidth <= 0.0f)
        return base;

    // fitToWidth's search loop measures text with GlyphArrangement up to ~100
    // times when it doesn't fit at the base size. It's called from
    // paintListBoxItem on every repaint, and list hover repaints rows on
    // every mouse-move pixel — without caching, that turns hovering a preset
    // list into dozens of these expensive searches per second, which reads
    // as sticky/laggy mouse tracking rather than a sustained CPU spike.
    struct Key {
        juce::String text, typeface;
        float maxWidth, pointHeight, tracking;
        bool bold, italic;
        bool operator< (const Key& o) const {
            return std::tie (text, typeface, maxWidth, pointHeight, tracking, bold, italic)
                 < std::tie (o.text, o.typeface, o.maxWidth, o.pointHeight, o.tracking, o.bold, o.italic);
        }
    };
    static std::map<Key, juce::Font> cache;

    const Key key { text, base.getTypefaceName(), maxWidth, base.getHeightInPoints(),
                    base.getExtraKerningFactor(), base.isBold(), base.isItalic() };
    if (auto it = cache.find (key); it != cache.end())
        return it->second;

    auto result = fitToWidthUncached (base, text, maxWidth);
    cache[key] = result;
    return result;
}

juce::Font mainTitle()         { return trackedFont (Family::Title, Weight::Bold,     37.0f, -0.030f); }
// All panel-facing Label-family roles share the same letter-spacing (0.08)
// so every knob/combo/toggle/section caption reads consistently — previously
// only controlLabel/comboLabel had tracking, which is why some captions
// (MODE, GLIDE, LEGATO, section titles) looked untouched next to others.
// Sizes and tracking trimmed slightly from the prior pass (10.0/0.10 etc.)
// to relieve crowding between captions and the controls/boxes beneath them.
juce::Font subtitle()          { return trackedFont (Family::Label, Weight::Regular,   9.5f, 0.08f); }
juce::Font sectionTitle()      { return trackedFont (Family::Label, Weight::Bold,     13.5f, 0.13f); }
juce::Font controlLabel()      { return trackedFont (Family::Label, Weight::Regular,  12.0f, 0.08f); }
juce::Font compactLabel()      { return trackedFont (Family::Label, Weight::Regular,   9.4f, 0.08f); }
juce::Font comboLabel()        { return trackedFont (Family::Label, Weight::Regular,  12.0f, 0.08f); }
juce::Font comboBoxText()      { return trackedFont (Family::Label, Weight::Bold,     10.8f, 0.08f); }
juce::Font toggleLabelLarge()  { return trackedFont (Family::Label, Weight::Regular,   9.0f, 0.08f); }
// Matches controlLabel/comboLabel (12.0) — this is the Controllers panel's
// push-button caption (GLIDE/LEGATO/RETRIGGER/KEYBOARD), the only medium-tier
// toggle in the app, and it previously sat at a noticeably smaller size than
// every other caption in that same panel.
juce::Font toggleLabelMedium() { return trackedFont (Family::Label, Weight::Regular,  12.0f, 0.08f); }
juce::Font buttonLabel()       { return trackedFont (Family::Label, Weight::Bold,     10.8f, 0.08f); }
juce::Font valueDisplay()      { return systemMonoFont (Weight::Bold, 11.4f); }
juce::Font popupTitle()        { return font        (Family::Label, Weight::SemiBold, 12.8f); }
juce::Font popupGroupHeader()  { return font        (Family::Label, Weight::SemiBold, 10.5f); }
juce::Font popupEntry()        { return trackedFont (Family::Label, Weight::Regular,  12.5f,  0.002f); }
juce::Font popupEntryItalic()  { return trackedFont (Family::Label, Weight::Italic,   12.5f,  0.002f); }
juce::Font footer()            { return trackedFont (Family::Label, Weight::Regular,  10.5f, 0.08f); }

void drawEngraved (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                   juce::Colour baseColour, juce::Justification justification)
{
    if (text.isEmpty())
        return;

    g.setColour (juce::Colours::black.withAlpha (0.40f));
    g.drawText (text, area.translated (-1, -1), justification);
    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawText (text, area.translated (1, 1), justification);
    g.setColour (baseColour);
    g.drawText (text, area, justification);
}
}
