# The round island: layout for sign-off (phase 3, step 1)

Plan phase 3 ("One big world") starts by laying the pie out to scale for Raven's OK, before any ground is built. This is
that layout. The numbers come from `Tools/terrain/round_island.mjs`, which writes `Tools/terrain/round_island_layout.json`
(every number the build scripts will use) and `round_island_map.svg` beside this file. To change the layout, change the
script's inputs and run it again: `node Tools/terrain/round_island.mjs`.

![The round island to scale](round_island_map.svg)

## What it follows

- **Raven's answers (11-18, the plan doc):** a round island with Hearthmoot in the middle; nine surface lands as uneven
  slices sized like Aaron's lands; 10 to 12 km across, every land scaled up by the same amount (about double the area);
  open, blended borders; easy at the coast, harder inland, guarded roads to the city, bosses off the roads; the sky
  islands over the north, the Crystal caves under the north-east, the Fungal caverns under the south-west; the Ocean all
  round; Wind, Spore and Arcane players wake inside their layer; each land's half of its old pass becomes its gate road
  (fort, champion, mouth shrine); the Old Island stays its own map.
- **Aaron's land sizes** (`Docs/design/cooks/world-map.md` 2.2), times two.

## The lands

Clockwise from the north-west. Bearings are degrees clockwise from north; the bands are equal depths between the
Commons and the coast.

| Land | Path | Aaron's km² | Now km² | Bearing | Coast | Coast / midlands / inland km² |
|---|---|---|---|---|---|---|
| Storm peaks | Lightning | 5.5 | 11.0 | 283–329° | 4.3 km | 5.6 / 3.7 / 1.8 |
| Desert | Sun | 6.5 | 13.0 | 329–21° | 4.9 km | 6.6 / 4.3 / 2.1 |
| Ancient forest | Nature | 7.0 | 14.0 | 21–77° | 5.3 km | 7.1 / 4.7 / 2.2 |
| Frozen tundra | Frost | 5.0 | 10.0 | 77–119° | 3.8 km | 5.1 / 3.3 / 1.6 |
| Swamp | Blood | 4.0 | 8.0 | 119–150° | 3.0 km | 4.1 / 2.7 / 1.3 |
| Ocean & reefs (shore) | Tide | 3.0 | 6.0 | 150–174° | 2.3 km | 3.1 / 2.0 / 1.0 |
| Graveyard | Necromancy | 2.5 | 5.0 | 174–195° | 2.0 km | 2.5 / 1.7 / 0.8 |
| Corrupted wasteland | Void | 6.0 | 12.0 | 195–244° | 4.6 km | 6.1 / 4.0 / 1.9 |
| Volcanic | Fire | 5.0 | 10.0 | 244–283° | 3.7 km | 5.1 / 3.3 / 1.6 |

- **Size:** 89 km² of land plus Hearthmoot and its Commons (1.3 km²): **10.7 km across**, the coast 5.1 to 5.5 km from
  the centre (a few slow bays and headlands, so it reads as land, not a disc).
- **Why this order:** the sky is thickest over Storm, Desert and Forest, so they take the north; the Crystal caves lie
  under Forest and Tundra, the Fungal caverns under Graveyard and Wasteland; the Ocean keeps its shore slice in the
  south. Opposites face each other across the city: Sun and Tide, Nature and Necromancy, Fire and Frost, Lightning and
  Blood (and the Wasteland faces the Crystal caves, Void and Arcane).
- **Each land:** three wake-up camps on its coast; a gate road from the middle camp to its city gate (about 4.7 to 5 km);
  on that road, its old pass's works: the mouth shrine where the road leaves the coast band, the champion where it
  enters the inland band, the fort at the Commons' edge; its major boss deep inland and off the road.

## Hearthmoot

- Walls **600 m across** on a low hill, ringed by **the Commons** (1.3 km across): open, guarded ground where the nine
  roads meet, with no wild creatures. Fully safe; nothing owned inside (Raven's answers).
- **Twelve quarters**, one per path, 30° each, in the ring's order so each land's quarter lies behind its gate where it
  can: Lightning, Wind, Sun, Nature, Arcane, Frost, Blood, Tide, Necromancy, Spore, Void, Fire. Wind's, Arcane's and
  Spore's quarters sit between the lands their layers lie over or under. Each quarter opens when its land's boss falls.

## The layers

| Layer | Where | Size | Ways in |
|---|---|---|---|
| Sky islands | Over Storm, Desert and Forest (306–49°), 1.9 to 3.8 km from the centre, 500 to 1,300 m up; a few over other slices | 4 km² of islands in ~10 km² of air | Updrafts, gliding, flying mounts |
| Crystal caves | Under Forest and Tundra (27–115°), 1.5 to 2.7 km from the centre | ~4 km² | The Severance (a rift-cut gorge at the Forest–Tundra edge), crystal rifts |
| Fungal caverns | Under Graveyard and Wasteland (176–238°), 1.5 to 3.6 km from the centre | ~6 km² | Cave mouths, sinkholes; the Flue rises from its floor to the surface, a sky island anchored over it |

- **Starts inside the layers (Raven Q18):** Wind on a low starter sky island over the north coast (ropes, ledges and
  updrafts down); Spore in a shallow cavern under the south-west coast; Arcane in a shallow crystal hall under the
  north-east coast. No markings out of the two caverns.
- No sea or rain inside caverns; a dim glow that pulses with the day above.

## The sea

Calm shallows 300 m out from every coast (by the camps), reefs out to 1.2 km, then deep water; Thalassor's waters past
the reefs south of the Ocean's shore; the level is 16 km square. Open sea counts as Ocean & reefs.

## After the OK

The build goes slice by slice, each checked against its land's design and its landmark checklist from the same camera
spots, with pictures for Raven at each slice. Proposed first: Hearthmoot with its Commons and roads, then Volcanic and
Frozen tundra (their content and creatures exist on the Old Island, so the slice method is proven on known ground),
then the rest round the ring.
