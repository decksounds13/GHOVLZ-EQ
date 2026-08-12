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
    model_.onChanged = [this]
    {
        repaint();
        if (! suppressApply_ && liveApply_ && dragMode_ != DragMode::moveNodes
            && dragMode_ != DragMode::pan && dragMode_ != DragMode::boxSelect)
            scheduleLiveApply();
    };
    startTimerHz (30);
}

ParticleNodeGraphCanvas::~ParticleNodeGraphCanvas()
{
    stopTimer();
}

void ParticleNodeGraphCanvas::timerCallback()
{
    if (dragMode_ == DragMode::wire || dragMode_ == DragMode::boxSelect)
        repaint();

    if (applyPending_ && applyCooldownTicks_ > 0)
        --applyCooldownTicks_;
    if (applyPending_ && applyCooldownTicks_ <= 0 && dragMode_ == DragMode::none)
        flushLiveApply();
}

void ParticleNodeGraphCanvas::log (LogLevel level, const juce::String& text)
{
    if (onLog)
        onLog (level, text);
    if (onStatus && (level == LogLevel::error || level == LogLevel::warning || level == LogLevel::success))
        onStatus (text);
}

void ParticleNodeGraphCanvas::scheduleLiveApply()
{
    if (! liveApply_ || target_ == nullptr)
        return;
    applyPending_ = true;
    applyCooldownTicks_ = 2; // ~60ms debounce at 30 Hz
}

void ParticleNodeGraphCanvas::flushLiveApply()
{
    applyPending_ = false;
    applyCooldownTicks_ = 0;
    applyGraphToTarget (true);
}

void ParticleNodeGraphCanvas::setLiveApplyEnabled (bool on) noexcept
{
    liveApply_ = on;
    log (LogLevel::info, liveApply_ ? "Live apply ON - connect/disconnect updates particles."
                                    : "Live apply OFF - use Apply (File menu or toolbar).");
    if (liveApply_)
        scheduleLiveApply();
}

void ParticleNodeGraphCanvas::pushUndo()
{
    if (suppressHistory_)
        return;
    undoStack_.push_back (model_.toValueTree());
    if ((int) undoStack_.size() > kMaxUndo)
        undoStack_.erase (undoStack_.begin());
    redoStack_.clear();
    if (onHistoryChanged)
        onHistoryChanged();
}

void ParticleNodeGraphCanvas::restoreSnapshot (const juce::ValueTree& snap)
{
    suppressHistory_ = true;
    suppressApply_ = true;
    model_.fromValueTree (snap);
    suppressApply_ = false;
    suppressHistory_ = false;
    repaint();
    if (liveApply_)
        scheduleLiveApply();
    if (onHistoryChanged)
        onHistoryChanged();
}

void ParticleNodeGraphCanvas::undo()
{
    if (undoStack_.empty())
        return;
    redoStack_.push_back (model_.toValueTree());
    const auto snap = undoStack_.back();
    undoStack_.pop_back();
    restoreSnapshot (snap);
    log (LogLevel::info, "Undo");
}

void ParticleNodeGraphCanvas::redo()
{
    if (redoStack_.empty())
        return;
    undoStack_.push_back (model_.toValueTree());
    const auto snap = redoStack_.back();
    redoStack_.pop_back();
    restoreSnapshot (snap);
    log (LogLevel::info, "Redo");
}

void ParticleNodeGraphCanvas::copySelection()
{
    clipboard_ = model_.copySelectionToValueTree();
    int n = 0;
    for (int i = 0; i < clipboard_.getNumChildren(); ++i)
        if (clipboard_.getChild (i).hasType ("Node"))
            ++n;
    if (n == 0)
        log (LogLevel::warning, "Nothing selected to copy.");
    else
        log (LogLevel::info, "Copied " + juce::String (n) + " node(s).");
}

void ParticleNodeGraphCanvas::pasteClipboard()
{
    if (! clipboard_.isValid() || clipboard_.getNumChildren() == 0)
    {
        log (LogLevel::warning, "Clipboard empty.");
        return;
    }
    pushUndo();
    model_.pasteValueTree (clipboard_, { 48.0f, 48.0f });
    log (LogLevel::info, "Pasted.");
    // Shift clipboard so repeated paste stacks
    // (paste already offsets; re-offset clipboard content for next paste)
}

void ParticleNodeGraphCanvas::duplicateSelection()
{
    if (model_.selectedNodeIds().empty())
    {
        log (LogLevel::warning, "Nothing selected to duplicate.");
        return;
    }
    pushUndo();
    model_.duplicateSelection ({ 40.0f, 40.0f });
    log (LogLevel::info, "Duplicated selection.");
}

void ParticleNodeGraphCanvas::cutSelection()
{
    if (model_.selectedNodeIds().empty() && model_.selectedWireId() == 0)
    {
        log (LogLevel::warning, "Nothing to cut.");
        return;
    }
    copySelection();
    pushUndo();
    model_.deleteSelection();
    log (LogLevel::info, "Cut.");
}

juce::File ParticleNodeGraphCanvas::defaultGraphFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Decksounds")
                   .getChildFile ("ParametricEq");
    dir.createDirectory();
    return dir.getChildFile ("particle_node_graph.xml");
}

bool ParticleNodeGraphCanvas::saveGraphToFile (const juce::File& file)
{
    auto tree = model_.toValueTree();
    if (auto xml = tree.createXml())
    {
        if (xml->writeTo (file))
        {
            log (LogLevel::success, "Saved graph -> " + file.getFileName());
            return true;
        }
    }
    log (LogLevel::error, "Failed to save graph.");
    return false;
}

