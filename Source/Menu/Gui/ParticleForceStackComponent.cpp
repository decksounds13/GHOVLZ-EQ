#include "ParticleForceStackComponent.h"
#include "../../ComboBoxLookAndFeel.h"
#include <cmath>

namespace
{
    constexpr int kRowH = 36;
    constexpr int kRowHRotation = 58;
    constexpr int kPad = 6;
    constexpr int kHeaderH = 26;
    constexpr int kAddH = 28;
    constexpr int kGripW = 14;

    /** Property key: typed values may exceed drag range (same idea as particle Look sliders). */
    const juce::Identifier kForceSliderActual ("forceSliderActual");

    double getForceSliderActual (const juce::Slider& s)
    {
        if (s.getProperties().contains (kForceSliderActual))
            return (double) s.getProperties()[kForceSliderActual];
        return s.getValue();
    }

    void setForceSliderActual (juce::Slider& s, double actual)
    {
        if (! std::isfinite (actual))
            actual = s.getValue();
        s.getProperties().set (kForceSliderActual, actual);
        const double thumb = juce::jlimit (s.getMinimum(), s.getMaximum(), actual);
        s.setValue (thumb, juce::dontSendNotification);
        s.updateText();
    }

    /** Drag stays in setRange; typed text can set any finite value (applied via actual prop). */
    void wireUncappedForceSlider (juce::Slider& s)
    {
        s.setTextBoxIsEditable (true);
        s.setNumDecimalPlacesToDisplay (3);
        s.valueFromTextFunction = [&s] (const juce::String& text)
        {
            const double typed = text.getDoubleValue();
            if (std::isfinite (typed))
                s.getProperties().set (kForceSliderActual, typed);
            return juce::jlimit (s.getMinimum(), s.getMaximum(), typed);
        };
        s.textFromValueFunction = [&s] (double v)
        {
            const double shown = s.getProperties().contains (kForceSliderActual)
                                     ? (double) s.getProperties()[kForceSliderActual]
                                     : v;
            if (std::abs (shown) >= 100.0 && std::abs (shown - std::round (shown)) < 1.0e-6)
                return juce::String ((int) std::round (shown));
            return juce::String (shown, 3);
        };
    }

    void styleForceSlider (juce::Slider& s, double minV, double maxV, double step)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 16);
        s.setRange (minV, maxV, step);
        s.setNumDecimalPlacesToDisplay (3);
        s.setColour (juce::Slider::thumbColourId, juce::Colours::goldenrod);
        s.setColour (juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha (0.45f));
        s.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.92f));
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.40f));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxHighlightColourId, juce::Colours::goldenrod.withAlpha (0.35f));
        wireUncappedForceSlider (s);
    }

    void styleAxisToggle (juce::ToggleButton& t)
    {
        t.setClickingTogglesState (true);
        t.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
        t.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
        t.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::grey);
    }

    /**
        Force sliders were ~100x too hot for world-unit particle sim.
        Ranges are ~1% of the old +/-20 / 0-20 spans; defaults match in ParticleForceModule.h.
    */
    void configureParamsForType (ParticleForceType type,
                                 juce::Slider& p0, juce::Slider& p1, juce::Slider& p2,
                                 bool linkAxes)
    {
        const bool multi = (type == ParticleForceType::wind
                            || type == ParticleForceType::curlNoise
                            || (type == ParticleForceType::rotation && ! linkAxes));
        p1.setVisible (multi);
        p2.setVisible (multi);

        switch (type)
        {
            case ParticleForceType::gravity:
                styleForceSlider (p0, -0.20, 0.20, 0.001);
                p0.setTooltip ("Gravity accel Y (world units/s^2). Negative pulls down.\n"
                               "Drag range is gentle; type any value for stronger pull.");
                break;
            case ParticleForceType::drag:
                styleForceSlider (p0, 0.0, 0.20, 0.001);
                p0.setTooltip ("Linear drag (vel *= exp(-k * dt)).\n"
                               "Drag range is gentle; type any value for stronger drag.");
                break;
            case ParticleForceType::wind:
                styleForceSlider (p0, -0.20, 0.20, 0.001);
                styleForceSlider (p1, -0.20, 0.20, 0.001);
                styleForceSlider (p2, -0.20, 0.20, 0.001);
                p0.setTooltip ("Wind accel X (type beyond drag range if needed)");
                p1.setTooltip ("Wind accel Y (type beyond drag range if needed)");
                p2.setTooltip ("Wind accel Z (type beyond drag range if needed)");
                break;
            case ParticleForceType::curlNoise:
                styleForceSlider (p0, 0.0, 0.20, 0.001);
                styleForceSlider (p1, 0.01, 2.0, 0.01);
                styleForceSlider (p2, 0.0, 0.50, 0.001);
                p0.setTooltip ("Curl strength (type any value beyond the drag range)");
                p1.setTooltip ("Spatial scale (type beyond drag range if needed)");
                p2.setTooltip ("Scroll speed (type beyond drag range if needed)");
                break;
            case ParticleForceType::turbulence:
                styleForceSlider (p0, 0.0, 0.20, 0.001);
                p0.setTooltip ("Random turbulence strength.\n"
                               "Drag range is gentle (0-0.2); type any value for more.");
                break;
            case ParticleForceType::rotation:
                // rad/s: drag +/-2; type higher for extreme spin.
                styleForceSlider (p0, -2.0, 2.0, 0.01);
                styleForceSlider (p1, -2.0, 2.0, 0.01);
                styleForceSlider (p2, -2.0, 2.0, 0.01);
                if (linkAxes)
                {
                    p0.setTooltip ("Spin rate (rad/s) on all enabled axes. Type beyond +/-2 if needed.");
                }
                else
                {
                    p0.setTooltip ("Spin rate X (rad/s). Type beyond +/-2 if needed.");
                    p1.setTooltip ("Spin rate Y (rad/s). Type beyond +/-2 if needed.");
                    p2.setTooltip ("Spin rate Z (rad/s). Type beyond +/-2 if needed.");
                }
                break;
            default:
                styleForceSlider (p0, -0.5, 0.5, 0.001);
                break;
        }
    }
}

