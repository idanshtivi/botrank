#include "AnalogLookAndFeel.h"
#include "BinaryData.h"
#include "Typography.h"

namespace {
// Mirrors Theme:: in PluginEditor.cpp (separate file/namespace, so kept in
// sync by hand) — updated alongside this session's warm-brown palette pass.
// valueFill in particular was too close to the new panel's own brown to
// read as a distinct inset display.
// Toned back down from an earlier, more saturated pass — at full/near-full
// alpha on 9 combo-box outlines plus every knob's ticks, that brightness
// stacked into a genuinely tiring amount of bright warm accent on screen.
const auto accentGold = juce::Colour(0xffab8148);
const auto valueFill = juce::Colour(0xff342c20);
const auto valueText = juce::Colour(0xffebe8d7);
const auto softBorder = juce::Colour(0xff3c3020);
// Muted bronze-brown, distinct from accentGold — combo box borders in
// particular still read as a bright contrasting yellow ring even after
// accentGold itself was toned down; this keeps the same warm family but
// desaturated enough to look like part of the chassis, not a decal.
const auto comboBorder = juce::Colour(0xff7a6242);

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
}

AnalogLookAndFeel::AnalogLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff050505));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd3a54d));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff151512));
    setColour(juce::Slider::textBoxTextColourId, valueText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00101010));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x002c281e));
    // Default Label fallback — muted (was a brighter cream, dialled back
    // now that the engraved shadow/highlight carries legibility instead of
    // flat colour contrast). Most labels set their own colour explicitly;
    // this only covers any that don't.
    setColour(juce::Label::textColourId, juce::Colour(0xffcbbc9c));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff151511));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xfff1dfb7));
    setColour(juce::ComboBox::outlineColourId, comboBorder);
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xfff1dfb7));
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff171713));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xfff1dfb7));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff3a2f19));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colour(0xffffedc1));

    largeKnobStrip = juce::ImageFileFormat::loadFrom(BinaryData::knob_large_strip_png,
                                                     BinaryData::knob_large_strip_pngSize);
    mediumKnobStrip = juce::ImageFileFormat::loadFrom(BinaryData::knob_medium_strip_png,
                                                      BinaryData::knob_medium_strip_pngSize);
    smallKnobStrip = juce::ImageFileFormat::loadFrom(BinaryData::knob_small_strip_png,
                                                     BinaryData::knob_small_strip_pngSize);
}

void AnalogLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPosProportional, float rotaryStartAngle,
                                         float rotaryEndAngle, juce::Slider& slider)
{
    const auto alpha = slider.isEnabled() ? 1.0f : 0.38f;
    const auto whole = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                              static_cast<float>(width), static_cast<float>(height));
    const auto compactControl = slider.getName().startsWithIgnoreCase("loudness");
    const auto utilityControl = slider.getName().startsWithIgnoreCase("lfo")
        || slider.getName() == "analogDrift"
        || slider.getName() == "modWheelAmount";
    const auto knobBox = whole.reduced((compactControl || utilityControl) ? 0.0f : 2.0f,
                                       (compactControl || utilityControl) ? 0.0f : 1.0f);

    const auto& strip = knobBox.getWidth() >= 100.0f ? largeKnobStrip
                       : (knobBox.getWidth() >= 76.0f ? mediumKnobStrip : smallKnobStrip);
    constexpr int filmstripFrameCount = 101;
    if (strip.isValid() && strip.getWidth() > 0 && strip.getHeight() >= filmstripFrameCount) {
        const int frameHeight = strip.getHeight() / filmstripFrameCount;
        if (frameHeight > 0) {
            // Sub-frame rotation instead of cross-fading two frames: each
            // frame is a fully independent raster (ticks, brushed-metal
            // texture noise etc. don't line up pixel-for-pixel between
            // adjacent frames), so blending two of them made those static
            // details visibly shimmer/waver during rotation instead of
            // staying put. Drawing a single frame and rotating it by the
            // small residual angle keeps every pixel consistent — ticks and
            // texture rotate together with the pointer, as they physically
            // would, with no blending artifacts.
            const float framePos = juce::jlimit(0.0f, static_cast<float>(filmstripFrameCount - 1),
                                                sliderPosProportional * static_cast<float>(filmstripFrameCount - 1));
            const int frameA = static_cast<int>(framePos);
            const float frameFrac = framePos - static_cast<float>(frameA);
            const int sourceY = frameA * frameHeight;
            const float perFrameAngle = (rotaryEndAngle - rotaryStartAngle) / static_cast<float>(filmstripFrameCount - 1);
            const float subFrameRotation = frameFrac * perFrameAngle;
            const float frameAspect = static_cast<float>(strip.getWidth()) / static_cast<float>(frameHeight);
            auto dest = knobBox;
            if (frameAspect > 1.0f)
                dest = dest.withHeight(dest.getWidth() / frameAspect).withCentre(knobBox.getCentre());
            else
                dest = dest.withWidth(dest.getHeight() * frameAspect).withCentre(knobBox.getCentre());

            // Solid backing behind the face before the image — pixel-sampled
            // the source art directly and the pale "face" pixels are only
            // ~20% opaque (a shading/highlight pass, not a solid fill), so
            // they were letting whatever sits behind them (the panel's own
            // colour, which varies by knob position) bleed through. Against
            // the old light cream panel that accidentally looked fine;
            // against the new warm-brown panel it read as inconsistent,
            // slightly-different-per-knob murkiness ("something's off").
            // A solid warm-pewter disc gives the translucent face a
            // consistent backing to blend against instead.
            const auto faceDiameter = juce::jmin(dest.getWidth(), dest.getHeight()) * 0.55f;
            const auto face = juce::Rectangle<float>(faceDiameter, faceDiameter).withCentre(dest.getCentre());
            g.setColour(juce::Colour(0xffc7c0ac).withMultipliedAlpha(alpha));
            g.fillEllipse(face);

            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            const auto destInt = dest.toNearestInt();
            {
                juce::Graphics::ScopedSaveState saveState(g);
                if (subFrameRotation != 0.0f)
                    g.addTransform(juce::AffineTransform::rotation(subFrameRotation, dest.getCentreX(), dest.getCentreY()));
                g.setOpacity(alpha);
                g.drawImage(strip,
                            destInt.getX(), destInt.getY(), destInt.getWidth(), destInt.getHeight(),
                            0, sourceY, strip.getWidth(), frameHeight);
                g.setOpacity(1.0f);
            }

            // Dimming ring removed — at 45% it made the tick marks around
            // the rim too weak/hard to see at the edges. Ticks now render
            // exactly as the source art has them, full brightness.
            // A custom amber pointer + accent dot was tried here (drawn on
            // top of the source art's own thin pointer) to improve
            // readability at small sizes, but read as an unwanted look
            // change rather than an improvement — reverted back to relying
            // solely on the source art's own indicator.
            return;
        }
    }

    const auto diameter = juce::jmin(knobBox.getWidth(), knobBox.getHeight());
    const bool large = diameter >= 55.0f;
    const auto bounds = juce::Rectangle<float>(diameter, diameter).withCentre(knobBox.getCentre()).reduced(large ? 1.0f : 0.5f);
    const auto radius = bounds.getWidth() * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
    const float outerRingWidth = large ? 10.0f : 6.0f;
    const float chromeWidth = large ? 2.6f : 1.7f;
    const int tickCount = large ? 25 : 17;

    auto shadow = bounds.translated(2.0f, large ? 5.0f : 3.2f).expanded(large ? 4.4f : 2.6f);
    g.setColour(juce::Colour(0xcc000000).withMultipliedAlpha(alpha));
    g.fillEllipse(shadow);

    // Black outside ring with integrated gold ticks.
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff34342f).withMultipliedAlpha(alpha), bounds.getX(), bounds.getY(),
                                           juce::Colour(0xff020202).withMultipliedAlpha(alpha), bounds.getRight(), bounds.getBottom(), false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xff000000).withMultipliedAlpha(alpha));
    g.drawEllipse(bounds.reduced(0.5f), large ? 2.0f : 1.3f);

    for (int i = 0; i < tickCount; ++i) {
        const auto a = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * static_cast<float>(i) / static_cast<float>(tickCount - 1);
        const bool major = i == 0 || i == tickCount / 2 || i == tickCount - 1;
        const auto inner = centre.getPointOnCircumference(radius - outerRingWidth * (major ? 0.88f : 0.72f), a);
        const auto outer = centre.getPointOnCircumference(radius - 2.2f, a);
        g.setColour((major ? juce::Colour(0xffffd88d) : juce::Colour(0xffc28f3c)).withMultipliedAlpha(alpha));
        g.drawLine({inner, outer}, major ? (large ? 2.0f : 1.35f) : (large ? 1.25f : 0.85f));
    }

    const auto innerWell = bounds.reduced(outerRingWidth);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff11110f).withMultipliedAlpha(alpha), innerWell.getX(), innerWell.getY(),
                                           juce::Colour(0xff000000).withMultipliedAlpha(alpha), innerWell.getRight(), innerWell.getBottom(), false));
    g.fillEllipse(innerWell);

    const auto chromeRing = innerWell.reduced(large ? 2.4f : 1.6f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2eee3).withMultipliedAlpha(alpha), chromeRing.getX() + chromeRing.getWidth() * 0.18f, chromeRing.getY(),
                                           juce::Colour(0xff464640).withMultipliedAlpha(alpha), chromeRing.getRight(), chromeRing.getBottom(), false));
    g.fillEllipse(chromeRing);
    g.setColour(juce::Colour(0xccffffff).withMultipliedAlpha(alpha));
    g.drawEllipse(chromeRing.reduced(0.8f), large ? 1.3f : 0.85f);
    g.setColour(juce::Colour(0xaa000000).withMultipliedAlpha(alpha));
    g.drawEllipse(chromeRing.reduced(chromeWidth + 0.6f), large ? 1.5f : 1.0f);

    const auto face = chromeRing.reduced(chromeWidth + (large ? 2.4f : 1.5f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff4f1e9).withMultipliedAlpha(alpha), face.getX() + face.getWidth() * 0.28f, face.getY() + face.getHeight() * 0.08f,
                                           juce::Colour(0xff3c3d39).withMultipliedAlpha(alpha), face.getRight(), face.getBottom(), false));
    g.fillEllipse(face);

    // Radial brushed metal face.
    for (int i = 0; i < 54; ++i) {
        const float a1 = juce::MathConstants<float>::twoPi * static_cast<float>(i) / 54.0f;
        const float a2 = juce::MathConstants<float>::twoPi * static_cast<float>(i + 1) / 54.0f;
        juce::Path sector;
        sector.startNewSubPath(centre);
        sector.lineTo(centre.getPointOnCircumference(face.getWidth() * 0.5f, a1));
        sector.lineTo(centre.getPointOnCircumference(face.getWidth() * 0.5f, a2));
        sector.closeSubPath();
        const auto c = (i % 6 == 1) ? juce::Colour(0x38ffffff)
                                    : ((i % 6 == 4) ? juce::Colour(0x2d000000) : juce::Colour(0x0fffffff));
        g.setColour(c.withMultipliedAlpha(alpha));
        g.fillPath(sector);
    }

    g.setColour(juce::Colour(0x32ffffff).withMultipliedAlpha(alpha));
    for (int i = 0; i < 72; ++i) {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float>(i) / 72.0f;
        const auto p1 = centre.getPointOnCircumference(face.getWidth() * 0.07f, a);
        const auto p2 = centre.getPointOnCircumference(face.getWidth() * 0.46f, a);
        g.drawLine({p1, p2}, large ? 0.32f : 0.22f);
    }

    auto shine = face.reduced(face.getWidth() * 0.12f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x70ffffff).withMultipliedAlpha(alpha), shine.getX(), shine.getY(),
                                           juce::Colour(0x00000000), shine.getCentreX(), shine.getBottom(), false));
    g.fillEllipse(shine);

    g.setColour(juce::Colour(0x66000000).withMultipliedAlpha(alpha));
    g.drawEllipse(face.reduced(face.getWidth() * 0.06f), large ? 0.95f : 0.65f);
    g.setColour(juce::Colour(0x82ffffff).withMultipliedAlpha(alpha));
    g.drawEllipse(face.reduced(face.getWidth() * 0.18f), large ? 0.8f : 0.55f);

    // Rotating black groove pointer with a bright cap.
    const auto pointerWidth = large ? 7.0f : 5.0f;
    const auto pointerLength = radius * (large ? 0.68f : 0.64f);
    const auto pointerTop = -face.getHeight() * 0.47f;
    const auto pointerTransform = juce::AffineTransform::rotation(angle).translated(centre.x, centre.y);

    juce::Path pointerPocket;
    pointerPocket.addRoundedRectangle(-pointerWidth * 0.5f, pointerTop,
                                      pointerWidth, pointerLength, large ? 2.4f : 1.7f);
    g.setColour(juce::Colour(0xee000000).withMultipliedAlpha(alpha));
    g.fillPath(pointerPocket, pointerTransform);

    juce::Path pointerHighlight;
    pointerHighlight.startNewSubPath(-pointerWidth * 0.18f, pointerTop + 2.0f);
    pointerHighlight.lineTo(-pointerWidth * 0.18f, pointerTop + pointerLength - 4.0f);
    g.setColour(juce::Colour(0x66ffffff).withMultipliedAlpha(alpha));
    g.strokePath(pointerHighlight, juce::PathStrokeType(large ? 0.75f : 0.55f), pointerTransform);

    juce::Path cap;
    cap.addRoundedRectangle(-pointerWidth * 0.72f, pointerTop - (large ? 5.5f : 4.0f),
                            pointerWidth * 1.44f, large ? 7.0f : 5.4f, large ? 1.6f : 1.2f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xffffedc5).withMultipliedAlpha(alpha), 0.0f, pointerTop - 5.0f,
                                           juce::Colour(0xff8e6a32).withMultipliedAlpha(alpha), 0.0f, pointerTop + 2.0f, false));
    g.fillPath(cap, pointerTransform);
    g.setColour(juce::Colour(0xaa000000).withMultipliedAlpha(alpha));
    g.strokePath(cap, juce::PathStrokeType(large ? 0.8f : 0.6f), pointerTransform);
}

void AnalogLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool isOn  = button.getToggleState();
    const bool hover = shouldDrawButtonAsHighlighted;
    const bool down  = shouldDrawButtonAsDown;

    // Housing size tiers: xlarge (h≥78) → rocker switch, medium (h≥22) → push button, small → rocker switch
    const bool xlarge = bounds.getHeight() >= 78.0f;
    const bool medium = !xlarge && bounds.getHeight() >= 22.0f;
    const float houW = xlarge ? 40.0f : 28.0f;
    const float houH = juce::jmin(xlarge ? 88.0f : 40.0f, bounds.getHeight() - 2.0f);
    auto hou = juce::Rectangle<float>(
        bounds.getX() + 2.0f,
        bounds.getCentreY() - houH * 0.5f,
        houW, houH);
    // Push button moves on press; rocker switch stays fixed — toggle happens on release
    if (down && medium) hou = hou.translated(0.4f, 1.0f);
    const float houCr = 2.5f;

    // MEDIUM: Vintage push button — square bakelite frame + dark face + amber LED pill, label above
    if (medium)
    {
        const bool pressed = isOn || down;
        const float frameCr = 4.5f;
        const float faceCr  = 3.5f;

        // Label above the square frame
        const float labelH = 18.0f;
        const float availH = bounds.getHeight() - labelH - 1.0f;
        const float frameSize = juce::jmin(bounds.getWidth() - 4.0f, availH);
        const auto frame = juce::Rectangle<float>(frameSize, frameSize)
            .withCentre({ bounds.getCentreX(),
                          bounds.getY() + labelH + 1.0f + frameSize * 0.5f });

        // LABEL — muted cream, engraved like every other caption on the
        // panel (was a brighter flat colour with no shadow/highlight).
        const auto pushLabelText = button.getButtonText().toUpperCase();
        const auto pushLabelBounds = juce::Rectangle<float>(bounds.getX(), bounds.getY(),
                                                             bounds.getWidth(), labelH).toNearestInt();
        g.setFont(Typography::fitToWidth(Typography::toggleLabelMedium(), pushLabelText, static_cast<float>(pushLabelBounds.getWidth())));
        Typography::drawEngraved(g, pushLabelText, pushLabelBounds,
                                 button.isEnabled() ? juce::Colour(0xffcbbc9c) : juce::Colour(0xff8a7d68),
                                 juce::Justification::centred);

        // Snap to pixel grid for crisp rendering
        const auto fr = frame.toNearestInt().toFloat();

        // Drop shadow — simulated with a few offset, decreasing-alpha
        // rounded rects instead of juce::DropShadow. DropShadow rasterizes
        // and Gaussian-blurs a fresh image on every single paint call with
        // no caching; called for every button on every repaint, that's
        // real, avoidable CPU cost on top of an already-busier frame this
        // session (per-knob solid backing fill, image draw + rotation
        // transform, tick-dimming ring). This gives a comparable soft-edge
        // look for a few cheap fillRoundedRectangle calls.
        {
            const int   offset = pressed ? 1 : 3;
            const float baseAlpha = pressed ? 0.28f : 0.55f;
            for (int i = 3; i >= 1; --i) {
                const float t = static_cast<float>(i) / 3.0f;
                g.setColour(juce::Colours::black.withAlpha(baseAlpha * (1.0f - t) + 0.06f));
                g.fillRoundedRectangle(fr.translated(static_cast<float>(offset) * t, static_cast<float>(offset + 2) * t)
                                          .expanded(t * 1.5f),
                                       frameCr + t * 1.5f);
            }
        }

        // FRAME — bakelite, pixel-crisp
        {
            // Original bakelite warm-brown gradient
            juce::ColourGradient fg(
                juce::Colour(0xff3c3028), fr.getX(), fr.getY(),
                juce::Colour(0xff1a1410), fr.getX(), fr.getBottom(), false);
            fg.addColour(0.50, juce::Colour(0xff24201a));
            g.setGradientFill(fg);
            g.fillRoundedRectangle(fr, frameCr);

            // Outer black border — 1.5px crisp
            g.setColour(juce::Colour(0xff050402));
            g.drawRoundedRectangle(fr.reduced(0.75f), frameCr, 1.5f);

            // Bronze/brass inlay — same muted comboBorder tone used for
            // dropdowns (was a separate, brighter hardcoded gold that stood
            // out against the warm-brown panel the same way the combo box
            // borders did before that fix).
            g.setColour(comboBorder.withAlpha(hover ? 0.90f : 0.75f));
            g.drawRoundedRectangle(fr.reduced(1.8f), frameCr - 0.8f, 1.2f);

            // All 4 bevel edges drawn as explicit lines (avoids rounded-rect blur on sides)
            const float bx1 = fr.getX()    + frameCr;
            const float bx2 = fr.getRight() - frameCr;
            const float by1 = fr.getY()    + frameCr;
            const float by2 = fr.getBottom() - frameCr;
            // Top light
            g.setColour(juce::Colours::white.withAlpha(0.30f));
            g.drawLine(bx1, fr.getY()+1.5f, bx2, fr.getY()+1.5f, 1.2f);
            // Left light (slightly fainter)
            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawLine(fr.getX()+1.5f, by1, fr.getX()+1.5f, by2, 1.0f);
            // Bottom shadow
            g.setColour(juce::Colours::black.withAlpha(0.78f));
            g.drawLine(bx1, fr.getBottom()-1.5f, bx2, fr.getBottom()-1.5f, 1.5f);
            // Right shadow
            g.setColour(juce::Colours::black.withAlpha(0.58f));
            g.drawLine(fr.getRight()-1.5f, by1, fr.getRight()-1.5f, by2, 1.2f);

            // Specular highlight — soft diagonal glare from the top-left,
            // simulating studio-light reflecting off glossy bakelite
            // plastic. Fills only the rounded-rect frame shape itself, so
            // no extra clipping is needed.
            g.setGradientFill(juce::ColourGradient(
                juce::Colours::white.withAlpha(hover ? 0.20f : 0.14f), fr.getX(), fr.getY(),
                juce::Colours::transparentWhite,
                fr.getX() + fr.getWidth() * 0.65f, fr.getY() + fr.getHeight() * 0.6f, false));
            g.fillRoundedRectangle(fr, frameCr);
        }

        // PUSH FACE — snapped square inset from frame
        const auto face = fr.reduced(5.0f).toNearestInt().toFloat();
        {
            if (pressed)
            {
                // Sunken — darker, inner shadow top+left
                juce::ColourGradient fg2(
                    juce::Colour(0xff0c0a08), face.getX(), face.getY(),
                    juce::Colour(0xff1c1814), face.getX(), face.getBottom(), false);
                g.setGradientFill(fg2);
                g.fillRoundedRectangle(face, faceCr);
                g.setColour(juce::Colours::black.withAlpha(0.60f));
                g.drawLine(face.getX()+2, face.getY()+1.5f, face.getRight()-2, face.getY()+1.5f, 1.5f);
                g.drawLine(face.getX()+1.5f, face.getY()+2, face.getX()+1.5f, face.getBottom()-2, 1.2f);
                g.setColour(juce::Colours::white.withAlpha(0.07f));
                g.drawLine(face.getX()+2, face.getBottom()-1.5f, face.getRight()-2, face.getBottom()-1.5f, 1.0f);
            }
            else
            {
                // Raised — lighter at top, darker at bottom
                juce::ColourGradient fg2(
                    juce::Colour(0xff2a2420), face.getX(), face.getY(),
                    juce::Colour(0xff131110), face.getX(), face.getBottom(), false);
                fg2.addColour(0.40, juce::Colour(0xff1c1814));
                g.setGradientFill(fg2);
                g.fillRoundedRectangle(face, faceCr);
                // Centred top sheen
                {
                    juce::ColourGradient sheen(
                        juce::Colours::white.withAlpha(0.12f), face.getCentreX(), face.getY(),
                        juce::Colours::transparentBlack,        face.getCentreX(), face.getY() + face.getHeight() * 0.42f, false);
                    g.setGradientFill(sheen);
                    g.fillRoundedRectangle(face, faceCr);
                }
                // Crisp 4-side bevel
                g.setColour(juce::Colours::white.withAlpha(hover ? 0.18f : 0.11f));
                g.drawLine(face.getX()+2, face.getY()+1.5f, face.getRight()-2, face.getY()+1.5f, 1.0f);
                g.drawLine(face.getX()+1.5f, face.getY()+2, face.getX()+1.5f, face.getBottom()-2, 0.8f);
                g.setColour(juce::Colours::black.withAlpha(0.72f));
                g.drawLine(face.getX()+2, face.getBottom()-1.5f, face.getRight()-2, face.getBottom()-1.5f, 1.4f);
                g.drawLine(face.getRight()-1.5f, face.getY()+2, face.getRight()-1.5f, face.getBottom()-2, 1.1f);
            }
            g.setColour(juce::Colour(0xff050504));
            g.drawRoundedRectangle(face.reduced(0.75f), faceCr, 1.5f);
        }

        // AMBER LED PILL — centred near top of face, pixel-snapped
        {
            const float pillW = face.getWidth() * 0.40f;
            const float pillH = juce::jmin(5.5f, face.getHeight() * 0.22f);
            const float pillCr = pillH * 0.5f;
            const auto pill = juce::Rectangle<float>(pillW, pillH)
                .withCentre({face.getCentreX(), face.getY() + face.getHeight() * 0.26f})
                .toNearestInt().toFloat();

            // Dark socket surround — always visible
            g.setColour(juce::Colour(0xff080706));
            g.fillRoundedRectangle(pill.expanded(1.2f, 0.8f), pillCr + 0.8f);

            if (isOn)
            {
                // Lamp fill — warm amber base
                g.setColour(juce::Colour(0xffdd8800));
                g.fillRoundedRectangle(pill, pillCr);
                // Brightness gradient — lighter at top centre
                {
                    juce::ColourGradient lg(
                        juce::Colour(0x90ffdd66), pill.getCentreX(), pill.getY(),
                        juce::Colours::transparentBlack, pill.getCentreX(), pill.getBottom(), false);
                    g.setGradientFill(lg);
                    g.fillRoundedRectangle(pill, pillCr);
                }
                // Tiny white specular spot
                g.setColour(juce::Colours::white.withAlpha(0.70f));
                g.fillEllipse(pill.getCentreX() - 1.8f, pill.getY() + pillH * 0.18f, 3.5f, 1.8f);
            }
            else
            {
                // Unlit lamp — very dark brown
                g.setColour(juce::Colour(0xff1e160a));
                g.fillRoundedRectangle(pill, pillCr);
            }
        }

        return;
    }

    // LAYER 1: CAST SHADOW
    if (medium)
    {
        g.setColour(juce::Colours::black.withAlpha(down ? 0.35f : 0.82f));
        g.fillRoundedRectangle(hou.translated(2.5f, down ? 2.0f : 4.0f).expanded(1.5f), houCr + 1.0f);
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.28f));
        g.fillRoundedRectangle(hou.translated(1.0f, 2.0f).expanded(0.5f), houCr + 0.5f);
    }

    // LAYER 2: BAKELITE FRAME — warm dark brown, clearly different from black well
    {
        juce::ColourGradient hg(
            juce::Colour(0xff3c3028), hou.getX(), hou.getY(),
            juce::Colour(0xff1a1410), hou.getX(), hou.getBottom(), false);
        hg.addColour(0.50, juce::Colour(0xff24201a));
        g.setGradientFill(hg);
        g.fillRoundedRectangle(hou, houCr);

        g.setColour(juce::Colour(0xff080604));
        g.drawRoundedRectangle(hou.reduced(0.5f), houCr, 1.5f);

        // EDGE WEAR — same muted comboBorder tone as the push buttons and
        // dropdowns (was the same brighter hardcoded gold).
        g.setColour(comboBorder.withAlpha(hover ? 0.80f : 0.65f));
        g.drawRoundedRectangle(hou.reduced(1.0f), houCr, 0.8f);

        // Top-left bevel highlight
        g.setColour(juce::Colours::white.withAlpha(hover ? 0.38f : 0.28f));
        g.drawLine(hou.getX() + 2.0f, hou.getY() + 1.0f,
                   hou.getRight() - 2.0f, hou.getY() + 1.0f, 1.3f);
        g.setColour(juce::Colours::white.withAlpha(hover ? 0.22f : 0.15f));
        g.drawLine(hou.getX() + 1.0f, hou.getY() + 2.0f,
                   hou.getX() + 1.0f, hou.getBottom() - 2.0f, 1.0f);

        // Bottom-right shadow bevel
        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.drawLine(hou.getX() + 2.0f, hou.getBottom() - 1.0f,
                   hou.getRight() - 2.0f, hou.getBottom() - 1.0f, 1.5f);
        g.drawLine(hou.getRight() - 1.0f, hou.getY() + 2.0f,
                   hou.getRight() - 1.0f, hou.getBottom() - 2.0f, 1.3f);
    }

    // LAYER 3: RECESSED WELL — pure black cavity
    const auto well = hou.reduced(5.0f, 5.0f);
    const float wellCr = 1.5f;
    {
        g.setColour(juce::Colour(0xff000000));
        g.fillRoundedRectangle(well, wellCr);
        g.setColour(juce::Colours::black.withAlpha(0.90f));
        g.fillRoundedRectangle(well.withHeight(well.getHeight() * 0.28f), wellCr);
        g.drawRoundedRectangle(well.reduced(0.3f), wellCr, 1.1f);
    }

    // LAYER 4: PIVOT ROCKER LEVER
    // Lever ~12% narrower/shorter than well — track stays full size
    auto lev = well.reduced(3.0f, 5.5f);
    // ON = "I" (top) pressed down → lever shifts down; OFF = "O" (bottom) pressed down → lever shifts up
    lev = lev.translated(0.0f, isOn ? 3.0f : -3.0f);
    const float levCr = 1.5f;
    const float pivotPos = 0.46f;

    // Contact shadow
    g.setColour(juce::Colours::black.withAlpha(isOn ? 0.65f : 0.40f));
    g.fillRoundedRectangle(lev.translated(1.2f, isOn ? 1.2f : 2.5f).expanded(0.8f), levCr + 0.5f);

    // Lever base fill
    g.setColour(juce::Colour(0xff1e1c18));
    g.fillRoundedRectangle(lev, levCr);

    // Tilt shadow — on the pressed/down end
    {
        const float shadowH = lev.getHeight() * 0.55f;
        if (isOn)
        {
            // ON: top pressed down → shadow at top
            auto shadowZone = lev.withHeight(shadowH);
            juce::ColourGradient sg(
                juce::Colours::black.withAlpha(0.70f), shadowZone.getX(), shadowZone.getY(),
                juce::Colours::black.withAlpha(0.0f),  shadowZone.getX(), shadowZone.getBottom(), false);
            g.setGradientFill(sg);
            g.fillRoundedRectangle(shadowZone, levCr);
        }
        else
        {
            // OFF: bottom pressed down → shadow at bottom
            auto shadowZone = lev.withTrimmedTop(lev.getHeight() - shadowH);
            juce::ColourGradient sg(
                juce::Colours::black.withAlpha(0.0f),  shadowZone.getX(), shadowZone.getY(),
                juce::Colours::black.withAlpha(0.70f), shadowZone.getX(), shadowZone.getBottom(), false);
            g.setGradientFill(sg);
            g.fillRoundedRectangle(shadowZone, levCr);
        }
    }

    // Lever border
    g.setColour(juce::Colours::black.withAlpha(0.90f));
    g.drawRoundedRectangle(lev.reduced(0.4f), levCr, 1.0f);

    // Specular on raised edge
    const float specAlpha = hover ? 0.42f : 0.32f;
    if (isOn)
    {
        // ON: bottom raised → specular at bottom
        g.setColour(juce::Colours::white.withAlpha(specAlpha));
        g.drawLine(lev.getX() + 2.0f, lev.getBottom() - 0.8f,
                   lev.getRight() - 2.0f, lev.getBottom() - 0.8f, 2.2f);
        g.setColour(juce::Colours::white.withAlpha(specAlpha * 0.5f));
        g.drawLine(lev.getX() + 3.0f, lev.getBottom() - 2.5f,
                   lev.getRight() - 3.0f, lev.getBottom() - 2.5f, 1.2f);
    }
    else
    {
        // OFF: top raised → specular at top
        g.setColour(juce::Colours::white.withAlpha(specAlpha));
        g.drawLine(lev.getX() + 2.0f, lev.getY() + 0.8f,
                   lev.getRight() - 2.0f, lev.getY() + 0.8f, 2.2f);
        g.setColour(juce::Colours::white.withAlpha(specAlpha * 0.5f));
        g.drawLine(lev.getX() + 3.0f, lev.getY() + 2.5f,
                   lev.getRight() - 3.0f, lev.getY() + 2.5f, 1.2f);
    }

    // Pivot crease
    {
        const float py = lev.getY() + lev.getHeight() * pivotPos;
        g.setColour(juce::Colours::black.withAlpha(isOn ? 0.55f : 0.35f));
        g.drawLine(lev.getX() + 1.5f, py, lev.getRight() - 1.5f, py, 1.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.62f));
    g.drawLine(lev.getX() + 2.5f, lev.getBottom() - 1.0f,
               lev.getRight() - 2.5f, lev.getBottom() - 1.0f, 1.1f);

    // I/O markings — debossed, xlarge only
    if (xlarge)
    {
        const float cx    = lev.getCentreX();
        const float alpha = 0.36f;

        // "I" — short vertical line, upper quarter of lever
        {
            const float iy  = lev.getY()  + lev.getHeight() * 0.13f;
            const float iy2 = lev.getY()  + lev.getHeight() * 0.27f;
            g.setColour(juce::Colours::black.withAlpha(alpha * 0.9f));
            g.drawLine(cx + 0.7f, iy + 0.7f, cx + 0.7f, iy2 + 0.7f, 1.3f);
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawLine(cx, iy, cx, iy2, 1.3f);
        }

        // "O" — small circle, lower quarter of lever
        {
            const float oR  = lev.getWidth() * 0.13f;
            const float oCY = lev.getBottom() - lev.getHeight() * 0.20f;
            g.setColour(juce::Colours::black.withAlpha(alpha * 0.9f));
            g.drawEllipse(cx - oR + 0.7f, oCY - oR + 0.7f, oR * 2.0f, oR * 2.0f, 1.2f);
            g.setColour(juce::Colours::white.withAlpha(alpha));
            g.drawEllipse(cx - oR, oCY - oR, oR * 2.0f, oR * 2.0f, 1.2f);
        }
    }

    // LABEL (xlarge and small rocker switches only — medium returns early
    // above) — muted and engraved, same reasoning as the medium push-button
    // label.
    const auto rockerLabelColour = button.isEnabled() ? juce::Colour(0xffcbbc9c) : juce::Colour(0xff8a7d68);
    const auto labelText = button.getButtonText().toUpperCase();
    if (xlarge)
    {
        // Centred above the housing for large switches
        const auto textBounds = juce::Rectangle<float>(
            hou.getX() - 4.0f,
            hou.getY() - 18.0f,
            houW + 8.0f,
            16.0f).toNearestInt();
        g.setFont(Typography::fitToWidth(Typography::toggleLabelLarge(), labelText, static_cast<float>(textBounds.getWidth())));
        Typography::drawEngraved(g, labelText, textBounds, rockerLabelColour, juce::Justification::centred);
    }
    else
    {
        // To the right of the housing for standard switches
        const auto textBounds = juce::Rectangle<float>(
            bounds.getX() + houW + 7.0f,
            hou.getY(),
            bounds.getWidth() - houW - 7.0f,
            houH).toNearestInt();
        g.setFont(Typography::fitToWidth(Typography::toggleLabelLarge(), labelText, static_cast<float>(textBounds.getWidth())));
        Typography::drawEngraved(g, labelText, textBounds, rockerLabelColour, juce::Justification::centredLeft);
    }
}

void AnalogLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                                     int buttonX, int buttonY, int buttonW, int buttonH,
                                     juce::ComboBox& box)
{
    juce::ignoreUnused(isButtonDown, buttonX, buttonY, buttonW, buttonH);
    auto r = juce::Rectangle<float>(0, 0, static_cast<float>(width), static_cast<float>(height)).reduced(1.0f);
    g.setColour(juce::Colour(0x72000000));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 4.5f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a2a25), 0.0f, 0.0f,
                                           juce::Colour(0xff080807), 0.0f, static_cast<float>(height), false));
    g.fillRoundedRectangle(r, 4.5f);
    // Alpha eased from 0.88 — 9 combo boxes on screen at once each drawing
    // a near-fully-opaque gold ring made the panel busier than intended.
    g.setColour(box.isEnabled() ? comboBorder.withAlpha(0.75f) : softBorder.withAlpha(0.55f));
    g.drawRoundedRectangle(r, 4.5f, 1.0f);
    g.setColour(juce::Colour(0x22fff0c0));
    g.drawRoundedRectangle(r.reduced(3.0f), 3.0f, 0.55f);
    g.setColour(juce::Colour(0x44000000));
    g.drawLine(3.0f, static_cast<float>(height) - 3.0f, static_cast<float>(width) - 3.0f, static_cast<float>(height) - 3.0f, 1.0f);
    g.setColour(box.isEnabled() ? juce::Colour(0xffffdf9a) : juce::Colour(0xff77756c));
    juce::Path arrow;
    arrow.addTriangle(width - 18.0f, height * 0.38f, width - 8.0f, height * 0.38f,
                      width - 13.0f, height * 0.68f);
    g.fillPath(arrow);
}

void AnalogLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                             const juce::Colour& backgroundColour,
                                             bool shouldDrawButtonAsHighlighted,
                                             bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(backgroundColour);
    auto r = button.getLocalBounds().toFloat().reduced(1.0f);
    const bool lightAction = button.getButtonText().equalsIgnoreCase("INIT");
    // Preset prev/next nav buttons get the same muted comboBorder outline as
    // every other interactive control on the panel (combo boxes, push
    // buttons) instead of the brighter accentGold chrome meant for standalone
    // action buttons like INIT — that bright border read as mismatched next
    // to the rest of the preset strip.
    const bool navButton = button.getName() == "presetNav";
    const auto top = lightAction ? (shouldDrawButtonAsDown ? juce::Colour(0xffc9b990) : juce::Colour(0xfffff1d0))
                                 : (shouldDrawButtonAsDown ? juce::Colour(0xff11110f) : juce::Colour(0xff30302b));
    const auto bottom = lightAction ? (shouldDrawButtonAsDown ? juce::Colour(0xfff1e6c8) : juce::Colour(0xffb28a46))
                                    : (shouldDrawButtonAsDown ? juce::Colour(0xff2b2b26) : juce::Colour(0xff070706));

    g.setColour(juce::Colour(0x72000000));
    g.fillRoundedRectangle(r.translated(0.0f, 1.6f), 5.0f);
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 5.0f);
    if (navButton) {
        g.setColour(comboBorder.withAlpha(shouldDrawButtonAsHighlighted ? 0.90f : 0.70f));
        g.drawRoundedRectangle(r.reduced(0.4f), 5.0f, shouldDrawButtonAsHighlighted ? 1.1f : 0.85f);
    } else {
        g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xffffce70) : accentGold.withAlpha(0.90f));
        g.drawRoundedRectangle(r.reduced(0.4f), 5.0f, shouldDrawButtonAsHighlighted ? 1.35f : 0.95f);
    }
    g.setColour(lightAction ? juce::Colour(0x88ffffff) : juce::Colour(0x32fff0c0));
    g.drawLine(r.getX() + 5.0f, r.getY() + 2.0f, r.getRight() - 5.0f, r.getY() + 2.0f, 0.9f);
}

void AnalogLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                       bool shouldDrawButtonAsHighlighted,
                                       bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    const bool lightAction = button.getButtonText().equalsIgnoreCase("INIT");
    g.setColour(lightAction ? juce::Colour(0xff16130d) : juce::Colour(0xffffdf9a));
    const auto buttonText = button.getButtonText();
    const auto r = button.getLocalBounds().reduced(4, 1).toFloat();
    constexpr float manualTrackPx = 0.35f;
    // drawTrackedText adds manualTrackPx of hand-positioned spacing between
    // every glyph on top of whatever the font itself measures — reserve that
    // extra width up front so the combined result still fits inside r.
    const float manualTrackingWidth = juce::jmax(0, buttonText.length() - 1) * manualTrackPx;
    g.setFont(Typography::fitToWidth(Typography::buttonLabel(), buttonText, r.getWidth() - manualTrackingWidth));
    drawTrackedText(g, buttonText,
                    r.getX(), r.getY(), r.getWidth(), r.getHeight(),
                    manualTrackPx, juce::Justification::centred);
}

void AnalogLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    const auto bounds = label.getLocalBounds().toFloat();
    const bool editableReadout = label.isEditable();

    if (editableReadout) {
        auto r = bounds.reduced(1.0f, 1.0f);
        g.setColour(juce::Colour(0x24000000));
        g.fillRoundedRectangle(r.translated(0.0f, 0.8f), 3.5f);
        g.setGradientFill(juce::ColourGradient(valueFill.brighter(0.18f), r.getX(), r.getY(),
                                               valueFill.darker(0.02f), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle(r, 3.5f);
        g.setColour(accentGold.withAlpha(0.30f));
        g.drawRoundedRectangle(r.reduced(0.2f), 3.5f, 0.55f);
        g.setColour(juce::Colour(0x2affffff));
        g.drawLine(r.getX() + 5.0f, r.getY() + 1.0f, r.getRight() - 5.0f, r.getY() + 1.0f, 0.65f);
        g.setColour(valueText);
        const auto valueTextArea = label.getLocalBounds().reduced(3, 0);
        g.setFont(Typography::fitToWidth(Typography::valueDisplay(), label.getText(), static_cast<float>(valueTextArea.getWidth())));
        g.drawText(label.getText(), valueTextArea, juce::Justification::centred, false);
        return;
    }

    // Every plain caption (WAVE, RANGE, MODE, DETUNE, section titles drawn
    // via a Label, etc.) gets the same engraved/debossed look as the rest
    // of the panel's text, instead of LookAndFeel_V4's flat default.
    if (!label.isBeingEdited()) {
        const auto border = label.getBorderSize();
        const auto textArea = border.subtractedFrom(label.getLocalBounds());
        g.setFont(label.getFont());
        Typography::drawEngraved(g, label.getText(), textArea,
                                 label.findColour(juce::Label::textColourId),
                                 label.getJustificationType());
        return;
    }

    LookAndFeel_V4::drawLabel(g, label);
}

juce::Font AnalogLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    const auto base = Typography::comboBoxText();

    // Text area matches positionComboBoxText's label bounds (9,1,w-30,h-2)
    // minus the Label's own default 5px-each-side border.
    const int usableWidth = box.getWidth() - 30 - 10;
    if (usableWidth <= 0)
        return base;

    // Fit against the box's own widest item, not just its current selection,
    // so every item renders at one consistent size and nothing truncates
    // if the user later picks a longer entry.
    juce::String widest;
    float widestW = 0.0f;
    for (int i = 0; i < box.getNumItems(); ++i)
    {
        const auto item = box.getItemText(i);
        const float w = Typography::measuredWidth(base, item);
        if (w > widestW) { widestW = w; widest = item; }
    }

    if (widest.isEmpty())
        return base;

    return Typography::fitToWidth(base, widest, static_cast<float>(usableWidth));
}

void AnalogLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(9, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, box.isEnabled() ? juce::Colour(0xffffdf9a) : juce::Colour(0xff77756c));
    label.setInterceptsMouseClicks(false, false);
}

void AnalogLookAndFeel::drawScrollbar(juce::Graphics& g, juce::ScrollBar& /*scrollbar*/,
                                      int x, int y, int width, int height,
                                      bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                                      bool isMouseOver, bool isMouseDown)
{
    // Track — dark, matches the recessed list background
    const auto track = juce::Rectangle<int>(x, y, width, height).toFloat();
    g.setColour(juce::Colour(0xff16130e));
    g.fillRoundedRectangle(track, 3.0f);
    g.setColour(juce::Colour(0xff5b574b).withAlpha(0.40f));
    g.drawRoundedRectangle(track.reduced(0.5f), 3.0f, 0.7f);

    if (thumbSize <= 0) return;

    // Thumb — warm amber/gold, brightens on hover/press
    const float thumbAlpha = isMouseDown ? 0.95f : (isMouseOver ? 0.78f : 0.55f);
    juce::Rectangle<float> thumb;
    if (isScrollbarVertical)
        thumb = { (float)x + 2.0f, (float)(y + thumbStartPosition) + 1.5f,
                  (float)width - 4.0f, (float)thumbSize - 3.0f };
    else
        thumb = { (float)(x + thumbStartPosition) + 1.5f, (float)y + 2.0f,
                  (float)thumbSize - 3.0f, (float)height - 4.0f };

    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xffc89040).withAlpha(thumbAlpha), thumb.getX(), thumb.getY(),
        juce::Colour(0xff7a5a28).withAlpha(thumbAlpha), thumb.getX(), thumb.getBottom(), false));
    g.fillRoundedRectangle(thumb, 2.5f);
    g.setColour(juce::Colour(0x38ffffff));
    g.drawRoundedRectangle(thumb.reduced(0.5f), 2.5f, 0.7f);
}
