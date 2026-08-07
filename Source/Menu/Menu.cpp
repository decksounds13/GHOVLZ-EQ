#include "Menu.h"
#include "Gui/AppearanceComponent.h"
#include "Gui/SpectrumComponent.h"
#include "Gui/FftComponent.h"
#include "Gui/OscilloscopeSettingsComponent.h"
#include "Gui/GoniometerSettingsComponent.h"
#include "Gui/SpectrogramSettingsComponent.h"
#include "Gui/Spectrogram3DSettingsComponent.h"
#include "Gui/Spectrogram3DDebugComponent.h"
#include "Gui/LevelMetersComponent.h"
#include "Gui/LoudnessSettingsComponent.h"
#include "Gui/StereogramSettingsComponent.h"
#include "Gui/HistogramSettingsComponent.h"
#include "SharedResources.h"
#include <JuceHeader.h>

namespace
{
    std::unique_ptr<juce::Component> takeOwned (juce::Component* raw)
    {
        return std::unique_ptr<juce::Component> (raw);
    }
}

Menu::Menu (SharedResources& resources,
            juce::AudioProcessorValueTreeState& state,
            TextButtonLookAndFeel& lookAndFeel,
            ColourRampBank& colourRamps)
    : sharedResources (resources),
      textButtonLookAndFeel (lookAndFeel),
      tabBar (*this)
{
    juce::Colour menuBorderColor = sharedResources.sharedColors.menuTabBarBorderColor;

    auto addOwnedTab = [this] (const juce::String& name, juce::Component* raw)
    {
        ownedTabContents.push_back (takeOwned (raw));
        allTabs.push_back ({ name, ownedTabContents.back().get() });
    };

    addOwnedTab ("Spectrum", new SpectrumComponent (resources, state, colourRamps));
    addOwnedTab ("FFT", new FftComponent (resources, state, colourRamps));
    addOwnedTab ("Oscilloscope", new OscilloscopeSettingsComponent (resources, state, colourRamps));
    addOwnedTab ("Goniometer", new GoniometerSettingsComponent (resources, state, colourRamps));
    addOwnedTab ("Spectrogram", new SpectrogramSettingsComponent (resources, state, colourRamps));
    addOwnedTab ("3D Spectrogram", new Spectrogram3DSettingsComponent (resources, state, colourRamps));
    addOwnedTab ("3D Debug", new Spectrogram3DDebugComponent (resources, state, colourRamps));
    addOwnedTab ("Level Meters", new LevelMetersComponent (resources, state));
    addOwnedTab ("Loudness", new LoudnessSettingsComponent (resources, state));
    addOwnedTab ("Stereogram", new StereogramSettingsComponent (resources, state, colourRamps));
    addOwnedTab ("Histogram", new HistogramSettingsComponent (resources, state, colourRamps));
    addOwnedTab ("Appearance", new AppearanceComponent (resources, state));

    appearanceComponentRef = dynamic_cast<AppearanceComponent*> (allTabs.back().content);

    tabBar.setLookAndFeel (&customTabBarLookAndFeel);
    tabBar.setTabBarDepth (35);
    // No content outline — the old 4px grey frame looked like a nested border,
    // especially once the Settings panel was resized wider than the page.
    juce::ignoreUnused (menuBorderColor);
    tabBar.setOutline (0.0f);
    tabBar.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    // Never compress into JUCE's "+" extras menu — we page instead.
    tabBar.getTabbedButtonBar().setMinimumTabScaleFactor (1.0);

    contentPanel.setSize (kContentWidth, kContentHeight);
    contentPanel.addAndMakeVisible (tabBar);

    tabPrevButton.setTooltip ("Previous tab page");
    tabNextButton.setTooltip ("Next tab page");
    tabPrevButton.onClick = [this] { setTabPage (tabPageIndex - 1); };
    tabNextButton.onClick = [this] { setTabPage (tabPageIndex + 1); };
    contentPanel.addAndMakeVisible (tabPrevButton);
    contentPanel.addAndMakeVisible (tabNextButton);

    rebuildTabsForCurrentPage();

    viewport.setViewedComponent (&contentPanel, false);
    viewport.setScrollBarsShown (false, false);
    viewport.setScrollBarThickness (0);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (viewport);

    verticalScrollBar = std::make_unique<CustomScrollBar> (viewport.getVerticalScrollBar(),
                                                           CustomScrollBar::Orientation::vertical);
    horizontalScrollBar = std::make_unique<CustomScrollBar> (viewport.getHorizontalScrollBar(),
                                                             CustomScrollBar::Orientation::horizontal);
    addAndMakeVisible (*verticalScrollBar);
    addAndMakeVisible (*horizontalScrollBar);
    syncScrollBarColours();

    constrainer.setMinimumSize (200, 140);
    constrainer.setMaximumSize (4000, 4000);
    // Keep the whole frame usable; tiny onscreen margins made left-edge resize
    // easy to shove most of the panel off-screen.
    constrainer.setMinimumOnscreenAmounts (kDragBarHeight, 80, 40, 80);
    borderResizer = std::make_unique<juce::ResizableBorderComponent> (this, &constrainer);
    // Thin edges on all sides; centre of the panel still receives clicks for scrolling / controls.
    // Top edge stays thin so most of the drag bar remains available for moving.
    borderResizer->setBorderThickness ({ 5, 5, 5, 5 });
    addAndMakeVisible (*borderResizer);

    // Title-bar close — Settings hamburger sits under the panel when open.
    closeButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x00D7)); // ×
    closeButton.setTooltip ("Close settings");
    closeButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    closeButton.onClick = [this]
    {
        if (onCloseRequest != nullptr)
            onCloseRequest();
    };
    styleCloseButton();
    addAndMakeVisible (closeButton);

    setPaintingIsUnclipped (true);
    setSize (kContentWidth, kContentHeight + kDragBarHeight);
}

