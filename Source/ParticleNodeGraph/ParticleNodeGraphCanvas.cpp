#include "ParticleNodeGraphCanvas.h"
#include "../Menu/SharedResources.h"
#include "../Spectrogram3DComponent.h"
#include "../ComboBoxLookAndFeel.h"
#include <cmath>

namespace ParticleNodeGraph
{

ParticleNodeGraphCanvas::ParticleNodeGraphCanvas()
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    model_.onChanged = [this] { repaint(); };
    startTimerHz (30);
}

ParticleNodeGraphCanvas::~ParticleNodeGraphCanvas()
{
    stopTimer();
}

void ParticleNodeGraphCanvas::timerCallback()
{
    // Soft repaint while wiring for smooth rubber-band.
    if (dragMode_ == DragMode::wire || dragMode_ == DragMode::boxSelect)
        repaint();
}

juce::Point<float> ParticleNodeGraphCanvas::screenToCanvas (juce::Point<float> screen) const
{
    return (screen - viewOffset_) / viewScale_;
}

juce::Point<float> ParticleNodeGraphCanvas::canvasToScreen (juce::Point<float> canvas) const
{
    return canvas * viewScale_ + viewOffset_;
}

void ParticleNodeGraphCanvas::loadDefaultGraph()
{
    model_.createDefaultGraph();
    if (onStatus)
        onStatus ("Default particle graph loaded.");
}

void ParticleNodeGraphCanvas::applyGraphToTarget()
{
    if (target_ == nullptr)
    {
        if (onStatus)
            onStatus ("No Spec3D target.");
        return;
    }
    const auto r = compileGraph (model_, target_->nextParticleForceUid());
    if (! r.ok)
    {
        if (onStatus)
            onStatus (r.message);
        return;
    }
    applyCompileResult (*target_, r);
    if (onStatus)
        onStatus (r.message);
    if (onGraphApplied)
        onGraphApplied();
}

void ParticleNodeGraphCanvas::drawGrid (juce::Graphics& g) const
{
    g.fillAll (juce::Colour (0xff1a1c20));
    const float major = 100.0f * viewScale_;
    const float minor = 20.0f * viewScale_;
    const auto bounds = getLocalBounds().toFloat();

    auto drawLines = [&] (float step, juce::Colour col)
    {
        if (step < 4.0f)
            return;
        g.setColour (col);
        const float ox = std::fmod (viewOffset_.x, step);
        const float oy = std::fmod (viewOffset_.y, step);
        for (float x = ox; x < bounds.getWidth(); x += step)
            g.drawVerticalLine ((int) x, 0.0f, bounds.getHeight());
        for (float y = oy; y < bounds.getHeight(); y += step)
            g.drawHorizontalLine ((int) y, 0.0f, bounds.getWidth());
    };
    drawLines (minor, juce::Colour (0xff252830));
    drawLines (major, juce::Colour (0xff2e3340));
}