bool ParticleNodeGraphCanvas::loadGraphFromFile (const juce::File& file)
{
    if (! file.existsAsFile())
    {
        log (LogLevel::error, "File not found: " + file.getFullPathName());
        return false;
    }
    if (auto xml = juce::parseXML (file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (tree.isValid())
        {
            pushUndo();
            suppressApply_ = true;
            model_.fromValueTree (tree);
            suppressApply_ = false;
            fitViewToGraph();
            log (LogLevel::success, "Loaded graph <- " + file.getFileName());
            if (liveApply_)
                flushLiveApply();
            return true;
        }
    }
    log (LogLevel::error, "Invalid graph file.");
    return false;
}

void ParticleNodeGraphCanvas::saveGraphDefault()
{
    saveGraphToFile (defaultGraphFile());
}

void ParticleNodeGraphCanvas::loadGraphDefault()
{
    const auto f = defaultGraphFile();
    if (f.existsAsFile())
        loadGraphFromFile (f);
    else
        log (LogLevel::warning, "No saved graph at " + f.getFullPathName());
}

void ParticleNodeGraphCanvas::editNodeProperties (uint32_t nodeId)
{
    auto* n = model_.findNode (nodeId);
    if (n == nullptr)
        return;

    // Attribute pickers use existing double-click menu
    if (n->kind == NodeKind::filterAttr || n->kind == NodeKind::colourRampAttr)
    {
        // Trigger same path as double-click via synthetic menu
        juce::Point<float> c = n->pos + juce::Point<float> (10, 10);
        // Fall through to param editor for threshold etc.
    }

    auto* aw = new juce::AlertWindow ("Edit | " + juce::String (nodeKindName (n->kind)),
                                      "Adjust parameters (applied on OK).",
                                      juce::MessageBoxIconType::NoIcon,
                                      this);

    if (n->kind == NodeKind::comment)
        aw->addTextEditor ("comment", n->commentText, "Comment");
    else if (n->kind == NodeKind::constFloat || n->kind == NodeKind::constInt
             || n->kind == NodeKind::constBool)
        aw->addTextEditor ("value", juce::String (n->param ("value", 0.0f), 4), "Value");
    else if (n->kind == NodeKind::constVec2)
    {
        aw->addTextEditor ("x", juce::String (n->param ("x"), 4), "X");
        aw->addTextEditor ("y", juce::String (n->param ("y"), 4), "Y");
    }
    else if (n->kind == NodeKind::constVec3)
    {
        aw->addTextEditor ("x", juce::String (n->param ("x"), 4), "X");
        aw->addTextEditor ("y", juce::String (n->param ("y"), 4), "Y");
        aw->addTextEditor ("z", juce::String (n->param ("z"), 4), "Z");
    }
    else if (n->kind == NodeKind::constVec4 || n->kind == NodeKind::constColour)
    {
        const bool col = n->kind == NodeKind::constColour;
        aw->addTextEditor (col ? "r" : "x", juce::String (n->param (col ? "r" : "x"), 4), col ? "R" : "X");
        aw->addTextEditor (col ? "g" : "y", juce::String (n->param (col ? "g" : "y"), 4), col ? "G" : "Y");
        aw->addTextEditor (col ? "b" : "z", juce::String (n->param (col ? "b" : "z"), 4), col ? "B" : "Z");
        aw->addTextEditor (col ? "a" : "w", juce::String (n->param (col ? "a" : "w", 1.0f), 4), col ? "A" : "W");
    }
    else
    {
        // Generic: first 8 params
        const auto desc = n->desc();
        const int maxP = juce::jmin (8, desc.paramKeys.size());
        for (int i = 0; i < maxP; ++i)
            aw->addTextEditor (desc.paramKeys[i],
                               juce::String (n->param (desc.paramKeys[i], desc.paramDefaults[i]), 4),
                               desc.paramKeys[i]);
        if (maxP == 0)
            aw->addTextBlock ("No editable parameters on this node.");
    }

    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    // takeOwnership=true -> AlertWindow deletes itself after the callback.
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<ParticleNodeGraphCanvas> (this),
         nodeId, aw] (int result)
        {
            if (safe == nullptr || result != 1)
                return;
            auto* node = safe->model().findNode (nodeId);
            if (node == nullptr)
                return;
            safe->pushUndo();
            if (node->kind == NodeKind::comment)
                node->commentText = aw->getTextEditorContents ("comment");
            else
            {
                const auto desc = node->desc();
                if (node->kind == NodeKind::constFloat || node->kind == NodeKind::constInt
                    || node->kind == NodeKind::constBool)
                    node->setParam ("value", aw->getTextEditorContents ("value").getFloatValue());
                else
                {
                    for (int i = 0; i < desc.paramKeys.size(); ++i)
                    {
                        const auto& key = desc.paramKeys[i];
                        if (aw->getTextEditor (key) != nullptr)
                            node->setParam (key, aw->getTextEditorContents (key).getFloatValue());
                    }
                    for (const char* k : { "x", "y", "z", "w", "r", "g", "b", "a", "value" })
                        if (aw->getTextEditor (k) != nullptr)
                            node->setParam (k, aw->getTextEditorContents (k).getFloatValue());
                }
            }
            if (safe->model().onChanged)
                safe->model().onChanged();
            safe->log (LogLevel::info, "Updated " + juce::String (nodeKindName (node->kind)));
        }), true);
}

bool ParticleNodeGraphCanvas::isPinCompatibleHighlight (const PinHit& pin) const
{
    if (dragMode_ != DragMode::wire || ! wireFrom_)
        return false;
    if (pin.nodeId == wireFrom_->nodeId)
        return false;
    // Dragging from output -> need input
    if (! wireFrom_->isInput && pin.isInput)
        return pinTypesCompatible (wireFrom_->type, pin.type);
    // (wireFrom is always stored as output side after mouseDown)
    if (wireFrom_->isInput && ! pin.isInput)
        return pinTypesCompatible (pin.type, wireFrom_->type);
    if (! wireFrom_->isInput && pin.isInput)
        return pinTypesCompatible (wireFrom_->type, pin.type);
    return false;
}

juce::Point<float> ParticleNodeGraphCanvas::screenToCanvas (juce::Point<float> screen) const
{
    return (screen - viewOffset_) / viewScale_;
}

juce::Point<float> ParticleNodeGraphCanvas::canvasToScreen (juce::Point<float> canvas) const
{
    return canvas * viewScale_ + viewOffset_;
}

void ParticleNodeGraphCanvas::clearGraph()
{
    pushUndo();
    suppressApply_ = true;
    model_.clear();
    const auto out = model_.addNode (NodeKind::simOutput, { 400.0f, 200.0f });
    juce::ignoreUnused (out);
    suppressApply_ = false;
    log (LogLevel::info, "Graph cleared (Simulation Output kept).");
    if (liveApply_)
        scheduleLiveApply();
}

void ParticleNodeGraphCanvas::loadDefaultGraph()
{
    pushUndo();
    suppressApply_ = true;
    model_.createDefaultGraph();
    suppressApply_ = false;
    log (LogLevel::info, "Default particle graph loaded.");
    fitViewToGraph();
    if (liveApply_)
        flushLiveApply();
    else if (onStatus)
        onStatus ("Default particle graph loaded.");
}

void ParticleNodeGraphCanvas::fitViewToGraph()
{
    auto bounds = model_.contentBounds();
    if (bounds.isEmpty())
    {
        viewScale_ = 1.0f;
        viewOffset_ = { 20.0f, 20.0f };
        repaint();
        return;
    }
    bounds = bounds.expanded (80.0f, 60.0f);
    const auto area = getLocalBounds().toFloat().reduced (8.0f);
    if (area.getWidth() < 1.0f || area.getHeight() < 1.0f || bounds.getWidth() < 1.0f)
        return;
    const float sx = area.getWidth() / bounds.getWidth();
    const float sy = area.getHeight() / bounds.getHeight();
    viewScale_ = juce::jlimit (0.35f, 1.5f, juce::jmin (sx, sy));
    viewOffset_ = area.getCentre() - bounds.getCentre() * viewScale_;
    repaint();
}

