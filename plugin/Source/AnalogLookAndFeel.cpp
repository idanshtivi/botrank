#include "AnalogLookAndFeel.h"
#include "BinaryData.h"

namespace {
juce::FontOptions uiFont(float size, int style = juce::Font::plain)
{
    return juce::FontOptions("Segoe UI", size, style);
}

const auto accentGold = juce::Colour(0xff96783c);
const auto valueFill = juce::Colour(0xff514a3c);
const auto valueText = juce::Colour(0xffebe8d7);
const auto softBorder = juce::Colour(0xff736e5f);
}

AnalogLookAndFeel::AnalogLookAndFeel()
{
    setColour(juce::Slider::thumbColourId, juce::Colour(0xff050505));
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(0xffd3a54d));
    setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(0xff151512));
    setColour(juce::Slider::textBoxTextColourId, valueText);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0x00101010));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x002c281e));
    setColour(juce::Label::textColourId, juce::Colour(0xff171713));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff151511));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xfff1dfb7));
    setColour(juce::ComboBox::outlineColourId, accentGold);
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
            const int frameIndex = juce::jlimit(0, filmstripFrameCount - 1,
                                                juce::roundToInt(sliderPosProportional * static_cast<float>(filmstripFrameCount - 1)));
            const int sourceY = frameIndex * frameHeight;
            const float frameAspect = static_cast<float>(strip.getWidth()) / static_cast<float>(frameHeight);
            auto dest = knobBox;
            if (frameAspect > 1.0f)
                dest = dest.withHeight(dest.getWidth() / frameAspect).withCentre(knobBox.getCentre());
            else
                dest = dest.withWidth(dest.getHeight() * frameAspect).withCentre(knobBox.getCentre());

            g.setOpacity(alpha);
            g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
            const auto destInt = dest.toNearestInt();
            g.drawImage(strip,
                        destInt.getX(), destInt.getY(), destInt.getWidth(), destInt.getHeight(),
                        0, sourceY, strip.getWidth(), frameHeight);
            g.setOpacity(1.0f);
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
    juce::ignoreUnused(shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    auto r = button.getLocalBounds().toFloat().reduced(3.0f);
    auto textArea = r;
    auto switchArea = textArea.removeFromLeft(38.0f).withSizeKeepingCentre(28.0f, 30.0f);
    textArea.removeFromLeft(3.0f);
    const bool on = button.getToggleState();

    g.setColour(juce::Colour(0x70000000));
    g.fillRoundedRectangle(switchArea.translated(0.0f, 2.0f), 5.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff4a4a43), switchArea.getX(), switchArea.getY(),
                                           juce::Colour(0xff050505), switchArea.getRight(), switchArea.getBottom(), false));
    g.fillRoundedRectangle(switchArea, 5.0f);
    g.setColour(accentGold.withAlpha(0.72f));
    g.drawRoundedRectangle(switchArea.reduced(0.5f), 5.0f, 0.95f);

    auto well = switchArea.reduced(5.0f, 5.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff050505), well.getX(), well.getY(),
                                           juce::Colour(0xff1c1c18), well.getRight(), well.getBottom(), false));
    g.fillRoundedRectangle(well, 3.0f);

    auto cap = well.withTrimmedTop(on ? 2.0f : well.getHeight() * 0.45f)
                   .withTrimmedBottom(on ? well.getHeight() * 0.45f : 2.0f);
    if (on) {
        g.setColour(juce::Colour(0x65ffbc33));
        g.fillEllipse(well.withSizeKeepingCentre(13.0f, 13.0f).translated(0.0f, -4.0f));
    }
    g.setGradientFill(juce::ColourGradient(on ? juce::Colour(0xffd8a64d) : juce::Colour(0xff77766f),
                                           cap.getX(), cap.getY(),
                                           on ? juce::Colour(0xff4b2e0f) : juce::Colour(0xff242421),
                                           cap.getRight(), cap.getBottom(), false));
    g.fillRoundedRectangle(cap, 2.5f);
    g.setColour(juce::Colour(0x99000000));
    g.drawRoundedRectangle(cap, 2.5f, 1.0f);

    g.setColour(button.isEnabled() ? juce::Colour(0xff151511) : juce::Colour(0xff77756c));
    g.setFont(uiFont(10.3f, juce::Font::bold));
    g.drawFittedText(button.getButtonText().toUpperCase(),
                     textArea.toNearestInt().reduced(1, 0),
                     juce::Justification::centredLeft, 1, 0.82f);
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
    g.setColour(box.isEnabled() ? accentGold.withAlpha(0.88f) : softBorder.withAlpha(0.55f));
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
    const auto top = lightAction ? (shouldDrawButtonAsDown ? juce::Colour(0xffc9b990) : juce::Colour(0xfffff1d0))
                                 : (shouldDrawButtonAsDown ? juce::Colour(0xff11110f) : juce::Colour(0xff30302b));
    const auto bottom = lightAction ? (shouldDrawButtonAsDown ? juce::Colour(0xfff1e6c8) : juce::Colour(0xffb28a46))
                                    : (shouldDrawButtonAsDown ? juce::Colour(0xff2b2b26) : juce::Colour(0xff070706));

    g.setColour(juce::Colour(0x72000000));
    g.fillRoundedRectangle(r.translated(0.0f, 1.6f), 5.0f);
    g.setGradientFill(juce::ColourGradient(top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(shouldDrawButtonAsHighlighted ? juce::Colour(0xffffce70) : accentGold.withAlpha(0.90f));
    g.drawRoundedRectangle(r.reduced(0.4f), 5.0f, shouldDrawButtonAsHighlighted ? 1.35f : 0.95f);
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
    g.setFont(uiFont(13.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(4, 1),
                     juce::Justification::centred, 1, 0.85f);
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
        g.setFont(uiFont(13.8f, juce::Font::bold));
        g.drawText(label.getText(), label.getLocalBounds().reduced(3, 0), juce::Justification::centred, false);
        return;
    }

    LookAndFeel_V4::drawLabel(g, label);
}

juce::Font AnalogLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    juce::ignoreUnused(box);
    return uiFont(12.0f, juce::Font::bold);
}

void AnalogLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(9, 1, box.getWidth() - 30, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
    label.setColour(juce::Label::textColourId, box.isEnabled() ? juce::Colour(0xffffdf9a) : juce::Colour(0xff77756c));
    label.setInterceptsMouseClicks(false, false);
}