void ParticleNodeGraphCanvas::drawWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                                        juce::Colour col, float thickness) const
{
    juce::Path p;
    p.startNewSubPath (a);
    const float dx = juce::jmax (40.0f, std::abs (b.x - a.x) * 0.5f);
    p.cubicTo ({ a.x + dx, a.y }, { b.x - dx, b.y }, b);
    g.setColour (col);
    g.strokePath (p, juce::PathStrokeType (thickness, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
}

void ParticleNodeGraphCanvas::drawNode (juce::Graphics& g, const GraphNode& n) const
{
    const auto b = GraphModel::nodeBounds (n);
    const auto sb = juce::Rectangle<float> (canvasToScreen (b.getTopLeft()),
                                            canvasToScreen (b.getBottomRight()));
    const float r = 6.0f * viewScale_;

    // Shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (sb.translated (2.0f, 3.0f), r);

    const juce::Colour body (0xff2a2e38);
    juce::Colour header = n.hasCustomHeaderColour() ? n.headerColour()
                                                    : juce::Colour (0xff343a48);
    if (n.selected)
        header = header.brighter (0.25f).interpolatedWith (juce::Colour (0xff3d6df0), 0.45f);
    g.setColour (body);
    g.fillRoundedRectangle (sb, r);
    auto headerR = sb.withHeight (GraphModel::headerH() * viewScale_);
    g.setColour (header);
    g.fillRoundedRectangle (headerR.getX(), headerR.getY(), headerR.getWidth(),
                            headerR.getHeight() + r, r);
    g.fillRect (headerR.withTrimmedTop (headerR.getHeight() * 0.4f));

    g.setColour (n.selected ? juce::Colour (0xff7aa2ff) : juce::Colour (0xff4a5160));
    g.drawRoundedRectangle (sb, r, 1.2f * viewScale_);

    g.setColour (juce::Colours::white.withAlpha (0.92f));
    g.setFont (juce::Font (juce::FontOptions (12.0f * viewScale_).withStyle ("Bold")));
    g.drawText (nodeKindName (n.kind), headerR.reduced (8.0f * viewScale_, 0),
                juce::Justification::centredLeft, true);

    if (n.kind == NodeKind::comment)
    {
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (11.0f * viewScale_)));
        g.drawFittedText (n.commentText, sb.reduced (10.0f * viewScale_).toNearestInt(),
                          juce::Justification::topLeft, 4);
        return;
    }

    const auto desc = n.desc();
    g.setFont (juce::Font (juce::FontOptions (10.5f * viewScale_)));
    const float row = GraphModel::rowH() * viewScale_;
    const float y0 = sb.getY() + GraphModel::headerH() * viewScale_;

    for (int i = 0; i < desc.inputs.size(); ++i)
    {
        const float y = y0 + row * (0.5f + (float) i);
        const auto col = pinTypeColour (desc.inputs[i].type);
        g.setColour (col);
        g.fillEllipse (sb.getX() - 5.0f * viewScale_, y - 5.0f * viewScale_,
                       10.0f * viewScale_, 10.0f * viewScale_);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (desc.inputs[i].label,
                    juce::Rectangle<float> (sb.getX() + 10.0f * viewScale_, y - row * 0.5f,
                                           sb.getWidth() * 0.5f, row),
                    juce::Justification::centredLeft, true);
    }
    for (int i = 0; i < desc.outputs.size(); ++i)
    {
        const float y = y0 + row * (0.5f + (float) i);
        const auto col = pinTypeColour (desc.outputs[i].type);
        g.setColour (col);
        g.fillEllipse (sb.getRight() - 5.0f * viewScale_, y - 5.0f * viewScale_,
                       10.0f * viewScale_, 10.0f * viewScale_);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (desc.outputs[i].label,
                    juce::Rectangle<float> (sb.getCentreX(), y - row * 0.5f,
                                           sb.getWidth() * 0.5f - 10.0f * viewScale_, row),
                    juce::Justification::centredRight, true);
    }

    // Compact param readout
    if (desc.paramKeys.size() > 0)
    {
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions (9.0f * viewScale_)));
        juce::String line;
        for (int i = 0; i < juce::jmin (3, desc.paramKeys.size()); ++i)
        {
            if (i)
                line << "  ";
            line << desc.paramKeys[i] << "=" << juce::String (n.param (desc.paramKeys[i]), 2);
        }
        g.drawText (line, sb.withTop (sb.getBottom() - 16.0f * viewScale_).reduced (8.0f * viewScale_, 0),
                    juce::Justification::centredLeft, true);
    }
}