void ParticleNodeGraphCanvas::fitViewToSelection()
{
    auto bounds = model_.selectionBounds();
    if (bounds.isEmpty())
    {
        fitViewToGraph();
        return;
    }
    bounds = bounds.expanded (100.0f, 80.0f);
    const auto area = getLocalBounds().toFloat().reduced (8.0f);
    if (area.getWidth() < 1.0f || bounds.getWidth() < 1.0f)
        return;
    const float sx = area.getWidth() / bounds.getWidth();
    const float sy = area.getHeight() / bounds.getHeight();
    viewScale_ = juce::jlimit (0.4f, 1.8f, juce::jmin (sx, sy));
    viewOffset_ = area.getCentre() - bounds.getCentre() * viewScale_;
    repaint();
}

void ParticleNodeGraphCanvas::applyGraphToTarget (bool quiet)
{
    if (target_ == nullptr)
    {
        log (LogLevel::error, "No Spec3D target - open graph from Spec3D particle settings.");
        return;
    }
    const auto r = compileGraph (model_, target_->nextParticleForceUid());
    for (const auto& line : r.log)
        log (line.level, line.text);

    if (! r.ok)
    {
        if (onStatus)
            onStatus (r.message);
        return;
    }
    applyCompileResult (*target_, r);
    if (! quiet && onStatus)
        onStatus (r.message);
    else if (quiet && onStatus)
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

    auto drawPinDot = [&] (float cx, float cy, PinType type, bool isInput, const juce::String& pinId)
    {
        PinHit ph { n.id, pinId, isInput, type, {} };
        const bool hi = isPinCompatibleHighlight (ph)
                        || (hoverPin_ && hoverPin_->nodeId == n.id && hoverPin_->pinId == pinId
                            && hoverPin_->isInput == isInput);
        const float rad = (hi ? 7.0f : 5.0f) * viewScale_;
        if (hi && dragMode_ == DragMode::wire)
        {
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.fillEllipse (cx - rad - 3.0f, cy - rad - 3.0f, (rad + 3.0f) * 2.0f, (rad + 3.0f) * 2.0f);
        }
        g.setColour (pinTypeColour (type));
        g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        if (hi)
        {
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, 1.5f * viewScale_);
        }
    };

    for (int i = 0; i < desc.inputs.size(); ++i)
    {
        const float y = y0 + row * (0.5f + (float) i);
        drawPinDot (sb.getX(), y, desc.inputs[i].type, true, desc.inputs[i].id);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawText (desc.inputs[i].label,
                    juce::Rectangle<float> (sb.getX() + 10.0f * viewScale_, y - row * 0.5f,
                                           sb.getWidth() * 0.5f, row),
                    juce::Justification::centredLeft, true);
    }
    for (int i = 0; i < desc.outputs.size(); ++i)
    {
        const float y = y0 + row * (0.5f + (float) i);
        drawPinDot (sb.getRight(), y, desc.outputs[i].type, false, desc.outputs[i].id);
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
    juce::String hud = "Ctrl+Z undo | C/V/D copy/paste/dup | RMB pin create+wire | Del | L arrange";
    if (liveApply_)
        hud += " | LIVE";
    if (snapEnabled_)
        hud += " | SNAP";
    g.drawText (hud, getLocalBounds().removeFromBottom (22).reduced (8, 0),
                juce::Justification::centredLeft, false);
}

void ParticleNodeGraphCanvas::resized() {}

