#include "Spectrogram3DComponent.h"
#include "SpectrogramComponent.h"
#include "Menu/SharedResources.h"
#include "ComboBoxLookAndFeel.h"
#include "Assets/VeniceSunsetHdri.h"
#include <cmath>
#include <cstring>

namespace
{
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
        void main()
        {
            vColour = colour;
            vNormal = mat3 (viewMatrix) * normal;
            vWorldNormal = normal;
            vec4 viewPos = viewMatrix * vec4 (position, 1.0);
            vViewDir = normalize (-viewPos.xyz);
            vWorldPos = position;
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
        uniform float uFreqBiasB; // w(u)=1+B*u^2 HF density; 0 = uniform
        uniform float uAoAmount;
        uniform float uAoRadius;
        // CPU-prepared light bearing on the floor (avoids GPU normalize edge cases).
        uniform vec2 uShadowDirXZ;
        uniform float uShadowSunTan;
        uniform float uShadowBias;
        uniform float uShadowSoftness;
        uniform float uShadowQuality; // 0=low, 1=medium, 2=high
        uniform float uContactShadow;
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
        // Audio-level mod matrix (level 0 = silence). Target: 0 bright, 1 lit amt,
        // 2 spec, 3 rim, 4 dome, 5 all lights, 6 bright+lights.
        // factor = mix(1+min, 1+max, level); min/max are fractional (±0.20 = ±20%).
        uniform float uAudioLevel;
        uniform float uAudioMin;
        uniform float uAudioMax;
        uniform float uAudioTarget;
        uniform float uAudioAffectPlayhead;
        uniform float uAudioAffectAnti;
        uniform float uPlayheadWallX;
        uniform float uAntiPlayheadWallX;

        // Frequency axis u (0=low…1=high) → height-map V (mesh-row CDF).
        float meshTFromFreqAxis (float uAxis)
        {
            float u = clamp (uAxis, 0.0, 1.0);
            float B = max (uFreqBiasB, 0.0);
            if (B < 1.0e-5)
                return u;
            return (u + B * u * u * u / 3.0) / (1.0 + B / 3.0);
        }

        float sampleHeightNorm (vec2 xz)
        {
            float texU = clamp (xz.x * 0.5 + 0.5, 0.001, 0.999);
            // World Z → frequency axis u (0=low, 1=high), matching CPU mesh build.
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
            Heightfield self-shadow — horizon + IQ soft ray-march.
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
                // Soft contact — no hard lit=0 cliff at the terminator.
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

        /** Closed solid: optical depth top→base + ridge taps. */
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

            vec3 albedo = vColour;
            float shadow = heightfieldSelfShadow (vWorldPos);
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
        R"(            // Self/contact shadows are key-light effects — when lighting is off they are
            // forced to 1.0 via uniforms; AO may still apply (ambient, own toggle).
            float shade = shadow * ao * contact;

            // Audio-level mod matrix: body always; closed playhead / anti-playhead opt-in.
            // factor = mix(1+min%, 1+max%, level); both % at 0 → factor 1 (no change).
            float audioT = 0.0;
            {
                float lvl = clamp (uAudioLevel, 0.0, 1.0);
                float band = 0.05;
                float nearPh = 1.0 - smoothstep (0.0, band, abs (vWorldPos.x - uPlayheadWallX));
                float nearAnti = 1.0 - smoothstep (0.0, band, abs (vWorldPos.x - uAntiPlayheadWallX));
                float onWall = max (nearPh, nearAnti);
                float m = 1.0 - onWall;
                if (uAudioAffectPlayhead > 0.5)
                    m = max (m, nearPh);
                if (uAudioAffectAnti > 0.5)
                    m = max (m, nearAnti);
                audioT = lvl * m;
            }
            float factor = mix (1.0 + uAudioMin, 1.0 + uAudioMax, audioT);
            int tgt = int (uAudioTarget + 0.5);
            bool wantBright = (tgt == 0 || tgt == 6);
            bool wantLitAmt = (tgt == 1 || tgt == 5 || tgt == 6);
            bool wantSpec = (tgt == 2 || tgt == 5 || tgt == 6);
            bool wantRimT = (tgt == 3 || tgt == 5 || tgt == 6);
            bool wantDomeT = (tgt == 4 || tgt == 5 || tgt == 6);
            if (wantBright)
                albedo *= factor;

            float amt = clamp (uLightingAmount, 0.0, 1.0);
            if (wantLitAmt)
                amt = clamp (amt * factor, 0.0, 1.0);

            if (amt < 1.0e-4)
            {
                fragColour = vec4 (albedo * ao, 1.0); // flat ramp; AO only if enabled
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

            float rough = clamp (uRoughness, 0.04, 1.0);
            float metal = clamp (uMetalness, 0.0, 1.0);
            float specAmt = clamp (uSpecular, 0.0, 1.0);
            if (wantSpec)
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

            // Soft Lambert wrap so the lighting terminator isn't a hard edge either.
            float wrap = NdotL * 0.72 + 0.28;
            vec3 kd = albedo * (1.0 - metal);
            // Opt-in energy split — off preserves the legacy Look.
            if (uEnergyConserve > 0.5)
                kd *= max (vec3 (0.0), vec3 (1.0) - F);
            vec3 diffuse = kd * (0.22 * ao + 0.78 * wrap * shadow) * lightCol;

            // Dome / hemisphere fill — sky vs ground, or equirectangular HDRI.
            float domeAmt = clamp (uDomeStrength, 0.0, 1.0);
            if (wantDomeT)
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

            float rimAmt = uRim;
            if (wantRimT)
                rimAmt = clamp (rimAmt * factor, 0.0, 2.0);
            float rim = pow (1.0 - NdotV, 2.5) * rimAmt * ao;
            vec3 sss = subsurfaceScatter (albedo, n, l, v, shadow);
            vec3 lit = diffuse + specular + albedo * rim * rimCol + sss;
            fragColour = vec4 (mix (albedo * shade, lit, amt), 1.0);
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
            // Soft elliptical contact stain — visible over Soft BG / EQ plate.
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
    //        5 = bloom composite, 6 = DOF disc gather,
    //        8 = SSGI gather+composite (legacy), 9 = SSGI gather GI-only,
    //        10 = SSGI bilateral denoise (Simple), 11 = SSGI temporal (Simple),
    //        12 = SSGI composite, 13 = tonemap/grade, 14 = SSGI bilateral upsample,
    //        15 = SVGF temporal + variance clamp (Modern), 16 = à-trous (Modern),
    //        18 = luminance moments update (Modern).
    // Vendor denoisers (NVIDIA NRD/OptiX, Intel OIDN) are intentionally not used:
    // they need D3D11/12, Vulkan, and/or CUDA/SYCL — incompatible with this JUCE
    // OpenGL soft FBO→Image Spec3D path without a full API rewrite.
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

        // Thin-lens CoC in pixels. uRadius = focus (view Z), uStrength = aperture 0–1,
        // uThreshold = quality (0/1/2 → base max blur px + sample count),
        // uParam = blur scale (DoF mode 6).
        float circleOfConfusionPx (float depth01)
        {
            float focus = max (uRadius, 0.05);
            float aperture = clamp (uStrength, 0.0, 1.0);
            float viewZ = linearViewZ (depth01);
            // Relative CoC — zero on the focus plane; gentle so aperture 0.01 stays sharp.
            float rel = abs (viewZ - focus) / max (min (viewZ, focus), 0.05);
            float baseBlur = (uThreshold < 0.5) ? 6.0
                           : ((uThreshold < 1.5) ? 10.0 : 14.0);
            float maxBlur = baseBlur * clamp (uParam, 0.25, 3.0);
            return clamp (rel * aperture * maxBlur, 0.0, maxBlur);
        }

        // Vogel disc (golden-angle) — even circular bokeh taps.
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
                // Gather DOF — disc samples weighted by neighbour CoC (avoids separable ghosting).
                // Soft BG composites with alpha, so RGB-only blur left hard coverage silhouettes
                // (especially when the mesh is small / distant on screen). Premultiply + blur A.
                float centerDepth = depthSample (vUv);
                float centerCoc = circleOfConfusionPx (centerDepth);
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));

                // uInvProj[0].xy = (cocDilate, edgeSpill) from CPU tune knobs.
                float dilateAmt = clamp (uInvProj[0][0], 0.0, 1.0);
                float spillAmt = clamp (uInvProj[0][1], 0.0, 1.0);

                // Dilate CoC from neighbours so silhouette pixels (sky next to mesh, or
                // MSAA-thin edges) still gather — otherwise far shots harden to a cutout.
                float dilateCoc = centerCoc;
                dilateCoc = max (dilateCoc, circleOfConfusionPx (depthSample (
                    clamp (vUv + vec2 (texel.x, 0.0), vec2 (0.0), vec2 (1.0)))));
                dilateCoc = max (dilateCoc, circleOfConfusionPx (depthSample (
                    clamp (vUv + vec2 (-texel.x, 0.0), vec2 (0.0), vec2 (1.0)))));
                dilateCoc = max (dilateCoc, circleOfConfusionPx (depthSample (
                    clamp (vUv + vec2 (0.0, texel.y), vec2 (0.0), vec2 (1.0)))));
                dilateCoc = max (dilateCoc, circleOfConfusionPx (depthSample (
                    clamp (vUv + vec2 (0.0, -texel.y), vec2 (0.0), vec2 (1.0)))));
                float gatherCoc = max (centerCoc, mix (centerCoc, dilateCoc, dilateAmt));

                if (gatherCoc < 0.4)
                {
                    fragColour = src;
                    return;
                }

                int nSamples = (uThreshold < 0.5) ? 8
                             : ((uThreshold < 1.5) ? 16 : 24);
                // Premultiplied accumulation so coverage softens with colour.
                vec4 acc = vec4 (src.rgb * src.a, src.a);
                float wSum = 1.0;
                bool centerSky = (centerDepth >= 0.9995);
                float spillFloor = mix (0.05, 0.95, spillAmt);

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

                    // Down-weight sharper neighbours so in-focus edges don't bleed into bokeh.
                    // Exception: sky/background centres must accept blurry mesh taps so
                    // out-of-focus silhouettes expand outward (not a hard cutout).
                    float w = 1.0;
                    if (! centerSky && sampleCoc + 0.5 < gatherCoc)
                        w = clamp (sampleCoc / max (gatherCoc, 1.0e-3), 0.05, 1.0);
                    if (centerSky && ! sampleSky)
                        w = max (w, clamp (sampleCoc / max (gatherCoc, 1.0e-3), spillFloor, 1.0));

                    acc += vec4 (s.rgb * s.a, s.a) * w;
                    wSum += w;
                }

