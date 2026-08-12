#include "Spectrogram3DComponent.h"
#include "Spec3DParticleSystem.h"
#include <utility>
#include <cmath>
#include <cctype>
#include "SpectrogramComponent.h"
#include "Menu/SharedResources.h"
#include "ComboBoxLookAndFeel.h"
#include "ColourRamp/ColourRampBank.h"
#include "ColourRamp/Spec3DRampTimelineComponent.h"
#include "Assets/VeniceSunsetHdri.h"
#include <cmath>
#include <cstring>

namespace
{
    // #region agent log
    inline void agentDbgLog (const char* hypothesisId, const char* location,
                             const char* message, const juce::String& dataJson)
    {
        const juce::String line = juce::String ("{\"sessionId\":\"70daa9\",\"hypothesisId\":\"")
            + hypothesisId + "\",\"location\":\"" + location + "\",\"message\":\"" + message
            + "\",\"data\":" + dataJson + ",\"timestamp\":"
            + juce::String ((juce::int64) juce::Time::currentTimeMillis()) + "}\n";
        // Absolute path - DAW CWD is unreliable for relative logs.
        juce::File ("C:/Users/jerem/Desktop/DecksoundsParametricEq/ParametricEqProject/debug-70daa9.log")
            .appendText (line, false, false);
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
            .getChildFile ("debug-70daa9.log")
            .appendText (line, false, false);
    }
    // #endregion

    static int gLabelDrawCallsThisFrame = 0;
    static int gSoftFrameCounter = 0;

    constexpr const char* kColourVertexShader = R"(
        #version 150
        in vec3 position;
        in vec3 colour;
        in vec3 normal;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        out vec3 vColour;
        out vec3 vNormal;
        out vec3 vViewDir;
        out vec3 vWorldPos;
        out vec3 vWorldNormal;
        out float vViewDepth;
        void main()
        {
            vColour = colour;
            vNormal = mat3 (viewMatrix) * normal;
            vWorldNormal = normal;
            vec4 viewPos = viewMatrix * vec4 (position, 1.0);
            vViewDir = normalize (-viewPos.xyz);
            vWorldPos = position;
            vViewDepth = max (-viewPos.z, 0.0);
            gl_Position = projectionMatrix * viewPos;
        }
    )";

    constexpr const char* kColourFragmentShader = R"(
        #version 150
        in vec3 vColour;
        in vec3 vNormal;
        in vec3 vViewDir;
        in vec3 vWorldPos;
        in vec3 vWorldNormal;
        in float vViewDepth;
        out vec4 fragColour;
        uniform vec2 uResolution;
        uniform float uCornerRadius;
        uniform vec4 uClearColour;
        uniform vec3 uLightDirView;
        uniform float uLightingAmount;
        uniform float uSpecular;
        uniform float uRoughness;
        uniform float uMetalness;
        uniform float uEnergyConserve; // 0=off (legacy), 1=diffuse*(1-F)
        uniform float uRim;
        uniform vec3 uLightColour;
        uniform vec3 uRimColour;
        uniform float uDomeStrength;
        uniform vec3 uDomeSky;
        uniform vec3 uDomeGround;
        uniform float uDomeUseTex; // 0=sky/ground colours, 1=equirect map
        uniform sampler2D uDomeMap;
        uniform sampler2D uHeightMap;
        uniform vec3 uLightDirWorld;
        uniform float uSelfShadow;
        uniform float uMeshHeight;
        uniform float uReverseFreq;
        uniform float uFreqBiasB; // HF density amount; 0 = uniform
        uniform float uFreqBiasPivot; // freq-axis u where density boost begins
        uniform float uAoAmount;
        uniform float uAoRadius;
        // CPU-prepared light bearing on the floor (avoids GPU normalize edge cases).
        uniform vec2 uShadowDirXZ;
        uniform float uShadowSunTan;
        uniform float uShadowBias;
        uniform float uShadowSoftness;
        uniform float uShadowQuality; // 0=low, 1=medium, 2=high
        uniform float uContactShadow;
        // Directional cast-shadow atlas (CSM: fixed far distance, N cascade tiles).
        uniform sampler2D uShadowAtlas;
        uniform float uCastShadow;       // 0=off, 1=on
        uniform mat4 uShadowMatrix0;
        uniform mat4 uShadowMatrix1;
        uniform mat4 uShadowMatrix2;
        uniform mat4 uShadowMatrix3;
        uniform vec4 uCascadeSplits;    // far view-Z per cascade
        uniform float uCascadeCount;
        uniform float uShadowAtlasTiles; // horizontal tiles (= cascade count)
        uniform float uCastBias;
        uniform float uCastSoftness;
        uniform float uCascadeTransition; // blend width as fraction of cascade range
        uniform float uViewZScale;      // unused reserve for cascade select
        // Material override for debug sphere (0 = vertex colour / mesh rough/metal/spec).
        uniform float uMatOverride;
        uniform vec3 uMatAlbedo;
        uniform float uMatRoughness;
        uniform float uMatMetalness;
        uniform float uMatSpecular;
        // Gizmo x-ray ghost pass: draw only fragments inside the debug sphere.
        uniform float uGizmoXray; // 0=off, 1=inside-only ghost
        uniform vec3 uXrayCenter;
        uniform float uXrayRadius;
        uniform float uXrayAlpha;
        // SSS: 0=off, 1=open heightfield taps, 2=closed volume (height-to-base + taps)
        uniform float uSssMode;
        uniform float uSssStrength;
        uniform float uSssWrap;
        uniform float uSssTransmission;
        uniform vec3 uSssTint;
        uniform float uSssRadius;
        uniform float uSssContrast;
        uniform float uSssQuality; // 0=low, 1=medium, 2=high
        uniform float uClosedFloorY; // closed mesh bottom (just under 0-intensity plane)
        uniform float uSssThickScale;
        uniform float uSssMaxThick;
        // Audio-level mod matrix (CPU sets which channels; flags are 0/1).
        // factor = mix(1+min, 1+max, level); min/max are fractional (+/-1 = +/-100%).
        uniform float uAudioLevel;
        uniform float uAudioMin;
        uniform float uAudioMax;
        uniform float uAudioModBright; // ramp brightness only - never lighting amount
        uniform float uAudioModLitAmt;
        uniform float uAudioModSpec;
        uniform float uAudioModRim;
        uniform float uAudioModDome;
        uniform float uAudioAffectPlayhead;
        uniform float uAudioAffectAnti;
        uniform float uPlayheadWallX;
        uniform float uAntiPlayheadWallX;

        // Frequency axis u (0=low...1=high) -> height-map V (mesh-row CDF).
        // Boost only above pivot: w=1 for u<=P, else 1+B*((u-P)/(1-P))^2.
        float meshTFromFreqAxis (float uAxis)
        {
            float u = clamp (uAxis, 0.0, 1.0);
            float B = max (uFreqBiasB, 0.0);
            if (B < 1.0e-5)
                return u;
            float P = clamp (uFreqBiasPivot, 0.0, 0.999);
            float I = 1.0 + B * (1.0 - P) / 3.0;
            if (u <= P)
                return u / I;
            float v = u - P;
            float omp = max (1.0 - P, 1.0e-5);
            return (u + B * v * v * v / (3.0 * omp * omp)) / I;
        }

        float sampleHeightNorm (vec2 xz)
        {
            float texU = clamp (xz.x * 0.5 + 0.5, 0.001, 0.999);
            // World Z -> frequency axis u (0=low, 1=high), matching CPU mesh build.
            float uAxis = (uReverseFreq > 0.5)
                            ? clamp (xz.y * 0.5 + 0.5, 0.0, 1.0)
                            : clamp ((1.0 - xz.y) * 0.5, 0.0, 1.0);
            // Non-uniform mesh rows: height map is indexed by mesh-row t = CDF(u).
            float texV = clamp (meshTFromFreqAxis (uAxis), 0.001, 0.999);
            return texture (uHeightMap, vec2 (texU, texV)).r;
        }

        float sampleHeight (vec2 xz)
        {
            return sampleHeightNorm (xz) * uMeshHeight;
        }

        /**
            Heightfield self-shadow - horizon + IQ soft ray-march.
            Bias fights acne; Softness widens the terminator / penumbra;
            Quality sets sample density.
        */
        float heightfieldSelfShadow (vec3 pos)
        {
            float strength = clamp (uSelfShadow, 0.0, 2.0);
            if (strength <= 0.0 || uMeshHeight <= 1.0e-5)
                return 1.0;

            vec2 dirXZ = uShadowDirXZ;
            float dirLen = length (dirXZ);
            if (dirLen < 1.0e-4)
            {
                dirXZ = uLightDirWorld.xz;
                dirLen = length (dirXZ);
                if (dirLen < 1.0e-4)
                    dirXZ = vec2 (1.0, 0.0);
                else
                    dirXZ /= dirLen;
            }

            float sunTan = clamp (uShadowSunTan, 0.05, 0.85);
            float bias01 = clamp (uShadowBias, 0.0, 1.0);
            float soft01 = clamp (uShadowSoftness, 0.0, 1.0);
            float heightBias = mix (0.002, 0.05, bias01) * uMeshHeight;
            // Extra-wide smoothstep = soft terminator / penumbra (UE lookdev Softness).
            float termLo = mix (0.08, 0.55, soft01);
            float termHi = mix (0.30, 1.80, soft01);
            float penumbraK = mix (12.0, 1.8, soft01); // lower = softer falloff

            int q = int (clamp (uShadowQuality + 0.5, 0.0, 2.0));
            int horizonSteps = (q == 0) ? 10 : ((q == 1) ? 18 : 28);
            int raySteps = (q == 0) ? 14 : ((q == 1) ? 24 : 36);
            float stepScale = (q == 0) ? 0.06 : ((q == 1) ? 0.045 : 0.032);

            float h0 = sampleHeight (pos.xz);

            float horizonOcc = 0.0;
            for (int i = 1; i <= 36; ++i)
            {
                if (i > horizonSteps)
                    break;
                float dist = float (i) * stepScale;
                vec2 xz = pos.xz + dirXZ * dist;
                if (abs (xz.x) > 1.05 || abs (xz.y) > 1.05)
                    break;
                float h = sampleHeight (xz);
                float horizonTan = (h - h0 - heightBias) / max (dist, 1.0e-4);
                horizonOcc = max (horizonOcc,
                                  smoothstep (sunTan - termLo, sunTan + termHi, horizonTan));
            }

            vec3 rd = normalize (uLightDirWorld);
            if (dot (rd, rd) < 1.0e-6)
                rd = normalize (vec3 (dirXZ.x, sunTan, dirXZ.y));

            float lit = 1.0;
            float t = mix (0.012, 0.045, bias01);
            vec3 ro = vec3 (pos.x, h0 + heightBias, pos.z);
            for (int i = 0; i < 40; ++i)
            {
                if (i >= raySteps)
                    break;
                vec3 p = ro + rd * t;
                if (abs (p.x) > 1.08 || abs (p.z) > 1.08 || p.y > uMeshHeight * 1.35)
                    break;
                float h = p.y - sampleHeight (p.xz);
                // Soft contact - no hard lit=0 cliff at the terminator.
                float softHit = smoothstep (-heightBias * mix (0.15, 0.8, soft01),
                                            heightBias * mix (0.8, 2.4, soft01),
                                            h);
                lit = min (lit, softHit * clamp (penumbraK * h / max (t, 1.0e-4), 0.0, 1.0)
                                     + (1.0 - softHit) * mix (0.0, 0.35, soft01));
                if (h < -heightBias * mix (0.5, 1.5, soft01))
                    break;
                t += clamp (abs (h), stepScale * 0.45, stepScale * 2.4);
            }
            lit = clamp (lit, 0.0, 1.0);

            float occ = max (horizonOcc, 1.0 - lit);
            // Ease occlusion so the terminator ramps instead of clipping.
            occ = smoothstep (0.0, mix (0.65, 1.45, soft01), occ);
            occ = pow (occ, mix (1.0, 0.65, soft01));
            return clamp (1.0 - occ * strength, 0.0, 1.0);
        }

)"
        R"(        mat4 shadowMatrixForCascade (int c)
        {
            if (c <= 0) return uShadowMatrix0;
            if (c == 1) return uShadowMatrix1;
            if (c == 2) return uShadowMatrix2;
            return uShadowMatrix3;
        }

        // PCF sample one cascade tile (returns 1 = lit).
        float sampleCastShadowCascade (vec3 worldPos, int cascade)
        {
            mat4 sm = shadowMatrixForCascade (cascade);
            vec4 lp = sm * vec4 (worldPos, 1.0);
            if (abs (lp.w) < 1.0e-6)
                return 1.0;
            vec3 ndc = lp.xyz / lp.w;
            vec2 uv = ndc.xy * 0.5 + 0.5;
            float depth = ndc.z * 0.5 + 0.5;
            if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                return 1.0;

            float tiles = max (uShadowAtlasTiles, 1.0);
            float tileW = 1.0 / tiles;
            uv.x = clamp (uv.x, 0.001, 0.999) * tileW + float (cascade) * tileW;
            uv.y = clamp (uv.y, 0.001, 0.999);

            float bias = mix (0.0008, 0.008, clamp (uCastBias, 0.0, 1.0));
            float soft = mix (0.4, 2.2, clamp (uCastSoftness, 0.0, 1.0));
            vec2 texel = vec2 (tileW / 512.0, 1.0 / 512.0) * soft;

            float lit = 0.0;
            for (int y = -1; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                {
                    float d = texture (uShadowAtlas, uv + vec2 (float (x), float (y)) * texel).r;
                    lit += (depth - bias <= d + 1.0e-5) ? 1.0 : 0.0;
                }
            return lit / 9.0;
        }

        // True cast shadows from the directional light-depth atlas (PCF + cascade blend).
        float sampleCastShadow (vec3 worldPos, float viewDepth)
        {
            if (uCastShadow < 0.5 || uShadowAtlasTiles < 0.5)
                return 1.0;

            int nCasc = int (clamp (uCascadeCount + 0.5, 1.0, 4.0));
            int cascade = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (i + 1 >= nCasc)
                    break;
                if (viewDepth > uCascadeSplits[i])
                    cascade = i + 1;
            }

            float shadow = sampleCastShadowCascade (worldPos, cascade);

            // Soften the hard split line (screen-stable diagonal artifact).
            float trans = clamp (uCascadeTransition, 0.0, 0.45);
            if (trans > 1.0e-4 && cascade + 1 < nCasc)
            {
                float splitFar = uCascadeSplits[cascade];
                float splitNear = (cascade == 0) ? 0.15 : uCascadeSplits[cascade - 1];
                float width = max ((splitFar - splitNear) * trans, 0.04);
                float edge0 = splitFar - width;
                if (viewDepth > edge0)
                {
                    float t = smoothstep (edge0, splitFar, viewDepth);
                    float shadowNext = sampleCastShadowCascade (worldPos, cascade + 1);
                    shadow = mix (shadow, shadowNext, t);
                }
            }
            return shadow;
        }

        // Crevice / horizon AO from the heightfield (reliable; no depth buffer).
        float heightfieldAO (vec3 pos)
        {
            float amount = clamp (uAoAmount, 0.0, 2.0);
            if (amount < 1.0e-4 || uMeshHeight < 1.0e-4)
                return 1.0;

            float rad = clamp (uAoRadius, 0.25, 3.0) * 0.08;
            float centre = sampleHeight (pos.xz);
            float occ = 0.0;
            const int kTaps = 8;
            vec2 offs[8] = vec2[] (
                vec2 ( 1.0,  0.0), vec2 (-1.0,  0.0),
                vec2 ( 0.0,  1.0), vec2 ( 0.0, -1.0),
                vec2 ( 0.707,  0.707), vec2 (-0.707,  0.707),
                vec2 ( 0.707, -0.707), vec2 (-0.707, -0.707)
            );
            for (int i = 0; i < kTaps; ++i)
            {
                float h1 = sampleHeight (pos.xz + offs[i] * rad);
                float h2 = sampleHeight (pos.xz + offs[i] * rad * 2.0);
                float rise1 = (h1 - centre) / max (uMeshHeight, 1.0e-3);
                float rise2 = (h2 - centre) / max (uMeshHeight, 1.0e-3);
                occ += clamp (rise1 * 1.6, 0.0, 1.0) * 0.65;
                occ += clamp (rise2 * 1.1, 0.0, 1.0) * 0.35;
            }
            occ = clamp (occ / float (kTaps), 0.0, 1.0);
            return clamp (1.0 - occ * amount, 0.0, 1.0);
        }

        // GGX / Smith / Schlick specular lobe.
        float distributionGGX (float NdotH, float rough)
        {
            float a = max (rough * rough, 0.0025);
            float a2 = a * a;
            float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
            return a2 / (3.14159265 * d * d);
        }

        float geometrySchlickGGX (float NdotX, float rough)
        {
            float r = rough + 1.0;
            float k = (r * r) / 8.0;
            return NdotX / (NdotX * (1.0 - k) + k);
        }

        vec3 fresnelSchlick (float cosTheta, vec3 F0)
        {
            return F0 + (1.0 - F0) * pow (clamp (1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        /** Fake thickness: thin ridges along light on the heightfield. */
        float heightfieldThickness (vec3 pos)
        {
            float rad = mix (0.02, 0.22, clamp (uSssRadius, 0.0, 1.0));
            float contrast = mix (0.35, 2.5, clamp (uSssContrast, 0.0, 1.0));
            int q = int (clamp (uSssQuality + 0.5, 0.0, 2.0));
            int steps = (q == 0) ? 4 : ((q == 1) ? 6 : 8);

            vec2 dirXZ = uShadowDirXZ;
            float dirLen = length (dirXZ);
            if (dirLen < 1.0e-4)
            {
                dirXZ = uLightDirWorld.xz;
                dirLen = length (dirXZ);
                if (dirLen < 1.0e-4)
                    dirXZ = vec2 (1.0, 0.0);
                else
                    dirXZ /= dirLen;
            }

            float h0 = sampleHeight (pos.xz);
            float thin = 0.0;
            for (int i = 1; i <= 8; ++i)
            {
                if (i > steps)
                    break;
                float dist = float (i) * rad / float (steps);
                float h = sampleHeight (pos.xz - dirXZ * dist);
                thin += max (0.0, h0 - h);
            }
            thin /= max (uMeshHeight * float (steps) * 0.35, 1.0e-4);
            return clamp (pow (clamp (thin, 0.0, 1.0), mix (1.4, 0.55, clamp (uSssContrast, 0.0, 1.0)))
                          * contrast, 0.0, 1.0);
        }

        /** Closed solid: optical depth top->base + ridge taps. */
        float closedThickness (vec3 pos)
        {
            float h0 = sampleHeight (pos.xz);
            float optical = max (0.0, h0 - uClosedFloorY);
            float maxT = mix (0.15, 1.35, clamp (uSssMaxThick, 0.0, 1.0)) * uMeshHeight;
            float scale = mix (0.35, 2.2, clamp (uSssThickScale, 0.0, 1.0));
            // Thin where the solid is short (peaks / edges).
            float volumeThin = 1.0 - smoothstep (0.0, maxT, optical * scale);
            float ridge = heightfieldThickness (pos);
            return clamp (volumeThin * 0.65 + ridge * 0.55, 0.0, 1.0);
        }

        vec3 subsurfaceScatter (vec3 albedo, vec3 n, vec3 l, vec3 v, float shadow)
        {
            int mode = int (clamp (uSssMode + 0.5, 0.0, 2.0));
            float strength = clamp (uSssStrength, 0.0, 1.0);
            if (mode <= 0 || strength < 1.0e-4)
                return vec3 (0.0);

            // Walls / underside: skip SSS (top surface only).
            if (n.y < 0.25)
                return vec3 (0.0);

            float wrapAmt = mix (0.15, 0.95, clamp (uSssWrap, 0.0, 1.0));
            float NdotL = dot (n, l);
            float scatterWrap = clamp ((NdotL + wrapAmt) / (1.0 + wrapAmt), 0.0, 1.0);

            float NdotV = max (dot (n, v), 1.0e-4);
            float transAmt = clamp (uSssTransmission, 0.0, 1.0);
            float back = pow (1.0 - NdotV, mix (1.2, 3.5, transAmt));

            float thick = (mode >= 2) ? closedThickness (vWorldPos) : heightfieldThickness (vWorldPos);
            float softSh = mix (0.35, 1.0, shadow); // penumbra still allows SSS

            vec3 tint = max (uSssTint, vec3 (0.0));
            float kFront = 0.55;
            float kBack = 0.85 * transAmt;
            return albedo * tint * scatterWrap
                 * (kFront + kBack * back * thick)
                 * strength * softSh;
        }

        void main()
        {
            vec2 halfSize = uResolution * 0.5;
            vec2 p = gl_FragCoord.xy - halfSize;
            vec2 q = abs(p) - halfSize + vec2(uCornerRadius);
            float d = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - uCornerRadius;
            if (d > 0.0)
            {
                fragColour = uClearColour;
                return;
            }

            vec3 albedo = (uMatOverride > 0.5) ? uMatAlbedo : vColour;
            float castSh = sampleCastShadow (vWorldPos, vViewDepth);
            float shadow = heightfieldSelfShadow (vWorldPos) * castSh;
            float ao = heightfieldAO (vWorldPos);

            // Contact: the mesh covers the whole floor, so a ground disc never shows.
            // Approximate as base darkening from surrounding taller terrain.
            float contact = 1.0;
            if (uContactShadow > 0.0)
            {
                float h0 = sampleHeight (vWorldPos.xz);
                float surround = 0.0;
                vec2 cOffs[4] = vec2[] (vec2(1,0), vec2(-1,0), vec2(0,1), vec2(0,-1));
                for (int i = 0; i < 4; ++i)
                {
                    float hn = sampleHeight (vWorldPos.xz + cOffs[i] * 0.08);
                    surround = max (surround, clamp ((hn - h0) / max (uMeshHeight, 1e-3), 0.0, 1.0));
                }
                float nestle = smoothstep (0.22, 0.0, h0 / max (uMeshHeight, 1e-3));
                contact = clamp (1.0 - surround * nestle * uContactShadow, 0.15, 1.0);
            }

)"
        R"(            // Self/contact shadows are key-light effects - when lighting is off they are
            // forced to 1.0 via uniforms; AO may still apply (ambient, own toggle).
            float shade = shadow * ao * contact;

            // Audio-level mod matrix: body always; closed playhead / anti-playhead opt-in.
            // rawFactor = mix(1+min%, 1+max%, level); both % at 0 -> 1 (no change).
            // Excluded walls keep factor=1 (base look) - NOT the silence endpoint.
            float factor = 1.0;
            {
                float lvl = clamp (uAudioLevel, 0.0, 1.0);
                float band = 0.05;
                float nearPh = 1.0 - smoothstep (0.0, band, abs (vWorldPos.x - uPlayheadWallX));
                float nearAnti = 1.0 - smoothstep (0.0, band, abs (vWorldPos.x - uAntiPlayheadWallX));
                float onBody = 1.0 - max (nearPh, nearAnti);
                float affect = onBody;
                if (uAudioAffectPlayhead > 0.5)
                    affect = max (affect, nearPh);
                if (uAudioAffectAnti > 0.5)
                    affect = max (affect, nearAnti);
                float rawFactor = mix (1.0 + uAudioMin, 1.0 + uAudioMax, lvl);
                factor = mix (1.0, rawFactor, clamp (affect, 0.0, 1.0));
            }

            // Early-out only when Lighting master is off.
            float baseAmt = clamp (uLightingAmount, 0.0, 1.0);

            // Ramp brightness: pulse colours only. Never touch lighting amount.
            // Soft floor while lit so At Silence −100% doesn't erase the lit form.
            if (uAudioModBright > 0.5)
            {
                float g = max (factor, 0.0);
                if (baseAmt > 1.0e-4)
                    g = max (g, 0.12);
                albedo *= g;
            }

            float amt = baseAmt;
            if (uAudioModLitAmt > 0.5)
                amt = clamp (baseAmt * factor, 0.0, 1.0);

            if (baseAmt < 1.0e-4)
            {
                // Flat / unlit (gizmo, grid). Optional gizmo x-ray: only fragments
                // inside the sphere, at reduced alpha - sphere itself stays opaque.
                float a = 1.0;
                if (uGizmoXray > 0.5)
                {
                    float d = length (vWorldPos - uXrayCenter);
                    float edge = max (0.004, uXrayRadius * 0.02);
                    float inside = 1.0 - smoothstep (uXrayRadius - edge, uXrayRadius + edge * 0.35, d);
                    if (inside < 0.02)
                        discard;
                    a = clamp (uXrayAlpha, 0.0, 1.0) * inside;
                }
                fragColour = vec4 (albedo * ao, a);
                return;
            }

            vec3 n = normalize (vNormal);
            vec3 l = normalize (uLightDirView);
            vec3 v = normalize (vViewDir);
            vec3 h = normalize (l + v);
            float NdotL = max (dot (n, l), 0.0);
            float NdotV = max (dot (n, v), 1.0e-4);
            float NdotH = max (dot (n, h), 0.0);
            float VdotH = max (dot (v, h), 0.0);

            float rough = clamp ((uMatOverride > 0.5) ? uMatRoughness : uRoughness, 0.04, 1.0);
            float metal = clamp ((uMatOverride > 0.5) ? uMatMetalness : uMetalness, 0.0, 1.0);
            float specAmt = clamp ((uMatOverride > 0.5) ? uMatSpecular : uSpecular, 0.0, 1.0);
            if (uMatOverride < 0.5 && uAudioModSpec > 0.5)
                specAmt = clamp (specAmt * factor, 0.0, 2.0);
            vec3 lightCol = max (uLightColour, vec3 (0.0));
            vec3 rimCol = max (uRimColour, vec3 (0.0));
            // Standard metalness workflow: metals tint F0 with albedo and kill diffuse.
            vec3 F0 = mix (vec3 (0.04), albedo, metal);
            float D = distributionGGX (NdotH, rough);
            float G = geometrySchlickGGX (NdotL, rough) * geometrySchlickGGX (NdotV, rough);
            vec3 F = fresnelSchlick (VdotH, F0);
            vec3 specular = (D * G * F) / max (4.0 * NdotV * max (NdotL, 1.0e-4), 1.0e-4);
            specular *= specAmt * shadow * lightCol;

            // Soft Lambert wrap on the mesh; lookdev sphere uses hard N|L so rough/spec read clearly.
            float wrap = (uMatOverride > 0.5) ? NdotL : (NdotL * 0.72 + 0.28);
            vec3 kd = albedo * (1.0 - metal);
            // Opt-in energy split - off preserves the legacy Look.
            if (uEnergyConserve > 0.5)
                kd *= max (vec3 (0.0), vec3 (1.0) - F);
            vec3 diffuse = kd * (0.22 * ao + 0.78 * wrap * shadow) * lightCol;

            // Dome / hemisphere fill - sky vs ground, or equirectangular HDRI.
            // Skipped for lookdev sphere so rough/specular aren't washed out by fill.
            float domeAmt = (uMatOverride > 0.5) ? 0.0 : clamp (uDomeStrength, 0.0, 1.0);
            if (uAudioModDome > 0.5)
                domeAmt = clamp (domeAmt * factor, 0.0, 2.0);
            if (domeAmt > 1.0e-4)
            {
                vec3 domeIrr;
                if (uDomeUseTex > 0.5)
                {
                    vec3 dn = normalize (vWorldNormal);
                    // Poly Haven-style equirect: +Y up, v=0 at zenith.
                    float u = atan (dn.x, dn.z) * 0.15915494309 + 0.5; // 1/(2π)
                    float v = 0.5 - asin (clamp (dn.y, -1.0, 1.0)) * 0.31830988618; // 1/π
                    domeIrr = max (texture (uDomeMap, vec2 (u, v)).rgb, vec3 (0.0));
                }
                else
                {
                    float hemi = clamp (n.y * 0.5 + 0.5, 0.0, 1.0);
                    domeIrr = mix (max (uDomeGround, vec3 (0.0)),
                                   max (uDomeSky, vec3 (0.0)), hemi);
                }
                // Stronger in shadow so occluded valleys pick up interdiffuse-like fill.
                float fillW = mix (1.0, 0.35, wrap * shadow);
                diffuse += kd * domeIrr * ao * domeAmt * fillW;
            }

            float rimAmt = (uMatOverride > 0.5) ? 0.0 : uRim;
            if (uMatOverride < 0.5 && uAudioModRim > 0.5)
                rimAmt = clamp (rimAmt * factor, 0.0, 2.0);
            float rim = pow (1.0 - NdotV, 2.5) * rimAmt * ao;
            vec3 sss = (uMatOverride > 0.5) ? vec3 (0.0)
                                            : subsurfaceScatter (albedo, n, l, v, shadow);
            vec3 lit = diffuse + specular + albedo * rim * rimCol + sss;
            fragColour = vec4 (mix (albedo * shade, lit, amt), 1.0);
        }
    )";

    constexpr const char* kShadowDepthVertexShader = R"(
        #version 150
        in vec3 position;
        uniform mat4 uLightVP;
        void main()
        {
            gl_Position = uLightVP * vec4 (position, 1.0);
        }
    )";

    constexpr const char* kShadowDepthFragmentShader = R"(
        #version 150
        void main()
        {
        }
    )";

    constexpr const char* kContactVertexShader = R"(
        #version 150
        in vec3 position;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        out vec2 vXZ;
        void main()
        {
            vXZ = position.xz;
            gl_Position = projectionMatrix * viewMatrix * vec4 (position, 1.0);
        }
    )";

    constexpr const char* kContactFragmentShader = R"(
        #version 150
        in vec2 vXZ;
        out vec4 fragColour;
        uniform float uStrength;
        void main()
        {
            float r = length (vXZ);
            // Soft elliptical contact stain - visible over Soft BG / EQ plate.
            float a = (1.0 - smoothstep (0.15, 1.25, r));
            a *= a;
            a *= clamp (uStrength, 0.0, 1.0) * 0.85;
            fragColour = vec4 (0.0, 0.0, 0.0, a);
        }
    )";

    constexpr const char* kNormalsVertexShader = R"(
        #version 150
        in vec3 position;
        in vec3 normal;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        out vec3 vNormal;
        void main()
        {
            vNormal = mat3 (viewMatrix) * normal;
            gl_Position = projectionMatrix * viewMatrix * vec4 (position, 1.0);
        }
    )";

    constexpr const char* kNormalsFragmentShader = R"(
        #version 150
        in vec3 vNormal;
        out vec4 fragColour;
        void main()
        {
            vec3 n = normalize (vNormal);
            fragColour = vec4 (n * 0.5 + 0.5, 1.0);
        }
    )";

    constexpr const char* kPostVertexShader = R"(
        #version 150
        in vec2 position;
        out vec2 vUv;
        void main()
        {
            vUv = position * 0.5 + 0.5;
            gl_Position = vec4 (position, 0.0, 1.0);
        }
    )";

    // mode: 0 = copy, 1 = SSAO, 2 = bloom extract, 3 = blur H, 4 = blur V,
    //        5 = bloom composite, 6 = DOF disc gather, 7 = SSR (screen-space reflections),
    //        8 = SSGI gather+composite (legacy), 9 = SSGI gather GI-only,
    //        10 = SSGI bilateral denoise (Simple), 11 = SSGI temporal (Simple),
    //        12 = SSGI composite, 13 = tonemap/grade, 14 = SSGI bilateral upsample,
    //        15 = SVGF temporal + variance clamp (Modern), 16 = à-trous (Modern),
    //        17 = DOF CoC write (mesh only; sky = 0), 18 = luminance moments (Modern),
    //        19 = DOF CoC dilate (spread mesh CoC into soft-BG void),
    //        20 = camera motion blur (depth reproject -> screen velocity -> gather).
    // Mode 6 uParam = DOF edge spill; mode 19 uParam = CoC dilate.
    // Mode 20: uStrength = shutter amount, uRadius = max blur px, uThreshold = quality.
    // Vendor denoisers (NVIDIA NRD/OptiX, Intel OIDN) are intentionally not used:
    // they need D3D11/12, Vulkan, and/or CUDA/SYCL - incompatible with this JUCE
    // OpenGL soft FBO->Image Spec3D path without a full API rewrite.
    constexpr const char* kPostFragmentShader = R"(
        #version 150
        in vec2 vUv;
        out vec4 fragColour;
        uniform sampler2D uTex;
        uniform sampler2D uDepth;
        uniform sampler2D uAux;
        uniform int uMode;
        uniform float uStrength;
        uniform float uRadius;
        uniform float uThreshold;
        uniform float uParam;
        uniform vec2 uResolution;
        uniform mat4 uInvProj;
        // Mode 20 motion blur: inv(current view), previous (proj * view).
        uniform mat4 uMotionInvView;
        uniform mat4 uMotionPrevVP;

        float depthSample (vec2 uv)
        {
            return texture (uDepth, uv).r;
        }

        // Matches GlHost::getProjectionMatrix near/far.
        float linearViewZ (float depth01)
        {
            const float nearP = 0.05;
            const float farP = 80.0;
            if (depth01 >= 0.9995)
                return farP;
            float zNdc = depth01 * 2.0 - 1.0;
            return (2.0 * nearP * farP) / (farP + nearP - zNdc * (farP - nearP));
        }

        // Thin-lens CoC (Marmoset/Substance-style). uRadius = focus (view Z),
        // uStrength = lens power (fMm/35)^2 / (fStop/5.6) from CPU,
        // uThreshold = quality (0/1/2 -> base max blur px + sample count).
        // Cleared / soft-BG depth is absence - always CoC 0.
        // Soft silhouettes: dilate mesh CoC into the void (modes 17->19->6).
        float circleOfConfusionPx (float depth01)
        {
            if (depth01 >= 0.9995)
                return 0.0;
            float focus = max (uRadius, 0.05);
            float power = clamp (uStrength, 0.0, 16.0);
            float viewZ = max (linearViewZ (depth01), 0.05);
            // Diopter difference - zero on the focus plane; F-Stop/focal length scale power.
            float diopter = abs (1.0 / viewZ - 1.0 / focus);
            float baseBlur = (uThreshold < 0.5) ? 6.0
                           : ((uThreshold < 1.5) ? 10.0 : 14.0);
            // Scene scale: power=1 (35mm @ f/5.6) ~ former aperture ~0.35 look.
            float coc = power * diopter * baseBlur * 12.0;
            float maxBlur = baseBlur * mix (1.0, 6.0, clamp (power * 0.35, 0.0, 1.0));
            return clamp (coc, 0.0, maxBlur);
        }

        // Vogel disc (golden-angle) - even circular bokeh taps.
        vec2 vogelDisk (int i, int n)
        {
            float r = sqrt ((float (i) + 0.5) / float (n));
            float theta = float (i) * 2.399963229728653;
            return vec2 (cos (theta), sin (theta)) * r;
        }

        // View-space position from depth (matches GlHost frustum).
        vec3 viewPosFromDepthUv (vec2 uv, float tanHalfW, float tanHalfH)
        {
            float z = linearViewZ (depthSample (uv));
            vec2 n = uv * 2.0 - 1.0;
            return vec3 (n.x * tanHalfW * z, n.y * tanHalfH * z, -z);
        }

        void main()
        {
            vec4 src = texture (uTex, vUv);
            if (uMode == 0)
            {
                fragColour = src;
                return;
            }
            if (uMode == 2)
            {
                float lum = dot (src.rgb, vec3 (0.299, 0.587, 0.114));
                float m = smoothstep (uThreshold, uThreshold + 0.25, lum);
                fragColour = vec4 (src.rgb * m, 1.0);
                return;
            }
            if (uMode == 3 || uMode == 4)
            {
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                vec2 dir = (uMode == 3) ? vec2 (texel.x, 0.0) : vec2 (0.0, texel.y);
                vec3 c = texture (uTex, vUv).rgb * 0.227027;
                c += texture (uTex, vUv + dir * 1.384615).rgb * 0.316216;
                c += texture (uTex, vUv - dir * 1.384615).rgb * 0.316216;
                c += texture (uTex, vUv + dir * 3.230769).rgb * 0.070270;
                c += texture (uTex, vUv - dir * 3.230769).rgb * 0.070270;
                fragColour = vec4 (c, 1.0);
                return;
            }
            if (uMode == 5)
            {
                vec3 bloom = texture (uDepth, vUv).rgb; // bloom buffer bound as uDepth slot
                fragColour = vec4 (src.rgb + bloom * uStrength, src.a);
                return;
            }
)"
        R"(            if (uMode == 6)
            {
                // Gather DOF - disc samples weighted by neighbour CoC.
                // Soft BG: sky CoC is 0; uAux holds mesh CoC dilated into the void (17->19).
                // Dilated CoC applies to SKY only - never inflate in-focus mesh/labels.
                //
                // Edge Dilate (mode 19) = how far CoC spreads into the void (radius only).
                // Edge Spill (uParam) = how strongly mesh colour bleeds onto sky.
                // Do not amplify both: spill weights stay ≤ 1 (no spillBoost).
                float baseBlur = (uThreshold < 0.5) ? 6.0
                               : ((uThreshold < 1.5) ? 10.0 : 14.0);
                float power = clamp (uStrength, 0.0, 16.0);
                float maxBlur = baseBlur * mix (1.0, 6.0, clamp (power * 0.35, 0.0, 1.0));
                float centerDepth = depthSample (vUv);
                bool centerSky = (centerDepth >= 0.9995);
                float centerCoc = circleOfConfusionPx (centerDepth);
                float dilatedCoc = texture (uAux, vUv).r * maxBlur;
                // Sky may use dilated radius; mesh always uses its own CoC only.
                float gatherCoc = centerSky ? max (centerCoc, dilatedCoc) : centerCoc;
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                float spillAmt = clamp (uParam, 0.0, 1.0);

                if (gatherCoc < 0.4)
                {
                    fragColour = src;
                    return;
                }

                int nSamples = (uThreshold < 0.5) ? 8
                             : ((uThreshold < 1.5) ? 16 : 24);
                // Straight (non-premultiplied) average - premult + unpremult was creating
                // bright silhouette rings when soft-BG alpha and mesh alpha mixed.
                vec3 accRgb = src.rgb;
                float accA = src.a;
                float wSum = 1.0;
                // Spill only: how much OOF mesh may contribute to sky centres.
                // dilate already set gatherCoc; do not also force a high weight floor.
                float cocGate = mix (0.75, 0.20, spillAmt);

                for (int i = 0; i < 24; ++i)
                {
                    if (i >= nSamples)
                        break;

                    vec2 offset = vogelDisk (i, nSamples) * gatherCoc;
                    vec2 uv2 = clamp (vUv + offset * texel, vec2 (0.0), vec2 (1.0));
                    float sampleDepth = depthSample (uv2);
                    float sampleCoc = circleOfConfusionPx (sampleDepth);
                    bool sampleSky = (sampleDepth >= 0.9995);
                    vec4 s = texture (uTex, uv2);

                    float w = 1.0;
                    // Mesh centre: prefer similarly defocused neighbours (no bright in-focus leak).
                    if (! centerSky && sampleCoc + 0.5 < gatherCoc)
                        w = clamp (sampleCoc / max (gatherCoc, 1.0e-3), 0.05, 1.0);
                    // Sky centre: mesh contribution gated by spill only (weights ≤ 1).
                    if (centerSky && ! sampleSky)
                    {
                        if (spillAmt < 1.0e-4 || sampleCoc < cocGate)
                            w = 0.0;
                        else
                            w = clamp (sampleCoc / max (gatherCoc, 1.0e-3), 0.0, 1.0)
                                * spillAmt;
                    }

                    accRgb += s.rgb * w;
                    accA += s.a * w;
                    wSum += w;
                }

                accRgb /= max (wSum, 1.0e-3);
                accA = clamp (accA / max (wSum, 1.0e-3), 0.0, 1.0);
                float t = smoothstep (0.4, 1.75, gatherCoc);
                fragColour = vec4 (mix (src.rgb, accRgb, t), mix (src.a, accA, t));
                return;
            }
)"
        R"(            // SSR - screen-space reflection march.
            // uStrength = mix, uRadius = march distance, uThreshold = quality,
            // uParam = hit thickness.
            // uInvProj packing:
            //   [0] = (roughness, fresnelAmt, edgeFade, roughInfluence)
            //   [1] = (intensity, metallicBias, metalness, useMeshNormals)
            //   [2] = dome sky RGB, [3].x = dome fallback
            // uAux: packed view-normals (rgb = n*0.5+0.5) when useMeshNormals.
            if (uMode == 7)
            {
                float depth = depthSample (vUv);
                if (depth > 0.999)
                {
                    fragColour = src;
                    return;
                }

                const float tanHalfW = 1.0 / 1.5;
                float aspect = uResolution.y / max (uResolution.x, 1.0);
                float tanHalfH = tanHalfW * aspect;
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));

                vec3 viewPos = viewPosFromDepthUv (vUv, tanHalfW, tanHalfH);
                vec3 nrm;
                bool useMeshN = (uInvProj[1][3] > 0.5);
                if (useMeshN)
                {
                    vec3 enc = texture (uAux, vUv).rgb;
                    float encLen = length (enc);
                    if (encLen > 0.05)
                        nrm = normalize (enc * 2.0 - 1.0);
                    else
                        useMeshN = false;
                }
                if (! useMeshN)
                {
                    // Depth derivatives are blocky on curved surfaces - last resort.
                    vec3 ddx = viewPosFromDepthUv (vUv + vec2 (texel.x, 0.0), tanHalfW, tanHalfH) - viewPos;
                    vec3 ddy = viewPosFromDepthUv (vUv + vec2 (0.0, texel.y), tanHalfW, tanHalfH) - viewPos;
                    nrm = normalize (cross (ddx, ddy));
                    if (dot (nrm, -viewPos) < 0.0)
                        nrm = -nrm;
                }

                vec3 viewDir = normalize (viewPos);
                vec3 reflDir = normalize (reflect (viewDir, nrm));
                // Facing away from camera / into surface - no useful SSR.
                if (dot (reflDir, -viewDir) < 0.02)
                {
                    fragColour = src;
                    return;
                }

                float rough = clamp (uInvProj[0][0], 0.04, 1.0);
                float fresnelAmt = clamp (uInvProj[0][1], 0.0, 1.0);
                float edgeFadeAmt = clamp (uInvProj[0][2], 0.0, 1.0);
                float roughInf = clamp (uInvProj[0][3], 0.0, 1.0);
                float intensity = max (uInvProj[1][0], 0.0);
                float metalBias = clamp (uInvProj[1][1], 0.0, 1.0);
                float metal = clamp (uInvProj[1][2], 0.0, 1.0);
                vec3 domeSky = max (vec3 (uInvProj[2][0], uInvProj[2][1], uInvProj[2][2]), vec3 (0.0));
                float domeFb = clamp (uInvProj[3][0], 0.0, 1.0);

                int nSteps = (uThreshold < 0.5) ? 10
                           : ((uThreshold < 1.5) ? 18
                           : ((uThreshold < 2.5) ? 28 : 40));
                float maxDist = mix (0.12, 2.8, clamp (uRadius, 0.0, 1.0));
                float stepLen = maxDist / float (nSteps);
                float thick = mix (0.015, 0.40, clamp (uParam, 0.0, 1.0));
                // Rough surfaces need a wider acceptance slab + softer contribution.
                thick *= mix (1.0, 2.25, rough * roughInf);

                vec3 hitCol = domeSky * domeFb;
                float hitW = 0.0;
                vec2 hitUv = vUv;
                bool found = false;
                float prevZ = max (-viewPos.z, 1.0e-3);
                vec2 prevUv = vUv;

                for (int s = 1; s <= 40; ++s)
                {
                    if (s > nSteps)
                        break;
                    vec3 p = viewPos + reflDir * (stepLen * float (s));
                    float z = max (-p.z, 1.0e-3);
                    vec2 uv2 = vec2 (p.x / (tanHalfW * z), p.y / (tanHalfH * z)) * 0.5 + 0.5;
                    if (uv2.x < 0.0 || uv2.x > 1.0 || uv2.y < 0.0 || uv2.y > 1.0)
                        break;

                    float sceneZ = linearViewZ (depthSample (uv2));
                    float delta = z - sceneZ;
                    // Crossed into geometry (ray behind surface within thickness).
                    if (delta > 0.0 && delta < thick && sceneZ < 79.0)
                    {
                        // Binary refine between previous and current sample.
                        vec2 aUv = prevUv;
                        vec2 bUv = uv2;
                        float aZ = prevZ;
                        float bZ = z;
                        for (int r = 0; r < 6; ++r)
                        {
                            vec2 mUv = mix (aUv, bUv, 0.5);
                            float mRayZ = mix (aZ, bZ, 0.5);
                            float mScene = linearViewZ (depthSample (mUv));
                            if (mRayZ > mScene)
                            {
                                bUv = mUv;
                                bZ = mRayZ;
                            }
                            else
                            {
                                aUv = mUv;
                                aZ = mRayZ;
                            }
                        }
                        hitUv = bUv;
                        hitCol = texture (uTex, hitUv).rgb;
                        hitW = 1.0 - float (s) / float (nSteps);
                        found = true;
                        break;
                    }
                    prevZ = z;
                    prevUv = uv2;
                }

                // Screen-edge fade (stronger with Edge Fade).
                float edge = smoothstep (0.0, mix (0.02, 0.18, edgeFadeAmt), hitUv.x)
                           * smoothstep (0.0, mix (0.02, 0.18, edgeFadeAmt), hitUv.y)
                           * smoothstep (0.0, mix (0.02, 0.18, edgeFadeAmt), 1.0 - hitUv.x)
                           * smoothstep (0.0, mix (0.02, 0.18, edgeFadeAmt), 1.0 - hitUv.y);
                if (! found)
                    edge *= domeFb;

                float ndv = clamp (dot (nrm, -viewDir), 0.0, 1.0);
                float f0 = mix (0.04, 0.92, metal);
                float fres = f0 + (1.0 - f0) * pow (1.0 - ndv, 5.0);
                fres = mix (1.0, fres, fresnelAmt);

                float roughKill = pow (clamp (1.0 - rough, 0.0, 1.0), mix (1.0, 3.5, roughInf));
                float metalBoost = mix (1.0, 1.0 + metalBias * 1.5, metal);
                float strength = clamp (uStrength, 0.0, 1.0);

                // Soft hit sample - always a few taps (kills blocky point-sample SSR);
                // roughness widens the cone further.
                vec3 refl = hitCol;
                if (found)
                {
                    int nBlur = (uThreshold < 0.5) ? 4
                              : ((uThreshold < 1.5) ? 6
                              : ((uThreshold < 2.5) ? 8 : 10));
                    float blurPx = mix (1.25, 7.0, rough * roughInf);
                    vec3 acc = hitCol;
                    float wSum = 1.0;
                    float hitD = depthSample (hitUv);
                    for (int i = 0; i < 10; ++i)
                    {
                        if (i >= nBlur)
                            break;
                        vec2 o = vogelDisk (i, nBlur) * blurPx * texel;
                        vec2 uvB = clamp (hitUv + o, vec2 (0.0), vec2 (1.0));
                        float dB = depthSample (uvB);
                        float dw = exp (-abs (hitD - dB) * 50.0);
                        acc += texture (uTex, uvB).rgb * dw;
                        wSum += dw;
                    }
                    refl = acc / max (wSum, 1.0e-4);
                }

                float w = strength * fres * roughKill * metalBoost * intensity
                        * mix (0.35, 1.0, hitW) * edge;
                fragColour = vec4 (src.rgb + refl * w, src.a);
                return;
            }
)"
        R"(            // Shared SSGI gather. uParam.x via uParam: frame rotation radians (0 = fixed).
            // uThreshold quality; uRadius march scale. If uMode==9: GI-only (A=depth01).
            // uMode==8: legacy scene+GI composite (uStrength = intensity).
            // uAux: optional packed mesh normals (rgb = n*0.5+0.5) when uInvProj[0][0] > 0.5
            //       (we stash useMeshNormals flag in uInvProj[0][0] for mode 8/9).
            if (uMode == 8 || uMode == 9)
            {
                float depth = depthSample (vUv);
                if (depth > 0.999)
                {
                    fragColour = (uMode == 9) ? vec4 (0.0, 0.0, 0.0, depth) : src;
                    return;
                }

                const float tanHalfW = 1.0 / 1.5;
                float aspect = uResolution.y / max (uResolution.x, 1.0);
                float tanHalfH = tanHalfW * aspect;

                vec3 viewPos = viewPosFromDepthUv (vUv, tanHalfW, tanHalfH);
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                vec3 nrm;
                bool useMeshN = (uInvProj[0][0] > 0.5);
                if (useMeshN)
                {
                    vec3 enc = texture (uAux, vUv).rgb;
                    float encLen = length (enc);
                    if (encLen > 0.05)
                        nrm = normalize (enc * 2.0 - 1.0);
                    else
                        useMeshN = false;
                }
                if (! useMeshN)
                {
                    vec3 ddx = viewPosFromDepthUv (vUv + vec2 (texel.x, 0.0), tanHalfW, tanHalfH) - viewPos;
                    vec3 ddy = viewPosFromDepthUv (vUv + vec2 (0.0, texel.y), tanHalfW, tanHalfH) - viewPos;
                    nrm = normalize (cross (ddx, ddy));
                    if (dot (nrm, -viewPos) < 0.0)
                        nrm = -nrm;
                }

                vec3 up = abs (nrm.y) < 0.999 ? vec3 (0.0, 1.0, 0.0) : vec3 (1.0, 0.0, 0.0);
                vec3 t = normalize (cross (up, nrm));
                vec3 b = cross (nrm, t);

                // Quality: 0=Low 6x4, 1=Med 10x6, 2=High 14x8, 3=Ultra 20x12
                // Extra march steps at high radius so step length stays small (avoids
                // discrete "ghost sphere" copies on receivers).
                int nSamples = (uThreshold < 0.5) ? 6
                             : ((uThreshold < 1.5) ? 10
                             : ((uThreshold < 2.5) ? 14 : 20));
                int baseSteps = (uThreshold < 0.5) ? 4
                              : ((uThreshold < 1.5) ? 6
                              : ((uThreshold < 2.5) ? 8 : 12));
                float radius01 = clamp (uRadius, 0.0, 1.0);
                int nSteps = int (min (24.0, float (baseSteps) + radius01 * 12.0));
                float maxDist = mix (0.08, 0.55, radius01);
                float stepLen = maxDist / float (nSteps);
                float rot = uParam;
                float rc = cos (rot);
                float rs = sin (rot);

                vec3 gi = vec3 (0.0);
                float wSum = 0.0;
                for (int i = 0; i < 20; ++i)
                {
                    if (i >= nSamples)
                        break;

                    vec2 disc = vogelDisk (i, nSamples);
                    if (abs (rot) > 1.0e-5)
                        disc = vec2 (rc * disc.x - rs * disc.y, rs * disc.x + rc * disc.y);
                    float elev = sqrt (max (0.0, 1.0 - dot (disc, disc)));
                    vec3 dir = normalize (t * disc.x + b * disc.y + nrm * elev);

                    for (int s = 1; s <= 24; ++s)
                    {
                        if (s > nSteps)
                            break;
                        float travel = stepLen * float (s);
                        vec3 p = viewPos + dir * travel;
                        float z = max (-p.z, 1.0e-3);
                        vec2 uv2 = vec2 (p.x / (tanHalfW * z), p.y / (tanHalfH * z)) * 0.5 + 0.5;
                        if (uv2.x < 0.0 || uv2.x > 1.0 || uv2.y < 0.0 || uv2.y > 1.0)
                            break;

                        float sceneZ = linearViewZ (depthSample (uv2));
                        // Keep hit slab tight at high radius - wide slabs stamp many ghosts.
                        float thick = mix (0.05, 0.09, radius01);
                        if (sceneZ < z - 0.008 && sceneZ > z - thick)
                        {
                            // Soft tap around hit UV - thin emissive lines (gizmo) otherwise
                            // become a star of discrete dashes on the receiver.
                            vec2 hitTexel = 1.0 / max (uResolution, vec2 (1.0));
                            vec3 rad = vec3 (0.0);
                            float rW = 0.0;
                            for (int oy = -1; oy <= 1; ++oy)
                            for (int ox = -1; ox <= 1; ++ox)
                            {
                                vec2 uvs = clamp (uv2 + vec2 (float (ox), float (oy)) * hitTexel * 1.5,
                                                  vec2 (0.0), vec2 (1.0));
                                float tw = (ox == 0 && oy == 0) ? 2.0 : 1.0;
                                rad += texture (uTex, uvs).rgb * tw;
                                rW += tw;
                            }
                            rad /= max (rW, 1.0e-3);
                            // Clamp fireflies from emissive UI / hot peaks.
                            float rLum = dot (rad, vec3 (0.299, 0.587, 0.114));
                            rad *= min (1.0, 1.15 / max (rLum, 1.0e-3));
                            float nd = max (dot (nrm, dir), 0.0);
                            float atten = 1.0 - float (s) / float (nSteps);
                            float distW = 1.0 / (1.0 + 8.0 * travel * travel);
                            float w = nd * atten * distW;
                            gi += rad * w;
                            wSum += w;
                            break;
                        }
                    }
                }

                if (wSum > 1.0e-4)
                    gi /= wSum;

                if (uMode == 9)
                {
                    fragColour = vec4 (gi, depth);
                    return;
                }

                float strength = clamp (uStrength, 0.0, 1.0);
                float lum = dot (src.rgb, vec3 (0.299, 0.587, 0.114));
                float shadowBias = 1.0 - smoothstep (0.15, 0.75, lum);
                fragColour = vec4 (src.rgb + gi * strength * mix (0.35, 1.0, shadowBias), src.a);
                return;
            }
            if (uMode == 10)
            {
                // Bilateral denoise on GI buffer (uTex). uStrength = amount 0-1.
                // uParam = step scale in texels (à-trous style; 1,2,4... merges vogel "stars").
                // Depth weights stay loose - tight bilateral fails on curved receivers
                // (sphere) and leaves the sample star intact.
                float centerD = depthSample (vUv);
                vec3 center = src.rgb;
                if (centerD > 0.999 || uStrength < 1.0e-4)
                {
                    fragColour = src;
                    return;
                }
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                float stepPx = max (uParam, 1.0);
                float rad = mix (1.5, 3.5, clamp (uStrength, 0.0, 1.0)) * stepPx;
                vec3 acc = center;
                float wSum = 1.0;
                float cLum = dot (center, vec3 (0.299, 0.587, 0.114));
                float neighLumAcc = cLum;
                float neighN = 1.0;
                for (int y = -4; y <= 4; ++y)
                for (int x = -4; x <= 4; ++x)
                {
                    if (x == 0 && y == 0) continue;
                    vec2 uv2 = clamp (vUv + vec2 (float (x), float (y)) * texel * rad,
                                      vec2 (0.0), vec2 (1.0));
                    float d2 = depthSample (uv2);
                    if (d2 > 0.999) continue;
                    // Loose depth gate so curved surfaces still blur sample ghosts.
                    float dw = exp (-abs (centerD - d2) * 12.0);
                    float spat = exp (-0.12 * float (x * x + y * y));
                    float w = dw * spat;
                    vec3 s = texture (uTex, uv2).rgb;
                    acc += s * w;
                    wSum += w;
                    neighLumAcc += dot (s, vec3 (0.299, 0.587, 0.114));
                    neighN += 1.0;
                }
                vec3 blurred = acc / max (wSum, 1.0e-4);
                // Firefly kill: if this pixel is a lone hot sample vs neighbours, snap to blur.
                float meanLum = neighLumAcc / max (neighN, 1.0);
                float hot = smoothstep (meanLum * 1.6, meanLum * 3.5, cLum);
                float amt = max (clamp (uStrength, 0.0, 1.0), hot);
                fragColour = vec4 (mix (center, blurred, amt), src.a);
                return;
            }
            if (uMode == 11)
            {
                // Temporal blend. uTex=current GI (a=depth01), uAux=history (rgb+a depth),
                // uDepth=scene depth, uStrength=temporal amount.
                float curD = src.a;
                vec3 cur = src.rgb;
                vec4 hist = texture (uAux, vUv);
                float histD = hist.a;
                float amount = clamp (uStrength, 0.0, 0.97);
                float depthDelta = abs (curD - histD);
                float reject = smoothstep (0.002, 0.02, depthDelta);
                float mixA = amount * (1.0 - reject);
                if (hist.a < 1.0e-5 && length (hist.rgb) < 1.0e-5)
                    mixA = 0.0;
                vec3 outGi = mix (cur, hist.rgb, mixA);
                fragColour = vec4 (outGi, curD);
                return;
            }
            if (uMode == 15)
            {
                // Modern SVGF-style temporal. uTex=cur GI (a=depth01), uAux=history GI,
                // uDepth=moments history (r=m1,g=m2). uStrength=temporal amount.
                // Output keeps a=depth01 for history reuse.
                float curD = src.a;
                vec3 cur = src.rgb;
                vec4 hist = texture (uAux, vUv);
                vec2 mom = texture (uDepth, vUv).rg;
                float amount = clamp (uStrength, 0.0, 0.97);
                float depthDelta = abs (curD - hist.a);
                float reject = smoothstep (0.002, 0.018, depthDelta);
                float mixA = amount * (1.0 - reject);
                if (hist.a < 1.0e-5 && length (hist.rgb) < 1.0e-5)
                    mixA = 0.0;
                float lum = dot (cur, vec3 (0.299, 0.587, 0.114));
                float histLum = dot (hist.rgb, vec3 (0.299, 0.587, 0.114));
                float variance = max (0.0, mom.y - mom.x * mom.x);
                float sigma = sqrt (variance + 1.0e-4);
                float lumDiff = abs (histLum - lum);
                float clampW = smoothstep (2.0 * sigma, 6.0 * sigma, lumDiff);
                vec3 histClamped = mix (hist.rgb, cur, clampW);
                vec3 outGi = mix (cur, histClamped, mixA);
                fragColour = vec4 (outGi, curD);
                return;
)"
        R"(            }
            if (uMode == 18)
            {
                // Moments update. uTex=temporal GI, uAux=prev moments, uStrength=blend alpha.
                float lum = dot (src.rgb, vec3 (0.299, 0.587, 0.114));
                vec2 prev = texture (uAux, vUv).rg;
                float a = clamp (uStrength, 0.0, 0.97);
                if (length (prev) < 1.0e-6)
                    a = 0.0;
                float m1 = mix (lum, prev.x, a);
                float m2 = mix (lum * lum, prev.y, a);
                fragColour = vec4 (m1, m2, 0.0, 1.0);
                return;
            }
            if (uMode == 16)
            {
                // Edge-aware à-trous. uTex=GI, uDepth=scene depth, uAux=normals (encoded),
                // uStrength=amount, uParam=step in texels.
                // uInvProj[0][0] > 0.5 -> use guide normals from uAux.
                float centerD = depthSample (vUv);
                vec3 center = src.rgb;
                if (centerD > 0.999 || uStrength < 1.0e-4)
                {
                    fragColour = src;
                    return;
                }
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                float stepPx = max (uParam, 1.0);
                bool useN = (uInvProj[0][0] > 0.5);
                vec3 nrm = vec3 (0.0, 0.0, 1.0);
                if (useN)
                {
                    vec3 enc = texture (uAux, vUv).rgb;
                    if (length (enc) > 0.05)
                        nrm = normalize (enc * 2.0 - 1.0);
                    else
                        useN = false;
                }
                float k0 = 1.0;
                float k1 = 0.6666667;
                float k2 = 0.1666667;
                vec3 acc = center * (k0 * k0);
                float wSum = k0 * k0;
                float sigmaZ = mix (40.0, 120.0, clamp (uStrength, 0.0, 1.0));
                float sigmaN = mix (8.0, 32.0, clamp (uStrength, 0.0, 1.0));
                float sigmaL = mix (0.08, 0.35, clamp (uStrength, 0.0, 1.0));
                float cLum = dot (center, vec3 (0.299, 0.587, 0.114));
                for (int y = -2; y <= 2; ++y)
                for (int x = -2; x <= 2; ++x)
                {
                    if (x == 0 && y == 0)
                        continue;
                    int ax = (x < 0) ? -x : x;
                    int ay = (y < 0) ? -y : y;
                    float kx = (ax == 0) ? k0 : ((ax == 1) ? k1 : k2);
                    float ky = (ay == 0) ? k0 : ((ay == 1) ? k1 : k2);
                    float kw = kx * ky;
                    vec2 uv2 = clamp (vUv + vec2 (float (x), float (y)) * texel * stepPx,
                                     vec2 (0.0), vec2 (1.0));
                    float d2 = depthSample (uv2);
                    float wz = exp (-abs (centerD - d2) * sigmaZ);
                    float wn = 1.0;
                    if (useN)
                    {
                        vec3 enc2 = texture (uAux, uv2).rgb;
                        vec3 n2 = normalize (enc2 * 2.0 - 1.0);
                        wn = pow (max (dot (nrm, n2), 0.0), sigmaN);
                    }
                    vec3 srgb = texture (uTex, uv2).rgb;
                    float sLum = dot (srgb, vec3 (0.299, 0.587, 0.114));
                    float wl = exp (-abs (cLum - sLum) / max (sigmaL, 1.0e-3));
                    float w = kw * wz * wn * wl;
                    acc += srgb * w;
                    wSum += w;
                }
                vec3 blurred = acc / max (wSum, 1.0e-4);
                fragColour = vec4 (mix (center, blurred, clamp (uStrength, 0.0, 1.0)), src.a);
                return;
            }
            if (uMode == 12)
            {
                // Composite GI (uDepth slot) into scene (uTex). uStrength = SSGI strength.
                vec3 gi = texture (uDepth, vUv).rgb;
                float strength = clamp (uStrength, 0.0, 1.0);
                float lum = dot (src.rgb, vec3 (0.299, 0.587, 0.114));
                float shadowBias = 1.0 - smoothstep (0.15, 0.75, lum);
                fragColour = vec4 (src.rgb + gi * strength * mix (0.35, 1.0, shadowBias), src.a);
                return;
            }
            if (uMode == 14)
            {
                // Bilateral upsample half-res GI (uTex) using full-res depth (uDepth).
                float centerD = depthSample (vUv);
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                vec3 acc = vec3 (0.0);
                float wSum = 0.0;
                for (int y = -1; y <= 1; ++y)
                for (int x = -1; x <= 1; ++x)
                {
                    vec2 uv2 = clamp (vUv + vec2 (float (x), float (y)) * texel, vec2 (0.0), vec2 (1.0));
                    float d2 = depthSample (uv2);
                    float dw = exp (-abs (centerD - d2) * 60.0);
                    vec3 g = texture (uTex, uv2).rgb;
                    acc += g * dw;
                    wSum += dw;
                }
                vec3 gi = acc / max (wSum, 1.0e-4);
                fragColour = vec4 (gi, centerD);
                return;
            }
            if (uMode == 13)
            {
                // Tonemap + grade. uStrength = exposure linear (2^stops), uThreshold = grade id.
                vec3 c = max (src.rgb, vec3 (0.0)) * max (uStrength, 1.0e-4);
                // Narkowicz ACES fit
                vec3 aces = clamp ((c * (2.51 * c + 0.03)) / (c * (2.43 * c + 0.59) + 0.14), 0.0, 1.0);
                // Simple filmic
                vec3 x = max (c - 0.004, 0.0);
                vec3 filmic = (x * (6.2 * x + 0.5)) / (x * (6.2 * x + 1.7) + 0.06);
                filmic = clamp (filmic, 0.0, 1.0);

                int grade = int (uThreshold + 0.5);
                vec3 outc = aces;
                if (grade == 1)
                    outc = filmic;
                else if (grade == 2)
                {
                    outc = aces;
                    outc = pow (max (outc, vec3 (1.0e-4)), vec3 (0.95));
                    outc *= vec3 (1.06, 1.0, 0.92);
                    outc = mix (outc, vec3 (dot (outc, vec3 (0.299, 0.587, 0.114))), -0.04);
                }
                else if (grade == 3)
                {
                    outc = aces;
                    outc = pow (max (outc, vec3 (1.0e-4)), vec3 (1.02));
                    outc *= vec3 (0.92, 1.0, 1.08);
                }
                else if (grade == 4)
                {
                    outc = aces;
                    float l = dot (outc, vec3 (0.299, 0.587, 0.114));
                    vec3 shadowTint = vec3 (0.85, 1.05, 1.12);
                    vec3 highlightTint = vec3 (1.10, 0.98, 0.88);
                    float t = smoothstep (0.15, 0.75, l);
                    outc *= mix (shadowTint, highlightTint, t);
                }
                else if (grade == 5)
                {
                    outc = aces;
                    float l = dot (outc, vec3 (0.299, 0.587, 0.114));
                    outc = mix (outc, vec3 (l), 0.45);
                    outc = clamp ((outc - 0.5) * 1.35 + 0.5, 0.0, 1.0);
                }
                fragColour = vec4 (clamp (outc, 0.0, 1.0), src.a);
                return;
            }
            if (uMode == 17)
            {
                // DOF CoC write - mesh only (sky / cleared depth -> 0).
                // Store CoC / maxBlur so RGBA8 ping-pong FBOs don't clamp pixel radii.
                float baseBlur = (uThreshold < 0.5) ? 6.0
                               : ((uThreshold < 1.5) ? 10.0 : 14.0);
                float power = clamp (uStrength, 0.0, 16.0);
                float maxBlur = max (baseBlur * mix (1.0, 6.0, clamp (power * 0.35, 0.0, 1.0)), 1.0e-3);
                float cocN = circleOfConfusionPx (depthSample (vUv)) / maxBlur;
                fragColour = vec4 (cocN, cocN, cocN, 1.0);
                return;
            }
            if (uMode == 19)
            {
                // DOF CoC dilate - max-filter spreads mesh CoC into the soft-BG void.
                // Radius scales with dilate only (was mix(1, maxBlur) so dilate=0 still
                // spread 1px and double-dipped with spill in mode 6).
                float dilateAmt = clamp (uParam, 0.0, 1.0);
                float baseBlur = (uThreshold < 0.5) ? 6.0
                               : ((uThreshold < 1.5) ? 10.0 : 14.0);
                float power = clamp (uStrength, 0.0, 16.0);
                float maxBlur = baseBlur * mix (1.0, 6.0, clamp (power * 0.35, 0.0, 1.0));
                float m = texture (uTex, vUv).r;
                // Zero dilate = exact copy of mode 17 (no free edge spread).
                float radius = maxBlur * dilateAmt;
                if (radius >= 0.5)
                {
                    vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                    const int nProbe = 16;
                    for (int i = 0; i < nProbe; ++i)
                    {
                        vec2 uv2 = clamp (vUv + vogelDisk (i, nProbe) * radius * texel,
                                          vec2 (0.0), vec2 (1.0));
                        float c2 = texture (uTex, uv2).r;
                        // Neighbour must be OOF enough to reach this far (stricter than before).
                        float dist = length (vogelDisk (i, nProbe) * radius);
                        if (c2 * maxBlur >= dist * 1.05)
                            m = max (m, c2);
                    }
                }
                fragColour = vec4 (m, m, m, 1.0);
                return;
            }
            if (uMode == 20)
            {
                // UE-style camera motion blur: reconstruct view pos from depth,
                // reproject with previous view-projection -> screen velocity, gather.
                // uStrength = shutter (0-1), uRadius = max streak (px), uThreshold = quality.
                float depth = depthSample (vUv);
                if (depth >= 0.9995)
                {
                    fragColour = src;
                    return;
                }
                float aspect = uResolution.y / max (uResolution.x, 1.0);
                const float tanHalfW = 1.0 / 1.5;
                float tanHalfH = tanHalfW * aspect;
                vec3 viewP = viewPosFromDepthUv (vUv, tanHalfW, tanHalfH);
                vec4 worldP = uMotionInvView * vec4 (viewP, 1.0);
                vec4 prevClip = uMotionPrevVP * worldP;
                if (abs (prevClip.w) < 1.0e-5)
                {
                    fragColour = src;
                    return;
                }
                vec2 prevUv = (prevClip.xy / prevClip.w) * 0.5 + 0.5;
                // Reject wild reprojections (teleport / first frame garbage).
                if (prevUv.x < -0.05 || prevUv.x > 1.05 || prevUv.y < -0.05 || prevUv.y > 1.05)
                {
                    fragColour = src;
                    return;
                }
                vec2 vel = (vUv - prevUv) * clamp (uStrength, 0.0, 2.0);
                vec2 velPx = vel * uResolution;
                float lenPx = length (velPx);
                float maxPx = max (uRadius, 1.0);
                if (lenPx < 0.35)
                {
                    fragColour = src;
                    return;
                }
                if (lenPx > maxPx)
                    velPx *= maxPx / lenPx;
                vel = velPx / max (uResolution, vec2 (1.0));

                int nSamples = (uThreshold < 0.5) ? 8
                             : ((uThreshold < 1.5) ? 16 : 24);
                float centerZ = linearViewZ (depth);
                vec3 acc = src.rgb;
                float wsum = 1.0;
                // Centered gather along +/-velocity (reconstruction-style).
                for (int i = 1; i < 24; ++i)
                {
                    if (i >= nSamples)
                        break;
                    float t = (float (i) / float (nSamples)) - 0.5;
                    vec2 uv2 = vUv + vel * (t * 2.0);
                    if (uv2.x < 0.0 || uv2.x > 1.0 || uv2.y < 0.0 || uv2.y > 1.0)
                        continue;
                    float d2 = depthSample (uv2);
                    if (d2 >= 0.9995)
                        continue;
                    float z2 = linearViewZ (d2);
                    // Depth rejection - reduce ghosting across silhouettes.
                    float dz = abs (z2 - centerZ);
                    float w = exp (-dz * 6.0);
                    if (w < 0.02)
                        continue;
                    acc += texture (uTex, uv2).rgb * w;
                    wsum += w;
                }
                fragColour = vec4 (acc / max (wsum, 1.0e-4), src.a);
                return;
            }

            // SSAO - depth-delta taps (stable without perfect inv-projection).
            float depth = depthSample (vUv);
            if (depth > 0.999)
            {
                fragColour = src;
                return;
            }
            vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
            float occl = 0.0;
            const int kTaps = 8;
            vec2 offs[8] = vec2[] (
                vec2 ( 1.0,  0.0), vec2 (-1.0,  0.0),
                vec2 ( 0.0,  1.0), vec2 ( 0.0, -1.0),
                vec2 ( 0.7,  0.7), vec2 (-0.7,  0.7),
                vec2 ( 0.7, -0.7), vec2 (-0.7, -0.7)
            );
            float rad = max (uRadius, 0.15) * 10.0;
            for (int i = 0; i < kTaps; ++i)
            {
                vec2 uv2 = clamp (vUv + offs[i] * texel * rad, vec2 (0.0), vec2 (1.0));
                float d2 = depthSample (uv2);
                float delta = depth - d2;
                occl += smoothstep (0.0005, 0.02, delta) * (1.0 - smoothstep (0.02, 0.08, delta));
            }
            occl = clamp (occl / float (kTaps), 0.0, 1.0);
            float ao = mix (1.0, 1.0 - occl, clamp (uStrength, 0.0, 1.0));
            fragColour = vec4 (src.rgb * ao, src.a);
        }
    )";

    constexpr const char* kLabelVertexShader = R"(
        #version 150
        in vec3 position;
        in vec2 texCoord;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        out vec2 vTex;
        void main()
        {
            vTex = texCoord;
            // World-space billboard - same depth path as the mesh so DOF CoC matches.
            gl_Position = projectionMatrix * viewMatrix * vec4 (position, 1.0);
        }
    )";

    constexpr const char* kLabelFragmentShader = R"(
        #version 150
        in vec2 vTex;
        out vec4 fragColour;
        uniform sampler2D uTex;
        uniform vec2 uResolution;
        uniform float uCornerRadius;
        uniform vec4 uClearColour;
        void main()
        {
            vec2 halfSize = uResolution * 0.5;
            vec2 p = gl_FragCoord.xy - halfSize;
            vec2 q = abs(p) - halfSize + vec2(uCornerRadius);
            float d = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - uCornerRadius;
            if (d > 0.0)
            {
                fragColour = uClearColour;
                return;
            }
            vec4 t = texture (uTex, vTex);
            if (t.a < 0.08)
                discard;
            fragColour = t;
        }
    )";

    constexpr const char* kTintVertexShader = R"(
        #version 150
        in vec2 position;
        void main()
        {
            gl_Position = vec4 (position, 0.0, 1.0);
        }
    )";

    constexpr const char* kTintFragmentShader = R"(
        #version 150
        out vec4 fragColour;
        uniform vec4 uTint;
        void main()
        {
            fragColour = uTint;
        }
    )";

    static constexpr float kMajorHz[] = {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };
    static constexpr float kMinorHz[] = {
        30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f,
        150.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
        1500.0f, 3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 15000.0f
    };
}

//==============================================================================
Spectrogram3DComponent::GlHost::GlHost (Spectrogram3DComponent& o)
    : owner (o)
{
    // Hard BG: visible nested HWND. Soft BG: tiny peer keeps the context alive for FBO render.
    setOpaque (true);
    setVisible (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    // Prefer 3.2 (GLSL 150 mesh/post). GPU particle compute probes for 4.3 at runtime.
    // Requiring 4.3 at attach time often fails or hangs when a game already owns the GPU.
    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer (this);
    openGLContext.setComponentPaintingEnabled (false);
    applyPixelFormat();
}

Spectrogram3DComponent::GlHost::~GlHost()
{
    openGLContext.detach();
}

void Spectrogram3DComponent::GlHost::applyPixelFormat()
{
    const int samples = (int) owner.msaaLevel;
    openGLContext.setMultisamplingEnabled (samples > 0);
    juce::OpenGLPixelFormat pf (8, 8, 24, 8);
    // Cap MSAA under contention - high MSAA + exclusive game = attach stalls.
    pf.multisamplingLevel = (uint8_t) juce::jlimit (0, 8, samples);
    openGLContext.setPixelFormat (pf);
}

void Spectrogram3DComponent::GlHost::resetAttachState() noexcept
{
    attachPending = false;
    contextFailed = false;
    attachAttempts = 0;
}

void Spectrogram3DComponent::GlHost::setActive (bool shouldBeActive) noexcept
{
    setVisible (shouldBeActive);
    if (shouldBeActive)
    {
        // Fresh attempt each time Spec3D is shown (e.g. after closing a game).
        resetAttachState();
        requestAttachAsync();
    }
}

void Spectrogram3DComponent::GlHost::requestAttachAsync()
{
    if (openGLContext.isAttached() || attachPending || contextFailed)
        return;

    if (attachAttempts >= kMaxAttachAttempts)
    {
        contextFailed = true;
        owner.repaint();
        return;
    }

    attachPending = true;
    ++attachAttempts;

    // Back off when the GPU is busy (game running): avoid flooding callAsync.
    const int delayMs = juce::jmin (750, (attachAttempts - 1) * 75);

    juce::Component::SafePointer<GlHost> safe (this);
    juce::Timer::callAfterDelay (delayMs, [safe]
    {
        if (safe == nullptr)
            return;
        safe->attachPending = false;
        if (! safe->isVisible() || safe->contextFailed)
            return;
        safe->attachNow();
    });
}

void Spectrogram3DComponent::GlHost::attachNow()
{
    if (! isVisible() || openGLContext.isAttached() || contextFailed)
        return;

    if (getPeer() == nullptr || getWidth() < 2 || getHeight() < 2)
    {
        requestAttachAsync();
        return;
    }

    applyPixelFormat();

    // attachTo can block under GPU contention; we already deferred via callAfterDelay.
    // If it does not attach, retry with backoff rather than spinning forever.
    try
    {
        openGLContext.attachTo (*this);
    }
    catch (...)
    {
        contextFailed = true;
        owner.repaint();
        return;
    }

    if (! openGLContext.isAttached())
    {
        requestAttachAsync();
        owner.repaint();
        return;
    }

    attachAttempts = 0;
    openGLContext.triggerRepaint();
    owner.repaint();

    // If the driver never finishes init (stuck on "Initialising..."), fail soft - never hang the host.
    juce::Component::SafePointer<GlHost> safe (this);
    juce::Timer::callAfterDelay (2500, [safe]
    {
        if (safe == nullptr || ! safe->isVisible() || safe->contextFailed)
            return;
        if (safe->glReady)
            return;
        // Drop a half-dead context so Ableton keeps running; user can retry Spec later.
        if (safe->openGLContext.isAttached())
            safe->openGLContext.detach();
        safe->glReady = false;
        safe->contextFailed = true;
        safe->owner.repaint();
    });
}

void Spectrogram3DComponent::GlHost::reattachWithCurrentFormat()
{
    applyPixelFormat();
    if (openGLContext.isAttached())
        openGLContext.detach();
    glReady = false;
    if (isVisible())
    {
        resetAttachState();
        requestAttachAsync();
    }
}

void Spectrogram3DComponent::GlHost::createShaders()
{
    colourShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! colourShader->addVertexShader (kColourVertexShader)
        || ! colourShader->addFragmentShader (kColourFragmentShader)
        || ! colourShader->link())
    {
        DBG ("Spectrogram3D colour shader: " + colourShader->getLastError());
        colourShader.reset();
        contextFailed = true;
        return;
    }

    colourPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*colourShader, "position");
    colourColourAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*colourShader, "colour");
    colourNormalAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*colourShader, "normal");
    colourProjectionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "projectionMatrix");
    colourViewUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "viewMatrix");
    colourResolutionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uResolution");
    colourCornerUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCornerRadius");
    colourClearUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uClearColour");
    colourLightDirUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uLightDirView");
    colourLightingAmountUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uLightingAmount");
    colourSpecularUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSpecular");
    colourRoughnessUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uRoughness");
    colourMetalnessUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMetalness");
    colourEnergyConserveUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uEnergyConserve");
    colourRimUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uRim");
    colourLightColourUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uLightColour");
    colourRimColourUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uRimColour");
    colourDomeStrengthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uDomeStrength");
    colourDomeSkyUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uDomeSky");
    colourDomeGroundUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uDomeGround");
    colourDomeUseTexUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uDomeUseTex");
    colourDomeMapUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uDomeMap");
    colourHeightMapUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uHeightMap");
    colourLightDirWorldUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uLightDirWorld");
    colourSelfShadowUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSelfShadow");
    colourMeshHeightUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMeshHeight");
    colourReverseFreqUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uReverseFreq");
    colourFreqBiasBUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uFreqBiasB");
    colourFreqBiasPivotUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uFreqBiasPivot");
    colourAoAmountUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAoAmount");
    colourAoRadiusUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAoRadius");
    colourShadowDirXZUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowDirXZ");
    colourShadowSunTanUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowSunTan");
    colourShadowBiasUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowBias");
    colourShadowSoftnessUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowSoftness");
    colourShadowQualityUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowQuality");
    colourContactUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uContactShadow");
    colourSssModeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssMode");
    colourSssStrengthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssStrength");
    colourSssWrapUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssWrap");
    colourSssTransmissionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssTransmission");
    colourSssTintUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssTint");
    colourSssRadiusUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssRadius");
    colourSssContrastUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssContrast");
    colourSssQualityUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssQuality");
    colourSssBaseDepthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uClosedFloorY");
    colourSssThickScaleUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssThickScale");
    colourSssMaxThickUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uSssMaxThick");
    colourAudioLevelUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioLevel");
    colourAudioMinUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioMin");
    colourAudioMaxUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioMax");
    colourAudioModBrightUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioModBright");
    colourAudioModLitAmtUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioModLitAmt");
    colourAudioModSpecUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioModSpec");
    colourAudioModRimUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioModRim");
    colourAudioModDomeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioModDome");
    colourAudioAffectPlayheadUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioAffectPlayhead");
    colourAudioAffectAntiUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioAffectAnti");
    colourPlayheadWallXUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uPlayheadWallX");
    colourAntiPlayheadWallXUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAntiPlayheadWallX");
    colourShadowAtlasUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowAtlas");
    colourCastShadowUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCastShadow");
    colourShadowMatrix0Uniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowMatrix0");
    colourShadowMatrix1Uniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowMatrix1");
    colourShadowMatrix2Uniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowMatrix2");
    colourShadowMatrix3Uniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowMatrix3");
    colourCascadeSplitsUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCascadeSplits");
    colourCascadeCountUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCascadeCount");
    colourCascadeTransitionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCascadeTransition");
    colourShadowAtlasTilesUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uShadowAtlasTiles");
    colourCastBiasUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCastBias");
    colourCastSoftnessUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uCastSoftness");
    colourViewZUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uViewZScale");
    colourMatOverrideUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMatOverride");
    colourMatAlbedoUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMatAlbedo");
    colourMatRoughUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMatRoughness");
    colourMatMetalUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMatMetalness");
    colourMatSpecularUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uMatSpecular");
    colourGizmoXrayUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uGizmoXray");
    colourXrayCenterUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uXrayCenter");
    colourXrayRadiusUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uXrayRadius");
    colourXrayAlphaUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uXrayAlpha");
    if (colourGizmoXrayUniform != nullptr)
        colourGizmoXrayUniform->set (0.0f);

    shadowDepthShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! shadowDepthShader->addVertexShader (kShadowDepthVertexShader)
        || ! shadowDepthShader->addFragmentShader (kShadowDepthFragmentShader)
        || ! shadowDepthShader->link())
    {
        DBG ("Spectrogram3D shadow depth: " + shadowDepthShader->getLastError());
        shadowDepthShader.reset();
    }
    else
    {
        shadowDepthPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*shadowDepthShader, "position");
        shadowDepthLightVpUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*shadowDepthShader, "uLightVP");
    }

    labelShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! labelShader->addVertexShader (kLabelVertexShader)
        || ! labelShader->addFragmentShader (kLabelFragmentShader)
        || ! labelShader->link())
    {
        DBG ("Spectrogram3D label shader: " + labelShader->getLastError());
        labelShader.reset();
    }
    else
    {
        labelPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*labelShader, "position");
        labelTexAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*labelShader, "texCoord");
        labelTexUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uTex");
        labelProjectionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "projectionMatrix");
        labelViewUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "viewMatrix");
        labelResolutionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uResolution");
        labelCornerUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uCornerRadius");
        labelClearUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*labelShader, "uClearColour");
    }

    tintShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! tintShader->addVertexShader (kTintVertexShader)
        || ! tintShader->addFragmentShader (kTintFragmentShader)
        || ! tintShader->link())
    {
        DBG ("Spectrogram3D tint shader: " + tintShader->getLastError());
        tintShader.reset();
    }
    else
    {
        tintPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*tintShader, "position");
        tintColourUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*tintShader, "uTint");
    }

    contactShadowShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! contactShadowShader->addVertexShader (kContactVertexShader)
        || ! contactShadowShader->addFragmentShader (kContactFragmentShader)
        || ! contactShadowShader->link())
    {
        DBG ("Spectrogram3D contact shadow: " + contactShadowShader->getLastError());
        contactShadowShader.reset();
    }
    else
    {
        contactPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*contactShadowShader, "position");
        contactProjectionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*contactShadowShader, "projectionMatrix");
        contactViewUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*contactShadowShader, "viewMatrix");
        contactStrengthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*contactShadowShader, "uStrength");
    }

    normalsShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! normalsShader->addVertexShader (kNormalsVertexShader)
        || ! normalsShader->addFragmentShader (kNormalsFragmentShader)
        || ! normalsShader->link())
    {
        DBG ("Spectrogram3D normals: " + normalsShader->getLastError());
        normalsShader.reset();
    }
    else
    {
        normalsPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*normalsShader, "position");
        normalsNormalAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*normalsShader, "normal");
        normalsProjectionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*normalsShader, "projectionMatrix");
        normalsViewUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*normalsShader, "viewMatrix");
    }

    postShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);
    if (! postShader->addVertexShader (kPostVertexShader)
        || ! postShader->addFragmentShader (kPostFragmentShader)
        || ! postShader->link())
    {
        DBG ("Spectrogram3D post: " + postShader->getLastError());
        postShader.reset();
    }
    else
    {
        postPositionAttrib = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*postShader, "position");
        postTexUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uTex");
        postDepthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uDepth");
        postAuxUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uAux");
        postModeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uMode");
        postStrengthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uStrength");
        postRadiusUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uRadius");
        postThresholdUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uThreshold");
        postParamUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uParam");
        postResolutionUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uResolution");
        postInvProjUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uInvProj");
        postMotionInvViewUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uMotionInvView");
        postMotionPrevVpUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uMotionPrevVP");
    }

    contextFailed = (colourShader == nullptr);
}

void Spectrogram3DComponent::GlHost::destroyShaders()
{
    colourXrayAlphaUniform.reset();
    colourXrayRadiusUniform.reset();
    colourXrayCenterUniform.reset();
    colourGizmoXrayUniform.reset();
    colourMatSpecularUniform.reset();
    colourMatMetalUniform.reset();
    colourMatRoughUniform.reset();
    colourMatAlbedoUniform.reset();
    colourMatOverrideUniform.reset();
    colourViewZUniform.reset();
    colourCastSoftnessUniform.reset();
    colourCastBiasUniform.reset();
    colourShadowAtlasTilesUniform.reset();
    colourCascadeTransitionUniform.reset();
    colourCascadeCountUniform.reset();
    colourCascadeSplitsUniform.reset();
    colourShadowMatrix3Uniform.reset();
    colourShadowMatrix2Uniform.reset();
    colourShadowMatrix1Uniform.reset();
    colourShadowMatrix0Uniform.reset();
    colourCastShadowUniform.reset();
    colourShadowAtlasUniform.reset();
    colourContactUniform.reset();
    colourShadowQualityUniform.reset();
    colourShadowSoftnessUniform.reset();
    colourShadowBiasUniform.reset();
    colourShadowSunTanUniform.reset();
    colourShadowDirXZUniform.reset();
    colourAoRadiusUniform.reset();
    colourAoAmountUniform.reset();
    colourFreqBiasPivotUniform.reset();
    colourFreqBiasBUniform.reset();
    colourReverseFreqUniform.reset();
    colourMeshHeightUniform.reset();
    colourSelfShadowUniform.reset();
    colourLightDirWorldUniform.reset();
    colourHeightMapUniform.reset();
    colourRimUniform.reset();
    colourDomeMapUniform.reset();
    colourDomeUseTexUniform.reset();
    colourDomeGroundUniform.reset();
    colourDomeSkyUniform.reset();
    colourDomeStrengthUniform.reset();
    colourRimColourUniform.reset();
    colourLightColourUniform.reset();
    colourEnergyConserveUniform.reset();
    colourMetalnessUniform.reset();
    colourRoughnessUniform.reset();
    colourSpecularUniform.reset();
    colourLightingAmountUniform.reset();
    colourLightDirUniform.reset();
    colourClearUniform.reset();
    colourCornerUniform.reset();
    colourResolutionUniform.reset();
    colourViewUniform.reset();
    colourProjectionUniform.reset();
    colourNormalAttrib.reset();
    colourColourAttrib.reset();
    colourPositionAttrib.reset();
    colourShader.reset();

    shadowDepthLightVpUniform.reset();
    shadowDepthPositionAttrib.reset();
    shadowDepthShader.reset();

    labelClearUniform.reset();
    labelCornerUniform.reset();
    labelResolutionUniform.reset();
    labelViewUniform.reset();
    labelProjectionUniform.reset();
    labelTexUniform.reset();
    labelTexAttrib.reset();
    labelPositionAttrib.reset();
    labelShader.reset();

    tintColourUniform.reset();
    tintPositionAttrib.reset();
    tintShader.reset();

    contactStrengthUniform.reset();
    contactViewUniform.reset();
    contactProjectionUniform.reset();
    contactPositionAttrib.reset();
    contactShadowShader.reset();

    postMotionPrevVpUniform.reset();
    postMotionInvViewUniform.reset();
    postInvProjUniform.reset();
    postResolutionUniform.reset();
    postParamUniform.reset();
    postThresholdUniform.reset();
    postRadiusUniform.reset();
    postStrengthUniform.reset();
    postModeUniform.reset();
    postAuxUniform.reset();
    postDepthUniform.reset();
    postTexUniform.reset();
    postPositionAttrib.reset();
    postShader.reset();

    normalsViewUniform.reset();
    normalsProjectionUniform.reset();
    normalsNormalAttrib.reset();
    normalsPositionAttrib.reset();
    normalsShader.reset();
}

void Spectrogram3DComponent::GlHost::newOpenGLContextCreated()
{
    meshVbo = meshIbo = floorVbo = labelVbo = tintVbo = contactVbo = 0;
    sphereVbo = sphereIbo = gizmoVbo = gizmoIbo = 0;
    shadowFbo = shadowDepthTex = 0;
    shadowAtlasW = shadowAtlasH = shadowTileRes = shadowAtlasCascades = 0;
    cascadesBuilt = 0;
    sphereIndexCount = gizmoIndexCount = 0;
    sphereNeedsUpload = true;
    softDepthTex = 0;
    softMsaaFbo = softMsaaColorRb = softMsaaDepthRb = 0;
    softMsaaW = softMsaaH = softMsaaSamples = 0;
    heightMapTex = 0;
    heightMapW = heightMapH = 0;
    floorVertexCount = 0;
    floorGridSr = 0.0;
    labelAtlasReady = false;
    softFboW = softFboH = 0;
    postFboW = postFboH = 0;
    ssgiHalfW = ssgiHalfH = 0;
    ssgiHistoryW = ssgiHistoryH = 0;
    ssgiMomentsW = ssgiMomentsH = 0;
    ssgiNormalsW = ssgiNormalsH = 0;
    ssgiHistoryValid = false;
    ssgiMomentsValid = false;
    postFrameIndex = 0;
    softContentDirty = true;
    createShaders();
    juce::gl::glGenBuffers (1, &meshVbo);
    juce::gl::glGenBuffers (1, &meshIbo);
    juce::gl::glGenBuffers (1, &labelVbo);
    juce::gl::glGenBuffers (1, &tintVbo);
    juce::gl::glGenBuffers (1, &contactVbo);
    juce::gl::glGenBuffers (1, &sphereVbo);
    juce::gl::glGenBuffers (1, &sphereIbo);
    juce::gl::glGenBuffers (1, &gizmoVbo);
    juce::gl::glGenBuffers (1, &gizmoIbo);
    glReady = (colourShader != nullptr && meshVbo != 0 && meshIbo != 0);
    meshNeedsUpload = true;
    owner.meshNeedsUpload = true;
}

void Spectrogram3DComponent::GlHost::openGLContextClosing()
{
    labelAtlas.release();
    labelAtlasReady = false;
    releasePostFrameBuffers();
    releaseSoftMsaaBuffers();
    if (softDepthTex != 0)
    {
        juce::gl::glDeleteTextures (1, &softDepthTex);
        softDepthTex = 0;
    }
    releaseShadowAtlas();
    if (heightMapTex != 0)
    {
        juce::gl::glDeleteTextures (1, &heightMapTex);
        heightMapTex = 0;
        heightMapW = heightMapH = 0;
    }
    domeMapTex.release();
    domeMapReady = false;
    softFbo.release();
    softFboW = softFboH = 0;
    if (meshVbo != 0) { juce::gl::glDeleteBuffers (1, &meshVbo); meshVbo = 0; }
    if (meshIbo != 0) { juce::gl::glDeleteBuffers (1, &meshIbo); meshIbo = 0; }
    if (floorVbo != 0) { juce::gl::glDeleteBuffers (1, &floorVbo); floorVbo = 0; }
    if (labelVbo != 0) { juce::gl::glDeleteBuffers (1, &labelVbo); labelVbo = 0; }
    if (tintVbo != 0) { juce::gl::glDeleteBuffers (1, &tintVbo); tintVbo = 0; }
    if (contactVbo != 0) { juce::gl::glDeleteBuffers (1, &contactVbo); contactVbo = 0; }
    if (sphereVbo != 0) { juce::gl::glDeleteBuffers (1, &sphereVbo); sphereVbo = 0; }
    if (sphereIbo != 0) { juce::gl::glDeleteBuffers (1, &sphereIbo); sphereIbo = 0; }
    if (gizmoVbo != 0) { juce::gl::glDeleteBuffers (1, &gizmoVbo); gizmoVbo = 0; }
    if (gizmoIbo != 0) { juce::gl::glDeleteBuffers (1, &gizmoIbo); gizmoIbo = 0; }
    if (owner.particleSystem != nullptr)
        owner.particleSystem->releaseGl();
    destroyShaders();
    glReady = false;
    meshIndexCount = 0;
    floorVertexCount = 0;
    sphereIndexCount = 0;
    gizmoIndexCount = 0;
}

void Spectrogram3DComponent::GlHost::setCornerUniforms (juce::OpenGLShaderProgram&) const
{
    const auto px = getViewPixelBounds();
    const float resX = (float) juce::jmax (1, px.getWidth());
    const float resY = (float) juce::jmax (1, px.getHeight());
    const float scale = resX / (float) juce::jmax (1, owner.getGlViewLocal().getWidth());
    // Soft/FBO path clips to the frame in paint(); keep GL fill full-bleed so tint isn't wiped.
    const bool fboPath = owner.usesSoftComposite();
    const float corner = fboPath ? 0.0f
                                 : juce::jmax (1.0f, (kCornerRadius - (float) kGlInset) * scale);
    const auto clear = owner.getClearColour();

    if (colourResolutionUniform != nullptr)
        colourResolutionUniform->set (resX, resY);
    if (colourCornerUniform != nullptr)
        colourCornerUniform->set (corner);
    const float cr = clear.getFloatRed();
    const float cg = clear.getFloatGreen();
    const float cb = clear.getFloatBlue();
    // Outside the rounded mesh: transparent in Soft BG; opaque clear when Soft BG is off.
    const float ca = owner.isTransparentBackground() ? 0.0f : clear.getFloatAlpha();

    if (colourClearUniform != nullptr)
        colourClearUniform->set (cr, cg, cb, ca);

    if (labelResolutionUniform != nullptr)
        labelResolutionUniform->set (resX, resY);
    if (labelCornerUniform != nullptr)
        labelCornerUniform->set (corner);
    if (labelClearUniform != nullptr)
        labelClearUniform->set (cr, cg, cb, ca);
}

void Spectrogram3DComponent::GlHost::uploadMeshIfNeeded()
{
    std::vector<Vertex> verts;
    std::vector<uint32_t> inds;
    {
        const juce::ScopedLock sl (owner.meshLock);
        if (! owner.meshNeedsUpload && ! meshNeedsUpload)
            return;
        verts = owner.cpuVertices;
        inds = owner.cpuIndices;
        owner.meshNeedsUpload = false;
        meshNeedsUpload = false;
    }

    if (verts.empty() || inds.empty() || meshVbo == 0 || meshIbo == 0)
        return;

    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, meshVbo);
    juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                            (GLsizeiptr) (verts.size() * sizeof (Vertex)),
                            verts.data(), juce::gl::GL_DYNAMIC_DRAW);
    juce::gl::glBindBuffer (juce::gl::GL_ELEMENT_ARRAY_BUFFER, meshIbo);
    juce::gl::glBufferData (juce::gl::GL_ELEMENT_ARRAY_BUFFER,
                            (GLsizeiptr) (inds.size() * sizeof (uint32_t)),
                            inds.data(), juce::gl::GL_DYNAMIC_DRAW);
    meshIndexCount = (int) inds.size();

    // Top surface is always the first meshW*meshH verts (closed mesh appends a bottom copy).
    // Must refresh every upload - a stale height map makes shadows/AO/SSS scroll vs the waterfall.
    if (owner.meshW >= 2 && owner.meshH >= 2
        && (int) verts.size() >= owner.meshW * owner.meshH)
        uploadHeightMap (verts, owner.meshW, owner.meshH);
}

void Spectrogram3DComponent::GlHost::ensureFloorGeometry()
{
    const double sr = (owner.dataSource != nullptr)
                          ? juce::jmax (1.0, owner.dataSource->getDisplaySampleRate())
                          : 48000.0;
    const bool logFreq = (owner.dataSource != nullptr) ? owner.dataSource->isLogFrequencyAxis() : true;
    const bool soft = owner.usesSoftComposite();
    const bool reverse = owner.reverseFrequencyAxis;
    // Epoch 6: soft - no perimeter, ~1px interior grid, ticks are billboards.
    constexpr int kFloorRibbonEpoch = 6;

    if (floorVbo != 0 && sr == floorGridSr && logFreq == floorGridLog && soft == floorGridSoftBg
        && reverse == floorGridReverseFreq && floorRibbonEpoch == kFloorRibbonEpoch)
        return;

    floorGridSr = sr;
    floorGridLog = logFreq;
    floorGridSoftBg = soft;
    floorGridReverseFreq = reverse;
    floorRibbonEpoch = kFloorRibbonEpoch;
    owner.rebuildFreqLabels (sr, logFreq);
    rebuildFloorGeometry();
    labelAtlasReady = false;
}

void Spectrogram3DComponent::GlHost::rebuildFloorGeometry()
{
    constexpr float groundY = -0.012f;
    constexpr float gridY = -0.010f;
    const bool soft = owner.usesSoftComposite();
    // Soft: thin ribbons (MSAA resolves edges). Solid keeps slightly wider strokes for DOF gather.
    // Was 0.00055 - sub-pixel without reliable MSAA on post-stack chrome looked jagged.
    const float kGridHalfW = soft ? 0.0011f : 0.0045f;
    // Solid-only thick ticks (soft uses camera-facing billboards in drawPlayheadTicks).
    constexpr float kTickHalfW = 0.00115f;
    constexpr float kTickTopY = 0.017f;
    constexpr float kTickX = 1.022f;
    std::vector<Vertex> verts;
    verts.reserve (900);

    auto pushVert = [&verts] (float x, float y, float z, float r, float g, float b)
    {
        verts.push_back ({ x, y, z, r, g, b, 0.0f, 1.0f, 0.0f });
    };

    // Axis-aligned ribbon in XZ (or vertical tick in XY). Two triangles.
    auto pushRibbon = [&pushVert] (float x0, float y0, float z0,
                                   float x1, float y1, float z1,
                                   float halfW, float r, float g, float b)
    {
        const float dx = x1 - x0, dy = y1 - y0, dz = z1 - z0;
        const float len = std::sqrt (dx * dx + dy * dy + dz * dz);
        if (len < 1.0e-6f)
            return;
        float px = 0.0f, py = 0.0f, pz = 0.0f;
        if (std::abs (dy) > std::abs (dx) && std::abs (dy) > std::abs (dz))
        {
            px = halfW;
        }
        else
        {
            px = -dz / len * halfW;
            pz =  dx / len * halfW;
        }

        pushVert (x0 + px, y0 + py, z0 + pz, r, g, b);
        pushVert (x0 - px, y0 - py, z0 - pz, r, g, b);
        pushVert (x1 - px, y1 - py, z1 - pz, r, g, b);
        pushVert (x0 + px, y0 + py, z0 + pz, r, g, b);
        pushVert (x1 - px, y1 - py, z1 - pz, r, g, b);
        pushVert (x1 + px, y1 + py, z1 + pz, r, g, b);
    };

    // Opaque ground plane (two triangles). Skipped for soft composite so EQ shows through.
    if (! soft)
    {
        const float gr = 0.06f, gg = 0.07f, gb = 0.09f;
        pushVert (-1.0f, groundY, -1.0f, gr, gg, gb);
        pushVert ( 1.0f, groundY, -1.0f, gr, gg, gb);
        pushVert ( 1.0f, groundY,  1.0f, gr, gg, gb);
        pushVert (-1.0f, groundY, -1.0f, gr, gg, gb);
        pushVert ( 1.0f, groundY,  1.0f, gr, gg, gb);
        pushVert (-1.0f, groundY,  1.0f, gr, gg, gb);
    }

    const float nyquist = (float) (floorGridSr * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);
    // Soft: inset from +/-1 so freq/time strokes never form a rectangular border.
    const float x0 = soft ? -0.98f : -1.0f;
    const float x1 = soft ?  0.98f :  1.0f;
    const float z0 = soft ? -0.98f : -1.0f;
    const float z1 = soft ?  0.98f :  1.0f;

    for (float hz : kMinorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float z = owner.worldZForFreq (hz, floorGridSr, floorGridLog);
        pushRibbon (x0, gridY, z, x1, gridY, z, kGridHalfW * 0.75f, 0.28f, 0.30f, 0.34f);
    }
    for (float hz : kMajorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float z = owner.worldZForFreq (hz, floorGridSr, floorGridLog);
        const float maj = soft ? 0.48f : 0.62f;
        pushRibbon (x0, gridY, z, x1, gridY, z, kGridHalfW, maj, maj + 0.03f, maj + 0.08f);
        // Solid path keeps world ticks; soft uses camera-facing drawPlayheadTicks().
        if (! soft)
            pushRibbon (kTickX, gridY, z, kTickX, kTickTopY, z, kTickHalfW, 0.85f, 0.85f, 0.88f);
    }

    constexpr int timeDiv = 8;
    for (int i = 0; i <= timeDiv; ++i)
    {
        // Soft: skip outer time edges (i==0 / i==timeDiv) - those were the L/R border.
        if (soft && (i == 0 || i == timeDiv))
            continue;
        const float x = (float) i / (float) timeDiv * 2.0f - 1.0f;
        const float a = (i == timeDiv && ! soft) ? 0.75f : 0.32f;
        const float half = (i == timeDiv && ! soft) ? kGridHalfW : kGridHalfW * 0.75f;
        pushRibbon (x, gridY, z0, x, gridY, z1, half, a, a, a + 0.02f);
    }

    // Outer frame - solid only. Soft: never draw a perimeter (NO BORDER).
    if (! soft)
    {
        pushRibbon (-1.0f, gridY, -1.0f, 1.0f, gridY, -1.0f, kGridHalfW, 0.7f, 0.72f, 0.75f);
        pushRibbon (1.0f, gridY, -1.0f, 1.0f, gridY, 1.0f, kGridHalfW, 0.7f, 0.72f, 0.75f);
        pushRibbon (1.0f, gridY, 1.0f, -1.0f, gridY, 1.0f, kGridHalfW, 0.7f, 0.72f, 0.75f);
        pushRibbon (-1.0f, gridY, 1.0f, -1.0f, gridY, -1.0f, kGridHalfW, 0.7f, 0.72f, 0.75f);
    }

    floorVertexCount = (int) verts.size();
    // #region agent log
    agentDbgLog ("J", "rebuildFloorGeometry", "floor_ribbons",
                 juce::String ("{\"totalVerts\":") + juce::String (floorVertexCount)
                     + ",\"gridHalfW\":" + juce::String (kGridHalfW, 5)
                     + ",\"softBg\":" + juce::String (soft ? 1 : 0)
                     + ",\"noPerimeter\":" + juce::String (soft ? 1 : 0)
                     + ",\"ticksInVbo\":" + juce::String (soft ? 0 : 1)
                     + "}");
    // #endregion
    if (floorVbo == 0)
        juce::gl::glGenBuffers (1, &floorVbo);

    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, floorVbo);
    juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                            (GLsizeiptr) (verts.size() * sizeof (Vertex)),
                            verts.data(), juce::gl::GL_DYNAMIC_DRAW);
    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::ensureLabelAtlas()
{
    if (labelAtlasReady || labelShader == nullptr)
        return;

    if (owner.labelAtlasDirty || ! owner.labelAtlasImage.isValid())
    {
        constexpr int cellW = 64;
        constexpr int cellH = 28;
        const int n = (int) owner.freqLabels.size();
        if (n <= 0)
            return;

        const int cols = 4;
        const int rows = (n + cols - 1) / cols;
        juce::Image img (juce::Image::ARGB, cols * cellW, rows * cellH, true);
        juce::Graphics g (img);
        g.setColour (juce::Colours::transparentBlack);
        g.fillAll();
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));

        for (int i = 0; i < n; ++i)
        {
            const int col = i % cols;
            const int row = i / cols;
            auto cell = juce::Rectangle<int> (col * cellW, row * cellH, cellW, cellH);
            // Soft drop-shadow under glyph (1px). Hypothesis E: can read as a second label.
            g.setColour (juce::Colours::black.withAlpha (0.75f));
            g.drawText (owner.freqLabels[(size_t) i].text, cell.translated (1, 1),
                        juce::Justification::centred, false);
            g.setColour (juce::Colours::white.withAlpha (0.92f));
            g.drawText (owner.freqLabels[(size_t) i].text, cell,
                        juce::Justification::centred, false);

            const float u0 = (float) (col * cellW) / (float) img.getWidth();
            const float v0 = 1.0f - (float) ((row + 1) * cellH) / (float) img.getHeight();
            const float u1 = (float) ((col + 1) * cellW) / (float) img.getWidth();
            const float v1 = 1.0f - (float) (row * cellH) / (float) img.getHeight();
            owner.freqLabels[(size_t) i].u0 = u0;
            owner.freqLabels[(size_t) i].v0 = v0;
            owner.freqLabels[(size_t) i].u1 = u1;
            owner.freqLabels[(size_t) i].v1 = v1;
        }

        owner.labelAtlasImage = std::move (img);
        owner.labelAtlasDirty = false;
        // #region agent log
        agentDbgLog ("E", "ensureLabelAtlas", "atlas_rebuilt_with_shadow",
                     juce::String ("{\"n\":") + juce::String (n)
                         + ",\"cellW\":64,\"cellH\":28,\"shadowPx\":1}");
        // #endregion
    }

    labelAtlas.loadImage (owner.labelAtlasImage);

    // Recompute UVs against the actual (possibly POT-padded) texture size.
    const float tw = (float) juce::jmax (1, labelAtlas.getWidth());
    const float th = (float) juce::jmax (1, labelAtlas.getHeight());
    constexpr int cellW = 64;
    constexpr int cellH = 28;
    constexpr int cols = 4;
    for (int i = 0; i < (int) owner.freqLabels.size(); ++i)
    {
        const int col = i % cols;
        const int row = i / cols;
        auto& lb = owner.freqLabels[(size_t) i];
        lb.u0 = (float) (col * cellW) / tw;
        lb.u1 = (float) ((col + 1) * cellW) / tw;
        lb.v1 = 1.0f - (float) (row * cellH) / th;
        lb.v0 = 1.0f - (float) ((row + 1) * cellH) / th;
    }

    labelAtlasReady = true;
}

juce::Rectangle<int> Spectrogram3DComponent::GlHost::getViewPixelBounds() const noexcept
{
    // Offline export may force exact pixel size (no live 1280 cap).
    if (exportForceW > 0 && exportForceH > 0)
        return { 0, 0, exportForceW, exportForceH };

    // Soft / docked-FBO: render at the framed view size (peer is a tiny context keeper).
    // Hard HWND: match this component's pixel size.
    const auto logical = owner.usesSoftComposite() ? owner.getGlViewLocal()
                                                   : getLocalBounds();
    const float scale = (float) openGLContext.getRenderingScale();
    int w = juce::jmax (1, juce::roundToInt ((float) logical.getWidth() * scale));
    int h = juce::jmax (1, juce::roundToInt ((float) logical.getHeight() * scale));

    // Cap soft-mode readback cost for live UI.
    constexpr int kMaxDim = 1280;
    const int maxSide = juce::jmax (w, h);
    if (owner.usesSoftComposite() && maxSide > kMaxDim)
    {
        const float s = (float) kMaxDim / (float) maxSide;
        w = juce::jmax (1, juce::roundToInt ((float) w * s));
        h = juce::jmax (1, juce::roundToInt ((float) h * s));
    }

    return { 0, 0, w, h };
}

bool Spectrogram3DComponent::GlHost::captureSoftFrameOnGlThread (int width, int height, juce::Image& outImage)
{
    if (! glReady || ! openGLContext.isAttached())
        return false;

    width = juce::jlimit (16, 7680, width);
    height = juce::jlimit (16, 4320, height);

    exportForceW = width;
    exportForceH = height;
    softContentDirty = true;
    // Fresh temporal history so export frames are not smeared by live SSGI.
    ssgiHistoryValid = false;
    ssgiMomentsValid = false;

    // Soft path is required for FBO readback.
    const bool wasSoft = owner.usesSoftComposite();
    juce::ignoreUnused (wasSoft);

    renderSoftComposite();

    exportForceW = 0;
    exportForceH = 0;

    const juce::ScopedLock sl (owner.softImageLock);
    if (! owner.softCompositeImage.isValid())
        return false;

    outImage = owner.softCompositeImage.createCopy();
    if (! outImage.isValid())
        return false;
    if (outImage.getWidth() != width || outImage.getHeight() != height)
        outImage = outImage.rescaled (width, height);
    return outImage.isValid();
}

juce::Matrix3D<float> Spectrogram3DComponent::GlHost::getProjectionMatrix() const
{
    constexpr float nearPlane = 0.05f;
    constexpr float farPlane = 80.0f;
    constexpr float fovHalfWAtNear = 1.0f / 1.5f;
    const auto view = owner.getGlViewLocal();
    const float aspect = (float) juce::jmax (1, view.getHeight())
                       / (float) juce::jmax (1, view.getWidth());
    const float w = nearPlane * fovHalfWAtNear;
    const float h = w * aspect;
    return juce::Matrix3D<float>::fromFrustum (-w, w, -h, h, nearPlane, farPlane);
}

juce::Matrix3D<float> Spectrogram3DComponent::GlHost::getViewMatrix() const
{
    // Freecam uses its own FPS view; orbit uses turntable. Fullscreen shares this.
    return owner.getActiveViewMatrix();
}

bool Spectrogram3DComponent::GlHost::projectWorldToNdc (float wx, float wy, float wz,
                                                       float& ndcX, float& ndcY, float& ndcZ) const
{
    const auto proj = getProjectionMatrix();
    const auto view = getViewMatrix();
    float v[4] = { wx, wy, wz, 1.0f };
    float eye[4] = {
        view.mat[0] * v[0] + view.mat[4] * v[1] + view.mat[8]  * v[2] + view.mat[12] * v[3],
        view.mat[1] * v[0] + view.mat[5] * v[1] + view.mat[9]  * v[2] + view.mat[13] * v[3],
        view.mat[2] * v[0] + view.mat[6] * v[1] + view.mat[10] * v[2] + view.mat[14] * v[3],
        view.mat[3] * v[0] + view.mat[7] * v[1] + view.mat[11] * v[2] + view.mat[15] * v[3]
    };
    float clip[4] = {
        proj.mat[0] * eye[0] + proj.mat[4] * eye[1] + proj.mat[8]  * eye[2] + proj.mat[12] * eye[3],
        proj.mat[1] * eye[0] + proj.mat[5] * eye[1] + proj.mat[9]  * eye[2] + proj.mat[13] * eye[3],
        proj.mat[2] * eye[0] + proj.mat[6] * eye[1] + proj.mat[10] * eye[2] + proj.mat[14] * eye[3],
        proj.mat[3] * eye[0] + proj.mat[7] * eye[1] + proj.mat[11] * eye[2] + proj.mat[15] * eye[3]
    };
    if (std::abs (clip[3]) < 1.0e-6f)
        return false;
    ndcX = clip[0] / clip[3];
    ndcY = clip[1] / clip[3];
    ndcZ = clip[2] / clip[3];
    return ndcX > -1.2f && ndcX < 1.2f && ndcY > -1.2f && ndcY < 1.2f
           && ndcZ >= -1.0f && ndcZ <= 1.0f;
}

int Spectrogram3DComponent::GlHost::effectiveMsaaSamples() const noexcept
{
    const int want = (int) owner.msaaLevel;
    if (want <= 0 || ! glReady)
        return 0;

    using namespace juce::gl;
    GLint maxSamples = 0;
    glGetIntegerv (GL_MAX_SAMPLES, &maxSamples);
    if (maxSamples <= 0)
        return 0;
    return juce::jmin (want, (int) maxSamples);
}

void Spectrogram3DComponent::GlHost::releaseSoftMsaaBuffers()
{
    using namespace juce::gl;
    if (softMsaaFbo != 0) { glDeleteFramebuffers (1, &softMsaaFbo); softMsaaFbo = 0; }
    if (softMsaaColorRb != 0) { glDeleteRenderbuffers (1, &softMsaaColorRb); softMsaaColorRb = 0; }
    if (softMsaaDepthRb != 0) { glDeleteRenderbuffers (1, &softMsaaDepthRb); softMsaaDepthRb = 0; }
    softMsaaW = softMsaaH = softMsaaSamples = 0;
}

void Spectrogram3DComponent::GlHost::ensureSoftMsaaBuffers (int width, int height, int samples)
{
    using namespace juce::gl;
    width = juce::jmax (1, width);
    height = juce::jmax (1, height);
    samples = juce::jmax (2, samples);

    if (softMsaaFbo != 0 && softMsaaW == width && softMsaaH == height && softMsaaSamples == samples)
        return;

    releaseSoftMsaaBuffers();

    glGenFramebuffers (1, &softMsaaFbo);
    glGenRenderbuffers (1, &softMsaaColorRb);
    glGenRenderbuffers (1, &softMsaaDepthRb);
    if (softMsaaFbo == 0 || softMsaaColorRb == 0 || softMsaaDepthRb == 0)
    {
        releaseSoftMsaaBuffers();
        return;
    }

    glBindRenderbuffer (GL_RENDERBUFFER, softMsaaColorRb);
    glRenderbufferStorageMultisample (GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    glBindRenderbuffer (GL_RENDERBUFFER, softMsaaDepthRb);
    glRenderbufferStorageMultisample (GL_RENDERBUFFER, samples, GL_DEPTH_COMPONENT24, width, height);

    glBindFramebuffer (GL_FRAMEBUFFER, softMsaaFbo);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, softMsaaColorRb);
    glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, softMsaaDepthRb);
    const bool ok = (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindRenderbuffer (GL_RENDERBUFFER, 0);

    if (! ok)
    {
        releaseSoftMsaaBuffers();
        return;
    }

    softMsaaW = width;
    softMsaaH = height;
    softMsaaSamples = samples;
}

void Spectrogram3DComponent::GlHost::uploadHeightMap (const std::vector<Vertex>& verts, int w, int h)
{
    using namespace juce::gl;
    if (w < 2 || h < 2 || (int) verts.size() < w * h)
        return;

    // RGBA8 - universally sampleable on host GL drivers (VST wrappers often choke on R32F).
    std::vector<juce::uint8> heights ((size_t) w * (size_t) h * 4u);
    const float invH = 1.0f / juce::jmax (1.0e-5f, owner.meshHeight);
    for (int z = 0; z < h; ++z)
    {
        for (int x = 0; x < w; ++x)
        {
            const float n = juce::jlimit (0.0f, 1.0f,
                                         verts[(size_t) z * (size_t) w + (size_t) x].y * invH);
            const auto b = (juce::uint8) juce::jlimit (0, 255, juce::roundToInt (n * 255.0f));
            const size_t i = ((size_t) z * (size_t) w + (size_t) x) * 4u;
            heights[i] = b;
            heights[i + 1] = b;
            heights[i + 2] = b;
            heights[i + 3] = 255;
        }
    }

    if (heightMapTex == 0)
        glGenTextures (1, &heightMapTex);
    if (heightMapTex == 0)
        return;

    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glBindTexture (GL_TEXTURE_2D, heightMapTex);
    // NEAREST keeps ridge edges sharp for horizon / ray-march occlusion tests.
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, heights.data());
    glBindTexture (GL_TEXTURE_2D, 0);
    heightMapW = w;
    heightMapH = h;
}

void Spectrogram3DComponent::GlHost::bindHeightMapForMesh() const
{
    using namespace juce::gl;
    if (colourHeightMapUniform == nullptr)
        return;

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, heightMapTex);
    colourHeightMapUniform->set (1);
    glActiveTexture (GL_TEXTURE0);
}

void Spectrogram3DComponent::GlHost::uploadDomeTextureIfNeeded()
{
    if (! owner.domeTextureDirty.exchange (false))
        return;

    juce::Image img;
    {
        const juce::ScopedLock lock (owner.domeTextureLock);
        img = owner.domeTextureImage;
    }

    if (! img.isValid())
    {
        domeMapTex.release();
        domeMapReady = false;
        return;
    }

    // Cap upload size for VST host GL drivers.
    constexpr int kMaxW = 2048;
    constexpr int kMaxH = 1024;
    if (img.getWidth() > kMaxW || img.getHeight() > kMaxH)
        img = img.rescaled (juce::jmin (img.getWidth(), kMaxW),
                            juce::jmin (img.getHeight(), kMaxH));

    domeMapTex.loadImage (img);
    domeMapReady = true;
}

void Spectrogram3DComponent::GlHost::bindDomeTextureForMesh() const
{
    using namespace juce::gl;
    if (colourDomeMapUniform == nullptr)
        return;

    glActiveTexture (GL_TEXTURE2);
    if (domeMapReady)
        domeMapTex.bind();
    else
        glBindTexture (GL_TEXTURE_2D, 0);
    colourDomeMapUniform->set (2);
    glActiveTexture (GL_TEXTURE0);
}

void Spectrogram3DComponent::GlHost::unbindDomeTexture() const
{
    using namespace juce::gl;
    glActiveTexture (GL_TEXTURE2);
    glBindTexture (GL_TEXTURE_2D, 0);
    glActiveTexture (GL_TEXTURE0);
}

void Spectrogram3DComponent::GlHost::ensureSoftFrameBuffer (int width, int height)
{
    using namespace juce::gl;

    width = juce::jmax (1, width);
    height = juce::jmax (1, height);
    if (softFbo.isValid() && softFbo.getWidth() == width && softFbo.getHeight() == height
        && softFboW == width && softFboH == height && softDepthTex != 0)
        return;

    if (softDepthTex != 0)
    {
        glDeleteTextures (1, &softDepthTex);
        softDepthTex = 0;
    }

    softFbo.release();
    if (! softFbo.initialise (openGLContext, width, height))
    {
        softFboW = softFboH = 0;
        return;
    }

    // Depth as texture so SSAO can sample it.
    softFbo.makeCurrentRenderingTarget();
    glGenTextures (1, &softDepthTex);
    glBindTexture (GL_TEXTURE_2D, softDepthTex);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, width, height, 0,
                  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Critical: default compare-mode makes texture() return 0/1, not raw depth.
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, softDepthTex, 0);
    glBindTexture (GL_TEXTURE_2D, 0);
    const bool complete = (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    softFbo.releaseAsRenderingTarget();

    if (softDepthTex == 0 || ! complete)
    {
        if (softDepthTex != 0)
        {
            glDeleteTextures (1, &softDepthTex);
            softDepthTex = 0;
        }
        softFbo.release();
        softFboW = softFboH = 0;
        return;
    }

    softFboW = width;
    softFboH = height;
    softContentDirty = true;
}

void Spectrogram3DComponent::GlHost::ensurePostFrameBuffers (int width, int height)
{
    // Full-resolution ping-pong targets (avoids sampling a bound FBO colour attachment).
    width = juce::jmax (1, width);
    height = juce::jmax (1, height);
    if (postFboA.isValid() && postFboB.isValid()
        && postFboW == width && postFboH == height)
        return;

    postFboA.release();
    postFboB.release();
    if (! postFboA.initialise (openGLContext, width, height)
        || ! postFboB.initialise (openGLContext, width, height))
    {
        postFboA.release();
        postFboB.release();
        postFboW = postFboH = 0;
        return;
    }
    postFboW = width;
    postFboH = height;
}

void Spectrogram3DComponent::GlHost::releasePostFrameBuffers()
{
    postFboA.release();
    postFboB.release();
    ssgiHalfFbo.release();
    ssgiHistoryFbo.release();
    ssgiMomentsFbo.release();
    ssgiNormalsFbo.release();
    postFboW = postFboH = 0;
    ssgiHalfW = ssgiHalfH = 0;
    ssgiHistoryW = ssgiHistoryH = 0;
    ssgiMomentsW = ssgiMomentsH = 0;
    ssgiNormalsW = ssgiNormalsH = 0;
    ssgiHistoryValid = false;
    ssgiMomentsValid = false;
}

void Spectrogram3DComponent::GlHost::ensureSsgiSupportBuffers (int width, int height,
                                                              bool halfRes, bool needHistory,
                                                              bool needNormals, bool needMoments)
{
    width = juce::jmax (1, width);
    height = juce::jmax (1, height);

    if (halfRes)
    {
        const int hw = juce::jmax (1, width / 2);
        const int hh = juce::jmax (1, height / 2);
        if (! ssgiHalfFbo.isValid() || ssgiHalfW != hw || ssgiHalfH != hh)
        {
            ssgiHalfFbo.release();
            if (ssgiHalfFbo.initialise (openGLContext, hw, hh))
            {
                ssgiHalfW = hw;
                ssgiHalfH = hh;
            }
            else
            {
                ssgiHalfW = ssgiHalfH = 0;
            }
        }
    }

    if (needHistory)
    {
        if (! ssgiHistoryFbo.isValid() || ssgiHistoryW != width || ssgiHistoryH != height)
        {
            ssgiHistoryFbo.release();
            ssgiHistoryValid = false;
            if (ssgiHistoryFbo.initialise (openGLContext, width, height))
            {
                ssgiHistoryW = width;
                ssgiHistoryH = height;
            }
            else
            {
                ssgiHistoryW = ssgiHistoryH = 0;
            }
        }
    }

    if (needMoments)
    {
        if (! ssgiMomentsFbo.isValid() || ssgiMomentsW != width || ssgiMomentsH != height)
        {
            ssgiMomentsFbo.release();
            ssgiMomentsValid = false;
            if (ssgiMomentsFbo.initialise (openGLContext, width, height))
            {
                ssgiMomentsW = width;
                ssgiMomentsH = height;
            }
            else
            {
                ssgiMomentsW = ssgiMomentsH = 0;
            }
        }
    }

    if (needNormals)
    {
        if (! ssgiNormalsFbo.isValid() || ssgiNormalsW != width || ssgiNormalsH != height)
        {
            ssgiNormalsFbo.release();
            if (ssgiNormalsFbo.initialise (openGLContext, width, height))
            {
                ssgiNormalsW = width;
                ssgiNormalsH = height;
            }
            else
            {
                ssgiNormalsW = ssgiNormalsH = 0;
            }
        }
    }
}

void Spectrogram3DComponent::GlHost::drawSoftTint()
{
    using namespace juce::gl;
    if (tintShader == nullptr || tintVbo == 0)
        return;

    const float verts[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };

    tintShader->use();
    const auto tint = owner.getClearColour();
    if (tintColourUniform != nullptr)
        tintColourUniform->set (tint.getFloatRed(), tint.getFloatGreen(),
                                tint.getFloatBlue(), tint.getFloatAlpha());

    glDisable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable (GL_CULL_FACE);

    glBindBuffer (GL_ARRAY_BUFFER, tintVbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_STREAM_DRAW);

    if (tintPositionAttrib != nullptr && tintPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) tintPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) tintPositionAttrib->attributeID, 2, GL_FLOAT, GL_FALSE,
                               (GLsizei) (2 * sizeof (float)), nullptr);
    }

    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);

    if (tintPositionAttrib != nullptr && tintPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) tintPositionAttrib->attributeID);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glDisable (GL_BLEND);
    glDepthMask (GL_TRUE);
    glEnable (GL_DEPTH_TEST);
}

void Spectrogram3DComponent::GlHost::readbackSoftImage (int width, int height)
{
    if (width < 1 || height < 1 || ! softFbo.isValid())
        return;

    const size_t n = (size_t) width * (size_t) height;
    if (softReadbackPixels.size() != n)
        softReadbackPixels.resize (n);

    if (! softFbo.readPixels (softReadbackPixels.data(),
                              { 0, 0, width, height },
                              juce::OpenGLFrameBuffer::RowOrder::fromTopDown))
        return;

    // Write into the back buffer (never the image paint may still be drawing).
    if (! owner.softCompositeBack.isValid()
        || owner.softCompositeBack.getWidth() != width
        || owner.softCompositeBack.getHeight() != height)
    {
        owner.softCompositeBack = juce::Image (juce::Image::ARGB, width, height, true,
                                               juce::SoftwareImageType());
    }

    {
        juce::Image::BitmapData bd (owner.softCompositeBack, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < height; ++y)
            std::memcpy (bd.getLinePointer (y),
                         softReadbackPixels.data() + (size_t) y * (size_t) width,
                         (size_t) width * sizeof (juce::PixelARGB));
    }

    {
        const juce::ScopedLock sl (owner.softImageLock);
        std::swap (owner.softCompositeImage, owner.softCompositeBack);
    }

    softContentDirty = false;

    juce::Component::SafePointer<Spectrogram3DComponent> safe (&owner);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe != nullptr)
            safe->repaint();
    });
}

void Spectrogram3DComponent::GlHost::renderSoftComposite()
{
    using namespace juce::gl;

    const auto px = getViewPixelBounds();
    const int w = px.getWidth();
    const int h = px.getHeight();
    if (w < 2 || h < 2)
        return;

    const bool needsDraw = softContentDirty || meshNeedsUpload || owner.meshNeedsUpload
                           || softFboW != w || softFboH != h || ! softFbo.isValid();
    {
        const juce::ScopedLock sl (owner.softImageLock);
        if (! needsDraw && owner.softCompositeImage.isValid())
            return;
    }

    ensureSoftFrameBuffer (w, h);
    if (! softFbo.isValid())
        return;

    uploadMeshIfNeeded();
    ensureFloorGeometry();
    ensureLabelAtlas();

    // Shadow atlas before soft colour target (must not leave soft FBO unbound).
    renderShadowDepthPass();

    const int samples = effectiveMsaaSamples();
    const bool useMsaa = samples >= 2;
    if (useMsaa)
        ensureSoftMsaaBuffers (w, h, samples);

    if (useMsaa && softMsaaFbo != 0)
    {
        glBindFramebuffer (GL_FRAMEBUFFER, softMsaaFbo);
        glViewport (0, 0, w, h);
        // Transparent clear, then a single soft plate (drawSoftTint) so DOF edge
        // dilate/spill (modes 17->19->6) can blur OOF mesh into Soft BG void colour.
        // Paint must not pre-fill a second translucent plate over this image.
        glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_MULTISAMPLE);
    }
    else
    {
        softFbo.makeCurrentAndClear();
        glViewport (0, 0, w, h);
        glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    uploadDomeTextureIfNeeded();
    // Soft plate in FBO (colour + alpha in the sky void). Required for DOF
    // silhouette soft into Soft BG; paint skips a second soft fill when Soft BG
    // is on so Osc/Gon-style double-tint does not return.
    drawSoftTint();
    // #region agent log
    gLabelDrawCallsThisFrame = 0;
    ++gSoftFrameCounter;
    // #endregion
    // Mesh + lookdev sphere + gizmo into SSGI/SSR (gizmo bounce is intentional).
    // Grid / ticks / labels stay deferred - bright chrome must not seed GI/bloom.
    drawContactShadow();
    drawSpectrogramSurface();
    drawDebugSphere();
    drawDebugGizmo();
    const bool postStack = owner.needsPostEffects();
    if (! postStack)
    {
        drawGroundAndGrid();
        drawPlayheadTicks();
        drawFrequencyLabels();
    }
    // #region agent log
    if ((gSoftFrameCounter % 30) == 1)
    {
        agentDbgLog ("H", "renderSoftComposite", "soft_frame_draw",
                     juce::String ("{\"frame\":") + juce::String (gSoftFrameCounter)
                         + ",\"labelDrawCalls\":" + juce::String (gLabelDrawCallsThisFrame)
                         + ",\"freqLabelCount\":" + juce::String ((int) owner.freqLabels.size())
                         + ",\"floorVerts\":" + juce::String (floorVertexCount)
                         + ",\"ssgi\":" + juce::String (owner.ssgiEnabled ? 1 : 0)
                         + ",\"ssgiStr\":" + juce::String (owner.ssgiStrength, 3)
                         + ",\"overlaysDeferred\":" + juce::String (postStack ? 1 : 0)
                         + ",\"ssr\":" + juce::String (owner.ssrEnabled ? 1 : 0)
                         + ",\"dof\":" + juce::String (owner.dofEnabled ? 1 : 0)
                         + ",\"bloom\":" + juce::String (owner.bloomEnabled ? 1 : 0)
                         + ",\"audioLvl\":" + juce::String (owner.audioLevelLive01, 3)
                         + "}");
    }
    // #endregion

    if (useMsaa && softMsaaFbo != 0)
    {
        const GLuint destFbo = (GLuint) softFbo.getFrameBufferID();
        glBindFramebuffer (GL_READ_FRAMEBUFFER, softMsaaFbo);
        glBindFramebuffer (GL_DRAW_FRAMEBUFFER, destFbo);
        glBlitFramebuffer (0, 0, w, h, 0, 0, w, h,
                           GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        softFbo.makeCurrentRenderingTarget();
        glViewport (0, 0, w, h);
    }

    if (postStack)
        applySsaoAndBloom (w, h);

    readbackSoftImage (w, h);
    softFbo.releaseAsRenderingTarget();
}

void Spectrogram3DComponent::GlHost::setLightingUniforms (juce::OpenGLShaderProgram& program) const
{
    const float amt = owner.lightingEnabled ? owner.lightingAmount : 0.0f;
    auto lightWorld = getLightDirectionWorld();
    // Transform light direction into view space (rotation only).
    const auto view = getViewMatrix();
    const float lx = view.mat[0] * lightWorld.x + view.mat[4] * lightWorld.y + view.mat[8]  * lightWorld.z;
    const float ly = view.mat[1] * lightWorld.x + view.mat[5] * lightWorld.y + view.mat[9]  * lightWorld.z;
    const float lz = view.mat[2] * lightWorld.x + view.mat[6] * lightWorld.y + view.mat[10] * lightWorld.z;
    const float len = juce::jmax (1.0e-5f, std::sqrt (lx * lx + ly * ly + lz * lz));

    // Floor-plane light bearing + capped sun tan for horizon shadows.
    const float horiz = std::sqrt (lightWorld.x * lightWorld.x + lightWorld.z * lightWorld.z);
    float shadowDirX = 1.0f, shadowDirZ = 0.0f;
    float sunTan = 0.45f;
    if (horiz > 1.0e-4f)
    {
        shadowDirX = lightWorld.x / horiz;
        shadowDirZ = lightWorld.z / horiz;
        sunTan = juce::jlimit (0.08f, 0.75f, lightWorld.y / horiz);
    }

    // Shadows follow the key light - disabled when Lighting is off.
    const float selfShadow = (owner.lightingEnabled && owner.selfShadowEnabled)
                                 ? owner.selfShadowStrength : 0.0f;
    const float aoAmt = owner.ssaoEnabled ? owner.ssaoStrength : 0.0f;
    const float contact = (owner.lightingEnabled && owner.contactShadowEnabled)
                              ? owner.contactShadowStrength : 0.0f;

    auto setF1 = [&program] (juce::OpenGLShaderProgram::Uniform* u, const char* name, float v)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->set (v);
        else
            program.setUniform (name, v);
    };
    auto setF2 = [&program] (juce::OpenGLShaderProgram::Uniform* u, const char* name, float a, float b)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->set (a, b);
        else
            program.setUniform (name, a, b);
    };
    auto setF3 = [&program] (juce::OpenGLShaderProgram::Uniform* u, const char* name, float a, float b, float c)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->set (a, b, c);
        else
            program.setUniform (name, a, b, c);
    };

    setF3 (colourLightDirUniform.get(), "uLightDirView", lx / len, ly / len, lz / len);
    setF1 (colourLightingAmountUniform.get(), "uLightingAmount", amt);
    setF1 (colourSpecularUniform.get(), "uSpecular", owner.specularAmount);
    setF1 (colourRoughnessUniform.get(), "uRoughness", owner.roughnessAmount);
    setF1 (colourMetalnessUniform.get(), "uMetalness", owner.metalnessAmount);
    setF1 (colourEnergyConserveUniform.get(), "uEnergyConserve",
           owner.energyConservingEnabled ? 1.0f : 0.0f);
    setF3 (colourLightColourUniform.get(), "uLightColour",
           owner.lightColour.getFloatRed(), owner.lightColour.getFloatGreen(),
           owner.lightColour.getFloatBlue());
    setF3 (colourRimColourUniform.get(), "uRimColour",
           owner.rimColour.getFloatRed(), owner.rimColour.getFloatGreen(),
           owner.rimColour.getFloatBlue());
    setF1 (colourDomeStrengthUniform.get(), "uDomeStrength",
           (owner.lightingEnabled && owner.domeFillEnabled) ? owner.domeFillStrength : 0.0f);
    setF3 (colourDomeSkyUniform.get(), "uDomeSky",
           owner.domeSkyColour.getFloatRed(), owner.domeSkyColour.getFloatGreen(),
           owner.domeSkyColour.getFloatBlue());
    setF3 (colourDomeGroundUniform.get(), "uDomeGround",
           owner.domeGroundColour.getFloatRed(), owner.domeGroundColour.getFloatGreen(),
           owner.domeGroundColour.getFloatBlue());
    setF1 (colourDomeUseTexUniform.get(), "uDomeUseTex",
           (owner.domeTextureEnabled && domeMapReady) ? 1.0f : 0.0f);
    setF1 (colourRimUniform.get(), "uRim", owner.rimAmount);
    setF3 (colourLightDirWorldUniform.get(), "uLightDirWorld", lightWorld.x, lightWorld.y, lightWorld.z);
    setF1 (colourSelfShadowUniform.get(), "uSelfShadow", selfShadow);
    setF1 (colourMeshHeightUniform.get(), "uMeshHeight", owner.meshHeight);
    setF1 (colourReverseFreqUniform.get(), "uReverseFreq", owner.reverseFrequencyAxis ? 1.0f : 0.0f);
    setF1 (colourFreqBiasBUniform.get(), "uFreqBiasB", owner.freqMeshBiasB());
    setF1 (colourFreqBiasPivotUniform.get(), "uFreqBiasPivot", owner.getFreqMeshBiasPivot());
    setF1 (colourAoAmountUniform.get(), "uAoAmount", aoAmt);
    setF1 (colourAoRadiusUniform.get(), "uAoRadius", owner.ssaoRadius);
    setF2 (colourShadowDirXZUniform.get(), "uShadowDirXZ", shadowDirX, shadowDirZ);
    setF1 (colourShadowSunTanUniform.get(), "uShadowSunTan", sunTan);
    setF1 (colourShadowBiasUniform.get(), "uShadowBias", owner.selfShadowBias);
    setF1 (colourShadowSoftnessUniform.get(), "uShadowSoftness", owner.selfShadowSoftness);
    setF1 (colourShadowQualityUniform.get(), "uShadowQuality",
           owner.selfShadowQuality == ShadowQuality::low ? 0.0f
               : (owner.selfShadowQuality == ShadowQuality::high ? 2.0f : 1.0f));
    setF1 (colourContactUniform.get(), "uContactShadow", contact);

    const bool sssOn = owner.lightingEnabled && owner.sssEnabled;
    const float sssModeF = ! sssOn ? 0.0f
                           : (owner.closedMeshEnabled ? 2.0f : 1.0f);
    setF1 (colourSssModeUniform.get(), "uSssMode", sssModeF);
    setF1 (colourSssStrengthUniform.get(), "uSssStrength", sssOn ? owner.sssStrength : 0.0f);
    setF1 (colourSssWrapUniform.get(), "uSssWrap", owner.sssWrap);
    setF1 (colourSssTransmissionUniform.get(), "uSssTransmission", owner.sssTransmission);
    setF3 (colourSssTintUniform.get(), "uSssTint",
           owner.sssTint.getFloatRed(), owner.sssTint.getFloatGreen(), owner.sssTint.getFloatBlue());
    setF1 (colourSssRadiusUniform.get(), "uSssRadius", owner.sssRadius);
    setF1 (colourSssContrastUniform.get(), "uSssContrast", owner.sssContrast);
    setF1 (colourSssQualityUniform.get(), "uSssQuality",
           owner.sssQuality == ShadowQuality::low ? 0.0f
               : (owner.sssQuality == ShadowQuality::high ? 2.0f : 1.0f));
    setF1 (colourSssBaseDepthUniform.get(), "uClosedFloorY",
           owner.closedMeshEnabled ? -kClosedMeshFloorBias : 0.0f);
    setF1 (colourSssThickScaleUniform.get(), "uSssThickScale", owner.sssThicknessScale);
    setF1 (colourSssMaxThickUniform.get(), "uSssMaxThick", owner.sssMaxThickness);

    const bool audioOn = owner.audioLevelModEnabled;
    setF1 (colourAudioLevelUniform.get(), "uAudioLevel",
           audioOn ? juce::jlimit (0.0f, 1.0f, owner.audioLevelLive01) : 0.0f);
    setF1 (colourAudioMinUniform.get(), "uAudioMin",
           audioOn ? owner.audioLevelMinPercent * 0.01f : 0.0f);
    setF1 (colourAudioMaxUniform.get(), "uAudioMax",
           audioOn ? owner.audioLevelMaxPercent * 0.01f : 0.0f);

    // Decode target on CPU so brightness can never accidentally gate lighting amount.
    using ALT = Spectrogram3DComponent::AudioLevelTarget;
    const auto tgt = owner.audioLevelTarget;
    const bool modBright = audioOn && (tgt == ALT::brightness || tgt == ALT::brightnessAndLights);
    const bool modLitAmt = audioOn && (tgt == ALT::lightingAmount || tgt == ALT::allLights
                                       || tgt == ALT::brightnessAndLights);
    const bool modSpec = audioOn && (tgt == ALT::specular || tgt == ALT::allLights
                                     || tgt == ALT::brightnessAndLights);
    const bool modRim = audioOn && (tgt == ALT::rim || tgt == ALT::allLights
                                    || tgt == ALT::brightnessAndLights);
    const bool modDome = audioOn && (tgt == ALT::domeFill || tgt == ALT::allLights
                                     || tgt == ALT::brightnessAndLights);
    setF1 (colourAudioModBrightUniform.get(), "uAudioModBright", modBright ? 1.0f : 0.0f);
    setF1 (colourAudioModLitAmtUniform.get(), "uAudioModLitAmt", modLitAmt ? 1.0f : 0.0f);
    setF1 (colourAudioModSpecUniform.get(), "uAudioModSpec", modSpec ? 1.0f : 0.0f);
    setF1 (colourAudioModRimUniform.get(), "uAudioModRim", modRim ? 1.0f : 0.0f);
    setF1 (colourAudioModDomeUniform.get(), "uAudioModDome", modDome ? 1.0f : 0.0f);
    setF1 (colourAudioAffectPlayheadUniform.get(), "uAudioAffectPlayhead",
           owner.audioLevelAffectPlayhead ? 1.0f : 0.0f);
    setF1 (colourAudioAffectAntiUniform.get(), "uAudioAffectAnti",
           owner.audioLevelAffectAntiPlayhead ? 1.0f : 0.0f);
    setF1 (colourPlayheadWallXUniform.get(), "uPlayheadWallX",
           1.0f + kClosedPlayheadWallBias);
    setF1 (colourAntiPlayheadWallXUniform.get(), "uAntiPlayheadWallX",
           -1.0f - kClosedWaterfallEndWallBias);

    setCastShadowUniforms (program);
    setMaterialOverrideUniforms (false, juce::Colours::white, 0.45f, 0.0f);
}

void Spectrogram3DComponent::GlHost::setCastShadowUniforms (juce::OpenGLShaderProgram& program) const
{
    const bool on = owner.lightingEnabled && owner.castShadowsEnabled
                    && shadowDepthTex != 0 && cascadesBuilt > 0;
    auto setF1 = [&program] (juce::OpenGLShaderProgram::Uniform* u, const char* name, float v)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->set (v);
        else
            program.setUniform (name, v);
    };
    auto setMat = [] (juce::OpenGLShaderProgram::Uniform* u, const juce::Matrix3D<float>& m)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->setMatrix4 (m.mat, 1, false);
    };
    juce::ignoreUnused (program);

    setF1 (colourCastShadowUniform.get(), "uCastShadow", on ? 1.0f : 0.0f);
    setF1 (colourCascadeCountUniform.get(), "uCascadeCount", (float) juce::jmax (1, cascadesBuilt));
    setF1 (colourShadowAtlasTilesUniform.get(), "uShadowAtlasTiles",
           (float) juce::jmax (1, shadowAtlasCascades));
    setF1 (colourCastBiasUniform.get(), "uCastBias", owner.selfShadowBias);
    setF1 (colourCastSoftnessUniform.get(), "uCastSoftness", owner.selfShadowSoftness);
    setF1 (colourCascadeTransitionUniform.get(), "uCascadeTransition",
           owner.shadowCascadeTransitionFraction);
    setF1 (colourViewZUniform.get(), "uViewZScale", 1.0f);

    const float s0 = cascadeSplitFar[0], s1 = cascadeSplitFar[1];
    const float s2 = cascadeSplitFar[2], s3 = cascadeSplitFar[3];
    if (colourCascadeSplitsUniform != nullptr && colourCascadeSplitsUniform->uniformID >= 0)
        colourCascadeSplitsUniform->set (s0, s1, s2, s3);
    else
        program.setUniform ("uCascadeSplits", s0, s1, s2, s3);

    setMat (colourShadowMatrix0Uniform.get(), shadowMatrix[0]);
    setMat (colourShadowMatrix1Uniform.get(), shadowMatrix[1]);
    setMat (colourShadowMatrix2Uniform.get(), shadowMatrix[2]);
    setMat (colourShadowMatrix3Uniform.get(), shadowMatrix[3]);

    if (on)
        bindShadowAtlasForMesh();
    else if (colourShadowAtlasUniform != nullptr)
        colourShadowAtlasUniform->set (3);
}

void Spectrogram3DComponent::GlHost::setMaterialOverrideUniforms (bool enabled, juce::Colour albedo,
                                                                 float roughness, float metalness,
                                                                 float specular) const
{
    if (colourShader == nullptr)
        return;
    auto& program = *colourShader;
    auto setF1 = [&program] (juce::OpenGLShaderProgram::Uniform* u, const char* name, float v)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->set (v);
        else
            program.setUniform (name, v);
    };
    auto setF3 = [&program] (juce::OpenGLShaderProgram::Uniform* u, const char* name,
                             float a, float b, float c)
    {
        if (u != nullptr && u->uniformID >= 0)
            u->set (a, b, c);
        else
            program.setUniform (name, a, b, c);
    };
    setF1 (colourMatOverrideUniform.get(), "uMatOverride", enabled ? 1.0f : 0.0f);
    setF3 (colourMatAlbedoUniform.get(), "uMatAlbedo",
           albedo.getFloatRed(), albedo.getFloatGreen(), albedo.getFloatBlue());
    setF1 (colourMatRoughUniform.get(), "uMatRoughness", roughness);
    setF1 (colourMatMetalUniform.get(), "uMatMetalness", metalness);
    setF1 (colourMatSpecularUniform.get(), "uMatSpecular", specular);
}

void Spectrogram3DComponent::GlHost::bindShadowAtlasForMesh() const
{
    using namespace juce::gl;
    if (colourShadowAtlasUniform == nullptr)
        return;
    glActiveTexture (GL_TEXTURE3);
    glBindTexture (GL_TEXTURE_2D, shadowDepthTex);
    colourShadowAtlasUniform->set (3);
    glActiveTexture (GL_TEXTURE0);
}

juce::Vector3D<float> Spectrogram3DComponent::GlHost::getLightDirectionWorld() const noexcept
{
    const float az = juce::degreesToRadians (owner.lightAzimuthDeg);
    const float el = juce::degreesToRadians (juce::jlimit (5.0f, 89.0f, owner.lightElevationDeg));
    // Direction toward the light (from surface).
    return { std::cos (el) * std::sin (az),
             std::sin (el),
             std::cos (el) * std::cos (az) };
}

void Spectrogram3DComponent::GlHost::drawGroundAndGrid()
{
    using namespace juce::gl;
    if (floorVbo == 0 || floorVertexCount <= 0 || colourShader == nullptr)
        return;

    colourShader->use();
    setCornerUniforms (*colourShader);
    setLightingUniforms (*colourShader);
    bindDomeTextureForMesh();
    if (colourLightingAmountUniform != nullptr)
        colourLightingAmountUniform->set (0.0f); // grid stays unlit
    if (colourSelfShadowUniform != nullptr)
        colourSelfShadowUniform->set (0.0f); // floor/grid: no heightfield shadow
    if (colourAoAmountUniform != nullptr)
        colourAoAmountUniform->set (0.0f);
    if (colourContactUniform != nullptr)
        colourContactUniform->set (0.0f);
    if (colourAudioLevelUniform != nullptr)
        colourAudioLevelUniform->set (0.0f); // floor/grid: no audio pulse
    if (colourAudioMinUniform != nullptr)
        colourAudioMinUniform->set (0.0f);
    if (colourAudioMaxUniform != nullptr)
        colourAudioMaxUniform->set (0.0f);
    if (colourAudioModBrightUniform != nullptr)
        colourAudioModBrightUniform->set (0.0f);
    if (colourAudioModLitAmtUniform != nullptr)
        colourAudioModLitAmtUniform->set (0.0f);
    if (colourAudioModSpecUniform != nullptr)
        colourAudioModSpecUniform->set (0.0f);
    if (colourAudioModRimUniform != nullptr)
        colourAudioModRimUniform->set (0.0f);
    if (colourAudioModDomeUniform != nullptr)
        colourAudioModDomeUniform->set (0.0f);
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (getViewMatrix().mat, 1, false);

    glDisable (GL_CULL_FACE);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glBindBuffer (GL_ARRAY_BUFFER, floorVbo);

    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourNormalAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 6));
    }

    // Ground plane + grid ribbons (all triangles - ribbons replace GL_LINES for clean DOF).
    if (floorVertexCount > 0)
        glDrawArrays (GL_TRIANGLES, 0, floorVertexCount);

    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
    unbindDomeTexture();
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::drawPlayheadTicks()
{
    using namespace juce::gl;
    if (! owner.usesSoftComposite() || colourShader == nullptr || labelVbo == 0)
        return;

    const double sr = floorGridSr > 1.0 ? floorGridSr : 48000.0;
    const float nyquist = (float) (sr * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);

    struct V { float x, y, z, r, g, b, nx, ny, nz; };
    std::vector<V> quads;
    quads.reserve (80);

    const auto viewRect = owner.getGlViewLocal();
    const int viewW = juce::jmax (1, viewRect.getWidth());
    const int viewH = juce::jmax (1, viewRect.getHeight());
    constexpr float kTanHalfW = 1.0f / 1.5f;
    const float tanHalfH = kTanHalfW * ((float) viewH / (float) viewW);
    // ~1.5 px wide, ~9 px tall - screen-constant so MSAA/resolve edges stay thin.
    const float ndcHalfW = 0.75f / (float) viewW;
    const float ndcHalfH = 4.5f / (float) viewH;

    juce::Vector3D<float> right, camUp, forward;
    owner.cameraBasis (right, camUp, forward);
    juce::ignoreUnused (camUp);
    const juce::Vector3D<float> up { 0.0f, 1.0f, 0.0f };
    const auto viewMat = getViewMatrix();

    constexpr float kTickX = 1.022f;
    constexpr float kTickY = -0.002f;
    constexpr float kGridY = -0.010f;

    for (float hz : kMajorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float az = owner.worldZForFreq (hz, sr, floorGridLog);
        const float ax = kTickX;
        const float ay = kTickY;

        const float eyeZ = viewMat.mat[2] * ax + viewMat.mat[6] * ay + viewMat.mat[10] * az + viewMat.mat[14];
        const float viewDist = juce::jmax (0.05f, -eyeZ);
        const float halfW = ndcHalfW * kTanHalfW * viewDist;
        const float halfH = ndcHalfH * tanHalfH * viewDist;

        const float fXZ = juce::jmax (1.0e-6f,
                                      std::sqrt (forward.x * forward.x + forward.z * forward.z));
        constexpr float bias = 0.014f;
        const float cx = ax - forward.x / fXZ * bias;
        const float cy = juce::jmax (kGridY + 0.001f, ay);
        const float cz = az - forward.z / fXZ * bias;

        auto corner = [&] (float sx, float sy) -> V
        {
            return { cx + right.x * sx * halfW + up.x * sy * halfH,
                     cy + right.y * sx * halfW + up.y * sy * halfH,
                     cz + right.z * sx * halfW + up.z * sy * halfH,
                     0.78f, 0.80f, 0.84f, 0.0f, 1.0f, 0.0f };
        };

        // sy -1..+1: tick sits mostly above the grid line.
        const V c00 = corner (-1.0f, -0.15f);
        const V c10 = corner ( 1.0f, -0.15f);
        const V c11 = corner ( 1.0f,  1.0f);
        const V c01 = corner (-1.0f,  1.0f);
        quads.push_back (c00);
        quads.push_back (c10);
        quads.push_back (c11);
        quads.push_back (c00);
        quads.push_back (c11);
        quads.push_back (c01);
    }

    if (quads.empty())
        return;

    // #region agent log
    if ((gSoftFrameCounter % 30) == 1)
        agentDbgLog ("J", "drawPlayheadTicks", "billboard_ticks",
                     juce::String ("{\"n\":") + juce::String ((int) quads.size() / 6)
                         + ",\"ndcHalfW\":" + juce::String (ndcHalfW, 5)
                         + ",\"pxW\":1.5,\"pxH\":9}");
    // #endregion

    colourShader->use();
    setCornerUniforms (*colourShader);
    setLightingUniforms (*colourShader);
    bindDomeTextureForMesh();
    if (colourLightingAmountUniform != nullptr)
        colourLightingAmountUniform->set (0.0f);
    if (colourSelfShadowUniform != nullptr)
        colourSelfShadowUniform->set (0.0f);
    if (colourAoAmountUniform != nullptr)
        colourAoAmountUniform->set (0.0f);
    if (colourContactUniform != nullptr)
        colourContactUniform->set (0.0f);
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (viewMat.mat, 1, false);

    glDisable (GL_CULL_FACE);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDepthFunc (GL_LEQUAL);
    // Stream through labelVbo (labels re-upload immediately after).
    glBindBuffer (GL_ARRAY_BUFFER, labelVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (quads.size() * sizeof (V)), quads.data(), GL_STREAM_DRAW);

    const GLsizei stride = (GLsizei) sizeof (V);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourNormalAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 6));
    }

    glDrawArrays (GL_TRIANGLES, 0, (GLsizei) quads.size());

    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
    unbindDomeTexture();
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::drawContactShadow()
{
    // Floor-disc contact is a no-op for this heightfield: the mesh covers the whole
    // XZ domain, so any ground stain is overwritten. Contact is applied in the mesh
    // fragment shader via uContactShadow instead.
}

namespace
{
    juce::Matrix3D<float> makeOrthoShadow (float l, float r, float b, float t, float n, float f)
    {
        juce::Matrix3D<float> m;
        const float rl = juce::jmax (1.0e-5f, r - l);
        const float tb = juce::jmax (1.0e-5f, t - b);
        const float fn = juce::jmax (1.0e-5f, f - n);
        float* d = m.mat;
        std::memset (d, 0, 16 * sizeof (float));
        d[0] = 2.0f / rl;
        d[5] = 2.0f / tb;
        d[10] = -2.0f / fn;
        d[12] = -(r + l) / rl;
        d[13] = -(t + b) / tb;
        d[14] = -(f + n) / fn;
        d[15] = 1.0f;
        return m;
    }

    juce::Matrix3D<float> makeLookAtShadow (juce::Vector3D<float> eye,
                                            juce::Vector3D<float> center,
                                            juce::Vector3D<float> up)
    {
        juce::Vector3D<float> f {
            center.x - eye.x, center.y - eye.y, center.z - eye.z
        };
        float fl = juce::jmax (1.0e-6f, std::sqrt (f.x * f.x + f.y * f.y + f.z * f.z));
        f.x /= fl; f.y /= fl; f.z /= fl;
        // s = normalize(f x up)
        juce::Vector3D<float> s {
            f.y * up.z - f.z * up.y,
            f.z * up.x - f.x * up.z,
            f.x * up.y - f.y * up.x
        };
        float sl = juce::jmax (1.0e-6f, std::sqrt (s.x * s.x + s.y * s.y + s.z * s.z));
        s.x /= sl; s.y /= sl; s.z /= sl;
        // u = s x f
        juce::Vector3D<float> u {
            s.y * f.z - s.z * f.y,
            s.z * f.x - s.x * f.z,
            s.x * f.y - s.y * f.x
        };
        juce::Matrix3D<float> m;
        float* d = m.mat;
        d[0] = s.x; d[4] = s.y; d[8] = s.z;  d[12] = -(s.x * eye.x + s.y * eye.y + s.z * eye.z);
        d[1] = u.x; d[5] = u.y; d[9] = u.z;  d[13] = -(u.x * eye.x + u.y * eye.y + u.z * eye.z);
        d[2] = -f.x; d[6] = -f.y; d[10] = -f.z; d[14] = (f.x * eye.x + f.y * eye.y + f.z * eye.z);
        d[3] = 0.0f; d[7] = 0.0f; d[11] = 0.0f; d[15] = 1.0f;
        return m;
    }
}

void Spectrogram3DComponent::GlHost::releaseShadowAtlas()
{
    using namespace juce::gl;
    if (shadowFbo != 0)
    {
        glDeleteFramebuffers (1, &shadowFbo);
        shadowFbo = 0;
    }
    if (shadowDepthTex != 0)
    {
        glDeleteTextures (1, &shadowDepthTex);
        shadowDepthTex = 0;
    }
    shadowAtlasW = shadowAtlasH = shadowTileRes = shadowAtlasCascades = 0;
    cascadesBuilt = 0;
}

void Spectrogram3DComponent::GlHost::ensureShadowAtlas()
{
    using namespace juce::gl;
    const int tile = (int) owner.shadowMapResolution;
    const int cascades = juce::jlimit (1, kMaxShadowCascades, owner.shadowCascadeCount);
    const int aw = tile * cascades;
    const int ah = tile;
    if (shadowFbo != 0 && shadowDepthTex != 0
        && shadowTileRes == tile && shadowAtlasCascades == cascades
        && shadowAtlasW == aw && shadowAtlasH == ah)
        return;

    releaseShadowAtlas();
    glGenTextures (1, &shadowDepthTex);
    glBindTexture (GL_TEXTURE_2D, shadowDepthTex);
    glTexImage2D (GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, aw, ah, 0,
                  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);

    glGenFramebuffers (1, &shadowFbo);
    glBindFramebuffer (GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowDepthTex, 0);
    glDrawBuffer (GL_NONE);
    glReadBuffer (GL_NONE);
    const bool ok = (glCheckFramebufferStatus (GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindTexture (GL_TEXTURE_2D, 0);

    if (! ok)
    {
        releaseShadowAtlas();
        return;
    }
    shadowAtlasW = aw;
    shadowAtlasH = ah;
    shadowTileRes = tile;
    shadowAtlasCascades = cascades;
}

void Spectrogram3DComponent::GlHost::updateCascadeMatrices()
{
    cascadesBuilt = 0;
    const int nCasc = juce::jlimit (1, kMaxShadowCascades, owner.shadowCascadeCount);
    auto lightDir = getLightDirectionWorld();
    float llen = juce::jmax (1.0e-5f,
                             std::sqrt (lightDir.x * lightDir.x + lightDir.y * lightDir.y
                                        + lightDir.z * lightDir.z));
    lightDir.x /= llen; lightDir.y /= llen; lightDir.z /= llen;

    // Scene bounds: mesh footprint + height + debug sphere.
    float minX = -1.05f, maxX = 1.05f;
    float minY = -0.05f, maxY = owner.meshHeight + 0.15f;
    float minZ = -1.05f, maxZ = 1.05f;
    if (owner.debugSphereEnabled)
    {
        const float r = owner.debugSphereDiameter * 0.5f;
        const auto& p = owner.debugSpherePosition;
        minX = juce::jmin (minX, p.x - r);
        maxX = juce::jmax (maxX, p.x + r);
        minY = juce::jmin (minY, p.y - r);
        maxY = juce::jmax (maxY, p.y + r);
        minZ = juce::jmin (minZ, p.z - r);
        maxZ = juce::jmax (maxZ, p.z + r);
    }
    const juce::Vector3D<float> sceneCenter {
        0.5f * (minX + maxX), 0.5f * (minY + maxY), 0.5f * (minZ + maxZ)
    };
    const float extent = juce::jmax (maxX - minX, juce::jmax (maxY - minY, maxZ - minZ)) * 0.55f;

    // Fixed shadow distance - cascade count only subdivides this range.
    const float nearD = 0.15f;
    const float farD = juce::jmax (3.0f, owner.camera.distance + extent * 2.0f);
    const float expN = juce::jlimit (1.0f, 4.0f, owner.shadowCascadeDistributionExponent);

    juce::Vector3D<float> right, upCam, forward;
    owner.cameraBasis (right, upCam, forward);
    const juce::Vector3D<float> lookAt {
        owner.camera.panX, owner.camera.panY, owner.camera.panZ
    };
    const juce::Vector3D<float> camEye {
        lookAt.x - forward.x * owner.camera.distance,
        lookAt.y - forward.y * owner.camera.distance,
        lookAt.z - forward.z * owner.camera.distance
    };

    const auto glView = owner.getGlViewLocal();
    const float aspect = (float) juce::jmax (1, glView.getHeight())
                       / (float) juce::jmax (1, glView.getWidth());
    constexpr float kFovHalfW = 1.0f / 1.5f; // matches getProjectionMatrix

    auto frustumCorner = [&] (float depth, float nx, float ny) -> juce::Vector3D<float>
    {
        const float hw = depth * kFovHalfW;
        const float hh = hw * aspect;
        return {
            camEye.x + forward.x * depth + right.x * (nx * hw) + upCam.x * (ny * hh),
            camEye.y + forward.y * depth + right.y * (nx * hw) + upCam.y * (ny * hh),
            camEye.z + forward.z * depth + right.z * (nx * hw) + upCam.z * (ny * hh)
        };
    };

    auto xform = [] (const juce::Matrix3D<float>& m, const juce::Vector3D<float>& p)
        -> juce::Vector3D<float>
    {
        const float* d = m.mat;
        return {
            d[0] * p.x + d[4] * p.y + d[8]  * p.z + d[12],
            d[1] * p.x + d[5] * p.y + d[9]  * p.z + d[13],
            d[2] * p.x + d[6] * p.y + d[10] * p.z + d[14]
        };
    };

    juce::Vector3D<float> sceneCorners[8];
    {
        const float xs[2] = { minX, maxX }, ys[2] = { minY, maxY }, zs[2] = { minZ, maxZ };
        int k = 0;
        for (float x : xs)
            for (float y : ys)
                for (float z : zs)
                    sceneCorners[k++] = { x, y, z };
    }

    float prevFar = nearD;
    for (int i = 0; i < nCasc; ++i)
    {
        const float p = (float) (i + 1) / (float) nCasc;
        const float logS = nearD * std::pow (farD / juce::jmax (nearD, 1.0e-3f), p);
        const float linS = nearD + (farD - nearD) * p;
        const float w = 1.0f / expN; // higher exponent -> more logarithmic (UE-like)
        const float splitFar = juce::jmap (w, linS, logS);
        cascadeSplitFar[i] = splitFar;

        const float sliceNear = prevFar;
        const float sliceFar = splitFar;

        // Light looks at scene centre (stable); ortho fitted to this cascade's frustum slice.
        const float lightDist = extent * 2.5f + 1.0f;
        const juce::Vector3D<float> lightEye {
            sceneCenter.x + lightDir.x * lightDist,
            sceneCenter.y + lightDir.y * lightDist,
            sceneCenter.z + lightDir.z * lightDist
        };
        juce::Vector3D<float> up { 0.0f, 1.0f, 0.0f };
        if (std::abs (lightDir.y) > 0.95f)
            up = { 0.0f, 0.0f, 1.0f };
        const auto lightView = makeLookAtShadow (lightEye, sceneCenter, up);

        float minLX = 1.0e9f, maxLX = -1.0e9f;
        float minLY = 1.0e9f, maxLY = -1.0e9f;
        float minLZ = 1.0e9f, maxLZ = -1.0e9f;
        auto expand = [&] (const juce::Vector3D<float>& world)
        {
            const auto lp = xform (lightView, world);
            minLX = juce::jmin (minLX, lp.x); maxLX = juce::jmax (maxLX, lp.x);
            minLY = juce::jmin (minLY, lp.y); maxLY = juce::jmax (maxLY, lp.y);
            minLZ = juce::jmin (minLZ, lp.z); maxLZ = juce::jmax (maxLZ, lp.z);
        };

        // Frustum slice corners (near cascades -> tighter ortho -> more texels).
        for (float nx : { -1.0f, 1.0f })
            for (float ny : { -1.0f, 1.0f })
            {
                expand (frustumCorner (sliceNear, nx, ny));
                expand (frustumCorner (sliceFar, nx, ny));
            }
        // Casters in/near this depth slice (last cascade always takes the full scene).
        const bool lastCasc = (i == nCasc - 1);
        for (const auto& c : sceneCorners)
        {
            if (lastCasc)
            {
                expand (c);
                continue;
            }
            const float dx = c.x - camEye.x, dy = c.y - camEye.y, dz = c.z - camEye.z;
            const float depth = dx * forward.x + dy * forward.y + dz * forward.z;
            if (depth >= sliceNear - 0.35f && depth <= sliceFar + 0.35f)
                expand (c);
        }

        const float padXY = juce::jmax (0.05f, 0.08f * juce::jmax (maxLX - minLX, maxLY - minLY));
        minLX -= padXY; maxLX += padXY;
        minLY -= padXY; maxLY += padXY;

        // View looks down -Z; positive ortho near/far are distances along that axis.
        const float z0 = -maxLZ;
        const float z1 = -minLZ;
        const float zNear = juce::jmax (0.05f, juce::jmin (z0, z1) - 0.25f);
        const float zFar  = juce::jmax (zNear + 0.25f, juce::jmax (z0, z1) + 0.25f);

        const auto proj = makeOrthoShadow (minLX, maxLX, minLY, maxLY, zNear, zFar);
        shadowMatrix[i] = proj * lightView;
        prevFar = splitFar;
    }
    for (int i = nCasc; i < kMaxShadowCascades; ++i)
    {
        cascadeSplitFar[i] = farD;
        shadowMatrix[i] = shadowMatrix[juce::jmax (0, nCasc - 1)];
    }
    cascadesBuilt = nCasc;
}

void Spectrogram3DComponent::GlHost::drawMeshIntoShadow (const juce::Matrix3D<float>& lightVP) const
{
    using namespace juce::gl;
    if (meshIndexCount <= 0 || shadowDepthShader == nullptr || meshVbo == 0)
        return;
    shadowDepthShader->use();
    if (shadowDepthLightVpUniform != nullptr)
        shadowDepthLightVpUniform->setMatrix4 (lightVP.mat, 1, false);
    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);
    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (shadowDepthPositionAttrib != nullptr && shadowDepthPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) shadowDepthPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) shadowDepthPositionAttrib->attributeID, 3, GL_FLOAT,
                               GL_FALSE, stride, nullptr);
    }
    glDrawElements (GL_TRIANGLES, meshIndexCount, GL_UNSIGNED_INT, nullptr);
    if (shadowDepthPositionAttrib != nullptr && shadowDepthPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) shadowDepthPositionAttrib->attributeID);
}

void Spectrogram3DComponent::GlHost::drawSphereIntoShadow (const juce::Matrix3D<float>& lightVP) const
{
    using namespace juce::gl;
    if (! owner.debugSphereEnabled || sphereIndexCount <= 0 || shadowDepthShader == nullptr)
        return;
    shadowDepthShader->use();
    if (shadowDepthLightVpUniform != nullptr)
        shadowDepthLightVpUniform->setMatrix4 (lightVP.mat, 1, false);
    glBindBuffer (GL_ARRAY_BUFFER, sphereVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, sphereIbo);
    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (shadowDepthPositionAttrib != nullptr && shadowDepthPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) shadowDepthPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) shadowDepthPositionAttrib->attributeID, 3, GL_FLOAT,
                               GL_FALSE, stride, nullptr);
    }
    glDrawElements (GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    if (shadowDepthPositionAttrib != nullptr && shadowDepthPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) shadowDepthPositionAttrib->attributeID);
}

void Spectrogram3DComponent::GlHost::renderShadowDepthPass()
{
    using namespace juce::gl;
    if (! owner.lightingEnabled || ! owner.castShadowsEnabled || shadowDepthShader == nullptr)
    {
        cascadesBuilt = 0;
        return;
    }

    ensureShadowAtlas();
    if (shadowFbo == 0 || shadowDepthTex == 0)
        return;

    updateCascadeMatrices();
    uploadMeshIfNeeded();
    if (owner.debugSphereEnabled)
    {
        ensureDebugSphereGeometry();
        uploadDebugSphereWorldVerts();
    }

    const int tile = shadowTileRes;
    const int nCasc = juce::jmax (1, cascadesBuilt);
    glBindFramebuffer (GL_FRAMEBUFFER, shadowFbo);
    // Clear the whole atlas once, then fill every cascade tile.
    glViewport (0, 0, shadowAtlasW, shadowAtlasH);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDisable (GL_BLEND);
    glDisable (GL_CULL_FACE);
    glColorMask (GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glClear (GL_DEPTH_BUFFER_BIT);
    glEnable (GL_POLYGON_OFFSET_FILL);
    glPolygonOffset (2.0f, 4.0f);

    for (int c = 0; c < nCasc; ++c)
    {
        glViewport (c * tile, 0, tile, tile);
        // Particle mode: no mesh casters (particles do not cast in v1).
        if (! owner.particleModeEnabled)
            drawMeshIntoShadow (shadowMatrix[c]);
        drawSphereIntoShadow (shadowMatrix[c]);
    }

    glDisable (GL_POLYGON_OFFSET_FILL);
    glColorMask (GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer (GL_FRAMEBUFFER, 0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::ensureDebugSphereGeometry()
{
    using namespace juce::gl;
    if (sphereVbo == 0 || sphereIbo == 0)
        return;
    if (sphereIndexCount > 0 && ! sphereNeedsUpload)
        return;

    constexpr int kStacks = 32;
    constexpr int kSlices = 64;
    std::vector<Vertex> unitVerts;
    std::vector<juce::uint32> inds;
    unitVerts.reserve ((size_t) (kStacks + 1) * (size_t) (kSlices + 1));
    for (int y = 0; y <= kStacks; ++y)
    {
        const float v = (float) y / (float) kStacks;
        const float phi = v * juce::MathConstants<float>::pi;
        const float sy = std::sin (phi);
        const float cy = std::cos (phi);
        for (int x = 0; x <= kSlices; ++x)
        {
            const float u = (float) x / (float) kSlices;
            const float th = u * juce::MathConstants<float>::twoPi;
            const float nx = sy * std::cos (th);
            const float ny = cy;
            const float nz = sy * std::sin (th);
            Vertex vtx {};
            vtx.x = nx; vtx.y = ny; vtx.z = nz;
            vtx.r = 1.0f; vtx.g = 1.0f; vtx.b = 1.0f;
            vtx.nx = nx; vtx.ny = ny; vtx.nz = nz;
            unitVerts.push_back (vtx);
        }
    }
    for (int y = 0; y < kStacks; ++y)
        for (int x = 0; x < kSlices; ++x)
        {
            const juce::uint32 i0 = (juce::uint32) (y * (kSlices + 1) + x);
            const juce::uint32 i1 = i0 + 1;
            const juce::uint32 i2 = i0 + (juce::uint32) (kSlices + 1);
            const juce::uint32 i3 = i2 + 1;
            inds.push_back (i0); inds.push_back (i2); inds.push_back (i1);
            inds.push_back (i1); inds.push_back (i2); inds.push_back (i3);
        }

    // Store unit sphere in VBO; world transform applied in uploadDebugSphereWorldVerts.
    glBindBuffer (GL_ARRAY_BUFFER, sphereVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (unitVerts.size() * sizeof (Vertex)),
                  unitVerts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, sphereIbo);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (inds.size() * sizeof (juce::uint32)),
                  inds.data(), GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    sphereIndexCount = (int) inds.size();
    sphereNeedsUpload = true; // force world upload next
}

void Spectrogram3DComponent::GlHost::uploadDebugSphereWorldVerts()
{
    using namespace juce::gl;
    if (sphereVbo == 0 || sphereIndexCount <= 0)
        return;

    constexpr int kStacks = 32;
    constexpr int kSlices = 64;
    const float r = owner.debugSphereDiameter * 0.5f;
    const auto& c = owner.debugSpherePosition;
    std::vector<Vertex> verts;
    verts.reserve ((size_t) (kStacks + 1) * (size_t) (kSlices + 1));
    for (int y = 0; y <= kStacks; ++y)
    {
        const float v = (float) y / (float) kStacks;
        const float phi = v * juce::MathConstants<float>::pi;
        const float sy = std::sin (phi);
        const float cy = std::cos (phi);
        for (int x = 0; x <= kSlices; ++x)
        {
            const float u = (float) x / (float) kSlices;
            const float th = u * juce::MathConstants<float>::twoPi;
            const float nx = sy * std::cos (th);
            const float ny = cy;
            const float nz = sy * std::sin (th);
            Vertex vtx {};
            vtx.x = c.x + nx * r;
            vtx.y = c.y + ny * r;
            vtx.z = c.z + nz * r;
            vtx.r = owner.debugSphereAlbedo.getFloatRed();
            vtx.g = owner.debugSphereAlbedo.getFloatGreen();
            vtx.b = owner.debugSphereAlbedo.getFloatBlue();
            vtx.nx = nx; vtx.ny = ny; vtx.nz = nz;
            verts.push_back (vtx);
        }
    }
    glBindBuffer (GL_ARRAY_BUFFER, sphereVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (verts.size() * sizeof (Vertex)),
                  verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    sphereNeedsUpload = false;
}

void Spectrogram3DComponent::GlHost::drawDebugSphere()
{
    using namespace juce::gl;
    if (! owner.debugSphereEnabled || colourShader == nullptr)
        return;

    ensureDebugSphereGeometry();
    uploadDebugSphereWorldVerts();
    if (sphereIndexCount <= 0)
        return;

    colourShader->use();
    setCornerUniforms (*colourShader);
    setLightingUniforms (*colourShader);
    // Lookdev: own rough/metal/spec; shader skips dome/SSS/wrap so they actually read.
    setMaterialOverrideUniforms (true, owner.debugSphereAlbedo,
                                 owner.debugSphereRoughness, owner.debugSphereMetalness,
                                 owner.debugSphereSpecular);
    // Sphere is not a heightfield - skip self-shadow / AO / contact; keep cast map.
    if (colourSelfShadowUniform != nullptr)
        colourSelfShadowUniform->set (0.0f);
    if (colourAoAmountUniform != nullptr)
        colourAoAmountUniform->set (0.0f);
    if (colourContactUniform != nullptr)
        colourContactUniform->set (0.0f);
    if (colourDomeStrengthUniform != nullptr)
        colourDomeStrengthUniform->set (0.0f);
    if (colourSssModeUniform != nullptr)
        colourSssModeUniform->set (0.0f);
    if (colourRimUniform != nullptr)
        colourRimUniform->set (0.0f);
    bindDomeTextureForMesh();
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (getViewMatrix().mat, 1, false);

    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);

    glBindBuffer (GL_ARRAY_BUFFER, sphereVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, sphereIbo);
    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, nullptr);
    }
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourNormalAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 6));
    }
    glDrawElements (GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, nullptr);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);

    setMaterialOverrideUniforms (false, juce::Colours::white, 0.45f, 0.0f);
    unbindDomeTexture();
    glActiveTexture (GL_TEXTURE3);
    glBindTexture (GL_TEXTURE_2D, 0);
    glActiveTexture (GL_TEXTURE0);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::setGizmoXrayUniforms (bool insideGhostPass) const
{
    if (colourGizmoXrayUniform != nullptr)
        colourGizmoXrayUniform->set (insideGhostPass ? 1.0f : 0.0f);
    if (colourXrayCenterUniform != nullptr)
    {
        const auto& c = owner.debugSpherePosition;
        colourXrayCenterUniform->set (c.x, c.y, c.z);
    }
    if (colourXrayRadiusUniform != nullptr)
        colourXrayRadiusUniform->set (owner.debugSphereDiameter * 0.5f);
    if (colourXrayAlphaUniform != nullptr)
        colourXrayAlphaUniform->set (0.4f);
}

void Spectrogram3DComponent::GlHost::drawDebugGizmo()
{
    using namespace juce::gl;
    if (! owner.debugSphereEnabled || colourShader == nullptr || gizmoVbo == 0 || gizmoIbo == 0)
        return;

    const auto& c = owner.debugSpherePosition;
    const float len = juce::jmax (0.08f, owner.debugSphereDiameter * 0.9f);
    const float shaftR = juce::jmax (0.0045f, owner.debugSphereDiameter * 0.035f);
    const float coneR = shaftR * 2.4f;
    const float coneLen = len * 0.28f;
    const float shaftLen = juce::jmax (0.02f, len - coneLen);
    constexpr int kSegs = 14;
    const bool xray = owner.isGizmoXrayActive();
    const auto hover = owner.gizmoHoverAxis;
    const auto drag = owner.dragMode;
    const auto hotAxis = (drag == DragMode::gizmoX || drag == DragMode::gizmoY || drag == DragMode::gizmoZ)
        ? drag : hover;

    struct GVert { float x, y, z, r, g, b, nx, ny, nz; };
    std::vector<GVert> verts;
    std::vector<juce::uint32> indices;
    verts.reserve ((size_t) kSegs * 3 * 8);
    indices.reserve ((size_t) kSegs * 3 * 12);

    auto addAxis = [&] (float ax, float ay, float az, float cr, float cg, float cb, DragMode axisMode)
    {
        // Brighten the hovered / active arrow slightly.
        if (xray && hotAxis == axisMode)
        {
            cr = juce::jmin (1.0f, cr * 1.25f + 0.08f);
            cg = juce::jmin (1.0f, cg * 1.25f + 0.08f);
            cb = juce::jmin (1.0f, cb * 1.25f + 0.08f);
        }

        // Orthonormal basis with axis = (ax,ay,az).
        juce::Vector3D<float> axis { ax, ay, az };
        juce::Vector3D<float> helper = (std::abs (ax) < 0.9f)
            ? juce::Vector3D<float> { 1.0f, 0.0f, 0.0f }
            : juce::Vector3D<float> { 0.0f, 1.0f, 0.0f };
        juce::Vector3D<float> u {
            axis.y * helper.z - axis.z * helper.y,
            axis.z * helper.x - axis.x * helper.z,
            axis.x * helper.y - axis.y * helper.x
        };
        float ul = juce::jmax (1.0e-6f, std::sqrt (u.x * u.x + u.y * u.y + u.z * u.z));
        u.x /= ul; u.y /= ul; u.z /= ul;
        juce::Vector3D<float> v {
            axis.y * u.z - axis.z * u.y,
            axis.z * u.x - axis.x * u.z,
            axis.x * u.y - axis.y * u.x
        };

        auto pushVert = [&] (float px, float py, float pz, float nx, float ny, float nz)
        {
            const float nl = juce::jmax (1.0e-6f, std::sqrt (nx * nx + ny * ny + nz * nz));
            verts.push_back ({ px, py, pz, cr, cg, cb, nx / nl, ny / nl, nz / nl });
        };

        const juce::uint32 base = (juce::uint32) verts.size();
        // Shaft cylinder: rings at t=0 and t=shaftLen.
        for (int ring = 0; ring < 2; ++ring)
        {
            const float t = (ring == 0) ? 0.0f : shaftLen;
            const float px0 = c.x + axis.x * t;
            const float py0 = c.y + axis.y * t;
            const float pz0 = c.z + axis.z * t;
            for (int i = 0; i < kSegs; ++i)
            {
                const float a = (float) i * juce::MathConstants<float>::twoPi / (float) kSegs;
                const float ca = std::cos (a), sa = std::sin (a);
                const float nx = u.x * ca + v.x * sa;
                const float ny = u.y * ca + v.y * sa;
                const float nz = u.z * ca + v.z * sa;
                pushVert (px0 + nx * shaftR, py0 + ny * shaftR, pz0 + nz * shaftR, nx, ny, nz);
            }
        }
        for (int i = 0; i < kSegs; ++i)
        {
            const juce::uint32 i0 = base + (juce::uint32) i;
            const juce::uint32 i1 = base + (juce::uint32) ((i + 1) % kSegs);
            const juce::uint32 i2 = base + (juce::uint32) kSegs + (juce::uint32) i;
            const juce::uint32 i3 = base + (juce::uint32) kSegs + (juce::uint32) ((i + 1) % kSegs);
            indices.push_back (i0); indices.push_back (i2); indices.push_back (i1);
            indices.push_back (i1); indices.push_back (i2); indices.push_back (i3);
        }

        // Cone: base ring at shaftLen, tip at len.
        const juce::uint32 coneBase = (juce::uint32) verts.size();
        const float bx = c.x + axis.x * shaftLen;
        const float by = c.y + axis.y * shaftLen;
        const float bz = c.z + axis.z * shaftLen;
        const float tipX = c.x + axis.x * len;
        const float tipY = c.y + axis.y * len;
        const float tipZ = c.z + axis.z * len;
        // Slant normals: blend radial with axis for soft cone shading / SSGI.
        const float slant = coneR / juce::jmax (coneLen, 1.0e-4f);
        for (int i = 0; i < kSegs; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / (float) kSegs;
            const float ca = std::cos (a), sa = std::sin (a);
            const float rx = u.x * ca + v.x * sa;
            const float ry = u.y * ca + v.y * sa;
            const float rz = u.z * ca + v.z * sa;
            pushVert (bx + rx * coneR, by + ry * coneR, bz + rz * coneR,
                      rx + axis.x * slant, ry + axis.y * slant, rz + axis.z * slant);
        }
        const juce::uint32 tipIdx = (juce::uint32) verts.size();
        pushVert (tipX, tipY, tipZ, axis.x, axis.y, axis.z);
        for (int i = 0; i < kSegs; ++i)
        {
            indices.push_back (coneBase + (juce::uint32) i);
            indices.push_back (tipIdx);
            indices.push_back (coneBase + (juce::uint32) ((i + 1) % kSegs));
        }
        // Cone base cap (faces toward shaft) so the arrow reads solid in GI.
        const juce::uint32 capCenter = (juce::uint32) verts.size();
        pushVert (bx, by, bz, -axis.x, -axis.y, -axis.z);
        const juce::uint32 capRing = (juce::uint32) verts.size();
        for (int i = 0; i < kSegs; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / (float) kSegs;
            const float ca = std::cos (a), sa = std::sin (a);
            const float rx = u.x * ca + v.x * sa;
            const float ry = u.y * ca + v.y * sa;
            const float rz = u.z * ca + v.z * sa;
            pushVert (bx + rx * coneR, by + ry * coneR, bz + rz * coneR,
                      -axis.x, -axis.y, -axis.z);
        }
        for (int i = 0; i < kSegs; ++i)
        {
            indices.push_back (capCenter);
            indices.push_back (capRing + (juce::uint32) ((i + 1) % kSegs));
            indices.push_back (capRing + (juce::uint32) i);
        }
    };

    addAxis (1.0f, 0.0f, 0.0f, 1.0f, 0.18f, 0.18f, DragMode::gizmoX);
    addAxis (0.0f, 1.0f, 0.0f, 0.22f, 1.0f, 0.22f, DragMode::gizmoY);
    addAxis (0.0f, 0.0f, 1.0f, 0.28f, 0.48f, 1.0f, DragMode::gizmoZ);

    glBindBuffer (GL_ARRAY_BUFFER, gizmoVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (verts.size() * sizeof (GVert)),
                  verts.data(), GL_DYNAMIC_DRAW);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, gizmoIbo);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (indices.size() * sizeof (juce::uint32)),
                  indices.data(), GL_DYNAMIC_DRAW);
    gizmoIndexCount = (int) indices.size();

    colourShader->use();
    setCornerUniforms (*colourShader);
    setLightingUniforms (*colourShader);
    // Unlit emissive axes (lookdev) - solid volume so SSGI can hit them evenly.
    if (colourLightingAmountUniform != nullptr)
        colourLightingAmountUniform->set (0.0f);
    if (colourCastShadowUniform != nullptr)
        colourCastShadowUniform->set (0.0f);
    if (colourSelfShadowUniform != nullptr)
        colourSelfShadowUniform->set (0.0f);
    if (colourAoAmountUniform != nullptr)
        colourAoAmountUniform->set (0.0f);
    setMaterialOverrideUniforms (false, juce::Colours::white, 0.45f, 0.0f);
    setGizmoXrayUniforms (false);
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (getViewMatrix().mat, 1, false);

    const GLsizei stride = (GLsizei) sizeof (GVert);
    auto bindAttribs = [&]()
    {
        if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
            glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                                   stride, nullptr);
        }
        if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
            glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                                   stride, (const void*) (sizeof (float) * 3));
        }
        if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
            glVertexAttribPointer ((GLuint) colourNormalAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                                   stride, (const void*) (sizeof (float) * 6));
        }
    };
    auto unbindAttribs = [&]()
    {
        if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
    };

    // Pass 1: opaque shafts outside the sphere (depth-tested against the sphere).
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDisable (GL_BLEND);
    glDisable (GL_CULL_FACE);
    bindAttribs();
    glDrawElements (GL_TRIANGLES, gizmoIndexCount, GL_UNSIGNED_INT, nullptr);

    // Pass 2 (hover/drag): ghost the portion inside the sphere over the opaque surface.
    if (xray)
    {
        setGizmoXrayUniforms (true);
        glDepthMask (GL_FALSE);
        glDisable (GL_DEPTH_TEST);
        glEnable (GL_BLEND);
        glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable (GL_CULL_FACE);
        glCullFace (GL_BACK);
        glDrawElements (GL_TRIANGLES, gizmoIndexCount, GL_UNSIGNED_INT, nullptr);
        setGizmoXrayUniforms (false);
        glDisable (GL_BLEND);
        glDisable (GL_CULL_FACE);
        glEnable (GL_DEPTH_TEST);
        glDepthMask (GL_TRUE);
    }

    unbindAttribs();
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::drawSpectrogramSurface()
{
    if (owner.particleModeEnabled)
    {
        owner.ensureParticleSystem();
        if (owner.particleSystem != nullptr)
        {
            owner.particleSystem->ensureGl (openGLContext);
            // GPU integrate (or deferred CPU fallback) must run with a current GL context.
            owner.particleSystem->integrateOnGlThread();
            juce::Vector3D<float> right, up, forward;
            owner.cameraBasis (right, up, forward);
            juce::ignoreUnused (forward);
            owner.particleSystem->draw (getProjectionMatrix(), getViewMatrix(), right, up);
        }
        return;
    }
    drawMesh();
}

void Spectrogram3DComponent::GlHost::drawMesh()
{
    using namespace juce::gl;
    if (meshIndexCount <= 0 || colourShader == nullptr)
        return;

    colourShader->use();
    setCornerUniforms (*colourShader);
    setLightingUniforms (*colourShader);
    bindHeightMapForMesh();
    bindDomeTextureForMesh();
    if (colourProjectionUniform != nullptr)
        colourProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (colourViewUniform != nullptr)
        colourViewUniform->setMatrix4 (getViewMatrix().mat, 1, false);

    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);
    glEnable (GL_POLYGON_OFFSET_FILL);
    glPolygonOffset (1.0f, 1.0f);

    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);

    const GLsizei stride = (GLsizei) sizeof (Vertex);
    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourColourAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);
        glVertexAttribPointer ((GLuint) colourNormalAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 6));
    }

    glDrawElements (GL_TRIANGLES, meshIndexCount, GL_UNSIGNED_INT, nullptr);

    if (colourPositionAttrib != nullptr && colourPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourPositionAttrib->attributeID);
    if (colourColourAttrib != nullptr && colourColourAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourColourAttrib->attributeID);
    if (colourNormalAttrib != nullptr && colourNormalAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) colourNormalAttrib->attributeID);

    glActiveTexture (GL_TEXTURE1);
    glBindTexture (GL_TEXTURE_2D, 0);
    glActiveTexture (GL_TEXTURE0);
    unbindDomeTexture();

    glDisable (GL_POLYGON_OFFSET_FILL);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
}

void Spectrogram3DComponent::GlHost::drawMeshNormalsPass (int width, int height)
{
    using namespace juce::gl;
    if (normalsShader == nullptr || ! ssgiNormalsFbo.isValid())
        return;

    // Reuse soft scene depth (read-only) so mesh + lookdev sphere layer correctly.
    // Clear colour only - wiping depth would destroy the soft FBO depth attachment.
    ssgiNormalsFbo.makeCurrentRenderingTarget();
    glViewport (0, 0, width, height);
    if (softDepthTex != 0)
        glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, softDepthTex, 0);
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    glClear (GL_COLOR_BUFFER_BIT);
    glEnable (GL_DEPTH_TEST);
    glDepthFunc (GL_LEQUAL);
    glDepthMask (GL_FALSE);
    glDisable (GL_BLEND);
    glEnable (GL_CULL_FACE);
    glCullFace (GL_BACK);

    normalsShader->use();
    const auto proj = getProjectionMatrix();
    const auto view = getViewMatrix();
    if (normalsProjectionUniform != nullptr)
        normalsProjectionUniform->setMatrix4 (proj.mat, 1, false);
    if (normalsViewUniform != nullptr)
        normalsViewUniform->setMatrix4 (view.mat, 1, false);

    const GLsizei stride = (GLsizei) sizeof (Vertex);
    auto drawNormalsVbo = [&] (unsigned int vbo, unsigned int ibo, int indexCount, bool cull)
    {
        if (vbo == 0 || ibo == 0 || indexCount <= 0)
            return;
        if (cull)
            glEnable (GL_CULL_FACE);
        else
            glDisable (GL_CULL_FACE);
        glBindBuffer (GL_ARRAY_BUFFER, vbo);
        glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ibo);
        if (normalsPositionAttrib != nullptr && normalsPositionAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) normalsPositionAttrib->attributeID);
            glVertexAttribPointer ((GLuint) normalsPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                                   stride, nullptr);
        }
        if (normalsNormalAttrib != nullptr && normalsNormalAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) normalsNormalAttrib->attributeID);
            glVertexAttribPointer ((GLuint) normalsNormalAttrib->attributeID, 3, GL_FLOAT, GL_FALSE,
                                   stride, (const void*) (sizeof (float) * 6));
        }
        glDrawElements (GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
        if (normalsPositionAttrib != nullptr && normalsPositionAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) normalsPositionAttrib->attributeID);
        if (normalsNormalAttrib != nullptr && normalsNormalAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) normalsNormalAttrib->attributeID);
    };

    drawNormalsVbo (meshVbo, meshIbo, meshIndexCount, true);

    if (owner.debugSphereEnabled)
    {
        ensureDebugSphereGeometry();
        uploadDebugSphereWorldVerts();
        drawNormalsVbo (sphereVbo, sphereIbo, sphereIndexCount, false);
    }

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    // Detach shared depth so later softFbo clears cannot race this FBO.
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glDisable (GL_CULL_FACE);
    glDisable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    ssgiNormalsFbo.releaseAsRenderingTarget();
}

void Spectrogram3DComponent::GlHost::applySsaoAndBloom (int width, int height)
{
    using namespace juce::gl;
    if (postShader == nullptr || ! softFbo.isValid() || softDepthTex == 0)
        return;

    ensurePostFrameBuffers (width, height);
    if (! postFboA.isValid() || ! postFboB.isValid())
        return;

    const float verts[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    auto drawFs = [&] (int mode, juce::OpenGLFrameBuffer* dest,
                       GLuint colourTex, GLuint secondTex, GLuint auxTex,
                       float strength, float radius, float threshold, float param,
                       int vw, int vh, bool useMeshNormalsFlag)
    {
        if (dest != nullptr)
        {
            dest->makeCurrentRenderingTarget();
            glViewport (0, 0, dest->getWidth(), dest->getHeight());
        }
        else
        {
            softFbo.makeCurrentRenderingTarget();
            glViewport (0, 0, width, height);
        }

        postShader->use();
        if (postModeUniform != nullptr) postModeUniform->set (mode);
        if (postStrengthUniform != nullptr) postStrengthUniform->set (strength);
        if (postRadiusUniform != nullptr) postRadiusUniform->set (radius);
        if (postThresholdUniform != nullptr) postThresholdUniform->set (threshold);
        if (postParamUniform != nullptr) postParamUniform->set (param);
        if (postResolutionUniform != nullptr)
            postResolutionUniform->set ((float) vw, (float) vh);
        if (postInvProjUniform != nullptr)
        {
            // Mode 6 (DoF gather): edge spill is uParam (not mat packing).
            // Mode 7 (SSR): lookdev pack (rough/fresnel/edge/roughInf, intensity/...).
            // SSGI gather: [0][0] = mesh-normals flag.
            float id[16] = {
                1, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
            };
            if (mode == 7)
            {
                id[0] = owner.roughnessAmount;
                id[1] = owner.ssrFresnel;
                id[2] = owner.ssrEdgeFade;
                id[3] = owner.ssrRoughnessInfluence;
                id[4] = owner.ssrIntensity;
                id[5] = owner.ssrMetallicBias;
                id[6] = owner.metalnessAmount;
                id[7] = useMeshNormalsFlag ? 1.0f : 0.0f;
                id[8] = owner.domeSkyColour.getFloatRed();
                id[9] = owner.domeSkyColour.getFloatGreen();
                id[10] = owner.domeSkyColour.getFloatBlue();
                id[12] = owner.ssrDomeFallback;
            }
            else if (mode != 6)
            {
                id[0] = useMeshNormalsFlag ? 1.0f : 0.0f;
                id[1] = 0.0f;
            }
            postInvProjUniform->setMatrix4 (id, 1, false);
        }

        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, colourTex);
        // SSR hit UVs are sub-pixel - linear colour sampling avoids blocky mirrors.
        if (mode == 7 && colourTex != 0)
        {
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        if (postTexUniform != nullptr) postTexUniform->set (0);
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, secondTex);
        if (postDepthUniform != nullptr) postDepthUniform->set (1);
        glActiveTexture (GL_TEXTURE2);
        // Allow auxTex == 0 (no fallback) so history/moments slots stay empty on first frame.
        glBindTexture (GL_TEXTURE_2D, auxTex);
        if (postAuxUniform != nullptr) postAuxUniform->set (2);

        glDisable (GL_DEPTH_TEST);
        glDepthMask (GL_FALSE);
        glDisable (GL_BLEND);
        glBindBuffer (GL_ARRAY_BUFFER, tintVbo);
        glBufferData (GL_ARRAY_BUFFER, sizeof (verts), verts, GL_STREAM_DRAW);
        if (postPositionAttrib != nullptr && postPositionAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) postPositionAttrib->attributeID);
            glVertexAttribPointer ((GLuint) postPositionAttrib->attributeID, 2, GL_FLOAT, GL_FALSE,
                                   (GLsizei) (2 * sizeof (float)), nullptr);
        }
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
        if (postPositionAttrib != nullptr && postPositionAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) postPositionAttrib->attributeID);
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        glActiveTexture (GL_TEXTURE2);
        glBindTexture (GL_TEXTURE_2D, 0);
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, 0);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, 0);
        if (dest != nullptr)
            dest->releaseAsRenderingTarget();
    };

    ++postFrameIndex;

    // AO / self-shadow / dome run in the mesh shader.
    // Post order: SSGI -> SSR -> bloom -> grid/ticks/labels -> DOF -> tonemap.
    // #region agent log
    if ((gSoftFrameCounter % 30) == 1 && owner.ssgiEnabled)
    {
        agentDbgLog ("H", "applySsaoAndBloom", "ssgi_pass_mesh_only",
                     juce::String ("{\"ssgi\":1")
                         + ",\"ssgiStr\":" + juce::String (owner.ssgiStrength, 3)
                         + ",\"ssgiRadius\":" + juce::String (owner.ssgiRadius, 3)
                         + ",\"advanced\":" + juce::String (owner.needsAdvancedSsgi() ? 1 : 0)
                         + ",\"audioLvl\":" + juce::String (owner.audioLevelLive01, 3)
                         + ",\"labelsInSceneYet\":" + juce::String (gLabelDrawCallsThisFrame)
                         + ",\"gridInSceneYet\":0"
                         + "}");
    }
    // #endregion
    if (owner.ssgiEnabled && owner.ssgiStrength > 1.0e-4f)
    {
        const float qualityF = owner.ssgiQuality == ShadowQuality::low ? 0.0f
                             : (owner.ssgiQuality == ShadowQuality::medium ? 1.0f
                             : (owner.ssgiQuality == ShadowQuality::high ? 2.0f : 3.0f));
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();
        // Temporal / denoise / half-res / mesh-normals path is retained below for later
        // use but is not exposed in Look UI (defaults stay off; Quality Ultra covers most cases).
        const bool advanced = owner.needsAdvancedSsgi();

        if (! advanced)
        {
            // Gather -> multi-scale bilateral (merges vogel-disk stars/copies) -> composite.
            drawFs (9, &postFboA, sceneTex, softDepthTex, 0,
                    1.0f, owner.ssgiRadius, qualityF, 0.0f,
                    width, height, false);
            const float denoiseAmt = juce::jlimit (0.70f, 1.0f,
                                                   0.70f + owner.ssgiRadius * 0.30f);
            juce::OpenGLFrameBuffer* cur = &postFboA;
            juce::OpenGLFrameBuffer* other = &postFboB;
            float stepPx = 1.0f;
            for (int pass = 0; pass < 4; ++pass)
            {
                drawFs (10, other, (GLuint) cur->getTextureID(), softDepthTex, 0,
                        denoiseAmt, 1.0f, 0.0f, stepPx,
                        width, height, false);
                std::swap (cur, other);
                stepPx *= 2.0f;
            }
            drawFs (12, other, sceneTex, (GLuint) cur->getTextureID(), 0,
                    owner.ssgiStrength, 1.0f, 0.0f, 0.0f, width, height, false);
            drawFs (0, nullptr, (GLuint) other->getTextureID(), softDepthTex, 0,
                    1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
        }
        else
        {
            const bool modernDenoise = owner.ssgiDenoiseEnabled
                && owner.ssgiDenoiseMode == SsgiDenoiseMode::modern
                && owner.ssgiDenoiseAmount > 1.0e-4f;
            const bool needNormals = owner.ssgiMeshNormalsEnabled || modernDenoise;
            const bool needMoments = modernDenoise && owner.ssgiTemporalEnabled;
            const bool needHistory = owner.ssgiTemporalEnabled;

            ensureSsgiSupportBuffers (width, height,
                                      owner.ssgiHalfResEnabled,
                                      needHistory,
                                      needNormals,
                                      needMoments);

            GLuint normalsTex = 0;
            if (needNormals && ssgiNormalsFbo.isValid())
            {
                drawMeshNormalsPass (width, height);
                normalsTex = (GLuint) ssgiNormalsFbo.getTextureID();
            }

            const bool jitter = owner.ssgiTemporalEnabled || owner.ssgiDenoiseEnabled;
            const float rot = jitter
                ? (float) postFrameIndex * 2.39996323f
                : 0.0f;

            juce::OpenGLFrameBuffer* giBuf = &postFboA;
            if (owner.ssgiHalfResEnabled && ssgiHalfFbo.isValid())
            {
                drawFs (9, &ssgiHalfFbo, sceneTex, softDepthTex, normalsTex,
                        1.0f, owner.ssgiRadius, qualityF, rot,
                        ssgiHalfW, ssgiHalfH, normalsTex != 0);
                drawFs (14, &postFboA, (GLuint) ssgiHalfFbo.getTextureID(), softDepthTex, 0,
                        1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
                giBuf = &postFboA;
            }
            else
            {
                drawFs (9, &postFboA, sceneTex, softDepthTex, normalsTex,
                        1.0f, owner.ssgiRadius, qualityF, rot,
                        width, height, normalsTex != 0);
            }

            juce::OpenGLFrameBuffer* cur = giBuf;
            juce::OpenGLFrameBuffer* other = (giBuf == &postFboA) ? &postFboB : &postFboA;

            if (modernDenoise)
            {
                // Modern: temporal+moments (optional) -> à-trous passes -> composite.
                if (owner.ssgiTemporalEnabled && ssgiHistoryFbo.isValid() && ssgiMomentsFbo.isValid())
                {
                    const GLuint histTex = ssgiHistoryValid
                        ? (GLuint) ssgiHistoryFbo.getTextureID() : 0;
                    const GLuint momTex = ssgiMomentsValid
                        ? (GLuint) ssgiMomentsFbo.getTextureID() : 0;
                    const float tempAmt = ssgiHistoryValid ? owner.ssgiTemporalAmount : 0.0f;
                    // mode 15: uDepth slot carries moments history.
                    drawFs (15, other, (GLuint) cur->getTextureID(), momTex, histTex,
                            tempAmt, 1.0f, 0.0f, 0.0f, width, height, false);
                    std::swap (cur, other);

                    const float momBlend = ssgiMomentsValid ? tempAmt : 0.0f;
                    const GLuint prevMom = ssgiMomentsValid
                        ? (GLuint) ssgiMomentsFbo.getTextureID() : 0;
                    drawFs (18, other, (GLuint) cur->getTextureID(), softDepthTex, prevMom,
                            momBlend, 1.0f, 0.0f, 0.0f, width, height, false);
                    drawFs (0, &ssgiMomentsFbo, (GLuint) other->getTextureID(), softDepthTex, 0,
                            1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
                    ssgiMomentsValid = true;

                    drawFs (0, &ssgiHistoryFbo, (GLuint) cur->getTextureID(), softDepthTex, 0,
                            1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
                    ssgiHistoryValid = true;
                }

                const int atrousPasses = owner.ssgiAtrousQuality == ShadowQuality::low ? 3
                                       : (owner.ssgiAtrousQuality == ShadowQuality::high ? 5 : 4);
                float stepPx = 1.0f;
                for (int p = 0; p < atrousPasses; ++p)
                {
                    drawFs (16, other, (GLuint) cur->getTextureID(), softDepthTex, normalsTex,
                            owner.ssgiDenoiseAmount, 1.0f, 0.0f, stepPx,
                            width, height, normalsTex != 0);
                    std::swap (cur, other);
                    stepPx *= 2.0f;
                }
            }
            else
            {
                // Simple: bilateral then basic temporal (legacy advanced path).
                if (owner.ssgiDenoiseEnabled && owner.ssgiDenoiseAmount > 1.0e-4f)
                {
                    drawFs (10, other, (GLuint) cur->getTextureID(), softDepthTex, 0,
                            owner.ssgiDenoiseAmount, 1.0f, 0.0f, 0.0f,
                            width, height, false);
                    std::swap (cur, other);
                }

                if (owner.ssgiTemporalEnabled && ssgiHistoryFbo.isValid())
                {
                    const GLuint histTex = ssgiHistoryValid
                        ? (GLuint) ssgiHistoryFbo.getTextureID() : 0;
                    const float tempAmt = ssgiHistoryValid ? owner.ssgiTemporalAmount : 0.0f;
                    drawFs (11, other, (GLuint) cur->getTextureID(), softDepthTex, histTex,
                            tempAmt, 1.0f, 0.0f, 0.0f, width, height, false);
                    std::swap (cur, other);

                    drawFs (0, &ssgiHistoryFbo, (GLuint) cur->getTextureID(), softDepthTex, 0,
                            1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
                    ssgiHistoryValid = true;
                }
            }

            drawFs (12, &postFboB, sceneTex, (GLuint) cur->getTextureID(), 0,
                    owner.ssgiStrength, 1.0f, 0.0f, 0.0f, width, height, false);
            drawFs (0, nullptr, (GLuint) postFboB.getTextureID(), softDepthTex, 0,
                    1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
        }
    }

    if (owner.ssrEnabled && owner.ssrStrength > 1.0e-4f)
    {
        // #region agent log
        static int ssrLogN = 0;
        if ((++ssrLogN % 30) == 1)
            agentDbgLog ("A", "applySsaoAndBloom", "ssr_pass_running",
                         juce::String ("{\"ssrStr\":") + juce::String (owner.ssrStrength, 3)
                             + ",\"ssrDist\":" + juce::String (owner.ssrDistance, 3)
                             + ",\"n\":" + juce::String (ssrLogN) + "}");
        // #endregion
        const float qualityF = owner.ssrQuality == ShadowQuality::low ? 0.0f
                             : (owner.ssrQuality == ShadowQuality::medium ? 1.0f
                             : (owner.ssrQuality == ShadowQuality::high ? 2.0f : 3.0f));
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();

        // Smooth mesh + sphere view-normals - depth derivatives were the "8-bit" look.
        ensureSsgiSupportBuffers (width, height, false, false, true, false);
        GLuint normalsTex = 0;
        if (ssgiNormalsFbo.isValid())
        {
            drawMeshNormalsPass (width, height);
            normalsTex = (GLuint) ssgiNormalsFbo.getTextureID();
        }

        drawFs (7, &postFboA, sceneTex, softDepthTex, normalsTex,
                owner.ssrStrength, owner.ssrDistance, qualityF, owner.ssrThickness,
                width, height, normalsTex != 0);
        drawFs (0, nullptr, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
    }

    if (owner.bloomEnabled)
    {
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();
        drawFs (2, &postFboA, sceneTex, softDepthTex, 0,
                owner.bloomStrength, 1.0f, owner.bloomThreshold, 0.0f, postFboW, postFboH, false);
        drawFs (3, &postFboB, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, postFboW, postFboH, false);
        drawFs (4, &postFboA, (GLuint) postFboB.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, postFboW, postFboH, false);
        drawFs (5, &postFboB, sceneTex, (GLuint) postFboA.getTextureID(), 0,
                owner.bloomStrength, 1.0f, 0.0f, 0.0f, width, height, false);
        drawFs (0, nullptr, (GLuint) postFboB.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
    }

    // Camera motion blur (depth -> velocity from prev view-proj). After bloom so
    // glow streaks with camera; before chrome overlays so labels stay readable.
    if (owner.motionBlurEnabled && motionPrevVpValid)
    {
        const auto proj = getProjectionMatrix();
        const auto view = getViewMatrix();
        // inv(view) for rigid camera (column-major OpenGL).
        float invView[16];
        {
            const float* m = view.mat;
            invView[0] = m[0]; invView[1] = m[4]; invView[2] = m[8];  invView[3] = 0.0f;
            invView[4] = m[1]; invView[5] = m[5]; invView[6] = m[9];  invView[7] = 0.0f;
            invView[8] = m[2]; invView[9] = m[6]; invView[10] = m[10]; invView[11] = 0.0f;
            const float tx = m[12], ty = m[13], tz = m[14];
            invView[12] = -(invView[0] * tx + invView[4] * ty + invView[8] * tz);
            invView[13] = -(invView[1] * tx + invView[5] * ty + invView[9] * tz);
            invView[14] = -(invView[2] * tx + invView[6] * ty + invView[10] * tz);
            invView[15] = 1.0f;
        }
        const float qualityF = owner.motionBlurQuality == ShadowQuality::low ? 0.0f
                             : (owner.motionBlurQuality == ShadowQuality::high ? 2.0f : 1.0f);
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();

        // Custom draw: mode 20 needs extra matrices beyond drawFs packing.
        postFboA.makeCurrentRenderingTarget();
        glViewport (0, 0, postFboA.getWidth(), postFboA.getHeight());
        postShader->use();
        if (postModeUniform != nullptr) postModeUniform->set (20);
        if (postStrengthUniform != nullptr) postStrengthUniform->set (owner.motionBlurAmount);
        if (postRadiusUniform != nullptr) postRadiusUniform->set (owner.motionBlurMax);
        if (postThresholdUniform != nullptr) postThresholdUniform->set (qualityF);
        if (postParamUniform != nullptr) postParamUniform->set (0.0f);
        if (postResolutionUniform != nullptr)
            postResolutionUniform->set ((float) width, (float) height);
        if (postMotionInvViewUniform != nullptr)
            postMotionInvViewUniform->setMatrix4 (invView, 1, false);
        if (postMotionPrevVpUniform != nullptr)
            postMotionPrevVpUniform->setMatrix4 (motionPrevVp, 1, false);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, sceneTex);
        if (postTexUniform != nullptr) postTexUniform->set (0);
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, softDepthTex);
        if (postDepthUniform != nullptr) postDepthUniform->set (1);
        glActiveTexture (GL_TEXTURE2);
        glBindTexture (GL_TEXTURE_2D, 0);
        if (postAuxUniform != nullptr) postAuxUniform->set (2);
        glDisable (GL_DEPTH_TEST);
        glDepthMask (GL_FALSE);
        glDisable (GL_BLEND);
        const float mbVerts[] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
        glBindBuffer (GL_ARRAY_BUFFER, tintVbo);
        glBufferData (GL_ARRAY_BUFFER, sizeof (mbVerts), mbVerts, GL_STREAM_DRAW);
        if (postPositionAttrib != nullptr && postPositionAttrib->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) postPositionAttrib->attributeID);
            glVertexAttribPointer ((GLuint) postPositionAttrib->attributeID, 2, GL_FLOAT, GL_FALSE,
                                   (GLsizei) (2 * sizeof (float)), nullptr);
        }
        glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
        if (postPositionAttrib != nullptr && postPositionAttrib->attributeID >= 0)
            glDisableVertexAttribArray ((GLuint) postPositionAttrib->attributeID);
        glBindBuffer (GL_ARRAY_BUFFER, 0);
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, 0);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, 0);
        postFboA.releaseAsRenderingTarget();

        drawFs (0, nullptr, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
    }

    // Grid / ticks / labels after SSGI+SSR+bloom - chrome must not feed GI/bloom.
    // Draw into the MSAA soft target when available so thin ribbons resolve cleanly
    // (drawing only on the resolved softFbo left jaggies even with MSAA "on").
    {
        const int samples = effectiveMsaaSamples();
        const bool chromeMsaa = samples >= 2 && softMsaaFbo != 0;
        if (chromeMsaa)
        {
            // Seed MSAA buffer with the post-processed scene, then AA the chrome on top.
            const GLuint sceneFbo = (GLuint) softFbo.getFrameBufferID();
            glBindFramebuffer (GL_READ_FRAMEBUFFER, sceneFbo);
            glBindFramebuffer (GL_DRAW_FRAMEBUFFER, softMsaaFbo);
            glBlitFramebuffer (0, 0, width, height, 0, 0, width, height,
                               GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer (GL_FRAMEBUFFER, softMsaaFbo);
            glViewport (0, 0, width, height);
            glEnable (GL_MULTISAMPLE);
            drawGroundAndGrid();
            drawPlayheadTicks();
            drawFrequencyLabels();
            glBindFramebuffer (GL_READ_FRAMEBUFFER, softMsaaFbo);
            glBindFramebuffer (GL_DRAW_FRAMEBUFFER, sceneFbo);
            glBlitFramebuffer (0, 0, width, height, 0, 0, width, height,
                               GL_COLOR_BUFFER_BIT, GL_NEAREST);
            softFbo.makeCurrentRenderingTarget();
            glViewport (0, 0, width, height);
        }
        else
        {
            softFbo.makeCurrentRenderingTarget();
            glViewport (0, 0, width, height);
            drawGroundAndGrid();
            drawPlayheadTicks();
            drawFrequencyLabels();
        }
        // #region agent log
        if ((gSoftFrameCounter % 30) == 1)
        {
            agentDbgLog ("J", "applySsaoAndBloom", "overlays_after_bloom",
                         juce::String ("{\"labelDrawCalls\":") + juce::String (gLabelDrawCallsThisFrame)
                             + ",\"floorVerts\":" + juce::String (floorVertexCount)
                             + ",\"ssgi\":" + juce::String (owner.ssgiEnabled ? 1 : 0)
                             + ",\"bloom\":" + juce::String (owner.bloomEnabled ? 1 : 0)
                             + ",\"ssr\":" + juce::String (owner.ssrEnabled ? 1 : 0)
                             + ",\"noChromeBorder\":1"
                             + ",\"frame\":" + juce::String (gSoftFrameCounter)
                             + "}");
        }
        // #endregion
    }

    if (owner.dofEnabled)
    {
        const float qualityF = owner.dofQuality == ShadowQuality::low ? 0.0f
                             : (owner.dofQuality == ShadowQuality::high ? 2.0f : 1.0f);
        const float lensPower = owner.getDofLensPower();
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();
        // Mesh CoC -> dilate into soft-BG void -> gather (industry transparent-BG pattern).
        // uStrength = thin-lens power from F-Stop + Focal Length.
        drawFs (17, &postFboA, sceneTex, softDepthTex, 0,
                lensPower, owner.dofFocusDistance, qualityF, 0.0f,
                width, height, false);
        drawFs (19, &postFboB, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
                lensPower, 1.0f, qualityF, owner.dofCocDilate,
                width, height, false);
        // Mode 6: uParam = edge spill (dilate already in uAux from mode 19).
        drawFs (6, &postFboA, sceneTex, softDepthTex, (GLuint) postFboB.getTextureID(),
                lensPower, owner.dofFocusDistance, qualityF, owner.dofEdgeSpill,
                width, height, false);
        drawFs (0, nullptr, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
    }

    if (owner.tonemapEnabled)
    {
        const float exposureLin = std::pow (2.0f, owner.tonemapExposureStops);
        const float gradeF = (float) static_cast<int> (owner.colorGrade);
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();
        drawFs (13, &postFboA, sceneTex, softDepthTex, 0,
                exposureLin, 1.0f, gradeF, 0.0f, width, height, false);
        drawFs (0, nullptr, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
                1.0f, 1.0f, 0.0f, 0.0f, width, height, false);
    }

    // Store current view-projection for next frame's camera velocity.
    {
        const auto proj = getProjectionMatrix();
        const auto view = getViewMatrix();
        const auto currVp = proj * view;
        std::memcpy (motionPrevVp, currVp.mat, sizeof (motionPrevVp));
        motionPrevVpValid = true;
    }

    softFbo.makeCurrentRenderingTarget();
    glViewport (0, 0, width, height);
    glDepthMask (GL_TRUE);
    glEnable (GL_DEPTH_TEST);
}

void Spectrogram3DComponent::GlHost::drawFrequencyLabels()
{
    using namespace juce::gl;
    if (labelShader == nullptr || ! labelAtlasReady || owner.freqLabels.empty() || labelVbo == 0)
        return;

    // #region agent log
    ++gLabelDrawCallsThisFrame;
    // #endregion

    struct V { float x, y, z, u, v; };
    std::vector<V> quads;
    quads.reserve (owner.freqLabels.size() * 6);

    const auto viewRect = owner.getGlViewLocal();
    const int viewW = juce::jmax (1, viewRect.getWidth());
    const int viewH = juce::jmax (1, viewRect.getHeight());
    // Match prior on-screen size (~52x22 px) at the label's view depth.
    constexpr float kTanHalfW = 1.0f / 1.5f;
    const float tanHalfH = kTanHalfW * ((float) viewH / (float) viewW);
    const float ndcHalfW = 52.0f / (float) viewW;
    const float ndcHalfH = 22.0f / (float) viewH * 0.85f;

    // Cylindrical billboard: horizontal = camera right (Y=0), vertical = world up.
    // Full camera-facing quads pitched with the view and sliced through the waterfall,
    // which looked like stacks of ghost labels even with DOF off.
    juce::Vector3D<float> right, camUp, forward;
    owner.cameraBasis (right, camUp, forward);
    juce::ignoreUnused (camUp);
    const juce::Vector3D<float> up { 0.0f, 1.0f, 0.0f };
    const auto viewMat = getViewMatrix();

    for (const auto& lb : owner.freqLabels)
    {
        // Just past the playhead edge - slightly above the grid.
        constexpr float kLabelWorldX = 1.008f;
        constexpr float kLabelWorldY = -0.006f;
        const float ax = kLabelWorldX;
        const float ay = kLabelWorldY;
        const float az = lb.worldZ;

        // View-space Z (positive distance) for screen-constant sizing.
        const float eyeX = viewMat.mat[0] * ax + viewMat.mat[4] * ay + viewMat.mat[8]  * az + viewMat.mat[12];
        const float eyeY = viewMat.mat[1] * ax + viewMat.mat[5] * ay + viewMat.mat[9]  * az + viewMat.mat[13];
        const float eyeZ = viewMat.mat[2] * ax + viewMat.mat[6] * ay + viewMat.mat[10] * az + viewMat.mat[14];
        juce::ignoreUnused (eyeX, eyeY);
        const float viewDist = juce::jmax (0.05f, -eyeZ);
        const float halfW = ndcHalfW * kTanHalfW * viewDist;
        const float halfH = ndcHalfH * tanHalfH * viewDist;
        // Nudge in XZ toward the camera (keep Y) so depth wins vs floor without lifting glyphs.
        const float fXZ = juce::jmax (1.0e-6f,
                                      std::sqrt (forward.x * forward.x + forward.z * forward.z));
        constexpr float bias = 0.012f;
        const float cx = ax - forward.x / fXZ * bias;
        const float cy = ay;
        const float cz = az - forward.z / fXZ * bias;

        auto corner = [&] (float sx, float sy) -> V
        {
            return { cx + right.x * sx * halfW + up.x * sy * halfH,
                     cy + right.y * sx * halfW + up.y * sy * halfH,
                     cz + right.z * sx * halfW + up.z * sy * halfH,
                     0.0f, 0.0f };
        };

        V c00 = corner (0.0f, -1.0f); c00.u = lb.u0; c00.v = lb.v0;
        V c10 = corner (2.0f, -1.0f); c10.u = lb.u1; c10.v = lb.v0;
        V c11 = corner (2.0f,  1.0f); c11.u = lb.u1; c11.v = lb.v1;
        V c01 = corner (0.0f,  1.0f); c01.u = lb.u0; c01.v = lb.v1;

        quads.push_back (c00);
        quads.push_back (c10);
        quads.push_back (c11);
        quads.push_back (c00);
        quads.push_back (c11);
        quads.push_back (c01);

        // #region agent log
        // Log first + 100Hz every ~15 frames: yMin below gridY (-0.01) => unoccluded ghosts (F).
        if (gLabelDrawCallsThisFrame == 1 && (gSoftFrameCounter % 15) == 1
            && (&lb == &owner.freqLabels.front() || std::abs (lb.hz - 100.0f) < 0.5f))
        {
            const float yMin = juce::jmin (c00.y, juce::jmin (c10.y, juce::jmin (c11.y, c01.y)));
            const float yMax = juce::jmax (c00.y, juce::jmax (c10.y, juce::jmax (c11.y, c01.y)));
            const float xMin = juce::jmin (c00.x, juce::jmin (c10.x, juce::jmin (c11.x, c01.x)));
            const float xMax = juce::jmax (c00.x, juce::jmax (c10.x, juce::jmax (c11.x, c01.x)));
            constexpr float kGridY = -0.010f;
            agentDbgLog ("F", "drawFrequencyLabels", "label_billboard_extent",
                         juce::String ("{\"hz\":") + juce::String (lb.hz, 1)
                             + ",\"yMin\":" + juce::String (yMin, 4)
                             + ",\"yMax\":" + juce::String (yMax, 4)
                             + ",\"belowGrid\":" + juce::String (yMin < kGridY ? 1 : 0)
                             + ",\"gridY\":" + juce::String (kGridY, 3)
                             + ",\"xMin\":" + juce::String (xMin, 4)
                             + ",\"xMax\":" + juce::String (xMax, 4)
                             + ",\"xSpan\":" + juce::String (xMax - xMin, 4)
                             + ",\"halfW\":" + juce::String (halfW, 4)
                             + ",\"halfH\":" + juce::String (halfH, 4)
                             + ",\"rightX\":" + juce::String (right.x, 3)
                             + ",\"rightZ\":" + juce::String (right.z, 3)
                             + ",\"labelCount\":" + juce::String ((int) owner.freqLabels.size())
                             + ",\"softBg\":" + juce::String (owner.usesSoftComposite() ? 1 : 0)
                             + "}");
        }
        // #endregion
    }

    if (quads.empty())
        return;

    glBindBuffer (GL_ARRAY_BUFFER, labelVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (quads.size() * sizeof (V)), quads.data(), GL_DYNAMIC_DRAW);

    labelShader->use();
    setCornerUniforms (*labelShader);
    if (labelProjectionUniform != nullptr)
        labelProjectionUniform->setMatrix4 (getProjectionMatrix().mat, 1, false);
    if (labelViewUniform != nullptr)
        labelViewUniform->setMatrix4 (viewMat.mat, 1, false);
    if (labelTexUniform != nullptr)
        labelTexUniform->set (0);
    labelAtlas.bind();
    glActiveTexture (GL_TEXTURE0);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_DEPTH_TEST);
    // Write depth so DOF CoC uses the label plane, not cleared far-plane depth.
    glDepthMask (GL_TRUE);
    glDepthFunc (GL_LEQUAL);
    glDisable (GL_CULL_FACE);

    const GLsizei stride = (GLsizei) sizeof (V);
    if (labelPositionAttrib != nullptr && labelPositionAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) labelPositionAttrib->attributeID);
        glVertexAttribPointer ((GLuint) labelPositionAttrib->attributeID, 3, GL_FLOAT, GL_FALSE, stride, nullptr);
    }
    if (labelTexAttrib != nullptr && labelTexAttrib->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) labelTexAttrib->attributeID);
        glVertexAttribPointer ((GLuint) labelTexAttrib->attributeID, 2, GL_FLOAT, GL_FALSE,
                               stride, (const void*) (sizeof (float) * 3));
    }

    glDrawArrays (GL_TRIANGLES, 0, (GLsizei) quads.size());

    if (labelPositionAttrib != nullptr && labelPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) labelPositionAttrib->attributeID);
    if (labelTexAttrib != nullptr && labelTexAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) labelTexAttrib->attributeID);

    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glDepthMask (GL_TRUE);
    glDepthFunc (GL_LESS);
    glDisable (GL_BLEND);
}

void Spectrogram3DComponent::GlHost::renderOpenGL()
{
    using namespace juce::gl;

    if (! glReady || colourShader == nullptr)
        return;

    if (owner.usesSoftComposite())
    {
        renderSoftComposite();
        // Tiny peer framebuffer is unused for display in soft mode.
        glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    if (getWidth() < 2 || getHeight() < 2)
        return;

    juce::OpenGLHelpers::clear (owner.getClearColour());
    glClear (GL_DEPTH_BUFFER_BIT);

    if (owner.msaaLevel != MsaaLevel::off)
        glEnable (GL_MULTISAMPLE);

    uploadMeshIfNeeded();
    ensureFloorGeometry();
    ensureLabelAtlas();
    uploadDomeTextureIfNeeded();

    const auto px = getViewPixelBounds();
    glViewport (0, 0, px.getWidth(), px.getHeight());

    renderShadowDepthPass();
    drawContactShadow();
    drawSpectrogramSurface();
    drawDebugSphere();
    drawDebugGizmo();
    drawGroundAndGrid();
    drawFrequencyLabels();
}

void Spectrogram3DComponent::GlHost::mouseDown (const juce::MouseEvent& e)
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();
    owner.handleMouseDown (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::GlHost::mouseMove (const juce::MouseEvent& e)
{
    owner.handleMouseMove (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::GlHost::mouseExit (const juce::MouseEvent&)
{
    owner.handleMouseExit();
}

void Spectrogram3DComponent::GlHost::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleMouseDrag (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::GlHost::mouseUp (const juce::MouseEvent& e)
{
    owner.handleMouseUp (e);
}

void Spectrogram3DComponent::GlHost::mouseWheelMove (const juce::MouseEvent&,
                                                    const juce::MouseWheelDetails& wheel)
{
    owner.handleMouseWheel (wheel);
}

void Spectrogram3DComponent::GlHost::mouseDoubleClick (const juce::MouseEvent&)
{
    owner.handleDoubleClick();
}

bool Spectrogram3DComponent::GlHost::keyPressed (const juce::KeyPress& key)
{
    return owner.keyPressed (key);
}

bool Spectrogram3DComponent::GlHost::keyStateChanged (bool isKeyDown)
{
    return owner.keyStateChanged (isKeyDown);
}

//==============================================================================
Spectrogram3DComponent::HitLayer::HitLayer (Spectrogram3DComponent& o)
    : owner (o)
{
    setOpaque (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);
}

void Spectrogram3DComponent::HitLayer::mouseDown (const juce::MouseEvent& e)
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();
    owner.handleMouseDown (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::HitLayer::mouseMove (const juce::MouseEvent& e)
{
    owner.handleMouseMove (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::HitLayer::mouseExit (const juce::MouseEvent&)
{
    owner.handleMouseExit();
}

void Spectrogram3DComponent::HitLayer::mouseDrag (const juce::MouseEvent& e)
{
    owner.handleMouseDrag (e.getEventRelativeTo (&owner));
}

void Spectrogram3DComponent::HitLayer::mouseUp (const juce::MouseEvent& e)
{
    owner.handleMouseUp (e);
}

void Spectrogram3DComponent::HitLayer::mouseWheelMove (const juce::MouseEvent&,
                                                       const juce::MouseWheelDetails& wheel)
{
    owner.handleMouseWheel (wheel);
}

void Spectrogram3DComponent::HitLayer::mouseDoubleClick (const juce::MouseEvent&)
{
    owner.handleDoubleClick();
}

bool Spectrogram3DComponent::HitLayer::keyPressed (const juce::KeyPress& key)
{
    return owner.keyPressed (key);
}

bool Spectrogram3DComponent::HitLayer::keyStateChanged (bool isKeyDown)
{
    return owner.keyStateChanged (isKeyDown);
}

//==============================================================================
namespace
{
    /** Translucent grip for Soft BG / Direct2D - default corner resizer paints as a black box. */
    class SoftResizeCorner final : public juce::ResizableCornerComponent
    {
    public:
        SoftResizeCorner (juce::Component* componentToResize,
                          juce::ComponentBoundsConstrainer* constrainerToUse)
            : juce::ResizableCornerComponent (componentToResize, constrainerToUse)
        {
            setOpaque (false);
            setMouseCursor (juce::MouseCursor::BottomRightCornerResizeCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.5f);
            const bool hot = isMouseOverOrDragging();
            const float alpha = hot ? 0.85f : 0.50f;
            g.setColour (juce::Colours::whitesmoke.withAlpha (alpha));
            for (int i = 0; i < 3; ++i)
            {
                const float o = 3.0f + (float) i * 3.6f;
                g.drawLine (b.getRight() - o, b.getBottom(),
                            b.getRight(), b.getBottom() - o, hot ? 1.6f : 1.35f);
            }
            g.setColour (juce::Colours::goldenrod.withAlpha (hot ? 0.55f : 0.28f));
            g.drawLine (b.getRight() - 5.0f, b.getBottom(),
                        b.getRight(), b.getBottom() - 5.0f, 1.1f);
        }
    };

    /** Top-right magnifying glass - click-drag vertically for wheel-equivalent zoom. */
    class ZoomHandleOverlay final : public juce::Component
    {
    public:
        explicit ZoomHandleOverlay (Spectrogram3DComponent& o)
            : owner (o)
        {
            setOpaque (false);
            setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
            setRepaintsOnMouseActivity (true);
        }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            const bool hot = isMouseOverOrDragging();
            const float plateA = hot ? 0.55f : 0.38f;
            g.setColour (juce::Colours::black.withAlpha (plateA));
            g.fillEllipse (b.reduced (1.0f));
            g.setColour (juce::Colours::white.withAlpha (hot ? 0.28f : 0.16f));
            g.drawEllipse (b.reduced (1.0f), 1.0f);

            // Lens
            const float lensR = b.getWidth() * 0.22f;
            const juce::Point<float> lensC (b.getCentreX() - b.getWidth() * 0.06f,
                                            b.getCentreY() - b.getHeight() * 0.06f);
            g.setColour (juce::Colours::whitesmoke.withAlpha (hot ? 0.95f : 0.78f));
            g.drawEllipse (lensC.x - lensR, lensC.y - lensR, lensR * 2.0f, lensR * 2.0f,
                           hot ? 2.0f : 1.6f);

            // Handle
            const float hx0 = lensC.x + lensR * 0.65f;
            const float hy0 = lensC.y + lensR * 0.65f;
            const float hx1 = b.getRight() - b.getWidth() * 0.18f;
            const float hy1 = b.getBottom() - b.getHeight() * 0.18f;
            g.drawLine (hx0, hy0, hx1, hy1, hot ? 2.4f : 2.0f);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            dragging = e.mods.isLeftButtonDown() && ! e.mods.isPopupMenu();
            lastY = e.position.y;
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (! dragging)
                return;
            const float dy = e.position.y - lastY;
            lastY = e.position.y;
            owner.applyUiZoomDrag (dy);
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            dragging = false;
        }

    private:
        Spectrogram3DComponent& owner;
        bool dragging = false;
        float lastY = 0.0f;
    };
}

//==============================================================================
Spectrogram3DComponent::Spectrogram3DComponent()
{
    setOpaque (false);
    setVisible (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    glHost = std::make_unique<GlHost> (*this);
    addChildComponent (*glHost);

    hitLayer = std::make_unique<HitLayer> (*this);
    addChildComponent (*hitLayer);
    applyBackgroundTransparency();

    constrainer.setMinimumSize (220 + kShadowPadFloating * 2, 160 + kShadowPadFloating * 2);
    constrainer.setMaximumSize (4000, 3000);
    resizer = std::make_unique<SoftResizeCorner> (this, &constrainer);
    addAndMakeVisible (*resizer);
    resizer->setAlwaysOnTop (true);

    zoomHandle = std::make_unique<ZoomHandleOverlay> (*this);
    addChildComponent (*zoomHandle);
    zoomHandle->setAlwaysOnTop (true);

    defaultCamera = getFactoryCameraState();
    camera = defaultCamera;
    initDefaultParticleModSlots();
    initDefaultParticleForceStack();
    applyChromeMode();
}

Spectrogram3DComponent::~Spectrogram3DComponent()
{
    stopTimer();
    zoomHandle.reset();
    resizer.reset();
    hitLayer.reset();
    glHost.reset();
}

Spectrogram3DComponent::CameraState Spectrogram3DComponent::getFactoryCameraState() noexcept
{
    // ¾ view from above: pitch = elevation above the floor horizon (not a tilted orbit axis).
    // Orbit pivot = centre of the mesh volume; distance places the eye ~3x height above peaks.
    constexpr float pitchDeg = 35.0f;
    constexpr float lookY = kDefaultMeshHeight * 0.5f;
    constexpr float eyeHeightAbovePeaks = 3.0f * kDefaultMeshHeight;
    const float eyeY = kDefaultMeshHeight + eyeHeightAbovePeaks;
    const float pitchRad = juce::degreesToRadians (pitchDeg);
    const float distance = (eyeY - lookY) / juce::jmax (0.25f, std::sin (pitchRad));
    return { -40.0f, pitchDeg, distance, 0.0f, lookY, 0.0f };
}

void Spectrogram3DComponent::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    repaint();
}

void Spectrogram3DComponent::setResizeLimits (int maxW, int maxH) noexcept
{
    const int pad = getShadowPad() * 2;
    constrainer.setMinimumSize (220 + pad, 160 + pad);
    constrainer.setMaximumSize (juce::jmax (220 + pad, maxW),
                                juce::jmax (160 + pad, maxH));
}

void Spectrogram3DComponent::setMovementBounds (juce::Rectangle<int> parentLocalBounds) noexcept
{
    constrainer.setMinimumOnscreenAmounts (24, 24, 24, 24);
    // Keep the framed window inside the available content area.
    if (auto* parent = getParentComponent())
    {
        juce::ignoreUnused (parent);
        constrainer.setSizeLimits (constrainer.getMinimumWidth(),
                                   constrainer.getMinimumHeight(),
                                   juce::jmax (constrainer.getMinimumWidth(), parentLocalBounds.getWidth()),
                                   juce::jmax (constrainer.getMinimumHeight(), parentLocalBounds.getHeight()));
    }
}

int Spectrogram3DComponent::getShadowPad() const noexcept
{
    return getShadowPadForMode (chromeMode);
}

int Spectrogram3DComponent::getShadowPadForMode (ChromeMode mode) noexcept
{
    return mode == ChromeMode::docked ? kShadowPadDocked : kShadowPadFloating;
}

void Spectrogram3DComponent::setChromeMode (ChromeMode mode) noexcept
{
    if (chromeMode == mode)
    {
        // Soft ↔ hard peer layout can still be stale after Soft BG toggles / Scope entry.
        applyBackgroundTransparency();
        return;
    }
    chromeMode = mode;
    applyChromeMode();
    applyBackgroundTransparency();
    markSoftContentDirty();
    resized();
    repaint();
}

void Spectrogram3DComponent::applyChromeMode() noexcept
{
    const bool floating = (chromeMode == ChromeMode::floating);
    if (resizer != nullptr)
        resizer->setVisible (floating && active);
}

void Spectrogram3DComponent::setMeshHeight (float heightWorld) noexcept
{
    const float h = juce::jlimit (kMinMeshHeight, kMaxMeshHeight, heightWorld);
    if (std::abs (h - meshHeight) < 1.0e-4f)
        return;
    const float oldH = meshHeight;
    meshHeight = h;
    // Keep the orbit pivot at the same relative height in the volume.
    if (oldH > 1.0e-4f)
    {
        const float t = camera.panY / oldH;
        camera.panY = t * h;
        defaultCamera.panY = (defaultCamera.panY / oldH) * h;
    }
    else
    {
        camera.panY = lookAtY();
        defaultCamera.panY = lookAtY();
    }
    // Rescale existing history in place - do not wipe meshDb.
    if (meshW >= 2 && meshH >= 2 && ! meshDb.empty() && lastBrightness >= 0.0f)
        rebuildVerticesFromMeshDb (lastBrightness, lastMinDb, lastMaxDb);
    markLookDirty();
}

void Spectrogram3DComponent::setActive (bool shouldBeActive) noexcept
{
    const bool changed = (active != shouldBeActive);
    active = shouldBeActive;
    setAlwaysOnTop (false);
    // Always re-apply visibility - Scope layout clears it before place, and an
    // early-return here used to leave the docked 3D pane permanently hidden.
    setVisible (active);

    if (! changed)
        return; // Look / prefs sync must not wipe or re-seed history.

    applyChromeMode();
    layoutPresentation();

    if (active)
    {
        if (glHost != nullptr)
            glHost->resetAttachState();
        clampCamera();
        markSoftContentDirty();
        syncSpec3DTimerRate();
        updateMeshFromSource();
    }
    else
    {
        stopTimer();
        // Free GPU resources so a game / other app can reclaim the device.
        if (glHost != nullptr)
        {
            if (glHost->getOpenGLContext().isAttached())
                glHost->getOpenGLContext().detach();
            glHost->resetAttachState();
        }
    }

    repaint();
}

void Spectrogram3DComponent::setMeshQuality (MeshQuality q) noexcept
{
    if (q == MeshQuality::overkill)
        q = MeshQuality::ultra; // removed - was crashy / too heavy
    if (meshQuality == q)
        return;
    meshQuality = q;
    invalidateMesh();
    if (active)
        updateMeshFromSource();
}

void Spectrogram3DComponent::setFreqMeshBias (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (freqMeshBias - amount01) < 1.0e-4f)
        return;
    freqMeshBias = amount01;
    invalidateMesh();
    if (active)
        updateMeshFromSource();
}

void Spectrogram3DComponent::setFreqMeshBiasPivot (float pivot01) noexcept
{
    pivot01 = juce::jlimit (0.0f, 0.95f, pivot01);
    if (std::abs (freqMeshBiasPivot - pivot01) < 1.0e-4f)
        return;
    freqMeshBiasPivot = pivot01;
    invalidateMesh();
    if (active)
        updateMeshFromSource();
}

void Spectrogram3DComponent::setMsaaLevel (MsaaLevel level) noexcept
{
    if (level != MsaaLevel::off && level != MsaaLevel::x4
        && level != MsaaLevel::x8 && level != MsaaLevel::x16)
        level = MsaaLevel::x4;

    if (msaaLevel == level)
        return;

    msaaLevel = level;
    if (glHost != nullptr)
    {
        glHost->reattachWithCurrentFormat();
        glHost->markSoftContentDirty();
        glHost->triggerRedraw();
    }
    repaint();
}

void Spectrogram3DComponent::setTransparentBackground (bool shouldEnable) noexcept
{
    if (transparentBackground == shouldEnable)
        return;

    transparentBackground = shouldEnable;
    applyBackgroundTransparency();
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::ensureParticleSystem()
{
    if (particleSystem == nullptr)
        particleSystem = std::make_unique<Spec3DParticleSystem> (*this);
    if (particleSystem != nullptr)
        particleSystem->setMaxAliveBudget (particleMaxAlive);
}

void Spectrogram3DComponent::setParticleModeEnabled (bool shouldEnable) noexcept
{
    if (particleModeEnabled == shouldEnable)
    {
        // Still clear when forcing off (toggle / prefs) so a stuck cloud cannot linger.
        if (! shouldEnable && particleSystem != nullptr)
            particleSystem->clear();
        return;
    }
    particleModeEnabled = shouldEnable;
    syncSpec3DTimerRate();
    if (particleModeEnabled)
        ensureParticleSystem();
    else if (particleSystem != nullptr)
        particleSystem->clear();
    markLookDirty();
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmitMode (ParticleEmitMode mode) noexcept
{
    if (mode != ParticleEmitMode::slice && mode != ParticleEmitMode::continuous)
        mode = ParticleEmitMode::slice;
    if (particleEmitMode == mode) return;
    particleEmitMode = mode;
    // Drop cross-mode spawn backlog so the new mode takes effect immediately.
    // Do not clear live particles - that made Continuous feel "broken" on CPU.
    if (particleSystem != nullptr)
        particleSystem->resetEmissionAccumulators();
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmitSurface (ParticleEmitSurface surface) noexcept
{
    // History-field emit is reserved for a future UI path. Force playhead so GPU/CPU
    // always birth only at the live tip (trail fills by scrolling).
    if (surface != ParticleEmitSurface::playhead && surface != ParticleEmitSurface::historyField)
        surface = ParticleEmitSurface::playhead;
    surface = ParticleEmitSurface::playhead;
    if (particleEmitSurface == surface) return;
    particleEmitSurface = surface;
    if (particleSystem != nullptr)
        particleSystem->resetEmissionAccumulators();
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmitterType (ParticleEmitterType type) noexcept
{
    if (type < ParticleEmitterType::spectrogram || type > ParticleEmitterType::cone)
        type = ParticleEmitterType::spectrogram;
    if (particleEmitterType == type) return;
    particleEmitterType = type;
    // Geometric emitters need free binding (no waterfall column lock).
    if (type != ParticleEmitterType::spectrogram
        && particleBindingMode == ParticleBindingMode::spectrogramTrail)
        particleBindingMode = ParticleBindingMode::freeVisualizer;
    if (particleSystem != nullptr)
        particleSystem->resetEmissionAccumulators();
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmitDomain (ParticleEmitDomain domain) noexcept
{
    if (domain != ParticleEmitDomain::surface && domain != ParticleEmitDomain::volume)
        domain = ParticleEmitDomain::surface;
    if (particleEmitDomain == domain) return;
    particleEmitDomain = domain;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmitterPos (float x, float y, float z) noexcept
{
    particleEmitterPosX = x;
    particleEmitterPosY = y;
    particleEmitterPosZ = z;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSprayYawDeg (float deg) noexcept
{
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    if (std::abs (particleSprayYawDeg - deg) < 1.0e-4f) return;
    particleSprayYawDeg = deg;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSprayPitchDeg (float deg) noexcept
{
    deg = juce::jlimit (-90.0f, 90.0f, deg);
    if (std::abs (particleSprayPitchDeg - deg) < 1.0e-4f) return;
    particleSprayPitchDeg = deg;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSpraySpreadDeg (float deg) noexcept
{
    deg = juce::jlimit (0.0f, 180.0f, deg);
    if (std::abs (particleSpraySpreadDeg - deg) < 1.0e-4f) return;
    particleSpraySpreadDeg = deg;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSpraySpeedMin (float unitsPerSec) noexcept
{
    if (! std::isfinite (unitsPerSec)) return;
    unitsPerSec = juce::jmax (0.0f, unitsPerSec);
    if (std::abs (particleSpraySpeedMin - unitsPerSec) < 1.0e-6f) return;
    particleSpraySpeedMin = unitsPerSec;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSpraySpeedMax (float unitsPerSec) noexcept
{
    if (! std::isfinite (unitsPerSec)) return;
    unitsPerSec = juce::jmax (0.0f, unitsPerSec);
    if (std::abs (particleSpraySpeedMax - unitsPerSec) < 1.0e-6f) return;
    particleSpraySpeedMax = unitsPerSec;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleBindingMode (ParticleBindingMode mode) noexcept
{
    if (mode != ParticleBindingMode::spectrogramTrail && mode != ParticleBindingMode::freeVisualizer)
        mode = ParticleBindingMode::spectrogramTrail;
    if (particleBindingMode == mode) return;
    particleBindingMode = mode;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmission (float amount) noexcept
{
    // No artistic cap - extreme values allowed via manual text entry.
    if (! std::isfinite (amount)) return;
    if (std::abs (particleEmission - amount) < 1.0e-6f) return;
    particleEmission = amount;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSpawnJitter (float amount) noexcept
{
    if (! std::isfinite (amount)) return;
    if (std::abs (particleSpawnJitter - amount) < 1.0e-6f) return;
    particleSpawnJitter = amount;
}

void Spectrogram3DComponent::setParticleEmitCatchupHz (float hz) noexcept
{
    if (! std::isfinite (hz)) return;
    hz = juce::jlimit (15.0f, 240.0f, hz);
    if (std::abs (particleEmitCatchupHz - hz) < 1.0e-4f) return;
    particleEmitCatchupHz = hz;
}

void Spectrogram3DComponent::setParticleSimCatchupHz (float hz) noexcept
{
    if (! std::isfinite (hz)) return;
    hz = juce::jlimit (10.0f, 120.0f, hz);
    if (std::abs (particleSimCatchupHz - hz) < 1.0e-4f) return;
    particleSimCatchupHz = hz;
}

void Spectrogram3DComponent::setParticleSliceBacklogSec (float seconds) noexcept
{
    if (! std::isfinite (seconds)) return;
    seconds = juce::jlimit (0.02f, 2.0f, seconds);
    if (std::abs (particleSliceBacklogSec - seconds) < 1.0e-5f) return;
    particleSliceBacklogSec = seconds;
}

void Spectrogram3DComponent::initDefaultParticleModSlots() noexcept
{
    for (auto& s : particleModSlots)
        s = {};

    // Slot 0: amplitude -> emission (off by default - enable in matrix UI)
    particleModSlots[0].enabled = false;
    particleModSlots[0].source = ParticleModSource::amplitude;
    particleModSlots[0].dest = ParticleModDest::emission;
    particleModSlots[0].op = ParticleModOp::multiply;
    particleModSlots[0].amount = 1.0f;

    // Slot 1: bin dB -> colour gain
    particleModSlots[1].enabled = false;
    particleModSlots[1].source = ParticleModSource::binDb;
    particleModSlots[1].dest = ParticleModDest::colourGain;
    particleModSlots[1].op = ParticleModOp::multiply;
    particleModSlots[1].amount = 1.0f;

    // Slot 2: bin freq -> colour hue
    particleModSlots[2].enabled = false;
    particleModSlots[2].source = ParticleModSource::binFreq;
    particleModSlots[2].dest = ParticleModDest::colourHue;
    particleModSlots[2].op = ParticleModOp::set;
    particleModSlots[2].amount = 1.0f;
}

ParticleModSlot Spectrogram3DComponent::getParticleModSlot (int index) const noexcept
{
    if (juce::isPositiveAndBelow (index, kParticleModSlotCount))
        return particleModSlots[(size_t) index];
    return {};
}

void Spectrogram3DComponent::setParticleModSlot (int index, const ParticleModSlot& slot) noexcept
{
    if (! juce::isPositiveAndBelow (index, kParticleModSlotCount))
        return;
    // No artistic clamps - manual extremes (amount, map range, constant, thr, A/R) must stick.
    if (! std::isfinite (slot.amount) || ! std::isfinite (slot.constant)
        || ! std::isfinite (slot.mapMin) || ! std::isfinite (slot.mapMax)
        || ! std::isfinite (slot.curveShape) || ! std::isfinite (slot.threshold)
        || ! std::isfinite (slot.attackMs) || ! std::isfinite (slot.releaseMs))
        return;
    particleModSlots[(size_t) index] = slot;
    markSoftContentDirty();
}

ParticleRandomSource Spectrogram3DComponent::getParticleRandomSource (int index) const noexcept
{
    if (juce::isPositiveAndBelow (index, kParticleRandomSourceCount))
        return particleRandomSources[(size_t) index];
    return {};
}

void Spectrogram3DComponent::setParticleRandomSource (int index, const ParticleRandomSource& src) noexcept
{
    if (! juce::isPositiveAndBelow (index, kParticleRandomSourceCount))
        return;
    if (! std::isfinite (src.minV) || ! std::isfinite (src.maxV) || ! std::isfinite (src.smoothMs))
        return;
    particleRandomSources[(size_t) index] = src;
}

void Spectrogram3DComponent::setParticleForcesEnabled (bool e) noexcept
{
    if (particleForcesEnabled == e) return;
    particleForcesEnabled = e;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleWaterfallLock (bool e) noexcept
{
    particleWaterfallLock = e;
}

void Spectrogram3DComponent::initDefaultParticleForceStack() noexcept
{
    particleForceStack.clear();
    particleForceStack.push_back (makeDefaultForceModule (ParticleForceType::gravity, nextParticleForceUid()));
    particleForceStack.back().enabled = false;
    particleForceStack.back().p[0] = 0.0f;
    particleForceStack.push_back (makeDefaultForceModule (ParticleForceType::drag, nextParticleForceUid()));
    particleForceStack.back().enabled = false;
    particleForceStack.back().p[0] = 0.0f;
}

ParticleForceModule Spectrogram3DComponent::getParticleForceModule (int index) const noexcept
{
    if (juce::isPositiveAndBelow (index, (int) particleForceStack.size()))
        return particleForceStack[(size_t) index];
    return {};
}

void Spectrogram3DComponent::setParticleForceModule (int index, const ParticleForceModule& m) noexcept
{
    if (! juce::isPositiveAndBelow (index, (int) particleForceStack.size()))
        return;
    particleForceStack[(size_t) index] = m;
    markSoftContentDirty();
}

void Spectrogram3DComponent::addParticleForceModule (ParticleForceType type) noexcept
{
    if ((int) particleForceStack.size() >= kParticleForceStackMax)
        return;
    particleForceStack.push_back (makeDefaultForceModule (type, nextParticleForceUid()));
    markSoftContentDirty();
}

void Spectrogram3DComponent::removeParticleForceModule (int index) noexcept
{
    if (! juce::isPositiveAndBelow (index, (int) particleForceStack.size()))
        return;
    particleForceStack.erase (particleForceStack.begin() + index);
    markSoftContentDirty();
}

void Spectrogram3DComponent::moveParticleForceModule (int from, int to) noexcept
{
    const int n = (int) particleForceStack.size();
    if (! juce::isPositiveAndBelow (from, n) || ! juce::isPositiveAndBelow (to, n) || from == to)
        return;
    auto m = particleForceStack[(size_t) from];
    particleForceStack.erase (particleForceStack.begin() + from);
    particleForceStack.insert (particleForceStack.begin() + to, m);
    markSoftContentDirty();
}

void Spectrogram3DComponent::clearParticleForceModules() noexcept
{
    particleForceStack.clear();
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleForceStack (std::vector<ParticleForceModule> stack) noexcept
{
    if (stack.size() > (size_t) kParticleForceStackMax)
        stack.resize ((size_t) kParticleForceStackMax);
    for (auto& m : stack)
        if (m.uid == 0)
            m.uid = nextParticleForceUid();
    particleForceStack = std::move (stack);
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleGraphProgram (const ParticleNodeGraph::GraphProgram& program) noexcept
{
    particleGraphProgram = program;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleMeshShape (ParticleMeshShape s) noexcept
{
    if (s != ParticleMeshShape::sphere && s != ParticleMeshShape::cube
        && s != ParticleMeshShape::billboard)
        s = ParticleMeshShape::sphere;
    if (particleMeshShape == s) return;
    particleMeshShape = s;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleInitRotX (float deg) noexcept
{
    if (std::isfinite (deg)) particleInitRotX = deg;
}
void Spectrogram3DComponent::setParticleInitRotY (float deg) noexcept
{
    if (std::isfinite (deg)) particleInitRotY = deg;
}
void Spectrogram3DComponent::setParticleInitRotZ (float deg) noexcept
{
    if (std::isfinite (deg)) particleInitRotZ = deg;
}
void Spectrogram3DComponent::setParticleInitRotRandom (float amount01) noexcept
{
    if (std::isfinite (amount01)) particleInitRotRandom = amount01;
}

void Spectrogram3DComponent::setParticleInitVelX (float unitsPerSec) noexcept
{
    if (! std::isfinite (unitsPerSec)) return;
    if (std::abs (particleInitVelX - unitsPerSec) < 1.0e-6f) return;
    particleInitVelX = unitsPerSec;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleInitVelY (float unitsPerSec) noexcept
{
    if (! std::isfinite (unitsPerSec)) return;
    if (std::abs (particleInitVelY - unitsPerSec) < 1.0e-6f) return;
    particleInitVelY = unitsPerSec;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleInitVelZ (float unitsPerSec) noexcept
{
    if (! std::isfinite (unitsPerSec)) return;
    if (std::abs (particleInitVelZ - unitsPerSec) < 1.0e-6f) return;
    particleInitVelZ = unitsPerSec;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleVelRandom (float amount01) noexcept
{
    if (! std::isfinite (amount01)) return;
    if (std::abs (particleVelRandom - amount01) < 1.0e-6f) return;
    particleVelRandom = amount01;
}

void Spectrogram3DComponent::setParticleLifespan (float seconds) noexcept
{
    if (! std::isfinite (seconds)) return;
    if (std::abs (particleLifespan - seconds) < 1.0e-6f) return;
    particleLifespan = seconds;
}

void Spectrogram3DComponent::setParticleLifespanRandom (float amount01) noexcept
{
    if (! std::isfinite (amount01)) return;
    if (std::abs (particleLifespanRandom - amount01) < 1.0e-6f) return;
    particleLifespanRandom = amount01;
}

void Spectrogram3DComponent::setParticleSize (float worldSize) noexcept
{
    if (! std::isfinite (worldSize)) return;
    if (std::abs (particleSize - worldSize) < 1.0e-7f) return;
    particleSize = worldSize;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSizeRandomMin (float scale) noexcept
{
    if (! std::isfinite (scale)) return;
    scale = juce::jlimit (0.05f, 4.0f, scale);
    if (std::abs (particleSizeRandomMin - scale) < 1.0e-6f) return;
    particleSizeRandomMin = scale;
}

void Spectrogram3DComponent::setParticleSizeRandomMax (float scale) noexcept
{
    if (! std::isfinite (scale)) return;
    scale = juce::jlimit (0.05f, 4.0f, scale);
    if (std::abs (particleSizeRandomMax - scale) < 1.0e-6f) return;
    particleSizeRandomMax = scale;
}

void Spectrogram3DComponent::setParticleEmissiveEnabled (bool shouldEnable) noexcept
{
    if (particleEmissiveEnabled == shouldEnable) return;
    particleEmissiveEnabled = shouldEnable;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleEmissiveStrength (float amount) noexcept
{
    if (! std::isfinite (amount)) return;
    amount = juce::jmax (0.0f, amount);
    if (std::abs (particleEmissiveStrength - amount) < 1.0e-6f) return;
    particleEmissiveStrength = amount;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleRoughness (float amount01) noexcept
{
    if (! std::isfinite (amount01)) return;
    amount01 = juce::jlimit (0.04f, 1.0f, amount01);
    if (std::abs (particleRoughness - amount01) < 1.0e-6f) return;
    particleRoughness = amount01;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleMetalness (float amount01) noexcept
{
    if (! std::isfinite (amount01)) return;
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (particleMetalness - amount01) < 1.0e-6f) return;
    particleMetalness = amount01;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleSpecular (float amount01) noexcept
{
    if (! std::isfinite (amount01)) return;
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (particleSpecular - amount01) < 1.0e-6f) return;
    particleSpecular = amount01;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setParticleGpuSimEnabled (bool shouldEnable) noexcept
{
    if (particleGpuSimEnabled == shouldEnable) return;
    particleGpuSimEnabled = shouldEnable;
    markSoftContentDirty();
}

bool Spectrogram3DComponent::isParticleGpuSimAvailable() const noexcept
{
    return particleSystem != nullptr && particleSystem->isGpuSimAvailable();
}

void Spectrogram3DComponent::setParticleMaxAlive (int maxAlive) noexcept
{
    // Budget only - must not pre-allocate 70k particles on the UI thread (host crash).
    maxAlive = juce::jlimit (Spec3DParticleSystem::kMinMaxAlive,
                             Spec3DParticleSystem::kHardCap,
                             maxAlive);
    if (particleMaxAlive == maxAlive)
        return;
    particleMaxAlive = maxAlive;
    // Only touch the system if it already exists (particle mode on). Creating it just to
    // store a budget is fine, but setMaxAliveBudget must stay allocation-free on raise.
    if (particleModeEnabled)
        ensureParticleSystem();
    if (particleSystem != nullptr)
        particleSystem->setMaxAliveBudget (particleMaxAlive);
    markSoftContentDirty();
}

int Spectrogram3DComponent::getParticleMaxAlive() const noexcept
{
    return particleMaxAlive;
}

int Spectrogram3DComponent::getParticleAliveCount() const noexcept
{
    return particleSystem != nullptr ? particleSystem->getAliveCount() : 0;
}

int Spectrogram3DComponent::getParticlePoolCapacity() const noexcept
{
    return particleSystem != nullptr ? particleSystem->getPoolCapacity() : 0;
}

int Spectrogram3DComponent::getParticleLastSpawnedCount() const noexcept
{
    return particleSystem != nullptr ? particleSystem->getLastSpawnedCount() : 0;
}

int Spectrogram3DComponent::getParticleLastCulledCount() const noexcept
{
    return particleSystem != nullptr ? particleSystem->getLastCulledCount() : 0;
}

float Spectrogram3DComponent::getParticleLastUpdateMs() const noexcept
{
    return particleSystem != nullptr ? particleSystem->getLastUpdateMs() : 0.0f;
}

int Spectrogram3DComponent::getParticleLoadLevel() const noexcept
{
    return particleSystem != nullptr ? particleSystem->getLoadLevel() : 0;
}

void Spectrogram3DComponent::clearParticles() noexcept
{
    if (particleSystem != nullptr)
        particleSystem->clear();
    markSoftContentDirty();
}

void Spectrogram3DComponent::syncSpec3DTimerRate() noexcept
{
    if (! active)
    {
        stopTimer();
        return;
    }
    // Particles / freecam: 60 Hz target. Idle waterfall mesh: 30 Hz is enough.
    const int hz = (particleModeEnabled || freecamActive || freecamRmbHeld) ? 60 : 30;
    if (! isTimerRunning() || getTimerInterval() != (1000 / hz))
        startTimerHz (hz);
}

void Spectrogram3DComponent::setParticleDebugOverlayEnabled (bool shouldShow) noexcept
{
    if (particleDebugOverlayEnabled == shouldShow)
        return;
    particleDebugOverlayEnabled = shouldShow;
    repaint();
}

void Spectrogram3DComponent::setLightingEnabled (bool shouldEnable) noexcept
{
    if (lightingEnabled == shouldEnable) return;
    lightingEnabled = shouldEnable;
    lastBrightness = -1.0f; // rebuild verts so weighted normals catch up
    markLookDirty();
}

void Spectrogram3DComponent::setLightingAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (lightingAmount - amount01) < 1.0e-4f) return;
    lightingAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setLightAzimuthDeg (float deg) noexcept
{
    while (deg > 180.0f) deg -= 360.0f;
    while (deg < -180.0f) deg += 360.0f;
    if (std::abs (lightAzimuthDeg - deg) < 1.0e-3f) return;
    lightAzimuthDeg = deg;
    markLookDirty();
}

void Spectrogram3DComponent::setLightElevationDeg (float deg) noexcept
{
    deg = juce::jlimit (5.0f, 89.0f, deg);
    if (std::abs (lightElevationDeg - deg) < 1.0e-3f) return;
    lightElevationDeg = deg;
    markLookDirty();
}

void Spectrogram3DComponent::setSpecularAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (specularAmount - amount01) < 1.0e-4f) return;
    specularAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setRoughnessAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.04f, 1.0f, amount01);
    if (std::abs (roughnessAmount - amount01) < 1.0e-4f) return;
    roughnessAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setMetalnessAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (metalnessAmount - amount01) < 1.0e-4f) return;
    metalnessAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setRimAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (rimAmount - amount01) < 1.0e-4f) return;
    rimAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setLightColour (juce::Colour c) noexcept
{
    if (lightColour == c) return;
    lightColour = c;
    markLookDirty();
}

void Spectrogram3DComponent::setRimColour (juce::Colour c) noexcept
{
    if (rimColour == c) return;
    rimColour = c;
    markLookDirty();
}

void Spectrogram3DComponent::setDomeFillEnabled (bool shouldEnable) noexcept
{
    if (domeFillEnabled == shouldEnable) return;
    domeFillEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setDomeFillStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (domeFillStrength - amount01) < 1.0e-4f) return;
    domeFillStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setDomeSkyColour (juce::Colour c) noexcept
{
    if (domeSkyColour == c) return;
    domeSkyColour = c;
    markLookDirty();
}

void Spectrogram3DComponent::setDomeGroundColour (juce::Colour c) noexcept
{
    if (domeGroundColour == c) return;
    domeGroundColour = c;
    markLookDirty();
}

void Spectrogram3DComponent::setDomeTextureEnabled (bool shouldEnable) noexcept
{
    if (domeTextureEnabled == shouldEnable) return;
    domeTextureEnabled = shouldEnable;
    if (domeTextureEnabled)
        refreshDomeTextureImage();
    else
        markLookDirty();
}

void Spectrogram3DComponent::setDomeTextureSource (DomeTextureSource source) noexcept
{
    if (domeTextureSource == source) return;
    domeTextureSource = source;
    if (domeTextureEnabled)
        refreshDomeTextureImage();
    else
        markLookDirty();
}

void Spectrogram3DComponent::setDomeTextureCustomPath (const juce::String& absolutePath) noexcept
{
    if (domeTextureCustomPath == absolutePath) return;
    domeTextureCustomPath = absolutePath;
    if (domeTextureEnabled && domeTextureSource == DomeTextureSource::custom)
        refreshDomeTextureImage();
    else
        markLookDirty();
}

void Spectrogram3DComponent::refreshDomeTextureImage()
{
    juce::Image img;

    if (domeTextureSource == DomeTextureSource::veniceSunset)
    {
        img = juce::ImageFileFormat::loadFrom (Spec3DBinaryData::venice_sunset_1k_jpg,
                                               (size_t) Spec3DBinaryData::venice_sunset_1k_jpgSize);
    }
    else if (domeTextureCustomPath.isNotEmpty())
    {
        const juce::File f (domeTextureCustomPath);
        if (f.existsAsFile())
            img = juce::ImageFileFormat::loadFrom (f);
    }

    {
        const juce::ScopedLock lock (domeTextureLock);
        domeTextureImage = img;
        domeTextureDirty.store (true);
    }
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiEnabled (bool shouldEnable) noexcept
{
    if (ssgiEnabled == shouldEnable) return;
    const bool softBefore = usesSoftComposite();
    ssgiEnabled = shouldEnable;
    if (softBefore != usesSoftComposite())
        applyBackgroundTransparency();
    markLookDirty();
}

void Spectrogram3DComponent::setSsrEnabled (bool shouldEnable) noexcept
{
    if (ssrEnabled == shouldEnable) return;
    const bool softBefore = usesSoftComposite();
    ssrEnabled = shouldEnable;
    if (softBefore != usesSoftComposite())
        applyBackgroundTransparency();
    markLookDirty();
}

void Spectrogram3DComponent::setSsrStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrStrength - amount01) < 1.0e-4f) return;
    ssrStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrDistance (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrDistance - amount01) < 1.0e-4f) return;
    ssrDistance = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrThickness (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrThickness - amount01) < 1.0e-4f) return;
    ssrThickness = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrQuality (ShadowQuality q) noexcept
{
    if (ssrQuality == q) return;
    ssrQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrFresnel (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrFresnel - amount01) < 1.0e-4f) return;
    ssrFresnel = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrRoughnessInfluence (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrRoughnessInfluence - amount01) < 1.0e-4f) return;
    ssrRoughnessInfluence = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrIntensity (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 2.0f, amount01);
    if (std::abs (ssrIntensity - amount01) < 1.0e-4f) return;
    ssrIntensity = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrEdgeFade (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrEdgeFade - amount01) < 1.0e-4f) return;
    ssrEdgeFade = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrMetallicBias (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrMetallicBias - amount01) < 1.0e-4f) return;
    ssrMetallicBias = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsrDomeFallback (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssrDomeFallback - amount01) < 1.0e-4f) return;
    ssrDomeFallback = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssgiStrength - amount01) < 1.0e-4f) return;
    ssgiStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiRadius (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssgiRadius - amount01) < 1.0e-4f) return;
    ssgiRadius = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiQuality (ShadowQuality q) noexcept
{
    if (ssgiQuality == q) return;
    ssgiQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiTemporalEnabled (bool shouldEnable) noexcept
{
    if (ssgiTemporalEnabled == shouldEnable) return;
    ssgiTemporalEnabled = shouldEnable;
    if (glHost != nullptr)
        glHost->invalidateSsgiHistory();
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiTemporalAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 0.97f, amount01);
    if (std::abs (ssgiTemporalAmount - amount01) < 1.0e-4f) return;
    ssgiTemporalAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiDenoiseEnabled (bool shouldEnable) noexcept
{
    if (ssgiDenoiseEnabled == shouldEnable) return;
    ssgiDenoiseEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiDenoiseAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (ssgiDenoiseAmount - amount01) < 1.0e-4f) return;
    ssgiDenoiseAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiDenoiseMode (SsgiDenoiseMode mode) noexcept
{
    if (ssgiDenoiseMode == mode) return;
    ssgiDenoiseMode = mode;
    if (glHost != nullptr)
        glHost->invalidateSsgiHistory();
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiAtrousQuality (ShadowQuality q) noexcept
{
    if (ssgiAtrousQuality == q) return;
    ssgiAtrousQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiHalfResEnabled (bool shouldEnable) noexcept
{
    if (ssgiHalfResEnabled == shouldEnable) return;
    ssgiHalfResEnabled = shouldEnable;
    if (glHost != nullptr)
        glHost->invalidateSsgiHistory();
    markLookDirty();
}

void Spectrogram3DComponent::setSsgiMeshNormalsEnabled (bool shouldEnable) noexcept
{
    if (ssgiMeshNormalsEnabled == shouldEnable) return;
    ssgiMeshNormalsEnabled = shouldEnable;
    lastBrightness = -1.0f;
    markLookDirty();
}

void Spectrogram3DComponent::setEnergyConservingEnabled (bool shouldEnable) noexcept
{
    if (energyConservingEnabled == shouldEnable) return;
    energyConservingEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setTonemapEnabled (bool shouldEnable) noexcept
{
    if (tonemapEnabled == shouldEnable) return;
    const bool softBefore = usesSoftComposite();
    tonemapEnabled = shouldEnable;
    if (softBefore != usesSoftComposite())
        applyBackgroundTransparency();
    markLookDirty();
}

void Spectrogram3DComponent::setTonemapExposureStops (float stops) noexcept
{
    stops = juce::jlimit (-4.0f, 4.0f, stops);
    if (std::abs (tonemapExposureStops - stops) < 1.0e-4f) return;
    tonemapExposureStops = stops;
    markLookDirty();
}

void Spectrogram3DComponent::setColorGrade (ColorGrade grade) noexcept
{
    if (colorGrade == grade) return;
    colorGrade = grade;
    markLookDirty();
}

void Spectrogram3DComponent::setContactShadowEnabled (bool shouldEnable) noexcept
{
    if (contactShadowEnabled == shouldEnable) return;
    contactShadowEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setContactShadowStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (contactShadowStrength - amount01) < 1.0e-4f) return;
    contactShadowStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSelfShadowEnabled (bool shouldEnable) noexcept
{
    if (selfShadowEnabled == shouldEnable) return;
    selfShadowEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setSelfShadowStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 2.0f, amount01);
    if (std::abs (selfShadowStrength - amount01) < 1.0e-4f) return;
    selfShadowStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSelfShadowBias (float bias01) noexcept
{
    bias01 = juce::jlimit (0.0f, 1.0f, bias01);
    if (std::abs (selfShadowBias - bias01) < 1.0e-4f) return;
    selfShadowBias = bias01;
    markLookDirty();
}

void Spectrogram3DComponent::setSelfShadowSoftness (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (selfShadowSoftness - amount01) < 1.0e-4f) return;
    selfShadowSoftness = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSelfShadowQuality (ShadowQuality q) noexcept
{
    if (selfShadowQuality == q) return;
    selfShadowQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setCastShadowsEnabled (bool shouldEnable) noexcept
{
    if (castShadowsEnabled == shouldEnable) return;
    castShadowsEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setShadowMapResolution (ShadowMapResolution res) noexcept
{
    if (shadowMapResolution == res) return;
    shadowMapResolution = res;
    markLookDirty();
}

void Spectrogram3DComponent::setShadowCascadeCount (int count) noexcept
{
    count = juce::jlimit (1, kMaxShadowCascades, count);
    if (shadowCascadeCount == count) return;
    shadowCascadeCount = count;
    markLookDirty();
}

void Spectrogram3DComponent::setShadowCascadeDistributionExponent (float exponent) noexcept
{
    exponent = juce::jlimit (1.0f, 4.0f, exponent);
    if (std::abs (shadowCascadeDistributionExponent - exponent) < 1.0e-4f) return;
    shadowCascadeDistributionExponent = exponent;
    markLookDirty();
}

void Spectrogram3DComponent::setShadowCascadeTransitionFraction (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 0.3f, amount01);
    if (std::abs (shadowCascadeTransitionFraction - amount01) < 1.0e-4f) return;
    shadowCascadeTransitionFraction = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSphereEnabled (bool shouldEnable) noexcept
{
    if (debugSphereEnabled == shouldEnable) return;
    debugSphereEnabled = shouldEnable;
    if (glHost != nullptr)
        glHost->markDebugSphereDirty();
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSphereDiameter (float metres) noexcept
{
    metres = juce::jlimit (kDebugSphereMinDiameter, kDebugSphereMaxDiameter, metres);
    if (std::abs (debugSphereDiameter - metres) < 1.0e-5f) return;
    debugSphereDiameter = metres;
    // Keep resting on the floor when only size changes and Y was on the floor.
    if (std::abs (debugSpherePosition.y - (debugSphereDiameter * 0.5f)) < 0.02f
        || debugSpherePosition.y < metres * 0.5f)
        debugSpherePosition.y = metres * 0.5f;
    if (glHost != nullptr)
        glHost->markDebugSphereDirty();
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSpherePosition (juce::Vector3D<float> worldPos) noexcept
{
    worldPos.x = juce::jlimit (-2.5f, 2.5f, worldPos.x);
    worldPos.y = juce::jlimit (0.0f, 3.0f, worldPos.y);
    worldPos.z = juce::jlimit (-2.5f, 2.5f, worldPos.z);
    if (std::abs (debugSpherePosition.x - worldPos.x) < 1.0e-5f
        && std::abs (debugSpherePosition.y - worldPos.y) < 1.0e-5f
        && std::abs (debugSpherePosition.z - worldPos.z) < 1.0e-5f)
        return;
    debugSpherePosition = worldPos;
    if (glHost != nullptr)
        glHost->markDebugSphereDirty();
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSphereAlbedo (juce::Colour c) noexcept
{
    if (debugSphereAlbedo == c) return;
    debugSphereAlbedo = c;
    if (glHost != nullptr)
        glHost->markDebugSphereDirty();
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSphereRoughness (float amount01) noexcept
{
    amount01 = juce::jlimit (0.04f, 1.0f, amount01);
    if (std::abs (debugSphereRoughness - amount01) < 1.0e-4f) return;
    debugSphereRoughness = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSphereMetalness (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (debugSphereMetalness - amount01) < 1.0e-4f) return;
    debugSphereMetalness = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setDebugSphereSpecular (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (debugSphereSpecular - amount01) < 1.0e-4f) return;
    debugSphereSpecular = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsaoEnabled (bool shouldEnable) noexcept
{
    if (ssaoEnabled == shouldEnable) return;
    ssaoEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setSsaoStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 2.0f, amount01);
    if (std::abs (ssaoStrength - amount01) < 1.0e-4f) return;
    ssaoStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSsaoRadius (float radius) noexcept
{
    radius = juce::jlimit (0.25f, 3.0f, radius);
    if (std::abs (ssaoRadius - radius) < 1.0e-4f) return;
    ssaoRadius = radius;
    markLookDirty();
}

void Spectrogram3DComponent::setBloomEnabled (bool shouldEnable) noexcept
{
    if (bloomEnabled == shouldEnable) return;
    const bool softBefore = usesSoftComposite();
    bloomEnabled = shouldEnable;
    if (softBefore != usesSoftComposite())
        applyBackgroundTransparency();
    markLookDirty();
}

void Spectrogram3DComponent::setBloomStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (bloomStrength - amount01) < 1.0e-4f) return;
    bloomStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setBloomThreshold (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (bloomThreshold - amount01) < 1.0e-4f) return;
    bloomThreshold = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setMotionBlurEnabled (bool shouldEnable) noexcept
{
    if (motionBlurEnabled == shouldEnable) return;
    const bool softBefore = usesSoftComposite();
    motionBlurEnabled = shouldEnable;
    if (softBefore != usesSoftComposite())
        applyBackgroundTransparency();
    // Force history rebuild so first blurred frame is not garbage.
    if (glHost != nullptr)
        glHost->invalidateMotionHistory();
    markLookDirty();
}

void Spectrogram3DComponent::setMotionBlurAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (motionBlurAmount - amount01) < 1.0e-4f) return;
    motionBlurAmount = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setMotionBlurMax (float maxPixels) noexcept
{
    maxPixels = juce::jlimit (kMotionBlurMaxMin, kMotionBlurMaxMax, maxPixels);
    if (std::abs (motionBlurMax - maxPixels) < 1.0e-3f) return;
    motionBlurMax = maxPixels;
    markLookDirty();
}

void Spectrogram3DComponent::setMotionBlurQuality (ShadowQuality q) noexcept
{
    if (motionBlurQuality == q) return;
    motionBlurQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setDofEnabled (bool shouldEnable) noexcept
{
    if (dofEnabled == shouldEnable) return;
    const bool softBefore = usesSoftComposite();
    dofEnabled = shouldEnable;
    if (softBefore != usesSoftComposite())
        applyBackgroundTransparency();
    markLookDirty();
}

void Spectrogram3DComponent::setDofFocusDistance (float distance, bool notifyPrefsCallback) noexcept
{
    distance = juce::jlimit (kDofFocusMin, kDofFocusMax, distance);
    if (std::abs (dofFocusDistance - distance) < 1.0e-4f) return;
    dofFocusDistance = distance;
    markLookDirty();
    if (notifyPrefsCallback && onDofFocusChanged != nullptr)
        onDofFocusChanged();
}

float Spectrogram3DComponent::getDofLensPower() const noexcept
{
    const float fNorm = dofFocalLengthMm / 35.0f;
    const float nNorm = juce::jmax (0.05f, dofFStop / 5.6f);
    return (fNorm * fNorm) / nNorm;
}

void Spectrogram3DComponent::setDofFStop (float fStop) noexcept
{
    fStop = juce::jlimit (kDofFStopMin, kDofFStopMax, fStop);
    if (std::abs (dofFStop - fStop) < 1.0e-4f) return;
    dofFStop = fStop;
    markLookDirty();
}

void Spectrogram3DComponent::setDofFocalLengthMm (float mm) noexcept
{
    mm = juce::jlimit (kDofFocalLengthMinMm, kDofFocalLengthMaxMm, mm);
    if (std::abs (dofFocalLengthMm - mm) < 1.0e-3f) return;
    dofFocalLengthMm = mm;
    markLookDirty();
}

void Spectrogram3DComponent::setDofAperture (float amount) noexcept
{
    // Legacy 0-3 openness -> F-Stop (higher openness = lower f-number).
    amount = juce::jlimit (0.0f, kDofApertureMax, amount);
    const float t = amount / kDofApertureMax;
    setDofFStop (juce::jmap (t, kDofFStopMax, kDofFStopMin));
}

float Spectrogram3DComponent::getDofAperture() const noexcept
{
    const float t = juce::jmap (dofFStop, kDofFStopMin, kDofFStopMax, 1.0f, 0.0f);
    return t * kDofApertureMax;
}

void Spectrogram3DComponent::setDofQuality (ShadowQuality q) noexcept
{
    if (dofQuality == q) return;
    dofQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setDofBlurScale (float) noexcept
{
    // Removed - F-Stop + Focal Length drive CoC. Kept as no-op for prefs compat.
}

void Spectrogram3DComponent::setDofCocDilate (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (dofCocDilate - amount01) < 1.0e-4f) return;
    dofCocDilate = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setDofEdgeSpill (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (dofEdgeSpill - amount01) < 1.0e-4f) return;
    dofEdgeSpill = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setClosedMeshEnabled (bool shouldEnable) noexcept
{
    if (closedMeshEnabled == shouldEnable) return;
    closedMeshEnabled = shouldEnable;
    indicesValid = false;
    if (meshW >= 2 && meshH >= 2 && ! meshDb.empty() && lastBrightness >= 0.0f)
        rebuildVerticesFromMeshDb (lastBrightness, lastMinDb, lastMaxDb);
    markLookDirty();
}

void Spectrogram3DComponent::setSssEnabled (bool shouldEnable) noexcept
{
    if (sssEnabled == shouldEnable) return;
    sssEnabled = shouldEnable;
    markLookDirty();
}

void Spectrogram3DComponent::setSssStrength (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssStrength - amount01) < 1.0e-4f) return;
    sssStrength = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSssWrap (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssWrap - amount01) < 1.0e-4f) return;
    sssWrap = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSssTransmission (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssTransmission - amount01) < 1.0e-4f) return;
    sssTransmission = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSssTint (juce::Colour c) noexcept
{
    if (sssTint == c) return;
    sssTint = c;
    markLookDirty();
}

void Spectrogram3DComponent::setSssRadius (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssRadius - amount01) < 1.0e-4f) return;
    sssRadius = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSssContrast (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssContrast - amount01) < 1.0e-4f) return;
    sssContrast = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSssQuality (ShadowQuality q) noexcept
{
    if (sssQuality == q) return;
    sssQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setSssThicknessScale (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssThicknessScale - amount01) < 1.0e-4f) return;
    sssThicknessScale = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setSssMaxThickness (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (sssMaxThickness - amount01) < 1.0e-4f) return;
    sssMaxThickness = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::applyBackgroundTransparency() noexcept
{
    setOpaque (! usesSoftComposite());
    if (glHost != nullptr)
        glHost->applyBackgroundTransparency();
    layoutPresentation();
}

bool Spectrogram3DComponent::usesSoftComposite() const noexcept
{
    // Nested GL HWNDs often stay black under Direct2D. Soft FBO->Image is reliable for
    // Soft BG floating overlays, docked Scope panes, and SSAO/bloom post passes.
    return transparentBackground || chromeMode == ChromeMode::docked || needsPostEffects();
}

void Spectrogram3DComponent::markLookDirty() noexcept
{
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::GlHost::applyBackgroundTransparency() noexcept
{
    // Soft: non-opaque peer so the parked context HWND doesn't punch a black square.
    // Hard: opaque nested HWND for direct display.
    const bool soft = owner.usesSoftComposite();
    setOpaque (! soft);
    setInterceptsMouseClicks (! soft, true);
    openGLContext.setComponentPaintingEnabled (false);
}

void Spectrogram3DComponent::markSoftContentDirty() noexcept
{
    if (glHost != nullptr)
        glHost->markSoftContentDirty();
}

void Spectrogram3DComponent::layoutPresentation() noexcept
{
    const auto glArea = getGlViewLocal();
    const bool soft = usesSoftComposite();

    if (hitLayer != nullptr)
    {
        hitLayer->setBounds (glArea);
        hitLayer->setVisible (active && soft);
        if (hitLayer->isVisible())
            hitLayer->toFront (false);
    }

    if (glHost == nullptr)
        return;

    if (soft)
    {
        // Small peer keeps the GL context alive for FBO work (not used for display).
        // Park far outside this component so the native HWND is clipped by the
        // plugin window - placing it in the shadow pad (or bottom-right) reads as
        // a black square over the chrome / resize grip under Direct2D.
        constexpr int kPeer = 4;
        constexpr int kPark = -20000;
        glHost->setOpaque (false);
        glHost->setBounds (kPark, kPark, kPeer, kPeer);
        glHost->setVisible (active);
        glHost->setInterceptsMouseClicks (false, false);
        glHost->toBack();
    }
    else
    {
        glHost->setBounds (glArea);
        glHost->setVisible (active);
        glHost->setInterceptsMouseClicks (true, true);
    }

    if (active)
        glHost->requestAttachAsync();

    if (zoomHandle != nullptr)
    {
        constexpr int kSize = 28;
        constexpr int kTopOffset = 50;
        constexpr int kMargin = 8;
        const auto gl = getGlViewLocal();
        zoomHandle->setBounds (gl.getRight() - kMargin - kSize,
                               gl.getY() + kTopOffset,
                               kSize, kSize);
        zoomHandle->setVisible (active);
        if (zoomHandle->isVisible())
            zoomHandle->toFront (false);
    }

    if (resizer != nullptr && resizer->isVisible())
        resizer->toFront (false);
}

void Spectrogram3DComponent::invalidateMesh() noexcept
{
    meshDb.clear();
    meshW = 0;
    meshH = 0;
    lastHistorySerial = 0;
    indicesValid = false;
    lastBrightness = -1.0f;
}

void Spectrogram3DComponent::recolourMesh() noexcept
{
    if (meshW < 2 || meshH < 2 || meshDb.empty() || lastBrightness < 0.0f)
        return;
    rebuildVerticesFromMeshDb (lastBrightness, lastMinDb, lastMaxDb);
    if (glHost != nullptr)
        glHost->triggerRedraw();
}

void Spectrogram3DComponent::recolourVertexColoursOnly() noexcept
{
    if (dataSource == nullptr || meshW < 2 || meshH < 2 || meshDb.empty() || lastBrightness < 0.0f)
        return;

    dataSource->refreshColourLutFor3D();

    const float brightness = lastBrightness;
    const float minDb = lastMinDb;
    const float maxDb = lastMaxDb;
    const float denom = juce::jmax (1.0f, maxDb - minDb);
    juce::ignoreUnused (denom);
    const bool closed = closedMeshEnabled;
    const int topCount = meshW * meshH;

    const juce::ScopedLock sl (meshLock);
    if ((int) cpuVertices.size() < topCount)
        return;

    for (int z = 0; z < meshH; ++z)
    {
        for (int x = 0; x < meshW; ++x)
        {
            const float db = meshDb[(size_t) x * (size_t) meshH + (size_t) z];
            const auto c = dataSource->colourFromHistoryDb3D (db, brightness, minDb, maxDb);
            auto& vtx = cpuVertices[(size_t) z * (size_t) meshW + (size_t) x];
            vtx.r = c.getFloatRed();
            vtx.g = c.getFloatGreen();
            vtx.b = c.getFloatBlue();

            if (closed && (int) cpuVertices.size() >= topCount * 2)
            {
                auto& bot = cpuVertices[(size_t) topCount + (size_t) z * (size_t) meshW + (size_t) x];
                bot.r = vtx.r * 0.55f;
                bot.g = vtx.g * 0.55f;
                bot.b = vtx.b * 0.55f;
            }
        }
    }

    if (closed && (int) cpuVertices.size() >= topCount * 2 + meshH * 4)
    {
        const int phTop = topCount * 2;
        const int phBot = phTop + meshH;
        const int endTop = phBot + meshH;
        const int endBot = endTop + meshH;
        for (int z = 0; z < meshH; ++z)
        {
            const auto& srcPh = cpuVertices[(size_t) z * (size_t) meshW + (size_t) (meshW - 1)];
            auto& phT = cpuVertices[(size_t) (phTop + z)];
            phT.r = srcPh.r; phT.g = srcPh.g; phT.b = srcPh.b;
            auto& phB = cpuVertices[(size_t) (phBot + z)];
            phB.r = srcPh.r * 0.55f; phB.g = srcPh.g * 0.55f; phB.b = srcPh.b * 0.55f;

            const auto& srcEnd = cpuVertices[(size_t) z * (size_t) meshW];
            auto& eT = cpuVertices[(size_t) (endTop + z)];
            eT.r = srcEnd.r; eT.g = srcEnd.g; eT.b = srcEnd.b;
            auto& eB = cpuVertices[(size_t) (endBot + z)];
            eB.r = srcEnd.r * 0.55f; eB.g = srcEnd.g * 0.55f; eB.b = srcEnd.b * 0.55f;
        }
    }

    meshNeedsUpload = true;
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
}

void Spectrogram3DComponent::resetCamera() noexcept
{
    seedDefaultOrientation();
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::saveAsDefaultView() noexcept
{
    if (zoomOscillateEnabled)
        camera.distance = zoomOscillateBaseDistance;
    clampCamera();
    defaultCamera = camera;
    if (onDefaultViewChanged != nullptr)
        onDefaultViewChanged();
}

void Spectrogram3DComponent::setAutoRotateEnabled (bool shouldEnable, bool notifyPrefsCallback) noexcept
{
    if (autoRotateEnabled == shouldEnable)
        return;
    autoRotateEnabled = shouldEnable;
    autoRotateLastTimeSec = 0.0; // avoid a dt jump on enable
    if (notifyPrefsCallback && onAutoRotateSettingsChanged != nullptr)
        onAutoRotateSettingsChanged();
    if (autoRotateEnabled)
    {
        markSoftContentDirty();
        if (glHost != nullptr)
            glHost->triggerRedraw();
        if (usesSoftComposite())
            repaint();
    }
}

void Spectrogram3DComponent::setAutoRotatePeriodSec (float secondsPerRevolution,
                                                     bool notifyPrefsCallback) noexcept
{
    secondsPerRevolution = juce::jlimit (kAutoRotatePeriodMinSec, kAutoRotatePeriodMaxSec,
                                         secondsPerRevolution);
    if (std::abs (autoRotatePeriodSec - secondsPerRevolution) < 1.0e-4f)
        return;
    autoRotatePeriodSec = secondsPerRevolution;
    if (notifyPrefsCallback && onAutoRotateSettingsChanged != nullptr)
        onAutoRotateSettingsChanged();
}

void Spectrogram3DComponent::applyZoomOscillateDistance() noexcept
{
    const float depth = juce::jlimit (0.0f, 0.85f, zoomOscillateDepth);
    const float s = std::sin (zoomOscillatePhaseRad);
    camera.distance = zoomOscillateBaseDistance * (1.0f + depth * s);
    clampCamera();
}

void Spectrogram3DComponent::captureZoomOscillateBaseFromCamera() noexcept
{
    const float depth = juce::jlimit (0.0f, 0.85f, zoomOscillateDepth);
    const float s = std::sin (zoomOscillatePhaseRad);
    const float denom = 1.0f + depth * s;
    zoomOscillateBaseDistance = (std::abs (denom) > 0.15f)
                                    ? camera.distance / denom
                                    : camera.distance;
    zoomOscillateBaseDistance = juce::jlimit (0.35f, 14.0f, zoomOscillateBaseDistance);
}

void Spectrogram3DComponent::setZoomOscillateEnabled (bool shouldEnable,
                                                     bool notifyPrefsCallback) noexcept
{
    if (zoomOscillateEnabled == shouldEnable)
        return;
    if (shouldEnable)
    {
        zoomOscillateBaseDistance = juce::jlimit (0.35f, 14.0f, camera.distance);
        zoomOscillatePhaseRad = 0.0f;
        zoomOscillateLastTimeSec = 0.0;
    }
    else
    {
        camera.distance = zoomOscillateBaseDistance;
        clampCamera();
    }
    zoomOscillateEnabled = shouldEnable;
    if (notifyPrefsCallback && onAutoRotateSettingsChanged != nullptr)
        onAutoRotateSettingsChanged();
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::setZoomOscillateDepth (float amount01,
                                                   bool notifyPrefsCallback) noexcept
{
    amount01 = juce::jlimit (0.0f, 0.85f, amount01);
    if (std::abs (zoomOscillateDepth - amount01) < 1.0e-4f)
        return;
    if (zoomOscillateEnabled)
        captureZoomOscillateBaseFromCamera();
    zoomOscillateDepth = amount01;
    if (zoomOscillateEnabled)
        applyZoomOscillateDistance();
    if (notifyPrefsCallback && onAutoRotateSettingsChanged != nullptr)
        onAutoRotateSettingsChanged();
    markSoftContentDirty();
}

void Spectrogram3DComponent::setZoomOscillatePeriodSec (float secondsPerCycle,
                                                       bool notifyPrefsCallback) noexcept
{
    secondsPerCycle = juce::jlimit (kZoomOscillatePeriodMinSec, kZoomOscillatePeriodMaxSec,
                                    secondsPerCycle);
    if (std::abs (zoomOscillatePeriodSec - secondsPerCycle) < 1.0e-4f)
        return;
    zoomOscillatePeriodSec = secondsPerCycle;
    if (notifyPrefsCallback && onAutoRotateSettingsChanged != nullptr)
        onAutoRotateSettingsChanged();
}

void Spectrogram3DComponent::setAudioLevelModEnabled (bool shouldEnable) noexcept
{
    if (audioLevelModEnabled == shouldEnable)
        return;
    audioLevelModEnabled = shouldEnable;
    if (! shouldEnable)
        audioLevelLive01 = 0.0f;
    markLookDirty(); // force soft-FBO redraw - target/factor changes must not stick
}

void Spectrogram3DComponent::setAudioLevelTarget (AudioLevelTarget target) noexcept
{
    const int idx = juce::jlimit (0, (int) AudioLevelTarget::brightnessAndLights, (int) target);
    target = static_cast<AudioLevelTarget> (idx);
    if (audioLevelTarget == target)
        return;
    audioLevelTarget = target;
    markLookDirty();
}

void Spectrogram3DComponent::setAudioLevelMinPercent (float pct) noexcept
{
    // Independent of max - both may span the full range (allows invert / fine-tune).
    pct = juce::jlimit (kAudioLevelPercentMin, kAudioLevelPercentMax, pct);
    if (std::abs (audioLevelMinPercent - pct) < 1.0e-3f)
        return;
    audioLevelMinPercent = pct;
    markLookDirty();
}

void Spectrogram3DComponent::setAudioLevelMaxPercent (float pct) noexcept
{
    pct = juce::jlimit (kAudioLevelPercentMin, kAudioLevelPercentMax, pct);
    if (std::abs (audioLevelMaxPercent - pct) < 1.0e-3f)
        return;
    audioLevelMaxPercent = pct;
    markLookDirty();
}

void Spectrogram3DComponent::setAudioLevelHpHz (float hz) noexcept
{
    audioLevelHpHz = juce::jlimit (20.0f, 18000.0f, hz);
}

void Spectrogram3DComponent::setAudioLevelLpHz (float hz) noexcept
{
    audioLevelLpHz = juce::jlimit (40.0f, 20000.0f, hz);
}

void Spectrogram3DComponent::setAudioLevelThresholdDb (float thresholdDb) noexcept
{
    audioLevelThresholdDb = juce::jlimit (kAudioLevelThresholdMinDb, kAudioLevelThresholdMaxDb,
                                          thresholdDb);
}

void Spectrogram3DComponent::setAudioLevelSpeed (AudioLevelSpeed speed) noexcept
{
    audioLevelSpeed = speed;
}

void Spectrogram3DComponent::audioLevelBallisticsMs (AudioLevelSpeed speed,
                                                     float& attackMs, float& releaseMs) noexcept
{
    // Match Side Check Fast / Med / Slow (no exposed A/R knobs).
    switch (speed)
    {
        case AudioLevelSpeed::slow: attackMs = 120.0f; releaseMs = 900.0f; break;
        case AudioLevelSpeed::med:  attackMs = 40.0f;  releaseMs = 300.0f; break;
        case AudioLevelSpeed::fast:
        default:                    attackMs = 8.0f;   releaseMs = 80.0f;  break;
    }
}

void Spectrogram3DComponent::setAudioLevelAffectPlayhead (bool shouldAffect) noexcept
{
    if (audioLevelAffectPlayhead == shouldAffect)
        return;
    audioLevelAffectPlayhead = shouldAffect;
    markLookDirty();
}

void Spectrogram3DComponent::setAudioLevelAffectAntiPlayhead (bool shouldAffect) noexcept
{
    if (audioLevelAffectAntiPlayhead == shouldAffect)
        return;
    audioLevelAffectAntiPlayhead = shouldAffect;
    markLookDirty();
}

void Spectrogram3DComponent::setAudioLevelProvider (std::function<float()> provider) noexcept
{
    audioLevelProvider = std::move (provider);
}

void Spectrogram3DComponent::setNormalCuspAngleDeg (float deg) noexcept
{
    deg = juce::jlimit (kNormalCuspMinDeg, kNormalCuspMaxDeg, deg);
    if (std::abs (normalCuspAngleDeg - deg) < 1.0e-3f)
        return;
    normalCuspAngleDeg = deg;
    // Normals are rebuilt with the mesh; force a full vertex rebuild.
    lastBrightness = -1.0f;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setNormalWeighting (NormalWeighting method) noexcept
{
    if (normalWeighting == method)
        return;
    normalWeighting = method;
    lastBrightness = -1.0f;
    markSoftContentDirty();
}

void Spectrogram3DComponent::computeTopSurfaceNormals (std::vector<Vertex>& verts, int w, int h)
{
    if (w < 2 || h < 2 || (int) verts.size() < w * h)
        return;

    const int nVerts = w * h;
    const size_t n = (size_t) nVerts;
    normalAccumX.assign (n, 0.0f);
    normalAccumY.assign (n, 0.0f);
    normalAccumZ.assign (n, 0.0f);
    if (normalCuspAngleDeg < 179.5f)
    {
        normalBestW.assign (n, -1.0f);
        normalBestNx.assign (n, 0.0f);
        normalBestNy.assign (n, 1.0f);
        normalBestNz.assign (n, 0.0f);
    }

    const bool useCusp = normalCuspAngleDeg < 179.5f;
    const float cuspCos = useCusp
        ? std::cos (juce::degreesToRadians (
              juce::jlimit (kNormalCuspMinDeg, kNormalCuspMaxDeg, normalCuspAngleDeg)))
        : -1.0f;
    const bool needAngle = normalWeighting == NormalWeighting::vertexAngle
                        || normalWeighting == NormalWeighting::angleAndArea;
    const bool needArea = normalWeighting == NormalWeighting::faceArea
                       || normalWeighting == NormalWeighting::angleAndArea;

    auto cornerAngle = [] (const Vertex& v, const Vertex& a, const Vertex& b) -> float
    {
        float axv = a.x - v.x, ayv = a.y - v.y, azv = a.z - v.z;
        float bxv = b.x - v.x, byv = b.y - v.y, bzv = b.z - v.z;
        const float al = std::sqrt (axv * axv + ayv * ayv + azv * azv);
        const float bl = std::sqrt (bxv * bxv + byv * byv + bzv * bzv);
        if (al < 1.0e-8f || bl < 1.0e-8f)
            return 0.0f;
        axv /= al; ayv /= al; azv /= al;
        bxv /= bl; byv /= bl; bzv /= bl;
        // Cheap angle proxy (avoids acos in the hot path). Larger corners -> larger weight.
        const float d = juce::jlimit (-1.0f, 1.0f, axv * bxv + ayv * byv + azv * bzv);
        return 1.0f - d;
    };

    auto visitTri = [&] (int i0, int i1, int i2, bool accumulate)
    {
        const auto& a = verts[(size_t) i0];
        const auto& b = verts[(size_t) i1];
        const auto& c = verts[(size_t) i2];
        const float e1x = b.x - a.x, e1y = b.y - a.y, e1z = b.z - a.z;
        const float e2x = c.x - a.x, e2y = c.y - a.y, e2z = c.z - a.z;
        float fnx = e1y * e2z - e1z * e2y;
        float fny = e1z * e2x - e1x * e2z;
        float fnz = e1x * e2y - e1y * e2x;
        const float area2 = std::sqrt (fnx * fnx + fny * fny + fnz * fnz);
        if (area2 < 1.0e-12f)
            return;
        const float inv = 1.0f / area2;
        fnx *= inv; fny *= inv; fnz *= inv;
        const float area = needArea ? (0.5f * area2) : 0.0f;

        const int idx[3] = { i0, i1, i2 };
        const Vertex* pv[3] = { &a, &b, &c };
        for (int k = 0; k < 3; ++k)
        {
            const size_t vi = (size_t) idx[k];
            float weight = 1.0f;
            if (needAngle || needArea)
            {
                const float ang = needAngle
                    ? cornerAngle (*pv[k], *pv[(k + 2) % 3], *pv[(k + 1) % 3])
                    : 0.0f;
                switch (normalWeighting)
                {
                    case NormalWeighting::equal:        weight = 1.0f; break;
                    case NormalWeighting::vertexAngle:  weight = ang; break;
                    case NormalWeighting::faceArea:     weight = area; break;
                    case NormalWeighting::angleAndArea: weight = ang * area; break;
                }
            }
            if (weight < 1.0e-12f)
                continue;

            if (! accumulate)
            {
                if (weight > normalBestW[vi])
                {
                    normalBestW[vi] = weight;
                    normalBestNx[vi] = fnx;
                    normalBestNy[vi] = fny;
                    normalBestNz[vi] = fnz;
                }
                continue;
            }

            if (useCusp)
            {
                const float d = fnx * normalBestNx[vi]
                              + fny * normalBestNy[vi]
                              + fnz * normalBestNz[vi];
                if (d < cuspCos)
                    continue;
            }

            normalAccumX[vi] += fnx * weight;
            normalAccumY[vi] += fny * weight;
            normalAccumZ[vi] += fnz * weight;
        }
    };

    // Freq reverse mirrors world Z; keep winding matched so normals stay upward.
    const bool flipWinding = reverseFrequencyAxis;

    auto forEachTopTri = [&] (bool accumulate)
    {
        for (int z = 0; z < h - 1; ++z)
        {
            for (int x = 0; x < w - 1; ++x)
            {
                const int i0 = z * w + x;
                const int i1 = i0 + 1;
                const int i2 = i0 + w;
                const int i3 = i2 + 1;
                if (! flipWinding)
                {
                    visitTri (i0, i2, i1, accumulate);
                    visitTri (i1, i2, i3, accumulate);
                }
                else
                {
                    visitTri (i0, i1, i2, accumulate);
                    visitTri (i1, i3, i2, accumulate);
                }
            }
        }
    };

    if (useCusp)
        forEachTopTri (false);
    forEachTopTri (true);

    for (int i = 0; i < nVerts; ++i)
    {
        auto& vtx = verts[(size_t) i];
        const float len = std::sqrt (normalAccumX[(size_t) i] * normalAccumX[(size_t) i]
                                   + normalAccumY[(size_t) i] * normalAccumY[(size_t) i]
                                   + normalAccumZ[(size_t) i] * normalAccumZ[(size_t) i]);
        if (len < 1.0e-8f)
        {
            if (useCusp)
            {
                vtx.nx = normalBestNx[(size_t) i];
                vtx.ny = normalBestNy[(size_t) i];
                vtx.nz = normalBestNz[(size_t) i];
            }
            else
            {
                vtx.nx = 0.0f;
                vtx.ny = 1.0f;
                vtx.nz = 0.0f;
            }
        }
        else
        {
            vtx.nx = normalAccumX[(size_t) i] / len;
            vtx.ny = normalAccumY[(size_t) i] / len;
            vtx.nz = normalAccumZ[(size_t) i] / len;
        }

        // Safety: top surface must face +Y (Z-mirror must not photonegative the ramp).
        if (vtx.ny < 0.0f)
        {
            vtx.nx = -vtx.nx;
            vtx.ny = -vtx.ny;
            vtx.nz = -vtx.nz;
        }
    }
}

void Spectrogram3DComponent::setDefaultCameraState (const CameraState& state) noexcept
{
    defaultCamera = state;
    camera = defaultCamera;
    clampCamera();
    defaultCamera = camera;
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

void Spectrogram3DComponent::seedDefaultOrientation() noexcept
{
    camera = defaultCamera;
    clampCamera();
    zoomOscillateBaseDistance = camera.distance;
    zoomOscillatePhaseRad = 0.0f;
    if (zoomOscillateEnabled)
        applyZoomOscillateDistance();
}

void Spectrogram3DComponent::clampCamera() noexcept
{
    // Turntable / orbit framing ONLY. Freecam never uses this while active.
    // No ground-plane clamp - freecam bake and free look keep full pitch / panY.
    camera.pitchDeg = juce::jlimit (kMinPitchDeg, kMaxPitchDeg, camera.pitchDeg);
    camera.distance = juce::jlimit (0.35f, 14.0f, camera.distance);
    camera.panX = juce::jlimit (-1.6f, 1.6f, camera.panX);
    camera.panZ = juce::jlimit (-1.6f, 1.6f, camera.panZ);
    camera.panY = juce::jlimit (-80.0f, 80.0f, camera.panY);
}

float Spectrogram3DComponent::freecamLevelToScale (float level1to8) noexcept
{
    // UE viewport chip is 1 (slowest) ... 8 (full). Log-spaced so low levels are crawl-speed.
    const float t = juce::jlimit (0.0f, 1.0f, (level1to8 - 1.0f) / 7.0f);
    const float logMin = std::log (kFreecamSpeedMin);
    const float logMax = std::log (kFreecamSpeedMax);
    return std::exp (logMin + (logMax - logMin) * t);
}

float Spectrogram3DComponent::freecamScaleToLevel (float scale) noexcept
{
    scale = juce::jlimit (kFreecamSpeedMin, kFreecamSpeedMax, scale);
    const float logMin = std::log (kFreecamSpeedMin);
    const float logMax = std::log (kFreecamSpeedMax);
    const float t = (std::log (scale) - logMin) / juce::jmax (1.0e-6f, logMax - logMin);
    return 1.0f + 7.0f * juce::jlimit (0.0f, 1.0f, t);
}

void Spectrogram3DComponent::setFreecamSpeedScale (float scale01to1) noexcept
{
    scale01to1 = juce::jlimit (kFreecamSpeedMin, kFreecamSpeedMax, scale01to1);
    if (std::abs (freecamSpeedScale - scale01to1) < 1.0e-6f)
        return;
    freecamSpeedScale = scale01to1;
    notifyFreecamSpeedChanged();
}

void Spectrogram3DComponent::setFreecamSpeedLevel (float level1to8) noexcept
{
    setFreecamSpeedScale (freecamLevelToScale (level1to8));
}

float Spectrogram3DComponent::getFreecamSpeedLevel() const noexcept
{
    return freecamScaleToLevel (freecamSpeedScale);
}

void Spectrogram3DComponent::setFreecamLookSensitivity (float degPerPixel) noexcept
{
    degPerPixel = juce::jlimit (0.02f, 2.0f, degPerPixel);
    if (std::abs (freecamLookSensitivity - degPerPixel) < 1.0e-6f)
        return;
    freecamLookSensitivity = degPerPixel;
    notifyFreecamSpeedChanged();
}

void Spectrogram3DComponent::setFreecamInvertY (bool shouldInvert) noexcept
{
    if (freecamInvertY == shouldInvert)
        return;
    freecamInvertY = shouldInvert;
    notifyFreecamSpeedChanged();
}

void Spectrogram3DComponent::notifyFreecamSpeedChanged() noexcept
{
    if (onFreecamSpeedChanged != nullptr)
        onFreecamSpeedChanged();
    if (freecamSpeedHandle != nullptr)
        freecamSpeedHandle->repaint();
}

void Spectrogram3DComponent::nudgeFreecamSpeedFromWheel (float wheelDeltaY) noexcept
{
    // UE: scroll up = faster, down = slower (while freecam).
    if (std::abs (wheelDeltaY) < 1.0e-8f)
        return;
    const float step = std::exp (wheelDeltaY * 0.85f);
    setFreecamSpeedScale (freecamSpeedScale * step);
}

void Spectrogram3DComponent::freecamBasis (juce::Vector3D<float>& outRight,
                                           juce::Vector3D<float>& outUp,
                                           juce::Vector3D<float>& outForward) const noexcept
{
    // Independent freecam (not orbit): FPS pitch 0 = horizon, + = look up.
    // yaw=0,pitch=0 -> look −Z. Used for WASD and look-at target only.
    const float yaw = juce::degreesToRadians (freecamYawDeg);
    const float pitch = juce::degreesToRadians (freecamPitchDeg);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    const float cy = std::cos (yaw);
    const float sy = std::sin (yaw);

    outForward = { sy * cp, sp, -cy * cp };
    // right = normalize (forward x worldUp) fails at poles; use horizontal right.
    outRight = { cy, 0.0f, sy };
    const float rLen = juce::jmax (1.0e-6f,
                                   std::sqrt (outRight.x * outRight.x + outRight.z * outRight.z));
    outRight.x /= rLen;
    outRight.z /= rLen;
    outUp = {
        outRight.y * outForward.z - outRight.z * outForward.y,
        outRight.z * outForward.x - outRight.x * outForward.z,
        outRight.x * outForward.y - outRight.y * outForward.x
    };
    const float uLen = juce::jmax (1.0e-6f,
                                   std::sqrt (outUp.x * outUp.x + outUp.y * outUp.y + outUp.z * outUp.z));
    outUp.x /= uLen;
    outUp.y /= uLen;
    outUp.z /= uLen;
}

void Spectrogram3DComponent::cameraBasis (juce::Vector3D<float>& outRight,
                                          juce::Vector3D<float>& outUp,
                                          juce::Vector3D<float>& outForward) const noexcept
{
    if (freecamActive)
    {
        freecamBasis (outRight, outUp, outForward);
        return;
    }

    // Turntable orbit basis (elevation pitch).
    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    const float cy = std::cos (yaw);
    const float sy = std::sin (yaw);

    outForward = { sy * cp, -sp, -cy * cp };
    outRight = { -outForward.z, 0.0f, outForward.x };
    const float rLen = juce::jmax (1.0e-6f, std::sqrt (outRight.x * outRight.x + outRight.z * outRight.z));
    outRight.x /= rLen;
    outRight.z /= rLen;
    outUp = {
        outRight.y * outForward.z - outRight.z * outForward.y,
        outRight.z * outForward.x - outRight.x * outForward.z,
        outRight.x * outForward.y - outRight.y * outForward.x
    };
    const float uLen = juce::jmax (1.0e-6f,
                                   std::sqrt (outUp.x * outUp.x + outUp.y * outUp.y + outUp.z * outUp.z));
    outUp.x /= uLen;
    outUp.y /= uLen;
    outUp.z /= uLen;
}

juce::Vector3D<float> Spectrogram3DComponent::getCameraEyePosition() const noexcept
{
    if (freecamActive)
        return freecamEye;

    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    const float cy = std::cos (yaw);
    const float sy = std::sin (yaw);
    return {
        camera.panX + (-sy * cp) * camera.distance,
        camera.panY + sp * camera.distance,
        camera.panZ + (cy * cp) * camera.distance
    };
}

void Spectrogram3DComponent::enterFreecamFromTurntable() noexcept
{
    // Snapshot orbit eye + convert elevation -> FPS look pitch. Orbit state left untouched.
    freecamEye = getCameraEyePosition(); // turntable-derived (freecam not active yet)
    freecamYawDeg = camera.yawDeg;
    // Turntable elev+ looks down; FPS pitch+ looks up -> invert.
    freecamPitchDeg = juce::jlimit (-kMaxPitchDeg, kMaxPitchDeg, -camera.pitchDeg);
    freecamActive = true;
    flyVel = { 0, 0, 0 };
    syncSpec3DTimerRate();
}

void Spectrogram3DComponent::exitFreecamToTurntable() noexcept
{
    if (! freecamActive)
        return;

    // Bake freecam pose into orbit rig so LMB orbit continues from the new view.
    // Preserve freecamEye exactly - no ground-plane panY/distance rewrite on RMB release.
    camera.yawDeg = freecamYawDeg;
    // FPS pitch+ = look up -> orbit elevation is inverted.
    camera.pitchDeg = juce::jlimit (kMinPitchDeg, kMaxPitchDeg, -freecamPitchDeg);

    float d = juce::jlimit (0.35f, 14.0f, camera.distance);
    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    const float cy = std::cos (yaw);
    const float sy = std::sin (yaw);

    // eye = pan + (-sy*cp, sp, cy*cp)*d  ->  pan = eye - offset
    camera.panX = freecamEye.x - (-sy * cp) * d;
    camera.panY = freecamEye.y - sp * d;
    camera.panZ = freecamEye.z - (cy * cp) * d;

    camera.distance = d;
    camera.panX = juce::jlimit (-1.6f, 1.6f, camera.panX);
    camera.panY = juce::jlimit (-80.0f, 80.0f, camera.panY);
    camera.panZ = juce::jlimit (-1.6f, 1.6f, camera.panZ);
    camera.pitchDeg = juce::jlimit (kMinPitchDeg, kMaxPitchDeg, camera.pitchDeg);
    camera.distance = juce::jlimit (0.35f, 14.0f, camera.distance);

    freecamActive = false;
    flyVel = { 0, 0, 0 };
    syncSpec3DTimerRate();
}

void Spectrogram3DComponent::applyFreecamLookDelta (float dxPixels, float dyPixels) noexcept
{
    if (! freecamActive)
        enterFreecamFromTurntable();
    if (! freecamActive)
        return;

    // UE freecam mouse-look: rotate in place about freecamEye only.
    // Never touch orbit camera.distance / pan - RMB must not dolly or zoom.
    // Mouse right -> turn right; mouse up -> look up (unless invert Y).
    const float sens = freecamLookSensitivity;
    freecamYawDeg += dxPixels * sens;
    while (freecamYawDeg > 180.0f) freecamYawDeg -= 360.0f;
    while (freecamYawDeg < -180.0f) freecamYawDeg += 360.0f;
    const float pitchDelta = freecamInvertY ? dyPixels * sens : -dyPixels * sens;
    freecamPitchDeg = juce::jlimit (-kMaxPitchDeg, kMaxPitchDeg, freecamPitchDeg + pitchDelta);
}

juce::Matrix3D<float> Spectrogram3DComponent::getTurntableViewMatrix() const noexcept
{
    // Orbit around the look-at (pan). LMB tumble only - never freecam.
    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);

    const auto toOrigin = juce::Matrix3D<float>::fromTranslation (
        { -camera.panX, -camera.panY, -camera.panZ });
    const auto rotYaw = juce::Matrix3D<float>::rotation ({ 0.0f, yaw, 0.0f });
    const auto rotPitch = juce::Matrix3D<float>::rotation ({ pitch, 0.0f, 0.0f });
    const auto pullBack = juce::Matrix3D<float>::fromTranslation (
        { 0.0f, 0.0f, -camera.distance });

    return pullBack * rotPitch * rotYaw * toOrigin;
}

juce::Matrix3D<float> Spectrogram3DComponent::getFreecamViewMatrix() const noexcept
{
    /**
        Pure FPS freecam: eye = freecamEye (fixed during mouse-look); only yaw/pitch
        change orientation. Do NOT reuse the orbit boom product (look-at + pull-back) -
        that drifted the effective eye when looking down and felt like a zoom-out.
        Column-major OpenGL view: rows of R are right, up, −forward; t = −R * eye.
    */
    juce::Vector3D<float> right, up, forward;
    freecamBasis (right, up, forward);

    const float m[16] = {
        right.x, up.x, -forward.x, 0.0f,
        right.y, up.y, -forward.y, 0.0f,
        right.z, up.z, -forward.z, 0.0f,
        -(right.x * freecamEye.x + right.y * freecamEye.y + right.z * freecamEye.z),
        -(up.x * freecamEye.x + up.y * freecamEye.y + up.z * freecamEye.z),
        (forward.x * freecamEye.x + forward.y * freecamEye.y + forward.z * freecamEye.z),
        1.0f
    };
    return juce::Matrix3D<float> (m);
}

juce::Matrix3D<float> Spectrogram3DComponent::getActiveViewMatrix() const noexcept
{
    // Hard split: freecam OR normal orbit/pan/zoom - never blended.
    return freecamActive ? getFreecamViewMatrix() : getTurntableViewMatrix();
}

juce::Colour Spectrogram3DComponent::getClearColour() const noexcept
{
    // Soft BG: translucent Osc-like tint. Docked Scope with Soft BG off stays opaque.
    constexpr float kSoftAlpha = 90.0f / 255.0f;
    const bool softTint = transparentBackground;
    if (theme != nullptr)
    {
        const auto base = softTint ? theme->sharedColors.oscBackground
                                   : theme->sharedColors.pluginBackground.darker (0.15f);
        return softTint ? base.withAlpha (kSoftAlpha) : base;
    }
    return softTint ? juce::Colour::fromFloatRGBA (0.06f, 0.07f, 0.09f, kSoftAlpha)
                    : juce::Colour (0xff12151a);
}

juce::Rectangle<int> Spectrogram3DComponent::getInnerFrameLocal() const noexcept
{
    return getLocalBounds().reduced (getShadowPad());
}

juce::Rectangle<int> Spectrogram3DComponent::getGlViewLocal() const noexcept
{
    return getInnerFrameLocal().reduced (chromeMode == ChromeMode::docked ? 1 : kGlInset);
}

bool Spectrogram3DComponent::freecamKeyDown (juce::juce_wchar c) noexcept
{
    const int lo = (int) std::tolower ((unsigned char) c);
    const int hi = (int) std::toupper ((unsigned char) c);
    return juce::KeyPress::isKeyCurrentlyDown (lo)
        || juce::KeyPress::isKeyCurrentlyDown (hi);
}

void Spectrogram3DComponent::tickFreecam (float dt) noexcept
{
    // Freecam-only: moves freecamEye. Never touches orbit pan/yaw/pitch/distance.
    if (! freecamActive)
        return;

    if (! freecamRmbHeld && flyVel.x * flyVel.x + flyVel.y * flyVel.y + flyVel.z * flyVel.z < 1.0e-8f)
    {
        // Coast finished - hand pose back to orbit rig.
        exitFreecamToTurntable();
        return;
    }

    dt = juce::jlimit (0.0f, 0.05f, dt);

    juce::Vector3D<float> right, up, forward;
    freecamBasis (right, up, forward);
    juce::ignoreUnused (up);

    juce::Vector3D<float> wish { 0, 0, 0 };
    if (freecamRmbHeld)
    {
        if (freecamKeyDown ('w')) { wish.x += forward.x; wish.y += forward.y; wish.z += forward.z; }
        if (freecamKeyDown ('s')) { wish.x -= forward.x; wish.y -= forward.y; wish.z -= forward.z; }
        if (freecamKeyDown ('d')) { wish.x += right.x;   wish.y += right.y;   wish.z += right.z; }
        if (freecamKeyDown ('a')) { wish.x -= right.x;   wish.y -= right.y;   wish.z -= right.z; }
        if (freecamKeyDown ('e')) wish.y += 1.0f;
        if (freecamKeyDown ('q')) wish.y -= 1.0f;
    }

    const float wishLen = std::sqrt (wish.x * wish.x + wish.y * wish.y + wish.z * wish.z);
    if (wishLen > 1.0e-5f)
    {
        wish.x /= wishLen; wish.y /= wishLen; wish.z /= wishLen;
        // Fixed base fly speed x user scale (not coupled to orbit dolly).
        constexpr float kBaseFly = 2.5f;
        const float maxSpeed = kBaseFly * freecamSpeedScale;
        const float accel = juce::jmax (0.05f, maxSpeed) * 9.0f;
        flyVel.x += wish.x * accel * dt;
        flyVel.y += wish.y * accel * dt;
        flyVel.z += wish.z * accel * dt;
        const float vLen = std::sqrt (flyVel.x * flyVel.x + flyVel.y * flyVel.y + flyVel.z * flyVel.z);
        if (vLen > maxSpeed && maxSpeed > 1.0e-6f)
        {
            const float s = maxSpeed / vLen;
            flyVel.x *= s; flyVel.y *= s; flyVel.z *= s;
        }
    }
    else
    {
        const float decay = std::exp (-dt * 6.5f);
        flyVel.x *= decay;
        flyVel.y *= decay;
        flyVel.z *= decay;
        if (std::abs (flyVel.x) < 1.0e-4f) flyVel.x = 0.0f;
        if (std::abs (flyVel.y) < 1.0e-4f) flyVel.y = 0.0f;
        if (std::abs (flyVel.z) < 1.0e-4f) flyVel.z = 0.0f;
    }

    if (flyVel.x == 0.0f && flyVel.y == 0.0f && flyVel.z == 0.0f)
        return;

    freecamEye.x += flyVel.x * dt;
    freecamEye.y += flyVel.y * dt;
    freecamEye.z += flyVel.z * dt;
    freecamEye.x = juce::jlimit (-80.0f, 80.0f, freecamEye.x);
    freecamEye.y = juce::jlimit (-80.0f, 80.0f, freecamEye.y);
    freecamEye.z = juce::jlimit (-80.0f, 80.0f, freecamEye.z);
}

bool Spectrogram3DComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onEscape != nullptr)
            onEscape();
        return true;
    }

    // F11 = editor-wide fullscreen toggle (also wired on MainComponent).
    if (key == juce::KeyPress::F11Key)
    {
        if (onToggleFullscreen != nullptr)
            onToggleFullscreen();
        return true;
    }

    if (key == juce::KeyPress ('f') || key == juce::KeyPress ('F'))
    {
        // Don't steal F while freecam is flying (user might mash keys).
        if (! freecamRmbHeld)
        {
            resetCamera();
            return true;
        }
    }

    // Consume freecam keys so host doesn't steal them while RMB-flying.
    if (freecamRmbHeld)
    {
        const auto c = (char) std::tolower ((unsigned char) key.getTextCharacter());
        if (c == 'w' || c == 'a' || c == 's' || c == 'd' || c == 'q' || c == 'e')
            return true;
    }

    return false;
}

bool Spectrogram3DComponent::keyStateChanged (bool /*isKeyDown*/)
{
    // Keep freecam responsive to key up/down while RMB held.
    return freecamRmbHeld;
}

void Spectrogram3DComponent::resized()
{
    const int pad = getShadowPad();
    layoutPresentation();
    markSoftContentDirty();

    if (resizer != nullptr)
    {
        // Keep grip entirely in the shadow chrome so the GL HWND cannot cover it.
        resizer->setBounds (getWidth() - pad, getHeight() - pad, pad, pad);
        resizer->setVisible (chromeMode == ChromeMode::floating && active);
        resizer->toFront (false);
    }

    if (glHost != nullptr)
        glHost->triggerRedraw();

    if (onUserResized != nullptr && resizer != nullptr && resizer->isMouseButtonDown())
        onUserResized();
}

void Spectrogram3DComponent::paint (juce::Graphics& g)
{
    // Match FramedFloatingScopeWindow (Osc / Gon / Spec 2D): shadow -> plate -> content -> stroke.
    // Soft BG plate lives in the soft FBO (drawSoftTint) so DOF can spill into it;
    // do not pre-fill a second translucent plate under a valid soft image.
    const auto inner = getInnerFrameLocal().toFloat();
    const float radius = chromeMode == ChromeMode::docked ? 4.0f : kCornerRadius;
    juce::Path panel;
    panel.addRoundedRectangle (inner, radius);

    if (chromeMode == ChromeMode::floating
        && (theme == nullptr || ! theme->disableGlowShadowEffects))
        panelShadow.render (g, panel);

    if (! usesSoftComposite())
    {
        g.setColour (getClearColour());
        g.fillPath (panel);
    }
    else if (active)
    {
        juce::Image softImg;
        {
            const juce::ScopedLock sl (softImageLock);
            softImg = softCompositeImage;
        }

        // Soft BG + valid FBO: plate already in the image (incl. DOF edge spill).
        // Soft BG off (opaque docked) or still loading: plate under/without the image.
        if (! transparentBackground || ! softImg.isValid())
        {
            g.setColour (getClearColour());
            g.fillPath (panel);
        }

        if (softImg.isValid())
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (panel);
            g.setOpacity (1.0f);
            g.drawImage (softImg, getGlViewLocal().toFloat(),
                         juce::RectanglePlacement::stretchToFit, false);
        }
    }
    else
    {
        g.setColour (getClearColour());
        g.fillPath (panel);
    }

    g.setColour (juce::Colours::white.withAlpha (chromeMode == ChromeMode::docked ? 0.12f : 0.22f));
    g.strokePath (panel, juce::PathStrokeType (chromeMode == ChromeMode::docked ? 1.0f : 1.2f));
    if (chromeMode == ChromeMode::floating)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (panel, juce::PathStrokeType (1.0f),
                      juce::AffineTransform::translation (0.0f, 1.0f));
    }

    if (active && glHost != nullptr && (glHost->hasContextFailed() || ! glHost->isGlReady()))
    {
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.7f));
        g.setFont (12.0f);
        g.drawFittedText (glHost->hasContextFailed()
                              ? "3D spectrogram unavailable (GPU busy? close game / retry Spec)"
                              : "Initialising 3D spectrogram...",
                          inner.toNearestInt().reduced (8),
                          juce::Justification::centred, 3);
    }

    if (active && waterfallFrozen)
    {
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        const juce::Rectangle<int> badge ((int) inner.getX() + 8, (int) inner.getY() + 6, 64, 20);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (badge.toFloat(), 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawFittedText ("FROZEN", badge, juce::Justification::centred, 1);
    }

    if (active && particleModeEnabled && particleDebugOverlayEnabled)
    {
        const int alive = getParticleAliveCount();
        const int maxA = getParticleMaxAlive();
        const int pool = getParticlePoolCapacity();
        const int spawned = getParticleLastSpawnedCount();
        const int culled = getParticleLastCulledCount();
        const float simMs = getParticleLastUpdateMs();
        const int load = getParticleLoadLevel();
        const float fps = getParticleDebugFps();
        const bool atCap = alive >= maxA;
        const bool freeVis = particleBindingMode == ParticleBindingMode::freeVisualizer;
        const bool gpuPath = particleGpuSimEnabled
                             && isParticleGpuSimAvailable();
        const char* emitLabel = (particleEmitMode == ParticleEmitMode::continuous)
                                    ? "cont" : "slice";
        const char* bindLabel = freeVis ? "free" : "trail";
        // +sp = births this sim tick; load 0-3 = hitch recovery tier; fps = timer redraw rate.
        const juce::String line =
            juce::String (gpuPath ? "GPU  " : "CPU  ")
            + juce::String (alive) + "/" + juce::String (maxA)
            + "  pool " + juce::String (pool)
            + "  +sp " + juce::String (spawned)
            + "  -cull " + juce::String (culled)
            + "  " + juce::String (fps, 0) + " fps"
            + "  sim " + juce::String (simMs, 1) + "ms"
            + "  L" + juce::String (load)
            + "  " + emitLabel
            + "  " + bindLabel
            + (atCap ? "  BUDGET" : "")
            + (gpuPath ? "  [GPU]" : "  [CPU]");
        g.setFont (juce::Font (juce::FontOptions (11.0f)).boldened());
        const int tw = juce::jmin ((int) inner.getWidth() - 16, 640);
        const juce::Rectangle<int> badge ((int) inner.getX() + 8,
                                          (int) inner.getBottom() - 28,
                                          tw, 20);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (badge.toFloat(), 4.0f);
        const auto ink = atCap ? juce::Colours::orange
                         : (load >= 2 ? juce::Colours::yellow
                                      : juce::Colours::white);
        g.setColour (ink.withAlpha (0.95f));
        g.drawFittedText (line, badge.reduced (6, 0), juce::Justification::centredLeft, 1);
        // Timer already repaints soft content; no extra repaint(badge) cascade.
    }
}

void Spectrogram3DComponent::timerCallback()
{
    if (! active)
        return;

    const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;

    // Rolling display FPS (this timer's cadence - never need > 60 Hz).
    if (particleFpsWindowStartSec <= 0.0)
        particleFpsWindowStartSec = nowSec;
    ++particleFpsFrameCount;
    const double fpsWin = nowSec - particleFpsWindowStartSec;
    if (fpsWin >= 0.5)
    {
        particleDebugFps = (float) ((double) particleFpsFrameCount / fpsWin);
        particleFpsFrameCount = 0;
        particleFpsWindowStartSec = nowSec;
    }

    // Keep rate in sync if freecam / particle mode toggled mid-session.
    syncSpec3DTimerRate();

    bool cameraMoved = false;

    // Freecam fly + coast (independent of orbit). Same in OS fullscreen.
    if (freecamActive || freecamRmbHeld)
    {
        float dt = 1.0f / 60.0f;
        if (freecamLastTimeSec > 0.0)
            dt = juce::jlimit (0.0f, 0.1f, (float) (nowSec - freecamLastTimeSec));
        freecamLastTimeSec = nowSec;
        const auto eye0 = freecamEye;
        const auto v0 = flyVel;
        tickFreecam (dt);
        if (flyVel.x != v0.x || flyVel.y != v0.y || flyVel.z != v0.z
            || freecamEye.x != eye0.x || freecamEye.y != eye0.y || freecamEye.z != eye0.z
            || freecamRmbHeld || freecamActive)
            cameraMoved = true;
    }

    if (autoRotateEnabled && dragMode == DragMode::none && ! freecamActive
        && flyVel.x == 0.0f && flyVel.y == 0.0f && flyVel.z == 0.0f)
    {
        if (autoRotateLastTimeSec > 0.0)
        {
            const float dt = juce::jlimit (0.0f, 0.1f, (float) (nowSec - autoRotateLastTimeSec));
            const float period = juce::jmax (kAutoRotatePeriodMinSec, autoRotatePeriodSec);
            // Full revolution every `period` seconds (same yaw sense as LMB orbit).
            camera.yawDeg -= 360.0f / period * dt;
            cameraMoved = true;
        }
        autoRotateLastTimeSec = nowSec;
    }
    else
    {
        autoRotateLastTimeSec = 0.0;
    }

    if (zoomOscillateEnabled && dragMode != DragMode::dolly)
    {
        if (zoomOscillateLastTimeSec > 0.0)
        {
            const float dt = juce::jlimit (0.0f, 0.1f, (float) (nowSec - zoomOscillateLastTimeSec));
            const float period = juce::jmax (kZoomOscillatePeriodMinSec, zoomOscillatePeriodSec);
            zoomOscillatePhaseRad += juce::MathConstants<float>::twoPi / period * dt;
            if (zoomOscillatePhaseRad > juce::MathConstants<float>::twoPi)
                zoomOscillatePhaseRad = std::fmod (zoomOscillatePhaseRad,
                                                   juce::MathConstants<float>::twoPi);
            applyZoomOscillateDistance();
            cameraMoved = true;
        }
        zoomOscillateLastTimeSec = nowSec;
    }
    else
    {
        zoomOscillateLastTimeSec = 0.0;
    }

    if (audioLevelModEnabled)
    {
        const float lvl = audioLevelProvider ? audioLevelProvider() : 0.0f;
        if (std::abs (lvl - audioLevelLive01) > 1.0e-4f)
        {
            audioLevelLive01 = lvl;
            markSoftContentDirty();
        }
    }
    else if (audioLevelLive01 != 0.0f)
    {
        audioLevelLive01 = 0.0f;
        markSoftContentDirty();
    }

    // Colour-ramp timeline morph + lighting automation.
    if (rampSequence.enabled && ! rampSequence.clips.empty())
    {
        if (rampTimelineLastTimeSec > 0.0)
        {
            const float dt = juce::jlimit (0.0f, 0.1f, (float) (nowSec - rampTimelineLastTimeSec));
            tickRampTimeline (dt);
        }
        rampTimelineLastTimeSec = nowSec;
    }
    else
    {
        rampTimelineLastTimeSec = 0.0;
        if (morphRampActive)
            clearMorphRamp();
    }

    if (cameraMoved)
        markSoftContentDirty();

    const bool meshRebuilt = updateMeshFromSource();

    // Particle mode: sim only when enabled (lazy system). Always dirties soft content
    // while active so rising particles animate.
    if (particleModeEnabled)
    {
        ensureParticleSystem();
        if (particleSystem != nullptr)
        {
            // Nominal 60 Hz step; max debt from settings (sim catch-up Hz).
            float pdt = 1.0f / 60.0f;
            if (particleLastUpdateSec > 0.0)
            {
                const float maxDt = 1.0f / juce::jmax (10.0f, particleSimCatchupHz);
                pdt = juce::jlimit (0.0f, maxDt, (float) (nowSec - particleLastUpdateSec));
            }
            particleLastUpdateSec = nowSec;
            particleSystem->update (pdt);
            markSoftContentDirty();
        }
    }
    else
    {
        particleLastUpdateSec = 0.0;
    }

    // FRC cumulative-curve eco tracks particle density (MainComponent).
    if (onParticleSimTick != nullptr)
        onParticleSimTick();

    // Lighting automation only writes uniforms. Fold into the mesh soft frame when
    // possible; if the waterfall is idle, throttle light-only soft redraws (~12 Hz)
    // so DOF/post is not run at full timer rate.
    if (lightingUniformsDirty)
    {
        if (meshRebuilt)
        {
            lightingUniformsDirty = false; // uniforms applied on this soft frame
        }
        else if (nowSec - lastLightingSoftRedrawSec >= (1.0 / 12.0))
        {
            // Waterfall idle: light-only soft redraw at ~12 Hz (not full 30 Hz DOF thrash).
            lastLightingSoftRedrawSec = nowSec;
            lightingUniformsDirty = false;
            markSoftContentDirty();
        }
    }

    if (glHost != nullptr)
    {
        glHost->triggerRedraw();
        if (active)
            glHost->requestAttachAsync();
    }

    if (usesSoftComposite())
        repaint();
}

bool Spectrogram3DComponent::isInMoveChrome (juce::Point<int> localPos) const noexcept
{
    if (chromeMode != ChromeMode::floating || ! active)
        return false;
    if (hitLayer != nullptr && hitLayer->isVisible() && hitLayer->getBounds().contains (localPos))
        return false;
    if (glHost != nullptr && ! usesSoftComposite() && glHost->getBounds().contains (localPos))
        return false;
    if (resizer != nullptr && resizer->getBounds().contains (localPos))
        return false;
    if (zoomHandle != nullptr && zoomHandle->isVisible()
        && zoomHandle->getBounds().contains (localPos))
        return false;
    return getLocalBounds().contains (localPos);
}

void Spectrogram3DComponent::mouseDown (const juce::MouseEvent& e)
{
    movingByChrome = isInMoveChrome (e.getPosition()) && e.mods.isLeftButtonDown()
                     && ! e.mods.isPopupMenu();
    if (movingByChrome)
        moveDragger.startDraggingComponent (this, e);
}

void Spectrogram3DComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (! movingByChrome)
        return;
    moveDragger.dragComponent (this, e, &constrainer);
    if (onUserMoved != nullptr)
        onUserMoved();
}

void Spectrogram3DComponent::mouseUp (const juce::MouseEvent&)
{
    movingByChrome = false;
}

void Spectrogram3DComponent::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isInMoveChrome (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                     : juce::MouseCursor::NormalCursor);
}

//==============================================================================
void Spectrogram3DComponent::meshSizeForQuality (int& outW, int& outH) const noexcept
{
    switch (meshQuality)
    {
        case MeshQuality::low:      outW = 64;  outH = 48;  break;
        case MeshQuality::high:     outW = 192; outH = 160; break;
        case MeshQuality::ultra:
        case MeshQuality::overkill: outW = 288; outH = 240; break; // overkill -> ultra
        case MeshQuality::medium:
        default:                    outW = 128; outH = 96;  break;
    }
}

float Spectrogram3DComponent::freqMeshBiasB() const noexcept
{
    return juce::jlimit (0.0f, 1.0f, freqMeshBias) * kFreqMeshBiasMaxB;
}

int Spectrogram3DComponent::effectiveFreqMeshRows (int baseH) const noexcept
{
    baseH = juce::jmax (2, baseH);
    const float B = freqMeshBiasB();
    if (B < 1.0e-5f)
        return baseH;
    // Boost only above pivot P: ∫w = 1 + B*(1-P)/3 -> fewer rows when P is higher.
    const float P = juce::jlimit (0.0f, 0.95f, freqMeshBiasPivot);
    const int n = (int) std::ceil ((double) baseH * (1.0 + (double) B * (1.0 - (double) P) / 3.0));
    return juce::jlimit (baseH, kMaxFreqMeshRows, n);
}

float Spectrogram3DComponent::meshTFromFreqAxis (float u, float B, float pivot) noexcept
{
    u = juce::jlimit (0.0f, 1.0f, u);
    if (B < 1.0e-5f)
        return u;
    const float P = juce::jlimit (0.0f, 0.999f, pivot);
    const float I = 1.0f + B * (1.0f - P) / 3.0f;
    if (u <= P)
        return u / I;
    const float v = u - P;
    const float omp = juce::jmax (1.0e-5f, 1.0f - P);
    return (u + B * v * v * v / (3.0f * omp * omp)) / I;
}

float Spectrogram3DComponent::freqAxisFromMeshT (float t, float B, float pivot) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);
    if (B < 1.0e-5f)
        return t;

    const float P = juce::jlimit (0.0f, 0.999f, pivot);
    const float I = 1.0f + B * (1.0f - P) / 3.0f;
    const float target = t * I;
    if (target <= P)
        return juce::jlimit (0.0f, 1.0f, target);

    // Solve u + (B/(3*omp^2))*(u-P)^3 = target via Newton.
    const float omp = juce::jmax (1.0e-5f, 1.0f - P);
    const float c = B / (3.0f * omp * omp);
    float u = juce::jlimit (P, 1.0f, target); // seed in boost region
    for (int i = 0; i < 8; ++i)
    {
        const float v = u - P;
        const float f = u + c * v * v * v - target;
        const float df = 1.0f + B * v * v / (omp * omp);
        u -= f / juce::jmax (1.0e-6f, df);
        u = juce::jlimit (P, 1.0f, u);
    }
    return u;
}

void Spectrogram3DComponent::fillMeshColumn (int meshCol, const float* histCol, int histH)
{
    if (histCol == nullptr || meshH <= 1 || histH <= 1
        || meshCol < 0 || meshCol >= meshW)
        return;

    const float B = freqMeshBiasB();
    const float P = freqMeshBiasPivot;
    for (int z = 0; z < meshH; ++z)
    {
        const float t = meshH > 1 ? (float) z / (float) (meshH - 1) : 0.0f;
        const float u = freqAxisFromMeshT (t, B, P); // 0=low ... 1=high
        // History: yNorm 0 = high Hz (top), 1 = low Hz (bottom).
        const float yNorm = 1.0f - u;
        const int row = juce::jlimit (0, histH - 1, (int) std::round (yNorm * (float) (histH - 1)));
        meshDb[(size_t) meshCol * (size_t) meshH + (size_t) z] = histCol[row];
    }
}

void Spectrogram3DComponent::seedMeshFromHistory (const std::vector<float>& history, int histW, int histH)
{
    meshDb.assign ((size_t) meshW * (size_t) meshH, -120.0f);
    const int cols = juce::jmin (meshW, histW);
    const int srcStart = histW - cols;
    for (int i = 0; i < cols; ++i)
        fillMeshColumn (meshW - cols + i,
                        history.data() + (size_t) (srcStart + i) * (size_t) histH, histH);

    // Full history reseed - drop particles so they do not sit on stale columns.
    if (particleSystem != nullptr)
        particleSystem->clear();
}

void Spectrogram3DComponent::appendMeshColumnsFromHistory (const std::vector<float>& history,
                                                           int histW, int histH, int numNew)
{
    numNew = juce::jlimit (1, meshW, numNew);
    if (meshW > numNew)
    {
        std::memmove (meshDb.data(),
                      meshDb.data() + (size_t) numNew * (size_t) meshH,
                      (size_t) (meshW - numNew) * (size_t) meshH * sizeof (float));
    }

    const int srcStart = juce::jmax (0, histW - numNew);
    for (int i = 0; i < numNew; ++i)
    {
        const int srcCol = juce::jmin (histW - 1, srcStart + i);
        fillMeshColumn (meshW - numNew + i,
                        history.data() + (size_t) srcCol * (size_t) histH, histH);
    }

    // Keep particles locked to the same history columns as meshDb (scroll −X).
    if (particleModeEnabled && particleSystem != nullptr)
        particleSystem->scrollHistory (numNew);
}

void Spectrogram3DComponent::ensureIndexBuffer (int w, int h)
{
    const bool wantClosed = closedMeshEnabled;
    const bool wantReverse = reverseFrequencyAxis;
    if (indicesValid && meshW == w && meshH == h && meshClosed == wantClosed
        && meshIndexReverseFreq == wantReverse && ! cpuIndices.empty())
        return;

    std::vector<uint32_t> inds;
    const int topCount = w * h;

    // Top heightfield (CCW from +Y). Freq-reverse mirrors Z, so flip winding to match.
    inds.reserve ((size_t) (w - 1) * (size_t) (h - 1) * 6
                  + (wantClosed ? (size_t) (w - 1) * (size_t) (h - 1) * 6
                                  + (size_t) (w + h) * 12 : 0));
    for (int z = 0; z < h - 1; ++z)
    {
        for (int x = 0; x < w - 1; ++x)
        {
            const uint32_t i0 = (uint32_t) (z * w + x);
            const uint32_t i1 = i0 + 1;
            const uint32_t i2 = i0 + (uint32_t) w;
            const uint32_t i3 = i2 + 1;
            if (! wantReverse)
            {
                inds.push_back (i0); inds.push_back (i2); inds.push_back (i1);
                inds.push_back (i1); inds.push_back (i2); inds.push_back (i3);
            }
            else
            {
                inds.push_back (i0); inds.push_back (i1); inds.push_back (i2);
                inds.push_back (i1); inds.push_back (i3); inds.push_back (i2);
            }
        }
    }

    if (wantClosed)
    {
        // Bottom cap: opposite winding of the top (normals down).
        for (int z = 0; z < h - 1; ++z)
        {
            for (int x = 0; x < w - 1; ++x)
            {
                const uint32_t i0 = (uint32_t) (topCount + z * w + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + (uint32_t) w;
                const uint32_t i3 = i2 + 1;
                if (! wantReverse)
                {
                    inds.push_back (i0); inds.push_back (i1); inds.push_back (i2);
                    inds.push_back (i1); inds.push_back (i3); inds.push_back (i2);
                }
                else
                {
                    inds.push_back (i0); inds.push_back (i2); inds.push_back (i1);
                    inds.push_back (i1); inds.push_back (i2); inds.push_back (i3);
                }
            }
        }

        // Walls: each border edge -> quad between top and bottom.
        auto wallQuad = [&] (uint32_t tA, uint32_t tB, uint32_t bA, uint32_t bB)
        {
            inds.push_back (tA); inds.push_back (bA); inds.push_back (tB);
            inds.push_back (tB); inds.push_back (bA); inds.push_back (bB);
        };

        // z = 0 / z = h-1: outward flips when freq-reverse swaps which end is +Z.
        for (int x = 0; x < w - 1; ++x)
        {
            const uint32_t tA = (uint32_t) x;
            const uint32_t tB = (uint32_t) (x + 1);
            if (! wantReverse)
                wallQuad (tA, tB, tA + (uint32_t) topCount, tB + (uint32_t) topCount);
            else
                wallQuad (tB, tA, tB + (uint32_t) topCount, tA + (uint32_t) topCount);
        }
        for (int x = 0; x < w - 1; ++x)
        {
            const uint32_t tA = (uint32_t) ((h - 1) * w + x);
            const uint32_t tB = tA + 1;
            if (! wantReverse)
                wallQuad (tB, tA, tB + (uint32_t) topCount, tA + (uint32_t) topCount);
            else
                wallQuad (tA, tB, tA + (uint32_t) topCount, tB + (uint32_t) topCount);
        }
        // Playhead wall (x = +1) + waterfall-end wall (x = -1): dedicated verts after
        // top+bottom, biased off the shared depth plane with the nearest history columns.
        // Layout: [2*topCount .. +h) ph top, [+h .. +2h) ph bot,
        //         [+2h .. +3h) end top, [+3h .. +4h) end bot.
        const uint32_t phTop = (uint32_t) (topCount * 2);
        const uint32_t phBot = phTop + (uint32_t) h;
        const uint32_t endTop = phBot + (uint32_t) h;
        const uint32_t endBot = endTop + (uint32_t) h;
        for (int z = 0; z < h - 1; ++z)
        {
            const uint32_t tA = phTop + (uint32_t) z;
            const uint32_t tB = tA + 1;
            const uint32_t bA = phBot + (uint32_t) z;
            const uint32_t bB = bA + 1;
            wallQuad (tA, tB, bA, bB);
        }
        // Waterfall end (x = -1): flip winding for outward (-X) normal.
        for (int z = 0; z < h - 1; ++z)
        {
            const uint32_t tA = endTop + (uint32_t) z;
            const uint32_t tB = tA + 1;
            const uint32_t bA = endBot + (uint32_t) z;
            const uint32_t bB = bA + 1;
            wallQuad (tB, tA, bB, bA);
        }
    }

    const juce::ScopedLock sl (meshLock);
    cpuIndices.swap (inds);
    meshClosed = wantClosed;
    meshIndexReverseFreq = wantReverse;
    indicesValid = true;
}

void Spectrogram3DComponent::rebuildVerticesFromMeshDb (float brightness, float minDb, float maxDb)
{
    if (dataSource == nullptr || meshW < 2 || meshH < 2 || meshDb.empty())
        return;

    dataSource->refreshColourLutFor3D();
    ensureIndexBuffer (meshW, meshH);

    const bool closed = closedMeshEnabled;
    const int topCount = meshW * meshH;
    // Closed: top + bottom + dedicated playhead + waterfall-end walls (along Z).
    const int vertCount = closed ? (topCount * 2 + meshH * 4) : topCount;
    // Reuse capacity - avoid multi-MB alloc on every scroll column / morph.
    meshBuildVerts.resize ((size_t) vertCount);
    auto& verts = meshBuildVerts;
    const float denom = juce::jmax (1.0f, maxDb - minDb);
    const float baseY = closed ? -kClosedMeshFloorBias : 0.0f;
    const float playheadWallX = 1.0f + kClosedPlayheadWallBias;
    const float waterfallEndWallX = -1.0f - kClosedWaterfallEndWallBias;

    const float B = freqMeshBiasB();
    const float P = freqMeshBiasPivot;

    auto freqUForRow = [&] (int z) -> float
    {
        const float t = meshH > 1 ? (float) z / (float) (meshH - 1) : 0.0f;
        return freqAxisFromMeshT (t, B, P); // 0=low ... 1=high
    };

    auto worldZForU = [&] (float freqU) -> float
    {
        return reverseFrequencyAxis ? (freqU * 2.0f - 1.0f)
                                    : ((1.0f - freqU) * 2.0f - 1.0f);
    };

    for (int z = 0; z < meshH; ++z)
    {
        const float freqU = freqUForRow (z);
        const float worldZ = worldZForU (freqU);
        for (int x = 0; x < meshW; ++x)
        {
            const float u = (float) x / (float) (meshW - 1);
            const float db = meshDb[(size_t) x * (size_t) meshH + (size_t) z];
            const float norm = juce::jlimit (0.0f, 1.0f, (db - minDb) / denom);
            const auto c = dataSource->colourFromHistoryDb3D (db, brightness, minDb, maxDb);

            auto& vtx = verts[(size_t) z * (size_t) meshW + (size_t) x];
            vtx.x = u * 2.0f - 1.0f;
            vtx.y = norm * meshHeight;
            vtx.z = worldZ;
            vtx.r = c.getFloatRed();
            vtx.g = c.getFloatGreen();
            vtx.b = c.getFloatBlue();
            vtx.nx = 0.0f;
            vtx.ny = 1.0f;
            vtx.nz = 0.0f;

            if (closed)
            {
                auto& bot = verts[(size_t) topCount + (size_t) z * (size_t) meshW + (size_t) x];
                bot = vtx;
                bot.y = baseY;
                bot.nx = 0.0f;
                bot.ny = -1.0f;
                bot.nz = 0.0f;
                // Slightly darker underside so the solid reads as volume.
                bot.r *= 0.55f;
                bot.g *= 0.55f;
                bot.b *= 0.55f;
            }
        }
    }

    // Weighted normals only matter when lit (or SSGI mesh-normals). Skip the
    // triangle walk when flat shading - it was the main mesh-rebuild cost.
    if (lightingEnabled || (ssgiEnabled && ssgiMeshNormalsEnabled))
        computeTopSurfaceNormals (verts, meshW, meshH);

    if (closed)
    {
        // Playhead face: copy newest column, push +X past scroll (history moves -X).
        const int phTop = topCount * 2;
        const int phBot = phTop + meshH;
        const int endTop = phBot + meshH;
        const int endBot = endTop + meshH;
        for (int z = 0; z < meshH; ++z)
        {
            const auto& src = verts[(size_t) z * (size_t) meshW + (size_t) (meshW - 1)];
            auto& top = verts[(size_t) (phTop + z)];
            top = src;
            top.x = playheadWallX;
            top.nx = 1.0f;
            top.ny = 0.0f;
            top.nz = 0.0f;

            auto& bot = verts[(size_t) (phBot + z)];
            bot = src;
            bot.x = playheadWallX;
            bot.y = baseY;
            bot.nx = 1.0f;
            bot.ny = 0.0f;
            bot.nz = 0.0f;
            bot.r *= 0.55f;
            bot.g *= 0.55f;
            bot.b *= 0.55f;
        }
        // Waterfall end: copy oldest column, push −X past history.
        for (int z = 0; z < meshH; ++z)
        {
            const auto& src = verts[(size_t) z * (size_t) meshW];
            auto& top = verts[(size_t) (endTop + z)];
            top = src;
            top.x = waterfallEndWallX;
            top.nx = -1.0f;
            top.ny = 0.0f;
            top.nz = 0.0f;

            auto& bot = verts[(size_t) (endBot + z)];
            bot = src;
            bot.x = waterfallEndWallX;
            bot.y = baseY;
            bot.nx = -1.0f;
            bot.ny = 0.0f;
            bot.nz = 0.0f;
            bot.r *= 0.55f;
            bot.g *= 0.55f;
            bot.b *= 0.55f;
        }
    }

    {
        const juce::ScopedLock sl (meshLock);
        cpuVertices.swap (verts);
        meshNeedsUpload = true;
    }
    // Keep meshBuildVerts capacity after swap (now holds previous cpu verts).
    if (meshBuildVerts.capacity() < (size_t) vertCount)
        meshBuildVerts.reserve ((size_t) vertCount);
}

void Spectrogram3DComponent::setWaterfallFrozen (bool shouldFreeze) noexcept
{
    if (waterfallFrozen == shouldFreeze)
        return;
    waterfallFrozen = shouldFreeze;
    if (! waterfallFrozen && dataSource != nullptr)
    {
        // Resume at live head - don't burst-drain columns accumulated while frozen.
        lastHistorySerial = dataSource->getHistoryColumnSerial();
    }
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

bool Spectrogram3DComponent::updateMeshFromSource()
{
    if (dataSource == nullptr || ! dataSource->isSpectrogramEnabled())
        return false;

    const uint64_t serial = dataSource->getHistoryColumnSerial();

    // Live scroll: skip full history copy when nothing new arrived (was a major hitch).
    // Frozen still falls through so brightness/range look changes can recolour.
    if (serial == lastHistorySerial && meshW >= 2 && ! meshDb.empty() && ! waterfallFrozen)
        return false;

    std::vector<float> history;
    int histW = 0, histH = 0;
    float brightness = 1.0f, minDb = -90.0f, maxDb = -6.0f;
    dataSource->getHistorySnapshot (history, histW, histH, brightness, minDb, maxDb);
    if (histW < 2 || histH < 2 || history.empty())
        return false;

    int wantW = 0, baseH = 0;
    meshSizeForQuality (wantW, baseH);
    wantW = juce::jmin (wantW, histW);
    // Base H may exceed histH (oversample); bias then adds HF rows without thinning lows.
    const int wantH = effectiveFreqMeshRows (baseH);

    const bool sizeChanged = (wantW != meshW || wantH != meshH || meshDb.empty());
    const bool historyReset = (serial < lastHistorySerial);
    const bool lookChanged = (brightness != lastBrightness || minDb != lastMinDb || maxDb != lastMaxDb);

    if (sizeChanged || historyReset)
    {
        meshW = wantW;
        meshH = wantH;
        indicesValid = false;
        seedMeshFromHistory (history, histW, histH);
        lastHistorySerial = serial;
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
        return true;
    }

    if (waterfallFrozen)
    {
        // Hold the mesh; stay serial-synced so unfreeze doesn't catch up a backlog.
        lastHistorySerial = serial;
        if (lookChanged)
        {
            lastBrightness = brightness;
            lastMinDb = minDb;
            lastMaxDb = maxDb;
            rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
            return true;
        }
        return false;
    }

    if (serial > lastHistorySerial)
    {
        // Spec can write up to kMaxColumnsPerTick per 60 Hz tick (and more when the
        // UI timer jitters). Consuming the whole serial delta scrolled several mesh
        // columns in one frame -> timebase bursts. Always advance exactly one column
        // per 3D tick at the live tip; if >1 behind, snap to latest-1 then append
        // the newest history column (stay realtime without multi-column shifts).
        if (serial - lastHistorySerial > 1)
            lastHistorySerial = serial - 1;

        appendMeshColumnsFromHistory (history, histW, histH, 1);
        lastHistorySerial = serial;
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
        return true;
    }

    if (lookChanged)
    {
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
        return true;
    }

    return false;
}

float Spectrogram3DComponent::worldZForFreq (float hz, double sampleRate, bool logFreq) const noexcept
{
    const float nyquist = (float) (juce::jmax (1.0, sampleRate) * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);
    const float minHz = SpectrogramComponent::kMinDisplayHz;
    if (maxHz <= minHz + 1.0f)
        return 0.0f;

    hz = juce::jlimit (minHz, maxHz, hz);
    float t = 0.0f;
    if (logFreq)
        t = std::log (hz / minHz) / std::log (maxHz / minHz);
    else
        t = (hz - minHz) / (maxHz - minHz);
    if (reverseFrequencyAxis)
        return t * 2.0f - 1.0f;
    return (1.0f - t) * 2.0f - 1.0f;
}

void Spectrogram3DComponent::setReverseFrequencyAxis (bool shouldReverse) noexcept
{
    if (reverseFrequencyAxis == shouldReverse)
        return;
    reverseFrequencyAxis = shouldReverse;
    // Z mirror changes triangle winding - rebuild indices with the verts.
    indicesValid = false;
    // Remap existing history - do not clear meshDb / serial.
    // Colours stay intensity->ramp; only world Z / winding / normals update.
    if (meshW >= 2 && meshH >= 2 && ! meshDb.empty() && lastBrightness >= 0.0f)
        rebuildVerticesFromMeshDb (lastBrightness, lastMinDb, lastMaxDb);
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    repaint();
}

juce::String Spectrogram3DComponent::formatGridHz (float hz)
{
    if (hz >= 1000.0f)
    {
        const float k = hz / 1000.0f;
        if (std::abs (k - std::round (k)) < 0.05f)
            return juce::String ((int) std::round (k)) + "k";
        return juce::String (k, 1) + "k";
    }
    return juce::String ((int) std::round (hz));
}

void Spectrogram3DComponent::rebuildFreqLabels (double sampleRate, bool logFreq)
{
    freqLabels.clear();
    const float nyquist = (float) (juce::jmax (1.0, sampleRate) * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);

    for (float hz : kMajorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        FreqLabel lb;
        lb.hz = hz;
        lb.text = formatGridHz (hz);
        lb.worldZ = worldZForFreq (hz, sampleRate, logFreq);
        freqLabels.push_back (std::move (lb));
    }
    labelAtlasDirty = true;
}

//==============================================================================
namespace
{
    void styleMenuSlider (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setScrollWheelEnabled (false);
        slider.setColour (juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha (0.55f));
        slider.setColour (juce::Slider::thumbColourId, juce::Colours::goldenrod);
        slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
    }

    void styleMenuLabels (juce::Label& label, juce::Label& valueLabel)
    {
        label.setJustificationType (juce::Justification::centredLeft);
        label.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
        label.setFont (juce::FontOptions().withName ("Lato").withHeight (13.0f));
        valueLabel.setJustificationType (juce::Justification::centredRight);
        valueLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
        valueLabel.setFont (juce::FontOptions().withName ("Lato").withHeight (13.0f));
    }

    /** Period slider under Turntable - does not dismiss the menu when dragged. */
    class AutoRotatePeriodMenuItem final : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit AutoRotatePeriodMenuItem (Spectrogram3DComponent& o)
            : juce::PopupMenu::CustomComponent (false),
              owner (o)
        {
            label.setText ("Speed", juce::dontSendNotification);
            styleMenuLabels (label, valueLabel);
            addAndMakeVisible (label);
            addAndMakeVisible (valueLabel);
            styleMenuSlider (slider);
            slider.setRange (Spectrogram3DComponent::kAutoRotatePeriodMinSec,
                             Spectrogram3DComponent::kAutoRotatePeriodMaxSec,
                             1.0);
            slider.setSkewFactorFromMidPoint (10.0);
            slider.setValue (owner.getAutoRotatePeriodSec(), juce::dontSendNotification);
            slider.onValueChange = [this]
            {
                owner.setAutoRotatePeriodSec ((float) slider.getValue());
                refreshValueLabel();
            };
            addAndMakeVisible (slider);
            refreshValueLabel();
        }

        void getIdealSize (int& idealWidth, int& idealHeight) override
        {
            idealWidth = 260;
            idealHeight = 44;
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 4);
            auto top = r.removeFromTop (16);
            label.setBounds (top.removeFromLeft (48));
            valueLabel.setBounds (top);
            r.removeFromTop (2);
            slider.setBounds (r);
        }

    private:
        void refreshValueLabel()
        {
            const int sec = juce::roundToInt (owner.getAutoRotatePeriodSec());
            valueLabel.setText ("1x / " + juce::String (sec) + " s", juce::dontSendNotification);
        }

        Spectrogram3DComponent& owner;
        juce::Label label;
        juce::Label valueLabel;
        juce::Slider slider;
    };

    class ZoomOscillateDepthMenuItem final : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit ZoomOscillateDepthMenuItem (Spectrogram3DComponent& o)
            : juce::PopupMenu::CustomComponent (false),
              owner (o)
        {
            label.setText ("Depth", juce::dontSendNotification);
            styleMenuLabels (label, valueLabel);
            addAndMakeVisible (label);
            addAndMakeVisible (valueLabel);
            styleMenuSlider (slider);
            slider.setRange (0.0, 0.85, 0.01);
            slider.setValue (owner.getZoomOscillateDepth(), juce::dontSendNotification);
            slider.onValueChange = [this]
            {
                owner.setZoomOscillateDepth ((float) slider.getValue());
                refreshValueLabel();
            };
            addAndMakeVisible (slider);
            refreshValueLabel();
        }

        void getIdealSize (int& idealWidth, int& idealHeight) override
        {
            idealWidth = 260;
            idealHeight = 44;
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 4);
            auto top = r.removeFromTop (16);
            label.setBounds (top.removeFromLeft (48));
            valueLabel.setBounds (top);
            r.removeFromTop (2);
            slider.setBounds (r);
        }

    private:
        void refreshValueLabel()
        {
            valueLabel.setText (juce::String (juce::roundToInt (owner.getZoomOscillateDepth() * 100.0f)) + "%",
                                juce::dontSendNotification);
        }

        Spectrogram3DComponent& owner;
        juce::Label label;
        juce::Label valueLabel;
        juce::Slider slider;
    };

    class ZoomOscillateRateMenuItem final : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit ZoomOscillateRateMenuItem (Spectrogram3DComponent& o)
            : juce::PopupMenu::CustomComponent (false),
              owner (o)
        {
            label.setText ("Rate", juce::dontSendNotification);
            styleMenuLabels (label, valueLabel);
            addAndMakeVisible (label);
            addAndMakeVisible (valueLabel);
            styleMenuSlider (slider);
            slider.setRange (Spectrogram3DComponent::kZoomOscillatePeriodMinSec,
                             Spectrogram3DComponent::kZoomOscillatePeriodMaxSec,
                             1.0);
            slider.setSkewFactorFromMidPoint (8.0);
            slider.setValue (owner.getZoomOscillatePeriodSec(), juce::dontSendNotification);
            slider.onValueChange = [this]
            {
                owner.setZoomOscillatePeriodSec ((float) slider.getValue());
                refreshValueLabel();
            };
            addAndMakeVisible (slider);
            refreshValueLabel();
        }

        void getIdealSize (int& idealWidth, int& idealHeight) override
        {
            idealWidth = 260;
            idealHeight = 44;
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (10, 4);
            auto top = r.removeFromTop (16);
            label.setBounds (top.removeFromLeft (48));
            valueLabel.setBounds (top);
            r.removeFromTop (2);
            slider.setBounds (r);
        }

    private:
        void refreshValueLabel()
        {
            const int sec = juce::roundToInt (owner.getZoomOscillatePeriodSec());
            valueLabel.setText ("1x / " + juce::String (sec) + " s", juce::dontSendNotification);
        }

        Spectrogram3DComponent& owner;
        juce::Label label;
        juce::Label valueLabel;
        juce::Slider slider;
    };

    class RampTimelineMenuItem final : public juce::PopupMenu::CustomComponent
    {
    public:
        RampTimelineMenuItem (Spectrogram3DComponent& o, SharedResources& res, ColourRampBank& bank)
            : juce::PopupMenu::CustomComponent (false),
              owner (o),
              timeline (res, bank, o.getRampSequence())
        {
            timeline.setExpandedLayout (false);
            timeline.setShowExpandButton (true);
            timeline.playheadProvider = [this] { return owner.getRampTimelinePlayheadSec(); };
            timeline.setPlayheadSec (o.getRampTimelinePlayheadSec());
            timeline.onSequenceChanged = [this]
            {
                if (owner.onRampSequenceChanged != nullptr)
                    owner.onRampSequenceChanged();
            };
            timeline.onEnabledChanged = [this]
            {
                if (! owner.getRampSequence().enabled)
                    owner.clearMorphRamp();
                if (owner.onRampSequenceChanged != nullptr)
                    owner.onRampSequenceChanged();
            };
            timeline.onRequestExpand = [this]
            {
                if (owner.onRequestRampTimelineExpand != nullptr)
                    owner.onRequestRampTimelineExpand();
            };
            timeline.onExportRegionOffline = [this] (const Spec3DExportSettings& s)
            {
                if (owner.onExportRegionOffline != nullptr)
                    owner.onExportRegionOffline (s);
            };
            addAndMakeVisible (timeline);
        }

        void getIdealSize (int& idealWidth, int& idealHeight) override
        {
            idealWidth = 320;
            idealHeight = timeline.getPreferredHeight() + 8;
        }

        void resized() override
        {
            timeline.setBounds (getLocalBounds().reduced (6, 4));
        }

        void updatePlayhead()
        {
            timeline.setPlayheadSec (owner.getRampTimelinePlayheadSec());
        }

    private:
        Spectrogram3DComponent& owner;
        Spec3DRampTimelineComponent timeline;
    };
}

void Spectrogram3DComponent::invalidateMorphSchedule() noexcept
{
    lastMorphClipIndex = -1;
    lastMorphFadeStep = -1;
}

void Spectrogram3DComponent::setRampSequence (const Spec3DRampSequence& s) noexcept
{
    rampSequence = s;
    rampSequence.clamp();
    if (colourRampBank != nullptr)
        rampSequence.hydrateFromStore (colourRampBank->getPresets());
    invalidateMorphSchedule();
    if (! rampSequence.enabled)
        clearMorphRamp();
}

void Spectrogram3DComponent::clearMorphRamp() noexcept
{
    if (! morphRampActive)
        return;
    morphRampActive = false;
    invalidateMorphSchedule();
    // Restore bank ramp; recolour once so the mesh doesn't keep sequence colours.
    if (dataSource != nullptr && colourRampBank != nullptr)
    {
        const auto& bankRamp = colourRampBank->get (ColourRampBank::Target::spectrogram3D);
        dataSource->setCustomColourRamp3D (bankRamp.isUsable() ? &bankRamp : nullptr);
        recolourVertexColoursOnly();
    }
}

void Spectrogram3DComponent::applyMorphLightingAutomation() noexcept
{
    if (! rampSequence.enabled || rampSequence.autoLanes.empty())
        return;

    bool anyEnabled = false;
    for (const auto& lane : rampSequence.autoLanes)
        if (lane.enabled) { anyEnabled = true; break; }
    if (! anyEnabled)
        return;

    // Enable lighting once (normals rebuild once via lastBrightness). Never call
    // markLookDirty / markSoftContentDirty here - that re-ran the full soft FBO+DOF
    // stack every envelope sample and was the sequencer stutter.
    if (! lightingEnabled)
    {
        lightingEnabled = true;
        lastBrightness = -1.0f; // next mesh rebuild includes weighted normals
        lightingUniformsDirty = true;
    }

    auto setAmt = [&] (float& field, float v, float lo, float hi)
    {
        v = juce::jlimit (lo, hi, v);
        if (std::abs (field - v) > 1.0e-4f)
        {
            field = v;
            lightingUniformsDirty = true;
        }
    };
    auto setDeg = [&] (float& field, float v)
    {
        while (v > 180.0f) v -= 360.0f;
        while (v < -180.0f) v += 360.0f;
        if (std::abs (field - v) > 1.0e-3f)
        {
            field = v;
            lightingUniformsDirty = true;
        }
    };

    rampSequence.evaluateAutomation (
        rampPlayheadSec,
        [&] (Spec3DSeqLaneType type, float v)
        {
            switch (type)
            {
                case Spec3DSeqLaneType::lightAmount:    setAmt (lightingAmount, v, 0.0f, 1.0f); break;
                case Spec3DSeqLaneType::lightAzimuth:   setDeg (lightAzimuthDeg, v); break;
                case Spec3DSeqLaneType::lightElevation:
                    setAmt (lightElevationDeg, v, 5.0f, 89.0f); break;
                case Spec3DSeqLaneType::rimAmount:      setAmt (rimAmount, v, 0.0f, 1.0f); break;
                default: break;
            }
        },
        [&] (Spec3DSeqLaneType type, juce::Colour col)
        {
            if (type == Spec3DSeqLaneType::lightColour)
            {
                if (lightColour != col) { lightColour = col; lightingUniformsDirty = true; }
            }
            else if (type == Spec3DSeqLaneType::rimColour)
            {
                if (rimColour != col) { rimColour = col; lightingUniformsDirty = true; }
            }
        });
    // Soft composite reads these uniforms on the next mesh/soft draw - no extra FBO.
}

void Spectrogram3DComponent::applyMorphRampIfNeeded() noexcept
{
    if (! rampSequence.enabled || colourRampBank == nullptr || dataSource == nullptr)
        return;

    if (! rampSequence.rampLaneEnabled || rampSequence.clips.empty())
    {
        if (morphRampActive)
            clearMorphRamp();
        return;
    }

    // evaluate() does solid hold + continuous lerpRamps across crossfadeOutSec.
    if (! rampSequence.evaluate (rampPlayheadSec, morphRamp))
    {
        clearMorphRamp();
        return;
    }

    morphRamp.mapMode = GradientRamp::MapMode::intensityLowToHigh;
    morphRamp.enabled = true;

    // Unique revision every apply. lerpRamps() always yields revision==1 on a fresh
    // GradientRamp, so without this setCustomColourRamp3D early-outs after the first
    // fade frame and the 3D LUT freezes (hard preset cuts instead of a crossfade).
    morphRamp.revision = ++morphRampSerial;

    const float len = juce::jlimit (Spec3DRampSequence::kMinLengthSec,
                                    Spec3DRampSequence::kMaxLengthSec,
                                    rampSequence.lengthSec);
    float t = std::fmod (rampPlayheadSec, len);
    if (t < 0.0f)
        t += len;

    rampSequence.buildLayout (morphLayoutCache);
    const int n = (int) morphLayoutCache.size();
    int solid = 0;
    for (int i = 0; i < n; ++i)
    {
        if (t >= morphLayoutCache[(size_t) i].startSec
            && t < morphLayoutCache[(size_t) i].endSec)
        {
            solid = i;
            break;
        }
        if (i == n - 1)
            solid = i;
    }

    const auto& L = morphLayoutCache[(size_t) juce::jlimit (0, juce::jmax (0, n - 1), solid)];
    const int clipIdx = L.index;
    const float fadeStart = L.endSec - L.fadeOutSec;
    const bool inFade = n > 0 && L.fadeOutSec > 1.0e-5f && t >= fadeStart && t < L.endSec;

    // Always push LUT during crossfade; solid only when clip changes / leave fade.
    const bool needUpdate = inFade
                            || lastMorphClipIndex != clipIdx
                            || lastMorphFadeStep >= 0
                            || ! morphRampActive;

    if (needUpdate)
    {
        dataSource->setCustomColourRamp3D (&morphRamp);
        morphRampActive = true;
        lastMorphClipIndex = clipIdx;
        lastMorphFadeStep = inFade ? 1 : -1;
        recolourVertexColoursOnly();
    }
}

void Spectrogram3DComponent::tickRampTimeline (float dt) noexcept
{
    if (! rampSequence.enabled || rampSequence.clips.empty())
    {
        rampTimelineLastTimeSec = 0.0;
        return;
    }

    rampPlayheadSec += dt;
    const float len = juce::jmax (Spec3DRampSequence::kMinLengthSec, rampSequence.lengthSec);
    if (rampPlayheadSec >= len)
        rampPlayheadSec = std::fmod (rampPlayheadSec, len);
    if (rampPlayheadSec < 0.0f)
        rampPlayheadSec += len;

    applyMorphRampIfNeeded();
    applyMorphLightingAutomation();
}

void Spectrogram3DComponent::setRampTimelinePlayheadSec (float sec) noexcept
{
    const float len = juce::jmax (Spec3DRampSequence::kMinLengthSec, rampSequence.lengthSec);
    float t = sec;
    if (len > 1.0e-4f)
    {
        t = std::fmod (t, len);
        if (t < 0.0f)
            t += len;
    }
    else
        t = 0.0f;

    if (std::abs (rampPlayheadSec - t) < 1.0e-6f)
    {
        // Still re-apply morph so export seeks are deterministic.
        applyMorphRampIfNeeded();
        applyMorphLightingAutomation();
        markSoftContentDirty();
        return;
    }

    rampPlayheadSec = t;
    // Force morph re-eval on seek (invalidate last-clip cache).
    lastMorphClipIndex = -2;
    lastMorphFadeStep = -2;
    applyMorphRampIfNeeded();
    applyMorphLightingAutomation();
    markSoftContentDirty();
}

juce::Image Spectrogram3DComponent::copySoftCompositeImage() const
{
    const juce::ScopedLock sl (softImageLock);
    if (! softCompositeImage.isValid())
        return {};
    return softCompositeImage.createCopy();
}

juce::Image Spectrogram3DComponent::captureExportFrame (int width, int height)
{
    if (glHost == nullptr || ! glHost->isGlReady())
        return {};

    juce::Image out;
    // OpenGLContext::executeOnGLThread is the correct barrier for offline readback.
    glHost->getOpenGLContext().executeOnGLThread (
        [this, width, height, &out] (juce::OpenGLContext&)
        {
            glHost->captureSoftFrameOnGlThread (width, height, out);
        },
        true);
    return out;
}

void Spectrogram3DComponent::showContextMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Freeze", true, waterfallFrozen);
    menu.addSeparator();
    const bool fs = (isFullscreenQuery != nullptr && isFullscreenQuery());
    menu.addItem (kContextMenuFullscreenId,
                  fs ? "Exit Fullscreen (F11)" : "Fullscreen (F11)",
                  true, fs);
    menu.addSeparator();
    menu.addItem (2, "Save as Default View");
    menu.addItem (3, "Reset Camera (F)");
    menu.addSeparator();
    menu.addItem (4, "Turntable", true, autoRotateEnabled);
    if (autoRotateEnabled)
        menu.addCustomItem (5, std::make_unique<AutoRotatePeriodMenuItem> (*this), nullptr, "Turntable speed");
    menu.addItem (6, "Oscillate Zoom", true, zoomOscillateEnabled);
    if (zoomOscillateEnabled)
    {
        menu.addCustomItem (7, std::make_unique<ZoomOscillateDepthMenuItem> (*this), nullptr, "Zoom depth");
        menu.addCustomItem (8, std::make_unique<ZoomOscillateRateMenuItem> (*this), nullptr, "Zoom rate");
    }

    menu.addSeparator();
    if (colourRampBank != nullptr && theme != nullptr)
    {
        menu.addCustomItem (10,
                            std::make_unique<RampTimelineMenuItem> (*this, *theme, *colourRampBank),
                            nullptr, "Ramp timeline");
    }

    if (onAugmentContextMenu != nullptr)
    {
        menu.addSeparator();
        onAugmentContextMenu (menu);
    }
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 })
                            .withMousePosition(),
        [safe = juce::Component::SafePointer<Spectrogram3DComponent> (this)] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == kContextMenuFullscreenId)
            {
                if (safe->onToggleFullscreen != nullptr)
                    safe->onToggleFullscreen();
                return;
            }
            if (safe->onContextMenuResult != nullptr && safe->onContextMenuResult (result))
                return;
            if (result == 1)
                safe->setWaterfallFrozen (! safe->isWaterfallFrozen());
            else if (result == 2)
                safe->saveAsDefaultView();
            else if (result == 3)
                safe->resetCamera();
            else if (result == 4)
                safe->setAutoRotateEnabled (! safe->isAutoRotateEnabled());
            else if (result == 6)
                safe->setZoomOscillateEnabled (! safe->isZoomOscillateEnabled());
        });
}

bool Spectrogram3DComponent::isRightMouse (const juce::MouseEvent& e) noexcept
{
    return e.mods.isRightButtonDown()
        || ((e.mods.getRawFlags() & juce::ModifierKeys::rightButtonModifier) != 0)
        || (e.mods.isPopupMenu() && ! e.mods.isLeftButtonDown() && ! e.mods.isMiddleButtonDown());
}

float Spectrogram3DComponent::heightAtWorldXZ (float wx, float wz) const noexcept
{
    if (meshW < 2 || meshH < 2 || meshDb.empty() || lastBrightness < 0.0f)
        return 0.0f;

    const float u = juce::jlimit (0.0f, 1.0f, (wx + 1.0f) * 0.5f);
    float freqU = reverseFrequencyAxis ? ((wz + 1.0f) * 0.5f)
                                       : ((1.0f - wz) * 0.5f);
    freqU = juce::jlimit (0.0f, 1.0f, freqU);
    const float t = meshTFromFreqAxis (freqU, freqMeshBiasB(), freqMeshBiasPivot);

    const float fx = u * (float) (meshW - 1);
    const float fz = t * (float) (meshH - 1);
    const int x0 = juce::jlimit (0, meshW - 2, (int) std::floor (fx));
    const int z0 = juce::jlimit (0, meshH - 2, (int) std::floor (fz));
    const float tx = fx - (float) x0;
    const float tz = fz - (float) z0;

    auto sampleDb = [this] (int x, int z) -> float
    {
        return meshDb[(size_t) x * (size_t) meshH + (size_t) z];
    };

    const float d00 = sampleDb (x0, z0);
    const float d10 = sampleDb (x0 + 1, z0);
    const float d01 = sampleDb (x0, z0 + 1);
    const float d11 = sampleDb (x0 + 1, z0 + 1);
    const float db = d00 * (1.0f - tx) * (1.0f - tz)
                   + d10 * tx * (1.0f - tz)
                   + d01 * (1.0f - tx) * tz
                   + d11 * tx * tz;

    const float denom = juce::jmax (1.0f, lastMaxDb - lastMinDb);
    const float n = juce::jlimit (0.0f, 1.0f, (db - lastMinDb) / denom);
    return n * meshHeight;
}

bool Spectrogram3DComponent::pickDofFocusAtLocalPoint (juce::Point<float> localPos) noexcept
{
    const auto view = getGlViewLocal().toFloat();
    if (view.getWidth() < 2.0f || view.getHeight() < 2.0f)
        return false;
    if (! view.contains (localPos))
        return false;

    const float nx = ((localPos.x - view.getX()) / view.getWidth()) * 2.0f - 1.0f;
    const float ny = 1.0f - ((localPos.y - view.getY()) / view.getHeight()) * 2.0f;

    juce::Vector3D<float> right, up, forward;
    cameraBasis (right, up, forward);

    // Eye = look-at + offset; offset matches getTurntableViewMatrix pull-back.
    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    const float cy = std::cos (yaw);
    const float sy = std::sin (yaw);
    const juce::Vector3D<float> eye {
        camera.panX + (-sy * cp) * camera.distance,
        camera.panY + sp * camera.distance,
        camera.panZ + (cy * cp) * camera.distance
    };

    // Same frustum as GlHost::getProjectionMatrix (near half-width = 1/1.5).
    constexpr float kTanHalfW = 1.0f / 1.5f;
    const float aspect = view.getHeight() / juce::jmax (1.0f, view.getWidth());
    const float tanHalfH = kTanHalfW * aspect;
    juce::Vector3D<float> dir {
        forward.x + right.x * (nx * kTanHalfW) + up.x * (ny * tanHalfH),
        forward.y + right.y * (nx * kTanHalfW) + up.y * (ny * tanHalfH),
        forward.z + right.z * (nx * kTanHalfW) + up.z * (ny * tanHalfH)
    };
    const float dirLen = juce::jmax (1.0e-6f,
                                     std::sqrt (dir.x * dir.x + dir.y * dir.y + dir.z * dir.z));
    dir.x /= dirLen;
    dir.y /= dirLen;
    dir.z /= dirLen;

    constexpr float kTMin = 0.05f;
    constexpr float kTMax = 24.0f;
    constexpr int kSteps = 96;
    float tHit = -1.0f;
    float tPrev = kTMin;
    bool prevAbove = true;

    for (int i = 0; i <= kSteps; ++i)
    {
        const float t = kTMin + (kTMax - kTMin) * ((float) i / (float) kSteps);
        const juce::Vector3D<float> p {
            eye.x + dir.x * t,
            eye.y + dir.y * t,
            eye.z + dir.z * t
        };

        // Slightly padded footprint so edge picks still register.
        if (p.x < -1.15f || p.x > 1.15f || p.z < -1.15f || p.z > 1.15f)
        {
            tPrev = t;
            prevAbove = true;
            continue;
        }

        const float h = heightAtWorldXZ (juce::jlimit (-1.0f, 1.0f, p.x),
                                         juce::jlimit (-1.0f, 1.0f, p.z));
        const bool above = p.y > h + 0.001f;
        if (prevAbove && ! above)
        {
            // Binary refine the crossing.
            float lo = tPrev, hi = t;
            for (int r = 0; r < 12; ++r)
            {
                const float mid = 0.5f * (lo + hi);
                const juce::Vector3D<float> pm {
                    eye.x + dir.x * mid,
                    eye.y + dir.y * mid,
                    eye.z + dir.z * mid
                };
                const float hm = heightAtWorldXZ (juce::jlimit (-1.0f, 1.0f, pm.x),
                                                  juce::jlimit (-1.0f, 1.0f, pm.z));
                if (pm.y > hm + 0.001f)
                    lo = mid;
                else
                    hi = mid;
            }
            tHit = hi;
            break;
        }
        tPrev = t;
        prevAbove = above;
    }

    if (tHit < 0.0f)
        return false;

    const juce::Vector3D<float> hit {
        eye.x + dir.x * tHit,
        eye.y + dir.y * tHit,
        eye.z + dir.z * tHit
    };
    // Post DOF focus uses view-space depth (look-at distance), not Euclidean.
    const float viewDepth = (hit.x - eye.x) * forward.x
                          + (hit.y - eye.y) * forward.y
                          + (hit.z - eye.z) * forward.z;
    if (viewDepth < kDofFocusMin * 0.5f)
        return false;

    setDofFocusDistance (viewDepth, true);
    return true;
}

Spectrogram3DComponent::DragMode Spectrogram3DComponent::hitTestDebugGizmo (juce::Point<float> localPos) const noexcept
{
    if (! debugSphereEnabled || glHost == nullptr || ! glHost->isGlReady())
        return DragMode::none;

    const auto& c = debugSpherePosition;
    const float len = juce::jmax (0.08f, debugSphereDiameter * 0.9f);
    struct Axis { juce::Vector3D<float> end; DragMode mode; };
    const Axis axes[] = {
        { { c.x + len, c.y, c.z }, DragMode::gizmoX },
        { { c.x, c.y + len, c.z }, DragMode::gizmoY },
        { { c.x, c.y, c.z + len }, DragMode::gizmoZ },
    };

    const auto view = getGlViewLocal().toFloat();
    // Slightly wider pick for cylinder+cone shafts.
    float bestDist = 22.0f;
    DragMode best = DragMode::none;
    for (const auto& ax : axes)
    {
        float ndc0x, ndc0y, ndc0z, ndc1x, ndc1y, ndc1z;
        if (! glHost->projectWorldToNdc (c.x, c.y, c.z, ndc0x, ndc0y, ndc0z)
            || ! glHost->projectWorldToNdc (ax.end.x, ax.end.y, ax.end.z, ndc1x, ndc1y, ndc1z))
            continue;
        const float x0 = view.getX() + (ndc0x * 0.5f + 0.5f) * view.getWidth();
        const float y0 = view.getY() + (1.0f - (ndc0y * 0.5f + 0.5f)) * view.getHeight();
        const float x1 = view.getX() + (ndc1x * 0.5f + 0.5f) * view.getWidth();
        const float y1 = view.getY() + (1.0f - (ndc1y * 0.5f + 0.5f)) * view.getHeight();
        // Distance from point to segment.
        const float vx = x1 - x0, vy = y1 - y0;
        const float wx = localPos.x - x0, wy = localPos.y - y0;
        const float vv = vx * vx + vy * vy;
        const float t = vv > 1.0e-6f ? juce::jlimit (0.0f, 1.0f, (wx * vx + wy * vy) / vv) : 0.0f;
        const float dx = localPos.x - (x0 + vx * t);
        const float dy = localPos.y - (y0 + vy * t);
        const float d = std::sqrt (dx * dx + dy * dy);
        if (d < bestDist)
        {
            bestDist = d;
            best = ax.mode;
        }
    }
    return best;
}

bool Spectrogram3DComponent::tryPickDebugGizmo (juce::Point<float> localPos) noexcept
{
    const auto best = hitTestDebugGizmo (localPos);
    if (best == DragMode::none)
        return false;

    dragMode = best;
    gizmoHoverAxis = best;
    gizmoDragStartPos = localPos;
    gizmoDragStartSpherePos = debugSpherePosition;
    return true;
}

bool Spectrogram3DComponent::isGizmoXrayActive() const noexcept
{
    return gizmoHoverAxis != DragMode::none
        || dragMode == DragMode::gizmoX
        || dragMode == DragMode::gizmoY
        || dragMode == DragMode::gizmoZ;
}

void Spectrogram3DComponent::updateGizmoHover (juce::Point<float> localPos) noexcept
{
    // Keep hover latched while dragging an axis.
    if (dragMode == DragMode::gizmoX || dragMode == DragMode::gizmoY || dragMode == DragMode::gizmoZ)
    {
        if (gizmoHoverAxis != dragMode)
        {
            gizmoHoverAxis = dragMode;
            markSoftContentDirty();
            if (glHost != nullptr)
                glHost->triggerRedraw();
            if (usesSoftComposite())
                repaint();
        }
        return;
    }

    const auto next = hitTestDebugGizmo (localPos);
    if (next == gizmoHoverAxis)
        return;

    gizmoHoverAxis = next;
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::handleMouseMove (const juce::MouseEvent& e)
{
    updateGizmoHover (e.position);
    setMouseCursor (isInMoveChrome (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                     : (gizmoHoverAxis != DragMode::none ? juce::MouseCursor::DraggingHandCursor
                                                        : juce::MouseCursor::NormalCursor));
}

void Spectrogram3DComponent::handleMouseExit()
{
    if (dragMode == DragMode::gizmoX || dragMode == DragMode::gizmoY || dragMode == DragMode::gizmoZ)
        return;
    if (gizmoHoverAxis == DragMode::none)
        return;
    gizmoHoverAxis = DragMode::none;
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::dragDebugGizmo (juce::Point<float> localPos) noexcept
{
    if (glHost == nullptr)
        return;
    const auto view = getGlViewLocal().toFloat();
    if (view.getWidth() < 2.0f || view.getHeight() < 2.0f)
        return;

    // Approximate world units per pixel at the sphere depth.
    const float worldPerPx = juce::jmax (0.0005f, camera.distance * 0.0018f);
    const float dx = (localPos.x - gizmoDragStartPos.x) * worldPerPx;
    const float dy = (gizmoDragStartPos.y - localPos.y) * worldPerPx; // screen Y down

    juce::Vector3D<float> right, up, forward;
    cameraBasis (right, up, forward);
    juce::Vector3D<float> p = gizmoDragStartSpherePos;
    if (dragMode == DragMode::gizmoX)
        p.x += dx * right.x + dy * up.x; // project screen drag onto world X via camera axes then clamp to X
    else if (dragMode == DragMode::gizmoY)
        p.y += dy;
    else if (dragMode == DragMode::gizmoZ)
        p.z += dx * right.z + dy * up.z;

    // Axis-constrain: only move the active component from the camera-space drag.
    if (dragMode == DragMode::gizmoX)
    {
        const float along = dx * right.x + dy * up.x;
        p = gizmoDragStartSpherePos;
        p.x += along;
    }
    else if (dragMode == DragMode::gizmoY)
    {
        p = gizmoDragStartSpherePos;
        p.y += dy;
    }
    else if (dragMode == DragMode::gizmoZ)
    {
        const float along = dx * right.z + dy * up.z;
        p = gizmoDragStartSpherePos;
        p.z += along;
    }

    setDebugSpherePosition (p);
    if (onDebugSphereChanged != nullptr)
        onDebugSphereChanged();
}

void Spectrogram3DComponent::handleMouseDown (const juce::MouseEvent& e)
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();

    lastDrag = e.position;
    rightClickCandidate = false;
    rightClickDragged = false;

    // Controls:
    //  LMB drag           = orbit around look-at (yaw / elevation) - turntable only
    //  RMB hold           = UE freecam: free look + WASD/QE fly (no orbit / ground limits)
    //  RMB click (no drag)= context menu
    //  RMB + wheel        = freecam speed (not zoom)
    //  Shift / MMB        = pan look-at on ground plane (not while freecam)
    //  Alt+LMB            = dolly (distance)
    //  Ctrl/Cmd+LMB       = set DOF focus distance under cursor
    //  Wheel              = zoom / dolly (when not freecam)
    //  LMB on debug gizmo = axis-constrained sphere move
    if (isRightMouse (e))
    {
        dragMode = DragMode::freecamLook;
        freecamRmbHeld = true;
        freecamLastTimeSec = 0.0;
        // Switch to the independent freecam rig (not orbit). Fullscreen shares this.
        if (! freecamActive)
            enterFreecamFromTurntable();
        rightClickCandidate = true;
        rightClickStart = e.position;
    }
    else if (e.mods.isMiddleButtonDown() || e.mods.isShiftDown())
    {
        // Ground-plane pan is turntable framing - not used in freecam.
        dragMode = DragMode::pan;
    }
    else if (e.mods.isLeftButtonDown()
             && (e.mods.isCtrlDown() || e.mods.isCommandDown())
             && ! e.mods.isAltDown())
    {
        pickDofFocusAtLocalPoint (e.position);
        dragMode = DragMode::none;
    }
    else if (e.mods.isLeftButtonDown() && e.mods.isAltDown())
    {
        dragMode = DragMode::dolly;
    }
    else if (e.mods.isLeftButtonDown() && tryPickDebugGizmo (e.position))
    {
        // dragMode set by tryPickDebugGizmo
    }
    else if (e.mods.isLeftButtonDown())
    {
        dragMode = DragMode::orbit;
    }
    else
    {
        dragMode = DragMode::none;
    }
}

void Spectrogram3DComponent::handleMouseDrag (const juce::MouseEvent& e)
{
    // Live button state wins - some hosts drop popup flags mid-drag.
    // RMB freecam look only (never steal LMB orbit because freecamActive is still true).
    if (isRightMouse (e) || freecamRmbHeld)
    {
        dragMode = DragMode::freecamLook;
        freecamRmbHeld = true;
        if (! freecamActive)
            enterFreecamFromTurntable();
    }
    else if (e.mods.isMiddleButtonDown())
    {
        dragMode = DragMode::pan;
    }

    if (dragMode == DragMode::none)
        return;

    if (dragMode == DragMode::gizmoX || dragMode == DragMode::gizmoY || dragMode == DragMode::gizmoZ)
    {
        dragDebugGizmo (e.position);
        lastDrag = e.position;
        return;
    }

    const auto d = e.position - lastDrag;
    lastDrag = e.position;

    if (dragMode == DragMode::orbit)
    {
        // Normal 3D orbit about look-at (LMB) - completely separate from freecam.
        camera.yawDeg -= d.x * 0.35f;
        camera.pitchDeg += d.y * 0.35f;
        clampCamera();
    }
    else if (dragMode == DragMode::pan)
    {
        // Normal 3D ground pan (MMB / Shift) - separate from freecam.
        juce::Vector3D<float> right, up, forward;
        cameraBasis (right, up, forward);
        juce::ignoreUnused (up);
        const float scale = 0.0025f * camera.distance;
        juce::Vector3D<float> fwdXZ { forward.x, 0.0f, forward.z };
        const float fLen = juce::jmax (1.0e-6f, std::sqrt (fwdXZ.x * fwdXZ.x + fwdXZ.z * fwdXZ.z));
        fwdXZ.x /= fLen;
        fwdXZ.z /= fLen;
        camera.panX += (right.x * d.x + fwdXZ.x * (-d.y)) * scale;
        camera.panZ += (right.z * d.x + fwdXZ.z * (-d.y)) * scale;
        clampCamera();
    }
    else if (dragMode == DragMode::freecamLook || dragMode == DragMode::screenPan)
    {
        // Click-without-drag = context menu; once past threshold, freecam look only.
        if (rightClickCandidate && ! rightClickDragged)
        {
            if (e.position.getDistanceFrom (rightClickStart) <= 3.0f)
                return;
            rightClickDragged = true;
        }

        // freecamEye fixed; only freecamYaw/Pitch change. Orbit camera.* untouched.
        applyFreecamLookDelta (d.x, d.y);
    }
    else if (dragMode == DragMode::dolly)
    {
        // Normal 3D dolly (Alt+LMB) - separate from freecam speed wheel.
        camera.distance *= (1.0f + d.y * 0.005f);
        clampCamera();
        if (zoomOscillateEnabled)
            zoomOscillateBaseDistance = camera.distance;
    }

    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::handleMouseUp (const juce::MouseEvent& e)
{
    // RMB click (no meaningful drag): full view menu (includes Freeze).
    if (rightClickCandidate && ! rightClickDragged)
    {
        // Context menu: leave freecam immediately so orbit state is consistent.
        if (freecamActive)
            exitFreecamToTurntable();
        showContextMenu (e.getScreenPosition());
    }

    rightClickCandidate = false;
    rightClickDragged = false;
    freecamRmbHeld = false;
    // Keep freecamActive while velocity coasts; tickFreecam exits when stopped.
    if (freecamActive
        && flyVel.x * flyVel.x + flyVel.y * flyVel.y + flyVel.z * flyVel.z < 1.0e-8f)
        exitFreecamToTurntable();
    freecamLastTimeSec = 0.0;
    dragMode = DragMode::none;
}

void Spectrogram3DComponent::applyUiZoomDrag (float deltaY) noexcept
{
    // Vertical drag only - same distance zoom as the scroll wheel (not orbit/pan).
    // Drag down -> zoom out; drag up -> zoom in.
    if (zoomOscillateEnabled)
    {
        zoomOscillateBaseDistance *= (1.0f + deltaY * 0.012f);
        zoomOscillateBaseDistance = juce::jlimit (0.35f, 14.0f, zoomOscillateBaseDistance);
        applyZoomOscillateDistance();
    }
    else
    {
        camera.distance *= (1.0f + deltaY * 0.012f);
        clampCamera();
    }

    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::handleMouseWheel (const juce::MouseWheelDetails& wheel)
{
    // Freecam: wheel is fly speed only (not orbit dolly).
    if (freecamActive || freecamRmbHeld || dragMode == DragMode::freecamLook)
    {
        nudgeFreecamSpeedFromWheel (wheel.deltaY);
        return;
    }

    if (zoomOscillateEnabled)
    {
        zoomOscillateBaseDistance *= (1.0f - wheel.deltaY * 0.15f);
        zoomOscillateBaseDistance = juce::jlimit (0.35f, 14.0f, zoomOscillateBaseDistance);
        applyZoomOscillateDistance();
    }
    else
    {
        camera.distance *= (1.0f - wheel.deltaY * 0.15f);
        clampCamera();
    }
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::handleDoubleClick()
{
    if (onDoubleClick != nullptr)
        onDoubleClick();
    else
        resetCamera();
}
