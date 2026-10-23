# M3: World

**Goal:** the real first-playable map: about 4 × 4 km, a central ridge splitting the volcano (west face)
from the glacier (east face), crossed only by the north and south passes.

**Gate:** walking from a spawn to the nearest pass takes about 5 minutes (design target), the server
holds 60 fps tick budget at 20 players spread across the map, and each zone reads at a glance.

Depends on: M0. Can run in parallel with M1/M2.

## Layout (from the Map tab)
| Zone | Contents |
|---|---|
| Outer lowlands | 3 spawn points on each outer edge (west = volcanic, east = tundra), also safe zones; easy creatures |
| Midlands | mini-bosses, most caves and ore, medium creatures |
| Highlands | major bosses against the ridge, toughest creatures |
| Ridge | impassable, blocks travel between biomes |
| North + south passes | contested borderland, the only crossing; both biomes' creatures fight here; invasion route; likely PvP hotspot |

The Map tab notes the layout still needs work into "a realistic landmass, with natural coastlines,
terrain, rivers and the shape of the mountain range" before building. Step 1 below is that work.

## Pipeline ($0, scriptable)
| Step | Tool | Output |
|---|---|---|
| 1. Layout sketch → masks | Hand-drawn layout in Krita at 1 px = 4 m: land/sea, ridge spine, passes, rivers, zone bands | `Art/Terrain/layout.png` |
| 2. Heightmap generation | `Tools/terrain/gen_heightmap.mjs` (Node, already installed): ridge from the spine mask, noise for lowlands, hydraulic + thermal erosion, rivers carved from the river mask, coastline falloff | 16-bit `height.r16`, **4033 × 4033** (≈ 4.03 km at 1 m/quad) |
| 3. Biome & material masks | Same script: volcanic vs glacier by side of ridge, slope, height, wetness | `mask_*.png` weight maps |
| 4. Landscape import | `Tools/terrain/import_landscape.py` (editor Python): create World Partition landscape, apply height + weight maps | `L_Wildreach` landscape |
| 5. Landscape material | One auto-material: rock on slopes, ash/basalt/lava cracks west, snow/ice/permafrost east, blend in passes; runtime virtual texture | `M_Landscape_Auto` |
| 6. Dressing | **PCG graphs** per biome: rocks, dead trees/ash (volcanic), pines/ice spikes (tundra), grass; exclusion around paths, spawns, arenas | `PCG_Volcanic`, `PCG_Tundra` |
| 7. Hand-placed | spawns, safe-zone volumes, boss arenas, cave entrances, ore nodes, altars | level instances |

Alternative for step 2: Gaea Community / World Machine Basic if their free tiers' resolution and license fit. The Node generator avoids that question and keeps the map reproducible from the repo.

## Gameplay pieces
| Piece | Implementation |
|---|---|
| Safe zone | `AWRSafeZoneVolume`: blocks player damage to players and building inside; creatures can't path in (nav modifier) |
| Spawn points | `AWRSpawnPoint` tagged by biome; character creation picks one of the three for the main path's biome |
| Difficulty bands | `AWRSpawnRegion` volumes with a creature table + level range; banding by zone |
| Hazards (gear checks, no temperature meter) | Lava: damage-over-time GE unless the player has `Resist.Lava` (Burning blood, fire gear, or a volcanic tame/mount). Slick ice: CMC ground friction override unless `Resist.Ice` (spiked boots, frost gear). |
| Caves & ore | Cave meshes as level instances; `AWROreNode` (copper → bronze ingredients → iron) with depth-scaled tiers; server-side respawn timer |
| Signature resources | Volcanic: sulfur crystals (also Basaltback taming food); tundra: lichen (Woolly drudge food). Proposal; confirm with Raven. |
| Day/night | Directional light rotation driven by server time (replicated `GameState` time of day). Day/night spawn tables are a Full release item. |

## World Partition & networking
- Grid cell size 128 m, loading range 512 m for clients.
- Dedicated server: enable server streaming of cells around each player (`wp.Runtime.EnableServerStreaming 1`); bosses and claims stay loaded through always-loaded data layers.
- HLODs built for the ridge and distant terrain so the ridge silhouette is always visible.

## Risks
- 4 km² of hand-dressed landscape is a lot for one person. PCG does the bulk; hand placement only at points of interest.
- Erosion sim at 4033² in Node: expect minutes per run; cache intermediate stages.