                acc /= max (wSum, 1.0e-3);
                float outA = clamp (acc.a, 0.0, 1.0);
                vec3 outRgb = (outA > 1.0e-4) ? acc.rgb / outA : acc.rgb;
                // Ease in so tiny apertures don't hard-switch to a full disc.
                float t = smoothstep (0.4, 1.75, gatherCoc);
                fragColour = vec4 (mix (src.rgb, outRgb, t), mix (src.a, outA, t));
                return;
            }
            // Shared SSGI gather. uParam.x via uParam: frame rotation radians (0 = fixed).
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

                // Quality: 0=Low 6×4, 1=Med 10×6, 2=High 14×8, 3=Ultra 20×12
                int nSamples = (uThreshold < 0.5) ? 6
                             : ((uThreshold < 1.5) ? 10
                             : ((uThreshold < 2.5) ? 14 : 20));
                int nSteps = (uThreshold < 0.5) ? 4
                           : ((uThreshold < 1.5) ? 6
                           : ((uThreshold < 2.5) ? 8 : 12));
                float maxDist = mix (0.08, 0.55, clamp (uRadius, 0.0, 1.0));
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

                    for (int s = 1; s <= 12; ++s)
                    {
                        if (s > nSteps)
                            break;
                        vec3 p = viewPos + dir * (stepLen * float (s));
                        float z = max (-p.z, 1.0e-3);
                        vec2 uv2 = vec2 (p.x / (tanHalfW * z), p.y / (tanHalfH * z)) * 0.5 + 0.5;
                        if (uv2.x < 0.0 || uv2.x > 1.0 || uv2.y < 0.0 || uv2.y > 1.0)
                            break;

                        float sceneZ = linearViewZ (depthSample (uv2));
                        float thick = mix (0.04, 0.18, clamp (uRadius, 0.0, 1.0));
                        if (sceneZ < z - 0.008 && sceneZ > z - thick)
                        {
                            vec3 rad = texture (uTex, uv2).rgb;
                            float nd = max (dot (nrm, dir), 0.0);
                            float atten = 1.0 - float (s) / float (nSteps);
                            gi += rad * nd * atten;
                            wSum += nd * atten;
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
                // Bilateral denoise on GI buffer (uTex). uStrength = amount 0–1.
                float centerD = depthSample (vUv);
                vec3 center = src.rgb;
                if (centerD > 0.999 || uStrength < 1.0e-4)
                {
                    fragColour = src;
                    return;
                }
                vec2 texel = 1.0 / max (uResolution, vec2 (1.0));
                float rad = mix (1.0, 3.5, clamp (uStrength, 0.0, 1.0));
                vec3 acc = center;
                float wSum = 1.0;
                for (int y = -2; y <= 2; ++y)
                for (int x = -2; x <= 2; ++x)
                {
                    if (x == 0 && y == 0) continue;
                    vec2 uv2 = clamp (vUv + vec2 (float (x), float (y)) * texel * rad, vec2 (0.0), vec2 (1.0));
                    float d2 = depthSample (uv2);
                    float dw = exp (-abs (centerD - d2) * 80.0);
                    float spat = exp (-0.35 * float (x * x + y * y));
                    float w = dw * spat;
                    acc += texture (uTex, uv2).rgb * w;
                    wSum += w;
                }
                vec3 blurred = acc / max (wSum, 1.0e-4);
                fragColour = vec4 (mix (center, blurred, clamp (uStrength, 0.0, 1.0)), src.a);
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
                // uInvProj[0][0] > 0.5 → use guide normals from uAux.
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

            // SSAO — depth-delta taps (stable without perfect inv-projection).
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
        out vec2 vTex;
        void main()
        {
            vTex = texCoord;
            // NDC xyz — z from the projected world anchor so the mesh can occlude labels.
            gl_Position = vec4 (position, 1.0);
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
    pf.multisamplingLevel = (uint8_t) juce::jlimit (0, 16, samples);
    openGLContext.setPixelFormat (pf);
}

void Spectrogram3DComponent::GlHost::setActive (bool shouldBeActive) noexcept
{
    setVisible (shouldBeActive);
    if (shouldBeActive)
        requestAttachAsync();
}

void Spectrogram3DComponent::GlHost::requestAttachAsync()
{
    if (openGLContext.isAttached() || attachPending)
        return;

    attachPending = true;
    juce::Component::SafePointer<GlHost> safe (this);
    juce::MessageManager::callAsync ([safe]
    {
        if (safe == nullptr)
            return;
        safe->attachPending = false;
        if (! safe->isVisible())
            return;
        safe->attachNow();
    });
}

void Spectrogram3DComponent::GlHost::attachNow()
{
    if (! isVisible() || openGLContext.isAttached())
        return;

    if (getPeer() == nullptr || getWidth() < 2 || getHeight() < 2)
    {
        requestAttachAsync();
        return;
    }

    applyPixelFormat();
    openGLContext.attachTo (*this);
    openGLContext.triggerRepaint();
}

void Spectrogram3DComponent::GlHost::reattachWithCurrentFormat()
{
    applyPixelFormat();
    if (openGLContext.isAttached())
        openGLContext.detach();
    if (isVisible())
        requestAttachAsync();
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
    colourAudioTargetUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioTarget");
    colourAudioAffectPlayheadUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioAffectPlayhead");
    colourAudioAffectAntiUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAudioAffectAnti");
    colourPlayheadWallXUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uPlayheadWallX");
    colourAntiPlayheadWallXUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uAntiPlayheadWallX");

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
    }

    contextFailed = (colourShader == nullptr);
}

void Spectrogram3DComponent::GlHost::destroyShaders()
{
    colourContactUniform.reset();
    colourShadowQualityUniform.reset();
    colourShadowSoftnessUniform.reset();
    colourShadowBiasUniform.reset();
    colourShadowSunTanUniform.reset();
    colourShadowDirXZUniform.reset();
    colourAoRadiusUniform.reset();
    colourAoAmountUniform.reset();
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

    labelClearUniform.reset();
    labelCornerUniform.reset();
    labelResolutionUniform.reset();
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
    destroyShaders();
    glReady = false;
    meshIndexCount = 0;
    floorVertexCount = 0;
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
    // Must refresh every upload — a stale height map makes shadows/AO/SSS scroll vs the waterfall.
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

    if (floorVbo != 0 && sr == floorGridSr && logFreq == floorGridLog && soft == floorGridSoftBg
        && reverse == floorGridReverseFreq)
        return;

    floorGridSr = sr;
    floorGridLog = logFreq;
    floorGridSoftBg = soft;
    floorGridReverseFreq = reverse;
    owner.rebuildFreqLabels (sr, logFreq);
    rebuildFloorGeometry();
    labelAtlasReady = false;
}

void Spectrogram3DComponent::GlHost::rebuildFloorGeometry()
{
    constexpr float groundY = -0.012f;
    constexpr float gridY = -0.010f;
    std::vector<Vertex> lines;
    lines.reserve (160);

    auto pushLine = [&lines] (float x0, float y0, float z0, float x1, float y1, float z1, float r, float g, float b)
    {
        lines.push_back ({ x0, y0, z0, r, g, b, 0.0f, 1.0f, 0.0f });
        lines.push_back ({ x1, y1, z1, r, g, b, 0.0f, 1.0f, 0.0f });
    };

    // Opaque ground plane (two triangles). Skipped for soft composite so EQ shows through.
    if (! owner.usesSoftComposite())
    {
        const float gr = 0.06f, gg = 0.07f, gb = 0.09f;
        auto pushGround = [&lines, gr, gg, gb] (float x, float y, float z)
        {
            lines.push_back ({ x, y, z, gr, gg, gb, 0.0f, 1.0f, 0.0f });
        };
        pushGround (-1.0f, groundY, -1.0f);
        pushGround ( 1.0f, groundY, -1.0f);
        pushGround ( 1.0f, groundY,  1.0f);
        pushGround (-1.0f, groundY, -1.0f);
        pushGround ( 1.0f, groundY,  1.0f);
        pushGround (-1.0f, groundY,  1.0f);
    }

    const float nyquist = (float) (floorGridSr * 0.5);
    const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);

    for (float hz : kMinorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float z = owner.worldZForFreq (hz, floorGridSr, floorGridLog);
        pushLine (-1.0f, gridY, z, 1.0f, gridY, z, 0.28f, 0.30f, 0.34f);
    }
    for (float hz : kMajorHz)
    {
        if (hz < SpectrogramComponent::kMinDisplayHz || hz > maxHz)
            continue;
        const float z = owner.worldZForFreq (hz, floorGridSr, floorGridLog);
        pushLine (-1.0f, gridY, z, 1.0f, gridY, z, 0.62f, 0.65f, 0.70f);
        // Playhead tick
        pushLine (1.0f, gridY, z, 1.0f, 0.07f, z, 0.85f, 0.85f, 0.88f);
    }

    constexpr int timeDiv = 8;
    for (int i = 0; i <= timeDiv; ++i)
    {
        const float x = (float) i / (float) timeDiv * 2.0f - 1.0f;
        const float a = (i == timeDiv) ? 0.75f : 0.32f;
        pushLine (x, gridY, -1.0f, x, gridY, 1.0f, a, a, a + 0.02f);
    }

    pushLine (-1.0f, gridY, -1.0f, 1.0f, gridY, -1.0f, 0.7f, 0.72f, 0.75f);
    pushLine (1.0f, gridY, -1.0f, 1.0f, gridY, 1.0f, 0.7f, 0.72f, 0.75f);
    pushLine (1.0f, gridY, 1.0f, -1.0f, gridY, 1.0f, 0.7f, 0.72f, 0.75f);
    pushLine (-1.0f, gridY, 1.0f, -1.0f, gridY, -1.0f, 0.7f, 0.72f, 0.75f);

    floorVertexCount = (int) lines.size();
    if (floorVbo == 0)
        juce::gl::glGenBuffers (1, &floorVbo);

    juce::gl::glBindBuffer (juce::gl::GL_ARRAY_BUFFER, floorVbo);
    juce::gl::glBufferData (juce::gl::GL_ARRAY_BUFFER,
                            (GLsizeiptr) (lines.size() * sizeof (Vertex)),
                            lines.data(), juce::gl::GL_DYNAMIC_DRAW);
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
    // Soft / docked-FBO: render at the framed view size (peer is a tiny context keeper).
    // Hard HWND: match this component's pixel size.
    const auto logical = owner.usesSoftComposite() ? owner.getGlViewLocal()
                                                   : getLocalBounds();
    const float scale = (float) openGLContext.getRenderingScale();
    int w = juce::jmax (1, juce::roundToInt ((float) logical.getWidth() * scale));
    int h = juce::jmax (1, juce::roundToInt ((float) logical.getHeight() * scale));

    // Cap soft-mode readback cost.
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
    return owner.getTurntableViewMatrix();
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

    // RGBA8 — universally sampleable on host GL drivers (VST wrappers often choke on R32F).
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

    std::vector<juce::PixelARGB> pixels ((size_t) width * (size_t) height);
    if (! softFbo.readPixels (pixels.data(),
                              { 0, 0, width, height },
                              juce::OpenGLFrameBuffer::RowOrder::fromTopDown))
        return;

    juce::Image img (juce::Image::ARGB, width, height, true, juce::SoftwareImageType());
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < height; ++y)
            std::memcpy (bd.getLinePointer (y),
                         pixels.data() + (size_t) y * (size_t) width,
                         (size_t) width * sizeof (juce::PixelARGB));
    }

    {
        const juce::ScopedLock sl (owner.softImageLock);
        owner.softCompositeImage = std::move (img);
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

    const int samples = effectiveMsaaSamples();
    const bool useMsaa = samples >= 2;
    if (useMsaa)
        ensureSoftMsaaBuffers (w, h, samples);

    if (useMsaa && softMsaaFbo != 0)
    {
        glBindFramebuffer (GL_FRAMEBUFFER, softMsaaFbo);
        glViewport (0, 0, w, h);
        const auto clear = owner.getClearColour();
        glClearColor (clear.getFloatRed(), clear.getFloatGreen(), clear.getFloatBlue(), clear.getFloatAlpha());
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable (GL_MULTISAMPLE);
    }
    else
    {
        softFbo.makeCurrentAndClear();
        glViewport (0, 0, w, h);
        glClear (GL_DEPTH_BUFFER_BIT);
    }

    uploadDomeTextureIfNeeded();
    drawSoftTint();
    drawGroundAndGrid();
    drawContactShadow();
    drawMesh();
    drawFrequencyLabels();

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

    if (owner.needsPostEffects())
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

    // Shadows follow the key light — disabled when Lighting is off.
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
    setF1 (colourAudioTargetUniform.get(), "uAudioTarget",
           (float) static_cast<int> (owner.audioLevelTarget));
    setF1 (colourAudioAffectPlayheadUniform.get(), "uAudioAffectPlayhead",
           owner.audioLevelAffectPlayhead ? 1.0f : 0.0f);
    setF1 (colourAudioAffectAntiUniform.get(), "uAudioAffectAnti",
           owner.audioLevelAffectAntiPlayhead ? 1.0f : 0.0f);
    setF1 (colourPlayheadWallXUniform.get(), "uPlayheadWallX",
           1.0f + kClosedPlayheadWallBias);
    setF1 (colourAntiPlayheadWallXUniform.get(), "uAntiPlayheadWallX",
           -1.0f - kClosedWaterfallEndWallBias);
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

    const int groundVerts = owner.usesSoftComposite() ? 0 : 6;
    if (groundVerts > 0)
        glDrawArrays (GL_TRIANGLES, 0, groundVerts);
    if (floorVertexCount > groundVerts)
        glDrawArrays (GL_LINES, groundVerts, floorVertexCount - groundVerts);

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
    if (normalsShader == nullptr || ! ssgiNormalsFbo.isValid() || meshIndexCount <= 0
        || meshVbo == 0 || meshIbo == 0)
        return;

    ssgiNormalsFbo.makeCurrentAndClear();
    glViewport (0, 0, width, height);
    glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
    glClear (GL_COLOR_BUFFER_BIT);
    // No shared depth attachment (softDepthTex is already on softFbo). Empty clear
    // leaves sky as zero so SSGI falls back to depth derivatives there.
    glDisable (GL_DEPTH_TEST);
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
    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);
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
    glDrawElements (GL_TRIANGLES, meshIndexCount, GL_UNSIGNED_INT, nullptr);
    if (normalsPositionAttrib != nullptr && normalsPositionAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) normalsPositionAttrib->attributeID);
    if (normalsNormalAttrib != nullptr && normalsNormalAttrib->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) normalsNormalAttrib->attributeID);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

    glDisable (GL_CULL_FACE);
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
            // Mode 6 (DoF): column0 = (cocDilate, edgeSpill, …).
            // SSGI gather: [0][0] = mesh-normals flag.
            float id[16] = {
                0, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
            };
            if (mode == 6)
            {
                id[0] = owner.dofCocDilate;
                id[1] = owner.dofEdgeSpill;
            }
            else
            {
                id[0] = useMeshNormalsFlag ? 1.0f : 0.0f;
            }
            postInvProjUniform->setMatrix4 (id, 1, false);
        }

        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, colourTex);
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
    // Post order: SSGI → bloom → DOF → tonemap.
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
            // Default single-pass path — Strength / Radius / Quality only.
            drawFs (8, &postFboA, sceneTex, softDepthTex, 0,
                    owner.ssgiStrength, owner.ssgiRadius, qualityF, 0.0f,
                    width, height, false);
            drawFs (0, nullptr, (GLuint) postFboA.getTextureID(), softDepthTex, 0,
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
                // Modern: temporal+moments (optional) → à-trous passes → composite.
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

    if (owner.dofEnabled && owner.dofAperture > 1.0e-4f)
    {
        const float qualityF = owner.dofQuality == ShadowQuality::low ? 0.0f
                             : (owner.dofQuality == ShadowQuality::high ? 2.0f : 1.0f);
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();
        drawFs (6, &postFboA, sceneTex, softDepthTex, 0,
                owner.dofAperture, owner.dofFocusDistance, qualityF, owner.dofBlurScale,
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

    struct V { float x, y, z, u, v; };
    std::vector<V> quads;
    quads.reserve (owner.freqLabels.size() * 6);

    const auto view = owner.getGlViewLocal();
    const float pxW = 52.0f / (float) juce::jmax (1, view.getWidth());
    const float pxH = 22.0f / (float) juce::jmax (1, view.getHeight());
    const float halfW = pxW;
    const float halfH = pxH;

    for (const auto& lb : owner.freqLabels)
    {
        // Ground plane, just past the playhead edge (x=1) — tight to the grid ticks.
        constexpr float kLabelWorldX = 1.008f;
        constexpr float kLabelWorldY = -0.006f; // sit on / slightly above grid
        float ndcX = 0.0f, ndcY = 0.0f, ndcZ = 0.0f;
        if (! projectWorldToNdc (kLabelWorldX, kLabelWorldY, lb.worldZ, ndcX, ndcY, ndcZ))
            continue;

        const float x0 = ndcX + 0.0015f;
        const float x1 = x0 + halfW * 2.0f;
        const float y0 = ndcY - halfH * 0.85f;
        const float y1 = ndcY + halfH * 0.85f;

        quads.push_back ({ x0, y0, ndcZ, lb.u0, lb.v0 });
        quads.push_back ({ x1, y0, ndcZ, lb.u1, lb.v0 });
        quads.push_back ({ x1, y1, ndcZ, lb.u1, lb.v1 });
        quads.push_back ({ x0, y0, ndcZ, lb.u0, lb.v0 });
        quads.push_back ({ x1, y1, ndcZ, lb.u1, lb.v1 });
        quads.push_back ({ x0, y1, ndcZ, lb.u0, lb.v1 });
    }

    if (quads.empty())
        return;

    glBindBuffer (GL_ARRAY_BUFFER, labelVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (quads.size() * sizeof (V)), quads.data(), GL_DYNAMIC_DRAW);

    labelShader->use();
    setCornerUniforms (*labelShader);
    if (labelTexUniform != nullptr)
        labelTexUniform->set (0);
    labelAtlas.bind();
    glActiveTexture (GL_TEXTURE0);

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_FALSE);
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

    drawGroundAndGrid();
    drawContactShadow();
    drawMesh();
    drawFrequencyLabels();
}

void Spectrogram3DComponent::GlHost::mouseDown (const juce::MouseEvent& e)
{
    if (! hasKeyboardFocus (true))
        grabKeyboardFocus();
    owner.handleMouseDown (e.getEventRelativeTo (&owner));
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

//==============================================================================
namespace
{
    /** Translucent grip for Soft BG / Direct2D — default corner resizer paints as a black box. */
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

    defaultCamera = getFactoryCameraState();
    camera = defaultCamera;
    applyChromeMode();
}

Spectrogram3DComponent::~Spectrogram3DComponent()
{
    stopTimer();
    resizer.reset();
    hitLayer.reset();
    glHost.reset();
}

Spectrogram3DComponent::CameraState Spectrogram3DComponent::getFactoryCameraState() noexcept
{
    // ¾ view from above: pitch = elevation above the floor horizon (not a tilted orbit axis).
    // Orbit pivot = centre of the mesh volume; distance places the eye ~3× height above peaks.
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
    // Rescale existing history in place — do not wipe meshDb.
    if (meshW >= 2 && meshH >= 2 && ! meshDb.empty() && lastBrightness >= 0.0f)
        rebuildVerticesFromMeshDb (lastBrightness, lastMinDb, lastMaxDb);
    markLookDirty();
}

void Spectrogram3DComponent::setActive (bool shouldBeActive) noexcept
{
    const bool changed = (active != shouldBeActive);
    active = shouldBeActive;
    setAlwaysOnTop (false);
    // Always re-apply visibility — Scope layout clears it before place, and an
    // early-return here used to leave the docked 3D pane permanently hidden.
    setVisible (active);

    if (! changed)
        return; // Look / prefs sync must not wipe or re-seed history.

    applyChromeMode();
    layoutPresentation();

    if (active)
    {
        clampCamera();
        markSoftContentDirty();
        startTimerHz (30);
        updateMeshFromSource();
    }
    else
    {
        stopTimer();
    }

    repaint();
}

void Spectrogram3DComponent::setMeshQuality (MeshQuality q) noexcept
{
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

void Spectrogram3DComponent::setDofAperture (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (dofAperture - amount01) < 1.0e-4f) return;
    dofAperture = amount01;
    markLookDirty();
}

void Spectrogram3DComponent::setDofQuality (ShadowQuality q) noexcept
{
    if (dofQuality == q) return;
    dofQuality = q;
    markLookDirty();
}

void Spectrogram3DComponent::setDofBlurScale (float scale) noexcept
{
    scale = juce::jlimit (0.25f, 3.0f, scale);
    if (std::abs (dofBlurScale - scale) < 1.0e-4f) return;
    dofBlurScale = scale;
    markLookDirty();
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
    // Nested GL HWNDs often stay black under Direct2D. Soft FBO→Image is reliable for
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
    // Soft mode paints via Image compositing; HWND stays opaque for the tiny peer.
    setOpaque (true);
    setInterceptsMouseClicks (! owner.usesSoftComposite(), true);
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
        // Keep it a bit larger than 2×2 — some hosts fail to attach tiny contexts.
        constexpr int kPeer = 16;
        const int x = juce::jmax (0, getWidth() - kPeer);
        const int y = juce::jmax (0, getHeight() - kPeer);
        glHost->setBounds (x, y, kPeer, kPeer);
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
    markSoftContentDirty();
    if (glHost != nullptr)
        glHost->triggerRedraw();
    if (usesSoftComposite())
        repaint();
}

void Spectrogram3DComponent::setAudioLevelTarget (AudioLevelTarget target) noexcept
{
    if (audioLevelTarget == target)
        return;
    audioLevelTarget = target;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setAudioLevelMinPercent (float pct) noexcept
{
    pct = juce::jlimit (kAudioLevelPercentMin, kAudioLevelPercentMax, pct);
    if (pct > audioLevelMaxPercent)
        pct = audioLevelMaxPercent;
    if (std::abs (audioLevelMinPercent - pct) < 1.0e-3f)
        return;
    audioLevelMinPercent = pct;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setAudioLevelMaxPercent (float pct) noexcept
{
    pct = juce::jlimit (kAudioLevelPercentMin, kAudioLevelPercentMax, pct);
    if (pct < audioLevelMinPercent)
        pct = audioLevelMinPercent;
    if (std::abs (audioLevelMaxPercent - pct) < 1.0e-3f)
        return;
    audioLevelMaxPercent = pct;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setAudioLevelHpHz (float hz) noexcept
{
    audioLevelHpHz = juce::jlimit (20.0f, 18000.0f, hz);
}

void Spectrogram3DComponent::setAudioLevelLpHz (float hz) noexcept
{
    audioLevelLpHz = juce::jlimit (40.0f, 20000.0f, hz);
}

void Spectrogram3DComponent::setAudioLevelAffectPlayhead (bool shouldAffect) noexcept
{
    if (audioLevelAffectPlayhead == shouldAffect)
        return;
    audioLevelAffectPlayhead = shouldAffect;
    markSoftContentDirty();
}

void Spectrogram3DComponent::setAudioLevelAffectAntiPlayhead (bool shouldAffect) noexcept
{
    if (audioLevelAffectAntiPlayhead == shouldAffect)
        return;
    audioLevelAffectAntiPlayhead = shouldAffect;
    markSoftContentDirty();
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
        // Cheap angle proxy (avoids acos in the hot path). Larger corners → larger weight.
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
                visitTri (i0, i2, i1, accumulate);
                visitTri (i1, i2, i3, accumulate);
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
    // Pitch = elevation above the floor horizon. 0° ≈ edge-on, 90° = top-down.
    // Never allow negative elevation (that would put the camera under the floor).
    camera.pitchDeg = juce::jlimit (kMinPitchDeg, kMaxPitchDeg, camera.pitchDeg);
    camera.distance = juce::jlimit (0.35f, 14.0f, camera.distance);
    // Keep the orbit pivot over the mesh footprint (with a little slack for framing).
    camera.panX = juce::jlimit (-1.6f, 1.6f, camera.panX);
    camera.panZ = juce::jlimit (-1.6f, 1.6f, camera.panZ);
    camera.panY = juce::jlimit (-0.05f, meshHeight * 1.4f, camera.panY);
}

void Spectrogram3DComponent::cameraBasis (juce::Vector3D<float>& outRight,
                                          juce::Vector3D<float>& outUp,
                                          juce::Vector3D<float>& outForward) const noexcept
{
    const float yaw = juce::degreesToRadians (camera.yawDeg);
    const float pitch = juce::degreesToRadians (camera.pitchDeg);
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    const float cy = std::cos (yaw);
    const float sy = std::sin (yaw);

    // Matches getTurntableViewMatrix eye offset: (-sy*cp, sp, cy*cp) * distance.
    outForward = { sy * cp, -sp, -cy * cp };
    // right = normalize(forward × worldUp)
    outRight = { -outForward.z, 0.0f, outForward.x };
    const float rLen = juce::jmax (1.0e-6f, std::sqrt (outRight.x * outRight.x + outRight.z * outRight.z));
    outRight.x /= rLen;
    outRight.z /= rLen;
    // up = right × forward (already unit-ish)
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

juce::Matrix3D<float> Spectrogram3DComponent::getTurntableViewMatrix() const noexcept
{
    // Orbit around the look-at (pan). RMB/MMB move that pivot so tumble always
    // spins around the centre of what you're framing — not a post-view truck.
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

bool Spectrogram3DComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onEscape != nullptr)
            onEscape();
        return true;
    }

    if (key == juce::KeyPress ('f') || key == juce::KeyPress ('F'))
    {
        resetCamera();
        return true;
    }

    return false;
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

        // Soft BG: FBO image already includes the translucent tint — do not pre-fill
        // a second translucent plate (that made the whole view look washed out).
        // Soft BG off (docked Scope): opaque plate under the FBO image.
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
        g.drawFittedText (glHost->hasContextFailed() ? "3D spectrogram unavailable"
                                                     : "Initialising 3D spectrogram...",
                          inner.toNearestInt().reduced (8),
                          juce::Justification::centred, 2);
    }
}

void Spectrogram3DComponent::timerCallback()
{
    if (! active)
        return;

    const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
    bool cameraMoved = false;

    if (autoRotateEnabled && dragMode == DragMode::none)
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

    if (cameraMoved)
        markSoftContentDirty();

    updateMeshFromSource();
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
        case MeshQuality::ultra:    outW = 288; outH = 240; break;
        case MeshQuality::overkill: outW = 512; outH = 448; break; // ~230k verts — GPU gym day
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
    // Keep low-end density ≥ uniform base: N' = base * ∫w = base * (1 + B/3).
    const int n = (int) std::ceil ((double) baseH * (1.0 + (double) B / 3.0));
    return juce::jlimit (baseH, kMaxFreqMeshRows, n);
}

float Spectrogram3DComponent::meshTFromFreqAxis (float u, float B) noexcept
{
    u = juce::jlimit (0.0f, 1.0f, u);
    if (B < 1.0e-5f)
        return u;
    // CDF of w(u)=1+B*u^2
    return (u + B * u * u * u / 3.0f) / (1.0f + B / 3.0f);
}

float Spectrogram3DComponent::freqAxisFromMeshT (float t, float B) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);
    if (B < 1.0e-5f)
        return t;

    // Solve u + (B/3) u^3 = t * (1 + B/3) via Newton.
    const float target = t * (1.0f + B / 3.0f);
    float u = t; // uniform seed
    for (int i = 0; i < 8; ++i)
    {
        const float f = u + (B / 3.0f) * u * u * u - target;
        const float df = 1.0f + B * u * u;
        u -= f / juce::jmax (1.0e-6f, df);
        u = juce::jlimit (0.0f, 1.0f, u);
    }
    return u;
}

void Spectrogram3DComponent::fillMeshColumn (int meshCol, const float* histCol, int histH)
{
    if (histCol == nullptr || meshH <= 1 || histH <= 1
        || meshCol < 0 || meshCol >= meshW)
        return;

    const float B = freqMeshBiasB();
    for (int z = 0; z < meshH; ++z)
    {
        const float t = meshH > 1 ? (float) z / (float) (meshH - 1) : 0.0f;
        const float u = freqAxisFromMeshT (t, B); // 0=low … 1=high
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
}

void Spectrogram3DComponent::ensureIndexBuffer (int w, int h)
{
    const bool wantClosed = closedMeshEnabled;
    if (indicesValid && meshW == w && meshH == h && meshClosed == wantClosed && ! cpuIndices.empty())
        return;

    std::vector<uint32_t> inds;
    const int topCount = w * h;

    // Top heightfield (CCW when viewed from above).
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
            inds.push_back (i0); inds.push_back (i2); inds.push_back (i1);
            inds.push_back (i1); inds.push_back (i2); inds.push_back (i3);
        }
    }

    if (wantClosed)
    {
        // Bottom cap: verts [topCount .. 2*topCount), winding flipped (normals down).
        for (int z = 0; z < h - 1; ++z)
        {
            for (int x = 0; x < w - 1; ++x)
            {
                const uint32_t i0 = (uint32_t) (topCount + z * w + x);
                const uint32_t i1 = i0 + 1;
                const uint32_t i2 = i0 + (uint32_t) w;
                const uint32_t i3 = i2 + 1;
                inds.push_back (i0); inds.push_back (i1); inds.push_back (i2);
                inds.push_back (i1); inds.push_back (i3); inds.push_back (i2);
            }
        }

        // Walls: each border edge → quad between top and bottom.
        auto wallQuad = [&] (uint32_t tA, uint32_t tB, uint32_t bA, uint32_t bB)
        {
            inds.push_back (tA); inds.push_back (bA); inds.push_back (tB);
            inds.push_back (tB); inds.push_back (bA); inds.push_back (bB);
        };

        // z = 0 edge (x increasing)
        for (int x = 0; x < w - 1; ++x)
        {
            const uint32_t tA = (uint32_t) x;
            const uint32_t tB = (uint32_t) (x + 1);
            wallQuad (tA, tB, tA + (uint32_t) topCount, tB + (uint32_t) topCount);
        }
        // z = h-1 edge (x increasing) — flip for outward normal
        for (int x = 0; x < w - 1; ++x)
        {
            const uint32_t tA = (uint32_t) ((h - 1) * w + x);
            const uint32_t tB = tA + 1;
            wallQuad (tB, tA, tB + (uint32_t) topCount, tA + (uint32_t) topCount);
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
    std::vector<Vertex> verts ((size_t) vertCount);
    const float denom = juce::jmax (1.0f, maxDb - minDb);
    const float baseY = closed ? -kClosedMeshFloorBias : 0.0f;
    const float playheadWallX = 1.0f + kClosedPlayheadWallBias;
    const float waterfallEndWallX = -1.0f - kClosedWaterfallEndWallBias;

    const float B = freqMeshBiasB();

    auto freqUForRow = [&] (int z) -> float
    {
        const float t = meshH > 1 ? (float) z / (float) (meshH - 1) : 0.0f;
        return freqAxisFromMeshT (t, B); // 0=low … 1=high
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
    // triangle walk when flat shading — it was the main mesh-rebuild cost.
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

    const juce::ScopedLock sl (meshLock);
    cpuVertices.swap (verts);
    meshNeedsUpload = true;
}

void Spectrogram3DComponent::updateMeshFromSource()
{
    if (dataSource == nullptr || ! dataSource->isSpectrogramEnabled())
        return;

    std::vector<float> history;
    int histW = 0, histH = 0;
    float brightness = 1.0f, minDb = -90.0f, maxDb = -6.0f;
    dataSource->getHistorySnapshot (history, histW, histH, brightness, minDb, maxDb);
    if (histW < 2 || histH < 2 || history.empty())
        return;

    int wantW = 0, baseH = 0;
    meshSizeForQuality (wantW, baseH);
    wantW = juce::jmin (wantW, histW);
    // Base H may exceed histH (oversample); bias then adds HF rows without thinning lows.
    const int wantH = effectiveFreqMeshRows (baseH);

    const uint64_t serial = dataSource->getHistoryColumnSerial();
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
        return;
    }

    if (serial > lastHistorySerial)
    {
        // Spec can write up to kMaxColumnsPerTick per 60 Hz tick (and more when the
        // UI timer jitters). Consuming the whole serial delta here scrolled several
        // mesh columns in one frame → visible timebase “bursts”. Always advance
        // exactly one column per 3D tick; if we were >1 behind, jump to latest so
        // we don't duplicate the newest column while draining (append always pulls
        // the rightmost history column).
        if (serial - lastHistorySerial > 1)
            lastHistorySerial = serial - 1;

        appendMeshColumnsFromHistory (history, histW, histH, 1);
        lastHistorySerial = serial;
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
        return;
    }

    if (lookChanged)
    {
        lastBrightness = brightness;
        lastMinDb = minDb;
        lastMaxDb = maxDb;
        rebuildVerticesFromMeshDb (brightness, minDb, maxDb);
    }
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
    // Remap existing history — do not clear meshDb / serial.
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

    /** Period slider under Turntable — does not dismiss the menu when dragged. */
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
            valueLabel.setText ("1× / " + juce::String (sec) + " s", juce::dontSendNotification);
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
            valueLabel.setText ("1× / " + juce::String (sec) + " s", juce::dontSendNotification);
        }

        Spectrogram3DComponent& owner;
        juce::Label label;
        juce::Label valueLabel;
        juce::Slider slider;
    };
}

