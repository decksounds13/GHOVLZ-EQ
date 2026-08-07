# GPU Particle Simulation Plan (Spec3D)

**Status:** Phase 1 hybrid implemented (toggle, CPU default).  
**Date:** 2026-08-06  
**Depends on:** Current hybrid system (CPU sim + GPU instanced draw). OpenGL 4.3 required for compute; falls back to CPU if unavailable.

---

## 1. Where we are today

| Stage | Location | Notes |
|-------|----------|--------|
| Spawn / lifetime / forces / matrix | CPU (`Spec3DParticleSystem::update`) | Caps spawns/frame to avoid melt |
| Instance pack | CPU → `GpuInstance` VBO stream | Per-frame upload |
| Draw | GPU `glDrawElementsInstanced` | Sphere / cube meshes, GGX + emissive |
| Billboard path | CPU verts + draw | Soft sprites |

**Bottleneck:** CPU spawn + force integration + matrix + buffer upload at high emission. Draw path is already GPU.

---

## 2. Goal

Move **simulation** (and eventually spawn) onto the GPU so:

- Emission can go much higher without frame spikes  
- Forces / age / matrix-style fields update in parallel  
- Instance data stays on GPU (no full pool readback)

Keep **Look / material** on existing mesh FS (PBR + emissive already matches waterfall).

---

## 3. Architecture options

### A. Compute shader particle buffers (preferred on GL 4.3+ / ES 3.1)

```
SSBO Particle[]  (pos, vel, life, colour, rot, em, flags…)
  → compute: integrate forces, age, kill
  → compute or CPU: spawn into free slots (atomic free-list)
  → draw: instanced mesh reading SSBO (vertex pull) OR compact alive → instance VBO
```

**Pros:** Modern, scalable, clean  
**Cons:** Needs GL 4.3+ compute; JUCE OpenGL module version / host context may be 3.2/4.1 core on some machines

### B. Transform feedback (GL 3.x compatible)

Ping-pong VBOs: update pass writes next state, draw pass consumes.

**Pros:** Works without compute  
**Cons:** Awkward spawn, harder variable particle counts, less flexible matrix routing

### C. Hybrid (recommended first milestone)

1. Keep **spawn + matrix source sampling** on CPU (audio/bin/age still easy)  
2. Upload only **birth events** or sparse updates  
3. **Integrate forces + age on GPU** (compute or TF)  
4. Draw instanced from GPU buffer  

Unblocks high particle counts while preserving Spec3D’s audio-driven matrix complexity.

---

## 4. Phased plan

### Phase 0 — Preconditions (short)

- [x] Spawn budget / emission safety (done)  
- [ ] Profile: % time spawn vs integrate vs `glBufferData`  
- [ ] Query runtime GL version; feature gate compute vs TF  
- [ ] Document max particles (current `kHardCap` ~24k)

### Phase 1 — GPU integrate (hybrid) ✅

- [x] Particle SSBO layout (`GpuSimParticle` 80 B std430) + force SSBO  
- [x] Compute pass: age/kill, gravity/drag/wind/curl/turbulence/rotation, settle, free OOB  
- [x] CPU still owns: emit, matrix colour/size, init vel/rot, trail XZ sample  
- [x] Draw: existing instanced mesh / billboard (readback pos after integrate)  
- [x] **Fallback:** CPU integrate if toggle off or compute unavailable  
- [x] Settings toggle **GPU particle integrate** (default off) + UI prefs `spec3dParticleGpuSim`  
- [x] OpenGL context required version **4.3** (soft; compute entry points gated)

### Phase 2 — GPU spawn

- Atomic free-list of dead indices  
- Spawn parameters buffer (binF, colour, vel…) from CPU playhead sampler  
- Continuous + slice modes produce **spawn requests**, not full Particle structs

### Phase 3 — GPU matrix-lite

- Upload global matrix slots as UBO (amount, dest, op, map range)  
- Sample simple sources on GPU: age, random, constant  
- Keep audio/bin sources as **per-particle fields set at spawn** or low-rate CPU refresh

### Phase 4 — Polish

- Sort / soft particles optional  
- Indirect draw (`glDrawElementsIndirect`)  
- LOD mesh (sphere subdiv by distance)  
- Telemetry: alive count, GPU timings

---

## 5. Force stack mapping

| Force | GPU Phase 1 | Notes |
|-------|-------------|--------|
| Gravity | Yes | Constant + matrix scale UBO |
| Drag | Yes | `vel *= exp(-k*dt)` |
| Wind | Yes | Constant / slow CPU UBO |
| Curl noise | Yes | Hash noise in compute (same as CPU) |
| Turbulence | Yes | Noise on vel |
| Rotation (spin) | Yes | Euler/quat integrate |
| Waterfall lock | Hybrid | Trail binding may stay CPU or GPU sample of heightfield texture |

Heightfield: already have height map path on waterfall — can bind as texture for GPU trail lock.

---

## 6. Risks

| Risk | Mitigation |
|------|------------|
| Host only gives GL 3.2 | TF path or keep CPU sim |
| JUCE buffer / context sharing | Own particle program; no readback of full pool |
| Matrix fidelity | Phase 1–2: spawn-time bake; Phase 3: subset on GPU |
| Debug harder | CPU reference path always available |
| Plugin CPU time budgets | Cap compute dispatches; respect `kHardCap` |

---

## 7. Success metrics

- 20k+ alive particles at 60 Hz with emission at UI max without hitch  
- Force stack visually matches CPU reference  
- No regression on PBR/emissive materials  
- Graceful fallback on weak GL

---

## 8. Suggested first implementation PR

**“GPU particle integrate (compute) + CPU spawn”**

1. Detect `GL_ARB_compute_shader`  
2. SSBO particle state  
3. Integrate compute for free-mode particles  
4. Compact alive → instance buffer for existing mesh draw  
5. Feature toggle: **GPU integrate** in particle settings (default off until stable)

---

## 9. Out of scope (for now)

- Full Niagara-style GPU graphs  
- Soft-body / collision  
- Multi-emitter systems  
- Export of particle-only passes  

---

*When ready to implement: start Phase 0 profiling + Phase 1 hybrid integrate.*
