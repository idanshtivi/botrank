#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

class AnalogLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    AnalogLookAndFeel();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawScrollbar(juce::Graphics& g, juce::ScrollBar& scrollbar,
                       int x, int y, int width, int height,
                       bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                       bool isMouseOver, bool isMouseDown) override;

    // Full custom repaint of AlertWindow (Save/Rename/Import/Delete dialogs)
    // rather than just recolouring LookAndFeel_V4's own drawAlertBox: that
    // default draws the icon glyph and the title+message textLayout as two
    // independently-positioned elements whose reserved widths can disagree,
    // which read as overlapping/doubled text once the background stopped
    // being a low-contrast grey — drawing the pre-built textLayout ourselves,
    // once, into the exact rect JUCE already reserves for it sidesteps that
    // entirely, and drops the icon glyph rather than reposition it.
    void drawAlertBox(juce::Graphics& g, juce::AlertWindow& alert,
                      const juce::Rectangle<int>& textArea, juce::TextLayout& textLayout) override;
    int getAlertBoxWindowFlags() override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;

    juce::Font getComboBoxFont(juce::ComboBox& box) override;
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override;
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

private:
    juce::Image largeKnobStrip;
    juce::Image mediumKnobStrip;
    juce::Image smallKnobStrip;
};
