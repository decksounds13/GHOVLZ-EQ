#include "Theme.h"
#include <cstdint>  // for uint8_t
#include <JuceHeader.h>
#include "SharedResources.h"

Theme::Theme() {
    // Initialize the timestamps to the current time
    created = juce::Time::getCurrentTime();
    modified = created; // Initially, lastModified is the same as created

    // ... (any other initialization code)
}

Theme::Theme(const SharedColors& colors)
    : colors(colors) {
    // ... (any other initialization code)
}

Theme::Theme(const juce::String& presetName, const juce::Time& createdTime, const juce::Time& modifiedTime, const SharedColors& colors)
    : name(presetName), created(createdTime), modified(modifiedTime), colors(colors) {
    // ... (any other initialization code)
}


juce::XmlElement* Theme::toXml() const {
    auto xml = new juce::XmlElement("Theme");

    auto toHexString = [](const juce::Colour& colour) -> juce::String {
        if (colour.getAlpha() == 255) { // If alpha is full, omit it
            return juce::String::formatted("%02X%02X%02X",
                colour.getRed(),
                colour.getGreen(),
                colour.getBlue());
        }
        else { // Include alpha channel if it's not full
            return juce::String::formatted("%02X%02X%02X%02X",
                colour.getRed(),
                colour.getGreen(),
                colour.getBlue(),
                colour.getAlpha());
        }
    };

    xml->setAttribute("MenuBackgroundGradientColor1", toHexString(colors.menuBackgroundGradientColor1));
    xml->setAttribute("MenuBackgroundGradientColor2", toHexString(colors.menuBackgroundGradientColor2));
    xml->setAttribute("MenuListBoxBackgroundGradientColor1", toHexString(colors.menuListBoxBackgroundGradientColor1));
    xml->setAttribute("MenuListBoxBackgroundGradientColor2", toHexString(colors.menuListBoxBackgroundGradientColor2));
    xml->setAttribute("MenuTabBarBorderColor", toHexString(colors.menuTabBarBorderColor));
    xml->setAttribute("MenuThinBorderColor", toHexString(colors.menuThinBorderColor));
    xml->setAttribute("MenuButtonGradientColor1", toHexString(colors.menuButtonGradientColor1));
    xml->setAttribute("MenuButtonGradientColor2", toHexString(colors.menuButtonGradientColor2));
    xml->setAttribute("MenuButtonTextColor1", toHexString(colors.menuButtonTextColor1));
    xml->setAttribute("MenuLabelTextColor1", toHexString(colors.menuLabelTextColor1));
    xml->setAttribute("MenuScrollBarTrackColor1", toHexString(colors.menuScrollBarTrackColor1));
    xml->setAttribute("MenuScrollBarThumbColor1", toHexString(colors.menuScrollBarThumbColor1));
    xml->setAttribute("MenuScrollBarOutlineColor1", toHexString(colors.menuScrollBarOutlineColor1));
    xml->setAttribute("MenuListBoxTextColor1", toHexString(colors.menuListBoxTextColor1));
    xml->setAttribute("MenuListBoxSelectionColor1", toHexString(colors.menuListBoxSelectionColor1));
    xml->setAttribute("MenuTextBoxTextColor1", toHexString(colors.menuTextBoxTextColor1));

    xml->setAttribute("ColorAttributeNames", getColorAttributeNames().joinIntoString(","));

    DBG("XML Created Attribute: " << created.toISO8601(true));
    DBG("XML Modified Attribute: " << modified.toISO8601(true));

    // Add metadata attributes to XML
    xml->setAttribute("Name", name);
    xml->setAttribute("Created", created.toISO8601(true));
    xml->setAttribute("Modified", modified.toISO8601(true));

    // Embed full plugin state (APVTS + A/B) under <STATE>…</STATE>
    if (pluginState.isValid())
    {
        if (auto stateXml = pluginState.createXml())
        {
            auto* wrapper = new juce::XmlElement ("STATE");
            wrapper->addChildElement (stateXml.release());
            xml->addChildElement (wrapper);
        }
    }

    return xml;
}



