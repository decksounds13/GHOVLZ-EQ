#pragma once

#include "ScopeModules.h"
#include <JuceHeader.h>
#include <memory>
#include <optional>
#include <vector>

/**
    Recursive BSP viewport tree for Scope mode (Maya / Blender style).
    Leaves hold at most one live module instance; extra cells stay unassigned.
*/
namespace ScopeViewport
{
enum class Split : uint8_t { none = 0, horizontal, vertical };

struct Node
{
    Split split = Split::none;
    float frac = 0.5f; // first-child share
    std::optional<ScopeModuleId> module; // leaf only
    std::unique_ptr<Node> a, b;

    bool isLeaf() const noexcept { return split == Split::none; }

    std::unique_ptr<Node> clone() const
    {
        auto n = std::make_unique<Node>();
        n->split = split;
        n->frac = frac;
        n->module = module;
        if (a != nullptr)
            n->a = a->clone();
        if (b != nullptr)
            n->b = b->clone();
        return n;
    }
};

struct LeafHit
{
    Node* node = nullptr;
    juce::Rectangle<int> cell;
    juce::Rectangle<int> content;
    std::optional<ScopeModuleId> module;
};

struct SplitHit
{
    Node* node = nullptr;
    bool vertical = false; // vertical bar (left/right)
    juce::Rectangle<int> seam;
    juce::Rectangle<int> parent;
};

inline std::unique_ptr<Node> makeLeaf (std::optional<ScopeModuleId> id = std::nullopt)
{
    auto n = std::make_unique<Node>();
    n->module = id;
    return n;
}

inline std::unique_ptr<Node> makeSplit (Split kind, float frac,
                                        std::unique_ptr<Node> first,
                                        std::unique_ptr<Node> second)
{
    auto n = std::make_unique<Node>();
    n->split = kind;
    n->frac = juce::jlimit (0.12f, 0.88f, frac);
    n->a = std::move (first);
    n->b = std::move (second);
    return n;
}

inline int countLeaves (const Node* n) noexcept
{
    if (n == nullptr)
        return 0;
    if (n->isLeaf())
        return 1;
    return countLeaves (n->a.get()) + countLeaves (n->b.get());
}

inline void collectModules (const Node* n, std::vector<ScopeModuleId>& out)
{
    if (n == nullptr)
        return;
    if (n->isLeaf())
    {
        if (n->module.has_value())
            out.push_back (*n->module);
        return;
    }
    collectModules (n->a.get(), out);
    collectModules (n->b.get(), out);
}

inline Node* findLeafUsing (Node* n, ScopeModuleId id) noexcept
{
    if (n == nullptr)
        return nullptr;
    if (n->isLeaf())
        return (n->module.has_value() && *n->module == id) ? n : nullptr;
    if (auto* l = findLeafUsing (n->a.get(), id))
        return l;
    return findLeafUsing (n->b.get(), id);
}

inline void clearModuleUses (Node* n, ScopeModuleId id) noexcept
{
    if (n == nullptr)
        return;
    if (n->isLeaf())
    {
        if (n->module.has_value() && *n->module == id)
            n->module.reset();
        return;
    }
    clearModuleUses (n->a.get(), id);
    clearModuleUses (n->b.get(), id);
}

inline void assignLeaf (Node& root, Node& leaf, std::optional<ScopeModuleId> id)
{
    if (id.has_value())
        clearModuleUses (&root, *id);
    leaf.module = id;
}

/** Split a leaf in place. Second child is unassigned (one live instance per module). */
inline bool splitLeaf (Node& leaf, Split kind)
{
    if (! leaf.isLeaf() || kind == Split::none)
        return false;
    auto first = makeLeaf (leaf.module);
    auto second = makeLeaf (std::nullopt);
    leaf.split = kind;
    leaf.frac = 0.5f;
    leaf.module.reset();
    leaf.a = std::move (first);
    leaf.b = std::move (second);
    return true;
}

/** Close a leaf: replace its parent split with the sibling. Cannot close the last pane. */
inline bool closeLeaf (std::unique_ptr<Node>& root, Node* leaf)
{
    if (root == nullptr || leaf == nullptr || ! leaf->isLeaf())
        return false;
    if (root.get() == leaf)
        return false; // last pane

    std::function<bool (std::unique_ptr<Node>&)> walk;
    walk = [&] (std::unique_ptr<Node>& n) -> bool
    {
        if (n == nullptr || n->isLeaf())
            return false;
        if (n->a.get() == leaf)
        {
            n = std::move (n->b);
            return true;
        }
        if (n->b.get() == leaf)
        {
            n = std::move (n->a);
            return true;
        }
        return walk (n->a) || walk (n->b);
    };
    return walk (root);
}

inline void layoutInto (Node& n,
                        juce::Rectangle<int> cell,
                        int gap,
                        std::vector<LeafHit>& leaves,
                        std::vector<SplitHit>& splits)
{
    if (n.isLeaf())
    {
        LeafHit h;
        h.node = &n;
        h.cell = cell;
        h.content = cell; // caller insets with ScopePaneChrome::contentBounds
        h.module = n.module;
        leaves.push_back (h);
        return;
    }

    const int g = juce::jmax (0, gap);
    auto r = cell;
    if (n.split == Split::vertical)
    {
        const int w = juce::jmax (8, r.getWidth() - g);
        int leftW = juce::jlimit (8, w - 8, juce::roundToInt ((float) w * n.frac));
        auto left = r.removeFromLeft (leftW);
        auto seam = r.removeFromLeft (g);
        SplitHit sh;
        sh.node = &n;
        sh.vertical = true;
        sh.parent = cell;
        sh.seam = seam.isEmpty() ? juce::Rectangle<int> (left.getRight(), left.getY(), g, left.getHeight())
                                 : seam;
        splits.push_back (sh);
        if (n.a != nullptr)
            layoutInto (*n.a, left, gap, leaves, splits);
        if (n.b != nullptr)
            layoutInto (*n.b, r, gap, leaves, splits);
    }
    else
    {
        const int h = juce::jmax (8, r.getHeight() - g);
        int topH = juce::jlimit (8, h - 8, juce::roundToInt ((float) h * n.frac));
        auto top = r.removeFromTop (topH);
        auto seam = r.removeFromTop (g);
        SplitHit sh;
        sh.node = &n;
        sh.vertical = false;
        sh.parent = cell;
        sh.seam = seam.isEmpty() ? juce::Rectangle<int> (top.getX(), top.getBottom(), top.getWidth(), g)
                                 : seam;
        splits.push_back (sh);
        if (n.a != nullptr)
            layoutInto (*n.a, top, gap, leaves, splits);
        if (n.b != nullptr)
            layoutInto (*n.b, r, gap, leaves, splits);
    }
}

// Need ScopePaneChrome - include after. I'll fix layoutInto to take a content mapper.

inline std::unique_ptr<juce::XmlElement> toXml (const Node& n)
{
    auto xml = std::make_unique<juce::XmlElement> ("Pane");
    if (n.isLeaf())
    {
        xml->setAttribute ("split", "leaf");
        if (n.module.has_value())
            xml->setAttribute ("module", ScopeModules::idToKey (*n.module));
        return xml;
    }
    xml->setAttribute ("split", n.split == Split::vertical ? "v" : "h");
    xml->setAttribute ("frac", (double) n.frac);
    if (n.a != nullptr)
        xml->addChildElement (toXml (*n.a).release());
    if (n.b != nullptr)
        xml->addChildElement (toXml (*n.b).release());
    return xml;
}

inline std::unique_ptr<Node> fromXml (const juce::XmlElement& xml)
{
    const auto kind = xml.getStringAttribute ("split", "leaf");
    if (kind == "leaf" || (! xml.hasAttribute ("split") && xml.getNumChildElements() == 0))
    {
        std::optional<ScopeModuleId> id;
        if (xml.hasAttribute ("module"))
            id = ScopeModules::keyToId (xml.getStringAttribute ("module"));
        return makeLeaf (id);
    }

    auto aEl = xml.getChildByName ("Pane");
    juce::XmlElement* bEl = nullptr;
    if (aEl != nullptr)
        bEl = aEl->getNextElementWithTagName ("Pane");

    auto first = aEl != nullptr ? fromXml (*aEl) : makeLeaf();
    auto second = bEl != nullptr ? fromXml (*bEl) : makeLeaf();
    const auto split = (kind == "v") ? Split::vertical : Split::horizontal;
    return makeSplit (split, (float) xml.getDoubleAttribute ("frac", 0.5),
                      std::move (first), std::move (second));
}

// ---- factory trees ----

inline std::unique_ptr<Node> grid2x2 (ScopeModuleId tl, ScopeModuleId tr,
                                      ScopeModuleId bl, ScopeModuleId br)
{
    return makeSplit (Split::vertical, 0.5f,
                      makeSplit (Split::horizontal, 0.5f, makeLeaf (tl), makeLeaf (bl)),
                      makeSplit (Split::horizontal, 0.5f, makeLeaf (tr), makeLeaf (br)));
}

inline std::unique_ptr<Node> splitH (ScopeModuleId top, ScopeModuleId bot)
{
    return makeSplit (Split::horizontal, 0.5f, makeLeaf (top), makeLeaf (bot));
}

inline std::unique_ptr<Node> splitV (ScopeModuleId left, ScopeModuleId right)
{
    return makeSplit (Split::vertical, 0.5f, makeLeaf (left), makeLeaf (right));
}

inline std::unique_ptr<Node> makeGrid (int cols, int rows, const ScopeModuleId* fill, int fillN)
{
    cols = juce::jmax (1, cols);
    rows = juce::jmax (1, rows);
    std::function<std::unique_ptr<Node> (int, int, int, int)> build;
    build = [&] (int c0, int c1, int r0, int r1) -> std::unique_ptr<Node>
    {
        const int cw = c1 - c0;
        const int rh = r1 - r0;
        if (cw <= 1 && rh <= 1)
        {
            const int idx = r0 * cols + c0;
            std::optional<ScopeModuleId> id;
            if (fill != nullptr && idx >= 0 && idx < fillN)
                id = fill[idx];
            return makeLeaf (id);
        }
        if (cw >= rh && cw > 1)
        {
            const int mid = c0 + cw / 2;
            const float frac = (float) (mid - c0) / (float) cw;
            return makeSplit (Split::vertical, frac,
                              build (c0, mid, r0, r1),
                              build (mid, c1, r0, r1));
        }
        const int mid = r0 + rh / 2;
        const float frac = (float) (mid - r0) / (float) rh;
        return makeSplit (Split::horizontal, frac,
                          build (c0, c1, r0, mid),
                          build (c0, c1, mid, r1));
    };
    return build (0, cols, 0, rows);
}

inline std::unique_ptr<Node> makeRow (const ScopeModuleId* ids, int n)
{
    if (n <= 0)
        return makeLeaf();
    if (n == 1)
        return makeLeaf (ids[0]);
    auto right = makeRow (ids + 1, n - 1);
    return makeSplit (Split::vertical, 1.0f / (float) n, makeLeaf (ids[0]), std::move (right));
}

struct Factory
{
    juce::String id;
    juce::String name;
    std::function<std::unique_ptr<Node>()> build;
};

inline const std::vector<Factory>& factories()
{
    static const std::vector<Factory> list = {
        { "h50", "Horizontal", [] {
            return splitH (ScopeModuleId::spectrum, ScopeModuleId::oscilloscope);
        }},
        { "v50", "Vertical", [] {
            return splitV (ScopeModuleId::spectrum, ScopeModuleId::oscilloscope);
        }},
        { "grid2", "2 x 2", [] {
            return grid2x2 (ScopeModuleId::goniometer, ScopeModuleId::spectrum,
                            ScopeModuleId::oscilloscope, ScopeModuleId::spectrogram);
        }},
        { "grid3", "3 x 3", [] {
            const ScopeModuleId fill[] = {
                ScopeModuleId::goniometer, ScopeModuleId::spectrum, ScopeModuleId::oscilloscope,
                ScopeModuleId::spectrogram, ScopeModuleId::levelIn, ScopeModuleId::loudness,
                ScopeModuleId::stereogram, ScopeModuleId::histogram, ScopeModuleId::levelOut
            };
            return makeGrid (3, 3, fill, 9);
        }},
        { "grid4", "4 x 4", [] {
            const ScopeModuleId fill[] = {
                ScopeModuleId::goniometer, ScopeModuleId::spectrum, ScopeModuleId::oscilloscope, ScopeModuleId::spectrogram,
                ScopeModuleId::levelIn, ScopeModuleId::loudness, ScopeModuleId::stereogram, ScopeModuleId::histogram,
                ScopeModuleId::levelOut, ScopeModuleId::thd, ScopeModuleId::spectrogram3D
            };
            return makeGrid (4, 4, fill, 11);
        }},
        { "top1bot2", "Wide top", [] {
            return makeSplit (Split::horizontal, 0.4f,
                              makeLeaf (ScopeModuleId::spectrum),
                              makeSplit (Split::vertical, 0.5f,
                                         makeLeaf (ScopeModuleId::oscilloscope),
                                         makeLeaf (ScopeModuleId::goniometer)));
        }},
        { "top2bot1", "Wide bottom", [] {
            return makeSplit (Split::horizontal, 0.6f,
                              makeSplit (Split::vertical, 0.5f,
                                         makeLeaf (ScopeModuleId::goniometer),
                                         makeLeaf (ScopeModuleId::spectrum)),
                              makeLeaf (ScopeModuleId::oscilloscope));
        }},
        { "left1right2", "Tall left", [] {
            return makeSplit (Split::vertical, 0.4f,
                              makeLeaf (ScopeModuleId::spectrum),
                              makeSplit (Split::horizontal, 0.5f,
                                         makeLeaf (ScopeModuleId::goniometer),
                                         makeLeaf (ScopeModuleId::oscilloscope)));
        }},
        { "grid2x3", "2 x 3", [] {
            const ScopeModuleId fill[] = {
                ScopeModuleId::goniometer, ScopeModuleId::spectrum, ScopeModuleId::oscilloscope,
                ScopeModuleId::spectrogram, ScopeModuleId::levelIn, ScopeModuleId::levelOut
            };
            return makeGrid (3, 2, fill, 6);
        }},
        { "grid3x2", "3 x 2", [] {
            const ScopeModuleId fill[] = {
                ScopeModuleId::goniometer, ScopeModuleId::spectrum,
                ScopeModuleId::oscilloscope, ScopeModuleId::spectrogram,
                ScopeModuleId::levelIn, ScopeModuleId::levelOut
            };
            return makeGrid (2, 3, fill, 6);
        }},
        { "row", "Row", [] {
            const ScopeModuleId fill[] = {
                ScopeModuleId::levelIn, ScopeModuleId::spectrogram, ScopeModuleId::spectrum,
                ScopeModuleId::goniometer, ScopeModuleId::loudness, ScopeModuleId::levelOut
            };
            return makeRow (fill, 6);
        }},
        { "image", "Image", [] {
            return grid2x2 (ScopeModuleId::stereogram, ScopeModuleId::goniometer,
                            ScopeModuleId::levelIn, ScopeModuleId::levelOut);
        }},
        { "punch", "Punch", [] {
            return grid2x2 (ScopeModuleId::oscilloscope, ScopeModuleId::histogram,
                            ScopeModuleId::spectrum, ScopeModuleId::levelOut);
        }},
    };
    return list;
}

inline std::unique_ptr<Node> defaultTree()
{
    return grid2x2 (ScopeModuleId::goniometer, ScopeModuleId::spectrum,
                    ScopeModuleId::oscilloscope, ScopeModuleId::spectrogram);
}

inline const Factory* findFactory (const juce::String& id)
{
    for (const auto& f : factories())
        if (f.id == id)
            return &f;
    return nullptr;
}

inline void paintSchematic (juce::Graphics& g, juce::Rectangle<float> bounds, const Node& n)
{
    if (bounds.getWidth() < 2.0f || bounds.getHeight() < 2.0f)
        return;

    if (n.isLeaf())
    {
        g.fillRoundedRectangle (bounds, 1.2f);
        return;
    }

    const float gap = 1.4f;
    if (n.split == Split::vertical)
    {
        const float w = juce::jmax (1.0f, bounds.getWidth() - gap);
        const float lw = juce::jmax (1.0f, w * n.frac);
        if (n.a != nullptr)
            paintSchematic (g, { bounds.getX(), bounds.getY(), lw, bounds.getHeight() }, *n.a);
        if (n.b != nullptr)
            paintSchematic (g, { bounds.getX() + lw + gap, bounds.getY(),
                                 juce::jmax (1.0f, bounds.getRight() - (bounds.getX() + lw + gap)),
                                 bounds.getHeight() }, *n.b);
    }
    else
    {
        const float h = juce::jmax (1.0f, bounds.getHeight() - gap);
        const float th = juce::jmax (1.0f, h * n.frac);
        if (n.a != nullptr)
            paintSchematic (g, { bounds.getX(), bounds.getY(), bounds.getWidth(), th }, *n.a);
        if (n.b != nullptr)
            paintSchematic (g, { bounds.getX(), bounds.getY() + th + gap, bounds.getWidth(),
                                 juce::jmax (1.0f, bounds.getBottom() - (bounds.getY() + th + gap)) }, *n.b);
    }
}

/** Promote a legacy flat module list + optional 2x2 splits into a tree. */
inline std::unique_ptr<Node> fromLegacy (const std::vector<ScopeModuleId>& modules,
                                         bool strip,
                                         float splitX, float splitY)
{
    juce::ignoreUnused (splitY);
    if (modules.empty())
        return defaultTree();

    if (strip)
        return makeRow (modules.data(), (int) modules.size());

    if (modules.size() == 4)
    {
        auto t = grid2x2 (modules[0], modules[1], modules[2], modules[3]);
        if (t != nullptr && t->split == Split::vertical)
        {
            t->frac = juce::jlimit (0.18f, 0.82f, splitX);
            if (t->a != nullptr)
                t->a->frac = juce::jlimit (0.18f, 0.82f, 0.5f);
            if (t->b != nullptr)
                t->b->frac = juce::jlimit (0.18f, 0.82f, 0.5f);
        }
        return t;
    }

    const int n = (int) modules.size();
    const int cols = juce::jmax (1, (int) std::ceil (std::sqrt ((float) n)));
    const int rows = (n + cols - 1) / cols;
    return makeGrid (cols, rows, modules.data(), n);
}
} // namespace ScopeViewport
