#include "ColourSwatchEditor.h"

ColourSwatchEditor::ColourSwatchEditor (const juce::String& labelText)
{
    label.setText (labelText, juce::dontSendNotification);
    label.setFont (juce::FontOptions (11.0f));
    label.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.75f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
}

void ColourSwatchEditor::setColour (juce::Colour c, juce::NotificationType notification)
{
    if (colour == c)
        return;

    colour = c;
    if (picker != nullptr)
        closePicker (true);
    repaint();

    if (notification != juce::dontSendNotification && onColourChanged != nullptr)
        onColourChanged (colour);
}

void ColourSwatchEditor::setExpanded (bool shouldExpand)
{
    if (shouldExpand == isExpanded())
        return;
    if (shouldExpand)
        openPicker();
    else
        closePicker (true);
}

int ColourSwatchEditor::getPreferredHeight() const
{
    constexpr int kLabelH = 18;
    constexpr int kLabelGap = 2;
    constexpr int kRowGap = 6;
    int h = kLabelH + kLabelGap + kSwatchRowH + kRowGap;
    if (picker != nullptr)
        h += 4 + RampColorPickerPanel::kPreferredHeight;
    return h;
}

void ColourSwatchEditor::paint (juce::Graphics& g)
{
    auto sw = swatchBounds.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (sw, 3.0f);
    g.setColour (colour);
    g.fillRoundedRectangle (sw.reduced (1.0f), 3.0f);
    g.setColour (juce::Colours::whitesmoke.withAlpha (isExpanded() ? 0.85f : 0.35f));
    g.drawRoundedRectangle (sw, 3.0f, 1.0f);
}

void ColourSwatchEditor::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (18));
    area.removeFromTop (2);
    auto row = area.removeFromTop (kSwatchRowH);
    swatchBounds = row.removeFromLeft (juce::jmin (72, row.getWidth())).reduced (0, 2);

    if (picker != nullptr)
    {
        area.removeFromTop (4);
        picker->setBounds (area.removeFromTop (RampColorPickerPanel::kPreferredHeight));
    }
}

void ColourSwatchEditor::mouseDown (const juce::MouseEvent& e)
{
    if (swatchBounds.contains (e.getPosition()) || label.getBounds().contains (e.getPosition()))
    {
        if (isExpanded())
            closePicker (true);
        else
            openPicker();
    }
}

void ColourSwatchEditor::openPicker()
{
    picker = std::make_unique<RampColorPickerPanel> (colour);
    picker->onColourChanged = [this] (juce::Colour c)
    {
        colour = c;
        repaint();
        if (onColourChanged != nullptr)
            onColourChanged (colour);
    };
    picker->onDone = [this] { closePicker (true); };
    addAndMakeVisible (*picker);
    resized();
    if (onHeightChanged != nullptr)
        onHeightChanged();
    repaint();
}

void ColourSwatchEditor::closePicker (bool notifyHeight)
{
    if (picker == nullptr)
        return;
    picker.reset();
    resized();
    if (notifyHeight && onHeightChanged != nullptr)
        onHeightChanged();
    repaint();
}