void Theme::fromXml(const juce::XmlElement& xmlElement) {
    auto ensureAlphaAndCheckColor = [](const juce::String& hexColor) -> juce::Colour {
        if (hexColor.isEmpty()) {
            return juce::Colours::grey; // Default grey color if attribute is empty
        }
        juce::String colorString = hexColor;
        // Ensure color has alpha if required
        if (colorString.length() == 6) {
            colorString = "ff" + colorString; // Prepend alpha if needed
        }
        // Check for the color to replace
        if (colorString.equalsIgnoreCase("0000FF00")) {
            return juce::Colours::grey; // Replace with grey color
        }
        return juce::Colour::fromString(colorString);
        };

    // Assign each color using the helper function. If the attribute is not present, the default grey color is used.
    colors.menuBackgroundGradientColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuBackgroundGradientColor1"));
    colors.menuBackgroundGradientColor2 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuBackgroundGradientColor2"));
    colors.menuListBoxBackgroundGradientColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuListBoxBackgroundGradientColor1"));
    colors.menuListBoxBackgroundGradientColor2 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuListBoxBackgroundGradientColor2"));
    colors.menuTabBarBorderColor = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuTabBarBorderColor"));
    colors.menuThinBorderColor = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuThinBorderColor"));
    colors.menuButtonGradientColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuButtonGradientColor1"));
    colors.menuButtonGradientColor2 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuButtonGradientColor2"));
    colors.menuButtonTextColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuButtonTextColor1"));
    colors.menuLabelTextColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuLabelTextColor1"));
    colors.menuScrollBarTrackColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuScrollBarTrackColor1"));
    colors.menuScrollBarThumbColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuScrollBarThumbColor1"));
    colors.menuScrollBarOutlineColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuScrollBarOutlineColor1"));
    colors.menuListBoxTextColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuListBoxTextColor1"));
    colors.menuListBoxSelectionColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuListBoxSelectionColor1"));
    colors.menuTextBoxTextColor1 = ensureAlphaAndCheckColor(xmlElement.getStringAttribute("MenuTextBoxTextColor1"));
    // ... and so on for any other color attributes

    name = xmlElement.getStringAttribute("Name", "Unnamed");

    // Check if 'Created' attribute exists, if not, set to current time
    juce::String createdString = xmlElement.getStringAttribute("Created");
    if (createdString.isEmpty()) {
        created = juce::Time::getCurrentTime();
    }
    else {
        created = juce::Time::fromISO8601(createdString);
    }

    // Check if 'LastModified' attribute exists, if not, set to current time
    juce::String lastModifiedString = xmlElement.getStringAttribute("Modified");
    if (lastModifiedString.isEmpty()) {
        modified = juce::Time::getCurrentTime();
    }
    else {
        modified = juce::Time::fromISO8601(lastModifiedString);
    }

    pluginState = {};
    if (auto* stateWrapper = xmlElement.getChildByName ("STATE"))
    {
        if (auto* stateChild = stateWrapper->getFirstChildElement())
            pluginState = juce::ValueTree::fromXml (*stateChild);
    }

    // Debugging
    DBG("Parsed Created: " << created.toString(true, true));
    DBG("Parsed LastModified: " << modified.toString(true, true));
}





void Theme::savePresetsToXML(juce::Array<Theme>& themes,
    const juce::StringArray& presetNames,
    const juce::Array<juce::Time>& themeCreatedTimes,
    const juce::Array<juce::Time>& themeModifiedTimes) {
    DBG("Saving presets to XML...");

    // Documents/Decksounds/ParametricEq/Themes/presets.xml
    // Each <Theme> may include a nested <STATE> child with full plugin APVTS XML.
    juce::File presetDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Decksounds")
        .getChildFile("ParametricEq")
        .getChildFile("Themes");

    // Create the directory if it doesn't exist
    if (!presetDirectory.exists()) {
        presetDirectory.createDirectory();
    }

    juce::File presetFile = presetDirectory.getChildFile("presets.xml");

    DBG("File path to save presets: " + presetFile.getFullPathName());

    // Create a new XML document
    juce::XmlElement xml("Presets");

    const int count = juce::jmin (themes.size(), presetNames.size());
    for (int i = 0; i < count; ++i) {
        auto themeElement = themes[i].toXml();
        themeElement->setAttribute("name", presetNames[i]);

        const auto created = (i < themeCreatedTimes.size())
                                 ? themeCreatedTimes[i]
                                 : themes[i].getCreated();
        const auto modified = (i < themeModifiedTimes.size())
                                  ? themeModifiedTimes[i]
                                  : themes[i].getModified();

        themeElement->setAttribute("Created", created.toISO8601(true));
        themeElement->setAttribute("Modified", modified.toISO8601(true));
        xml.addChildElement(themeElement);
    }

    // Save the XML document to the file
    if (xml.writeToFile(presetFile, "", "UTF-8", 60)) {
        DBG("Presets saved successfully.");
    }
    else {
        DBG("Failed to save presets.");
    }
}

juce::StringArray Theme::getColorAttributeNames() const {
    return {
        "MenuBackgroundGradientColor1",
        "MenuBackgroundGradientColor2",
        "MenuListBoxBackgroundGradientColor1",
        "MenuListBoxBackgroundGradientColor2",
        "MenuTabBarBorderColor",
        "MenuThinBorderColor",
        "MenuButtonGradientColor1",
        "MenuButtonGradientColor2",
        "MenuButtonTextColor1",
        "MenuLabelTextColor1",
        "MenuScrollBarTrackColor1",
        "MenuScrollBarThumbColor1",
        "MenuScrollBarOutlineColor1",
        "MenuListBoxTextColor1",
        "MenuListBoxSelectionColor1",
         "MenuTextBoxTextColor1"
        // Add new color attribute names here as your theme expands
    };
}







