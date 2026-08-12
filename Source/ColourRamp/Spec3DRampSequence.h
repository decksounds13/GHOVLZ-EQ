#pragma once

#include "GradientRamp.h"
#include "RampPresetStore.h"
#include <vector>

/** Lerp two ramps by resampling to equal pole counts (Spec3D timeline morph). */
GradientRamp lerpRamps (const GradientRamp& a, const GradientRamp& b,
                        float t, int poles = 16);

// ── Keyframe interpolation ──────────────────────────────────────────────────
enum class Spec3DKeyInterp : int
{
    step = 0,
    linear,
    smooth,
    bezier
};

struct Spec3DFloatKey
{
    float timeSec = 0.0f;
    float value = 0.0f;
    Spec3DKeyInterp interp = Spec3DKeyInterp::linear;
    float inTangent = 0.0f;  // value units per second (bezier)
    float outTangent = 0.0f;
};

struct Spec3DFloatEnvelope
{
    float minV = 0.0f;
    float maxV = 1.0f;
    float defaultV = 0.5f;
    std::vector<Spec3DFloatKey> keys;

    void sortKeys() noexcept;
    float evaluate (float timeSec, float lengthSec) const noexcept;
    juce::ValueTree toValueTree (const juce::Identifier& type) const;
    static Spec3DFloatEnvelope fromValueTree (const juce::ValueTree& tree);
};

struct Spec3DColourKey
{
    float timeSec = 0.0f;
    juce::Colour colour { juce::Colours::white };
    Spec3DKeyInterp interp = Spec3DKeyInterp::linear;
    float inTangent = 0.0f;
    float outTangent = 0.0f;
};

struct Spec3DColourEnvelope
{
    juce::Colour defaultColour { juce::Colours::white };
    std::vector<Spec3DColourKey> keys;

    void sortKeys() noexcept;
    juce::Colour evaluate (float timeSec, float lengthSec) const noexcept;
    juce::ValueTree toValueTree (const juce::Identifier& type) const;
    static Spec3DColourEnvelope fromValueTree (const juce::ValueTree& tree);
};

// ── Ramp clips ──────────────────────────────────────────────────────────────
struct Spec3DRampClip
{
    juce::String displayName;   // UI label
    GradientRamp ramp;          // source of truth (persists without named preset)
    float weight = 1.0f;
    /** Outgoing X-fade into next clip (seconds). Keep in sync with kDefaultCrossfadeSec. */
    float crossfadeOutSec = 2.0f;
};

enum class Spec3DSeqLaneType : int
{
    colourRamp = 0, // always present as ramp clips on the sequence root
    lightAmount,
    lightAzimuth,
    lightElevation,
    lightColour,
    rimAmount,
    rimColour
};

struct Spec3DSeqLane
{
    Spec3DSeqLaneType type = Spec3DSeqLaneType::lightAmount;
    juce::String label;
    /** When false, lane is greyed out and skipped at evaluate (debug solo/mute). */
    bool enabled = true;
    Spec3DFloatEnvelope floatEnv;
    Spec3DColourEnvelope colourEnv;

    bool isColourLane() const noexcept
    {
        return type == Spec3DSeqLaneType::lightColour || type == Spec3DSeqLaneType::rimColour;
    }

    static juce::String defaultLabel (Spec3DSeqLaneType t);
    static Spec3DSeqLane makeDefault (Spec3DSeqLaneType t);
};

/**
    Spec3D sequencer document: ramp clips (≤5) + optional lighting automation lanes.
    Lightweight single-owner model used by menu strip, pop-out, and Spec3D timer.
*/
struct Spec3DRampSequence
{
    static constexpr float kMinLengthSec = 0.5f;
    static constexpr float kMaxLengthSec = 300.0f;
    static constexpr float kDefaultLengthSec = 10.0f;
    static constexpr float kDefaultCrossfadeSec = 2.0f;
    static constexpr int kMaxClips = 5;
    static constexpr float kMinWeight = 0.05f;

    bool enabled = false;
    /** Colour/ramp lane mute — independent of master Seq enable. */
    bool rampLaneEnabled = true;
    float lengthSec = kDefaultLengthSec;
    float defaultCrossfadeSec = kDefaultCrossfadeSec;
    std::vector<Spec3DRampClip> clips;
    std::vector<Spec3DSeqLane> autoLanes; // lighting etc. (Ramp is not in this list)

    void clamp() noexcept;
    bool canAddClip() const noexcept { return (int) clips.size() < kMaxClips; }
    bool hasLaneType (Spec3DSeqLaneType t) const noexcept;
    bool addLane (Spec3DSeqLaneType t);
    void removeLaneAt (int autoLaneIndex);

    juce::ValueTree toValueTree() const;
    static Spec3DRampSequence fromValueTree (const juce::ValueTree& tree);
    /** Replace all fields from a snapshot (undo / redo). */
    void applyValueTree (const juce::ValueTree& tree);
    /** Migrate + hydrate ramps from store when only preset names exist. */
    void hydrateFromStore (const RampPresetStore& store);

    struct LaidOutClip
    {
        int index = 0;
        float startSec = 0.0f;
        float endSec = 0.0f;
        float fadeOutSec = 0.0f;
    };

    void buildLayout (std::vector<LaidOutClip>& out) const;

    static bool resolvePreset (const RampPresetStore& store, const juce::String& name,
                               GradientRamp& dest);

    /** Morph colour ramp at time (uses embedded clip.ramp). */
    bool evaluate (float timeSec, GradientRamp& out) const;

    /** Apply float/colour auto lanes into live Spec3D lighting (caller supplies apply fns). */
    template <typename ApplyFloat, typename ApplyColour>
    void evaluateAutomation (float timeSec,
                             ApplyFloat&& applyFloat,
                             ApplyColour&& applyColour) const
    {
        if (! enabled)
            return;
        const float len = juce::jlimit (kMinLengthSec, kMaxLengthSec, lengthSec);
        for (const auto& lane : autoLanes)
        {
            if (! lane.enabled)
                continue;
            if (lane.isColourLane())
                applyColour (lane.type, lane.colourEnv.evaluate (timeSec, len));
            else
                applyFloat (lane.type, lane.floatEnv.evaluate (timeSec, len));
        }
    }
};