void ParticleNodeGraphCanvas::paint (juce::Graphics& g)
{
    drawGrid (g);

    // Wires under nodes
    for (const auto& w : model_.wires())
    {
        juce::Point<float> a, b;
        PinType ta, tb;
        if (! model_.pinCentre (w.fromNode, w.fromPin, false, a, ta))
            continue;
        if (! model_.pinCentre (w.toNode, w.toPin, true, b, tb))
            continue;
        const auto sa = canvasToScreen (a);
        const auto sb = canvasToScreen (b);
        juce::Colour col = w.typeValid ? pinTypeColour (ta).withAlpha (0.85f)
                                       : juce::Colour (0xffff3344);
        float thick = (w.id == hoverWire_ || w.id == model_.selectedWireId() ? 3.5f : 2.2f) * viewScale_;
        drawWire (g, sa, sb, col, thick);
    }

    if (dragMode_ == DragMode::wire && wireFrom_)
    {
        juce::Point<float> a;
        PinType t;
        if (model_.pinCentre (wireFrom_->nodeId, wireFrom_->pinId, wireFrom_->isInput, a, t)
            || model_.pinCentre (wireFrom_->nodeId, wireFrom_->pinId, ! wireFrom_->isInput, a, t))
        {
            // wireFrom is always stored as the output side after mouseDown
            model_.pinCentre (wireFrom_->nodeId, wireFrom_->pinId, false, a, t);
            const auto sa = canvasToScreen (a);
            const auto sb = canvasToScreen (wireCursor_);
            bool ok = true;
            if (hoverPin_ && hoverPin_->isInput)
                ok = pinTypesCompatible (t, hoverPin_->type);
            drawWire (g, sa, sb, ok ? pinTypeColour (t) : juce::Colour (0xffff3344),
                      2.5f * viewScale_);
        }
    }

    for (const auto& n : model_.nodes())
        drawNode (g, n);

    if (dragMode_ == DragMode::boxSelect)
    {
        auto r = boxSelectRect_;
        const auto s0 = canvasToScreen (r.getTopLeft());
        const auto s1 = canvasToScreen (r.getBottomRight());
        juce::Rectangle<float> sr (s0, s1);
        g.setColour (juce::Colour (0x553d6df0));
        g.fillRect (sr);
        g.setColour (juce::Colour (0xff7aa2ff));
        g.drawRect (sr, 1.0f);
    }

    if (hoverTip_.isNotEmpty())
    {
        const auto tipFont = juce::Font (juce::FontOptions (12.0f));
        g.setFont (tipFont);
        const float tw = juce::GlyphArrangement::getStringWidth (tipFont, hoverTip_) + 16.0f;
        auto tipR = juce::Rectangle<float> ((float) hoverTipPos_.x + 14.0f,
                                            (float) hoverTipPos_.y + 18.0f, tw, 24.0f);
        tipR = tipR.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));
        g.setColour (juce::Colour (0xf0222228));
        g.fillRoundedRectangle (tipR, 4.0f);
        g.setColour (juce::Colour (0xffff6677));
        g.drawRoundedRectangle (tipR, 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.drawText (hoverTip_, tipR, juce::Justification::centred, true);
    }

    // HUD
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText ("MMB pan · Wheel zoom · RMB create/node · Ctrl+click pin disconnect · L arrange · Del delete",
                getLocalBounds().removeFromBottom (22).reduced (8, 0),
                juce::Justification::centredLeft, false);
}

void ParticleNodeGraphCanvas::resized() {}

void ParticleNodeGraphCanvas::updateHover (juce::Point<float> canvasPos)
{
    hoverPin_ = model_.hitTestPin (canvasPos);
    hoverNode_ = hoverPin_ ? 0 : model_.hitTestNode (canvasPos);
    hoverWire_ = (hoverPin_ || hoverNode_) ? 0 : model_.hitTestWire (canvasPos);
    hoverTip_.clear();
}

void ParticleNodeGraphCanvas::mouseMove (const juce::MouseEvent& e)
{
    updateHover (screenToCanvas (e.position));
    hoverTipPos_ = e.getPosition();
    repaint();
}