ParticleForceStackComponent::ParticleForceStackComponent (SharedResources& resources)
    : shared (resources)
{
    juce::ignoreUnused (shared);

    title.setText ("Force stack", juce::dontSendNotification);
    title.setFont (SharedResources::uiFont (14.0f));
    title.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    addButton.setColour (juce::TextButton::buttonColourId, juce::Colours::darkgoldenrod.withAlpha (0.50f));
    addButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::goldenrod.withAlpha (0.55f));
    addButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    addButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addButton.setTooltip ("Add a force module - pick type from the menu (evaluated top to bottom)");
    addButton.onClick = [this] { showAddForceMenu(); };
    addAndMakeVisible (addButton);
}

void ParticleForceStackComponent::showAddForceMenu()
{
    if ((int) modules.size() >= kParticleForceStackMax)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    // Item id = (int) type + 1
    for (int t = 0; t < (int) ParticleForceType::count; ++t)
        menu.addItem (t + 1, particleForceTypeName ((ParticleForceType) t));

    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetComponent (&addButton)
                            .withMinimumWidth (addButton.getWidth()),
                        [safe = juce::Component::SafePointer<ParticleForceStackComponent> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;
                            safe->addForceOfType ((ParticleForceType) (result - 1));
                        });
}

void ParticleForceStackComponent::addForceOfType (ParticleForceType type)
{
    if ((int) modules.size() >= kParticleForceStackMax)
        return;
    if (type < ParticleForceType::gravity || type >= ParticleForceType::count)
        return;
    const uint32_t uid = onRequestUid ? onRequestUid() : (uint32_t) (modules.size() + 1);
    modules.push_back (makeDefaultForceModule (type, uid));
    rebuildRows();
    notifyChanged (true);
}

void ParticleForceStackComponent::setModules (const std::vector<ParticleForceModule>& mods)
{
    auto next = mods;
    if (next.size() > (size_t) kParticleForceStackMax)
        next.resize ((size_t) kParticleForceStackMax);

    // In-place value update when structure matches - never destroy rows mid-drag
    // (Menu sync + force slider onValueChange used to rebuild and crash).
    const bool structureSame = next.size() == modules.size()
                               && next.size() == (size_t) rows.size()
                               && [&]
    {
        for (size_t i = 0; i < next.size(); ++i)
            if (next[i].type != modules[i].type || next[i].uid != modules[i].uid)
                return false;
        return true;
    }();

    if (structureSame)
    {
        modules = std::move (next);
        for (int i = 0; i < rows.size(); ++i)
            if (rows[i] != nullptr)
                rows[i]->setModule (modules[(size_t) i]);
        return;
    }

    modules = std::move (next);
    rebuildRows();
}

