#include "Spectrogram3DDebugComponent.h"
#include "../../MainComponent.h"
#include "../Menu.h"

namespace
{
    constexpr int kPadX = 16;
    constexpr int kPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kSliderH = 22;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;
    constexpr int kToggleH = 22;
}

Spectrogram3DDebugComponent::Content::Content (SharedResources& resources,
                                               juce::AudioProcessorValueTreeState& state,
                                               ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      sphereSection (resources, "spec3d.debug.sphere", "Sphere", false)
{
    juce::ignoreUnused (treeState, colourRamps);

    titleLabel.setText ("3D Debug", juce::dontSendNotification);
    titleLabel.setFont (SharedResources::uiFont (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    noteLabel.setText ("Lookdev sphere + RGB move gizmo. Cast-shadow / cascade settings "
                       "are on the 3D Spectrogram Look tab (needs Lighting enabled).",
                       juce::dontSendNotification);
    noteLabel.setFont (juce::FontOptions().withHeight (12.5f));
    noteLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (noteLabel);

    auto setupToggle = [this] (juce::ToggleButton& t, const juce::String& tip)
    {
        styleToggle (t);
        t.setTooltip (tip);
        t.onClick = [this] { applyControlsToMain(); };
        addAndMakeVisible (t);
    };
    auto setupSlider = [this] (juce::Label& lab, juce::Slider& s,
                               double minV, double maxV, double step, const juce::String& tip)
    {
        styleLabel (lab);
        styleSlider (s);
        s.setRange (minV, maxV, step);
        s.setTooltip (tip);
        s.onValueChange = [this] { applyControlsToMain(); };
        addAndMakeVisible (lab);
        addAndMakeVisible (s);
    };

    setupToggle (sphereToggle, "White lookdev sphere with soft normals + move gizmo.");
    sphereSizeLabel.setText ("Sphere Diameter (m)", juce::dontSendNotification);
    setupSlider (sphereSizeLabel, sphereSizeSlider,
                 Spectrogram3DComponent::kDebugSphereMinDiameter,
                 Spectrogram3DComponent::kDebugSphereMaxDiameter,
                 0.005,
                 "Default 2/12 m (1/12 of waterfall footprint width).");
    sphereSizeSlider.setValue (Spectrogram3DComponent::kDebugSphereDefaultDiameter,
                               juce::dontSendNotification);

    sphereXLabel.setText ("Position X (m)", juce::dontSendNotification);
    setupSlider (sphereXLabel, sphereXSlider, -2.5, 2.5, 0.01, "World X (time axis).");
    sphereXSlider.setValue (0.6, juce::dontSendNotification);
    sphereYLabel.setText ("Position Y (m)", juce::dontSendNotification);
    setupSlider (sphereYLabel, sphereYSlider, 0.0, 3.0, 0.01, "World Y (height).");
    sphereYSlider.setValue (Spectrogram3DComponent::kDebugSphereDefaultDiameter * 0.5,
                            juce::dontSendNotification);
    sphereZLabel.setText ("Position Z (m)", juce::dontSendNotification);
    setupSlider (sphereZLabel, sphereZSlider, -2.5, 2.5, 0.01, "World Z (frequency axis).");
    sphereZSlider.setValue (0.6, juce::dontSendNotification);

    wireColourEditor (albedoEditor);
    albedoEditor.setColour (juce::Colours::white);

    specularLabel.setText ("Specular", juce::dontSendNotification);
    setupSlider (specularLabel, specularSlider, 0.0, 1.0, 0.01,
                 "GGX specular lobe intensity for the lookdev sphere (independent of Look Specular).");
    specularSlider.setValue (1.0, juce::dontSendNotification);
    roughLabel.setText ("Roughness", juce::dontSendNotification);
    setupSlider (roughLabel, roughSlider, 0.04, 1.0, 0.01, "GGX roughness.");
    roughSlider.setValue (0.70, juce::dontSendNotification);
    metalLabel.setText ("Metalness", juce::dontSendNotification);
    setupSlider (metalLabel, metalSlider, 0.0, 1.0, 0.01, "Metalness.");
    metalSlider.setValue (0.0, juce::dontSendNotification);

    styleLabel (titleLabel);
    styleLabel (noteLabel);
    noteLabel.setColour (juce::Label::textColourId,
                         sharedResources.sharedColors.menuLabelTextColor1.withAlpha (0.75f));

    juce::Timer::callAfterDelay (0, [safe = juce::Component::SafePointer<Content> (this)]
    {
        if (safe != nullptr)
            safe->syncControlsFromMain();
    });

    wireSection (sphereSection);
}

void Spectrogram3DDebugComponent::Content::wireSection (SettingsSection& section)
{
    addAndMakeVisible (section);
    section.onChanged = [this]
    {
        resized();
        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
}

Spectrogram3DDebugComponent::Content::~Content() = default;

void Spectrogram3DDebugComponent::Content::styleSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
    slider.setScrollWheelEnabled (false);
}

void Spectrogram3DDebugComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (SharedResources::uiFont (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void Spectrogram3DDebugComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setColour (juce::ToggleButton::textColourId,
                      sharedResources.sharedColors.menuLabelTextColor1);
}

void Spectrogram3DDebugComponent::Content::layoutSliderRow (juce::Rectangle<int>& area,
                                                            juce::Label& label,
                                                            juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    slider.setBounds (area.removeFromTop (kSliderH));
    area.removeFromTop (kRowGap);
}

void Spectrogram3DDebugComponent::Content::layoutToggle (juce::Rectangle<int>& area,
                                                         juce::ToggleButton& toggle)
{
    toggle.setBounds (area.removeFromTop (kToggleH));
    area.removeFromTop (6);
}

void Spectrogram3DDebugComponent::Content::layoutColourEditor (juce::Rectangle<int>& area,
                                                               ColourSwatchEditor& editor)
{
    editor.setBounds (area.removeFromTop (28));
    area.removeFromTop (kRowGap);
}

void Spectrogram3DDebugComponent::Content::wireColourEditor (ColourSwatchEditor& editor)
{
    addAndMakeVisible (editor);
    editor.onColourChanged = [this] (juce::Colour) { applyControlsToMain(); };
}

void Spectrogram3DDebugComponent::Content::syncDebugSphereFromMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;
    const auto p = main->getSpec3DDebugSpherePosition();
    sphereXSlider.setValue (p.x, juce::dontSendNotification);
    sphereYSlider.setValue (p.y, juce::dontSendNotification);
    sphereZSlider.setValue (p.z, juce::dontSendNotification);
    sphereSizeSlider.setValue (main->getSpec3DDebugSphereDiameter(), juce::dontSendNotification);
}

void Spectrogram3DDebugComponent::Content::syncControlsFromMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    sphereToggle.setToggleState (main->isSpec3DDebugSphereEnabled(), juce::dontSendNotification);
    syncDebugSphereFromMain();
    albedoEditor.setColour (main->getSpec3DDebugSphereAlbedo());
    specularSlider.setValue (main->getSpec3DDebugSphereSpecular(), juce::dontSendNotification);
    roughSlider.setValue (main->getSpec3DDebugSphereRoughness(), juce::dontSendNotification);
    metalSlider.setValue (main->getSpec3DDebugSphereMetalness(), juce::dontSendNotification);
}

void Spectrogram3DDebugComponent::Content::applyControlsToMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    constexpr bool kSave = false;
    main->setSpec3DDebugSphereEnabled (sphereToggle.getToggleState(), kSave);
    main->setSpec3DDebugSphereDiameter ((float) sphereSizeSlider.getValue(), kSave);
    main->setSpec3DDebugSpherePosition ({ (float) sphereXSlider.getValue(),
                                          (float) sphereYSlider.getValue(),
                                          (float) sphereZSlider.getValue() },
                                        kSave);
    main->setSpec3DDebugSphereAlbedo (albedoEditor.getColour(), kSave);
    main->setSpec3DDebugSphereSpecular ((float) specularSlider.getValue(), kSave);
    main->setSpec3DDebugSphereRoughness ((float) roughSlider.getValue(), kSave);
    main->setSpec3DDebugSphereMetalness ((float) metalSlider.getValue(), kSave);
    main->requestUiPrefsSave();
}

int Spectrogram3DDebugComponent::Content::getPreferredHeight() const
{
    const int row = kLabelH + kLabelGap + kSliderH + kRowGap;
    return kPadY * 2 + 24 + 8
           + sphereSection.heightFor (52 + 8 + (kToggleH + 6) + row * 7 + 28 + 8);
}

void Spectrogram3DDebugComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);
    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    sphereSection.applyVisible ({
        &noteLabel, &sphereToggle,
        &sphereSizeLabel, &sphereSizeSlider,
        &sphereXLabel, &sphereXSlider,
        &sphereYLabel, &sphereYSlider,
        &sphereZLabel, &sphereZSlider,
        &albedoEditor,
        &specularLabel, &specularSlider,
        &roughLabel, &roughSlider,
        &metalLabel, &metalSlider });
    sphereSection.placeHeader (area);
    if (sphereSection.isOpen())
    {
        noteLabel.setBounds (area.removeFromTop (52));
        area.removeFromTop (8);
        layoutToggle (area, sphereToggle);
        layoutSliderRow (area, sphereSizeLabel, sphereSizeSlider);
        layoutSliderRow (area, sphereXLabel, sphereXSlider);
        layoutSliderRow (area, sphereYLabel, sphereYSlider);
        layoutSliderRow (area, sphereZLabel, sphereZSlider);
        layoutColourEditor (area, albedoEditor);
        layoutSliderRow (area, specularLabel, specularSlider);
        layoutSliderRow (area, roughLabel, roughSlider);
        layoutSliderRow (area, metalLabel, metalSlider);
    }
}

Spectrogram3DDebugComponent::Spectrogram3DDebugComponent (SharedResources& resources,
                                                          juce::AudioProcessorValueTreeState& state,
                                                          ColourRampBank& colourRamps)
    : sharedResources (resources),
      content (resources, state, colourRamps)
{
    juce::ignoreUnused (sharedResources);
    addAndMakeVisible (content);
}

Spectrogram3DDebugComponent::~Spectrogram3DDebugComponent() = default;

void Spectrogram3DDebugComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void Spectrogram3DDebugComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