void ParticleNodeGraphCanvas::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    const auto canvas = screenToCanvas (e.position);
    lastCanvas_ = canvas;
    dragStartScreen_ = e.position;
    dragStartCanvas_ = canvas;
    viewOffsetAtDragStart_ = viewOffset_;
    hoverTip_.clear();

    if (e.mods.isMiddleButtonDown() || (e.mods.isLeftButtonDown() && e.mods.isShiftDown())
        || (e.mods.isLeftButtonDown() && juce::KeyPress::isKeyCurrentlyDown (' ')))
    {
        dragMode_ = DragMode::pan;
        return;
    }

    if (e.mods.isPopupMenu())
    {
        // Node context (colour / arrange) vs empty-canvas create menu.
        if (const auto nid = model_.hitTestNode (canvas))
        {
            model_.selectNode (nid, false);
            showNodeContextMenu (nid, e.getScreenPosition());
        }
        else
            showCreateMenu (e.getScreenPosition(), canvas);
        return;
    }

    if (auto pin = model_.hitTestPin (canvas))
    {
        // Ctrl / Cmd + click on a pin circle: break all wires on that pin.
        if (e.mods.isCommandDown() || e.mods.isCtrlDown())
        {
            const int n = model_.removeWiresOnPin (pin->nodeId, pin->pinId, pin->isInput);
            if (onStatus)
                onStatus (n > 0 ? juce::String ("Disconnected ") + juce::String (n) + " wire(s)."
                                : "No wires on that pin.");
            dragMode_ = DragMode::none;
            repaint();
            return;
        }

        if (! pin->isInput)
        {
            dragMode_ = DragMode::wire;
            wireFrom_ = pin;
            wireCursor_ = canvas;
            return;
        }
        // Grabbing an input: pull existing wire to rewire.
        for (const auto& w : model_.wires())
        {
            if (w.toNode == pin->nodeId && w.toPin == pin->pinId)
            {
                PinType t;
                juce::Point<float> c;
                if (model_.pinCentre (w.fromNode, w.fromPin, false, c, t))
                {
                    model_.removeWire (w.id);
                    dragMode_ = DragMode::wire;
                    wireFrom_ = PinHit { w.fromNode, w.fromPin, false, t, c };
                    wireCursor_ = canvas;
                    return;
                }
            }
        }
    }

    if (const auto nid = model_.hitTestNode (canvas))
    {
        const bool add = e.mods.isCommandDown() || e.mods.isCtrlDown();
        if (auto* n = model_.findNode (nid))
            if (! n->selected)
                model_.selectNode (nid, add);
        dragMode_ = DragMode::moveNodes;
        return;
    }

    if (const auto wid = model_.hitTestWire (canvas))
    {
        model_.selectWire (wid);
        repaint();
        return;
    }

    if (! e.mods.isCommandDown() && ! e.mods.isCtrlDown())
        model_.clearSelection();
    dragMode_ = DragMode::boxSelect;
    boxSelectRect_ = { canvas, canvas };
    repaint();
}

void ParticleNodeGraphCanvas::mouseDrag (const juce::MouseEvent& e)
{
    const auto canvas = screenToCanvas (e.position);
    if (dragMode_ == DragMode::pan)
    {
        viewOffset_ = viewOffsetAtDragStart_ + (e.position - dragStartScreen_);
        repaint();
        return;
    }
    if (dragMode_ == DragMode::moveNodes)
    {
        model_.moveSelectedBy (canvas - lastCanvas_);
        lastCanvas_ = canvas;
        return;
    }
    if (dragMode_ == DragMode::wire)
    {
        wireCursor_ = canvas;
        updateHover (canvas);
        if (hoverPin_ && wireFrom_ && hoverPin_->isInput)
        {
            if (! pinTypesCompatible (wireFrom_->type, hoverPin_->type))
            {
                hoverTip_ = juce::String ("Cannot connect ")
                            + pinTypeName (wireFrom_->type) + " → "
                            + pinTypeName (hoverPin_->type);
                hoverTipPos_ = e.getPosition();
            }
            else
                hoverTip_.clear();
        }
        repaint();
        return;
    }
    if (dragMode_ == DragMode::boxSelect)
    {
        boxSelectRect_ = juce::Rectangle<float> (dragStartCanvas_, canvas);
        repaint();
    }
}

