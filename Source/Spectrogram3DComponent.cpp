#include "Spectrogram3DComponent.h"
#include "SpectrogramComponent.h"
#include "Menu/SharedResources.h"
#include "ComboBoxLookAndFeel.h"
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
        void main()
        {
            vColour = colour;
            vNormal = mat3 (viewMatrix) * normal;
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
        out vec4 fragColour;
        uniform vec2 uResolution;
        uniform float uCornerRadius;
        uniform vec4 uClearColour;
        uniform vec3 uLightDirView;
        uniform float uLightingAmount;
        uniform float uSpecular;
        uniform float uRoughness;
        uniform float uRim;
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

            // Self/contact shadows are key-light effects — when lighting is off they are
            // forced to 1.0 via uniforms; AO may still apply (ambient, own toggle).
            float shade = shadow * ao * contact;
            float amt = clamp (uLightingAmount, 0.0, 1.0);

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
            float metalish = clamp (uSpecular, 0.0, 1.0);
            vec3 F0 = mix (vec3 (0.04), albedo, metalish * 0.35);
            float D = distributionGGX (NdotH, rough);
            float G = geometrySchlickGGX (NdotL, rough) * geometrySchlickGGX (NdotV, rough);
            vec3 F = fresnelSchlick (VdotH, F0);
            vec3 specular = (D * G * F) / max (4.0 * NdotV * max (NdotL, 1.0e-4), 1.0e-4);
            specular *= metalish * shadow;

            // Soft Lambert wrap so the lighting terminator isn't a hard edge either.
            float wrap = NdotL * 0.72 + 0.28;
            vec3 diffuse = albedo * (0.22 * ao + 0.78 * wrap * shadow);
            float rim = pow (1.0 - NdotV, 2.5) * uRim * ao;
            vec3 sss = subsurfaceScatter (albedo, n, l, v, shadow);
            vec3 lit = diffuse + specular + albedo * rim + sss;
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

    // mode: 0 = copy, 1 = SSAO, 2 = bloom extract, 3 = blur H, 4 = blur V, 5 = bloom composite
    constexpr const char* kPostFragmentShader = R"(
        #version 150
        in vec2 vUv;
        out vec4 fragColour;
        uniform sampler2D uTex;
        uniform sampler2D uDepth;
        uniform int uMode;
        uniform float uStrength;
        uniform float uRadius;
        uniform float uThreshold;
        uniform vec2 uResolution;
        uniform mat4 uInvProj;

        float depthSample (vec2 uv)
        {
            return texture (uDepth, uv).r;
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
    colourRimUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*colourShader, "uRim");
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
        postModeUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uMode");
        postStrengthUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uStrength");
        postRadiusUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uRadius");
        postThresholdUniform = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*postShader, "uThreshold");
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
    postThresholdUniform.reset();
    postRadiusUniform.reset();
    postStrengthUniform.reset();
    postModeUniform.reset();
    postDepthUniform.reset();
    postTexUniform.reset();
    postPositionAttrib.reset();
    postShader.reset();
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
    postFboW = postFboH = 0;
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
    if (colourLightingAmountUniform != nullptr)
        colourLightingAmountUniform->set (0.0f); // grid stays unlit
    if (colourSelfShadowUniform != nullptr)
        colourSelfShadowUniform->set (0.0f); // floor/grid: no heightfield shadow
    if (colourAoAmountUniform != nullptr)
        colourAoAmountUniform->set (0.0f);
    if (colourContactUniform != nullptr)
        colourContactUniform->set (0.0f);
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

    glDisable (GL_POLYGON_OFFSET_FILL);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
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
                       GLuint colourTex, GLuint secondTex,
                       float strength, float radius, float threshold, int vw, int vh)
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
        if (postResolutionUniform != nullptr)
            postResolutionUniform->set ((float) vw, (float) vh);
        if (postInvProjUniform != nullptr)
        {
            float id[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
            postInvProjUniform->setMatrix4 (id, 1, false);
        }

        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, colourTex);
        if (postTexUniform != nullptr) postTexUniform->set (0);
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, secondTex);
        if (postDepthUniform != nullptr) postDepthUniform->set (1);

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
        glActiveTexture (GL_TEXTURE1);
        glBindTexture (GL_TEXTURE_2D, 0);
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, 0);
        if (dest != nullptr)
            dest->releaseAsRenderingTarget();
    };

    // AO / self-shadow run in the mesh shader from the heightfield.
    // Post path is bloom-only (depth SSAO was unreliable with Soft BG + MSAA).
    if (owner.bloomEnabled)
    {
        const GLuint sceneTex = (GLuint) softFbo.getTextureID();
        drawFs (2, &postFboA, sceneTex, softDepthTex,
                owner.bloomStrength, 1.0f, owner.bloomThreshold, postFboW, postFboH);
        drawFs (3, &postFboB, (GLuint) postFboA.getTextureID(), softDepthTex,
                1.0f, 1.0f, 0.0f, postFboW, postFboH);
        drawFs (4, &postFboA, (GLuint) postFboB.getTextureID(), softDepthTex,
                1.0f, 1.0f, 0.0f, postFboW, postFboH);
        drawFs (5, &postFboB, sceneTex, (GLuint) postFboA.getTextureID(),
                owner.bloomStrength, 1.0f, 0.0f, width, height);
        drawFs (0, nullptr, (GLuint) postFboB.getTextureID(), softDepthTex,
                1.0f, 1.0f, 0.0f, width, height);
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
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
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

void Spectrogram3DComponent::setRimAmount (float amount01) noexcept
{
    amount01 = juce::jlimit (0.0f, 1.0f, amount01);
    if (std::abs (rimAmount - amount01) < 1.0e-4f) return;
    rimAmount = amount01;
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

    if (autoRotateEnabled && dragMode == DragMode::none)
    {
        const double nowSec = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (autoRotateLastTimeSec > 0.0)
        {
            const float dt = juce::jlimit (0.0f, 0.1f, (float) (nowSec - autoRotateLastTimeSec));
            const float period = juce::jmax (kAutoRotatePeriodMinSec, autoRotatePeriodSec);
            // Full revolution every `period` seconds (same yaw sense as LMB orbit).
            camera.yawDeg -= 360.0f / period * dt;
            markSoftContentDirty();
        }
        autoRotateLastTimeSec = nowSec;
    }
    else
    {
        autoRotateLastTimeSec = 0.0;
    }

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
        // x = 0 edge (z increasing) — flip
        for (int z = 0; z < h - 1; ++z)
        {
            const uint32_t tA = (uint32_t) (z * w);
            const uint32_t tB = (uint32_t) ((z + 1) * w);
            wallQuad (tB, tA, tB + (uint32_t) topCount, tA + (uint32_t) topCount);
        }
        // Playhead wall (x = +1): dedicated verts after top+bottom, biased to +X
        // so the face doesn’t share a depth plane with history behind the playhead.
        // Layout: [2*topCount .. 2*topCount+h) top row, then bottom row.
        const uint32_t phTop = (uint32_t) (topCount * 2);
        const uint32_t phBot = phTop + (uint32_t) h;
        for (int z = 0; z < h - 1; ++z)
        {
            const uint32_t tA = phTop + (uint32_t) z;
            const uint32_t tB = tA + 1;
            const uint32_t bA = phBot + (uint32_t) z;
            const uint32_t bB = bA + 1;
            wallQuad (tA, tB, bA, bB);
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
    // Closed: top + bottom + dedicated playhead wall (top/bottom rows along Z).
    const int vertCount = closed ? (topCount * 2 + meshH * 2) : topCount;
    std::vector<Vertex> verts ((size_t) vertCount);
    const float denom = juce::jmax (1.0f, maxDb - minDb);
    const float baseY = closed ? -kClosedMeshFloorBias : 0.0f;
    const float playheadWallX = 1.0f + kClosedPlayheadWallBias;

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

    auto heightAt = [&] (int x, int z) -> float
    {
        x = juce::jlimit (0, meshW - 1, x);
        z = juce::jlimit (0, meshH - 1, z);
        const float db = meshDb[(size_t) x * (size_t) meshH + (size_t) z];
        const float n = juce::jlimit (0.0f, 1.0f, (db - minDb) / denom);
        return n * meshHeight;
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

            // Central differences with actual world spacing (HF bias packs rows near highs).
            const float dx = 2.0f / (float) (meshW - 1);
            const float zPrev = worldZForU (freqUForRow (z - 1));
            const float zNext = worldZForU (freqUForRow (z + 1));
            const float dz = juce::jmax (1.0e-5f, std::abs (zNext - zPrev) * 0.5f);
            const float dHx = (heightAt (x + 1, z) - heightAt (x - 1, z)) / (2.0f * dx);
            float dHz = (heightAt (x, z + 1) - heightAt (x, z - 1)) / (2.0f * dz);
            if (zNext < zPrev)
                dHz = -dHz;
            // Normal = normalize((-dH/dx, 1, -dH/dz))
            float nx = -dHx, ny = 1.0f, nz = -dHz;
            const float len = juce::jmax (1.0e-5f, std::sqrt (nx * nx + ny * ny + nz * nz));
            vtx.nx = nx / len;
            vtx.ny = ny / len;
            vtx.nz = nz / len;

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

    if (closed)
    {
        // Playhead face: copy newest column, push +X past scroll (history moves -X).
        const int phTop = topCount * 2;
        const int phBot = phTop + meshH;
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
        const int delta = (int) juce::jmin<uint64_t> ((uint64_t) meshW, serial - lastHistorySerial);
        appendMeshColumnsFromHistory (history, histW, histH, delta);
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
    /** Period slider under Turntable — does not dismiss the menu when dragged. */
    class AutoRotatePeriodMenuItem final : public juce::PopupMenu::CustomComponent
    {
    public:
        explicit AutoRotatePeriodMenuItem (Spectrogram3DComponent& o)
            : juce::PopupMenu::CustomComponent (false),
              owner (o)
        {
            label.setText ("Speed", juce::dontSendNotification);
            label.setJustificationType (juce::Justification::centredLeft);
            label.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
            label.setFont (juce::FontOptions().withName ("Lato").withHeight (13.0f));
            addAndMakeVisible (label);

            valueLabel.setJustificationType (juce::Justification::centredRight);
            valueLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
            valueLabel.setFont (juce::FontOptions().withName ("Lato").withHeight (13.0f));
            addAndMakeVisible (valueLabel);

            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            slider.setRange (Spectrogram3DComponent::kAutoRotatePeriodMinSec,
                             Spectrogram3DComponent::kAutoRotatePeriodMaxSec,
                             1.0);
            slider.setSkewFactorFromMidPoint (10.0); // denser control near the 10s default
            slider.setScrollWheelEnabled (false);
            slider.setColour (juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha (0.55f));
            slider.setColour (juce::Slider::thumbColourId, juce::Colours::goldenrod);
            slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
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
}

void Spectrogram3DComponent::showContextMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Save as Default View");
    menu.addItem (2, "Reset Camera (F)");
    menu.addSeparator();
    menu.addItem (3, "Turntable", true, autoRotateEnabled);
    menu.addCustomItem (4, std::make_unique<AutoRotatePeriodMenuItem> (*this), nullptr, "Turntable speed");
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
        });
}

bool Spectrogram3DComponent::isRightMouse (const juce::MouseEvent& e) noexcept
{
    return e.mods.isRightButtonDown()
        || ((e.mods.getRawFlags() & juce::ModifierKeys::rightButtonModifier) != 0)
        || (e.mods.isPopupMenu() && ! e.mods.isLeftButtonDown() && ! e.mods.isMiddleButtonDown());
}

void Spectrogram3DComponent::handleMouseDown (const juce::MouseEvent& e)
{
    lastDrag = e.position;
    rightClickCandidate = false;
    rightClickDragged = false;

    // Turntable controls (no free tumble / roll):
    //  LMB drag        = orbit around look-at (yaw / elevation)
    //  RMB drag        = move look-at (truck / pedestal) — keeps orbit centred
    //  RMB click       = context menu (Save Default / Reset Camera)
    //  Shift / MMB     = pan look-at on the ground plane
    //  Alt / Ctrl+LMB  = dolly (distance)
    //  Wheel           = zoom / dolly
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
    else if (e.mods.isLeftButtonDown() && (e.mods.isAltDown() || e.mods.isCtrlDown() || e.mods.isCommandDown()))
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
    camera.distance *= (1.0f - wheel.deltaY * 0.15f);
    clampCamera();
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