ThemeList* Menu::getThemeList() const noexcept
{
    return appearanceComponentRef != nullptr ? &appearanceComponentRef->getThemeList() : nullptr;
}

void Menu::syncSpec3DDofFocusFromMain()
{
    for (auto& tab : ownedTabContents)
        if (auto* s3d = dynamic_cast<Spectrogram3DSettingsComponent*> (tab.get()))
            s3d->syncDofFocusFromMain();
}

void Menu::syncSpec3DSettingsFromMain()
{
    for (auto& tab : ownedTabContents)
        if (auto* s3d = dynamic_cast<Spectrogram3DSettingsComponent*> (tab.get()))
            s3d->syncFromMain();
}

void Menu::syncSpec3DDebugSphereFromMain()
{
    for (auto& tab : ownedTabContents)
        if (auto* dbg = dynamic_cast<Spectrogram3DDebugComponent*> (tab.get()))
            dbg->syncDebugSphereFromMain();
}

Menu::~Menu()
{
    tabBar.setLookAndFeel (nullptr);
    // Detach before owned contents are destroyed (deleteComponents=false).
    tabBar.clearTabs();
}

int Menu::getNumTabPages() const noexcept
{
    return juce::jmax (1, ((int) allTabs.size() + kTabsPerPage - 1) / kTabsPerPage);
}

void Menu::setTabPage (int page)
{
    const int pages = getNumTabPages();
    tabPageIndex = juce::jlimit (0, pages - 1, page);
    rebuildTabsForCurrentPage();
    resized();
}

void Menu::rebuildTabsForCurrentPage()
{
    juce::String keepName;
    if (tabBar.getNumTabs() > 0)
        keepName = tabBar.getCurrentTabName();

    // clearTabs detaches the panel without deleting (we never set deleteByTabComp_).
    tabBar.clearTabs();

    const int start = tabPageIndex * kTabsPerPage;
    const int end = juce::jmin (start + kTabsPerPage, (int) allTabs.size());
    int selectIdx = 0;

    for (int i = start; i < end; ++i)
    {
        // Hide off-page content so a previous page's panel can't linger as a child.
        if (allTabs[(size_t) i].content != nullptr)
            allTabs[(size_t) i].content->setVisible (false);

        tabBar.addTab (allTabs[(size_t) i].name,
                       juce::Colours::transparentBlack,
                       allTabs[(size_t) i].content,
                       false);
        if (allTabs[(size_t) i].name == keepName)
            selectIdx = i - start;
    }

    // Also hide contents that are not on this page.
    for (int i = 0; i < (int) allTabs.size(); ++i)
    {
        if (i < start || i >= end)
            if (allTabs[(size_t) i].content != nullptr)
                allTabs[(size_t) i].content->setVisible (false);
    }

    if (tabBar.getNumTabs() > 0)
        tabBar.setCurrentTabIndex (juce::jlimit (0, tabBar.getNumTabs() - 1, selectIdx), false);

    const int pages = getNumTabPages();
    tabPrevButton.setEnabled (tabPageIndex > 0);
    tabNextButton.setEnabled (tabPageIndex + 1 < pages);
    tabPrevButton.setVisible (pages > 1);
    tabNextButton.setVisible (pages > 1);
    tabPrevButton.toFront (false);
    tabNextButton.toFront (false);
}