std::vector<ParticleForceModule> ParticleForceStackComponent::getModules() const
{
    std::vector<ParticleForceModule> out;
    out.reserve ((size_t) rows.size());
    for (auto* r : rows)
        if (r != nullptr)
            out.push_back (r->getModule());
    return out;
}

int ParticleForceStackComponent::getPreferredHeight() const
{
    int h = kHeaderH + kPad + kAddH + kPad * 2;
    for (const auto& m : modules)
        h += (m.type == ParticleForceType::rotation ? kRowHRotation : kRowH) + 2;
    // Fallback if modules empty but rows exist
    if (modules.empty() && rows.size() > 0)
        for (auto* r : rows)
            if (r != nullptr)
                h += r->getRowHeight() + 2;
    return h;
}

void ParticleForceStackComponent::rebuildRows()
{
    rows.clear (true);
    for (int i = 0; i < (int) modules.size(); ++i)
    {
        auto* row = rows.add (new ForceRow (*this, i));
        row->setModule (modules[(size_t) i]);
        addAndMakeVisible (row);
    }
    resized();
}

void ParticleForceStackComponent::notifyChanged (bool structureChanged)
{
    modules = getModules();
    if (onChanged)
        onChanged (structureChanged);
    // Only reflow when row count / height may have changed - not on every param scrub.
    if (structureChanged)
    {
        if (auto* p = getParentComponent())
            p->resized();
    }
}

void ParticleForceStackComponent::beginDrag (int index)
{
    dragFrom = index;
}

void ParticleForceStackComponent::updateDrag (int mouseY)
{
    if (dragFrom < 0 || rows.isEmpty())
        return;
    // Approximate target index from cumulative row heights.
    int y = kHeaderH + kPad;
    int to = dragFrom;
    for (int i = 0; i < rows.size(); ++i)
    {
        const int rh = rows[i] != nullptr ? rows[i]->getRowHeight() : kRowH;
        if (mouseY < y + rh / 2)
        {
            to = i;
            break;
        }
        y += rh + 2;
        to = i;
    }
    to = juce::jlimit (0, rows.size() - 1, to);
    if (to == dragFrom)
        return;
    modules = getModules();
    auto m = modules[(size_t) dragFrom];
    modules.erase (modules.begin() + dragFrom);
    modules.insert (modules.begin() + to, m);
    dragFrom = to;
    rebuildRows();
}

void ParticleForceStackComponent::endDrag()
{
    if (dragFrom >= 0)
    {
        dragFrom = -1;
        notifyChanged (true);
    }
}

void ParticleForceStackComponent::resized()
{
    auto a = getLocalBounds().reduced (4);
    title.setBounds (a.removeFromTop (kHeaderH));
    a.removeFromTop (kPad / 2);
    for (auto* r : rows)
    {
        if (r == nullptr) continue;
        const int rh = r->getRowHeight();
        r->setBounds (a.removeFromTop (rh));
        a.removeFromTop (2);
    }
    auto addRow = a.removeFromTop (kAddH);
    addButton.setBounds (addRow.removeFromLeft (120));
}

void ParticleForceStackComponent::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (juce::Colours::goldenrod.withAlpha (0.18f));
    g.drawRoundedRectangle (b.reduced (0.5f), 5.0f, 1.0f);

    g.setColour (juce::Colours::whitesmoke.withAlpha (0.08f));
    const float y = (float) (kHeaderH + 2);
    g.drawHorizontalLine ((int) y, 8.0f, (float) getWidth() - 8.0f);
}

// ── ForceRow ────────────────────────────────────────────────────────────────

