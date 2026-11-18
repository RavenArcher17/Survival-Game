# Build lane: world systems (Fire and Frost, data-driven for all 12)

Lane build-world-systems, 2026-11-14. Scope (brief): shrines with path XP and fast travel, path masters with quests,
NPC villages / factions and traveling merchants, player messages, world events (meteor showers), biome events
(eruption), opposite-biome invasions, day/night spawn tables, hidden sub-biomes (lava tubes), roaming mini-bosses,
summoning altars. Built for Fire and Frost; every row is data so a new biome only adds rows.

Specs followed (kept exactly unless a deviation is listed in 0.2): FULL-GAME 3.1 / 3.3, cooks/fire-frost.md 3.1,
3.3.5, 3.3.7, 3.3.10, 3.3.11, 3.4.5, 3.4.7, 3.4.10, 3.4.11, 3.8; cooks/world-map.md 2.7, 2.8, 2.9 (villages);
cooks/endgame.md 2.8 (summoning stones); cooks/ui-controls.md (message stones).

## 0. Requirement ledger

| # | Requirement (from the brief and the Aaron quotes in it) | Done where |
|---|---|---|
| R1 | Shrines exist in the world, 5 per biome (1 Great + 4 lesser), Fire and Frost | Data/world.json `shrines.list`; WRWorldSystems.cpp:887 `PlaceShrines`; look WRWorldSites.cpp:184 `AWRShrine::BuildLook` |
| R2 | Shrines give path XP when cleansed/claimed (lesser 1,000 own-path / 500 each; Great 2,000; re-cleanse 20%) | WRWorldSites.cpp:471 `FinishKindle` (own 1,000 / each 500, re-kindle 20%); Great first visit WRWorldSystems.cpp:1195 |
| R3 | Lesser shrines are guarded (2 packs from their sub-zone); kill guards, then kindle 3 s | WRWorldSites.cpp:391 `StepGuards` (ring of guards), :340 `StartKindle` (3 s, cancel on step / blow / guards) |
| R4 | Discovered per you and your clan; saved | WRWorldSystems.cpp:1139 `NoteFound` + `RefreshFound` (clan via CanShare); saved :647 `WriteJson` (`shrines.<id>.found`) |
| R5 | Re-guard every 4 game days | WRWorldSites.cpp:391 `StepGuards` (`ReguardAtHours`, shrines.reguardDays 4) |
| R6 | Fast travel between discovered shrines; not in combat; tames within 30 m come; not to a shrine held by an invasion | WRWorldSystems.cpp:1369 `TravelBlock` (lit shrine, combat, mount, held), :1388 `Travel` (fade, tames 30 m); screen WRUIWorld.cpp:117 `SShrine` |
| R7 | Fast travel works from the map too (map shows shrines, Travel button) | WRUIMap.cpp:139 (shrine / village markers), :1076 (Travel here button) |
| R8 | A path master per biome at the Great Shrine who teaches the path through quests (Sigrun Ashhand, Áilu the Tracker) | Data/world.json `masters`; WRWorldSystems.cpp:934 `PlaceMasters`; WRWorldNpc.cpp `AWRWorldNpc` (hero rig, path marks, work loop) |
| R9 | Masters speak short lines within 6 m, no camera grab, no cutscene | WRWorldNpc.cpp:175 greetings within greetRange (ClientNotify line + sound, no camera) |
| R10 | Quests: 6 per master, paying path XP + recipe/secret; the last needs the third (roaming) mini | Data/world.json `quests` (6 each, last = Cinderhorn / Old Tusker); WRWorldSystems.cpp:1329 `QuestEvent`, :2149 take / hand in; hooks WRCharacterBase.cpp (OnKilled), WRCreature.cpp (OnTamed) |
| R11 | Quests show in the Journal | WRUIWorld.cpp:668 `SQuestsTab` (journal "quests", key U); talk screen WRUIWorld.cpp:222 |
| R12 | NPC villages per biome (Emberwick, Rimehold), safe zones, built around the Great Shrine | Data/world.json `villages`; WRWorldSystems.cpp:775 (placed + AWRSafeZone); look WRWorldSites.cpp:1033 `AWRVillage::BuildLook`, work places :1124 |
| R13 | Factions per biome with standing (Forgebound, Rime-kin) | Data/world.json `factions`; WRWorldSystems.cpp:515 `UWRQuestLog::AddStanding` / `TierOf` |
| R14 | Traveling merchants: a caravan per side on the road, village → camps → pass mouths, 1 game hour stops, can be attacked, killed → 10% of stock dropped, back next game day | Data/world.json `caravans`; WRWorldSystems.cpp:2035 `StepCaravans` (clock-placed loop, stops, death -> next day); WRWorldNpc.cpp:225 `OnDeath` (10% of stock) |
| R15 | Trade screen (barter, stock tiers by standing), mouse and pad | WRUIWorld.cpp:355 `STrade`; WRWorldSystems.cpp:2208 `world trade` (tier lock, discount, spill if bag full) |
| R16 | Player messages left in the world: write (templates, pad-friendly), read, appraise (heals the author if online), poor ratings hide it; saved | WRWorldSystems.cpp:1435 `PlaceMessage`, :2253 `world rate` (heal author 20%, away count), hide at -3; screens WRUIWorld.cpp:463; actor WRWorldSites.cpp `AWRMessageStone` |
| R17 | World event: Starfall (meteor shower) dropping rare ore, night, setting-scaled | WRWorldSystems.cpp:1675 `DropMeteor` + `StepStarfall` (dusk, streaks in AWRWorldState::Tick); crater WRWorldSites.cpp `AWRWorldCache` crater |
| R18 | Biome event: Eruption (warning, plume, ash, lava bombs ≥0.9 s glow never homing, roofs block, cores, sulfur ×3, opens the Sealed Vault) | WRWorldSystems.cpp:1620 `StepEruption`; WRWorldSites.cpp:762 `AWRLavaBomb::Land` (ApplyHit, roof, core); weather.json `eruption`; vault open |
| R19 | Biome event: Whiteout for Frost (25 m sight, gusts, Gloomhowls hunt by day, aurora night lichen ×3) | weather.json `whiteout`; WRWorldSystems.cpp:375 `IsNightTable`, :1717 aurora night; :359 `GatherScale` (x3) |
| R20 | Opposite-biome invasions: setting, 5 min warning, 8-14 + its roaming mini through a pass, march on nearest claim or shrine within 1.5 km, hold a shrine 30 min (travel shut), pay both biomes' path materials + 2,000 path XP to ≥10% dealers | WRWorldSystems.cpp:1747 `StartInvasion`, `StepInvasion`, :1941 `EndInvasion` (10% ledger, 2,000 XP, essences, standing); setting WRUISettings.cpp:1851 |
| R21 | Day and night spawn tables per biome and sub-zone (night table switches 30-50% of kinds) | Data/spawn_regions.json `night`; WRWildlife.cpp:781 (table of the hour); WRCreature.cpp:3331 `IsLyingUp`; night aggro already WRCreature.cpp:2274 |
| R22 | Hidden sub-biome: lava tubes under the volcano (Kiln Roads) | Data/world.json `hidden.kilnroads`; WRWorldSystems.cpp:1032 `PlaceTubes`; WRWorldSites.cpp:1212 `AWRHiddenTube::BuildLook`; vault WRWorldSites.cpp:1387 |
| R23 | Hidden sub-biome for Frost (the Blue Below, ice caves) — data-driven, same actor | Data/world.json `hidden.bluebelow` (same actor, ice look) |
| R24 | Roaming mini-bosses wander the biome | WRGameMode.cpp:143 `StepRoamingBosses` (combat-full); leads invasions WRWorldSystems.cpp:1866; last quests in world.json |
| R25 | Summoning altars: offer an effigy to raise a dead boss now; only the summoner's party can hurt it | WRWorldSites.cpp:598 `AWRSummoningStone::Offer` (spawner SummonNow / roaming); seal WRWorldSystems.cpp:295 via WRCombatLibrary.cpp:386 |
| R26 | Effigy recipes learned on the first kill of that boss | recipes.json `effigy_*` blueprint-gated; WRBoss.cpp:1190 `OnBossCredit` |
| R27 | Data-driven: new biome = new rows (shrines, masters, quests, villages, events, invasions, hidden, stones) | Data/world.json (biomes rows drive everything; no biome named in code but the volcanic/tundra wildlife side flag already in WRWildlife) |
| R28 | Every screen mouse AND pad; no dead states | WRUIWorld.cpp: every control a SWRButton / SWRSlot (the pad processor drives them); Esc / B closes; screens close when the target goes |
| R29 | Server authority, replication, multiplayer | Server: UWRWorldSystems (authority only), replicated AWRWorldState / AWRShrine / AWRWorldNpc / UWRQuestLog (owner only); clients send "world ..." commands |
| R30 | Sounds and fx as table rows | Data/fx.json 12 rows (shrine.fire, bomb.*, meteor.*, crater.smoke, core.smoke, summon.flare...); Audio/sounds.json 18 `world.*` rows |
| R31 | Design with intention: every placed thing has a job, connects, meets the ground | 2.x "why here" columns; anvil + trough / hide frame at the masters' spots (WRWorldSites.cpp:1124); stones face the arena / camp; tube walls leave room mouths open |
| R32 | No placeholders shown as finished; unfinished stays hidden | Section 4 below; unbuilt parts not shown (no buttons or rows for them) |