bool Menu::isInDragBar (juce::Point<int> localPos) const noexcept
{
    if (localPos.y < 0 || localPos.y >= kDragBarHeight)
        return false;
    // Leave the close control for its own click handler.
    if (closeButton.isVisible() && closeButton.getBounds().contains (localPos))
        return false;
    return true;
}

void Menu::styleCloseButton() noexcept
{
    const auto ink = sharedResources.sharedColors.menuLabelTextColor1;
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, ink.withAlpha (0.85f));
    closeButton.setColour (juce::TextButton::textColourOnId, ink);
    closeButton.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
}

void Menu::layoutCloseButton() noexcept
{
    constexpr int size = 22;
    constexpr int padR = 6;
    const int y = juce::jmax (1, (kDragBarHeight - size) / 2);
    closeButton.setBounds (getWidth() - padR - size, y, size, size);
    closeButton.toFront (false);
}

void Menu::mouseDown (const juce::MouseEvent& e)
{
    dragging = isInDragBar (e.getPosition()) && ! e.mods.isPopupMenu();
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void Menu::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        dragger.dragComponent (this, e, &constrainer);
}

void Menu::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

void Menu::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isInDragBar (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                   : juce::MouseCursor::NormalCursor);
}

void Menu::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // Scroll the settings page from anywhere in the frame (drag bar, borders, gutters).
    if (! viewport.useMouseWheelMoveIfNeeded (e.getEventRelativeTo (&viewport), wheel))
        juce::Component::mouseWheelMove (e, wheel);
}

void Menu::syncScrollBarColours()
{
    const auto track = sharedResources.sharedColors.menuScrollBarTrackColor1;
    const auto thumb = sharedResources.sharedColors.menuScrollBarThumbColor1;
    const auto outline = sharedResources.sharedColors.menuScrollBarOutlineColor1;

    auto apply = [&] (CustomScrollBar* bar)
    {
        if (bar == nullptr)
            return;
        bar->setTrackBackgroundColour (track);
        bar->setThumbBackgroundColour (thumb);
        bar->setThumbOutlineColour (outline);
        bar->repaint();
    };

    apply (verticalScrollBar.get());
    apply (horizontalScrollBar.get());

    const auto fill = sharedResources.sharedColors.pluginButtonBackground;
    const auto ink = sharedResources.sharedColors.menuLabelTextColor1;
    tabPrevButton.setChromeColours (fill, ink);
    tabNextButton.setChromeColours (fill, ink);
}

void Menu::layoutScrollBars()
{
    syncScrollBarColours();

    auto r = getLocalBounds();
    r.removeFromTop (kDragBarHeight);

    // Decide which bars are needed from content vs available area.
    bool needV = contentPanel.getHeight() > r.getHeight();
    bool needH = contentPanel.getWidth() > r.getWidth();
    if (needV)
        needH = contentPanel.getWidth() > (r.getWidth() - kScrollBarThickness);
    if (needH)
        needV = contentPanel.getHeight() > (r.getHeight() - kScrollBarThickness);
    if (needV && ! needH)
        needH = contentPanel.getWidth() > (r.getWidth() - kScrollBarThickness);
    if (needH && ! needV)
        needV = contentPanel.getHeight() > (r.getHeight() - kScrollBarThickness);

    juce::Rectangle<int> vBounds, hBounds;
    if (needV && needH)
    {
        vBounds = r.removeFromRight (kScrollBarThickness);
        hBounds = r.removeFromBottom (kScrollBarThickness);
        vBounds.removeFromBottom (kScrollBarThickness); // leave corner empty
    }
    else if (needV)
    {
        vBounds = r.removeFromRight (kScrollBarThickness);
    }
    else if (needH)
    {
        hBounds = r.removeFromBottom (kScrollBarThickness);
    }

    viewport.setBounds (r);

    if (verticalScrollBar != nullptr)
    {
        verticalScrollBar->setVisible (needV);
        if (needV)
        {
            verticalScrollBar->setBounds (vBounds);
            verticalScrollBar->updateThumbPosition();
            verticalScrollBar->toFront (false);
        }
    }

    if (horizontalScrollBar != nullptr)
    {
        horizontalScrollBar->setVisible (needH);
        if (needH)
        {
            horizontalScrollBar->setBounds (hBounds);
            horizontalScrollBar->updateThumbPosition();
            horizontalScrollBar->toFront (false);
        }
    }
}