void ParticleNodeGraphCanvas::updateHover (juce::Point<float> canvasPos)
{
    hoverPin_ = model_.hitTestPin (canvasPos);
    hoverNode_ = hoverPin_ ? 0 : model_.hitTestNode (canvasPos);
    hoverWire_ = (hoverPin_ || hoverNode_) ? 0 : model_.hitTestWire (canvasPos);
    hoverTip_.clear();

    if (hoverPin_)
    {
        const auto* n = model_.findNode (hoverPin_->nodeId);
        juce::String tip;
        tip << (hoverPin_->isInput ? "In | " : "Out | ")
            << hoverPin_->pinId << "  (" << pinTypeName (hoverPin_->type) << ")";
        if (n != nullptr)
            tip << "  |  " << nodeKindName (n->kind);
        if (dragMode_ == DragMode::wire && wireFrom_)
        {
            const bool ok = isPinCompatibleHighlight (*hoverPin_);
            tip << (ok ? "  compatible" : "  incompatible");
        }
        else
            tip << "  |  RMB create+wire";
        hoverTip_ = tip;
    }
    else if (hoverWire_)
    {
        hoverTip_ = "Wire | Del to remove";
    }
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
        // Pin first (dots sit on the node body) - create compatible node + wire.
        if (auto pin = model_.hitTestPin (canvas, 14.0f))
        {
            showPinCreateMenu (*pin, e.getScreenPosition());
            return;
        }
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
            pushUndo();
            const int n = model_.removeWiresOnPin (pin->nodeId, pin->pinId, pin->isInput);
            if (n > 0)
                log (LogLevel::info, "Disconnected " + juce::String (n) + " wire(s).");
            else if (onStatus)
                onStatus ("No wires on that pin.");
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
        movePushedUndo_ = false;
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
        if (! movePushedUndo_)
        {
            pushUndo();
            movePushedUndo_ = true;
        }
        model_.moveSelectedBy (canvas - lastCanvas_, true);
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
                            + pinTypeName (wireFrom_->type) + " -> "
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
    const bool wasMove = dragMode_ == DragMode::moveNodes;
    if (dragMode_ == DragMode::wire && wireFrom_)
    {
        if (auto pin = model_.hitTestPin (canvas))
        {
            if (pin->isInput && pin->nodeId != wireFrom_->nodeId)
            {
                if (! pinTypesCompatible (wireFrom_->type, pin->type))
                {
                    hoverTip_ = juce::String ("Type mismatch: ")
                                + pinTypeName (wireFrom_->type) + " -> "
                                + pinTypeName (pin->type) + " (refused)";
                    hoverTipPos_ = e.getPosition();
                    log (LogLevel::warning, hoverTip_);
                }
                else
                {
                    pushUndo();
                    const auto wid = model_.connect (wireFrom_->nodeId, wireFrom_->pinId,
                                                     pin->nodeId, pin->pinId);
                    hoverTip_.clear();
                    if (wid != 0)
                        log (LogLevel::debug,
                             juce::String ("Connected ") + pinTypeName (wireFrom_->type)
                             + " -> " + pinTypeName (pin->type));
                    else
                        log (LogLevel::warning, "Connection refused (cycle or invalid).");
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
    if (wasMove && snapEnabled_)
        model_.snapSelectedToGrid (GraphModel::gridSize());

    dragMode_ = DragMode::none;
    movePushedUndo_ = false;
    // Force order uses node Y - re-apply after moves when live.
    if (wasMove && liveApply_)
        scheduleLiveApply();
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
            // Filter / colour-ramp: pick attribute from dropdown
            if (n->kind == NodeKind::filterAttr || n->kind == NodeKind::colourRampAttr)
            {
                juce::PopupMenu menu;
                menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
                int count = 0;
                const auto* list = builtinAttrList (count);
                const int cur = (int) std::lround (n->param ("attr", (float) AttrId::fft));
                for (int i = 0; i < count; ++i)
                {
                    const auto id = list[i];
                    menu.addItem (1000 + (int) id, attrMenuLabel (id), true, (int) id == cur);
                }
                if (n->kind == NodeKind::filterAttr)
                {
                    menu.addSeparator();
                    menu.addItem (1, "Threshold 0.15");
                    menu.addItem (2, "Threshold 0.25");
                    menu.addItem (3, "Threshold 0.50");
                    menu.addItem (4, "Stage: Spawn only");
                    menu.addItem (5, "Stage: Update only");
                    menu.addItem (6, "Stage: Both");
                }
                const uint32_t nodeId = nid;
                menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                    [safe = juce::Component::SafePointer<ParticleNodeGraphCanvas> (this),
                                     nodeId] (int result)
                                    {
                                        if (safe == nullptr || result == 0)
                                            return;
                                        auto* node = safe->model().findNode (nodeId);
                                        if (node == nullptr)
                                            return;
                                        safe->pushUndo();
                                        if (result >= 1000)
                                        {
                                            node->setParam ("attr", (float) (result - 1000));
                                            safe->log (LogLevel::info,
                                                       "Attribute -> "
                                                       + juce::String (attrName ((AttrId) (result - 1000))));
                                        }
                                        else if (result == 1) node->setParam ("threshold", 0.15f);
                                        else if (result == 2) node->setParam ("threshold", 0.25f);
                                        else if (result == 3) node->setParam ("threshold", 0.50f);
                                        else if (result == 4) node->setParam ("stage", 0.0f);
                                        else if (result == 5) node->setParam ("stage", 1.0f);
                                        else if (result == 6) node->setParam ("stage", 2.0f);
                                        if (safe->model().onChanged)
                                            safe->model().onChanged();
                                    });
                return;
            }
            editNodeProperties (nid);
        }
}

void ParticleNodeGraphCanvas::arrangeGraph()
{
    model_.arrangeNodes();
    fitViewToGraph();
    log (LogLevel::info, "Arranged nodes (L).");
    if (liveApply_)
        scheduleLiveApply();
}

bool ParticleNodeGraphCanvas::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const bool cmd = mods.isCommandDown() || mods.isCtrlDown();

    if (key == juce::KeyPress::returnKey && cmd)
    {
        applyGraphToTarget (false);
        return true;
    }
    if (cmd && key.getKeyCode() == 'z')
    {
        if (mods.isShiftDown())
            redo();
        else
            undo();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'y' || key.getKeyCode() == 'Y'))
    {
        redo();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'c' || key.getKeyCode() == 'C'))
    {
        copySelection();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'x' || key.getKeyCode() == 'X'))
    {
        cutSelection();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'v' || key.getKeyCode() == 'V'))
    {
        pasteClipboard();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'd' || key.getKeyCode() == 'D'))
    {
        duplicateSelection();
        return true;
    }
    if (cmd && (key.getKeyCode() == 's' || key.getKeyCode() == 'S'))
    {
        saveGraphDefault();
        return true;
    }
    if (cmd && (key.getKeyCode() == 'o' || key.getKeyCode() == 'O'))
    {
        loadGraphDefault();
        return true;
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (model_.selectedWireId() != 0 || ! model_.selectedNodeIds().empty())
        {
            pushUndo();
            model_.deleteSelection();
        }
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
    if (key == juce::KeyPress::homeKey)
    {
        fitViewToGraph();
        return true;
    }
    if (! cmd && (key.getKeyCode() == 'f' || key.getKeyCode() == 'F'))
    {
        fitViewToSelection();
        return true;
    }
    if (cmd && key.getKeyCode() == 'a')
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
            pushUndo();
            arrangeGraph();
            return true;
        }
    }
    return false;
}

namespace
{
/**
    Pick a pin on `desc` to auto-wire against `otherType`.
    wantOutput=true  -> choose an OUTPUT that can feed an input of type otherType
                       (wire: new.out -> target.in)
    wantOutput=false -> choose an INPUT that can receive an output of type otherType
                       (wire: target.out -> new.in)
*/
static juce::String pickBestPin (const NodeDesc& desc, PinType otherType, bool wantOutput)
{
    const auto& list = wantOutput ? desc.outputs : desc.inputs;
    auto compatible = [&] (const PinDesc& p) -> bool
    {
        // Wire direction is always output -> input.
        if (wantOutput)
            return pinTypesCompatible (p.type, otherType);
        return pinTypesCompatible (otherType, p.type);
    };

    static const char* preferOut[] = {
        "out", "value", "result", "particles", "emitter", "force", "colour", nullptr
    };
    static const char* preferIn[] = {
        "in", "in0", "a", "x", "v", "s", "particles", "emitter", "force",
        "rate", "life", "size", "enabled", "strength", "c0", "c1", "sel", nullptr
    };
    const char** prefer = wantOutput ? preferOut : preferIn;

    for (int i = 0; prefer[i] != nullptr; ++i)
        for (const auto& pin : list)
            if (pin.id == prefer[i] && compatible (pin))
                return pin.id;

    for (const auto& pin : list)
        if (compatible (pin))
            return pin.id;
    return {};
}

static bool nodeHasCompatiblePin (NodeKind kind, PinType targetType, bool targetIsInput)
{
    // targetIsInput -> need a node that outputs a compatible type.
    // !targetIsInput -> need a node that inputs a compatible type.
    const auto desc = describeNode (kind);
    return pickBestPin (desc, targetType, targetIsInput).isNotEmpty();
}
} // namespace

