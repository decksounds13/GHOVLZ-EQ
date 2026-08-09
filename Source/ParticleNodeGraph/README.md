# Particle Node Graph

UE / Embergen / Houdini–inspired graph editor for Spec3D particles.

## Open
**Settings → Spec3D Look → Particles → “Open Particle Node Graph…”**

## Interaction
| Input | Action |
|-------|--------|
| MMB or Space+LMB drag | Pan |
| Wheel | Zoom |
| LMB on node | Select / move |
| LMB drag empty | Box select |
| LMB drag output pin → input | Connect wire |
| Drag from wired input | Rewire (pulls existing wire) |
| **Ctrl+click** (or **Cmd+click**) on pin circle | **Disconnect** all wires on that pin |
| RMB empty | Create node menu |
| RMB on node | Header colour palette, Arrange, Delete |
| **L** | **Arrange Nodes** (layered left→right, Houdini/UE style) — also in RMB menus as **Arrange Nodes · L** |
| Del / Backspace | Delete selection |
| Esc | Cancel wire drag |
| RMB on node → **Header Colour** | Palette for node header accent |

## Types
Float, Vector3, Bool, Force, Emitter — colour-coded pins.  
**Incompatible connections** still appear as **red wires** and show a type-mismatch tooltip; they do not compile.

## Apply
**Apply to Particles** compiles the graph into emission / lifespan / size / emitter / force stack on the live Spec3D particle system.

## Node library
- **System:** Simulation Output, Comment  
- **Emitters:** Spectrogram, Point  
- **Forces:** Gravity, Drag, Wind, Curl, Turbulence, Rotation  
- **Combine:** Forces, Emitters, Float (sum), Vector3 (sum)  
- **Constants:** Float, Vector3, Bool  
- **Math:** Add, Sub, Mul, Div, Lerp, Clamp, Abs, Negate, Min, Max, Power, Sin, Cos, Switch  
- **Vector:** Make / Break Vector3, Length, Normalize, Scale, Add, Dot, Float→Vector3  

### Combine nodes
- **Combine Forces** — In 1…6, multi-wire per pin. Recursively flattens into the force stack (order by node Y).  
- **Combine Emitters** — merges emitter branches; primary = first Spectrogram if present, else first by Y.  
- **Combine Float / Vector3** — sums all wired inputs.  

Sim Output **Force** / **Emitter** pins also allow multi-wire fan-in.

Default graph: Spectrogram → Sim Out, Gravity+Drag+Turbulence → **Combine Forces** → Sim Out.

## Future
- Simultaneous multi-emitter spawn, fluid nodes, collision, fields, live pin values, undo, graph prefs.