void Menu::paint (juce::Graphics& g)
{
    const auto& c1 = sharedResources.sharedColors.menuBackgroundGradientColor1;
    const auto& c2 = sharedResources.sharedColors.menuBackgroundGradientColor2;

    juce::Path panelPath;
    panelPath.addRoundedRectangle (0.0f, 0.0f, (float) getWidth(), (float) getHeight(), 14.0f);
    if (SharedResources::glowShadowEffectsEnabled())
        panelShadow.render (g, panelPath);

    juce::ColourGradient gradient (
        c1,
        juce::Point<float> ((float) getWidth() * 0.5f, 0.0f),
        c2,
        juce::Point<float> ((float) getWidth() * 0.5f, (float) getHeight()),
        false);

    g.setGradientFill (gradient);
    g.fillPath (panelPath);

    // Drag bar
    auto bar = getLocalBounds().removeFromTop (kDragBarHeight).toFloat();
    g.setColour (c1.brighter (0.08f).withAlpha (0.9f));
    g.fillRoundedRectangle (bar.getX() + 1.0f, bar.getY() + 1.0f,
                            bar.getWidth() - 2.0f, bar.getHeight() + 8.0f, 12.0f);

    g.setColour (sharedResources.sharedColors.menuLabelTextColor1.withAlpha (0.9f));
    g.setFont (juce::FontOptions().withHeight (13.0f).withStyle ("Bold"));
    // Leave room for the title-bar close (X) on the right.
    auto titleArea = getLocalBounds().removeFromTop (kDragBarHeight).reduced (10, 0);
    titleArea.removeFromRight (28);
    g.drawText ("Settings", titleArea, juce::Justification::centredLeft, false);

    g.setColour (sharedResources.sharedColors.menuTabBarBorderColor.withAlpha (0.55f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) getWidth() - 1.0f, (float) getHeight() - 1.0f, 14.0f, 1.5f);
}

int Menu::getActiveTabPreferredContentHeight() const
{
    constexpr int tabBarDepth = 35;
    auto* c = tabBar.getCurrentContentComponent();
    if (c == nullptr)
        return kContentHeight - tabBarDepth;

    if (auto* t = dynamic_cast<SpectrumComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<FftComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<OscilloscopeSettingsComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<GoniometerSettingsComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<SpectrogramSettingsComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<Spectrogram3DSettingsComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<Spectrogram3DDebugComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<LevelMetersComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<LoudnessSettingsComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<StereogramSettingsComponent*> (c))
        return t->getPreferredContentHeight();
    if (auto* t = dynamic_cast<HistogramSettingsComponent*> (c))
        return t->getPreferredContentHeight();

    // Appearance (and any future tabs without preferred-height API).
    return kContentHeight - tabBarDepth;
}

int Menu::getActiveTabPreferredContentWidth() const
{
    // Frame width is user-controlled; tabs must not request a wider panel.
    juce::ignoreUnused (tabBar);
    return kContentWidth;
}

void Menu::disableSliderScrollWheelRecursive (juce::Component& root)
{
    if (auto* slider = dynamic_cast<juce::Slider*> (&root))
        slider->setScrollWheelEnabled (false);

    for (int i = 0; i < root.getNumChildComponents(); ++i)
        if (auto* child = root.getChildComponent (i))
            disableSliderScrollWheelRecursive (*child);
}