uint32_t ParticleNodeGraphCanvas::createAndConnectToPin (const PinHit& pin, NodeKind kind)
{
    const auto desc = describeNode (kind);
    const bool pinIsInput = pin.isInput;

    juce::String otherPinId;
    if (pinIsInput)
        otherPinId = pickBestPin (desc, pin.type, true);  // new node's output
    else
        otherPinId = pickBestPin (desc, pin.type, false); // new node's input

    if (otherPinId.isEmpty())
    {
        log (LogLevel::warning, juce::String ("No compatible pin on ") + nodeKindName (kind));
        return 0;
    }

    // Place new node to the left of inputs, right of outputs.
    juce::Point<float> pinCentre;
    PinType pt;
    if (! model_.pinCentre (pin.nodeId, pin.pinId, pin.isInput, pinCentre, pt))
        pinCentre = { 200.0f, 200.0f };

    const float gap = 220.0f;
    juce::Point<float> pos;
    if (pinIsInput)
        pos = { pinCentre.x - gap, pinCentre.y - GraphModel::headerH() };
    else
        pos = { pinCentre.x + 40.0f, pinCentre.y - GraphModel::headerH() };

    // Nudge if stacked on an existing node
    for (int attempt = 0; attempt < 12; ++attempt)
    {
        bool hit = false;
        const juce::Rectangle<float> trial { pos.x, pos.y, GraphModel::nodeW(), 80.0f };
        for (const auto& n : model_.nodes())
            if (GraphModel::nodeBounds (n).intersects (trial.expanded (8.0f)))
            {
                hit = true;
                break;
            }
        if (! hit)
            break;
        pos.y += GraphModel::rowH() * 2.5f;
    }

    pushUndo();
    const uint32_t newId = model_.addNode (kind, pos);
    if (newId == 0)
        return 0;

    if (snapEnabled_)
        if (auto* nn = model_.findNode (newId))
        {
            nn->pos.x = GraphModel::snap (nn->pos.x);
            nn->pos.y = GraphModel::snap (nn->pos.y);
        }

    uint32_t wireId = 0;
    if (pinIsInput)
        wireId = model_.connect (newId, otherPinId, pin.nodeId, pin.pinId);
    else
        wireId = model_.connect (pin.nodeId, pin.pinId, newId, otherPinId);

    model_.clearSelection();
    model_.selectNode (newId, false);

    log (LogLevel::info,
         juce::String ("Created ") + nodeKindName (kind)
         + " -> wired " + pinTypeName (pin.type)
         + (wireId != 0 ? juce::String() : juce::String (" (wire failed)")));
    return newId;
}