ParticleForceStackComponent::ForceRow::ForceRow (ParticleForceStackComponent& o, int index)
    : ownerRef (o), rowIndex (index)
{
    enable.setClickingTogglesState (true);
    enable.setTooltip ("Enable / disable this force module");
    enable.onClick = [this] { ownerRef.notifyChanged (false); };
    addAndMakeVisible (enable);

    typeLabel.setFont (SharedResources::uiFont (12.5f));
    typeLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.92f));
    typeLabel.setJustificationType (juce::Justification::centredLeft);
    typeLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (typeLabel);

    auto wireSlider = [this] (juce::Slider& s)
    {
        // Param scrub only - do not rebuild rows or request Menu relayout (that crashed).
        // Drag updates actual = thumb; typed out-of-range keeps actual from valueFromTextFunction.
        s.onValueChange = [this, &s]
        {
            if (s.isMouseButtonDown())
                s.getProperties().set (kForceSliderActual, s.getValue());
            else if (! s.getProperties().contains (kForceSliderActual))
                s.getProperties().set (kForceSliderActual, s.getValue());
            ownerRef.notifyChanged (false);
        };
        s.setNumDecimalPlacesToDisplay (3);
        addAndMakeVisible (s);
    };
    wireSlider (p0); wireSlider (p1); wireSlider (p2);

    auto wireToggle = [this] (juce::ToggleButton& t, const juce::String& tip)
    {
        styleAxisToggle (t);
        t.setTooltip (tip);
        t.onClick = [this]
        {
            refreshRotationChrome();
            // Link axes can show/hide p1/p2 - height may change for rotation rows.
            ownerRef.notifyChanged (true);
            ownerRef.resized();
            if (auto* p = ownerRef.getParentComponent())
                p->resized();
        };
        addChildComponent (t);
    };
    wireToggle (axisX, "Apply rotational force on X");
    wireToggle (axisY, "Apply rotational force on Y");
    wireToggle (axisZ, "Apply rotational force on Z");
    wireToggle (linkAxes, "Link axes: one strength for all enabled axes");
    wireToggle (randomDir, "Per-particle random spin (-1 to 1 per axis). Each particle tumbles differently. Off = same rate for all.");

    remove.setButtonText ("x");
    remove.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    remove.setColour (juce::TextButton::buttonOnColourId, juce::Colours::indianred.withAlpha (0.35f));
    remove.setColour (juce::TextButton::textColourOffId, juce::Colours::indianred.brighter (0.25f));
    remove.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    remove.setTooltip ("Remove force module");
    remove.onClick = [this]
    {
        auto mods = ownerRef.getModules();
        if (juce::isPositiveAndBelow (rowIndex, (int) mods.size()))
        {
            mods.erase (mods.begin() + rowIndex);
            ownerRef.setModules (mods);
            ownerRef.notifyChanged (true);
        }
    };
    addAndMakeVisible (remove);
}

int ParticleForceStackComponent::ForceRow::getRowHeight() const noexcept
{
    return moduleType == ParticleForceType::rotation ? kRowHRotation : kRowH;
}

void ParticleForceStackComponent::ForceRow::refreshRotationChrome()
{
    const bool isRot = moduleType == ParticleForceType::rotation;
    axisX.setVisible (isRot);
    axisY.setVisible (isRot);
    axisZ.setVisible (isRot);
    linkAxes.setVisible (isRot);
    randomDir.setVisible (isRot);
    if (isRot)
        configureParamsForType (moduleType, p0, p1, p2, linkAxes.getToggleState());
}

void ParticleForceStackComponent::ForceRow::setModule (const ParticleForceModule& m)
{
    moduleUid = m.uid;
    moduleType = m.type;
    enable.setToggleState (m.enabled, juce::dontSendNotification);
    typeLabel.setText (particleForceTypeName (m.type), juce::dontSendNotification);
    axisX.setToggleState (m.axisX, juce::dontSendNotification);
    axisY.setToggleState (m.axisY, juce::dontSendNotification);
    axisZ.setToggleState (m.axisZ, juce::dontSendNotification);
    linkAxes.setToggleState (m.linkAxes, juce::dontSendNotification);
    randomDir.setToggleState (m.randomDir, juce::dontSendNotification);
    configureParamsForType (m.type, p0, p1, p2, m.linkAxes);
    // Preserve values outside drag range (typed / prefs).
    setForceSliderActual (p0, (double) m.p[0]);
    setForceSliderActual (p1, (double) m.p[1]);
    setForceSliderActual (p2, (double) m.p[2]);
    refreshRotationChrome();
    setAlpha (m.enabled ? 1.0f : 0.55f);
    resized();
}

