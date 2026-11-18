# build-wild-strongholds: the NPC strongholds as Elden Ring forts

Lane `build-wild-strongholds` (2026-11-16). Aaron 11-16 changed q-stronghold-garrison (was "Raven needs to give
direction"): **"Yes this is fine and good, make the NPC strongholds designed like elden ring areas, with small, medium,
and boss enemies."** Builds on r-siege-pve (wild-answers-1115 2.7: Emberwatch + Rimehold, built 0.8.x in
`WRStronghold.cpp`).

## 0. REQUIREMENT LEDGER

| # | Requirement (one each) | Source |
|---|---|---|
| R1 | The NPC strongholds are designed like Elden Ring forts/castles/legacy dungeons | Aaron 11-16 q-stronghold-garrison, brief |
| R2 | Research Stormveil (layered approach), Castle Morne, Fort Haight / Faroth, Caria Manor, Raya Lucaria; write the specific lessons | brief, cook step 1 |
| R3 | Gates and walls | brief |
| R4 | Side paths (a second way in that isn't the gate) | brief |
| R5 | Ladders / lifts | brief |
| R6 | Vantage points (seen from, and seen into) | brief |
| R7 | Shortcuts that open back (one-way, opened from inside) | brief |
| R8 | A courtyard | brief |
| R9 | A keep | brief |
| R10 | A boss arena at the heart | brief |
| R11 | Each stronghold has a deliberate layout with clear intent (why each piece is there, what it touches) | brief, DESIGN WITH INTENTION |
| R12 | Garrison tier 1, small: patrols, archers, dogs | Aaron, brief |
| R13 | Garrison tier 2, medium: knights / captains / elites with real move sets | Aaron, brief |
| R14 | Garrison tier 3: a BOSS with an arena and its own phases | Aaron, brief |
| R15 | The garrison is placed for the siege AND for on-foot play | brief |
| R16 | Loot that fits (each tier, the boss, the war chest) | brief |
| R17 | Respawn rules that suit a survival game | brief |
| R18 | Readable on approach (silhouette, landmark, what's where) | brief |
| R19 | Use the project's biome kits so each fits its land | brief |
| R20 | Covers "the strongholds players practise sieging" AND any existing NPC forts/camps | brief |
| R21 | Siege still works: every piece breakable by ram/trebuchet/charge, PvE rules kept (r-siege-pve) | wild-answers-1115 R17-R18 |
| R22 | Combat per COMBAT-AND-LESSONS: attacks as data rows (windup, punish, parryable), projectiles >= 0.9 s warning, never home, no attacker limit, stance/poise tiers, phase transition = attack, never on a crit | CAL 1.13-1.14, THE BAR |
| R23 | Data-driven: numbers in Data/*.json + tuning.json; all damage through ApplyHit; server authority; replication | THE BAR |
| R24 | Multiplayer: server authority, every client sees the same fort, doors, lifts, boss state; join-in-progress | THE BAR |
| R25 | Story told through the environment only (no staged scenes) | THE BAR |
| R26 | Nothing unfinished shown as finished | THE BAR |
| R27 | Machines look engineered: lifts on a believable mechanism, move continuously, never snap | DESIGN WITH INTENTION |
| R28 | Full game scope (12 biomes) considered: the system takes a fort per land as data | Aaron 11-13 scope |
| R29 | CHANGELOG Unreleased + HANDOFF in the same commit; compile gate only; LFS size reported | lane rules |
| R30 | LANDED.md line + LAPTOP-SAYS line | lane rules |

(Proof column filled in section 4 at the end.)

## 1. RESEARCH

### 1.1 Elden Ring's forts and legacy dungeons: what each teaches

**Stormveil Castle (Limgrave, the first legacy dungeon).** The approach is layered: Gatefront ruins (a camp in the
open), Margit at the bridge, then the castle face. The **main gate is a killing field**: a portcullis that lifts with a
grinding winch, a straight ramp, a battery of ballistae and archers on the battlements, soldiers pouring out. The
**side path** goes along the cliff, past a hut and a rampart tower, through the crumbling outer wall: slower, full of
hounds and ambushes, but out of the ballistae's line. Once inside, **the main gate can be opened from inside**, so the
hard way back becomes a short way. Lifts (the Liftside Chamber), ladders up rampart towers, walkways along the wall
top. Elites stand at chokepoints you can see coming (the Banished Knights in the courtyard, the Grafted Scion behind a
gate); the courtyard is a crossroads that sees every level above it. Godrick waits at the **top and heart**, behind a
fog gate, on a wind-swept rampart: phase 2 at half health starts with a transition (he grafts the dragon head), then
fire breath and a new rhythm. Sources: [Wikipedia: Stormveil Castle](https://en.wikipedia.org/wiki/Stormveil_Castle),
[Gamerguides walkthrough](https://www.gamerguides.com/elden-ring/guide/stormveil-castle/walkthrough),
[Game Informer preview](https://gameinformer.com/preview/2021/08/27/elden-rings-first-legacy-dungeon-is-stormveil-castle),
[doublexp: main gate or side path](https://doublexp.com/guides/should-you-use-stormveil-castles-main-gate-or-take-the-side-path-in-elden-ring).

Lessons: (a) **two ways in, and the defenders cover the obvious one hardest**; (b) **the gate opens from inside**
once you get round; (c) **elites guard chokepoints that are visible before you commit**; (d) **the boss is at the
heart, reached last, through a doorway that says "boss here"**; (e) phase 2 is a transition move + a new rhythm.

**Castle Morne (Weeping Peninsula).** A fort fallen to its own servants (the Misbegotten): the story is the set (burnt
bodies, servants rioting in the yard). A cliff road to the gate, a courtyard, the Castle Morne lift down to the
rampart, the boss (Leonine Misbegotten) on a **cliff-edge rampart** where falling kills you, a summon sign beside the
boss gate. Sources: [PowerPyx Weeping Peninsula](https://www.powerpyx.com/elden-ring-weeping-peninsula-walkthrough/),
[Push Square: Castle Morne](https://pushsquare.com/guides/elden-ring-how-to-complete-castle-morne),
[Fextralife: Castle Morne](https://eldenring.wiki.fextralife.com/Castle+Morne).
Lessons: **the occupiers aren't the builders** (the set says who built it and who holds it now); **the arena has an
edge or hazard you use and fear**; a lift is a slow ride that is itself a set-piece.

**Fort Haight (Limgrave) / Fort Faroth (Caelid).** The small forts: our scale. Haight: the gate is rubble, a lower
yard with soldiers, **stairs and a ladder up the ramparts**, a crossbowman covering the yard, and a **Godrick Knight on
the top floor guarding the chest** (the Dectus Medallion half). Faroth: the same fort abandoned to bats, a giant bat
dropping from the ceiling. Sources: [Gamerguides: Fort Haight](https://www.gamerguides.com/elden-ring/guide/limgrave/locations/how-to-find-and-complete-fort-haight-in-limgrave),
[WePC Fort Haight](https://www.wepc.com/gaming/elden-ring/fort-haight-guide/),
[RPG Site: Dectus Medallions](https://www.rpgsite.net/feature/12467-elden-ring-grand-lift-of-dectus-how-to-get-the-medallions-to-operate-the-lift-or-skip-it-entirely).
Lessons: **a small fort is a vertical loop** (yard, stairs, wall walk, tower ladder, the top room with the prize);
**the knight guards the treasure**; **ranged defenders sit high and cover the yard**, climbing to them is the counter.

**Caria Manor (Liurnia).** A walled estate climbed in **terraces**, each a short arena (spectral hands, troll
knights), a side staircase, and **Royal Knight Loretta in an open moonlit garden at the very top**, a framed stage with
the manor front as backdrop. Lessons: **terraces = escalation you can see above you**; **the boss stage is a framed
set, not a room**.

**Raya Lucaria Academy (Liurnia).** A key to get in, a mid-dungeon boss (the Red Wolf of Radagon), **lifts and ladders
that loop back** to the first grace, Rennala at the heart with two phases. Its towers are Liurnia's landmark.
Lessons: **readable from far** (towers, a beacon); **a mid boss before the main one**; **loop back before the boss**
so a death doesn't cost the whole climb.

### 1.2 Other games that solve the "fort full of soldiers" loop
- **Ghost of Tsushima (Mongol camps/forts):** a **horn-blower** (kill him first or he calls the rest), archers in
  towers, a **leader** you can see, dogs that smell you. The camp stays liberated for a while.
- **Far Cry outposts:** the **alarm** is the design: undetected = easy, alarmed = reinforcements.
- **Valheim Fuling villages:** wooden walls, a shaman (caster), a berserker (elite), chests; structures don't regrow.
- **Dark Souls (Undead Burg, Sen's Fortress):** **shortcuts open only from the far side** ("It doesn't open from this
  side"); enemies come back on rest, bosses never.
- **Mount & Blade sieges:** archers on the walls make the engines' range matter; the garrison sallies when the gate
  breaks; the lord fights in the keep.
- **Conan Exiles:** NPC camps whose chests refill on a timer; named bosses respawn on a timer.

### 1.3 Real-world reference
- **Motte-and-bailey (11th-12th c.):** a bailey (yard) in a timber palisade with a gatehouse; the motte (raised mound)
  carries the keep. **Towers flank the gate** so its approach is shot from two sides. A **postern** (small back door)
  lets the garrison out: our side path.
- **Wall walk:** a timber fighting platform ~1.5-2 m below the palisade tips, reached by ladders.
- **Keep:** the last refuge, entered at first-floor level up an outside stair; the strongest door.
- **Watch beacon:** a fire basket on the highest tower, seen across the land: readability on approach.
- **Whaling stations (Rimehold):** flensing decks, try-pots, whale-rib arches, bone fences.
- **Smithies (Emberwatch):** anvils, quench troughs, slag heaps, chimney stacks.

### 1.4 What the code already has (survey, file:line)
- Forts today: `WRStronghold.cpp` (16-segment palisade ring, 2 gate towers with bolt turrets, a 2x2 keep, war chest,
  beasts in the yard), sited by `Tools/terrain/strongholds.mjs`. No humans, no layering, no boss.
- Humans on the hero body exist only as `AWRWorldNpc : AWRCharacter` (`WRWorldSystems.h:394`): MetaHuman rig, real
  clips, armour looks (`SetWornArmor`), own ASC, AI controller; they never fight.
- The survivor's real move sets are data (`Data/weapons.json` classes: sword, axe, spear, greatsword, great_hammer,
  scythe, dagger, each with light chains, heavy, charged, riposte...) run by `UWRGA_Melee` on any `AWRCharacterBase`
  with an ASC; `AWRCharacter::PressAbilityInput` drives them ("used by real input and by the test bot",
  `WRCharacter.h:99`). Bows: `WRRanged.cpp` (aim pose, `LaunchProjectile` with drop, never homes).
- `FWRWeapons::ClassFor` reads the class only from an inventory (`WRWeapons.cpp:257`): a body with no inventory
  swings a sword whatever it holds.
- Ladders: `AWRSiegeLadder` (`WRRaiding.h:127`), climbed by walking into it, dies with the wall it leans on.
- Biome kits: `Data/building_sets.json` tiers `basalt_hall` (volcanic, "columns", ember glow) and `rime_lodge`
  (tundra, "logs") expand to `<tier>_<shape>` pieces drawn by `WRBuildLook`, plus each set's decor.
- Boss bar: `WRUIHud.cpp:713-770` reads only `AWRBoss`. Run-back stone: `AWRRunbackStone` (`WRBoss.h:130`).
- Doors: `AWRBuildPiece::SwingFor` (`WRBuilding.cpp:667`), hold doors refused at `WRBuilding.cpp:1323`.
- No existing NPC fort or camp holds enemies: island "camps" are player camps (safe zones); `ringfort`,
  `rootgrave_fort`, `stormgut_fort`, `hollow_fort` are scenery in other lands; shrine guards are beasts in a ring.

## 2. DESIGN (plan mode)

### 2.1 The big decisions
1. **The garrison are people** (Stormveil, Haight, Morne: forts are held by soldiers; Aaron's tiers are soldier words:
   patrols, archers, knights, captains). They are built on the hero's own body (`AWRGarrison : AWRCharacter`), so they
   are real, attractive people in real armour, use the survivor's real clips and **the survivor's real move sets**
   (light chains, heavies, charged heavies, guard, dodge, the bow), and every hit goes through `ApplyHit` with the
   full hit stack. "Real move sets" with no new clips.
2. **Dogs are the land's beasts kept as war-hounds** (cinderfangs at Emberwatch, gloomhowls at Rimehold), tagged to
   the fort so they never fight its garrison.
3. **The boss is the fort's warlord**, a person on the hero body with a great weapon: its class's move set plus its
   own data rows (leaps, kicks, a thrown harpoon, bursts) and a phase change at 50% that is itself an attack (CAL 1.14).
4. **Layered like Stormveil, sized like Fort Haight**: an outpost on the approach, a gate front covered by turrets and
   archers, a breach on the far side (the side path), a bailey yard with a wall walk on ladders, a two-storey keep
   entered up an outside stair (its ground door barred: the siege door), and the inner ward behind the keep: the boss
   arena at the heart, with a postern that opens from inside back to the run-back stone (the shortcut).
5. **Siege still works on every piece** (r-siege-pve): wood bailey (ram, charges, fire), biome-stone keep and ward
   (trebuchet). Engines stand off from the archers; a broken gate or keep door sounds the alarm (the sally).
6. **Strongholds stay on the Old Island** (Fire and Frost lands); the system is data per fort, so other lands get
   forts as rows later (LATER-COOKS line). Ladders, not lifts: the forts stand on flat ground (the siege needs it), so
   no drop is tall enough for a lift to have a job; the keep's 6 m roof is reached by stairs, the towers and the wall
   walk by ladders.

### 2.2 The layout (fort frame: +X out of the gate toward the approach, cm; every piece is a build piece)
```
 X:  -3320 ......... -1320 ......... -520 .............. +1320 ...... +4200
     [ INNER WARD 20x20 m ][ KEEP 8x8 ]   [ BAILEY YARD ]   [GATE]   [OUTPOST]
      boss arena, biome      2 storeys +    campfire, forge/      gate towers + turrets
      stone walls, postern   roof beacon    try-pots, barracks,   corner towers + archers
      in its back wall ->    outside stair  kennel by the breach  wall walk on ladders
      run-back stone 10 m    on its +Y side (Y +1100 side, rear)
```
- **Outpost** (X +4200, on the approach line, 29 m in front of the gate): a guard tower with a ladder, a brazier at its
  foot. The first layer: an archer on its deck and the horn-blower at its foot. Seen first on approach.
- **Bailey wall**: palisade, 6 segments by 5 (26.4 x 22 m). Gate in the front's middle segment between the two **gate
  towers** (bolt turrets on both decks, as built). **Corner towers** at the four corners, each with a fixed ladder
  (an `AWRSiegeLadder` owned by the fort) up its shaft to the deck.
- **The breach (side path)**: on the +Y side the two segments next to the rear corner are down (not built), rubble
  (flattened rocks) across the gap; just inside, the **kennel** (a lean-to) where the hounds sleep. Out of both
  turrets' arcs; one archer on the rear +Y corner tower covers it.
- **Wall walk**: along the front wall either side of the gate, wood floor plates on posts 2.5 m up (the palisade's
  points 1.5 m above it: a parapet at chest height), a fixed ladder up from the yard at each end. Archers on it.
- **The yard (courtyard)**: the crossroads every level looks down on. Campfire with benches (footmen at their ease),
  the **barracks** (a 2x1-cell wood hall, doorway on the yard, fur beds inside), and the land's work: Emberwatch's
  **forge** (forge, basalt anvil table, brazier, slag rocks); Rimehold's **try-pots** (cooking pots, a long table).
- **The keep** (X -1320..-520, Y -400..400, 2x2 cells, the biome kit `basalt_hall_*` / `rime_lodge_*`): ground floor =
  the hall, its front a **barred door** (the siege door) facing the yard; the first floor is reached by an **outside
  stair** on its +Y side to a doorway: the on-foot way in. On the first floor the **keep warden** (the knight) guards
  the **war chest**. Stairs inside down to the hall and up to the **roof** (half-wall parapet, the **beacon** brazier,
  the banner): the vantage point over the approach and down into the ward. The hall's back wall is a **doorway into
  the ward** (the boss door: lamps either side, a banner over it).
- **The inner ward** (X -3320..-1320, Y -1000..1000, the biome kit's walls): the boss arena, open to the sky, a level
  floor of the kit's foundations, lamps at its corners, the warlord's seat at its back (Emberwatch: the Reeve's anvil
  table and fireplace; Rimehold: a throne-bench under a tapestry). The **postern** (a kit door) in its back wall,
  barred from outside ("It doesn't open from this side"); from inside, E lifts the bar and it opens for good (until
  the fort rebuilds). 10 m outside it stands the **run-back stone**: die to the warlord, wake there, walk through the
  postern straight to the arena (Stakes of Marika + Raya Lucaria's loop).
- **The gate opens from inside**: from the yard, E on the gate lifts its bar (Stormveil's gate). From outside it's
  barred: ram it, or go round by the breach and open it for your friends and your engines.

### 2.3 Why each thing is where it is
| Thing | Job in the puzzle | Touches |
|---|---|---|
| Outpost + horn-blower | first layer; teaches stealth: kill the horn first (Tsushima) | approach, the gate's sight line |
| Gate + towers + turrets | the obvious way in, defended hardest | approach lane (engines at 40 m), ram target |
| Breach + kennel | the side path: quieter, but the hounds sleep there | rear +Y wall, rear corner archer |
| Wall walk + ladders | archers over the yard and the approach; climbing to them is the counter | palisade, yard |
| Corner towers + ladders | vantage for archers; a survivor who climbs one sees the whole yard | corners |
| Yard, campfire, barracks | the crossroads; the fight with the footmen and the captain | gate, keep stair, kennel |
| Keep, barred door | the siege's prize (ram / trebuchet) | yard; ward behind |
| Outside stair, first floor | the on-foot way in; the knight guards the chest | keep side, yard |
| Roof beacon | readable from afar; vantage into the ward | sky, ward |
| Ward + warlord | the heart, reached last | keep's back doorway, postern |
| Postern + run-back stone | the shortcut that opens back | ward back wall, outside ground |

### 2.4 The garrison (data rows; names and numbers first drafts)
Factions (the set tells it): **Emberwatch is held by the Cinder Reavers**, forge-hands who stayed when the
Forge-wardens left and turned to raiding the coast camps; **Rimehold by the Rime Wreckers**, whalers turned wreckers
when the sledges stopped coming. Elites wear the land's set (forgeplate / rimeguard), the ranks iron, bronze and hide.

| Rank | Tier | Weapon (real move set) | Behaviour | HP / poise | Loot |
|---|---|---|---|---|---|
| footman | small | sword+shield / axe / spear | posts and patrols; guards (shield), light chains 1-3, heavy 25%, dodge 15% | 160 / 30 | arrows, iron scrap |
| archer | small | hunting bow | keeps 8-30 m; draws 1.1 s (the draw pose and creak), looses at where you stand at release (no lead, never homes); under 4 m slings the bow and fights with a dagger | 110 / 20 | arrows |
| horn-blower | small | sword | sees you: raises the horn 2.0 s (kill or stagger him), then the alarm | 140 / 25 | rations |
| hound | small | the beast's own rows | sleeps in the kennel; wakes to noise or the alarm | creature | creature loot |
| captain | medium | sword + shield | walks the yard; guards, chains 3 lights, charged heavy, kicks a guard held 0.8 s | 420 / 60 | upgrade shard, iron |
| keep warden | medium | greatsword (Emberwatch) / great hammer (Rimehold) | guards the chest; hyperarmour heavies, charged heavy, a shoulder charge row | 650 / 80 | shards, the set's piece 15% |
| warlord | boss | great hammer (Hask) / spear (Skarde) | phases and rows below; never leaves the ward | 2600 solo, +80% a fighter | first kill + shards; unbars the chest |

**Hask, the Forge-Reeve** (Emberwatch, great hammer, fire). Phase 1: the great hammer's set plus `anvil_leap`
(1.0 s wind-up, hold 0.3, leaps to where you stood, 2.6 m ring, 55, impact 4, punish 1.3; picks the furthest tame
first) and `bellows_kick` (0.4 s, breaks guard, 15 + 90 poise, punish 0.8). **Transition at 50%: `quench_flare`**
(1.3 s, the hammer driven into the ground, an unblockable 4.5 m fire burst, crimson ring, burn 40); then his blows
burn and he moves 12% faster. Phase 2 adds `cinder_sweep` (hold 0.5, a 3 m cone, burning ground 4 s) and
`forge_combo` (two lights, then a heavy held 0.45), and `anvil_leap` holds 0.6.

**Skarde, the Wreck-Queen** (Rimehold, whaler's spear, frost). Phase 1: the spear's set plus `harpoon_cast` (1.0 s
wind-up, a thrown harpoon 1000 cm/s, straight, never homes, 40, frostbite 25) and `ice_hook` (0.7 s hooked sweep,
3.2 m cone, low: jump it). **Transition at 50%: `whiteout`** (1.3 s, unblockable 4.5 m frost burst, crimson ring,
frostbite 45). Phase 2 adds `flense` (three thrusts, the last held 0.5) and `tide_drag` (a 9 m line lunge,
unblockable, 50), and `harpoon_cast` throws two, 0.6 s apart.

Every row obeys CAL 1.13: wind-ups >= 0.34 s, specials >= 1.0 s, punish >= 1.2 s after slams, projectiles >= 0.9 s
from wind-up to arrival, nothing thrown inside 4 m, crimson rings only on unblockables; the phase change waits out a
crit or a stance break.

### 2.5 Behaviour (one brain, by rank)
- **Senses**: sight cone 120 deg (archers 140), range by rank (footman 18 m, archer 35 m, horn 30 m, captain 22 m,
  warden 14 m, warlord the ward), line of sight checked, half range on a crouching survivor. Hearing: a fight or a
  breaking piece within 15 m. Something seen turns them to look (0.8 s) before they commit (ER's pause, no UI).
- **States**: Post (stand facing out), Patrol (walk the route, pause 3 s at each mark), Fight, Search (8 s at the last
  seen point), Return (walk back to the post; leash 30 m from the post, the warlord the ward).
- **Fight (melee)**: close to the class reach; every 0.3 s pick: guard when the target swings in front (shields 55%,
  others 20%), dodge 15% (stamina allowing), else attack (light chain 1-3 by pressing in the combo window, heavy 25%,
  charged heavy 10% held 0.7 s), back off when stamina < 25, strafe between. No attack token: everyone near attacks.
- **The alarm**: the horn's wind-up is 2.0 s (arm up, a rising note). Then a 6 s horn every survivor within 200 m
  hears: footmen come to the last seen point, the hounds wake and are loosed, the captain leads. A broken gate or keep
  door sounds it too (the sally).
- **Respawn**: while the fort stands, a dead soldier comes back after `respawnHours` (2 game hours), only with no
  survivor within 150 m. The warlord's fall **takes the fort**: the beacon goes dark, the banners drop, the war chest
  unbars, nothing comes back until it **rebuilds** (`rebuildHours` 12 with nobody within 150 m) whole.
- **Loot**: each body's loot goes to its killer (a tame's owner) as creature kills do; the warlord pays every fighter
  with >= 10% of his health; the first time, a first-kill item.

### 2.6 Readable on approach
The beacon on the keep roof burns (a brazier, lit day and night) and the warband's banners hang on the gate towers and
over the boss door: seen from far, the fort is held. Taken: the beacon is out, the banners down. The outpost stands
alone on the approach: the first thing seen is a guard post with a man on it.

### 2.7 Edge cases
- Restart: broken pieces, fallen time, dead soldiers' return times, gate/postern unbarred, warlord down all save in the
  `strongholds` document.
- PvE worlds: soldiers and hounds always fight survivors, like beasts. Tames fight soldiers; turrets shoot only
  survivors and tames.
- Join mid-fight: everything is replicated (characters, pieces, doors, the warlord's phase).
- The warlord can't be pulled out: past the ward he walks back and resets (full health, phase 1) when no fighter has
  been in the ward for 8 s.
- A soldier with no target far from his post walks back; stuck 30 s, he's put back unseen (nobody within 60 m).
- The keep door never opens; gate and postern open only from inside.
- Bodies fade after 20 s.

## 3. PLAN (build steps, in order)
1. `FWRWeapons::ClassFor`: no inventory -> the class of the character's held `MeleeItem`.
2. `WRGarrison.h/.cpp`: `AWRGarrison : AWRCharacter` (own ASC like AWRWorldNpc; grants Light/Heavy/Dodge/Block/
   Critical; armour/weapon/bow from its row; the brain of 2.5; archer shots through `LaunchProjectile`; warlord rows,
   phases, ledger, first kill; replicated rank, phase, engaged, participants).
3. Hostility: garrison and its hounds share the hold (`FWRStrongholds::HoldOf`).
4. `Tools/terrain/strongholds.mjs` -> `Data/strongholds.json`: the layout of 2.2 in pieces (biome kit per fort), the
   ladders, the garrison rows, the warlord rows.
5. `WRStronghold.cpp`: ladders; the garrison (respawn), hounds; the alarm; gate and postern bars; beacon and banners by
   state; the warlord's fall takes the fort; the run-back stone; save/load.
6. Tuning `strongholds.garrison` in `Data/tuning.json`; sounds/fx rows.
7. HUD: the boss bar shows an engaged warlord; the hold-door prompt says "Lift the bar" / "Barred from the other side".
8. CHANGELOG Unreleased + HANDOFF; compile.


## 4. LEDGER PROOF (commits 247444a, cb96c04)

| # | Done | Where |
|---|---|---|
| R1 | yes | the whole layout + garrison below |
| R2 | yes | this doc 1.1-1.3 |
| R3 | yes | Tools/terrain/strongholds.mjs layout() (palisade, gate, towers); WRStronghold.cpp Build |
| R4 | yes | the breach (mjs layout, "+Y segments by the rear corner are down"); rubble WRStronghold.cpp ClearDressing |
| R5 | yes, ladders | mjs ladders() + place(); WRStronghold.cpp Build spawns fort AWRSiegeLadders. No lift: flat sites, see 2.1.6 |
| R6 | yes | corner decks, wall walk, keep roof (mjs layout) |
| R7 | yes | postern + gate open from inside only: WRStronghold.cpp UseDoor / FWRStrongholds::IsInside, DoorPrompt; saved "unbarred"; run-back stone in Build |
| R8 | yes | yard: campfire, benches, barracks, work, kennel (mjs layout) |
| R9 | yes | keep, 2 storeys + roof, barred door, landing (mjs layout) |
| R10 | yes | inner ward (mjs layout); warlord confined: WRGarrison.cpp InWard, ResetWarlord |
| R11 | yes | 2.2-2.3 here; every row's reason in mjs comments |
| R12 | yes | footman / archer / horn ranks + hounds: WRGarrison.cpp TickMelee, TickArcher/Shoot, TickHorn; TopUpHounds |
| R13 | yes | captain (guard, chains, charged, Kick) and warden (greatsword/great hammer heavies): the survivor's move sets via PressAbilityInput; ranks in strongholds.json |
| R14 | yes | warlords hask/skarde, phase 2 via transition row: WRGarrison.cpp CheckPhase, PickMove, StrikeMove; boss bar WRUIHud.cpp EngagedWarlord |
| R15 | yes | archers over the approach and the yard, turrets, sally on a broken door (Step), outpost/horn |
| R16 | yes | rank loot + rare (OnDeath), warlord ledger pay + first kill (WarlordDied), war chest |
| R17 | yes | respawnHours while standing + quiet; taken until rebuild; rebuild 12 h after last near (Step, TopUpGarrison) |
| R18 | yes | beacon + banners (held signs), outpost first on the approach, map landmark |
| R19 | yes | keep + ward in basalt_hall / rime_lodge pieces and decor; forge/try-pots |
| R20 | yes | both strongholds rebuilt; no other NPC fort/camp holds enemies (survey 1.4); scenery forts in other lands -> LATER-COOKS |
| R21 | yes | all build pieces owned @hold, raid rules unchanged; kitHealthScale |
| R22 | yes | rows obey CAL 1.13 (strongholds.json warlords); draw >= 0.9 s; throw never inside 4 m, never homes; transition waits crit/stance break |
| R23 | yes | data in strongholds.json + tuning strongholds.*; ApplyHit only |
| R24 | yes | server brain; replicated rank/name/warlord/phase/engaged/participants; pieces and doors replicated |
| R25 | yes | factions told by the set (forge vs try-pots, armour, banners); the existing first-visit lore line kept |
| R26 | yes | CHANGELOG "Not finished" says the navmesh-less steering and voices |
| R27 | n/a | no lift built; ladders are the existing climbable actor |
| R28 | yes | forts are data rows; more lands -> LATER-COOKS |
| R29 | yes | CHANGELOG + HANDOFF in 247444a; compile Succeeded; no binaries |
| R30 | yes | LANDED.md, LAPTOP-SAYS.md |