void ParticleNodeGraphCanvas::showPinCreateMenu (const PinHit& pin, juce::Point<int> screenPos)
{
    juce::PopupMenu root;
    root.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    // Menu title context
    const juce::String dir = pin.isInput ? "-> feed this input" : "-> take this output";
    root.addSectionHeader (juce::String (pinTypeName (pin.type)) + "  " + dir);
    root.addSeparator();

    struct Item { int id; NodeKind kind; };
    juce::Array<Item> items;
    int nextId = 1;

    auto addKind = [&] (juce::PopupMenu& m, NodeKind k) -> bool
    {
        if (k == NodeKind::comment)
            return false;
        // Don't offer wiring a second Simulation Output into itself awkwardly often,
        // but allow if it has a matching pin (e.g. rare).
        if (! nodeHasCompatiblePin (k, pin.type, pin.isInput))
            return false;
        const int id = nextId++;
        items.add ({ id, k });
        m.addItem (id, nodeKindName (k));
        return true;
    };

    auto addCategory = [&] (const char* name, std::initializer_list<NodeKind> kinds)
    {
        juce::PopupMenu sub;
        int added = 0;
        for (auto k : kinds)
            if (addKind (sub, k))
                ++added;
        if (added > 0)
            root.addSubMenu (name, sub);
    };

    // Flat "quick picks" first for common constants matching the type
    {
        juce::PopupMenu quick;
        int q = 0;
        auto tryQuick = [&] (NodeKind k)
        {
            if (addKind (quick, k))
                ++q;
        };
        switch (pin.type)
        {
            case PinType::floatT:
                tryQuick (NodeKind::constFloat);
                tryQuick (NodeKind::uniformAmplitude);
                tryQuick (NodeKind::uniformTime);
                tryQuick (NodeKind::mapRange);
                tryQuick (NodeKind::mathAdd);
                tryQuick (NodeKind::mathMul);
                break;
            case PinType::intT:
                tryQuick (NodeKind::constInt);
                break;
            case PinType::boolT:
                tryQuick (NodeKind::constBool);
                break;
            case PinType::vec2:
                tryQuick (NodeKind::constVec2);
                tryQuick (NodeKind::makeVec2);
                break;
            case PinType::vec3:
                tryQuick (NodeKind::constVec3);
                tryQuick (NodeKind::makeVec3);
                tryQuick (NodeKind::floatToVec3);
                break;
            case PinType::vec4:
                tryQuick (NodeKind::constVec4);
                tryQuick (NodeKind::makeVec4);
                break;
            case PinType::colour:
                tryQuick (NodeKind::constColour);
                tryQuick (NodeKind::makeColour);
                tryQuick (NodeKind::colourLerp);
                tryQuick (NodeKind::colourRampAttr);
                break;
            case PinType::particles:
                tryQuick (NodeKind::emitterSpectrogram);
                tryQuick (NodeKind::emitterPoint);
                tryQuick (NodeKind::filterAttr);
                tryQuick (NodeKind::colourRampAttr);
                tryQuick (NodeKind::combineParticles);
                break;
            case PinType::emitter:
                tryQuick (NodeKind::emitterSpectrogram);
                tryQuick (NodeKind::emitterPoint);
                tryQuick (NodeKind::combineEmitter);
                break;
            case PinType::force:
                tryQuick (NodeKind::forceGravity);
                tryQuick (NodeKind::forceDrag);
                tryQuick (NodeKind::forceWind);
                tryQuick (NodeKind::forceCurl);
                tryQuick (NodeKind::forceTurbulence);
                tryQuick (NodeKind::forceRotation);
                tryQuick (NodeKind::combineForce);
                break;
            default:
                break;
        }
        if (q > 0)
        {
            root.addSubMenu ("Quick", quick);
            root.addSeparator();
        }
    }

    addCategory ("System", { NodeKind::simOutput });
    addCategory ("Emitters", { NodeKind::emitterSpectrogram, NodeKind::emitterPoint });
    addCategory ("Forces", { NodeKind::forceGravity, NodeKind::forceDrag, NodeKind::forceWind,
                             NodeKind::forceCurl, NodeKind::forceTurbulence, NodeKind::forceRotation });
    addCategory ("Attributes", { NodeKind::filterAttr });
    addCategory ("Colour", { NodeKind::constColour, NodeKind::makeColour, NodeKind::breakColour,
                             NodeKind::colourLerp, NodeKind::colourMul, NodeKind::colourRampAttr });
    addCategory ("Uniforms", { NodeKind::uniformTime, NodeKind::uniformDelta, NodeKind::uniformAmplitude });
    addCategory ("Combine", { NodeKind::combineForce, NodeKind::combineEmitter, NodeKind::combineParticles,
                              NodeKind::combineFloat, NodeKind::combineVec3 });
    addCategory ("Constants", { NodeKind::constFloat, NodeKind::constInt, NodeKind::constBool,
                                NodeKind::constVec2, NodeKind::constVec3, NodeKind::constVec4,
                                NodeKind::constColour });
    addCategory ("Math", { NodeKind::mathAdd, NodeKind::mathSub, NodeKind::mathMul, NodeKind::mathDiv,
                           NodeKind::mathLerp, NodeKind::mathClamp, NodeKind::mathAbs, NodeKind::mathNegate,
                           NodeKind::mathMin, NodeKind::mathMax, NodeKind::mathPow, NodeKind::mathSin,
                           NodeKind::mathCos, NodeKind::switchFloat });
    addCategory ("Vector", { NodeKind::makeVec2, NodeKind::breakVec2, NodeKind::makeVec3, NodeKind::breakVec3,
                             NodeKind::makeVec4, NodeKind::breakVec4, NodeKind::vecLength, NodeKind::vecNormalize,
                             NodeKind::vecScale, NodeKind::vecAdd, NodeKind::vecDot, NodeKind::floatToVec3 });
    addCategory ("Utility", { NodeKind::mapRange, NodeKind::thresholdGate, NodeKind::convertToFloat,
                              NodeKind::convertToVec3, NodeKind::convertToColour });

    if (items.isEmpty())
    {
        root.addItem (-1, "(no compatible nodes)", false);
    }

    // Also offer disconnect if the pin already has wires
    const int disconnectId = 9901;
    {
        int wireCount = 0;
        for (const auto& w : model_.wires())
        {
            if (pin.isInput && w.toNode == pin.nodeId && w.toPin == pin.pinId)
                ++wireCount;
            if (! pin.isInput && w.fromNode == pin.nodeId && w.fromPin == pin.pinId)
                ++wireCount;
        }
        if (wireCount > 0)
        {
            root.addSeparator();
            root.addItem (disconnectId, "Disconnect " + juce::String (wireCount) + " wire(s)");
        }
    }

    const PinHit pinCopy = pin;
    root.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                            { screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<ParticleNodeGraphCanvas> (this),
                         items, pinCopy, disconnectId] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == disconnectId)
                            {
                                safe->pushUndo();
                                const int n = safe->model().removeWiresOnPin (
                                    pinCopy.nodeId, pinCopy.pinId, pinCopy.isInput);
                                safe->log (LogLevel::info,
                                           "Disconnected " + juce::String (n) + " wire(s).");
                                return;
                            }
                            for (const auto& it : items)
                                if (it.id == result)
                                {
                                    safe->createAndConnectToPin (pinCopy, it.kind);
                                    break;
                                }
                        });
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

    juce::PopupMenu sys, emit, force, combine, math, vec, cons, colour, attr, unif, util;
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
    addKind (combine, NodeKind::combineParticles);
    addKind (combine, NodeKind::combineFloat);
    addKind (combine, NodeKind::combineVec3);
    addKind (cons, NodeKind::constFloat);
    addKind (cons, NodeKind::constInt);
    addKind (cons, NodeKind::constBool);
    addKind (cons, NodeKind::constVec2);
    addKind (cons, NodeKind::constVec3);
    addKind (cons, NodeKind::constVec4);
    addKind (cons, NodeKind::constColour);
    for (auto k : { NodeKind::mathAdd, NodeKind::mathSub, NodeKind::mathMul, NodeKind::mathDiv,
                    NodeKind::mathLerp, NodeKind::mathClamp, NodeKind::mathAbs, NodeKind::mathNegate,
                    NodeKind::mathMin, NodeKind::mathMax, NodeKind::mathPow, NodeKind::mathSin,
                    NodeKind::mathCos, NodeKind::switchFloat })
        addKind (math, k);
    for (auto k : { NodeKind::makeVec2, NodeKind::breakVec2, NodeKind::makeVec3, NodeKind::breakVec3,
                    NodeKind::makeVec4, NodeKind::breakVec4, NodeKind::vecLength, NodeKind::vecNormalize,
                    NodeKind::vecScale, NodeKind::vecAdd, NodeKind::vecDot, NodeKind::floatToVec3 })
        addKind (vec, k);
    addKind (colour, NodeKind::constColour);
    addKind (colour, NodeKind::makeColour);
    addKind (colour, NodeKind::breakColour);
    addKind (colour, NodeKind::colourLerp);
    addKind (colour, NodeKind::colourMul);
    addKind (colour, NodeKind::colourRampAttr);
    addKind (attr, NodeKind::filterAttr);
    addKind (unif, NodeKind::uniformTime);
    addKind (unif, NodeKind::uniformDelta);
    addKind (unif, NodeKind::uniformAmplitude);
    addKind (util, NodeKind::mapRange);
    addKind (util, NodeKind::thresholdGate);
    addKind (util, NodeKind::convertToFloat);
    addKind (util, NodeKind::convertToVec3);
    addKind (util, NodeKind::convertToColour);

    root.addSubMenu ("System", sys);
    root.addSubMenu ("Emitters", emit);
    root.addSubMenu ("Forces", force);
    root.addSubMenu ("Attributes", attr);
    root.addSubMenu ("Colour", colour);
    root.addSubMenu ("Uniforms", unif);
    root.addSubMenu ("Combine", combine);
    root.addSubMenu ("Constants", cons);
    root.addSubMenu ("Math", math);
    root.addSubMenu ("Vector", vec);
    root.addSubMenu ("Utility", util);
    root.addSeparator();
    root.addItem (9002, "Arrange Nodes\tL");
    root.addItem (9003, "Fit View");
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
                            if (result == 9003)
                            {
                                safe->fitViewToGraph();
                                return;
                            }
                            for (const auto& it : items)
                                if (it.id == result)
                                {
                                    safe->pushUndo();
                                    auto pos = canvasPos;
                                    if (safe->isSnapEnabled())
                                    {
                                        pos.x = GraphModel::snap (pos.x);
                                        pos.y = GraphModel::snap (pos.y);
                                    }
                                    safe->model().addNode (it.kind, pos);
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
    menu.addItem (199, "Edit Properties...\tDouble-click");
    menu.addSubMenu ("Header Colour", colours);
    menu.addSeparator();
    menu.addItem (202, "Duplicate\tCtrl+D");
    menu.addItem (200, "Arrange Nodes\tL");
    menu.addItem (201, "Delete Node\tDel");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (
                            { screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<ParticleNodeGraphCanvas> (this),
                         nodeId, colourIds] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;
                            if (result == 199)
                            {
                                safe->editNodeProperties (nodeId);
                                return;
                            }
                            if (result >= 100 && result < 100 + colourIds.size())
                            {
                                safe->pushUndo();
                                safe->model().setNodeHeaderColour (
                                    nodeId, colourIds[result - 100]);
                                if (safe->onStatus)
                                    safe->onStatus ("Header colour set.");
                                return;
                            }
                            if (result == 200)
                            {
                                safe->pushUndo();
                                safe->arrangeGraph();
                                return;
                            }
                            if (result == 202)
                            {
                                safe->model().clearSelection();
                                safe->model().selectNode (nodeId, false);
                                safe->duplicateSelection();
                                return;
                            }
                            if (result == 201)
                            {
                                safe->pushUndo();
                                safe->model().selectNode (nodeId, false);
                                safe->model().deleteSelection();
                            }
                        });
}

