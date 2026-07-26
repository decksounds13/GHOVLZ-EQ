#include <JuceHeader.h>
#include "../../TextButtonLookAndFeel.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "shadows-main/source/StackShadow.h"

class CustomTextButton : public juce::TextButton {
public:
    CustomTextButton(const juce::String& buttonText);
    ~CustomTextButton();
    void paint(juce::Graphics& g) override;
    void resized() override; // Override the resized method to update the button path

    const juce::Path& getButtonPath() const { return buttonPath; }

private:
    TextButtonLookAndFeel customLookAndFeel;
    juce::Path buttonPath; // Path for the button shape
    std::unique_ptr<shadows::StackShadow> customShadow;

};
