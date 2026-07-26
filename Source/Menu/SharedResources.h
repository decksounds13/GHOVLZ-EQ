#pragma once
#include "../JuceLibraryCode/JuceHeader.h"


class SharedColors
{
public:

	float saturationLowerLimit = 0.1f;
	float saturationUpperLimit = 0.35f;

	float hueLowerLimit = 0.0f;
	float hueUpperLimit = 1.0f;

	float brightnessLowerLimit = 0.0f;
	float brightnessUpperLimit = 1.0f;

	bool randomizeHue = true;
	bool randomizeSaturation = true;
	bool randomizeBrightness = true;
	bool randomizeAlpha = true; // New flag for alpha


	juce::Colour createColorWithOptionalAlpha(int r, int g, int b, int a) {
		if (a == 255) {
			// Create RGB color if alpha is 255 (fully opaque)
			return juce::Colour(r, g, b);
		}
		else {
			// Create RGBA color if alpha is not 255
			return juce::Colour::fromRGBA(r, g, b, a);
		}
	}

	std::array<bool, 16> colorRandomizationFlags = {}; // Initialize all flags to false

	juce::Colour menuBackgroundGradientColor1 = createColorWithOptionalAlpha(54, 48, 41, 255);
	juce::Colour menuBackgroundGradientColor2 = createColorWithOptionalAlpha(0, 0, 0, 255);
	juce::Colour menuListBoxBackgroundGradientColor1 = createColorWithOptionalAlpha(60, 47, 39, 255);
	juce::Colour menuListBoxBackgroundGradientColor2 = createColorWithOptionalAlpha(0, 0, 0, 255);
	juce::Colour menuTabBarBorderColor = createColorWithOptionalAlpha(125, 125, 125, 255);
	juce::Colour menuThinBorderColor = createColorWithOptionalAlpha(125, 125, 125, 255);
	juce::Colour menuButtonGradientColor1 = createColorWithOptionalAlpha(54, 48, 41, 255);
	juce::Colour menuButtonGradientColor2 = createColorWithOptionalAlpha(0, 0, 0, 255);
	juce::Colour menuButtonTextColor1 = createColorWithOptionalAlpha(245, 245, 245, 217);
	juce::Colour menuLabelTextColor1 = createColorWithOptionalAlpha(245, 245, 245, 217);
	juce::Colour menuScrollBarTrackColor1 = createColorWithOptionalAlpha(80, 80, 80, 255);
	juce::Colour menuScrollBarThumbColor1 = createColorWithOptionalAlpha(140, 140, 140, 255);
	juce::Colour menuScrollBarOutlineColor1 = createColorWithOptionalAlpha(20, 20, 20, 255);
	juce::Colour menuListBoxTextColor1 = createColorWithOptionalAlpha(245, 245, 245, 255);
	juce::Colour menuListBoxSelectionColor1 = createColorWithOptionalAlpha(220, 220, 220, 255);
	juce::Colour menuTextBoxTextColor1 = createColorWithOptionalAlpha(245, 245, 245, 217);


	// Works for randomizing individual colors


	// New methods to toggle randomization
	void setRandomizeHue(bool enabled) { randomizeHue = enabled; }
	void setRandomizeSaturation(bool enabled) { randomizeSaturation = enabled; }
	void setRandomizeBrightness(bool enabled) { randomizeBrightness = enabled; }
	void setRandomizeAlpha(bool enabled) { randomizeAlpha = enabled; }


	void setMenuBackgroundGradientColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[0]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuBackgroundGradientColor1 = newColor;
		}
	}

	void setMenuBackgroundGradientColor2(juce::Colour newColor) {
		if (colorRandomizationFlags[1]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuBackgroundGradientColor2 = newColor;
		}
	}

	void setMenuListBoxBackgroundGradientColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[2]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuListBoxBackgroundGradientColor1 = newColor;
		}
	}

	void setMenuListBoxBackgroundGradientColor2(juce::Colour newColor) {
		if (colorRandomizationFlags[3]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuListBoxBackgroundGradientColor2 = newColor;
		}
	}

	void setMenuTabBarBorderColor(juce::Colour newColor) {
		if (colorRandomizationFlags[4]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuTabBarBorderColor = newColor;
		}
	}

	void setMenuThinBorderColor(juce::Colour newColor) {
		if (colorRandomizationFlags[5]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuThinBorderColor = newColor;
		}
	}

	void setMenuButtonGradientColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[6]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuButtonGradientColor1 = newColor;
		}
	}

	void setMenuButtonGradientColor2(juce::Colour newColor) {
		if (colorRandomizationFlags[7]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuButtonGradientColor2 = newColor;
		}
	}

	void setMenuButtonTextColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[8]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuButtonTextColor1 = newColor;
		}
	}

	void setMenuLabelTextColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[9]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuLabelTextColor1 = newColor;
		}
	}

	void setMenuScrollBarTrackColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[10]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuScrollBarTrackColor1 = newColor;
		}
	}

	void setMenuScrollBarThumbColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[11]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuScrollBarThumbColor1 = newColor;
		}
	}

	void setMenuScrollBarOutlineColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[12]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuScrollBarOutlineColor1 = newColor;
		}
	}

	void setMenuListBoxTextColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[13]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuListBoxTextColor1 = newColor;
		}
	}

	void setMenuListBoxSelectionColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[14]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuListBoxSelectionColor1 = newColor;
		}
	}

	void setMenuTextBoxTextColor1(juce::Colour newColor) {
		if (colorRandomizationFlags[15]) {
			// Normalize the alpha value to the range 0.0 - 1.0
			float normalizedAlpha = static_cast<float>(newColor.getAlpha()) / 255.0f;
			newColor = newColor.withAlpha(normalizedAlpha);

			menuTextBoxTextColor1 = newColor;
		}

	}



	void setBrightnessRange(float lower, float upper) {
		brightnessLowerLimit = lower;
		brightnessUpperLimit = upper;
	}


	void setHueRange(float lower, float upper) {
		hueLowerLimit = lower;
		hueUpperLimit = upper;
	}

	void setSaturationRange(float lower, float upper) {
		saturationLowerLimit = lower;
		saturationUpperLimit = upper;
	}

	void toggleColorRandomizationFlags(const juce::Array<int>& indices) {
		// Reset flags
		std::fill(colorRandomizationFlags.begin(), colorRandomizationFlags.end(), false);

		// Set flags for provided indices
		for (auto index : indices) {
			if (index >= 0 && index < colorRandomizationFlags.size()) {
				colorRandomizationFlags[index] = true;
			}
		}
	}


	void updateColorsDirectly(juce::Colour newColor, const juce::Array<int>& colorIndices) {
		//DBG("Updating colors directly for indices: ");
		
		for (auto index : colorIndices) {
			
			DBG(index << ", ");

			auto originalFlags = this->colorRandomizationFlags;

			switch (index) {
			case 0: this->setMenuBackgroundGradientColor1(newColor); break;
			case 1: this->setMenuBackgroundGradientColor2(newColor); break;
			case 2: this->setMenuListBoxBackgroundGradientColor1(newColor); break;
			case 3: this->setMenuListBoxBackgroundGradientColor2(newColor); break;
			case 4: this->setMenuTabBarBorderColor(newColor); break;
			case 5: this->setMenuThinBorderColor(newColor); break;
			case 6: this->setMenuButtonGradientColor1(newColor); break;
			case 7: this->setMenuButtonGradientColor2(newColor); break;
			case 8: this->setMenuButtonTextColor1(newColor); break;
			case 9: this->setMenuLabelTextColor1(newColor); break;
			case 10: this->setMenuScrollBarTrackColor1(newColor); break;
			case 11: this->setMenuScrollBarThumbColor1(newColor); break;
			case 12: this->setMenuScrollBarOutlineColor1(newColor); break;
			case 13: this->setMenuListBoxTextColor1(newColor); break;
			case 14: this->setMenuListBoxSelectionColor1(newColor); break;
			case 15: this->setMenuTextBoxTextColor1(newColor); break;
				// ... other cases for other colors
			}

			this->colorRandomizationFlags = originalFlags;
		}
	}



	void randomizeColors() {
		auto randomHue = [this]() -> float {
			// Random hue within the specified range
			float range = hueUpperLimit - hueLowerLimit;
			return randomizeHue ? juce::Random::getSystemRandom().nextFloat() * range + hueLowerLimit : hueLowerLimit;
			};

		auto randomSaturation = [this]() -> float {
			return randomizeSaturation ? juce::Random::getSystemRandom().nextFloat() * (saturationUpperLimit - saturationLowerLimit) + saturationLowerLimit : saturationLowerLimit;
			};

		auto randomBrightness = [this]() -> float {
			return randomizeBrightness ? juce::Random::getSystemRandom().nextFloat() * (brightnessUpperLimit - brightnessLowerLimit) + brightnessLowerLimit : brightnessLowerLimit;
			};

		auto randomColor = [randomHue, randomSaturation, randomBrightness]() -> juce::Colour {
			float hue = randomHue();
			float saturation = randomSaturation();
			float brightness = randomBrightness();
			return juce::Colour::fromHSV(hue, saturation, brightness, 1.0f);
			};


		// Apply random colors
		menuBackgroundGradientColor1 = randomColor();
		menuBackgroundGradientColor2 = randomColor();
		menuListBoxBackgroundGradientColor1 = randomColor();
		menuListBoxBackgroundGradientColor2 = randomColor();
		menuTabBarBorderColor = randomColor();
		menuThinBorderColor = randomColor();
		menuButtonGradientColor1 = randomColor();
		menuButtonGradientColor2 = randomColor();
		menuButtonTextColor1 = randomColor();
		menuLabelTextColor1 = randomColor();
		menuScrollBarTrackColor1 = randomColor();
		menuScrollBarThumbColor1 = randomColor();
		menuScrollBarOutlineColor1 = randomColor();
		menuListBoxTextColor1 = randomColor();
		menuListBoxSelectionColor1 = randomColor();
		menuTextBoxTextColor1 = randomColor();

		// Add any other color properties here...
	}

	juce::Colour randomizeSelectedColorsWithinRange() {
		auto randomHue = [this]() -> float {
			// Random hue within the specified range
			float range = hueUpperLimit - hueLowerLimit;
			return randomizeHue ? juce::Random::getSystemRandom().nextFloat() * range + hueLowerLimit : hueLowerLimit;
			};

		auto randomSaturation = [this]() -> float {
			return randomizeSaturation ? juce::Random::getSystemRandom().nextFloat() * (saturationUpperLimit - saturationLowerLimit) + saturationLowerLimit : saturationLowerLimit;
			};

		auto randomBrightness = [this]() -> float {
			return randomizeBrightness ? juce::Random::getSystemRandom().nextFloat() * (brightnessUpperLimit - brightnessLowerLimit) + brightnessLowerLimit : brightnessLowerLimit;
			};

		auto randomColor = [randomHue, randomSaturation, randomBrightness]() -> juce::Colour {
			float hue = randomHue();
			float saturation = randomSaturation();
			float brightness = randomBrightness();
			return juce::Colour::fromHSV(hue, saturation, brightness, 1.0f);
			};

		juce::Colour randomizedColor; // Initialize to a default value

		// Check each flag before applying random colors
		if (!colorRandomizationFlags[0]) menuBackgroundGradientColor1 = randomColor();
		if (!colorRandomizationFlags[1]) menuBackgroundGradientColor2 = randomColor();
		if (!colorRandomizationFlags[2]) menuListBoxBackgroundGradientColor1 = randomColor();
		if (!colorRandomizationFlags[3]) menuListBoxBackgroundGradientColor2 = randomColor();
		if (!colorRandomizationFlags[4]) menuTabBarBorderColor = randomColor();
		if (!colorRandomizationFlags[5]) menuThinBorderColor = randomColor();
		if (!colorRandomizationFlags[6]) menuButtonGradientColor1 = randomColor();
		if (!colorRandomizationFlags[7]) menuButtonGradientColor2 = randomColor();
		if (!colorRandomizationFlags[8]) menuButtonTextColor1 = randomColor();
		if (!colorRandomizationFlags[9]) menuLabelTextColor1 = randomColor();
		if (!colorRandomizationFlags[10]) menuScrollBarTrackColor1 = randomColor();
		if (!colorRandomizationFlags[11]) menuScrollBarThumbColor1 = randomColor();
		if (!colorRandomizationFlags[12]) menuScrollBarOutlineColor1 = randomColor();
		if (!colorRandomizationFlags[13]) menuListBoxTextColor1 = randomColor();
		if (!colorRandomizationFlags[14]) menuListBoxSelectionColor1 = randomColor();
		if (!colorRandomizationFlags[15]) menuTextBoxTextColor1 = randomColor();

		// Add any other color properties and corresponding flags here...

		return randomizedColor;
	}