//==============================================================================
GraphOutputPanel::GraphOutputPanel()
{
    setOpaque (true);
    log_.setMultiLine (true);
    log_.setReadOnly (true);
    log_.setScrollbarsShown (true);
    log_.setCaretVisible (false);
    log_.setFont (juce::Font (juce::FontOptions (12.0f)));
    log_.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff12141a));
    log_.setColour (juce::TextEditor::textColourId, juce::Colour (0xffd0d4dc));
    log_.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    log_.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (log_);
    addAndMakeVisible (clearBtn_);
    addAndMakeVisible (toggleBtn_);
    clearBtn_.onClick = [this] { clear(); };
    toggleBtn_.onClick = [this] { toggle(); };
    toggleBtn_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e38));
    clearBtn_.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2a2e38));
    append (LogLevel::info, "Output ready. Live apply is ON - wiring updates particles immediately.");
}

void GraphOutputPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1d24));
    g.setColour (juce::Colour (0xff2e3340));
    g.drawLine (0.0f, 0.5f, (float) getWidth(), 0.5f, 1.0f);
}

void GraphOutputPanel::resized()
{
    auto r = getLocalBounds();
    auto bar = r.removeFromTop (collapsedH_);
    toggleBtn_.setBounds (bar.removeFromLeft (110).reduced (4, 4));
    clearBtn_.setBounds (bar.removeFromRight (72).reduced (4, 4));
    if (expanded_)
        log_.setBounds (r.reduced (4, 2));
    else
        log_.setBounds ({});
}

void GraphOutputPanel::append (LogLevel level, const juce::String& text)
{
    const char* tag = "[i] ";
    switch (level)
    {
        case LogLevel::success: tag = "[ok] "; break;
        case LogLevel::warning: tag = "[!] "; break;
        case LogLevel::error:   tag = "[err] "; break;
        case LogLevel::debug:   tag = "[dbg] "; break;
        default: break;
    }
    const auto line = juce::Time::getCurrentTime().formatted ("%H:%M:%S ") + tag + text;
    log_.moveCaretToEnd();
    log_.insertTextAtCaret (line + "\n");
    log_.moveCaretToEnd();
    // Cap growth
    if (log_.getTotalNumChars() > 40000)
        log_.setText (log_.getText().substring (log_.getText().length() - 20000));
}

void GraphOutputPanel::clear()
{
    log_.clear();
    append (LogLevel::info, "Output cleared.");
}

void GraphOutputPanel::setExpanded (bool on)
{
    if (expanded_ == on)
        return;
    expanded_ = on;
    toggleBtn_.setButtonText (expanded_ ? "Hide Output" : "Show Output");
    resized();
    if (onExpandChanged)
        onExpandChanged();
}

//==============================================================================
class ParticleNodeGraphWindow::Content final : public juce::Component
{
public:
    ParticleNodeGraphCanvas canvas;
    GraphOutputPanel output;
    juce::TextButton applyBtn { "Apply" };
    juce::TextButton liveBtn { "Live: ON" };
    juce::Label status;

    Content (Spectrogram3DComponent& target, SharedResources* theme)
    {
        setOpaque (true);
        canvas.setTheme (theme);
        canvas.setTarget (&target);
        addAndMakeVisible (canvas);
        addAndMakeVisible (output);
        addAndMakeVisible (applyBtn);
        addAndMakeVisible (liveBtn);
        addAndMakeVisible (status);

        status.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.75f));
        status.setText ("Live apply ON | connect/disconnect to update particles",
                        juce::dontSendNotification);
        applyBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2b5a9e));
        liveBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1f6f4a));
        applyBtn.onClick = [this] { canvas.applyGraphToTarget (false); };
        liveBtn.onClick = [this]
        {
            canvas.setLiveApplyEnabled (! canvas.isLiveApplyEnabled());
            liveBtn.setButtonText (canvas.isLiveApplyEnabled() ? "Live: ON" : "Live: OFF");
            liveBtn.setColour (juce::TextButton::buttonColourId,
                               canvas.isLiveApplyEnabled() ? juce::Colour (0xff1f6f4a)
                                                           : juce::Colour (0xff5a3a1f));
        };
        canvas.onStatus = [this] (const juce::String& s)
        {
            status.setText (s, juce::dontSendNotification);
        };
        canvas.onLog = [this] (LogLevel level, const juce::String& text)
        {
            output.append (level, text);
        };
        output.onExpandChanged = [this] { resized(); };

        // Initial live push so opening the window syncs the default graph.
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Content> (this)]
        {
            if (safe != nullptr)
                safe->canvas.applyGraphToTarget (true);
        });
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff16181d));
        g.setColour (juce::Colour (0xff1e222b));
        g.fillRect (getLocalBounds().removeFromTop (34));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto tool = r.removeFromTop (34);
        applyBtn.setBounds (tool.removeFromRight (88).reduced (6, 5));
        liveBtn.setBounds (tool.removeFromRight (96).reduced (4, 5));
        status.setBounds (tool.reduced (10, 6));

        const int outH = output.preferredHeight();
        output.setBounds (r.removeFromBottom (outH));
        canvas.setBounds (r);
    }
};

ParticleNodeGraphWindow::ParticleNodeGraphWindow (Spectrogram3DComponent& target,
                                                  SharedResources* theme)
    : DocumentWindow ("Particle Node Graph",
                      juce::Colour (0xff1e2128),
                      DocumentWindow::closeButton | DocumentWindow::minimiseButton)
{
    setUsingNativeTitleBar (true);
    setResizable (true, true);
    setResizeLimits (800, 520, 4000, 3000);

    auto* layout = new Content (target, theme);
    content_ = layout;
    setContentOwned (layout, true);
    // Native menu bar under title (File / Options / View / Help)
    setMenuBar (this);

    centreWithSize (1200, 800);
    setVisible (true);
    toFront (true);
}

ParticleNodeGraphWindow::~ParticleNodeGraphWindow()
{
    setMenuBar (nullptr);
    content_ = nullptr;
    clearContentComponent();
}

ParticleNodeGraphCanvas* ParticleNodeGraphWindow::getCanvas() noexcept
{
    return content_ != nullptr ? &content_->canvas : nullptr;
}

void ParticleNodeGraphWindow::closeButtonPressed()
{
    setVisible (false);
    if (onClosed)
        onClosed();
}

