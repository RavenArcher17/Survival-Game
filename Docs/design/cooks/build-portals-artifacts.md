# Cook: build-portals-artifacts (FULL-GAME 8.2 dungeon portals, 8.3 artifacts)

Lane: build-portals-artifacts. Written 2026-11-14. Builds in Raven's repo `C:\Dev\Wildreach` (UE 5.8 C++).
Design sources, KEPT as written (this lane builds them, it does not re-design them): `cooks/endgame.md` 2.5 (portals) and
2.6 (artifacts), `cooks/biome-desert.md` 2.22 (the portal framework), `cooks/fire-frost.md` (the two built lands).
This doc adds: the requirement ledger, the research the build leans on, every gap the build hit (where the fold lives,
how rooms are drawn, how doors, traps, puzzles, chests and bosses meet each other and the player) and the build steps.

---

## 0. Requirement ledger

Split from the brief, FULL-GAME 8.2 / 8.3 and the Aaron quotes in it. "Plan" = the section that answers it; "Proof" is
filled in at the end (section 6).

| # | Requirement (source) | Plan |
|---|---|---|
| L1 | Full cook: research → plan → build → spot-check (Aaron 11-13 night) | 1, 2, 3, 5 |
| L2 | Research names real games, the specific systems, what makes them work; real-world reference | 1 |
| L3 | Plan solves every design gap in writing before building; lists every build step | 2, 3 |
| L4 | Design with intention: every object's job, what it connects to, how it meets what it touches (Aaron 11-14) | 2.3, 2.4, 2.5, 2.6 |
| L5 | Machines look engineered; moving things move continuously on a believable mechanism (Aaron 11-14) | 2.4 (doors, chute, bellows), 2.6 |
| L6 | No missed requirements: ledger with proof per row (Aaron 11-14) | 0, 6 |
| L7 | Scope = full game (12 biomes), RAVEN-PLANNED is spec, DESIGN_REVIEW defaults used, nothing left "for Raven" | 2.1, 2.10 |
| P1 | Random portals in every biome, themed by biome | 2.1, 2.2 |
| P2 | Rank shown by colour; 5 ranks; higher ranks spawn deeper | 2.1 |
| P3 | Handmade rooms shuffled into new layouts (never generated) | 2.3 |
| P4 | Random modifiers (no healing, double damage + 8 more), shown before entry, +25% loot each | 2.7 |
| P5 | Puzzles and traps (traps in dungeons only, DR #21) | 2.4 |
| P6 | A boss at the end of every portal | 2.6 |
| P7 | Break timers 1 / 3 / 6 / 10 / 18 real days (server setting ×0.5 / ×1 / ×2 / never) | 2.2 |
| P8 | An ignored portal escalates and spills monsters first (cracking 50%, spilling 80% + warden) | 2.2 |
| P9 | When its boss escapes the portal closes; escaped boss roams 400 m, never targets claims | 2.2 |
| P10 | Escaped boss blocks that rank in that biome until killed | 2.2 |
| P11 | Entering a breaking portal is allowed until the boss escapes; clearing it stops the break | 2.2 |
| P12 | Craftable keys spawn a chosen rank; the signature resource sets the biome | 2.8 |
| P13 | Key portals belong to the opener (party and clan); wild portals are first come | 2.8 |
| P14 | Legendary portals announced to the whole server | 2.2 |
| P15 | Death inside follows server death rules; re-enter and run back from the entry stone while it's open | 2.5 |
| P16 | If the portal closes, bodies (death bags) appear where the entrance was | 2.5 |
| P17 | No dungeon currency, no forced party size; loot and materials only | 2.9 |
| P18 | A cleared portal drops a rank-coloured shard used in keys (DR #20 GAP, endgame default) | 2.9 |
| P19 | Rewards per rank (chests personal, boss roll per 10% player, blueprints, artifacts 3/8/15%) | 2.9 |
| P20 | Party entry (10 m, 5 s accept), others join until the boss door seals, leave by the entry arch | 2.5 |
| P21 | At most 8 runs open at once; "All folds are full" with the time | 2.5 |
| P22 | Portal look: a 3 m tear edged in the biome's material, light pillar in rank colour (60 m; rank 5 300 m), a hum by rank | 2.2 |
| P23 | Portal creatures by rank (lowland → portal-tempered) | 2.6 |
| P24 | Portal map inside the fold shows only rooms you've seen | 2.5 |
| P25 | Music: the biome's layer under a portal bed (through stone) | 2.11 |
| A1 | Three artifact slots | 2.12 |
| A2 | Artifacts drop from bosses and portals | 2.13 |
| A3 | Penalties: more damage taken (+7%/AL) plus a weird rule | 2.12 |
| A4 | Rewards: loot +5%/AL, rarity (part chance, quality step), path XP +4%/AL, exclusive drops (Scarred AL5, III-V weights AL10) | 2.12 |
| A5 | Party uses the highest multiplier; weird rules only on the wearer | 2.12 |
| A6 | PvE only: player damage pauses artifacts 30 s | 2.12 |
| A7 | Hidden third boss phases need artifacts (the top-damage party's AL ≥ 1) | 2.12 |
| A8 | The thirty artifacts, each one rule written as a geis | 2.12, 2.14 |
| A9 | Changing artifacts: 2 s, out of combat, not near an engaged boss, locked inside a portal; paused in safe zones | 2.12 |
| A10 | Geis records: one-time rewards per boss per new AL | 2.13 |
| A11 | Fusing two of a kind (n → n+1) and salvage at the alchemy table | 2.13 |
| A12 | Server settings: artifact level cap (off/5/10/15), artifact slots (0-3) | 2.10 |
| A13 | The band: a party beyond the clan for sharing multipliers | 2.12 |
| B1 | THE BAR: tells ≥ 0.9 s, projectiles never home, damage on visible contact, no attacker limit, players die | 2.4, 2.6 |
| B2 | UI: mouse and pad on every screen, no dead states | 2.15 |
| B3 | Data-driven: numbers in Data/*.json + tuning, all damage through ApplyHit, sounds/fx as rows, server authority | 3 |
| B4 | No placeholders shown as finished; anything unfinished stays hidden | 2.10, 4 |

---

## 1. Research (what the build takes from it)

| Game / source | The specific system | What makes it work | What we take, exactly |
|---|---|---|---|
| **Valheim**, Burial Chambers, Troll Caves, Sunken Crypts, Frost Caves | Each dungeon interior is a separate location placed **5,000 m straight up** from its entrance; walking through the door teleports you; the interior sets its own environment (no weather, dark, its own music) | No streaming seams, no holes in the terrain, any number of dungeons, and the interior can't be reached any other way | **The fold.** A portal's interior is built at run time in a **bay** high above the sea east of the island (bay 0..7, 1.2 km apart, 2 km up), closed on every side (floor, walls, ceiling), so the sky, sun and weather can't reach it (Lumen dark, sheltered exposure). Inside a fold the biome is the portal's (sound, temperature, darkness), it counts as underground (no building, no bombs) |
| **Valheim** dungeon generator | Hand-built rooms with typed connection sockets, joined by a seeded random pick; end caps close leftover sockets | Hand art, endless variety, every room still readable | **Shuffled handmade rooms**: the *rooms* are hand-written rows (`Data/portal_rooms/<biome>.json`), the *graphs* are hand-written slot lists (4 per rank); the run only picks which graph and which room fills each slot. Nothing is generated (DD rule) |
| **Diablo III Greater Rifts / Diablo IV Nightmare Dungeons** | A keystone (Sigil) opens a dungeon with affixes shown before you enter; affixes like "Drifting Shade", "Suppressor" add danger and reward | The player chooses risk knowingly; the affix list is one readable line each | **Keys** open a chosen rank on the spot; **modifiers** are one line each and sit on the portal's card before entry (no surprises); each adds +25% loot |
| **Path of Exile maps** | Map mods raise item quantity; maps drop maps | A loop that feeds itself without currency | **Rank shards → keys**: a cleared portal drops its rank shard; three of the rank below + one of the rank make a key. No currency |
| **Solo Leveling** gates | Ranked by colour; an ignored gate "breaks" and its monsters pour out; the boss escapes | Pressure that grows in the world itself | **Breaking**: stable → cracking (50%, packs every 30 game min) → spilling (80%, packs every 15 + a warden at the mouth) → break (100%): the boss walks out and roams 400 m; that rank in that biome is blocked until it dies |
| **Elden Ring catacombs** (Black Knife, Giant-Conquering Hero's Grave) | Lever-opened gates, pressure-plate traps with a click, fire-spitting statues, a Stake of Marika before the boss fog | Traps have a sound and a look before they bite and can be turned on enemies; the run-back is short | **Trap rule**: ≥ 0.9 s sound + visual tell, can kill, disarmable or usable on creatures, hurts creatures too. **Entry stone** is the one run-back point. **Boss door seals** behind the first player through it |
| **Zelda: Breath of the Wild shrines** | One idea per room, readable in a glance, solvable by anyone | Puzzles teach with the room itself | **Puzzles never need a specific path**, every clue is in the room (a lintel colour, a sun carving, a skylight band) |
| **Hades, Pact of Punishment** | Heat: conditions you pick that raise one number; bounties pay once per new Heat | One number for difficulty, a long climb one step at a time | **Artifact Level** 0-15 = sum of three Weights; **Geis records** pay once per boss per new highest AL |
| **Halo skulls** (Iron, Famine, Black Eye) | One rule per skull, stackable, with a score multiplier | One line each, easy to remember | **One weird rule per artifact**, written as a geis, never two effects |
| **Monster Hunter** quest parties / **Destiny** fireteams | A temporary group anyone can form, apart from the clan | Lets strangers play together without joining a guild | **The band**: invite any player from the Clan tab; no size cap (DD); ends when everyone leaves or logs off 10 min |
| **Irish geis** (Cú Chulainn, Connla, Conaire) | A personal taboo that brings power while kept | The real root of "a personal rule worn for reward" | Artifact rules are geasa (endgame 2.6.2, KEPT) |
| **Real smithing: temper colours** | Steel heated then quenched shows straw (~220 °C), bronze (~240), purple (~270), blue (~300) oxide colours; water quench is fastest, oil slower, brine harshest | The quench puzzle's clue is real | **Quench the blade**: the lintel's enamel band shows the temper colour; straw → oil, purple → water, blue → brine (the three troughs) |
| **Real smithing: bellows** | A double-chambered bellows gives a steady blast; a smith's striker answers the master's hammer in a set rhythm | The rhythm is something you hear | **Bellows rhythm**: the hammer automaton strikes 3-5 beats on its anvil, a pause, then you pump the three bellows in the same order |
| **Sea ice safety** (Inuit and Arctic travel rules) | Clear blue ice is strong (10 cm holds a person); white or grey ice is weak and full of air; black water shows through thin ice | A real-world readable rule | **Thin ice**: the safe path is dark-blue ice; the thin panes are grey-white with fine cracks; they creak 1.0 s, crack white, and drop you into black water 2 m deep |
| **Qulliq** (Inuit soapstone seal-oil lamp) | A half-moon soapstone dish with a moss wick along the straight edge | Warm light in an ice room | **Tundra lighting**: qulliq lamps in niches (warm), glacier blue through ice walls (cold) |
| **The sun's path** | East at dawn, south at noon, west at dusk | Anyone can read a carving of the sun's height | **Light the braziers**: each brazier's carving shows the sun at a height; light dawn → noon → dusk (east → west) |
| **Völundarkviða, Helgakviða Hjörvarðssonar** | Böðvildr at the smith's island; Hrímgerðr turned to stone by the dawn | Real roots for rank-5 bosses (endgame 2.5.6, KEPT) | Their arena mechanics: quench tanks (Böðvildr), frost bolts holding the dawn window (Hrímgerðr) |

---

## 2. Design (gaps the build hit, solved)

### 2.1 What ships now and what waits for its land (L7, B4)
The world today has two lands (Volcanic west, Frozen tundra east: `WRBiome::At`). A portal opens on its biome's ground,
so **portals spawn in Volcanic and Tundra now**; the other ten biomes' rows are in `Data/portals.json` (slots, colours,
signature resource, the rest of endgame 2.5.2's table) and stay **hidden** until their land exists (`land` false: no
slots, no key recipes shown). The Fire and Frost room kits, traps, puzzles, portal creatures and the ten portal bosses
are built in full. The other ten room kits and fifty bosses wait for their lands (logged in LATER-COOKS).

Ranks spawn deeper (P2): the island's bands by distance from the coast inward (lowland ring, midland, highland at the
crater / glacier): rank 1 in lowland, 2 lowland or midland, 3 midland, 4 highland, 5 highland or the hidden sub-biome
route (Kiln Roads / Blue Below), read from the existing spawn regions' `tier` (`Data/spawn_regions.json`).

### 2.2 The portal in the world (P1, P7-P11, P14, P22)
**Slots** (endgame 2.5.2): Volcanic 4 (C, U, R, High), Tundra 4. High rolls Epic 95% / Legendary 5%. An empty slot
refills 2-6 real hours later at a valid point: on land, in its band's spawn region, ≥ 150 m from claims, camps, shrines,
villages and boss arenas, ≥ 150 m from another portal, not in a pass (the world map's pass polygons), not in water, slope
under 25°. Times are **real UTC seconds** saved in the world (`portals.json`), so a server that's off still counts
(Raven's "real days").

**The look** (biome's signature material edging the tear):
- *Volcanic*: a ring of 14 fused obsidian shards standing in a 3.2 m tall ellipse on a cracked basalt plinth, each shard
  leaning in and glowing at its root; inside, the tear (a vertical glowing membrane: two crossed lit planes in the rank
  colour, slowly turning, ember motes drawn **in**). Slag cairns at the plinth's four corners.
- *Tundra*: a ring of 12 blue glacier-ice blades frozen into a snow-drifted plinth, rime on their tips; snow drawn in.
- The **light pillar**: a 60 m column (rank 5: 300 m) of the rank colour (bone white #E8E2D0, green #4FB04A, blue
  #3A7BDB, violet #8E4FD8, flame orange #FF7A1A; never gold or crimson), seen from 500 m (rank 5: the whole land).
- **Hum**: a looped bed at the portal, pitch by rank (1: 1.2, 5: 0.6 and wider), audible within 40 m.
- **States** shown on the tear: stable (steady pillar); cracking (the pillar flickers, the shards show bright cracks,
  the hum warbles); spilling (the pillar stutters, a red-black rim, the tear bulges, a creature voice in the hum).

**Breaking** (KEPT endgame): days by rank 1 / 3 / 6 / 10 / 18 × the server setting (`portalBreak`: half / normal /
double / never). Cracking at 50%: every 30 game min one pack of the portal's creatures steps out of the tear and roams
within 150 m. Spilling at 80%: every 15 game min, and once a **warden** (the biome's portal creature as an elite, 3,000
HP, ×1.3 size, a rank-colour glow) that stands guard within 12 m of the mouth. At 100% the boss **walks out of the tear**
(the tear flares, a roar), the portal closes, the boss roams within 400 m of the spot at full portal-boss HP and never
rests; it never targets claims (it has no raid route) and fights anything near it. While it lives that biome's rank
can't spawn or be keyed ("The Epic fold of the Volcanic is held shut: <boss> roams free"). Killing it pays its boss roll
and the shard. Spill packs and the warden stay until killed. **A run under way stops the clock** (entering a breaking
portal is allowed; clearing it ends the break). Packs only spill while a player is within 300 m (out of everyone's
range nothing is simulated; the clock still runs).

**Legendary** (P14): when a rank 5 opens (wild or key) every player gets the gold-free banner line "A Legendary portal
has opened in the <land>" with the rank colour, and a world sound. (The biome's Crown pillar flicker waits for the
Crown, which isn't built: logged.)

### 2.3 The fold: layout and rooms (P3, L4)
**The bay** (Valheim lesson): bay *i* origin = (1,500,000, i × 120,000, 200,000) cm. The run's rooms are built there,
the main line running +X, side branches ±Y.

**Handmade rooms**: each room row is `{id, type, w, d, h, doors, features, spawns, chest, trap, puzzle}` with sizes in
4 m cells (3 to 10 cells) and features placed by hand in room metres. **Types and pools** (KEPT): entry 2, combat 10,
trap 6, puzzle 4, rest 2, treasure 3, boss 5 (one per rank) = 32 per biome, 64 written now.

**Graphs** (KEPT 4 per rank per biome): a main list of slot types from entry to boss, and branches `{after: n, type}`
that hang a treasure (or rest) room off room *n* to the north or south. Placement: main rooms left to right with a 8 m
corridor between door sockets (walled and roofed, the room's stone); a branch room sits 8 m off the host's north wall
(south if the north is taken or would overlap). No room shares space with another (checked; a branch flips side, and a
graph whose branch still can't fit drops that branch rather than overlap).

**Room build** (every room, both kits):
- Floor: flagstones laid on the 4 m grid with 2 cm joints (one block per 2 × 2 m stone, colour jitter ±6%), a 40 cm
  thick slab under them (collision).
- Walls: coursed ashlar (volcanic black basalt, 60 cm courses, offset joints) or glacier ice (tundra: 1 m thick ice
  panels with trapped stones, a darker base course of fieldstone), pilasters every 8 m, a cornice at the springing.
- Ceiling: a flat slab 40 cm thick with cross beams every 4 m (volcanic: bronze-banded basalt lintels; tundra: whale-bone
  and driftwood ribs), closed so no light leaks.
- Doors: every door socket is a 4 m wide, 6 m tall opening with a lintel and jambs; between rooms a corridor (4 m wide,
  6 m tall, 8 m long) of the same stone.
- Light: from what's in the room, never ambient: lava runnels and crucibles (volcanic: warm orange 2,700 K), qulliq lamps
  in niches and blue glow through the ice (tundra). Each light is a real thing drawn in the room.

**Features** (drawn by code from the build kit, collision fitted): `pillar` (volcanic hex basalt column on a plinth with
a bronze band; tundra a column of ice with a fieldstone foot), `ledge` (a raised 1.6 m platform with a stepped ramp:
height and cover), `runnel` (a floor channel 0.8 m wide with lava (lit, hurts 20/s + burn) or meltwater (cold, slows)),
`crucible` (an iron crucible on chains hung from a ceiling beam, glowing), `anvil`, `brazier`, `lamp` (wall niche), `rack`
(tool rack: tongs, hammers), `rubble`, `statue` (volcanic a smith figure in basalt; tundra an inuksuk), `bones`,
`icicles`, `pool` (black water, tundra), `cinder` (a slag heap). Spawns: hand-placed points per room; the creatures there
are rolled from the kit's table by rank.

**Why each room exists** (the fold's story, endgame 2.5.2): a volcanic fold is a sealed forge hall of the Smiths (the
Kiln Roads people): the combat halls are casting floors and storerooms, the trap corridors are the guarded ways to the
treasury, the puzzle rooms are the trial rooms an apprentice had to pass to reach the master's forge (the boss arena),
the treasure rooms are the treasury and the rest alcoves the quench-water spring. A tundra fold is the Rime-folk's Blue
Below: ice halls where they stored meat and hid their dead, the trap ways are the old crevasse paths, the puzzle rooms
are their seasonal rites (the sun's order, the aurora), the boss arena their sealed cave.

### 2.4 Traps and puzzles (P5, B1, L4, L5)
Trap rule: ≥ 0.9 s sound + a visible tell, can kill, can be disarmed or used on creatures, hurts creatures too. All
damage through `ApplyHit` (no instigator: the fold's own hazard).

| Kit | Trap | Mechanism (what drives it, where it goes) | Tell | Effect | Answer |
|---|---|---|---|---|---|
| Volcanic | **Pulse grates** | Iron floor grates (1.6 × 1.6 m, 6 bars) over lava channels that run under the floor from a lava sump in the wall; a pulse every 4.5 s, offset per grate along the corridor | The grate glows dull red and the hiss rises for 1.2 s | Lava wells up through the grate for 2 s: 30/s + burn 20/s to anyone standing on it, creatures too | Walk the bare stone between; slide a loose anvil block (Interact 1.5 s, it scrapes along the floor) onto a grate: it's plugged for the run |
| Volcanic | **Slag chute** | A ceiling chute fed by a crucible above (seen through the slot); its lip is held shut by a rope over a pulley to a cleat on the wall | The bell on the chute's chain rings once and the lip glows 1.0 s | A 3 × 6 m strip of molten slag: 80 + burn 40, then 3 s of glowing slag on the floor (20/s) | Stand clear; the rope (100 HP) can be cut to pour it early onto creatures under it; it re-arms after 12 s |
| Tundra | **Icicle fall** | Icicles grown from the ceiling over the corridor, cracking from their roots | A sharp crack, ice dust drifting, the icicle's shadow growing on the floor for 1.0 s | 60 + frostbite 30 in a 1.5 m circle; the icicle shatters (it's gone) | Step out of the shadow; any fire or a hit (a thrown stone, an arrow) drops it early |
| Tundra | **Thin ice** | Panes of thin grey-white ice over a black-water pool 2 m deep; the safe path is dark blue ice | Creaking and white cracks spreading for 1.0 s after you step on | The pane gives: you drop into black water (frostbite 15/s, slowed; climb out at the edge, a ledge), the pane refreezes after 20 s | Walk the dark blue ice; creatures fall in too |

| Kit | Puzzle | How it works | Clue (in the room) | Fail | Opens |
|---|---|---|---|---|---|
| Volcanic | **Quench the blade** | A blank glows on the anvil; three troughs (water, oil, brine) by the far wall. Interact the blank: it's in your tongs, glowing, and you walk slower (it cools back onto the anvil after 30 s); Interact a trough to quench | The door lintel's enamel band: straw / purple / blue (real temper colours: straw → oil, purple → water, blue → brine) | Wrong trough: a crack, steam, the blank is ruined; a new blank heats on the anvil in 20 s | The door's iron bar lifts on a chain (it slides, it doesn't vanish) |
| Volcanic | **Bellows rhythm** | A great forge with three bellows (left, middle, right) worked by levers; a hammer automaton (iron arm on a cam shaft driven from the forge's water wheel slot) strikes 3-5 beats, each beat on the anvil spot under one bellows | The beats: which spot the hammer falls on, in order | A wrong pump: the forge coughs smoke, the rhythm repeats (a new one after 3 fails) | The forge roars and the bar lifts |
| Tundra | **Light the braziers** | 3-5 frozen braziers round the room; Interact 2 s to light (you need a lit torch or fire path; or the room's qulliq lamp to carry fire from, Interact to take a flame for 20 s) | Over each a carving of the sun at a height (low left = dawn in the east, high = noon, low right = dusk); light them east → west | Out of order: all go out with a hiss of snow from the vent above | The ice door melts (it shrinks and slumps over 3 s, meltwater runs) |
| Tundra | **Aurora lock** | A skylight shaft of a slow aurora in three colour bands; three ice prisms on stands throw a coloured beam onto the door's three sockets; Interact turns a prism 60° | Each socket's colour must match the band above it | — (turn until right) | The door's three sockets glow and the ice door melts |

The trap and puzzle rooms put a fight behind the trap (creatures that push you into it: they're spawned beyond the traps
and come at you through them) as KEPT.

### 2.5 Running a portal (P15, P16, P20, P21, P24)
- **Approach**: Interact at the tear opens the **portal card** (mouse and pad): rank (its colour), land, the modifiers
  (one line each), the break state ("Stable · breaks in 4 d 6 h", "Cracking", "Spilling", "A run is inside"), "Owner:
  <name>" on key portals, your party that will come (everyone within 10 m), **Enter** / **Back**. A key portal of
  another party says "Sealed to <name>'s party" and the Enter button is off with that reason (never blank).
- **Party entry**: Enter asks everyone else within 10 m (a 5 s accept card: "Enter the Rare fold with <name>? A / click
  to come, B / Esc to stay", a bar running down). Those who accept go with you; a run's roster is everyone who went in.
- **Joining later**: anyone the portal admits (wild: anyone; key: the owner's party/clan/band) can enter while the run
  is open, until the boss door seals. After that: "The boss door is sealed: wait".
- **Bays**: up to 8 runs at once (server setting `portalRuns`); a ninth: "All folds are full: one frees in <time>"
  (the soonest run's elapsed vs typical length is unknown, so the message shows "when a run ends" with the count).
- **Inside**: you arrive at the **entry stone** (a carved standing stone with the rank colour in its rune; it's the
  run-back point and the way out: Interact "Leave the fold" puts you back outside at the tear; the run goes on).
- **Doors**: combat and puzzle rooms' exit doors are shut (a stone slab in its slot) until the room is cleared /
  solved: then a stone grind and a bar dropping, the slab slides up into the lintel over 2 s (continuous, on its
  slot). The boss door is open until the first player crosses its threshold; then it drops behind them (sealed).
- **Room activation**: a room's creatures are made when a player first comes within 25 m of its entrance (they're there
  waiting, not popping in view: the room's far half). Traps run only while a player is within 40 m.
- **Death**: server rules (the penalty and the bag as set). You wake at your bed / camp outside. While the portal is
  open you can walk back in and run from the entry stone; your bag lies where you fell.
- **Clear**: the boss dies → its roll to everyone at 10%, the shard, the 2-minute loot window ("The fold closes in 2:00"),
  then everyone inside is put back at the tear, every death bag still inside is moved to the tear, the fold is torn
  down, the portal closes and leaves a scorched ring for a game day. Leaving everyone dead or gone does **not** close a
  run: it stays as it is (creatures where they are) until cleared, until the portal breaks or, for a run nobody has been
  in for 30 real minutes, it resets (the fold torn down, the portal stays with its clock).
- **The fold map** (P24): the journal map, while you're in a fold, draws only the rooms you've walked into (outlines,
  doors, the entry stone and you), not the island. No markers for chests or exits.

### 2.6 Portal creatures and bosses (P6, P23, B1)
**Creature strength by rank** (KEPT): HP × and damage × applied per creature: 1: lowland ×0.6 HP / ×0.6 hit for midland
kinds (lowland kinds as they are); 2: midland as they are; 3: ×1.25 HP; 4: highland ×1.0 (midland kinds ×1.65 HP ×1.6
hit); 5: ×1.6 HP, ×1.3 hit, stance ×1.3 on top of rank 4. Packs per room by rank (1-3 / 2-4 / 2-5 / 2-5 / 3-6 creatures).

**Kit tables**: Volcanic: Cinderfang, Kilnmite, **Anvil mites** (Kilnmite young, 0.55 size, 25 HP, ×6 swarm), Magmaw
(rank 4-5), Scorchwing (big rooms). Tundra: Gloomhowl, Rimeclaw (rank 3+), Glacimaw (rank 4+, big rooms), Rimeshell,
**Rime-calf** (a Woolly drudge calf of the old herds at 0.45 size, ice-white, aggressive, horn charge). The Slag thrall
and the Rimecrawler wait for their sculpts (logged).

**Bosses** (endgame 2.5.6, attacks as data rows, mechanic in code). The arena is the boss room of the kit; its props
are the mechanic.

| Rank | Volcanic | Body (built rig) | Mechanic built | Tundra | Body | Mechanic built |
|---|---|---|---|---|---|---|
| 1 | Cinderfang Alpha | Cinderfang ×1.5, rank-colour glow | Mane lit at the arena's lava runnels: ×1.2 damage; a frost hit or thrown water jar douses it 15 s; *howl* calls 2 Cinderfangs | Rimeclaw Alpha | Rimeclaw ×1.5 | Ice plates: stance damage ×0.5 until fire melts them (20 s); frost roar slows |
| 2 | Kilnmite Broodqueen | Kilnmite ×3 | Three egg clutches (200 HP each) on hot rock; while any stands she lays 4 mites every 20 s | Glacimaw Elder | Glacimaw ×1.6 | Burrow: cracks show its path; erupting under a player standing on the dark thick ice stuns it 3 s |
| 3 | The Mould-Born | Obsidian Warden rig, iron casting tint | Frost hardens it (stance ×1.5, damage ×0.8), fire heals 1% a hit; two hanging crucibles: Interact the chain 1.5 s to tip one: 600 + 4 s stagger if it's under | The Frostbound Thurs | Grimjaw rig, blue-white | Regrows an ice shell (×0.4 taken) every 25 s unless burning or bleeding |
| 4 | Brannoch, the Bellows-Keeper | Grimjaw rig, soot and bronze | Two bellows vents (lock points, 800 HP each): one broken halves the cone; both: no cone, staggered 6 s | The Rime-Witch | Ignessa rig, skating low | Braziers: standing in a lit brazier's light stops her freeze; she snuffs one every 25 s; relight 2 s |
| 5 | Böðvildr, the Smith's Daughter | Vulkarn rig ×1.2, ash-grey, ember cracks | Three quench tanks: her overhead slam on a tank's rim tips it: steam staggers her 8 s and cools her (×1.3 taken 12 s); tanks refill in 60 s from the cistern pipe | Hrímgerðr, the Glacier's Daughter | Ignessa rig ×5, grounded | The dawn window: 4 frost bolts (600 HP each) on the east ice wall; each broken lets in a dawn shaft: while she stands in it she's slowed ×0.7 and takes ×1.5 |

Rank 4-5 bodies borrow built rigs (as Cinderhorn and Old Tusker do); their own sculpts are logged.

### 2.7 Modifiers (P4)
The ten rows (KEPT): No healing (potions and heals do nothing; lifesteal works), Double damage (both ways ×2), Darkness
(light radius halved), Iron hide (creatures have hyperarmor on every attack), Frenzy (creatures ×1.15 move and attack
speed), Venomous (creature status buildup ×1.5), Brittle gear (durability ×3), No tames (tames wait at the tear),
Bloodied (enter at 50% HP), Swarming (+50% creatures per room). Count by rank 0 / 1 / 1-2 / 2 / 3, rolled when the portal
opens (shown on its card), never two the same. Each adds +25% loot quantity.

### 2.8 Keys (P12, P13)
Items `portal_key_<biome>_<rank>` (10 now: volcanic and tundra), made at the **alchemy table**: rank 1 = 3 Common shards
+ the signature ×2; rank n = 3 shards of rank n−1 + 1 of rank n + signature ×2n. Signature: volcanic **sulfur crystal**,
tundra **lichen** (the lands' signature resources in `world.json biomes.<land>.signature`). Use (from the bag or a quick slot): on open
ground in its land (the same placement rules, 30 m from you), the tear opens in front of you over 3 s. It belongs to the
opener, their clan and band; it never breaks; it can't open a rank whose boss is loose in that land.

### 2.9 Rewards (P17-P19)
Endgame 2.5.4 table, KEPT: chests personal (each player opens their own copy: the chest shows open only for you),
boss roll per player at 10%, rank shard to everyone at 10%, upgrade shards rank 2+, blueprints by `gear.json portalOdds`
(BUILT `FWRBlueprints::PortalDrop`), artifacts at rank 3 / 4 / 5 = 3 / 8 / 15% (+1 point per party AL), the boss's
trophy (decor). Quantities × (1 + 0.25 × modifiers) × (1 + 0.05 × party AL). No currency.

### 2.10 Server settings (P7, P21, A12)
`portalBreak` (half / normal / double / never), `portalRuns` (1-8), `artifactCap` (off / 5 / 10 / 15), `artifactSlots`
(0-3): in every preset and on the world settings screen (casual co-op: cap 10). "Off" also closes the hidden phase 3s
(the row says so).

### 2.11 Sound and music (P22, P25)
Rows in `Data/Audio/sounds.json` (CC0 recordings already shipped, reused and pitched): the hum per rank, the tear
opening, the break roar, the warden's arrival, door grind + bar drop, grate hiss, chute bell + pour, icicle crack, ice
creak, quench hiss, bellows breath, hammer beat, brazier light / douse, prism turn, chest open, entry stone hum, the
legendary horn. Inside a fold the land is the fold's own (WRBiome::At), so its music plays as in the land (ducking as
the director does); the ambience is the cave bed (closed stone), never the bay's high wind. The "through stone" reverb
on the music is not built (section 4).

### 2.12 Artifacts (A1-A9, A13)
Kept as endgame 2.6: charms on three belt hooks; Weight I-V by metal (bone, bronze, silver, blackened iron, starsteel);
**AL** = sum of Weights (0-15, capped by the server's `artifactCap`). Per AL: damage taken from creatures, bosses and
the fold's traps +7%; loot +5%; path XP +4% (inside the ×2 cap); boss part chance +1 point; dropped gear quality step
+2% per AL; artifact drop chance +1 point. AL 5+: a dropped boss part is Scarred 25% (×1.5); AL 10+: artifacts drop at
III-V. **Party** = you + clanmates + your band, counted where it matters: in a fold, the run's roster inside; at a boss,
those in its chase radius; elsewhere, those within 60 m. The party's **highest** AL is used for the shared penalty and
rewards; weird rules only on the wearer. **PvE only**: dealing or taking player damage pauses all of your artifact
effects 30 s (the charms dim to grey). **Hooking** a charm takes 2 s standing (the hero ties it on), out of combat,
never within 90 m of an engaged boss, never inside a fold; artifacts pause in safe zones. **Hidden phase 3** opens when
the party with the largest share of the boss's ledger has AL ≥ 1 (and the server's cap isn't off).

**The band** (A13): on the journal's **Artifacts** page (key N), under YOUR BAND: the players online with "Ask into
band", invites to you with Join / Decline, and Leave. No size cap. (It lives with the artifacts because the band's only
job is sharing what charms give and take; the clan stays the clan.)

### 2.13 Getting artifacts (A2, A10, A11)
Major boss kill: 8% + 1 point per AL (hidden phase 3 kill ×1.5), weights by party AL (I/II/III at 60/30/10 below AL 5,
II/III/IV 5-9, III/IV/V 10+). The first kill of your 4th distinct major: that boss's artifact at I, guaranteed. Portal
ranks 3/4/5: 3/8/15% of that land's portal artifact (rank 3 I 70/II 30; rank 4 II 70/III 30; rank 5 III 60/IV 30/V 10).
They fly to you as the first-kill drop does (the gold light is yours). **Geis records**: your highest AL per major; a
new highest pays 300 XP × AL to your main path; first at AL 5+ a guaranteed Scarred part; first at AL 15 the title
"<Boss>'s Bane". **Fusing** at the alchemy table: two of the same at n → one at n+1 + the signature ×5n; **salvage**
gives that land's path essence ×2 per Weight.

### 2.14 The thirty weird rules (A8)
Each is one hook, wearer only (endgame 2.6.2 table, KEPT word for word in `Data/artifacts.json`). Every rule is built
where its system exists; rules whose system isn't in the game yet (Rod-crown needs storms in its land; Wardcarver's Chime
needs spells to carry; Lonely Tide needs tames, built) are listed in section 4 if not live. Drop sources outside Fire and
Frost (other lands' majors, their portals, the raids) don't exist yet, so those artifacts can't be found until they do;
their rows are complete.

### 2.15 Screens (B2)
- **Portal card** (`portal`): the frame of the world screens; rank swatch, land, state, modifiers list, party list,
  Enter (primary) / Back. Pad: A enter, B back; mouse: buttons.
- **Party accept** (`portalinvite`): a small card with a 5 s bar, Come / Stay buttons.
- **Artifact belt** (`artifacts`, from the Inventory's Artifacts button and the alchemy table): three hooks (slot cards
  with the charm's metal colour, name, Weight, geis line), AL and the party's AL with what it does now, the pouch list
  (Hook / Unhook, 2 s with a bar), at the alchemy table Fuse and Salvage buttons; geis records list (boss, highest AL).
- **Clan tab BAND section**.
- **HUD**: in a fold, a top line "Rare fold · Volcanic · No healing" and, after the boss, the close count; AL badge
  next to the health bar when ≥ 1 (dim grey while paused).

---

## 3. Plan: build steps (in order)

| # | Step | Files |
|---|---|---|
| S1 | Data: `Data/portals.json` (ranks, slots by biome for all 12, rules, escalation, entry, modifiers, rewards, keys, layouts per biome per rank), `Data/portal_rooms/volcanic.json` and `tundra.json` (32 rooms each, creature tables, boss row ids), `Data/artifacts.json` (30 artifacts, AL numbers, drop rules, fuse/salvage) | Data |
| S2 | Data rows: items (5 shards, 10 keys, 10 trophies, blank/held items), recipes (10 keys at alchemy), creatures (10 bosses, anvil_mite, rime_calf), attacks.json (10 bosses' rows), tuning (`bosses.<id>` health/stance; `portals.*` look numbers), sounds/fx rows, server presets (4 settings) | Data |
| S3 | `WRPortals.h`: hooks (`WRPortals::` namespace), `AWRPortal`, `AWRFold`, `AWRFoldDoor`, `AWRFoldTrap`, `AWRFoldPuzzle`, `AWRFoldChest`, `AWRFoldStone`, `UWRPortalSystem` | Source |
| S4 | `WRPortals.cpp`: the system: data, slots, placement, clocks, escalation, escaped bosses, keys, runs/bays, entry, invites, clear, close, bodies, rewards, save/load, commands, dev | Source |
| S5 | `WRPortalSites.cpp`: the portal's look and states; the fold's build (rooms, corridors, features, lights), doors, entry stone, chests | Source |
| S6 | `WRPortalTraps.cpp`: four traps, four puzzles | Source |
| S7 | `WRPortalBosses.cpp`: the ten bosses' arena props and mechanics (a brain component on the boss, server) + damage hooks | Source |
| S8 | `WRArtifacts.h/.cpp`: data, `UWRArtifactComponent` on the PlayerState (pouch, hooks, records, pause, band), AL and party AL, the shared effects, the 30 rules' query, drops, fusing, salvage, save | Source |
| S9 | Hooks in existing code: ApplyHit (damage taken, double damage, rules), Die (portal kills, escaped boss, artifact drops), boss phase 3 rule, boss loot (quantity, part chance), path XP, potions/heals (no healing, Leech Tooth, Seed-heart, Open Vein), roll (Wyrm's Stillness), block (Thorn Oath, Glass Guard), light radius (Darkness, Mourner's Veil), WRBiome::At / IsUnderground (fold), interact/prompt, commands, player save, respawn, HUD lines | Source |
| S10 | `WRUIPortals.cpp`: portal card, accept card, artifact belt; Clan tab band; HUD lines; fold map | Source |
| S11 | Server settings rows in the world settings screen | Source |
| S12 | Compile (editor), fix every warning; CHANGELOG + HANDOFF; commit, push | — |

---

## 4. What is NOT finished (kept hidden or plainly not there)

- **Ten lands.** Portals open only in the Volcanic and the Frozen tundra (the only lands in the world). The other ten
  lands' rows are in `Data/portals.json` (`land: false`: no slots, no keys) and their room kits, traps, puzzles, portal
  creatures and 50 bosses (endgame 2.5.5 / 2.5.6) wait for their lands. Their portal artifacts can't drop until then.
- **Bodies.** Ranks 3-5 wear built rigs with an overlay in their colours (the Mould-Born on the Obsidian Warden, Brannoch
  and the Thurs on Grimjaw, Böðvildr on Vulkarn, the Rime-Witch and Hrímgerðr on Ignessa, wings and all); they need
  their own sculpts (Böðvildr and Hrímgerðr as giantesses, the Rime-Witch wingless). The Slag thrall and the
  Rimecrawler (portal-only) aren't made; the kits use the lands' creatures, Anvil mites and Rime-calves.
- **Voices.** The new creatures speak with pitched shared calls (`sounds.json creatures`), not their own fused recordings.
- **The Rime-Witch's mirror clones** aren't built (her move list has no clone move); **the Legendary's Crown pillar
  flicker** waits for the Crown; **the "through stone" music reverb** isn't built.
- **Connla's challenge**: creatures see you from ten times as far, but bosses don't chase twice as far yet. **Connla's
  third geis**: no ability heals another player in this game, so only the nameplate part would apply; it isn't wired.
  Both are raid artifacts that can't drop until raids exist.
- **Artifacts that can drop now**: Forge-brand (Vulkarn), Wyrm's Stillness (Skaldrith), Bellows Lung (Volcanic folds),
  Rime Ring (Tundra folds). The onboarding artifact needs a fourth great boss, which the two-boss island doesn't have.
- **Spilled packs and escaped bosses' bodies** aren't saved over a server restart (the escaped boss's record is: it comes
  back where it roams when someone comes near; the clock and the stage are saved).
- Built and compiled, not played (Aaron and the testers test).

## 5. Spot-check (read against the plan and THE BAR)

Read through after the build and fixed before the push:
- Clients compared server times (portal opening, the fold's close count, artifact pause / hooking) with their own clock:
  all now read the server's (`PServerNow`, `ArtNow`).
- Puzzle parts linked to their controller only on the server: the link and the answers (`Link`, `Seq`) replicate now, so
  every machine shows the right verbs, the hammer's rhythm and the aurora's bands.
- Doors, thin ice, clutches and frost bolts toggled collision only on the server: every machine follows the state now
  (`ShowState`), so a client never runs into an open door.
- A fold is sent to whoever is about to come in (its roster), so it's built on their machine during the fade.
- Bosses would have put a run-back stone 50 m out (in another room): portal bosses don't; the entry stone is the run-back.
- The portal plinth would perch on slopes: its slabs now run 1.8 m into the ground.
- A Legendary coming back with the world isn't announced again; spilled packs are capped at 14 alive per portal.
- In a fold the ambience was the bay's high wind: the audio director now uses the fold's land and the cave bed.
- Helper names that could meet others in a full unity build were made unique; full unity compiles clean.
- THE BAR: every trap tells at least 0.9 s ahead (grates 1.2 s glow and hiss, chute bell 1.0 s, icicles 1.0 s crack and
  growing shadow, thin ice 1.0 s creak and cracks); nothing homes; all damage goes through ApplyHit; there's no cap on
  attackers; screens are buttons (mouse and pad).

## 6. Ledger proof (file:line in C:\Dev\Wildreach\Source\Wildreach unless said)

| # | Done | Where |
|---|---|---|
| L1 | Yes | This doc (1 research, 2 design, 3 plan, 5 spot-check); the build is the commit |
| L2 | Yes | Section 1 (Valheim, Diablo III/IV, PoE, Solo Leveling, Elden Ring, BotW, Hades, Halo, Monster Hunter / Destiny, the geis, temper colours, bellows, sea ice, the qulliq, the sun's path) |
| L3 | Yes | Sections 2 and 3 |
| L4 | Yes | 2.3 (why each room exists), 2.4 (what drives each trap and puzzle); drawn so in WRPortalFold.cpp BuildRoom 556 and WRPortalThings.cpp BuildLook 698 |
| L5 | Yes | Doors slide up 2 s into the lintel on every machine (WRPortalThings.cpp Tick, the door branch); the chute's lip on its rope over a pulley to a wall cleat; the anvil block scrapes at an even pace (StepServer, block); the hammer automaton on its cam shaft beats the rhythm; the crucible's haul chain runs over a pulley to a floor ring clear of the pour |
| L6 | Yes | This table |
| L7 | Yes | Data/portals.json has all 12 lands; nothing left for anyone else to decide |
| P1 | Yes | WRPortals.cpp StepSlots 936, FindSpot 895 (each land's band, its own ground) |
| P2 | Yes | Data/portals.json ranks (colours, bands); the pillar colour in WRPortalFold.cpp BuildLook 212 |
| P3 | Yes | Data/portal_rooms/volcanic.json, tundra.json (33 + 32 handmade rooms, 20 graphs each); WRPortals.cpp BuildFold 1241 |
| P4 | Yes | Data/portals.json modifiers (10); rolled in OpenPortal 972; shown on the card (WRUIPortals.cpp SPortalCard); applied in DamageScale 361 and the WRPortals hooks (heal, light, wear, pace, iron hide, buildup), Bloodied and No tames in PutInside 1404, Swarming in WakeRoom 1533; +25% loot each |
| P5 | Yes | WRPortalThings.cpp: grates 480, chutes 447, icicles 441, thin ice 579; quench / bellows / braziers / prisms in Interact 231 |
| P6 | Yes | WRPortals.cpp SpawnBoss; WRPortalBosses.cpp (Spawned 71, Incoming 133, OnAttack 194, TickComponent 266); Data/attacks.json bosses rows |
| P7 | Yes | OpenPortal 972 (breakDays × the server's portalBreak); StepPortal 1048 |
| P8 | Yes | StepPortal 1048 (cracking 50%, spilling 80%), SpillPack 1099 (packs, the warden) |
| P9 | Yes | Break 1140 (the boss walks out, the portal closes); the escaped boss roams 400 m (WRPortalBosses.cpp TickComponent, bEscaped) with no raid route |
| P10 | Yes | RankBlocked 925, used in StepSlots and UseKey 614; EscapedDied 1174 frees it |
| P11 | Yes | StepPortal holds the clock while a run is open; BossDied 1602 ends the break |
| P12 | Yes | Data/items.json portal_key_*, Data/recipes.json (alchemy table); WRPortals::UseKey 614; WRInventory.cpp UseSlot (portal_key) |
| P13 | Yes | AWRPortal::IsSealedTo and Allowed (owner, clan, band; StepPortal); wild portals open to all |
| P14 | Yes | OpenPortal 1018 → Announce 1023 (every player's banner and chat line) |
| P15 | Yes | Death follows the server's penalty (untouched); re-entry through the portal while it's open (Enter 1354); the entry stone (Leave 1454) |
| P16 | Yes | TearDownFold 1685 (bags moved to the entrance) |
| P17 | Yes | Rewards are items only (Data/portals.json rewards); no currency; any party size |
| P18 | Yes | Reward 1621 (the rank shard to everyone at 10%) |
| P19 | Yes | Reward 1621 (boss roll, upgrade shards, blueprints, artifacts 3/8/15%); personal chests (WRPortalThings.cpp Interact) |
| P20 | Yes | Enter 1354, Accept 1383, PutInside 1404; the accept card (WRUIPortals.cpp SPortalInvite); joining until the boss door seals (EnterBlock 526, StepRuns 1467) |
| P21 | Yes | EnterBlock 526 (server portalRuns, at most 8 bays) |
| P22 | Yes | WRPortalFold.cpp AWRPortal::BuildLook 212 (ring, tear, pillar 60 m / 300 m, hum by rank) and Tick (states) |
| P23 | Yes | Data/portals.json ranks.creatures; PsScaleFor / PsSpawnScaled; WRTaming.cpp RecomputeStats and WRCreature.cpp BeginPlay use PortalHealth / PortalDamage; MaxStanceFor uses PortalStance |
| P24 | Yes | WRUIMap.cpp 419 (the fold map: rooms you've walked into); WRPortals::SeenRooms |
| P25 | Partly | The land's music plays in its fold, the cave bed under it (WRAudioDirector.cpp); the "through stone" reverb isn't built (section 4) |
| A1 | Yes | WRArtifacts.h Hooks (3); server artifactSlots |
| A2 | Yes | WRArtifacts.cpp OnBossKilled 769 (majors), PortalDrop 837 (folds rank 3+) |
| A3 | Yes | DamageTakenScale 594 (in ApplyHit); the weird rules (A8) |
| A4 | Yes | LootScale (WRBoss.cpp 1205, portal rewards and chests), PartChanceBonus (WRBoss.cpp 1233), RollScarred, RollQualityStep (portal gear), XPScale (WRPathComponent.cpp 281), drop chance +1 point, high Weights at AL 10 |
| A5 | Yes | PartyAL 570 (the highest in the party); rules read only the wearer (Has) |
| A6 | Yes | OnHitLanded 661 → NotePvP 638 (30 s pause); safe zones (IsPaused 196); DamageTakenScale skips players |
| A7 | Yes | PhaseThreeOpen 615 (WRBoss.cpp 466) |
| A8 | Yes, two raid rules partly | Data/artifacts.json (30); WRAbilities.cpp 1351 (no roll, never turn back), 1517 (no block), 1534 (short parry); WRInventory.cpp 1030 / 1058 / 1192 (potions); WRStatusComponent.cpp 773 (regen); WRBossCombat.cpp 135 (Omen), 443 (Gale Feather); WRCreatureCombat.cpp 742; WRUIHud.cpp 500 / 1025 and WRHUD.cpp 420 (bars); WRCreature.cpp 2319 (challenge), 3070 (Lonely Tide); WRSurvival.cpp 596 / 605 and WRHeldLight.cpp 89 (light); WRArtifacts.cpp StepRules 349 (Kite String, Rod-crown, sunwise, the Hag's cup), OnHitLanded 661 (Forge-brand, Rime Ring, Hollow Coin, Gale Feather's knockdown), OnKilled 707 (Hive Seed, The Feather, Grave Count), OnAbilityUsed 867 (Blood-price, Wardcarver's Chime), BlocksHit 647, HealFilter 854 (Leech Tooth, Open Vein); section 4 for Connla's challenge and third geis |
| A9 | Yes | Hook 239 / Unhook (2 s, not in a fight, not near an engaged boss, not in a fold); the tick completes it |
| A10 | Yes | OnBossKilled 769 (records, XP, Scarred part, the Bane title) |
| A11 | Yes | Fuse 273, Salvage 296 (alchemy table); the Artifacts page's buttons |
| A12 | Yes | Data/server_presets.json; WRWorlds.h / .cpp; WRUISettings.cpp (artifact cap, hooks, portal break timers, runs) |
| A13 | Yes | WRArtifacts::Command 918 (band invite / accept / decline / leave), TickBands; the Artifacts page |
| B1 | Yes | Section 5 |
| B2 | Yes | WRUIPortals.cpp (buttons only); the journal tab (N; the pad's LB / RB) |
| B3 | Yes | Data/portals.json, portal_rooms, artifacts.json, tuning bosses rows, attacks.json rows, sounds / fx rows; damage only through ApplyHit |
| B4 | Yes | Section 4; land-false rows give no slots or keys |