/*
	void randomizeSelectedColorsWithinRange(const juce::Array<int>& selectedIndices,
		float saturationLower,
		float saturationUpper,
		bool individualRandomization,
		bool randomizeHue,
		bool randomizeSaturation,
		bool randomizeBrightness) {
		for (auto index : selectedIndices) {
			juce::Random rand = individualRandomization ? juce::Random(juce::Random::getSystemRandom().nextInt()) : juce::Random::getSystemRandom();

			auto randomHue = [this, &rand, randomizeHue]() -> float {
				float range = hueUpperLimit - hueLowerLimit;
				return randomizeHue ? rand.nextFloat() * range + hueLowerLimit : 0.0f;
				};

			auto randomSaturation = [this, &rand, saturationLower, saturationUpper, randomizeSaturation]() -> float {
				return randomizeSaturation ? rand.nextFloat() * (saturationUpper - saturationLower) + saturationLower : 0.0f;
				};

			auto randomBrightness = [this, &rand, randomizeBrightness]() -> float {
				return randomizeBrightness ? rand.nextFloat() : 0.0f;
				};

			juce::Colour newColor = juce::Colour::fromHSV(randomHue(), randomSaturation(), randomBrightness(), 1.0f);

			setRandomColorForIndex(index, rand, saturationLower, saturationUpper, randomizeHue, randomizeSaturation, randomizeBrightness);
		}
	}

	// Updated function to use the separate randomization controls
	void setRandomColorForIndex(int index, juce::Random& rand, float saturationLower, float saturationUpper, bool randomizeHue, bool randomizeSaturation, bool randomizeBrightness) {
		auto randomHue = [&rand, this, randomizeHue]() -> float {
			float range = hueUpperLimit - hueLowerLimit;
			return randomizeHue ? rand.nextFloat() * range + hueLowerLimit : 0.0f;
			};

		auto randomSaturation = [&rand, saturationLower, saturationUpper, randomizeSaturation]() -> float {
			return randomizeSaturation ? rand.nextFloat() * (saturationUpper - saturationLower) + saturationLower : 0.0f;
			};

		auto randomBrightness = [&rand, randomizeBrightness]() -> float {
			return randomizeBrightness ? rand.nextFloat() : 0.0f;
			};

		auto randomColor = [randomHue, randomSaturation, randomBrightness]() -> juce::Colour {
			float hue = randomHue();
			float saturation = randomSaturation();
			float brightness = randomBrightness();
			return juce::Colour::fromHSV(hue, saturation, brightness, 1.0f);
			};

		// Set the color for the current index based on the case
		switch (index) {
		case 0:
			if (colorRandomizationFlags[0]) menuBackgroundGradientColor1 = randomColor();
			break;
		case 1:
			if (colorRandomizationFlags[1]) menuBackgroundGradientColor2 = randomColor();
			break;
		case 2:
			if (colorRandomizationFlags[2]) menuListBoxBackgroundGradientColor1 = randomColor();
			break;
		case 3:
			if (colorRandomizationFlags[3]) menuListBoxBackgroundGradientColor2 = randomColor();
			break;
		case 4:
			if (colorRandomizationFlags[4]) menuTabBarBorderColor = randomColor();
			break;
		case 5:
			if (colorRandomizationFlags[5]) menuThinBorderColor = randomColor();
			break;
		case 6:
			if (colorRandomizationFlags[6]) menuButtonGradientColor1 = randomColor();
			break;
		case 7:
			if (colorRandomizationFlags[7]) menuButtonGradientColor2 = randomColor();
			break;
		case 8:
			if (colorRandomizationFlags[8]) menuButtonTextColor1 = randomColor();
			break;
		case 9:
			if (colorRandomizationFlags[9]) menuLabelTextColor1 = randomColor();
			break;
		case 10:
			if (colorRandomizationFlags[10]) menuScrollBarTrackColor1 = randomColor();
			break;
		case 11:
			if (colorRandomizationFlags[11]) menuScrollBarThumbColor1 = randomColor();
			break;
		case 12:
			if (colorRandomizationFlags[12]) menuScrollBarOutlineColor1 = randomColor();
			break;
		case 13:
			if (colorRandomizationFlags[13]) menuListBoxTextColor1 = randomColor();
			break;
		case 14:
			if (colorRandomizationFlags[14]) menuListBoxSelectionColor1 = randomColor();
			break;
			// Add cases for other indices...
		}
	}

	*/


};

