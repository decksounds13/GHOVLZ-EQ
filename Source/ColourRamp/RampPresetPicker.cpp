#include "RampPresetPicker.h"
#include "../ComboBoxLookAndFeel.h"

void paintRampSwatch (juce::Graphics& g, juce::Rectangle<float> bounds,
                      const GradientRamp& ramp, float corner)
{
    if (bounds.getWidth() < 1.0f || bounds.getHeight() < 1.0f)
        return;

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bounds, corner);

    if (ramp.stops.size() < 2)
        return;

    juce::ColourGradient grad (ramp.colourAt (0.0f), bounds.getX(), bounds.getCentreY(),
                               ramp.colourAt (1.0f), bounds.getRight(), bounds.getCentreY(), false);
    for (size_t i = 1; i + 1 < ramp.stops.size(); ++i)
        grad.addColour ((double) ramp.stops[i].position, ramp.stops[i].colour);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds.reduced (1.0f), corner);
}

class RampPresetPicker::Row final : public juce::Component
{
public:
    Row (juce::String nameIn, GradientRamp rampIn,
         std::function<void()> onApplyIn, std::function<void()> onDeleteIn)
        : name (std::move (nameIn)),
          ramp (std::move (rampIn)),
          onApply (std::move (onApplyIn)),
          onDelete (std::move (onDeleteIn))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (4.0f, 3.0f);
        if (hovered)
        {
            g.setColour (juce::Colours::goldenrod.withAlpha (0.18f));
            g.fillRoundedRectangle (bounds.expanded (2.0f), 3.0f);
        }

        auto swatch = bounds.removeFromLeft (bounds.getWidth() * 0.55f).reduced (0.0f, 2.0f);
        paintRampSwatch (g, swatch, ramp, 2.5f);

        bounds.removeFromLeft (8.0f);
        g.setColour (PluginMenuTheme::text().withAlpha (0.95f));
        g.setFont (juce::FontOptions (12.5f));
        g.drawText (name, bounds.toNearestIntEdges(), juce::Justification::centredLeft, true);
    }

    void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! e.mouseWasClicked())
            return;
        if (e.mods.isPopupMenu())
        {
            if (onDelete)
                onDelete();
            return;
        }
        if (onApply)
            onApply();
    }

private:
    juce::String name;
    GradientRamp ramp;
    std::function<void()> onApply, onDelete;
    bool hovered = false;
};

RampPresetPicker::RampPresetPicker (RampPresetStore& storeIn,
                                    std::function<void (int)> onPickIn,
                                    std::function<void (int)> onDeleteIn)
    : store (storeIn),
      onPick (std::move (onPickIn)),
      onDelete (std::move (onDeleteIn))
{
    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    constexpr int rowH = 28;
    const int n = store.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& p = store.getPresets().getReference (i);
        const bool canDelete = ! p.isFactory && onDelete != nullptr;
        auto* row = rows.add (new Row (
            p.isFactory ? (p.name + "  (factory)") : p.name,
            p.ramp,
            [this, i]
            {
                if (onPick)
                    onPick (i);
            },
            [this, i, canDelete]
            {
                if (canDelete && onDelete)
                    onDelete (i);
            }));
        list.addAndMakeVisible (row);
    }

    list.setSize (220, juce::jmax (rowH, n * rowH));
    for (int i = 0; i < rows.size(); ++i)
        rows[i]->setBounds (0, i * rowH, 220, rowH);

    const int listH = juce::jmin (8 * rowH, list.getHeight());
    setSize (236, 8 + (n > 0 ? listH : 36) + 8);

    if (n == 0)
    {
        emptyLabel.setText ("No presets", juce::dontSendNotification);
        emptyLabel.setJustificationType (juce::Justification::centred);
        emptyLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.55f));
        addAndMakeVisible (emptyLabel);
    }
}

RampPresetPicker::~RampPresetPicker()
{
    viewport.setViewedComponent (nullptr, false);
}

void RampPresetPicker::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (28, 28, 26));
    g.setColour (juce::Colours::goldenrod.withAlpha (0.4f));
    g.drawRect (getLocalBounds().toFloat(), 1.0f);
}

void RampPresetPicker::resized()
{
    auto area = getLocalBounds().reduced (8);
    if (rows.isEmpty())
        emptyLabel.setBounds (area);
    else
        viewport.setBounds (area);
}

void showRampPresetPickerCallOut (RampPresetStore& store,
                                  juce::Component* anchor,
                                  std::function<void (int)> onPick,
                                  std::function<void (int)> onDelete)
{
    auto boxPtr = std::make_shared<juce::Component::SafePointer<juce::CallOutBox>>();

    auto* picker = new RampPresetPicker (
        store,
        [onPick, boxPtr] (int index)
        {
            if (onPick)
                onPick (index);
            if (auto* b = boxPtr->getComponent())
                b->dismiss();
        },
        [onDelete, boxPtr] (int index)
        {
            if (onDelete)
                onDelete (index);
            if (auto* b = boxPtr->getComponent())
                b->dismiss();
        });

    juce::Rectangle<int> area;
    if (anchor != nullptr)
        area = anchor->getScreenBounds();
    else
    {
        const auto p = juce::Desktop::getInstance().getMousePosition().roundToInt();
        area = { p.x, p.y, 1, 1 };
    }

    auto& box = juce::CallOutBox::launchAsynchronously (
        std::unique_ptr<juce::Component> (picker), area, nullptr);
    *boxPtr = &box;
}
