#pragma once

#include <JuceHeader.h>
#include "EqProcessor.h"
#include "ShapeMod.h"
#include "ShapeCurveEditor.h"
#include "ComboBoxLookAndFeel.h"
#include "TextButtonLookAndFeel.h"
#include "RotaryImageKnobForOptionBox.h"
#include "RetrigButton.h"
#include "LfoMod.h"

/**
    Floating, resizable Shape editor (OptionBox-style child of the plugin editor).
    Shares the same ShapeMod::Engine as the compact column editor.
*/
class ShapeEditorPopup : public juce::Component,
                         public juce::Timer,
                         public juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit ShapeEditorPopup (EqProcessor& processorToUse)
        : processor (processorToUse),
          treeState (processorToUse.treeState),
          editor (processorToUse.getShapeEngine())
    {
        setOpaque (false);
        setAlwaysOnTop (true);

        title.setText ("Shape Editor", juce::dontSendNotification);
        title.setJustificationType (juce::Justification::centredLeft);
        title.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 190, 150));
        title.setFont (juce::FontOptions (13.0f).withStyle ("Bold"));
        title.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (title);

        closeButton.setButtonText ("X");
        styleSmallButton (closeButton);
        closeButton.setTooltip ("Close shape editor");
        closeButton.onClick = [this]
        {
            setVisible (false);
            if (onClosed != nullptr)
                onClosed();
        };
        addAndMakeVisible (closeButton);

        editor.onCurveChanged = [this] { syncSmoothToCurve(); };
        addAndMakeVisible (editor);

        styleImageKnob (rateSlider);
        styleImageKnob (phaseSlider);
        styleImageKnob (smoothSlider);
        rateSlider.onPopupMenu = [this] { showRateModeMenu(); };
        rateSlider.setTooltip ("Shape rate. Right-click for Hz / Sync");
        phaseSlider.setTooltip ("Phase offset in degrees");
        smoothSlider.setTooltip ("Smooth the whole shape curve");
        smoothSlider.setTextValueSuffix (" %");
        addAndMakeVisible (rateSlider);
        addAndMakeVisible (phaseSlider);
        addAndMakeVisible (smoothSlider);

        styleSmallButton (retrigButton);
        wireRetrigButton (retrigButton, treeState, ShapeMod::retriggerParamId());
        addAndMakeVisible (retrigButton);

        setupLabel (rateLabel, "Rate");
        setupLabel (phaseLabel, "Phase");
        setupLabel (smoothLabel, "Smooth");
        addAndMakeVisible (rateLabel);
        addAndMakeVisible (phaseLabel);
        addAndMakeVisible (smoothLabel);

        // Mirror params (column already owns SliderAttachments).
        wireMirrorSlider (rateSlider, ShapeMod::rateParamId());
        wireMirrorSlider (phaseSlider, ShapeMod::phaseParamId());
        wireMirrorSlider (smoothSlider, ShapeMod::smoothParamId());

        treeState.addParameterListener (ShapeMod::rateSyncParamId(), this);
        treeState.addParameterListener (ShapeMod::rateParamId(), this);
        treeState.addParameterListener (ShapeMod::syncDivParamId(), this);
        treeState.addParameterListener (ShapeMod::phaseParamId(), this);
        treeState.addParameterListener (ShapeMod::smoothParamId(), this);
        treeState.addParameterListener (ShapeMod::retriggerParamId(), this);

        constrainer.setMinimumSize (320, 260);
        constrainer.setMaximumSize (1200, 900);
        resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
        addAndMakeVisible (*resizer);

        syncRateModeUi();
        syncAllFromParams();
        syncSmoothToCurve();
        startTimerHz (24);
        setSize (480, 380);
    }

    ~ShapeEditorPopup() override
    {
        stopTimer();
        treeState.removeParameterListener (ShapeMod::rateSyncParamId(), this);
        treeState.removeParameterListener (ShapeMod::rateParamId(), this);
        treeState.removeParameterListener (ShapeMod::syncDivParamId(), this);
        treeState.removeParameterListener (ShapeMod::phaseParamId(), this);
        treeState.removeParameterListener (ShapeMod::smoothParamId(), this);
        treeState.removeParameterListener (ShapeMod::retriggerParamId(), this);
        closeButton.setLookAndFeel (nullptr);
        retrigButton.setLookAndFeel (nullptr);
    }

    std::function<void()> onClosed;

    void showCenteredIn (juce::Component& parent)
    {
        if (getParentComponent() != &parent)
            parent.addAndMakeVisible (this);

        const int w = juce::jlimit (320, juce::jmax (320, parent.getWidth() - 20), getWidth());
        const int h = juce::jlimit (260, juce::jmax (260, parent.getHeight() - 20), getHeight());
        setSize (w, h);
        setCentrePosition (parent.getLocalBounds().getCentre());
        constrainToParent();
        setVisible (true);
        toFront (true);
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        g.setColour (juce::Colour::fromRGBA (22, 16, 12, 245));
        g.fillRoundedRectangle (bounds, 6.0f);
        g.setColour (juce::Colour::fromRGBA (160, 130, 70, 220));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.5f);

        auto chrome = bounds.removeFromTop (28.0f).reduced (1.0f, 1.0f);
        g.setColour (juce::Colour::fromRGBA (40, 30, 20, 255));
        g.fillRoundedRectangle (chrome, 5.0f);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6);
        auto titleRow = r.removeFromTop (22);
        closeButton.setBounds (titleRow.removeFromRight (22).reduced (1));
        title.setBounds (titleRow);

        auto knobs = r.removeFromBottom (78);
        const int colW = knobs.getWidth() / 3;
        auto k0 = knobs.removeFromLeft (colW);
        auto k1 = knobs.removeFromLeft (colW);
        auto k2 = knobs;

        auto place = [] (juce::Rectangle<int> area, juce::Label& lab, juce::Component& knob, juce::Component* extra = nullptr)
        {
            auto header = area.removeFromTop (14);
            if (extra != nullptr)
            {
                extra->setBounds (header.removeFromRight (16));
                header.removeFromRight (2);
            }
            lab.setBounds (header);
            const int side = juce::jmin (area.getWidth(), area.getHeight(), 48);
            knob.setBounds (area.withSizeKeepingCentre (side, side));
        };

        place (k0, rateLabel, rateSlider, &retrigButton);
        place (k1, phaseLabel, phaseSlider);
        place (k2, smoothLabel, smoothSlider);

        editor.setBounds (r.reduced (2));
        if (resizer != nullptr)
            resizer->setBounds (getWidth() - 16, getHeight() - 16, 16, 16);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.eventComponent == this && e.getPosition().y < 28)
        {
            dragging = true;
            dragOffset = e.getEventRelativeTo (getParentComponent()).getPosition() - getBounds().getPosition();
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging || getParentComponent() == nullptr)
            return;

        const auto p = e.getEventRelativeTo (getParentComponent()).getPosition() - dragOffset;
        setTopLeftPosition (p);
        constrainToParent();
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    void timerCallback() override
    {
        const bool active = LfoMod::isSourceAssignedInMatrix (treeState, LfoMod::srcShape);
        editor.setPlayheadActive (active);
        editor.setPlayheadPhase (processor.getPublishedShapePhase());
        editor.repaint();
    }

    void parameterChanged (const juce::String& parameterID, float) override
    {
        if (parameterID == ShapeMod::rateSyncParamId())
            syncRateModeUi();
        else if (parameterID == ShapeMod::smoothParamId())
            syncSmoothToCurve();

        syncAllFromParams();
    }