void Spectrogram3DComponent::showContextMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Save as Default View");
    menu.addItem (2, "Reset Camera (F)");
    menu.addSeparator();
    menu.addItem (3, "Turntable", true, autoRotateEnabled);
    if (autoRotateEnabled)
        menu.addCustomItem (4, std::make_unique<AutoRotatePeriodMenuItem> (*this), nullptr, "Turntable speed");
    menu.addItem (5, "Oscillate Zoom", true, zoomOscillateEnabled);
    if (zoomOscillateEnabled)
    {
        menu.addCustomItem (6, std::make_unique<ZoomOscillateDepthMenuItem> (*this), nullptr, "Zoom depth");
        menu.addCustomItem (7, std::make_unique<ZoomOscillateRateMenuItem> (*this), nullptr, "Zoom rate");
    }
    menu.showMenuAsync (juce::PopupMenu::Options()
                            .withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 })
                            .withMousePosition(),
        [safe = juce::Component::SafePointer<Spectrogram3DComponent> (this)] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 1)
                safe->saveAsDefaultView();
            else if (result == 2)
                safe->resetCamera();
            else if (result == 3)
                safe->setAutoRotateEnabled (! safe->isAutoRotateEnabled());
            else if (result == 5)
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
    const float t = meshTFromFreqAxis (freqU, freqMeshBiasB());

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