void ParticleNodeGraphCanvas::mouseUp (const juce::MouseEvent& e)
{
    const auto canvas = screenToCanvas (e.position);
    if (dragMode_ == DragMode::wire && wireFrom_)
    {
        if (auto pin = model_.hitTestPin (canvas))
        {
            if (pin->isInput && pin->nodeId != wireFrom_->nodeId)
            {
                if (! pinTypesCompatible (wireFrom_->type, pin->type))
                {
                    // Create invalid wire for red feedback + toast
                    model_.connect (wireFrom_->nodeId, wireFrom_->pinId, pin->nodeId, pin->pinId);
                    hoverTip_ = juce::String ("Type mismatch: ")
                                + pinTypeName (wireFrom_->type) + " → "
                                + pinTypeName (pin->type);
                    hoverTipPos_ = e.getPosition();
                    if (onStatus)
                        onStatus (hoverTip_);
                }
                else
                {
                    model_.connect (wireFrom_->nodeId, wireFrom_->pinId, pin->nodeId, pin->pinId);
                    hoverTip_.clear();
                }
            }
        }
        wireFrom_.reset();
    }
    if (dragMode_ == DragMode::boxSelect)
    {
        auto r = boxSelectRect_;
        if (r.getWidth() < 0) { r.setX (r.getRight()); r.setWidth (-r.getWidth()); }
        if (r.getHeight() < 0) { r.setY (r.getBottom()); r.setHeight (-r.getHeight()); }
        const bool add = e.mods.isCommandDown() || e.mods.isCtrlDown();
        if (! add)
            model_.clearSelection();
        for (const auto& n : model_.nodes())
            if (r.intersects (GraphModel::nodeBounds (n)))
                model_.selectNode (n.id, true);
    }
    dragMode_ = DragMode::none;
    repaint();
}

void ParticleNodeGraphCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const float factor = (w.deltaY > 0 ? 1.1f : 0.9f);
    const float newScale = juce::jlimit (0.25f, 2.5f, viewScale_ * factor);
    const auto mouse = e.position;
    const auto before = screenToCanvas (mouse);
    viewScale_ = newScale;
    const auto after = screenToCanvas (mouse);
    viewOffset_ += (after - before) * viewScale_;
    repaint();
}

void ParticleNodeGraphCanvas::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto canvas = screenToCanvas (e.position);
    if (const auto nid = model_.hitTestNode (canvas))
        if (auto* n = model_.findNode (nid))
        {
            if (n->kind == NodeKind::constFloat || n->kind == NodeKind::constVec3
                || n->kind == NodeKind::comment)
            {
                auto* safe = this;
                juce::AlertWindow::showAsync (
                    juce::MessageBoxOptions()
                        .withTitle (n->kind == NodeKind::comment ? "Comment" : "Edit value")
                        .withMessage (n->kind == NodeKind::constVec3
                                          ? "Enter x,y,z"
                                          : "Enter value")
                        .withButton ("OK")
                        .withButton ("Cancel")
                        .withAssociatedComponent (this),
                    [safe, nid] (int result)
                    {
                        // Async alert with text editor not available simply — use default
                        juce::ignoreUnused (safe, nid, result);
                    });
            }
            // Quick param nudge dialog via CallOut would be heavier; use status for now.
            if (n->kind == NodeKind::constFloat)
            {
                n->setParam ("value", n->param ("value", 1.0f) * 2.0f);
                model_.onChanged();
                if (onStatus)
                    onStatus ("Float value doubled (double-click). Use Apply to push sim.");
            }
        }
}

void ParticleNodeGraphCanvas::arrangeGraph()
{
    model_.arrangeNodes();
    // Frame layout after arrange
    viewScale_ = 1.0f;
    viewOffset_ = { 20.0f, 20.0f };
    if (onStatus)
        onStatus ("Arranged nodes (L).");
    repaint();
}

bool ParticleNodeGraphCanvas::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        model_.deleteSelection();
        return true;
    }
    if (key == juce::KeyPress::escapeKey)
    {
        dragMode_ = DragMode::none;
        wireFrom_.reset();
        hoverTip_.clear();
        repaint();
        return true;
    }
    if (key == juce::KeyPress ('a') && key.getModifiers().isCommandDown())
    {
        for (const auto& n : model_.nodes())
            model_.selectNode (n.id, true);
        repaint();
        return true;
    }
    // L = layered auto-layout (Houdini / UE style columns)
    if (key == juce::KeyPress ('l') || key == juce::KeyPress ('L'))
    {
        if (! key.getModifiers().isAnyModifierKeyDown())
        {
            arrangeGraph();
            return true;
        }
    }
    return false;
}