void Menu::refreshContentPanelSize (bool preserveScrollPosition)
{
    constexpr int tabBarDepth = 35;

    // Shrinking contentPanel for the provisional measure clamps the viewport to (0,0).
    // Capture first so toggles that grow/shrink Look rows don't kick the user to the top.
    const auto savedScroll = viewport.getViewPosition();

    // Resolve viewport bounds first so content can fill the panel when the user
    // widens Settings (avoids a floating content island / grey inset frame).
    //
    // IMPORTANT: never setSize() the outer frame from tab preferred width.
    // That pushed the right edge off-screen (frame is often right-anchored) and
    // fought left-edge resize (grow-back-to-preferred). Frame width is owned by
    // the user / MainComponent layout; content lays out into the viewport.
    layoutScrollBars();

    // Content fills the panel viewport. Wider Settings → wider content; never
    // forces the outer frame wider (that used to shove the right edge off-screen).
    const int panelContentW = juce::jmax (1, viewport.getWidth());

    // Provisional size so tab content can sync/layout before we measure height.
    contentPanel.setSize (panelContentW, juce::jmax (kContentHeight, viewport.getHeight()));
    tabBar.setBounds (contentPanel.getLocalBounds());
    if (auto* c = tabBar.getCurrentContentComponent())
    {
        // Spec3D Look rows depend on prefs — sync before measuring preferred height
        // so the scrollbar range is correct without a manual resize.
        if (auto* s3d = dynamic_cast<Spectrogram3DSettingsComponent*> (c))
            s3d->syncFromMain();
        if (auto* dbg = dynamic_cast<Spectrogram3DDebugComponent*> (c))
            dbg->syncFromMain();
        c->resized();
    }

    const int preferred = getActiveTabPreferredContentHeight();
    const int contentH = juce::jmax (viewport.getHeight(), tabBarDepth + preferred);
    contentPanel.setSize (panelContentW, contentH);
    tabBar.setBounds (contentPanel.getLocalBounds());

    // Page arrows sit on the tab strip (right), clear of tab labels on each page.
    constexpr int arrowW = 22;
    constexpr int arrowH = 22;
    constexpr int arrowPad = 6;
    constexpr int arrowGap = 3;
    const int arrowY = (tabBarDepth - arrowH) / 2;
    tabNextButton.setBounds (panelContentW - arrowPad - arrowW, arrowY, arrowW, arrowH);
    tabPrevButton.setBounds (tabNextButton.getX() - arrowGap - arrowW, arrowY, arrowW, arrowH);
    tabPrevButton.toFront (false);
    tabNextButton.toFront (false);

    // Wheel over sliders should scroll the page (empty gutter isn't required).
    disableSliderScrollWheelRecursive (contentPanel);

    layoutScrollBars();

    if (preserveScrollPosition)
        viewport.setViewPosition (savedScroll); // Viewport clamps to the new content size.

    if (verticalScrollBar != nullptr)
        verticalScrollBar->updateThumbPosition();
    if (horizontalScrollBar != nullptr)
        horizontalScrollBar->updateThumbPosition();
}

void Menu::notifyContentHeightChanged()
{
    refreshContentPanelSize (true);
}

void Menu::setResizeLimitsWithinParent (juce::Rectangle<int> parentLocalBounds) noexcept
{
    if (parentLocalBounds.isEmpty())
        return;

    const int maxW = juce::jmax (200, parentLocalBounds.getWidth());
    const int maxH = juce::jmax (140, parentLocalBounds.getHeight());
    constrainer.setMinimumSize (200, 140);
    constrainer.setMaximumSize (maxW, maxH);
    // Prefer keeping the panel fully inside the parent when possible.
    constrainer.setMinimumOnscreenAmounts (
        juce::jmin (kDragBarHeight, maxH),
        juce::jmin (80, maxW / 4),
        juce::jmin (40, maxH / 4),
        juce::jmin (80, maxW / 4));
}

void Menu::resized()
{
    refreshContentPanelSize (true);

    tabBar.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);

    if (borderResizer != nullptr)
    {
        borderResizer->setBounds (getLocalBounds());
        borderResizer->toFront (false);
    }

    // After the border resizer so the X stays clickable on the top-right edge.
    layoutCloseButton();
}

void Menu::buttonClicked (juce::Button* button)
{
    juce::ignoreUnused (button);
}

void Menu::updateColors (const juce::Array<juce::Colour>& colors)
{
    juce::ignoreUnused (colors);

    textButtonLookAndFeel.setButtonOutlineColor (sharedResources.sharedColors.menuThinBorderColor);
    textButtonLookAndFeel.setButtonTextColor (sharedResources.sharedColors.menuButtonTextColor1);
    textButtonLookAndFeel.setGradientColor1 (sharedResources.sharedColors.menuButtonGradientColor1);
    textButtonLookAndFeel.setGradientColor2 (sharedResources.sharedColors.menuButtonGradientColor2);

    tabBar.setColour (juce::TabbedComponent::outlineColourId, juce::Colours::transparentBlack);
    tabBar.repaint();
    syncScrollBarColours();
    styleCloseButton();

    if (appearanceComponentRef != nullptr)
        appearanceComponentRef->repaintNewPresetButton();

    repaint();
}

void Menu::setAppearanceComponentRef (AppearanceComponent& component)
{
    appearanceComponentRef = &component;
}