### 0.2 Deviations from the fire-frost cook (and why)

- **Sigrun quest 4 "The Bellows"** needs the Bellows Stacks, which aren't on the map. It becomes **"Kindle the
  vents": light the Glasswall, Blue Fire and Crater Rim shrines** (same lesson: tend the fires of the land).
- **Áilu quest 2 "Read the ice"** needs thin ice (not built). It becomes **"Cross to the lake shrine": kindle the
  Lake Island Shrine** (the same journey).
- **Quest recipe rewards** that point at items not in the game (Cinder salve, Spiked boots, Resin fire pot, Obsidian
  arrow) are added as items + recipes here, so the rewards are real.
- **Hidden sub-biomes sit at the surface**, not 20-60 m down: the island is a heightfield and the game lifts anyone
  under it back up (`RescueUnderground`). Real lava tubes run just under the crust and show as long rock ridges with
  collapsed-roof skylights (NPS lava-flow features), so the tube is built as that ridge: a rock-roofed corridor on the
  ground with skylight gaps, rooms from the sculpted cave shells. The ice caves follow the same frame (glacier
  meltwater tunnels also run at the base of the ice).

## 1. Research

| Game / reference | What it does | What we take |
|---|---|---|
| Breath of the Wild towers and shrines (CEDEC 2017 "triangle rule") | Shrines glow orange from afar until done, then blue; warp from anywhere | A dark shrine reads at night as a cold silhouette; a lit one burns a tall flame seen 300+ m |
| Elden Ring Sites of Grace | Grace lights on touch, warp from map, not in combat or near enemies | Map Travel; no travel while combat-tagged |
| Dark Souls 1 bonfires | Warp only between bonfires | We warp **from a lit shrine** (survival hauling stays real, as Valheim keeps ore off portals) |
| Valheim portals | Ore can't go through; hauling is the game ([gamepressure](https://www.gamepressure.com/newsroom/valheim-devs-said-no-to-metal-teleportation/z02f3e)) | Travel from shrine to shrine only, so bases near shrines matter |
| Enshrouded flame altars | No fast travel inside the Shroud ([PC Gamer](https://pcgamer.com/enshrouded-fast-travel)) | No travel to a shrine held by an invasion |
| Elden Ring messages ([wiki](https://eldenring.wiki.gg/wiki/Messages), [Defector](https://defector.com/elden-ring-makes-everyone-a-poet-and-a-reader)) | 20 templates, ~370 words, 10 conjunctions; a "Good" heals the author if online; poor-rated vanish | Template + word + optional conjunction + second clause, picked by pad; appraise heals author 20%; net −3 hides |
| Monster Hunter Wilds Fallow / Inclemency / Plenty ([TheBackdash](https://www.thebackdash.com/gaming/monster-hunter-wilds-weather-system-explained)) | A region's storm (eruption, blizzard) then a Plenty of resources | Eruption / Whiteout, then triple signature resource and bomb cores / aurora-night lichen |
| Dragon's Dogma 2 oxcarts and DD2 roaming | Carts on roads get ambushed; roamers walk into fights | The caravan walks the road; creatures may hit it; killed → 10% of its stock on the road |
| Ark tributes / Palworld altars | Items summon a boss on demand | Effigy at the summoning stone, owned by the summoner's party |
| Valheim Haldor / Rust outposts | A fixed trader, barter tiers | Merchant stock by faction standing (Stranger / Known / Trusted / Kin) |
| Real reference | Kīlauea / Hawaiian lava tubes (NPS): tubes run under a thin roof, skylights are collapses; Icelandic fissure eruptions: tremor, plume, tephra; Perseids: streaks first, then rare falls; Sámi duodji bands; Inuksuit as waymarks | Tube ridge + skylights; 5-min tremor warning; meteor streaks before the falls; Áilu's band colours; inuksuk ring round the Frost summoning stone |

## 2. Design (build-ready)

### 2.1 Shrines

| Id | Name | Biome | At (m) | Guards | Why here |
|---|---|---|---|---|---|
| forge | Great Shrine of the Forge | volcanic | village Emberwick (−1,250, −470) | none (path master) | Heart of the newcomers' village by the middle camp; first fire you find |
| kipuka | Kipuka Shrine | volcanic | (−1,150, 650) | 2 Cinderfang packs | Living pines in the lava fields; road to Ignessa |
| glasswall | Glasswall Shrine | volcanic | (−1,180, −900) | Kilnmite ×4 + Scorchwing ×2 | Obsidian ridge, road to the Warden |
| bluefire | Blue Fire Shrine | volcanic | (−430, −1,050) | Basaltback ×3 (mad while dark) | Sulfur vents by the north pass |
| craterrim | Crater Rim Shrine | volcanic | (−1,000, −250) | Magmaw ×2 | Last fire before Vulkarn |
| rime | Great Shrine of the Rime | tundra | village Rimehold (1,250, 470) | none | Village by the middle camp |
| lake | Lake Island Shrine | tundra | (900, −1,150) | Gloomhowl ×2 packs | Frozen lakes, road to Grimjaw |
| pine | Pine Shrine | tundra | (700, 1,150) | Gloomhowl + Frostantler bull | Pine hollows, the Pale Hunter's woods |
| crevasse | Crevasse Shrine | tundra | (450, 60) | Rimeshell ×3 + Glacimaw | Ice shelves below the ridge |
| glacier | Glacier Rim Shrine | tundra | (1,050, 420) | Rimeclaw ×2 | Above Skaldrith's bowl |

Every position is settled on land at runtime (traced down; if it lands in the sea it steps 20 m toward the island's
middle until it doesn't), so the data needs no re-placement when the island is rebuilt.

**Look (modelled in code from the build kit, one style per biome):** a 6 m round dais of fitted stone, three steps,
ringed by 7 standing stones (1.2 to 2.4 m, leaning a few degrees, the biome's rock: basalt `#1E1B19` / pale granite
`#B9C4CE` with rime). In the middle a stone pillar (1.1 m) carries an iron fire-bowl (1.1 m across, riveted rim, three
straps down to the pillar). Volcanic: slag cairns between the stones, sulfur crust `#D9C24A` at the dais foot. Tundra:
an inuksuk each side of the steps, hide prayer strips (red / blue / yellow, Sámi band colours) tied to two stones.
**Dark:** the bowl is cold, a faint blue rune line on each standing stone (a 600 lux pale light) reads at night.
**Lit:** fire FX in the bowl (`shrine.fire` row), a warm point light (6,000, 18 m), embers; seen across the land as a
flame on the skyline. The Great Shrines are larger (8 m dais, 9 stones) and always lit.

**Rules:** guards spawn when a player comes within 120 m of a dark lesser shrine (2 packs, round the dais at 15-25 m,
tagged to the shrine). All dead → "The shrine is clear: kindle it" notice to those near. Kindling: Interact at the
bowl starts a 3 s kneel (the hero's pick-up clip, a rising flame); moving 1.5 m, being hit or guards alive cancels.
On kindle: lit for the world (fire + light for everyone), discovered for the kindler's clan and everyone within 30 m,
path XP: lesser 1,000 to the biome's path if it's one of yours, else 500 to each of your paths; Great 2,000 on first
visit (same split). A re-kindle after re-guarding pays 20%. 4 game days after kindling, a lesser shrine goes dark and
its guards return the next time someone comes near.

**Travel:** Interact at a lit shrine (tap) opens the Shrine screen: the shrine's name and biome, its state, and a list
of every shrine you or your clan have found, by biome, each with Travel (grey with the reason: "Held by invaders",
"You're here", "In combat"). The Map tab shows shrines (found: flame glyph; unfound: none); picking a found one while
standing at a lit shrine shows Travel. Travel: 1.2 s fade out, move to the target dais edge facing the bowl, tames of
yours within 30 m with you (placed round you), fade in. Not while combat-tagged, riding a flying mount in the air,
or carrying over 100% weight.

### 2.2 Path masters and quests

| Master | Where | Look (MetaHuman hero rig, real idle clips) | Voice |
|---|---|---|---|
| Sigrun Ashhand (fire) | Emberwick, at the anvil 4 m from the Great Shrine | F face 3, strong build; Cinderhide chest and legs; works the anvil: a hammer swing every 2.6 s with an anvil ring and sparks while no one talks to her | Text lines; ember-low anvil ring under each line |
| Áilu the Tracker (frost) | Rimehold, by the drying racks 5 m from the Great Shrine | M face 2, athletic build; Snowhide chest and legs | Text lines; a soft wind-chime under each line |

Greeting: when you come within 6 m (once per 90 s per player) a line shows as a subtitle at the bottom of your screen
(4 s): first meeting / quest waiting / quest in hand / all done. Interact opens the Talk screen: the master's name and
path, their line, the current quest (title, task, progress, pays), buttons **Take it** / **Hand it in** (lit when the
task is done; bring-quests take the items) / **Leave**. One quest per master at a time, in order.

| Master | # | Quest | Task (type) | Pays |
|---|---|---|---|---|
| Sigrun | 1 | Feed the fire | Bring sulfur crystal ×10 (`bring`, taken) | 800 Fire XP; recipe Cinder salve |
| | 2 | Quench | Hold a fire-infused weapon in water (`quench`) | 1,000 XP |
| | 3 | Ash and bone | Kill 5 Cinderfangs while they burn (`kill`, status burning) | 1,200 XP |
| | 4 | Kindle the vents | Light the Glasswall, Blue Fire and Crater Rim shrines (`shrines`) | 1,500 XP; recipe Obsidian arrow |
| | 5 | Glass heart | Show her the Obsidian heart (`show`) | 2,000 XP; recipe Resin fire pot |
| | 6 | The yoke | Kill Cinderhorn (`kill`) | 3,000 XP; upgrade shard ×2 |
| Áilu | 1 | Listen | Come within 5 m of a sleeping Gloomhowl and get 30 m off without waking it (`sneak`) | 800 Frost XP |
| | 2 | Cross to the lake shrine | Light the Lake Island Shrine (`shrines`) | 1,000 XP; recipe Spiked boots |
| | 3 | The patient hand | Tame a Woolly drudge (`tame`) | 1,200 XP |
| | 4 | Slow the saber | Kill a Rimeclaw while it's slowed (`kill`, status slowed) | 1,500 XP |
| | 5 | Under the bridge | Show him the Troll marrow (`show`) | 2,000 XP |
| | 6 | The Earth-Bull | Kill Old Tusker (`kill`) | 3,000 XP; upgrade shard ×2 |

Quest XP goes to the master's path whether or not it's yours (DD: they teach anyone; the XP waits there for the day
you take the path, as saved per-path progress does). Every quest also gives the master's faction +150 standing.
Journal tab **Quests**: each master's current quest, progress ("3 / 5"), where to go, and a done list.

### 2.3 Villages and factions

| Village | Biome | Faction | At | Why here |
|---|---|---|---|---|
| Emberwick | volcanic | the Forgebound (newcomers who took up the old smiths' craft) | 250 m inland of the middle camp, around the Great Shrine of the Forge | Settlers built where the old forge-shrine still held fire |
| Rimehold | tundra | the Rime-kin (hunters and herders) | 250 m inland of the middle camp, around the Great Shrine of the Rime | The herders' winter camp by the shrine |

A village is a 45 m safe zone (no creature enters, no PvP, no building by others). It holds the Great Shrine, its path
master, the merchant stall (where the caravan's merchant stands at its home stop) and 5 dwellings modelled from the
kit, set in a ring facing the shrine with doors toward it: Emberwick's are basalt-walled with turf-and-ash roofs and a
smoke hole glowing; Rimehold's are hide lavvu tents (conical poles crossing at the top, hide wrap, a smoke flap)
and a drying rack with hanging fish. Each dwelling has a lit doorway lantern so the village reads at night.

**Standing** per faction, saved per player: Stranger 0, Known 300, Trusted 900, Kin 2,000. Earned: quests +150,
kindling a shrine of their biome +60, trades +5 per trade, defending their biome from an invasion +200. Standing
opens the merchant's stock tiers and takes 10% / 20% off at Trusted / Kin.

### 2.4 Traveling merchants

One caravan per side: **Brannoc the Cinder-trader** (volcanic) and **Ivalo the Furrier** (tundra), each a merchant
(MetaHuman hero rig with a pack frame on the back) walking at 3 m/s with an outrider (second NPC). Route (data):
home village → camp → pass mouth → camp → home, stopping 1 game hour at each stop. Its place on the route comes from
the world clock (so it's where it should be when anyone arrives) and it exists as actors only within 150 m of a player.
Creatures may attack it; the merchants fight back weakly (they have 400 HP). If the merchant dies, a pack (loot bag) of
10% of its stock drops and the caravan is gone until the next game day, then restarts from home.

**Trade (barter)**: each offer is "give X, get Y" with a standing tier. Volcanic sells iron ingots, sulfur, obsidian,
fire essence, a Glowmoth lure (nectar), bronze armour pieces, rope; buys hides, meat, lichen, frost essence, fish.
Tundra the mirror. Prices are standing-scaled.

### 2.5 Player messages (message stones)

Item **Scratch-stone** (hand recipe: stone ×2, flint/fiber). Use it from the bag (or the quick wheel) → the Write
screen: Template (20, e.g. "**** ahead", "Beware of ****", "Try ****", "Could this be a ****?", "Don't give up", "Look
for ****", "Time for ****"...), Word (5 lists: creatures of every built biome, places, gear, tactics, elements; ~90
words, pad scrolls with D-pad), optional Conjunction (and then, but, therefore, or, by the way, so to speak, all the
more) + a second template and word. Place: a flat carved stone slab (0.9 m, in the biome's rock) with the words cut
in and a strip of cloth tied round it appears at your feet. 10 per player (the oldest crumbles). Read: walk up, prompt
**Read** → the Read screen (the message, its author, Good / Poor). Good: the author, if online, gets "Your message was
appraised" and 20% health back; Poor: −1. A message at net −3 or below is hidden. Saved with the world.

### 2.6 World event: Starfall (meteor shower)

Setting **worldEvents** (Off / Rare / Normal / Frequent; Normal = one every 2 real days, at dusk). From dusk: streaks
across the whole sky for 5 min (`starfall.streak` fx, a sky-wide meteor every 1.5-3 s), notice "Stars are falling".
Then 3 to 5 meteors (each a fire-trailing rock, 2.4 s fall, a roar) land in random midland or highland ground of the
built biomes, never within 400 m of a claim, camp, shrine or arena. Each leaves a 20 m crater (scorched ring, thrown
rocks, smoke, a glowing core with a light seen 400 m) for 1 game day; the core is a star-iron node (6 to 10 star iron;
a gem 25%). 2 game hours after landing the glow draws one midland pack of that biome to the crater.

### 2.7 Biome events

Setting **biomeEvents** (Off / Rare / Normal / Frequent: one per week / 3 days / day per biome). At most two biome
events at once. Both follow fire-frost 3.3.11 / 3.4.11:

**Eruption (volcanic):** −5 min: tremors (camera shake 0.05 every 30 s for players in the biome), plume over the
crater (seen island-wide), banner "The volcano stirs". 0-10 min Plume: the `eruption` weather (80 m sight, ash). 10-50
min Fire: lava bombs at every outdoor player within 1.2 km of the crater, aimed 3-12 m off them every 8-14 s; a glowing
rock with a smoke trail on a ballistic arc, the boom 1.2 s before landing, ground glow the last 0.9 s; 3.5 m blast,
70 damage + burn 50, unblockable, never homes; a roof over the landing point takes it (60 damage to the piece); never
within 50 m of a camp or village. 50-60 Settling: no bombs. After: bomb cores stay 10 min (sulfur ×4-6, iron ore ×1-2,
obsidian 20%). The Kiln Roads' Sealed Vault opens for the game day. Sulfur crystals gather ×3 during the event.

**Whiteout (tundra):** −5 min: banner "A white wall rolls off the glacier"; 0-60 min: the `whiteout` weather (25 m
sight, gusts 600 cm/s 0.5 s every 10-20 s with a 1 s howl), Gloomhowls hunt by day (the night table runs in the
tundra), shrine braziers stay visible. After: the next night is an aurora night: lichen gathers ×3.

### 2.8 Invasions (Fire ↔ Frost)

Setting **invasions** (Off / Rare 1 / Normal 2 / Frequent 4 per pair per real week). An invasion picks a direction
and a pass. 5 min warning to every player in the defending biome: the raid banner "Frost is coming through the north
pass" with the way. Then at the attacking mouth of the pass: 8-14 invaders (Frost into Fire: Gloomhowl packs at night
or Woolly drudges + Frostantlers by day, plus 1-2 Rimeclaws; Fire into Frost: Cinderfang packs, Scorchwings and a
Magmaw) and the attacking side's roaming mini (Old Tusker / Cinderhorn) leave its loop to lead. They march to the
nearest claim or shrine within 1.5 km of the defending mouth (a claim: they break pieces as a raid does; a shrine:
they hold it, it goes dark and Travel to it shuts) for 30 min. Ends when all are dead (win) or time runs out (they go
back). Win pays everyone who dealt ≥10% of the invaders' total health: 2,000 path XP (split as shrine XP) + ember
essence ×3 and frost essence ×3, and +200 standing with the defending biome's faction.

### 2.9 Day and night spawn tables

`spawn_regions.json`: each region keeps `creatures` (its day table) and gets `night`. A kind only in `night` is a night
pack: by day it lies up in its den (not brought out near players); a kind only in `creatures` sleeps at night (built,
diurnal). Tables per fire-frost 3.3.7 / 3.4.7 by band (a region is a band today). At night nocturnal hunters get aggro
range ×1.25. A whiteout or the Dimming runs the night table by day.

### 2.10 Hidden sub-biomes

| Id | Name | Biome | Route (m) | Rooms | Light / hazard |
|---|---|---|---|---|---|
| kilnroads | The Kiln Roads | volcanic | 6 points from the Skylight of Embers (−560, 250) round under the crater flank to (−620, −320) | Ember Gallery (Kilnmite ×3, a lava stream), Smiths' Hall (3 iron nodes, a ruby, lore cache "Smith's tally"), Pele's Loom (Scorchwing ×2), the Sealed Vault (eruption only: iron ×6, gem ×2, upgrade shard ×2) | Dark; lava stream in the floor channel (the built lava rule) |
| bluebelow | The Blue Below | tundra | from the Moulin of Voices (520, −380) to (640, 300) | Meltwater Hall (Rimeshell ×2), the Burnt Wyrm (frost / ember essence cache, lore "Smith's spike"), Rime-folk Ice Tomb (cache: bronze spear, ivory), the Deep Vein (iron ×6 + a gem node) | Dim blue; ice floor (slick) |

Built as a tube: a corridor 7 m wide, 5.5 m high along the route; a roof of overlapping basalt / ice boulders resting
on side walls sunk 3 m into the ground; two skylight gaps (no roof for 9 m) with a heap of ash / snow under them so
the 7 m drop doesn't hurt; rooms are the sculpted cave shells (cave_a/b/c) set at the route's corners with their mouths
on the corridor. Inside: a dim glow (ember orange / ice blue) every 25 m, the contents above. The Sealed Vault is a
basalt slab door (kit: stone leaf, iron bands, a ring of runes) that sinks into the floor while an eruption is on.

### 2.11 Roaming mini-bosses

Built (Cinderhorn, Old Tusker walk their loops: `creatures.json roam`, `AWRGameMode::StepRoamingBosses`
WRGameMode.cpp:141). This lane adds: they lead invasions (2.8) and Sigrun's / Áilu's last quests send you after them.

### 2.12 Summoning stones

One beside each major's run-back stone (6 m to its side) and one at each mini's home (its lair, 12 m off the spawn
toward the nearest camp). Look: a 1.6 m flat altar stone with the boss's sigil cut in its face (a glowing glyph in the
boss's element), a bowl hollow on top, ringed by its biome's waymark: 6 slag cairns (volcanic) or 4 inuksuit (tundra).
Effigy items (8: Vulkarn, Skaldrith, Obsidian Warden, Ignessa, Cinderhorn, Grimjaw, Pale Hunter, Old Tusker), alchemy
recipes visible after your first kill of that boss (`bossKill` on the recipe). Major: signature ×15 + essence ×5 + any
of the biome's mini trophies; mini: signature ×5 + 10 midland materials. Interact (2 s kneel) with the effigy in your
bag: if the boss is dead it rises now (the boss's tell sound heard within 300 m, the glyph flares) and belongs to the
summoner's party (clan + whoever was within 30 m of the stone): others' hits do nothing ("Sealed to <name>'s party").
If it's alive: "<Boss> is already awake".

## 3. Plan (build steps)

1. `Data/world.json` (new): `shrines`, `masters`, `quests`, `villages`, `factions`, `caravans`, `trade`,
   `messages` (templates, words, conjunctions), `events` (starfall, eruption, whiteout), `invasions`, `hidden`,
   `summoning`; tuning blocks in the same file. `Data/weather.json`: `eruption`, `whiteout` kinds (chance 0: event
   only). `Data/items.json`: effigies ×8, scratch_stone, cinder_salve, obsidian_arrow, resin_fire_pot, spiked_boots,
   lore items. `Data/recipes.json`: those recipes (`bossKill`, `hidden` until learned). `Data/spawn_regions.json`
   `night`. `Data/server_presets.json`: `worldEvents`, `biomeEvents`, `invasions`. `fx.json` / `sounds.json` rows.
2. `WRWorldSystems.h/.cpp`: `UWRWorldSystems` (server world subsystem: load data, settle positions, spawn shrines /
   stones / villages / tubes, the event / invasion / caravan schedulers, the save file `world_systems.json`, fast
   travel) and `AWRWorldState` (replicated: events on now, invasion, found shrines per player list sent to owners).
3. `WRWorldSites.cpp`: `AWRShrine`, `AWRSummoningStone`, `AWRMessageStone`, `AWRMeteorCrater`, `AWRLavaBomb`,
   `AWRBombCore`, `AWRVillage`, `AWRHiddenTube`, `AWRVaultDoor` (kit-modelled looks).
4. `WRWorldNpc.cpp`: `AWRWorldNpc` (subclass of `AWRCharacter`, `bNpc`, no controller: masters, merchants), the
   quest log `UWRQuestLog` (PlayerState component, saved in the player file) and the hooks (kill, tame, craft, shrine,
   status, sneak, quench).
5. `WRUIWorld.cpp`: screens `shrine`, `talk`, `trade`, `msgwrite`, `msgread`; journal tab `quests`; HUD subtitle.
6. Wiring: `EWRMenu` entries, `WRPlayerUI` mapping, interact routing + prompts, server commands
   (`shrine`, `travel`, `quest`, `trade`, `msg`, `summon`), `ApplyHit` gates (NPCs immune, sealed summons), NPC skips
   in player loops, gather multiplier, night tables in `WRWildlife`, map shrines + Travel.
7. Compile, fix every warning; spot-check against this ledger; CHANGELOG + HANDOFF; commit; LANDED line.

## 4. Not finished (honest)

- Compiled only (editor adaptive + full unity), commit in the CHANGELOG; nothing played here (Aaron tests, F8).
- Masters speak in text lines (notices) with a soft sound under them: no voice recordings. Sigrun's work is the hero's
  chop clip with empty hands (no hammer prop); Áilu's is the gather clip at his hide frame.
- Merchants walk with an outrider but no pack beasts and no pack frames on their backs.
- The hidden tubes are boulder corridors on the island's ground along data routes nobody has walked yet: the routes may
  cross steep ground and need tuning (`world.json hidden.route`). Not built: the Flooded Tube's sinking crust, Hekla's
  Mouth and its geyser, the Skylight of Bones, the moulin drop and the frozen waterfall, the back door into Vulkarn's
  forge.
- Eruption: the lava river's rise and the spill channels, Magmaw counting double and Scorchwing aggro x1.5, and the
  event music layer are not built. Whiteout: the Pale Hunter's roam and the sharp footprints are not built.
- Invaders leave no slick-ice patches (frost) or melted slush (fire).
- The Dimming and the Migration (world-map 2.7 GAP-COOKED events) are not built (not in this brief's list).
- The new items' icons are flat drawn icons (Tools/art/make_world_icons.py), like the other lanes'.