juce::StringArray ParticleNodeGraphWindow::getMenuBarNames()
{
    return { "File", "Edit", "Options", "View", "Help" };
}

juce::PopupMenu ParticleNodeGraphWindow::getMenuForIndex (int topLevelIndex, const juce::String&)
{
    juce::PopupMenu m;
    m.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    auto* c = getCanvas();
    switch (topLevelIndex)
    {
        case 0: // File
            m.addItem (1001, "New Graph");
            m.addItem (1002, "Reset to Default Graph");
            m.addSeparator();
            m.addItem (1005, "Save Graph\tCtrl+S");
            m.addItem (1006, "Load Graph\tCtrl+O");
            m.addItem (1007, "Save Graph As...");
            m.addItem (1008, "Load Graph From...");
            m.addSeparator();
            m.addItem (1003, "Apply to Particles\tCtrl+Enter");
            m.addSeparator();
            m.addItem (1004, "Close");
            break;
        case 1: // Edit
            m.addItem (1101, "Undo\tCtrl+Z", c != nullptr && c->canUndo());
            m.addItem (1102, "Redo\tCtrl+Y", c != nullptr && c->canRedo());
            m.addSeparator();
            m.addItem (1103, "Cut\tCtrl+X");
            m.addItem (1104, "Copy\tCtrl+C");
            m.addItem (1105, "Paste\tCtrl+V");
            m.addItem (1106, "Duplicate\tCtrl+D");
            m.addItem (1107, "Delete\tDel");
            m.addSeparator();
            m.addItem (1108, "Select All\tCtrl+A");
            break;
        case 2: // Options
            m.addItem (2001, "Live Apply", true, c != nullptr && c->isLiveApplyEnabled());
            m.addItem (2003, "Snap to Grid", true, c != nullptr && c->isSnapEnabled());
            m.addSeparator();
            m.addItem (2002, "Clear Output Log");
            break;
        case 3: // View
            m.addItem (3001, "Arrange Nodes\tL");
            m.addItem (3002, "Fit View to Graph\tHome");
            m.addItem (3004, "Frame Selection\tF");
            m.addSeparator();
            m.addItem (3003, "Show Output Console", true,
                       content_ != nullptr && content_->output.isExpanded());
            break;
        case 4: // Help
            m.addItem (4001, "Shortcuts...");
            m.addItem (4002, "About Particle Node Graph");
            break;
        default:
            break;
    }
    return m;
}

void ParticleNodeGraphWindow::menuItemSelected (int menuItemID, int)
{
    auto* c = getCanvas();
    if (c == nullptr && menuItemID != 1004)
        return;

    switch (menuItemID)
    {
        case 1001: c->clearGraph(); break;
        case 1002: c->loadDefaultGraph(); break;
        case 1003: c->applyGraphToTarget (false); break;
        case 1004: closeButtonPressed(); break;
        case 1005: c->saveGraphDefault(); break;
        case 1006: c->loadGraphDefault(); break;
        case 1007:
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Save particle graph", ParticleNodeGraphCanvas::defaultGraphFile(), "*.xml");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [c, chooser] (const juce::FileChooser& fc)
                                  {
                                      auto f = fc.getResult();
                                      if (f != juce::File())
                                      {
                                          if (! f.hasFileExtension (".xml"))
                                              f = f.withFileExtension (".xml");
                                          c->saveGraphToFile (f);
                                      }
                                  });
            break;
        }
        case 1008:
        {
            auto chooser = std::make_shared<juce::FileChooser> (
                "Load particle graph", ParticleNodeGraphCanvas::defaultGraphFile(), "*.xml");
            chooser->launchAsync (juce::FileBrowserComponent::openMode
                                      | juce::FileBrowserComponent::canSelectFiles,
                                  [c, chooser] (const juce::FileChooser& fc)
                                  {
                                      const auto f = fc.getResult();
                                      if (f != juce::File())
                                          c->loadGraphFromFile (f);
                                  });
            break;
        }
        case 1101: c->undo(); menuItemsChanged(); break;
        case 1102: c->redo(); menuItemsChanged(); break;
        case 1103: c->cutSelection(); break;
        case 1104: c->copySelection(); break;
        case 1105: c->pasteClipboard(); break;
        case 1106: c->duplicateSelection(); break;
        case 1107:
            c->pushUndo();
            c->model().deleteSelection();
            break;
        case 1108:
            for (const auto& n : c->model().nodes())
                c->model().selectNode (n.id, true);
            break;
        case 2001:
            c->setLiveApplyEnabled (! c->isLiveApplyEnabled());
            if (content_ != nullptr)
            {
                content_->liveBtn.setButtonText (c->isLiveApplyEnabled() ? "Live: ON" : "Live: OFF");
                content_->liveBtn.setColour (juce::TextButton::buttonColourId,
                                             c->isLiveApplyEnabled() ? juce::Colour (0xff1f6f4a)
                                                                     : juce::Colour (0xff5a3a1f));
            }
            menuItemsChanged();
            break;
        case 2003:
            c->setSnapEnabled (! c->isSnapEnabled());
            menuItemsChanged();
            break;
        case 2002:
            if (content_ != nullptr)
                content_->output.clear();
            break;
        case 3001: c->pushUndo(); c->arrangeGraph(); break;
        case 3002: c->fitViewToGraph(); break;
        case 3004: c->fitViewToSelection(); break;
        case 3003:
            if (content_ != nullptr)
            {
                content_->output.toggle();
                menuItemsChanged();
            }
            break;
        case 4001:
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                "Shortcuts",
                "Navigation\n"
                "  MMB / Space+LMB - pan | Wheel - zoom | Home - fit all | F - frame selection\n\n"
                "Editing\n"
                "  Ctrl+Z / Y - undo / redo | Ctrl+C/X/V/D - copy/cut/paste/duplicate\n"
                "  Del - delete | Double-click - edit properties | L - arrange\n\n"
                "Wiring\n"
                "  Drag pin -> pin | Ctrl+click pin - disconnect\n"
                "  RMB pin - create compatible node + wire\n"
                "  Compatible pins glow while dragging a wire\n\n"
                "Graph\n"
                "  Ctrl+S / O - save / load | Ctrl+Enter - apply\n"
                "  Live Apply - particles update on connect/disconnect");
            break;
        case 4002:
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                "Particle Node Graph",
                "Industry-style typed node editor for Spec3D particles.\n\n"
                "Undo/redo | copy/paste | grid snap | type-safe wiring\n"
                "RMB-pin create | live apply | save/load | output log\n\n"
                "Types: Float, Int, Bool, Vec2/3/4, Colour,\n"
                "Particles, Force, Emitter (Field reserved for fluids).");
            break;
        default:
            break;
    }
}

} // namespace ParticleNodeGraph