ParticleForceModule ParticleForceStackComponent::ForceRow::getModule() const
{
    ParticleForceModule m;
    m.type = moduleType;
    m.uid = moduleUid;
    m.enabled = enable.getToggleState();
    m.p[0] = (float) getForceSliderActual (p0);
    m.p[1] = (float) getForceSliderActual (p1);
    m.p[2] = (float) getForceSliderActual (p2);
    m.axisX = axisX.getToggleState();
    m.axisY = axisY.getToggleState();
    m.axisZ = axisZ.getToggleState();
    m.linkAxes = linkAxes.getToggleState();
    m.randomDir = randomDir.getToggleState();
    return m;
}

void ParticleForceStackComponent::ForceRow::resized()
{
    auto r = getLocalBounds().reduced (4, 3);
    r.removeFromLeft (kGripW);

    if (moduleType == ParticleForceType::rotation)
    {
        auto top = r.removeFromTop (26);
        enable.setBounds (top.removeFromLeft (20).withSizeKeepingCentre (18, 18));
        top.removeFromLeft (4);
        typeLabel.setBounds (top.removeFromLeft (70));
        top.removeFromLeft (4);
        remove.setBounds (top.removeFromRight (22).withSizeKeepingCentre (20, 20));
        top.removeFromRight (2);

        // Axis + mode toggles on the top row remainder
        const int tw = 36;
        axisX.setBounds (top.removeFromLeft (tw));
        axisY.setBounds (top.removeFromLeft (tw));
        axisZ.setBounds (top.removeFromLeft (tw));
        top.removeFromLeft (4);
        linkAxes.setBounds (top.removeFromLeft (48));
        randomDir.setBounds (top.removeFromLeft (42));

        r.removeFromTop (2);
        // Strength slider(s) on second line
        const int nVis = 1 + (p1.isVisible() ? 1 : 0) + (p2.isVisible() ? 1 : 0);
        const int gap = 3;
        const int w = (r.getWidth() - gap * (nVis - 1)) / juce::jmax (1, nVis);
        p0.setBounds (r.removeFromLeft (w).withHeight (22));
        if (p1.isVisible())
        {
            r.removeFromLeft (gap);
            p1.setBounds (r.removeFromLeft (w).withHeight (22));
        }
        if (p2.isVisible())
        {
            r.removeFromLeft (gap);
            p2.setBounds (r.removeFromLeft (w).withHeight (22));
        }
        return;
    }

    // Standard single-line force row
    enable.setBounds (r.removeFromLeft (20).withSizeKeepingCentre (18, 18));
    r.removeFromLeft (4);
    typeLabel.setBounds (r.removeFromLeft (76));
    r.removeFromLeft (4);
    remove.setBounds (r.removeFromRight (22).withSizeKeepingCentre (20, 20));
    r.removeFromRight (2);

    const int nVis = 1 + (p1.isVisible() ? 1 : 0) + (p2.isVisible() ? 1 : 0);
    const int gap = 3;
    const int w = (r.getWidth() - gap * (nVis - 1)) / juce::jmax (1, nVis);
    p0.setBounds (r.removeFromLeft (w));
    if (p1.isVisible())
    {
        r.removeFromLeft (gap);
        p1.setBounds (r.removeFromLeft (w));
    }
    if (p2.isVisible())
    {
        r.removeFromLeft (gap);
        p2.setBounds (r);
    }
}

void ParticleForceStackComponent::ForceRow::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    const bool en = enable.getToggleState();
    g.setColour (juce::Colours::white.withAlpha (dragging ? 0.12f : (en ? 0.055f : 0.03f)));
    g.fillRoundedRectangle (b, 3.5f);
    if (dragging)
    {
        g.setColour (juce::Colours::goldenrod.withAlpha (0.45f));
        g.drawRoundedRectangle (b, 3.5f, 1.0f);
    }

    g.setColour (juce::Colours::whitesmoke.withAlpha (dragging ? 0.65f : 0.32f));
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 2; ++j)
            g.fillEllipse (4.0f + (float) j * 4.0f,
                           11.0f + (float) i * 5.0f,
                           2.4f, 2.4f);
}

void ParticleForceStackComponent::ForceRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < kGripW + 2)
    {
        dragging = true;
        dragStartY = e.getEventRelativeTo (&ownerRef).y;
        ownerRef.beginDrag (rowIndex);
        repaint();
    }
}

void ParticleForceStackComponent::ForceRow::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        ownerRef.updateDrag (e.getEventRelativeTo (&ownerRef).y);
}

void ParticleForceStackComponent::ForceRow::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
    {
        dragging = false;
        ownerRef.endDrag();
        repaint();
    }
}
