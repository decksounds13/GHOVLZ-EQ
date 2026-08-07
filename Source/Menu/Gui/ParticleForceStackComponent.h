#pragma once

#include <JuceHeader.h>
#include "../../ParticleForceModule.h"
#include "../SharedResources.h"
#include <functional>
#include <vector>

/**
    Ordered force-module list (add / enable / reorder / remove).
    Drag the handle to reorder. Matches menu chrome styling.
    "+ Add force" opens a popup to pick type (no separate type combo).
    Rotation rows expose axis checkboxes (X/Y/Z), Link, and Random dir.
*/
class ParticleForceStackComponent : public juce::Component
{
public:
    ParticleForceStackComponent (SharedResources& resources);
    ~ParticleForceStackComponent() override = default;

    void setModules (const std::vector<ParticleForceModule>& mods);
    std::vector<ParticleForceModule> getModules() const;

    std::function<void()> onChanged;
    std::function<uint32_t()> onRequestUid;

    int getPreferredHeight() const;
    void resized() override;
    void paint (juce::Graphics& g) override;

private:
    class ForceRow : public juce::Component
    {
    public:
        ForceRow (ParticleForceStackComponent& owner, int index);
        void setModule (const ParticleForceModule& m);
        ParticleForceModule getModule() const;
        void resized() override;
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        int getRowHeight() const noexcept;

        ParticleForceStackComponent& ownerRef;
        int rowIndex = 0;
        juce::ToggleButton enable { "" };
        juce::Label typeLabel;
        juce::Slider p0, p1, p2;
        /** Rotation-only axis / mode controls. */
        juce::ToggleButton axisX { "X" }, axisY { "Y" }, axisZ { "Z" };
        juce::ToggleButton linkAxes { "Link" };
        juce::ToggleButton randomDir { "Rnd" };
        juce::TextButton remove { "x" };
        uint32_t moduleUid = 0;
        ParticleForceType moduleType = ParticleForceType::gravity;
        bool dragging = false;
        int dragStartY = 0;

        void refreshRotationChrome();
    };

    SharedResources& shared;
    juce::Label title;
    juce::TextButton addButton { "+ Add force" };
    juce::OwnedArray<ForceRow> rows;
    std::vector<ParticleForceModule> modules;
    int dragFrom = -1;

    void showAddForceMenu();
    void addForceOfType (ParticleForceType type);
    void rebuildRows();
    void notifyChanged();
    void beginDrag (int index);
    void updateDrag (int mouseY);
    void endDrag();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleForceStackComponent)
};