void ParticleNodeGraphCanvas::showCreateMenu (juce::Point<int> screenPos, juce::Point<float> canvasPos)
{
    juce::PopupMenu root;
    root.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    struct Item { int id; NodeKind kind; };
    juce::Array<Item> items;
    int nextId = 1;
    auto addKind = [&] (juce::PopupMenu& m, NodeKind k)
    {
        const int id = nextId++;
        items.add ({ id, k });
        m.addItem (id, nodeKindName (k));
    };

    juce::PopupMenu sys, emit, force, combine, math, vec, cons;
    addKind (sys, NodeKind::simOutput);
    addKind (sys, NodeKind::comment);
    addKind (emit, NodeKind::emitterSpectrogram);
    addKind (emit, NodeKind::emitterPoint);
    addKind (force, NodeKind::forceGravity);
    addKind (force, NodeKind::forceDrag);
    addKind (force, NodeKind::forceWind);
    addKind (force, NodeKind::forceCurl);
    addKind (force, NodeKind::forceTurbulence);
    addKind (force, NodeKind::forceRotation);
    addKind (combine, NodeKind::combineForce);
    addKind (combine, NodeKind::combineEmitter);
    addKind (combine, NodeKind::combineFloat);
    addKind (combine, NodeKind::combineVec3);
    addKind (cons, NodeKind::constFloat);
    addKind (cons, NodeKind::constVec3);
    addKind (cons, NodeKind::constBool);
    for (auto k : { NodeKind::mathAdd, NodeKind::mathSub, NodeKind::mathMul, NodeKind::mathDiv,
                    NodeKind::mathLerp, NodeKind::mathClamp, NodeKind::mathAbs, NodeKind::mathNegate,
                    NodeKind::mathMin, NodeKind::mathMax, NodeKind::mathPow, NodeKind::mathSin,
                    NodeKind::mathCos, NodeKind::switchFloat })
        addKind (math, k);
    for (auto k : { NodeKind::makeVec3, NodeKind::breakVec3, NodeKind::vecLength, NodeKind::vecNormalize,
                    NodeKind::vecScale, NodeKind::vecAdd, NodeKind::vecDot, NodeKind::floatToVec3 })
        addKind (vec, k);

    root.addSubMenu ("System", sys);
    root.addSubMenu ("Emitters", emit);
    root.addSubMenu ("Forces", force);
    root.addSubMenu ("Combine", combine);
    root.addSubMenu ("Constants", cons);
    root.addSubMenu ("Math", math);
    root.addSubMenu ("Vector", vec);
    root.addSeparator();
    root.addItem (9002, "Arrange Nodes\tL");
    root.addItem (9000, "Apply graph to particles");
    root.addItem (9001, "Reset to default graph");

    root.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                            { screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<ParticleNodeGraphCanvas> (this),
                         items, canvasPos] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == 9000)
                            {
                                safe->applyGraphToTarget();
                                return;
                            }
                            if (result == 9001)
                            {
                                safe->loadDefaultGraph();
                                return;
                            }
                            if (result == 9002)
                            {
                                safe->arrangeGraph();
                                return;
                            }
                            for (const auto& it : items)
                                if (it.id == result)
                                {
                                    safe->model().addNode (it.kind, canvasPos);
                                    break;
                                }
                        });
}