private:
    class RateImageKnob : public RotaryImageKnobForOptionBox
    {
    public:
        std::function<void()> onPopupMenu;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onPopupMenu != nullptr)
            {
                onPopupMenu();
                return;
            }
            RotaryImageKnobForOptionBox::mouseDown (e);
        }
    };

    void styleSmallButton (juce::TextButton& b)
    {
        b.setLookAndFeel (&buttonLf);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }

    void styleImageKnob (RotaryImageKnobForOptionBox& s) { s.setCompactNoValueBox (true); }

    void setupLabel (juce::Label& lab, const juce::String& text)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setJustificationType (juce::Justification::centred);
        lab.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.65f));
        lab.setFont (juce::FontOptions (10.8f));
    }

    void constrainToParent()
    {
        if (auto* parent = getParentComponent())
            setBounds (getBounds().constrainedWithin (parent->getLocalBounds().reduced (4)));
    }

    void syncSmoothToCurve()
    {
        float sm = 0.0f;
        if (auto* v = treeState.getRawParameterValue (ShapeMod::smoothParamId()))
            sm = v->load();
        processor.getShapeEngine().uiCurve.setSmoothPercent (sm);
        processor.getShapeEngine().publishFromUi();
        editor.repaint();
    }

    void wireMirrorSlider (juce::Slider& slider, const juce::String& paramId)
    {
        slider.onDragStart = [this, paramId]
        {
            if (auto* p = treeState.getParameter (paramId))
                p->beginChangeGesture();
        };
        slider.onDragEnd = [this, paramId]
        {
            if (auto* p = treeState.getParameter (paramId))
                p->endChangeGesture();
        };
        slider.onValueChange = [this, &slider, paramId]
        {
            if (updatingFromHost)
                return;
            if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter (paramId)))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) slider.getValue()));
        };
    }

    void wireMirrorButton (juce::Button& button, const juce::String& paramId)
    {
        button.onClick = [this, &button, paramId]
        {
            if (updatingFromHost)
                return;
            if (auto* p = dynamic_cast<juce::AudioParameterBool*> (treeState.getParameter (paramId)))
            {
                p->beginChangeGesture();
                *p = button.getToggleState();
                p->endChangeGesture();
            }
        };
    }

    void syncAllFromParams()
    {
        updatingFromHost = true;

        if (rateSyncMode)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (ShapeMod::syncDivParamId())))
                rateSlider.setValue ((double) p->getIndex(), juce::dontSendNotification);
        }
        else if (auto* v = treeState.getRawParameterValue (ShapeMod::rateParamId()))
        {
            rateSlider.setValue ((double) v->load(), juce::dontSendNotification);
        }

        if (auto* v = treeState.getRawParameterValue (ShapeMod::phaseParamId()))
            phaseSlider.setValue ((double) v->load(), juce::dontSendNotification);
        if (auto* v = treeState.getRawParameterValue (ShapeMod::smoothParamId()))
            smoothSlider.setValue ((double) v->load(), juce::dontSendNotification);
        syncRetrigButton (retrigButton, getRetrigModeIndex (treeState, ShapeMod::retriggerParamId()));

        updatingFromHost = false;
    }

    void showRateModeMenu()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
        menu.addItem (1, "Hz", true, ! rateSyncMode);
        menu.addItem (2, "Sync", true, rateSyncMode);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rateSlider),
                            [this] (int result)
                            {
                                if (result <= 0)
                                    return;
                                if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
                                        treeState.getParameter (ShapeMod::rateSyncParamId())))
                                {
                                    const bool wantSync = (result == 2);
                                    if (p->get() == wantSync)
                                        return;
                                    p->beginChangeGesture();
                                    *p = wantSync;
                                    p->endChangeGesture();
                                }
                            });
    }

    void syncRateModeUi()
    {
        rateSyncMode = treeState.getRawParameterValue (ShapeMod::rateSyncParamId()) != nullptr
                       && treeState.getRawParameterValue (ShapeMod::rateSyncParamId())->load() > 0.5f;

        rateSlider.textFromValueFunction = nullptr;
        rateSlider.valueFromTextFunction = nullptr;

        if (rateSyncMode)
        {
            rateLabel.setText ("Sync", juce::dontSendNotification);
            rateSlider.setTextValueSuffix ({});
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (ShapeMod::syncDivParamId())))
            {
                rateSlider.setRange (0.0, (double) juce::jmax (0, p->choices.size() - 1), 1.0);
                rateSlider.textFromValueFunction = [] (double v)
                {
                    const int i = juce::jlimit (0, LfoMod::numSyncDivs - 1, (int) std::lround (v));
                    return LfoMod::getSyncDivNames()[i];
                };
            }
            wireMirrorSlider (rateSlider, ShapeMod::syncDivParamId());
        }
        else
        {
            rateLabel.setText ("Rate", juce::dontSendNotification);
            rateSlider.setTextValueSuffix (" Hz");
            const auto range = LfoMod::rateHzRange();
            rateSlider.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start, (double) range.end, (double) range.interval,
                (double) range.skew, range.symmetricSkew));
            wireMirrorSlider (rateSlider, ShapeMod::rateParamId());
        }

        syncAllFromParams();
    }

    EqProcessor& processor;
    juce::AudioProcessorValueTreeState& treeState;
    ShapeCurveEditor editor;

    juce::Label title;
    juce::TextButton closeButton;
    RateImageKnob rateSlider;
    RetrigTextButton retrigButton { "N" };
    RotaryImageKnobForOptionBox phaseSlider;
    RotaryImageKnobForOptionBox smoothSlider;
    juce::Label rateLabel, phaseLabel, smoothLabel;

    TextButtonLookAndFeel buttonLf;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    bool rateSyncMode = false;
    bool updatingFromHost = false;
    bool dragging = false;
    juce::Point<int> dragOffset;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShapeEditorPopup)
};