void Spectrogram3DComponent::handleMouseDown (const juce::MouseEvent& e)
{
    lastDrag = e.position;
    rightClickCandidate = false;
    rightClickDragged = false;

    // Turntable controls (no free tumble / roll):
    //  LMB drag           = orbit around look-at (yaw / elevation)
    //  RMB drag           = move look-at (truck / pedestal) — keeps orbit centred
    //  RMB click          = context menu (Save Default / Reset Camera)
    //  Shift / MMB        = pan look-at on the ground plane
    //  Alt+LMB            = dolly (distance)
    //  Ctrl/Cmd+LMB       = set DOF focus distance under cursor
    //  Wheel              = zoom / dolly
    if (isRightMouse (e))
    {
        dragMode = DragMode::screenPan;
        rightClickCandidate = true;
        rightClickStart = e.position;
    }
    else if (e.mods.isMiddleButtonDown() || e.mods.isShiftDown())
    {
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
    // Live button state wins — some hosts deliver RMB drag without keeping popup flags.
    if (isRightMouse (e))
        dragMode = DragMode::screenPan;
    else if (e.mods.isMiddleButtonDown())
        dragMode = DragMode::pan;

    if (dragMode == DragMode::none)
        return;

    const auto d = e.position - lastDrag;
    lastDrag = e.position;

    if (dragMode == DragMode::orbit)
    {
        // Yaw around world +Y through the look-at; pitch = elevation only.
        camera.yawDeg -= d.x * 0.35f;
        camera.pitchDeg += d.y * 0.35f;
        clampCamera();
    }
    else if (dragMode == DragMode::pan)
    {
        // Slide the orbit pivot across the floor (XZ), relative to current yaw.
        juce::Vector3D<float> right, up, forward;
        cameraBasis (right, up, forward);
        const float scale = 0.0025f * camera.distance;
        // Flatten forward onto the floor so MMB stays a ground pan.
        juce::Vector3D<float> fwdXZ { forward.x, 0.0f, forward.z };
        const float fLen = juce::jmax (1.0e-6f, std::sqrt (fwdXZ.x * fwdXZ.x + fwdXZ.z * fwdXZ.z));
        fwdXZ.x /= fLen;
        fwdXZ.z /= fLen;
        camera.panX += (right.x * d.x + fwdXZ.x * (-d.y)) * scale;
        camera.panZ += (right.z * d.x + fwdXZ.z * (-d.y)) * scale;
        clampCamera();
    }
    else if (dragMode == DragMode::screenPan)
    {
        if (rightClickCandidate
            && ! rightClickDragged
            && e.position.getDistanceFrom (rightClickStart) > 4.0f)
        {
            rightClickDragged = true;
            // Start pan from the click origin so the threshold deadzone doesn't jump.
            lastDrag = rightClickStart;
            return;
        }

        // Click-without-drag opens the menu — ignore micro-moves under the threshold.
        if (rightClickCandidate && ! rightClickDragged)
            return;

        // Truck / pedestal by moving the orbit pivot in camera right / up —
        // so subsequent orbits still spin around the framed centre.
        juce::Vector3D<float> right, up, forward;
        cameraBasis (right, up, forward);
        juce::ignoreUnused (forward);
        const float scale = 0.0025f * camera.distance;
        camera.panX += (right.x * d.x + up.x * (-d.y)) * scale;
        camera.panY += (right.y * d.x + up.y * (-d.y)) * scale;
        camera.panZ += (right.z * d.x + up.z * (-d.y)) * scale;
        clampCamera();
    }
    else if (dragMode == DragMode::dolly)
    {
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
    // RMB click (no meaningful drag) restores the view menu.
    if (rightClickCandidate && ! rightClickDragged)
        showContextMenu (e.getScreenPosition());

    rightClickCandidate = false;
    rightClickDragged = false;
    dragMode = DragMode::none;
}

void Spectrogram3DComponent::handleMouseWheel (const juce::MouseWheelDetails& wheel)
{
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