void ParticleNodeGraphCanvas::showNodeContextMenu (uint32_t nodeId, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    // Header colour palette (UE-style accent chips)
    struct Swatch { const char* name; uint32_t argb; };
    static const Swatch kSwatches[] = {
        { "Default",       0 },
        { "Slate",         0xff343a48 },
        { "Blue",          0xff2b5a9e },
        { "Teal",          0xff1f6f6a },
        { "Green",         0xff2d6b3a },
        { "Amber",         0xff8a6a1a },
        { "Orange",        0xff9a4e1a },
        { "Red",           0xff8a2e2e },
        { "Magenta",       0xff7a2e6a },
        { "Purple",        0xff4a2e8a },
        { "Steel",         0xff3a4555 },
    };

    juce::PopupMenu colours;
    int cid = 100;
    juce::Array<uint32_t> colourIds;
    for (const auto& s : kSwatches)
    {
        colours.addItem (cid, s.name);
        colourIds.add (s.argb);
        ++cid;
    }
    menu.addSubMenu ("Header Colour", colours);
    menu.addSeparator();
    menu.addItem (200, "Arrange Nodes\tL");
    menu.addItem (201, "Delete Node\tDel");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                            { screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<ParticleNodeGraphCanvas> (this),
                         nodeId, colourIds] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result >= 100 && result < 100 + colourIds.size())
                            {
                                safe->model().setNodeHeaderColour (
                                    nodeId, colourIds[result - 100]);
                                if (safe->onStatus)
                                    safe->onStatus ("Header colour set.");
                                return;
                            }
                            if (result == 200)
                            {
                                safe->arrangeGraph();
                                return;
                            }
                            if (result == 201)
                            {
                                safe->model().selectNode (nodeId, false);
                                safe->model().deleteSelection();
                            }
                        });
}

//==============================================================================
namespace
{
struct GraphWindowContent final : public juce::Component
{
    ParticleNodeGraphCanvas canvas;
    juce::TextButton applyBtn { "Apply to Particles" };
    juce::TextButton resetBtn { "Reset Default Graph" };
    juce::Label status;

    GraphWindowContent()
    {
        setOpaque (true);
        addAndMakeVisible (canvas);
        addAndMakeVisible (applyBtn);
        addAndMakeVisible (resetBtn);
        addAndMakeVisible (status);
        status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.7f));
        status.setText ("Right-click canvas to add nodes. Wire output→input. Red wires = type mismatch.",
                        juce::dontSendNotification);
        applyBtn.onClick = [this] { canvas.applyGraphToTarget(); };
        resetBtn.onClick = [this] { canvas.loadDefaultGraph(); };
        canvas.onStatus = [this] (const juce::String& s)
        {
            status.setText (s, juce::dontSendNotification);
        };
    }

    void paint (juce::Graphics& g) override { g.fillAll (juce::Colour (0xff16181d)); }

    void resized() override
    {
        auto r = getLocalBounds();
        auto bar = r.removeFromBottom (36);
        status.setBounds (bar.removeFromLeft (juce::jmax (200, bar.getWidth() - 280)).reduced (8, 6));
        applyBtn.setBounds (bar.removeFromRight (150).reduced (6, 4));
        resetBtn.setBounds (bar.removeFromRight (150).reduced (6, 4));
        canvas.setBounds (r);
    }
};
} // namespace

ParticleNodeGraphWindow::ParticleNodeGraphWindow (Spectrogram3DComponent& target,
                                                  SharedResources* theme)
    : DocumentWindow ("Particle Node Graph",
                      juce::Colour (0xff1e2128),
                      DocumentWindow::closeButton | DocumentWindow::minimiseButton)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (720, 480, 4000, 3000);

    auto* layout = new GraphWindowContent();
    layout->canvas.setTheme (theme);
    layout->canvas.setTarget (&target);
    setContentOwned (layout, true);

    centreWithSize (1100, 720);
    setVisible (true);
    toFront (true);
}

ParticleNodeGraphWindow::~ParticleNodeGraphWindow()
{
    clearContentComponent();
}

ParticleNodeGraphCanvas* ParticleNodeGraphWindow::getCanvas() noexcept
{
    if (auto* c = dynamic_cast<GraphWindowContent*> (getContentComponent()))
        return &c->canvas;
    return nullptr;
}

void ParticleNodeGraphWindow::closeButtonPressed()
{
    setVisible (false);
    if (onClosed)
        onClosed();
}

} // namespace ParticleNodeGraph
