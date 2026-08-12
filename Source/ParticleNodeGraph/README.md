# Particle Node Graph

Industry-style typed node editor for Spec3D particles (Houdini / Embergen / UE Niagara inspired).

## Open
**Settings → Spec3D Look → Particles → “Open Particle Node Graph…”**

## Window
| UI | Role |
|----|------|
| **File · Edit · Options · View · Help** | Full menu bar |
| **Toolbar** | Live ON/OFF · Apply · status |
| **Canvas** | Graph editor |
| **Output** | Collapsible compile / debug log |

## Industry-standard editing
| Feature | How |
|---------|-----|
| **Undo / Redo** | `Ctrl+Z` / `Ctrl+Y` (or Shift+Ctrl+Z) · Edit menu |
| **Copy / Cut / Paste** | `Ctrl+C` / `X` / `V` |
| **Duplicate** | `Ctrl+D` · keeps internal wires |
| **Delete** | `Del` (nodes or selected wire) |
| **Grid snap** | Options → Snap to Grid (default ON); snaps on move end |
| **Property edit** | Double-click node · RMB → Edit Properties |
| **Save / Load** | `Ctrl+S` / `O` → `Documents/Decksounds/ParametricEq/particle_node_graph.xml` · Save/Load As… |
| **Type-safe wires** | Incompatible connections are **refused** (no silent red graph pollution) |
| **Compatible pin glow** | While dragging a wire, valid targets pulse white |
| **RMB on pin** | Create **only compatible** node + auto-wire |
| **Live Apply** | Topology/param changes push to particles (~60 ms debounce) |

## Navigation
| Input | Action |
|-------|--------|
| MMB / Space+LMB | Pan |
| Wheel | Zoom |
| **Home** | Fit all nodes |
| **F** | Frame selection |
| **L** | Layered auto-arrange |

## Wiring
| Input | Action |
|-------|--------|
| Drag output → input | Connect (type-checked) |
| **Ctrl+click pin** | Disconnect all wires on pin |
| **RMB pin (dot)** | Filtered create+wire menu |
| Select wire + Del | Remove wire |
| Hover pin | Tooltip: name · type · RMB hint |

## Types
Float, Int, Bool, Vector2/3/4, Colour, Particles, Force, Emitter — colour-coded.  
Ramp / Field reserved for future assets & fluids.

## Attributes
- **Filter by Attribute** — threshold / amount / curve / stage  
- **Colour Ramp (Attribute)** — 2-stop colour from FFT, age, etc.  
Double-click Filter/Ramp → attribute picker; double-click again path or Edit Properties for params.

## Live apply
**Live: ON** (default) recompiles emitters, forces, filters, and colour ramps into the running Spec3D system when you wire, disconnect, or edit. Turn off for bulk edits, then **Apply** (`Ctrl+Enter`).

## Default graph
Spectrogram → Filter (FFT) → Colour Ramp → Sim Out + force stack.

## Architecture
- Compile → force stack + `GraphProgram` (filters / colour ramps)  
- Runtime spawn + update cull/colour  
- `Field` pin type reserved so fluids can land without a type-system rewrite  