class SharedResources
{
public:
	SharedColors sharedColors;

	// You can add more shared resources here as needed
	// e.g., images, configurations, etc.


	static SharedColors getDefaultTheme() {
		SharedColors defaultColors;
		// Set the default color values
		defaultColors.setMenuBackgroundGradientColor1(juce::Colour::fromRGBA(54, 48, 41, 255));
		defaultColors.setMenuBackgroundGradientColor2(juce::Colour::fromRGBA(0, 0, 0, 255));
		defaultColors.setMenuListBoxBackgroundGradientColor1(juce::Colour::fromRGBA(60, 47, 39, 255));
		defaultColors.setMenuListBoxBackgroundGradientColor2(juce::Colour::fromRGBA(0, 0, 0, 255));
		defaultColors.setMenuTabBarBorderColor(juce::Colour::fromRGBA(125, 125, 125, 255));
		defaultColors.setMenuThinBorderColor(juce::Colour::fromRGBA(125, 125, 125, 255));
		defaultColors.setMenuButtonGradientColor1(juce::Colour::fromRGBA(54, 48, 41, 255));
		defaultColors.setMenuButtonGradientColor2(juce::Colour::fromRGBA(0, 0, 0, 255));
		defaultColors.setMenuButtonTextColor1(juce::Colour::fromRGBA(245, 245, 245, 217));
		defaultColors.setMenuLabelTextColor1(juce::Colour::fromRGBA(245, 245, 245, 217));
		defaultColors.setMenuScrollBarTrackColor1(juce::Colour::fromRGBA(80, 80, 80, 255));
		defaultColors.setMenuScrollBarThumbColor1(juce::Colour::fromRGBA(140, 140, 140, 255));
		defaultColors.setMenuScrollBarOutlineColor1(juce::Colour::fromRGBA(20, 20, 20, 255));
		defaultColors.setMenuListBoxTextColor1(juce::Colour::fromRGBA(245, 245, 245, 255));
		defaultColors.setMenuListBoxSelectionColor1(juce::Colour::fromRGBA(220, 220, 220, 255));
		defaultColors.setMenuTextBoxTextColor1(juce::Colour::fromRGBA(245, 245, 245, 217));
		// Set all other necessary colors...
		return defaultColors;
	}
};
