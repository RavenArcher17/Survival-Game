# Wildreach cook: ENDGAME

Lane: endgame. Written 2026-11-14. Scope: FULL-GAME section 8 (8.1 the arc, 8.2 dungeon portals, 8.3 artifacts and the rest of the endgame), plus what section 8 depends on: the level and gear curve across 12 biomes, endgame raids and bosses, repeatable goals, account and world progression, and what a player does at hour 100 and later. Every GAP is designed with numbers.

**How to read the tags**
- **SPEC**: BUILT or RAVEN-PLANNED. Kept exactly; the source is cited (DD = `Docs/design/design-doc.md`, CF = `crafting.md`, SN = `starting-numbers.md`, SS = `server-settings.md`, BP = `build-plan.md`, BO = `bosses.md`, DR = `DESIGN_REVIEW.md`).
- **KEPT (cook)**: another lane already cooked it. It stays as written; this cook only adds what's missing. Sources: `cooks/biome-desert.md` 2.22 (portal framework), `cooks/biome-void.md` 2.23 (raid frame, Severance raid, mythic gear, prestige, records, achievements, titles, cosmetics), `cooks/survival-crafting.md` 2.13 to 2.18 (metals, blueprints, runes, slots, monuments), `cooks/world-map.md` (the Spine, the Crown, seven passes, Twelve Peoples), `cooks/combat.md` (boss numbers, Wildbrands).
- **GAP-COOKED**: was a GAP, or nobody designed it yet. Fully designed here.
- **DREAM**: in Raven's Dream list (BP). Designed here so nothing is vague, built only after the full-release gate.
- Times in seconds. Attack timings are **W / A / P** = windup / active / punish window. Damage is at the boss's base before group scaling.

This lane is research, design and plan (the cook brief). The build happens later, round by round, from section 3. Nothing here is in Aaron's build yet.

---

## 0. Requirement ledger

Every requirement from the brief, split one per row. "Where" is the section of this doc that answers it. "Build" is the plan step that builds it (section 3.7). No row is in Aaron's build yet; each becomes done only when its step ships.

| # | Requirement (source) | Where | Build |
|---|---|---|---|
| R1 | Full cook: research, design, plan for the endgame scope (brief) | 1, 2, 3 | — |
| R2 | Research names real games, specific systems, and what makes them work (Aaron 11-13 night) | 1.1 | — |
| R3 | Real-world reference where it applies (folklore, nature, culture) (brief) | 1.2 | — |
| R4 | Specific details we will use, written to this doc (brief) | 1.3 | — |
| R5 | Split brief into single requirements as a numbered ledger at the top (Aaron 11-14) | 0 | — |
| R6 | Progression arc across 12 biomes with numbers (scope) | 2.1 | E0 |
| R7 | Level curve: character level, path points, hours (scope) | 2.2 | E0 |
| R8 | Gear curve: metal, quality, upgrades, hits to kill per stage (scope) | 2.3 | E0 |
| R9 | Dungeon portals: Raven's rules kept exactly (DD) | 2.5.1 | E6 |
| R10 | Portal framework (ranks, spawning, breaking, keys, shards, modifiers) (GAP) | 2.5.2 to 2.5.4 | E6 |
| R11 | Portal rooms, traps, puzzles and bosses for all 12 biomes (GAP; LATER-COOKS desert line) | 2.5.5, 2.5.6 | E6, E7 |
| R12 | Endless tower portal with leaderboard (DREAM) | 2.5.7 | E14 |
| R13 | Artifacts: 3 slots, penalties for rewards, party uses highest, weird rules on wearer only, PvE only (DD) | 2.6.1 | E1 |
| R14 | Artifact list, weird rules, multipliers, drop tables (GAP; LATER-COOKS desert line) | 2.6.2 to 2.6.6 | E1, E2 |
| R15 | Hidden third boss phases need artifacts (BO; BUILT flag) | 2.6.4 | E1 |
| R16 | Server setting: artifact modifier limits (SS) | 2.6.7 | E2 |
| R17 | Raids for 6 to 10 players (DD); tuned for, not capped (DR #22) | 2.9 | E10 |
| R18 | Raid content for all seven passes with full move tables (GAP; LATER-COOKS void line) | 2.9.2 | E10, E11 |
| R19 | The 10% loot rule in a 10-player raid (LATER-COOKS void line) | 2.9.1 | E10 |
| R20 | Raven's "no cross-biome endgame bosses" rule respected (DD Ruled out) | 2.9.1 | E10 |
| R21 | Mythic gear needing materials from every biome (DD) | 2.10 | E12 |
| R22 | Boss soul forging (DD; LATER-COOKS sky line: no design anywhere) | 2.7 | E4, E5 |
| R23 | Summoning altars: offer items to fight a boss on demand (DD Bosses; FRP R5) | 2.8 | E3 |
| R24 | Prestige: reset a path for permanent perks (DD); confirm "only while equipped" (LATER-COOKS void line) | 2.11 | E12 |
| R25 | Leaderboards, achievements, titles, cosmetics, monuments (DD Endgame) | 2.12 | E13 |
| R26 | Repeatable goals that respect "no weekly challenges" (DD Ruled out) | 2.13 | E8, E9 |
| R27 | Account and world progression (scope) | 2.4 | E13 |
| R28 | What a player does at hour 100+ (scope) | 2.14 | — |
| R29 | Legendary weapons with unique movesets, secret super-bosses, god path, hybrid paths (DREAM) | 2.15 | E14 |
| R30 | DR #18: does mastery count saved progress across swaps? | 2.15.3 | E14 |
| R31 | Endgame is sized for groups; difficulty is personal; no player makes a biome harder for others (DD) | 2.6.3, 2.13 | E1 |
| R32 | Ruled out stays out: server-wide boss events, world boss, world-wide escalation, weekly challenges, first-kill announcements, dungeon currency, PvP portals, party limits (DD) | 2.0 | — |
| R33 | Combat bar: hit stack, hitstop 0.06 to 0.30, visible coil, one verb per button, 0.33 s buffer (brief) | 2.0, every boss table | E6, E10 |
| R34 | Attacks show the real weapon, claw or projectile; damage only on visible contact; small telegraph; tells read at 15 m (brief) | 2.0, every boss table | E6, E10 |
| R35 | Projectiles: ≥0.9 s warning, never home (brief) | 2.0, every boss table | E6, E10 |
| R36 | No attacker limit; players die; no safety floors; no helpers after a fail (brief) | 2.0 | all |
| R37 | Monsters terrifying, readable silhouettes, believable anatomy, no slop, no primitives (brief) | 2.5.6, 2.9.2, 3.3 | E7, E11 |
| R38 | Unique creature voices from fused real animal calls (brief) | 2.5.6, 2.9.2, 3.4 | E7, E11 |
| R39 | Real animation clips, no gliding (brief) | 3.3 | E7, E11 |
| R40 | World: POI rule, story told only through the environment (brief) | 2.5.5, 2.9.2, 2.10 | E6 |
| R41 | Music: real composed per place, always audible, ducks ≤4 dB, planned silences only (brief) | 3.4 | E6, E10 |
| R42 | UI: mouse and pad on every screen, no dead states, F1 controls page (brief) | 2.16 | E2, E13 |
| R43 | People attractive, naturally sexy, never sanitized (brief) | 2.5.6, 2.9.2 | E7, E11 |
| R44 | Wildbrands named so; taming as Raven designed; no spirit summons (brief) | 2.7, 2.0 | E5 |
| R45 | Design with intention: every object's job, what it connects to, how it meets what it touches (Aaron 11-14) | 2.5.5, 2.9.2, 2.17 | all |
| R46 | Machines look engineered; moving platforms move continuously on a believable mechanism (Aaron 11-14) | 2.5.6 (Sampo, Lift Golem), 2.9.2 (Flue) | E7, E11 |
| R47 | Plan follows Raven's patterns: UWRGameplayAbility + ClassForNode, all damage through ApplyHit, data-driven (brief) | 3.1, 3.2 | all |
| R48 | Assets per item and the art route (concept → Hunyuan3D 2.1 → Blender → Tools/art rig, or Fab) (brief) | 3.3 | all |
| R49 | Build order with the proof-of-one first (brief) | 3.6, 3.7 | E1 |
| R50 | Out-of-scope findings appended one line each to LATER-COOKS (brief) | 4 | — |

---

## 1. Research

### 1.1 What the best games do, and what we take

| Game and system | What it does and why it works | What Wildreach takes |
|---|---|---|
| **Hades, Pact of Punishment** | After the credits you pick conditions that fill a Heat gauge. Each condition has ranks. Bounty rewards only pay at a new Heat with a given weapon, so players climb one step at a time and then switch weapons ([RPG Site](https://rpgsite.net/feature/10287-hades-pact-of-punishment-heat-modifiers-and-how-to-maximize-your-rewards), [Prima](https://primagames.com/gaming/hades-guide-pact-of-punishment-modifiers-heat)). Some Heat changes the encounter itself, such as all three Furies at once | **Artifact Level (AL)** as a single readable number, built from three artifacts. First-time rewards per boss per AL step ("Geis records", 2.6.5) so the climb is one step at a time. High AL changes the fight (the hidden phase 3), not only the numbers. Rewards stop growing at AL 15, a clear ceiling |
| **Halo skulls** | Each skull is one rule with its own score multiplier: Iron (a death restarts), Famine (less ammo), Black Eye (no shield regen), Mythic (double enemy health). One rule each, easy to explain, stackable ([Halopedia H3 skulls](https://www.halopedia.org/H3:Skulls), [MCC skulls](https://www.halopedia.org/MCC:Skulls)) | **One weird rule per artifact**, written in one line, never two effects. The rule is the fun part; the shared penalty is just damage taken. Some rules borrow Halo's shapes: "no run-back" (Iron), "own HUD hidden" (Blind) |
| **Path of Exile maps** | Map modifiers raise danger and raise item quantity for everything in the map; quantity also raises the chance of more maps, a loop that feeds itself ([PoE wiki: Maps](https://www.poewiki.net/wiki/Maps)) | **Portal modifiers each add loot (+25%)**, KEPT from desert. Portal shards are what keys are made of, so clearing portals feeds more portals. No currency (Raven) |
| **Diablo IV Torment tiers** | Difficulty tiers scale enemy health, damage and loot; the 2026 expansion split 4 tiers into 12 smaller steps so rewards scale more precisely ([PC Gamer](https://www.pcgamer.com/games/rpg/blizzard-wants-diablo-4s-outrageous-damage-numbers-to-mean-something-again-in-lord-of-hatred-so-its-letting-you-crank-the-difficulty-up-to-near-impossible-levels-its-going-to-be-really-f-cking-hard), [Icy Veins](https://www.icy-veins.com/d4/news/torment-xii-is-here-and-most-diablo-4-players-shouldnt-touch-it/)) | **16 small steps (AL 0 to 15), not 4 big ones.** Each step is +7% damage taken and +5% loot. Small steps mean a player always has a next step they can reach |
| **Monster Hunter Wilds, tempered and arch-tempered** | The same monsters return tougher at high star ranks; their parts make the endgame Artian weapons, with random bonus rolls. Arch-tempered is the hardest band ([RPG Site endgame](https://rpgsite.net/guide/16920-monster-hunter-wilds-endgame-explained-tempered-monsters-decorations-artian-weapons-and-investigations), [Icy Veins](https://www.icy-veins.com/monster-hunter-wilds/news/monster-hunter-wilds-tempered-monsters-artian-weapons-explained/)) | **Scarred beasts** (2.13.3): rare tougher versions of highland creatures, a hunt that never runs out. **Scarred boss parts** at AL 5+: the same part with a stronger effect, the reason to fight a boss you've already beaten |
| **Elden Ring remembrances and Enia** | A remembrance boss drops a remembrance; you trade it for one of two unique items. The other stays locked unless you duplicate the remembrance ([eldenring.wiki.gg](https://eldenring.wiki.gg/wiki/Remembrance_Bosses), [RPG Site](https://www.rpgsite.net/feature/12459-elden-ring-remembrance-all-remembrances-boss-weapon-unlocks-and-how-to-clone-them)) | **Boss soul forging** (2.7): a major's soul is offered at that boss's own altar for one of two items, an awakened soul weapon or a soul talisman. A second soul (repeat kills) buys the other. No NPC: the altar is the world |
| **Elden Ring New Game Plus** | Each journey raises enemy HP and damage and the rune payout, stopping at NG+7 ([Fextralife](https://eldenring.wiki.fextralife.com/New+Game+Plus), [Game8](https://game8.co/games/Elden-Ring/archives/355390)) | Wildreach has no wipes and no NG+, and Raven rules out world-wide escalation. The same "harder for more" curve lives in **personal** artifacts. Like NG+7, it has a cap |
| **Valheim boss chain and Forsaken powers** | Each boss gates the next biome's tools; its trophy mounted on a stone gives a timed power ([Mobalytics](https://mobalytics.gg/gamebase/guides/valheim-boss-guide), [xgamingserver](https://xgamingserver.com/blog/valheim-boss-order-all-bosses/)) | Raven already did the better version: **Hearth tech by distinct major cores, any order** (CF). Boss trophies become **monuments** (KEPT). We take Valheim's lesson that a boss must change what you can make, and Raven's that the order is free |
| **Ark tribute tiers (gamma, beta, alpha) and Ascension** | The same boss at three tiers; summoned with tributes; the higher tiers unlock more and demand bred armies ([guildorder Ark bosses](https://guildorder.com/games/ark/wiki/boss-fights)) | **Summoning altars** (2.8): an effigy built from that biome's materials calls the boss on demand. One tier only; difficulty above it comes from artifacts, never a separate boss version |
| **Palworld summoning altars and raid bosses** | Slabs at summoning altars call a raid boss; Feybreak added an endgame island and a raid boss ([Gematsu](https://www.gematsu.com/2024/12/palworld-feybreak-update-now-available), [Windows Central](https://windowscentral.com/gaming/palworld-gets-first-raid-boss-bellanoir-in-major-update-though-xbox-players-have-to-wait-for-the-patch)) | Summoned fights belong to the summoner's party, like key portals (Raven). The altar is a fixed place in the world, not a menu |
| **V Rising V Blood chain and Dracula** | 57 V Blood bosses fought roughly by gear level; the final fight needs every boss, a gear level of about 80 to 90 and a castle upgrade ([xgamingserver](https://xgamingserver.com/blog/v-rising-v-blood-bosses-guide/), [supercraft](https://supercraft.host/wiki/v-rising/dracula-boss-guide)) | A visible gear number helps players pick fights, but Raven rules out a "world boss". We take the **checklist**: the Journal's Records tab shows every boss, which you've killed, your best time, and your highest AL kill |
| **Enshrouded endgame** | Level cap raised step by step (45, then 50) with gear going 5 levels past the cap; better chest odds for legendaries; the 1.0 release ended the story with an endgame biome ([gamerguides](https://gamerguides.com/enshrouded/guide/gameplay/basics/level-cap-explained), [games.gg 1.0 notes](https://games.gg/enshrouded/guides/enshrouded-10-patch-notes-every-major-change-explained/)) | Enshrouded's endgame thins out once the cap is hit. Wildreach's level stops at 50 (Raven) by design, so **hour 100+ must be horizontal**: other paths, combos, prestige, artifacts, raids, mythics. No level-cap raises |
| **Destiny 2 raids** | Encounters built on roles you discover, callouts and hidden information; no tutorials or markers; a checkpoint per encounter ([Destinypedia](https://destinypedia.com/Raid), [PC Gamer DSC](https://pcgamer.com/destiny-2-deep-stone-crypt-raid-weapons)) | **Two champions who also fight each other** (KEPT void frame) is a roles puzzle: split, keep them even, swap. Each war-hall adds one discoverable mechanic that needs callouts. No markers: the hall's own architecture shows the answer |
| **Dune: Awakening Deep Desert** | A PvP zone wiped weekly by the Coriolis storm, with the best loot behind danger; heavy criticism of the weekly wipe loop ([Deltia's](https://deltiasgaming.com/?p=266311), [Icy Veins](https://wp-prod.icy-veins.com/dune-awakening-how-deep-desert-bases-and-pvp-work/)) | What not to do: Raven rules out wipes and weekly challenges. Wildreach's repeatables run on **natural world timers** (boss respawns, portal refills, events), never a weekly reset |
| **Solo Leveling gates** | Gates ranked by colour; an ignored gate breaks and spills monsters; a red gate hides its true rank ([Prima](https://primagames.com/gaming/solo-leveling-arise-gate-types-explained), [Beebom](https://beebom.com/red-gates-solo-leveling-explained)) | The clear root of Raven's portal design. KEPT as desert cooked it. We don't add hidden-rank portals: the modifiers always show before entry (no surprises) |

### 1.2 The real world we build from

| Source | What it gives |
|---|---|
| **The Irish geis** ([Wikipedia: Geis](https://en.wikipedia.org/wiki/Geas), [Sacred Texts, Celtic legends](https://www.sacred-texts.com/neu/celt/rac/rac20.htm)) | A geis is a personal taboo, an obligation or prohibition laid on a hero, that brings power and fortune while kept and ruin if broken. Cú Chulainn must not eat dog and must eat what a woman offers; Connla must not turn back, must not refuse a challenge and must not tell his name; Conaire must not go sunwise round Tara. **This is exactly Raven's artifact: a personal rule worn for reward.** Each artifact's weird rule is written as a geis, and the raid artifacts take Connla's and Conaire's geasa directly |
| **Völundarkviða** (Wayland and Böðvildr) | The captive smith and the king's daughter; the Volcanic rank-5 portal boss |
| **Helgakviða Hjörvarðssonar** (Hrímgerðr) | The giantess kept talking until dawn turned her to stone; the Frost rank-5 boss's mechanic is letting dawn in |
| **Thor's goats Tanngrisnir and Tanngnjóstr** (Prose Edda) | Eaten each night, raised from their bones at dawn; one came back lame because a bone was split. The Storm rank-4 boss |
| **Lei Gong and Dianmu** (Chinese thunder deities) | The thunder drummer with a ring of drums; the Storm rank-5 boss |
| **Black Annis** (Leicestershire) and **Baba Yaga's hut on fowl's legs** (Slavic) | The Swamp ranks 4 and 5 |
| **The Leshy** (Slavic forest spirit) | Leads travellers astray; wearing your clothes inside out breaks the spell. The Forest rank-4 mechanic |
| **Sir Gawain and the Green Knight** | The beheading game; the knight picks up his own head. The Forest rank-5 boss |
| **The Ankou** (Brittany) and **the Mari Lwyd** (Wales) | The death-cart collector, and the horse-skull wassail answered by a battle of verses (pwnco). The Graveyard ranks 4 and 5 |
| **The Stymphalian birds** (Heracles' sixth labour) and **the Ziz** (Jewish legend) | Bronze-feathered birds driven up by bronze rattles (krotala); the giant bird whose wings blot out the sun. The Sky ranks 4 and 5 |
| **Tezcatlipoca, the Smoking Mirror** (Aztec) and **the Sampo** (Finnish Kalevala) | The obsidian-mirror god, and the magic mill Ilmarinen forged that grinds out grain, salt and gold. The Crystal ranks 4 and 5 |
| **The each-uisge** (Scottish water horse) and **the umibōzu** (Japanese sea giant) | Its hide sticks to whoever touches it and it drags them under; the umibōzu asks for a barrel, and sailors give it one with no bottom. The Ocean ranks 4 and 5 |
| **Fairy rings** (British Isles folklore) and **Ophiocordyceps** (real zombie-ant fungus biology) | Step into the ring and you dance for years; a fungus steers its insect host to a high point, then fruits from its head. The Fungal ranks 4 and 5 (a parasite steering an animal, never a human, so it stays clear of Raven's "no zombies") |
| **Hrafnkels saga, Norse grave goods, Bronze Age hoards** | Real reasons for treasure rooms in portals: offerings placed with the dead, hoards hidden in bogs |
| **Real ore ladder** (native copper → bronze from copper and tin → bloomery iron → crucible and pattern-welded steel → meteoric iron) | The metal curve (CF) mirrors real history; starsteel uses star iron from meteors, as Tutankhamun's meteoric iron dagger did |

### 1.3 Lessons, short (each one used below)

1. **Horizontal after level 50.** The level cap is fixed (Raven). Hour 100+ grows sideways: more paths through altar swaps, 66 combo pairs, prestige marks, artifacts, raids, mythics, records (2.14).
2. **One number for difficulty.** Artifact Level 0 to 15, in 16 small steps, shown everywhere difficulty matters (2.6).
3. **One rule per artifact, written as a geis.** Fun first, damage second (2.6.2).
4. **Rewards per step, once.** Each boss pays a one-time Geis record the first time you kill it at each new AL (2.6.5).
5. **The world is the menu.** Soul forging at the boss's altar, mythic forging at the Ring, summoning at the arena stone, records on a tablet by the run-back stone. No NPC vendors (2.7, 2.8, 2.10, 2.12).
6. **Repeat on natural timers, never weekly resets** (2.13).
7. **Personal difficulty never spills onto strangers.** Party multipliers stop at your clan or band (2.6.3).
8. **Reuse rigs low, build new bodies high.** Portal ranks 1 and 2 are tougher versions of biome creatures; ranks 3 to 5 are new or heavily remade bodies (2.5.6). This keeps 60 portal bosses buildable.

---

## 2. Design

### 2.0 Rules every endgame item follows (SPEC and the bar)

| Rule | Source | Tag |
|---|---|---|
| Endgame is sized for groups, not the whole server. Difficulty is personal: artifacts make you or your group take more damage for better rewards, so no player can make a biome harder for everyone else | DD Endgame | SPEC |
| Ruled out and kept out: server-wide boss events, a server-ending world boss, cross-biome endgame bosses, world-wide escalation, weekly challenges, first-kill announcements, procedural dungeon layouts, portal timers (timed runs, DR #20 reading), party limits, PvP portals, dungeon currency, seasonal wipes, a lore ending for mastering paths, a mentor system | DD Ruled out | SPEC |
| Everyone who deals 10% of a boss's health gets their own loot, rivals included (fixed on every server). 150 points, no path over 100 (fixed). Artifact weird rules only on the wearer (fixed) | SS | SPEC (BUILT for the first two) |
| Bosses scale +80% HP per extra player or tame | SN | SPEC (BUILT) |
| Every attack shows the real weapon, claw, jaw or projectile and deals damage only on the frame of visible contact; the telegraph is a small cue; tells read at 15 m; hit stack in full (reaction, flash, sparks, camera trauma, rumble, 4 sound layers), hitstop 0.06 to 0.30 s | Brief; CAL; combat cook | KEPT |
| Projectiles: windup plus travel ≥0.9 s, max 1,600 cm/s, never home after launch, nothing fired inside 4 m | Combat cook; FULL-GAME 3.3 | KEPT |
| No attacker limit: every add, champion and portal creature attacks freely. Players die from everything. No safety floors, no catch-up helpers after a wipe | Brief | KEPT |
| Colour law: gold = yours or parry; crimson = unblockable. Portal rank colours, artifact metals and UI never use gold or crimson for anything else | CAL 1.13 | KEPT |
| Taming as Raven designed; tames count as players for boss HP; no spirit summons (path summons and necro raises are Raven's abilities, not spirit summons) | DD; combat cook 2.13 | KEPT |
| Weapon arts are **Wildbrands** | CAL 1.8 | KEPT |

### 2.1 The arc across 12 biomes (8.1; stages SPEC where BUILT, hours and numbers GAP-COOKED)

**The shape.** Biomes are paths, not a ladder (DD). What moves a player forward is **how many different major bosses they've beaten**, because that sets the Hearth's tech level (CF: 0 / 1 / 2 / 4 / 6), which sets metal, upgrade cap and stations. Any major counts, in any order. The creature bands (lowland, midland, highland) are the same in every biome (SN), so every biome has a soft start by its coast and a hard end against the Spine.

Hours are for a regular player at 1× rates in a group of three, playing about 15 hours a week. A solo player takes about 1.4× as long on boss stages (bosses scale down solo, but there's no one to split aggro).

| Stage | Hours | Character level | Path points (main / sub) | Majors beaten | Hearth | Metal and upgrade cap | What you're doing | Tag |
|---|---|---|---|---|---|---|---|---|
| 1. Landfall | 0 to 5 | 1 to 3 | 0 to 10 / – | 0 | 1 | Copper, +2 | Home lowlands: crude tools, first base, claim, first tame, copper gear, first night with the pack | BUILT stage; hours GAP-COOKED |
| 2. Home midlands | 5 to 15 | 4 to 8 | 10 to 25 / – | 0 | 1 | Copper fine, +2 | Caves and ore, 2 to 3 mini-bosses (shards, trophies), copper masterwork, first mount, first hidden-sub-biome entrance found | BUILT stage (the FULL-GAME row says "bronze" here; CF puts bronze at Hearth 2, so bronze moves to stage 3. Logged in section 4) |
| 3. First major | 15 to 25 | 8 to 10 | 25 to 30 / – | 1 | 2 | Bronze, +4 | The home major with the group (weak to the opposite element, so players bring a friend's frost or fire, or plain steel and patience); bows and shields; first brews | BUILT (Fire/Frost) |
| 4. Across the pass | 25 to 45 | 10 to 16 | 30 to 40 / 0 to 10 | 2 | 3 | Iron, +6 | Cross to the opposite biome through its pass, survive its lowlands, beat its major, take its path at the altar as subclass; creature armour, windmill, power | BUILT (Fire/Frost) |
| 5. The wide middle | 45 to 90 | 16 to 33 | 40 to 70 / 10 to 30 | 3 to 4 | 4 at 4 majors | Steel, +8 | Neighbouring biomes through the necks and rib gaps; 25/25 combo (about hour 55); first biome armour set; breeding; machinery; portals rank 1 to 3; invasions; first artifact (guaranteed with the 4th distinct major's first kill, 2.6.6) | RAVEN-PLANNED / GAP-COOKED numbers |
| 6. Late game | 90 to 140 | 33 to 50 | 70 to 100 / 30 to 50 | 5 to 6 | 5 at 6 majors | Starsteel, +10 | Starsteel and legendary blueprints; portals rank 4 and 5; artifacts AL 1 to 6; hidden phase 3s; boss souls (from R7); 50/50 combo (about hour 110); **level 50 at about hour 130** | RAVEN-PLANNED / GAP-COOKED numbers |
| 7. Endgame | 140 to 250 | 50 (cap) | 100 / 50 (cap 150) | 7 to 12 | 5 | Starsteel +10, mythic | All twelve majors and their altars; raids under the passes (from R10); champion's marks; mythic forging; AL 7 to 15; prestige; records | RAVEN-PLANNED / GAP-COOKED |
| 8. Long tail | 250+ | 50 | Swapped paths keep their saved points | 12 | 5 | Mythic, soul weapons, Scarred parts | Mastering other paths through altar swaps, all 66 combo pairs, 36 prestige marks, AL 15 on every boss, Scarred hunts, breeding lines, monuments, PvP and clan wars, the endless tower when it ships (DREAM) | GAP-COOKED |

**Why 150 to 250 hours to the endgame and not 40:** Raven's numbers say one path is about 80 hours (SN), and his pillar is "mid-to-late game first, without tedious grind" (DD). The arc puts the first major at hour 15 to 25 (no slog to the first boss) and makes every stage after that a new place with a new boss, never a repeat of the last one.

**Where the arc sends you (world-map cook KEPT).** Spawn on your home coast, push up to the Spine, cross your pair's pass for the subclass. Then the belts on either side through the necks and rib gaps. The Broken End (Void and Crystal) and the Hollow Cape (Wind and Spore) are furthest from the Old Island, so they're naturally later for Fire and Frost players without being a ladder: a Void main starts there and finds Fire far away.

### 2.2 The level curve (SPEC numbers; hours GAP-COOKED)

SPEC (BUILT): point n costs 100 + 10n path XP; 60,500 XP to 100 points, about 80 hours at 1× (SN); character level = points ÷ 3; +3 HP and +1 stamina per level, 250 / 150 at level 50 (DR #35, #36). Cumulative XP to point P is 100P + 5P(P+1). At 756 XP per hour of play on one path (60,500 ÷ 80):

| Path points | XP to reach | Hours on that path | Unlocks |
|---|---|---|---|
| 10 | 1,050 | 1.4 | Trunk done; branches open |
| 25 | 5,750 | 7.6 | First combo threshold (with 25 in a second path) |
| 50 | 17,750 | 23.5 | Second combo threshold |
| 75 | 36,000 | 47.6 | Most branch capstones in reach |
| 100 | 60,500 | 80.0 | Path maxed; prestige at its altar (2.11) |

**Level 50 (100 main + 50 sub) costs 78,250 XP**, about 103 hours of path XP gain. Real play spends time building, taming and travelling, so level 50 lands about hour 130.

| Level | HP | Stamina |
|---|---|---|
| 1 | 103 | 101 |
| 10 | 130 | 110 |
| 25 | 175 | 125 |
| 50 | 250 | 150 |

**Path XP boosts in the endgame (GAP-COOKED; capped; none bought):** artifacts +4% per AL (2.6.3), claimed shrines (SPEC, DD), Rested (SPEC, survival lane), and the server XP setting (SPEC). The first three add together and cap at +100% (×2) before the server multiplier, so the curve never collapses.

### 2.3 The gear curve (SPEC multipliers; stage table GAP-COOKED)

SPEC (SN; BUILT `tuning.json` `weapon`): copper sword light 12, heavy 22; metal ×1.0 / 1.3 / 1.7 / 2.2 / 2.8; quality ×0.9 / 1.0 / 1.1 / 1.2; +5% per upgrade. Creatures 70 / 120 / 200 HP; mini 3,000 solo; major 12,000 solo.

| Stage | Expected weapon | Light hit | Heavy hit | Lights to kill lowland / midland / highland | Notes |
|---|---|---|---|---|---|
| 1 | Copper common +0 | 12.0 | 22.0 | 6 / 10 / 17 | SN target: lowland about 6 ✓ |
| 2 | Copper fine +2 | 14.5 | 26.6 | 5 / 8 / 14 | Highland is a real threat |
| 3 | Bronze fine +4 | 20.6 | 37.8 | 3.4 / 6 / 10 | SN target: midland about 6 with upgraded bronze ✓ |
| 4 | Iron fine +6 | 29.2 | 53.5 | 2.4 / 4.1 / 7 | SN target: highland 6 to 7 with upgraded iron ✓ |
| 5 | Steel masterwork +8 | 44.4 | 81.3 | 1.6 / 2.7 / 4.5 | Biome armour set (its 4 pieces pass its hazard) |
| 6 | Starsteel masterwork +10 | 60.5 | 110.9 | 1.2 / 2 / 3.3 | The overworld stops being the threat; artifacts, portals 4 to 5 and raids take over |
| 7 | Mythic +10 (×1.15 over starsteel, KEPT void) | 69.6 | 127.5 | 1 / 1.7 / 2.9 | Two elements at once (KEPT) |

**Why the overworld gets easy late, and why that's right.** Raven rules out world-wide escalation, and creatures don't level with you. Old biomes feeling easy at hour 150 is the Valheim and Elden Ring payoff for growing. The endgame threat is opt-in and personal: **artifacts** (up to +105% damage taken), **portal ranks 4 and 5** (creatures tuned for starsteel, 2.5.2), **raids** and **hidden phase 3s**.

**Armour and poise** (KEPT combat cook 2.6): poise hide 8 → copper 18 → bronze 24 → iron 32 → steel 40 → starsteel 46; biome sets 30 to 50. Armour value multiplies by the same metal factors as weapons (survival-crafting KEPT).

**Upgrade shard cost per step** (SPEC: mini-boss drops only, higher steps cost more; numbers GAP-COOKED to fit a 3 to 4 minute mini that drops 3 to 5 shards, BUILT roll):

| Step | +1 | +2 | +3 | +4 | +5 | +6 | +7 | +8 | +9 | +10 |
|---|---|---|---|---|---|---|---|---|---|---|
| Shards | 1 | 2 | 3 | 4 | 6 | 8 | 10 | 12 | 15 | 18 |
| Running total | 1 | 3 | 6 | 10 | 16 | 24 | 34 | 46 | 61 | 79 |

One weapon to +10 is about 20 mini kills spread across the 40 or so minis in the game, so no single mini gets farmed to death. Portal bosses rank 3+ and raid champions also drop shards (2.5.4, 2.9.1).

### 2.4 Account and world progression (GAP-COOKED)

Wildreach is persistent servers with no wipes (DD), saved per world on the host (BUILT `WRWorlds.cpp`; players in `players/<name>.json`, `WRGameMode.cpp` `SavePlayer`). Three layers, with power only in the first:

| Layer | Holds | Saved in | Rule |
|---|---|---|---|
| **Character (per world)** | Everything that gives power: paths and saved per-path points, gear, tames, life skills, artifacts, souls, prestige marks, known blueprints, achievements, titles, cosmetics earned on this world | `players/<name>.json` (BUILT file, new fields) | One character per player per world, as built. Power never crosses worlds, so a persistent server stays fair |
| **World Ledger (per world)** | The world's history: each boss's total kills, records boards (2.12.1), who first killed each boss (shown only on that boss's record tablet; never announced, DD), monuments, escaped portal bosses, open passes, kindled pillars | New `records.json` beside `world.json` | Never makes anything harder (DD rules out escalation). Memory, not difficulty |
| **Hall of Deeds (per player, across worlds)** | A read-only list of titles and achievements earned on any world | The player's PC: `Saved/Profile/deeds.json`; Steam achievements when M7's Steam/EOS login lands (RAVEN-PLANNED) | Shown on the main menu's Hall of Deeds page only. Gives no power and no cosmetics in other worlds (a local file can be edited, so it isn't trusted on another server) |

**The kindled pillars (the world remembers).** On the Ring of the Crown (world-map KEPT), each people's pillar carries its waymark on top. The first time anyone on the server kills that biome's major, the waymark's carved lines fill with a faint glow in the biome's colour (1 nit, visible within 200 m) and stay lit forever on that world. Twelve lit pillars mean the server has beaten every land. Nobody is told; you see it climbing the Ring. This sits under the KEPT event beacons (the 300 m columns during biome events), which are far brighter and temporary.

### 2.5 Dungeon portals (8.2)

#### 2.5.1 Raven's rules (SPEC, DD "Dungeon portals" and "How portals work"; restated in full in biome-desert 2.22.1, KEPT)

Random small portals in every biome lead to short crawls; themed by biome; rank shown by colour; five ranks (Common, Uncommon, Rare, Epic, Legendary), higher ranks deeper, longer to run and longer to break; break after 1 / 3 / 6 / 10 / 18 real days; an ignored portal spills monsters first, then closes when its boss escapes; an escaped boss blocks that rank in that biome until killed; entering a breaking portal is allowed until the boss escapes, and clearing it stops the break; handmade rooms shuffled with random modifiers (no healing, double damage); puzzles and traps; a boss at the end of every portal; craftable keys spawn a chosen rank; key portals belong to the opener, wild portals are first come; legendary portals announced server-wide; death inside follows server rules, bodies appear at the entrance if it closes; loot and materials only, no dungeon currency, no forced party size. DR #20 reading (KEPT): "portal timers" ruled out = timed runs; DR #21 (KEPT): traps in dungeons yes, bases no; "procedural layouts" ruled out = no generated rooms or graphs (both are handmade; only the pick is random).

#### 2.5.2 Ranks, spawning, breaking, entry (KEPT biome-desert 2.22.2; additions GAP-COOKED)

Kept exactly: the rank table (colours bone white #E8E2D0, green #4FB04A, blue #3A7BDB, violet #8E4FD8, flame orange #FF7A1A; rooms 3 / 5 / 7 / 9 / 12 plus boss; about 10 / 15 / 25 / 35 / 50 min; 0 / 1 / 1 to 2 / 2 / 3 modifiers; boss HP solo 1,500 / 2,500 / 4,000 / 7,000 / 12,000), the slot rule (1 per 1.5 km², rounded up), refills 2 to 6 real hours, placement (≥150 m from claims, camps, shrines, arenas; never in a pass), the light pillar (60 m; rank 5: 300 m), the hum, the legendary announcement (global chat line plus the biome's Crown pillar flickering orange), escalation (cracking at 50%, spilling at 80% with a 3,000 HP warden, break at 100%), the escaped boss (roams 400 m, never targets claims), party entry (10 m, 5 s accept), the entry run-back stone, modifiers shown before entry, the 2-minute loot window, rank shards to everyone at 10%, the ten modifiers (+25% loot each), and the key rule (rank-n key = 3 rank n−1 shards + 1 rank n shard + signature resource ×2n; the signature resource sets the biome).

**Added here (GAP-COOKED):**

**Slots for all 12 biomes.** Area counts caverns, catacombs and caves where the biome lives underground. Every biome always has one high slot (Epic 95% / Legendary 5%), so small biomes still have hard portals.

| Biome | Area used (km²) | Slots | Mix |
|---|---|---|---|
| Volcanic | 5.0 | 4 | Common, Uncommon, Rare, High |
| Storm peaks | 5.5 | 4 | C, U, R, High |
| Desert | 6.5 | 5 | 2 C, U, R, High (KEPT) |
| Ancient forest | 7.0 | 5 | 2 C, U, R, High |
| Sky islands | 2.0 | 2 | C, High |
| Crystal caves | 1.0 + 2.0 caves | 2 | C, High |
| Frozen tundra | 5.0 | 4 | C, U, R, High |
| Swamp | 4.0 | 3 | C, R, High |
| Ocean & reefs | 3.0 land | 2 | C, High (portals open on islets and the reef shelf, never in deep water) |
| Graveyard | 2.5 + 2.0 catacombs | 3 | C, R, High |
| Fungal caverns | 1.5 + 3.0 caverns | 3 | C, R, High |
| Corrupted wasteland | 6.0 | 4 | C, U, R, High |
| **Total** | | **41** | At most 8 runs open at once (KEPT desert cap, server setting); a portal whose bay is full shows "All folds are full: wait" with the time until the next bay frees |

**Portal creature strength by rank** (portal rooms reuse the biome's own creatures plus 2 portal-only ones each):

| Rank | Creature stats | Packs | Why |
|---|---|---|---|
| 1 | Lowland (70 HP, 12 hit) | 1 to 3 | Copper-geared players |
| 2 | Midland (120 / 20) | 2 to 4 | Bronze |
| 3 | Midland ×1.25 HP (150 / 20), wind-up delay holds on | 2 to 5 | Iron |
| 4 | Highland (200 / 32) | 2 to 5 | Steel |
| 5 | Highland **portal-tempered**: ×1.6 HP (320), ×1.3 hit (42), stance ×1.3 | 3 to 6 | Tuned so starsteel +10 needs 5 to 6 lights a creature; the only place outside raids where the endgame's gear is tested without artifacts |

**What a cleared portal leaves (DR #20, KEPT):** it closes 2 minutes after the boss dies, everyone inside is put at the entrance, and the spot shows a faint scorched ring for one game day.

**A portal's story (why it's here, design with intention).** Every portal is a fold into a sealed place of that biome's old people (world-map's Twelve Peoples, KEPT): a vault, a tomb, a workshop, a shrine they walled up. That's why the rooms are built in their stone, why their traps protect something, and why the boss is a guardian, an experiment or something they locked away. The rift's edge is made of the biome's signature material (sunglass in the Desert, KEPT), so the portal reads at a glance as "this land's door".

#### 2.5.3 Running a portal (GAP-COOKED, inside SPEC)

| Moment | What happens |
|---|---|
| Approach | The prompt card shows rank, biome, the modifiers, the break state and "Owner: <name>" on key portals. Pad A / mouse click to enter; B / Esc to back out. A key portal owned by someone else says "Sealed to <name>'s party" (never blank) |
| Inside | Doors between rooms are open; a room's exit door closes behind a fight until the room is clear (sound: a stone grind and a bar dropping) and opens when the last creature dies or the puzzle is solved. The boss door seals when the first player crosses its threshold (KEPT) |
| Map | Your map screen shows only the rooms you've seen inside the fold, drawn as you walk. No markers for chests or exits (story through the place) |
| Death | Server rules (SPEC). If your death bag is inside and the portal is still open, you can re-enter from the outside portal and run back from the entry stone (KEPT) |
| Leaving early | Walk back through the entry stone's arch: you're put outside. The portal stays as it was (its timer keeps running) |
| Music | The biome's combat layer under a portal bed (a low version of the biome theme through a long reverb, as if heard through stone), ducking ≤4 dB under fights; the rank-5 boss gets its own composed track (3.4) |

#### 2.5.4 Rewards per rank (SPEC: loot and materials only; numbers GAP-COOKED)

Each player who deals 10% of the boss (KEPT rule) gets the boss roll. Chests are personal (each player opens their own copy: no chest racing in a group).

| Rank | Chests on the main line | Treasure room (side branch) | Boss roll per player | Rank shards | Upgrade shards | Blueprint chance (KEPT survival-crafting) | Artifact chance (KEPT desert) |
|---|---|---|---|---|---|---|---|
| 1 | 1 (biome materials ×6, copper or tin ×8) | 1 chest: a gem 25% | Biome materials ×8, ore at band 1, potion ×2 | 1 Common | 0 | 10% | 0 |
| 2 | 2 (materials ×8, iron ×6) | 1 chest: gem 50% | Materials ×10, band 2 ore and coal, a fine-quality weapon or armour piece of the biome's metal tier 30% | 1 Uncommon | 1 | 20% | 0 |
| 3 | 2 (materials ×10, rich iron ×6, silver ×2) | 1 chest + a found rune 15% (KEPT survival-crafting R9) | Materials ×12, band 3 ore, path essence ×3 | 1 Rare | 2 | 30% | 3% |
| 4 | 3 (steel-tier inputs: charcoal ×10, steel plate ×2) | 1 chest + gem cluster | Materials ×15, path essence ×5, star iron ×1 | 1 Epic | 3 | 45% (legendary blueprints possible, KEPT) | 8% |
| 5 | 4 + a vault (KEPT) | Vault: star iron ×3, a masterwork piece 20% | Materials ×20, path essence ×8, star iron ×2, core shard ×2 (starsteel input) | 1 Legendary | 4 | 60% (legendary possible) | 15% |

Every modifier adds +25% to quantities (KEPT), and the party's artifact multiplier stacks on top (2.6.3).

#### 2.5.5 The room kit for every biome (KEPT desert framework and pool sizes; the eleven other kits GAP-COOKED)

**Pool per biome (KEPT):** 2 entry, 10 combat halls, 6 trap corridors, 4 puzzle rooms, 2 rest alcoves, 3 treasure rooms, 5 boss arenas = 32 handmade rooms on the 4 m grid, door sockets 4 m wide by 6 m tall; 4 handmade layout graphs per rank (main line plus 1 or 2 side branches). **Trap rule (KEPT):** every trap has ≥0.9 s of sound plus a visual tell, can kill, can be disarmed or turned on enemies, and hurts enemies too. **Puzzle rule (KEPT):** never needs a specific path; any player can solve it.

| Biome (whose fold) | Room look (materials, light) | Trap 1: tell → effect → answer | Trap 2 | Puzzle 1 | Puzzle 2 | Portal-only creatures |
|---|---|---|---|---|---|---|
| **Desert** (Sunwatch tombs) | KEPT biome-desert 2.22.3 | Dart walls (KEPT) | Sand room, rolling sun-stone, scarab floor, collapsing floor (KEPT) | Mirror relay, hour lock (KEPT) | The weighing, the sand clock (KEPT) | Ushabti, flesh scarabs, tomb jackal (KEPT) |
| **Volcanic** (the Smiths' kiln roads: sealed forge halls) | Black basalt ashlar, bronze-banded doors, slag-glass floor inlays; light from lava runnels in the floor channels and hanging crucibles (warm orange, deep shadows) | **Pulse grates:** iron floor grates glow dull red for 1.2 s with a rising bubbling hiss → lava wells up through them for 2 s, 30 damage/s + burn 20/s → walk the bare stone between, or block a grate with a dropped anvil block (Interact a loose block to slide it, 1.5 s) | **Slag chute:** a bell on a chain rings once (1.0 s) and a ceiling chute's lip glows → molten slag pours a 3 m strip, 80 damage + burn 40 → stand clear; the chute's rope can be cut (100 HP) to pour it early onto creatures below | **Quench the blade:** three cooling troughs (water, oil, brine) and a glowing blade blank on the anvil; the door's lintel shows the blade's temper colour (straw, blue, purple) → carry the blank (2 s) to the matching trough; wrong trough cracks the blank and a new one heats in 20 s | **Bellows rhythm:** a great forge with three bellows; the hammer automaton strikes a rhythm (3 to 5 beats) → pump the bellows in the same order and count; the forge roars and the door's iron bar lifts | **Slag thrall** (midland): a half-cast iron humanoid broken from a sand mould, sprue spikes still on its back, glowing seams; hammer-arm combo 0.6 / 0.2 / 0.8, sprue burst 0.9 (2 m ring, `low`); voice: bellows wheeze + iron scraping. **Anvil mites** (swarm): fist-sized Kilnmite young, 25 HP ×6, ember bite |
| **Frozen tundra** (the Rime-folk's Blue Below: ice-cut halls) | Glacier ice walls with trapped stones and air bubbles, bone-and-driftwood lintels, inuksuk niches; cold blue light through the ice, whale-oil lamps (warm) in niches | **Icicle fall:** a sharp crack and powder dust drifting down 1.0 s before; the icicle's shadow grows on the floor → 60 damage + frostbite 30 → step out of the shadow; fire or a thrown stone drops them early | **Thin ice:** creaking underfoot and white cracks spreading for 1.0 s → the floor gives into black water 2 m deep (frostbite 15/s, swim out at the edge) → walk on the darker blue (thicker) ice; spiked boots warn with a grip sound | **Light the braziers:** 3 to 5 braziers frozen over; carvings above each show a sun position → light them in the sun's order (east to west); the ice door melts | **Aurora lock:** a skylight shows a slow aurora in 3 colours; three ice prisms on the floor → rotate each (Interact turns 60°) so its beam matches the aurora band above it | **Rime-calf** (midland): a frost-calf of the old herds, ice-plated, low and wide; horn charge 0.8 (8 m), stamp 0.7 (`low` ring); voice: musk-ox bellow + ice creak. **Rimecrawler** (midland): a 3 m ice centipede in the walls; bursts out with a 1.0 s crack tell; mandible 0.55 / 0.2 / 0.7; voice: centipede click + glacier groan |
| **Storm peaks** (the Rodwardens' Hollow Spire workshops) | Granite with copper bands green with age, glass insulator rows, chain conduits running along the walls into the floor; light from arcing gaps (blue-white flicker) and the pale glow of stormglass | **Charged plates:** a rising hum and blue static crawling over copper floor plates for 1.0 s → 40 damage + shock 40 → step on the stone joints; a tamed Rodcrest or a grounding charm lets you walk them | **Arc gate:** two copper posts in a doorway; the hum climbs and the air smells of ozone (a sound cue) for 1.0 s → an arc fires between them every 3 s, 50 + shock 50 → time the run; throw a metal item between them to short it for 6 s | **Route the current:** a grid of 6 to 9 copper rods on pivots; one end has a charged coil, the door lock at the other → rotate rods (Interact 90°) to make a connected line; sparks run along the connected ones as you build | **Thunder count:** lightning flashes in a skylight; three bells marked 1 to 3 → strike the bell matching the seconds between flash and thunder (sound travels about 1 km in 3 s, real physics); three right in a row opens the door | **Ball lightning** (midland): a drifting 0.8 m sphere of crackling light; drifts at walking speed, crackles 1.2 s, then bursts in 3 m, 45 + shock 45; it's a creature you can pop early with a ranged hit. **Rod-warden automaton** (midland): a 2.4 m copper-and-oak guard with a spear; thrust 0.5 / 0.15 / 0.7, sweep 0.75; voice: ratchet clicks + transformer hum |
| **Swamp** (the Mire Coven's sunken rooms) | Black bog oak, bone fetish posts with crow skulls, red ochre handprints, wattle walls sunk to the knee in bleedwater; light from rush lights and lantern-toad glow (sickly green) | **Rising bleedwater:** a gurgle from floor drains and the water's surface trembling for 1.5 s → the room floods to waist height over 6 s, 10 bleed/s while wading (KEPT hazard number) → climb the root shelves, or pull the drain chain (2 s) | **Tithe hooks:** a creak of rope and swaying bone hooks 1.0 s before → hooks swing across a walkway, 45 + bleed 30, and knock you into the water → duck the low swing (crouch), or cut the hook ropes (80 HP) | **Feed the basins:** three tithe basins under crow-skull posts with 1 to 3 skulls each → place the matching count of blood offerings (bloodmoss bundles from the room's shelves) in each; the Coven door swings | **Hex mirrors:** a black-water mirror pool that shows the room reversed; one carving on the real wall is missing in the reflection → strike the carving that the reflection lacks | **Leech knot** (swarm): a ball of fist-sized leeches that unrolls; 25 HP ×8, each bite bleeds 10. **Hag-crow** (midland): a crow the size of a heron with a human-like grasping foot; dive 0.8, pluck 0.5 / 0.2 / 0.7 (steals a potion from your belt if it lands; drop it by killing the crow); voice: raven croak + a woman's laugh pitched down |
| **Ancient forest** (the Grovekin's Hollow Root chambers) | Living root walls, heartwood beams, carved Ogham stones, moss carpets; light through root gaps in green shafts, glowcap clumps | **Briar snap:** dry rattling as thorn walls draw back 1.0 s before → walls of briar snap shut across the corridor, 40 + bleed 40 → walk the gaps in rhythm; fire burns a briar wall away for 30 s | **Deadfall:** a creak and a shower of bark from above 1.2 s before → a log drops, 90 damage → step through; enemies lured under it take the same | **Grow the bridge:** a chasm with root stubs on both sides and three water channels with wooden gates → open the gates so water reaches the right roots; the roots grow across in 6 s (a real visible growth animation), and close behind if the water stops | **Ogham stones:** three stone posts with tree-letter notches (Ogham: birch, rowan, alder...) and real living trees of those kinds growing in the room → match each stone to its tree by touching the stone then the tree | **Woodwose** (midland): a wild man of the woods, shaggy, bark-skinned, attractive in a feral way (strong, lean, hair down the back), with a club; club combo 0.6 / 0.2 / 0.5 hold 0.3 / 0.9, thrown stone (0.9 W, arc, landing shadow); voice: gorilla chest-beat + deep human grunt. **Barkhound** (midland): a wolf whose back is plated with bark; bite 0.5 / 0.15 / 0.7; voice: wolf growl + wood creak |
| **Graveyard** (the Lantern Mourners' catacomb vaults) | Ossuary walls of stacked skulls and long bones, tōrō stone lanterns, lead coffins on shelves, candle wax in rivers; light only from lanterns and candles (warm, pools of dark between) | **Snuffing censers:** chains clink and incense smoke thickens 1.0 s before → swinging censers sweep the corridor, 35 damage, and put out your torch for 8 s (darkness rules apply: Gravehounds hunt at double aggro, KEPT) → time it; light-bearing tames keep their light | **Ossuary slide:** bones trickle from a wall for 1.2 s → the bone wall collapses, burying a 4 m strip, 70 damage and pinned 1.5 s → move; it buries creatures too | **Lay the bones:** a lead coffin open with an outline of a skeleton; loose bones in the room → place skull, ribs, pelvis and long bones in the order the wall fresco shows a burial | **The bell order:** five hanging bells with different tones; an epitaph carved above the door lists them as a verse → ring them in the verse's order | **Bone-picker** (midland): a long, stooped thing of fused ribs that crawls the ossuary shelves, clicking; claw rake 0.55 / 0.2 / 0.7, drop from shelf 0.8; voice: hyena whoop + bone rattle. **Candlewight** (midland): a guttering human-sized figure of wax and wick; melts toward you, wax splash 0.9 W (arc, landing glow, burn 20); voice: candle hiss + a long sigh |
| **Sky islands** (the Kitewrights' cloud-workshops) | White limestone on floating rock, kite-silk sails, wind-poles with faded cloth, chain bridges, open edges over cloud; bright daylight and wind | **Gust vents:** a whistle rising 1.5 s (KEPT hazard tell) → a 1,200 cm/s gust for 0.8 s pushes you toward an open edge → brace (crouch halves it) or anchor boots; it throws creatures off too | **Snapping kite-lines:** a twang and a line shivering for 1.0 s → a taut kite line snaps and whips across a 6 m arc, 50 damage, knockback 600 → stand behind the anchor post | **Set the sails:** wind-poles with flags show the wind; three sails on a mill shaft → turn each sail to catch the wind so the shaft turns, the shaft drives the door's winch (gears visible, chain to the door) | **Balance the platform:** a hanging platform on a beam with counterweights → move weights (Interact pick up, carry, set) so the beam levels and the platform swings within reach | **Paper-wing drone** (midland): a Kitewright kite of bamboo and silk that flies itself, with a bronze blade keel; dive 0.8, slash pass 0.4; destroyed by fire in one hit; voice: silk flutter + a whistle. **Cloudling** (lowland swarm): small puffed birds that mob, 25 HP ×6 |
| **Crystal caves** (the Wardcarvers' geode vaults) | Geode halls of violet and blue crystal, carved ward lines in the floor, chime stones on cords; light from crystal glow (cool violet) and resonance pulses | **Ward field** (KEPT hazard): a faint shimmer and a rising tone 1.0 s before it switches on → 25 damage and a 600 cm throw every 1.5 s inside → read the floor ward lines; a ward charm passes | **Resonance spikes:** a chime starts low and climbs for 1.0 s → crystal spikes burst from the floor in a line, 60 damage → step off the line; striking the room's chime stone early sets them off | **Tune the stones:** four chime stones on cords and a door with a carved melody (four notes drawn as heights) → strike stones in the melody's order; wrong notes reset | **Prism path:** a beam from a crystal and 3 to 5 prisms on stands (Interact turns 45°) → carry the beam to the door's crystal lock (same as Desert's mirror relay, different look) | **Shardling** (swarm): crystal mites, 25 HP ×6, shatter in 1 m (10 damage). **Geode husk** (midland): a hollow crystal shell walking on four legs; slam 0.7, curl and roll 0.9 (rolls 10 m); shell takes ×0.3 until a strike hit cracks it; voice: armadillo hiss + glass grinding |
| **Ocean & reefs** (the Saltkin's drowned halls) | Coral-grown stone, driftwood beams, shell-chime strings, tide pools in the floor, kelp curtains; light from bioluminescent plankton (blue-green) and shafts from above water | **Tide surge:** a roar from a sea tunnel and water pulling back from the floor for 1.5 s → a wave sweeps the room, 50 damage, carries you 1,500 cm → climb a coral shelf or hold a mooring ring (Interact) | **Clam-jaw door:** a giant clam set in a doorway gapes and shivers 1.0 s → snaps shut, 100 damage, holds you 2 s → pass when it's open and still; feed it a fish to keep it open 10 s | **Shell chimes:** driftwood beacons with shell chimes; the wind (a draft from a tunnel) plays a melody → hang shells (from pools) so the chimes match the melody | **Tide gates:** three valve wheels; water levels in three basins → turn valves to set each basin to the line carved on its wall; the drowned door floats up | **Selkie raider** (midland): a seal-skinned raider who sheds the skin to fight in human form, attractive and athletic, a sealskin cloak at the shoulders and little else above the waist, with a bone harpoon; harpoon thrust 0.5 / 0.15 / 0.7, throw (0.9 W, straight, 1,400 cm/s), dives into tide pools to reposition; hide the sealskin (Interact on a dropped skin) and the raider can't flee; voice: harbour seal bark + a human war cry. **Brine eel** (midland): an eel in the floor pools; strike 0.6 from a pool, shock 30 |
| **Fungal caverns** (the Deepfarmers' mushroom halls) | Grown-cap pillars, mycelium-netted walls, clay farming terraces, glow-cap rings; light from glow-caps (soft teal) and slow pulses through the mycelium | **Puffball mines:** a puffball swells and its skin tightens with a stretching creak 1.0 s → it bursts in 3 m, poison 40 buildup → walk round; a thrown stone pops it early, poisoning creatures nearby | **Mycelial net:** white threads tremble on the floor 1.0 s → threads snap up and snare for 2 s → step over the threads (they're visible); fire burns a patch clear | **Glow-cap rings:** three fairy rings of glow-caps pulse in a sequence → step into each ring in the same order (stepping into a ring out of order snares you 1 s) | **Spore lamps:** the Deepfarmers' lamps need lighting with a spore-puff carried from a puffball (a 6 s carry before it fades) → light the lamps that the wall map shows, routing round snares | **Capling soldier** (midland): a 2 m mushroom-capped humanoid with a sharpened bone spear; thrust 0.55 / 0.15 / 0.7, spore puff 0.9 (cone, poison 25); voice: wet squelch + frog trill. **Lantern grub** (lowland): glowing grubs that burrow and ambush; bite 0.45 |
| **Corrupted wasteland** (the Hollow Choir's unmade rooms) | Grey ash stone, cairns balanced point-down, voice pillars with open carved mouths, rubble hanging mid-air; light bends near rifts; colourless except the rift edges | **Gravity flip:** a low hum and pebbles lifting off the floor for 1.0 s → gravity drops for 3 s: you float 4 m up, then fall, fall damage as normal → stand under a ledge or hold a chain (Interact) | **Null zone:** a ring of dead grey air with a soundless pulse 1.0 s before → abilities and Wildbrands fail inside for 6 s (weapons still work) → step out; creatures lose their abilities too | **Balance the cairns:** cairns balanced point-down; a door with a carving of a cairn stack → stack stones (each shows its weight and shape on inspect) to match the carving; a wrong stack falls with a clatter | **The echo room:** voices play a sequence of tones from the voice pillars → strike the pillars in the reverse order (the Choir sang backwards) | **The Unmade** (midland): a person-shaped absence, edges fraying into motes; claw combo 0.55 / 0.2 / 0.7, blink 3 m behind you 0.8 (a sound of tearing paper 0.6 s before); voice: reversed breath + owl screech. **Gnawer swarm** (KEPT void creature) |

#### 2.5.6 Portal bosses, all 12 biomes (Desert KEPT; eleven GAP-COOKED)

**The rule (lesson 8).** Ranks 1 and 2 are **an Alpha and an Elder**: one of the biome's own creatures at 1.4 to 1.8× scale on its own rig, with two new moves and one mechanic; its scars glow in the rank colour. **The same Alpha move adds are used by the Scarred beasts (2.13.3)**, so one animation set serves both. Ranks 3 to 5 are new or heavily remade bodies with real-folklore roots. All follow the mini template (4 to 5 attacks and one counter mechanic, combat cook KEPT), projectiles follow the 0.9 s rule, every attack hits on visible contact, every boss has its own voice from fused real animal calls (four layers: tell, strike, hit, pain), and the rank-5 arena is the biome's most dressed room. Desert's five unique bosses are KEPT as cooked.

**Volcanic**

| Rank | Boss | Look | Attacks (W / A / P, s) | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Cinderfang Alpha** | A Cinderfang at 1.5×, mane of open flame, white-hot scars | Bite pair 0.5 / 0.15 / 0.7; ember lunge 0.8 (8 m); **howl** 1.0 (calls 2 Cinderfangs); **pounce-and-pin** 0.9 (grab, crimson) | Its mane relights at lava runnels: while lit it deals ×1.2. A frost hit or a thrown water jar douses it for 15 s | Wolf howl + gas burner roar |
| 2 | **Kilnmite Broodqueen** | A Kilnmite at 3×, swollen ore-veined abdomen | Mandible pair 0.55 / 0.2 / 0.8; burrow and erupt 1.0 (ring of cracking ground); **slag spit** 0.9 W (ballistic, ground glow, burn 30); **egg-lay** 1.0 (4 mites) | Three egg clutches on hot rock (200 HP each); while any stands, she lays every 20 s. Break the clutches to stop the brood | Cicada drone + kettle shriek |
| 3 | **The Mould-Born** | A 4 m unfinished iron casting cracked from its sand mould: sprue spikes on the back, one arm a hammer-shaped lump, orange heat in the seams | Hammer-arm combo 0.6 / 0.2 / 0.6 hold 0.3 / 0.9; sprue burst 1.0 (5 m ring, `low`); **pour** 1.2 (steps under a crucible and tips molten iron over a 4 m circle) ; shoulder charge 0.8 | It's still cooling. Frost hardens it (stance ×1.5, damage taken ×0.8); fire heals it 1% a hit. The arena's two hanging crucibles can be tipped onto it (Interact the chain 1.5 s) for 600 damage and a 4 s stagger | Foundry clang + bull groan |
| 4 | **Brannoch, the Bellows-Keeper** | A 6 m bronze-bellied giant whose chest is a living leather bellows; arms long like forge tongs; a soot-black beard braided with iron rings | Tongs combo 0.6 / 0.2 / 0.5 hold 0.4 / 0.9; **tongs grab** 1.0 (crimson, throws you into a forge, 80 + burn 60); bellows cone 1.1 (10 m, burn 60); hammer throw 0.9 W (straight, 1,400 cm/s, never homes); slam 1.2 (punish 1.2) | Two bellows vents on his chest are lock points (800 HP each). Break one: the cone halves. Break both: the cone stops and he staggers 6 s | Elephant rumble + bellows wheeze |
| 5 | **Böðvildr, the Smith's Daughter** (Völundarkviða) | An 8 m giantess smith: ash-grey skin cracked with ember light, a broad strong build, a leather apron over bare shoulders, braids bound in iron rings; a beautiful, furious face. Her arena is the quench hall: three great water tanks round an anvil | Hammer combo ×4 0.6 → 0.5 → hold 0.4 → 0.9 / 0.2 each / 1.0; overhead slam 1.1 (leaves a lava crack 6 s); chain-hammer spin 1.1 (360°, 9 m); **anvil grab** 1.2 (crimson: lays you on the anvil and strikes, 140); quench hiss 0.9 (steam cloud, blind 30 within 6 m) | **The tanks:** bait her overhead slam onto a tank's rim and it tips: a steam burst staggers her 8 s and cools her armour (damage taken ×1.3 for 12 s). Tanks refill from a pipe in 60 s (the pipe runs from a lava-heated cistern visible behind a grille) | Lioness roar + forge quench hiss + a woman's war cry, pitched low |

**Frozen tundra**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Rimeclaw Alpha** | A Rimeclaw at 1.5×, fur iced into plates | Its built claw set (`creatures.json`) plus **frost roar** 1.0 (cone, slow 3 s) and **ice-plate charge** 0.8 (10 m) | Ice plates are stance armour (stance damage ×0.5). Fire melts them for 20 s | Bear roar + ice crack |
| 2 | **Glacimaw Elder** | A Glacimaw at 1.6×, jaw rimed with old ice | Its built bite set plus **burrow under the ice** 1.0 (cracks spread, erupts) and **hoarfrost breath** 1.0 (8 m cone, frostbite 40) | Cracks in the floor show its path; standing on the dark, thick ice when it erupts makes it stun itself 3 s | Walrus bellow + glacier groan |
| 3 | **The Frostbound Thurs** | A 4.5 m frost-giant youth on the Grimjaw troll rig, remade: blue-white skin, an icicle beard, a club of frozen pine | Club combo 0.6 / 0.2 / 0.5 / 0.9; boulder throw 0.9 W (arc, landing shadow 0.9 s); ground pound 1.0 (`low` ring, slick ice 6 s); grab 1.0 (crimson) | It regrows an ice shell (×0.4 damage) every 25 s unless it's burning or bleeding (Grimjaw's built rule, KEPT pattern) | Gorilla roar + snow crunch |
| 4 | **The Rime-Witch** | A 3 m Rime-folk seer: pale skin, blue lips, white braids, furs open at the collar over a lithe, strong body; she skates on the ice floor and carries a bone staff | Ice-needle fan 0.9 W (5 needles, straight, 1,500 cm/s); skate slash ×3 0.45 / 0.15 / 0.4 / 0.8; frost circle 1.1 (ring, freeze 1.5 s); **mirror clones** 1.0 (2 ice clones copy her first swing, then shatter) | **The braziers:** standing in lit brazier light stops the freeze. She douses one brazier with snow every 25 s; relighting takes 2 s | Arctic fox scream + a woman's breath on cold glass |
| 5 | **Hrímgerðr, the Glacier's Daughter** (Helgakviða Hjörvarðssonar) | A 10 m frost giantess in a cave under the glacier; skin like blue marble, white hair to the waist, a broad, attractive, cruel face; frost crusts her shoulders and hips | Fist combo ×3 0.7 / 0.25 / 0.6 / 1.0; stomp 1.0 (`low` ring); **frost breath** 1.2 (12 m cone, sweeps 90°); grab 1.1 (crimson, crushes, 150); avalanche call 1.3 (ice blocks fall in growing shadows for 4 s) | **Let dawn in:** the cave's east wall is an ice window held by 4 frost bolts (lock points, 600 HP each). Each bolt broken lets in a shaft of dawn: while she stands in the light she slows (×0.7) and takes ×1.5 as her skin turns to stone (15 s), as in the poem | Elk bugle + ice sheet boom + a deep female laugh |

**Storm peaks**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Voltfang Alpha** | Voltfang at 1.5×, fur standing on end with crawling arcs | Bite pair 0.5 / 0.15 / 0.7; flank leap 0.8; **arc howl** 1.0 (shock 40 within 5 m); **chain bite** 0.9 (grab, crimson) | Every 15 s it charges (fur glows brighter): its next hit chains to a second player. Hitting it with water or a tide soak drains the charge | Wolf howl + electric crackle (KEPT voice rule) |
| 2 | **Stormkite Brood-mother** | Stormkite at 1.8×, tail of copper-bright feathers | Talon dive 0.9; tail lash 0.6; **charged dive** 1.1 (strikes the ground in a 4 m ring, shock 60); calls 2 Stormkites 1.0 | Grounded by arrows to the wings (6 hits, FULL-GAME Kharvox pattern); grounded 8 s | Red-tailed hawk scream + spark gap |
| 3 | **The Rodwarden Construct** | A 4 m copper-and-oak automaton with a lightning-rod crest, glass insulator joints, a halberd | Halberd combo 0.6 / 0.2 / 0.5 hold 0.3 / 0.9; sweep 0.8; **crest strike** 1.1 (calls lightning on a marked 3 m circle, 1.0 s ground glow); overload 1.2 (5 m nova, shock 60) | Its insulator joints (4 lock points): breaking one makes it shock itself on every crest strike (10% of its HP) | Ratchet clicks + transformer hum + a ship's bell |
| 4 | **Tanngrisnir, the Thunder Goat** (Prose Edda) | A 5 m goat with iron-dark horns spiralling forward, a coat that crackles; its arena floor is scattered with its own old bones | Horn charge 0.9 (12 m); rear and stamp 1.0 (`low` ring, shock 50); head-butt combo 0.55 / 0.15 / 0.8; **thunder bleat** 1.0 (cone, stagger 0.6 s) | **The bones:** at 0 HP it falls, and 6 s later rises from its bones at 40% HP, unless you broke its right hind leg (a lock point, 900 HP) in the first life: then it rises lame (move speed ×0.6, no charge), as the goat that came back lame in the Edda | Goat scream + thunderclap |
| 5 | **Lei, the Thunder Drummer** (Lei Gong) | A 9 m winged figure with a hawk-like beak and taloned feet, carrying a chisel and mallet; a ring of eight great drums on stands circles the arena; a mirror-woman of light (Dianmu) flickers on the walls before each strike | Mallet combo 0.6 / 0.2 / 0.5 / 0.9; talon kick 0.7; **drum strike** 1.0 (he beats a drum: a lightning line runs 25 m from that drum in its facing, 1.0 s line glow); **mirror flash** 1.1 (the wall-mirror flashes: blind 40 for anyone facing it; look away); dive 1.2 | **Silence the drums:** each drum skin has 400 HP. A broken drum can't fire its line. With all eight broken he lands and fights on foot (no flight), and takes ×1.25 | Eagle scream + taiko boom + thunder |

**Swamp**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Murkjaw Alpha** | Murkjaw at 1.5×, scarred hide | Its built bite set plus **death roll** 1.0 (grab, crimson, 90) and **tail sweep** 0.8 (`low`) | It submerges in bleedwater to heal 1%/s; draining the pool (pull the chain, 2 s) leaves it stranded | Crocodile hiss + bullfrog (KEPT voice) |
| 2 | **Gorgeleech Elder** | Gorgeleech at 1.8× | Lunge latch 0.8 (grab, drains 15/s until hit off); body slam 0.9; **blood spray** 1.0 W (arc, bleed 40) | Salt (rock salt from the shelves, thrown) makes it recoil and stun 3 s, as real leeches do | Wet suction + elephant seal |
| 3 | **The Hex-Bound** | A 4 m Hexcrow remade: man-tall legs, wings like torn cloaks, a beak hung with bone charms | Beak combo 0.5 / 0.15 / 0.4 / 0.8; wing buffet 0.8 (knockback); **hex mark** 1.0 W (a slow black orb, straight, 900 cm/s; a hit applies curse 60); murder call 1.0 (4 crows) | Its bone charms (3 on the beak, lock points) feed the curses: each broken charm halves the curse buildup | Raven croak + bone rattle |
| 4 | **Black Annis** (Leicestershire folklore) | A 4 m blue-faced hag with iron claws and one eye, in a cave hung with flayed hides; long, lean, terrifying | Claw combo ×3 0.5 / 0.15 / 0.45 / 0.9; **leap from the oak** 1.0 (she climbs her oak pillar and drops); grab 1.0 (crimson, rake 110); scream 0.9 (cone, stagger 0.6) | **Her oak:** she always climbs the hollow oak in the middle to heal 2% a second inside it. Burning the oak (fire, 8 s) shuts her out for the rest of the fight | Barn owl scream + a cat yowl + an old woman's cackle |
| 5 | **The Hut on Fowl's Legs and its Mistress** (Baba Yaga) | A 10 m log hut on two giant chicken legs, scaled and taloned, with a smoking chimney; its mistress, a tall hag, flies round it in a mortar, steering with a pestle and sweeping her trail with a broom | Hut: talon stomp 1.0 (`low`), kick 0.9, **turning** 1.2 (the hut spins, its door sweeps 8 m). Mistress: pestle strike 0.7 (diving pass), **broom sweep** 0.9 (wipes your marks and buffs), bone fence 1.1 (summons a ring of skull fence posts that burn) | **"Turn your back to the forest, your front to me":** the hut's door faces away. Shout the words (Interact at the hut's front for 2 s while it stands still after a stomp) and the hut kneels, door toward you, for 10 s: the mistress lands at the door and takes ×1.5. Kill the mistress and the hut collapses | Rooster crow slowed down + timber groan + a crone's laugh |

**Ancient forest**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Mossmaul Alpha** | Mossmaul at 1.4×, moss grown into a mane | Swipe pair 0.6 / 0.2 / 0.8; maul charge 0.9; **rear slam** 1.0 (`low`); **bear hug** 1.0 (grab, crimson) | Moss regrows its HP 1%/s in shade; fire on the moss stops it 20 s | Grizzly roar + creaking wood |
| 2 | **Rootcoil Elder** | Rootcoil at 1.8×, bark-scaled | Strike 0.6; constrict 1.0 (grab, crimson, 20/s); tail whip 0.8; **burrow** 1.0 | While coiled on someone, its head is a lock point taking ×2; allies break the hold by hitting the head (300 damage) | Python hiss + root tearing |
| 3 | **Thornlynx Ghost** | A pale, oversized Thornlynx on its rig, fur white with green eyes, silent | Canopy drop 0.9; claw pair 0.5 / 0.15 / 0.7; **vanish** 1.0 (into the canopy); pounce 0.8 | Like the Grey Matron (KEPT forest rule), it hunts from walkways; the room's walkways have rope ties (200 HP): cut them and it falls and stays grounded | Lynx scream + a silent breath |
| 4 | **The Leshy** (Slavic) | A forest spirit as tall as the trees round it (it grows and shrinks), bark skin, eyes like coals, beard of moss, green hair; a figure that reads as a tree until it moves | Branch combo 0.7 / 0.2 / 0.6 / 1.0; **grow** 1.0 (it grows to 9 m and slams, `low` ring); whirlwind 1.1 (pulls leaves into a 6 m vortex); **lead astray** 1.2 (the room fills with 4 false Leshies that copy it) | **Turn your cloak:** wearing your clothes inside out breaks the Leshy's spell in Slavic belief. Interact "Turn your cloak" (3 s, any time): for 15 s the false Leshies vanish for you, and the real one takes ×1.25 from you | Elk groan + wind through branches + a man's laugh |
| 5 | **The Green Knight of the Hollow Root** (Sir Gawain and the Green Knight) | An 8 m knight of green skin and holly, a great green beard, a handsome hard face, a huge axe and a holly bough; his hall is ringed by holly | Axe combo 0.7 / 0.25 / 0.6 / 1.0; axe sweep 1.1 (300°); bough lash 0.8 (bleed 30); **the return blow** 1.2 (crimson: an overhead chop on whoever dealt the last heavy hit) | **The beheading game:** at 50% he kneels and offers his neck (6 s). A heavy hit to the neck beheads him: he picks up his head and fights on, but the head on the floor is a lock point (1,500 HP) and the body takes ×0.5 while the head lives. Break the head to finish him. If nobody takes the offer, he rises at full stance and the return blow comes twice as often | Stag roar + deep male laugh + leaves |

**Graveyard**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Gravehound Alpha** | Gravehound at 1.5×, ribs showing through grave-soil fur | Bite pair 0.5 / 0.15 / 0.7; lunge 0.8; **grave howl** 1.0 (calls 2 hounds); **drag** 0.9 (grab, crimson) | Hunts at double aggro outside light (KEPT); inside a lit circle (lanterns, 3 in the arena) it takes ×1.25 | Hyena whoop + mastiff growl |
| 2 | **Cryptstalker Elder** | Cryptstalker at 1.6× | Its built set plus **wall crawl ambush** 1.0 and **shroud** 1.0 (darkness 6 m, 8 s) | Ringing the arena bell reveals it for 6 s | Leopard growl + bell hum |
| 3 | **The Charnel Knight** | A 3.5 m skeleton knight in rusted plate, a coffin lid for a shield, a broad sword; bones bound with lead wire | Sword combo 0.6 / 0.2 / 0.5 / 0.9; shield bash 0.7; **coffin slam** 1.0 (`low`); grab 1.0 (crimson, impales) | Its lead wire binds the bones (3 lock points on arms and hip); each broken lets a limb fall loose: arm (no shield), hip (slow) | Armour grind + bone rattle + horse snort |
| 4 | **The Ankou** (Brittany) | A tall, gaunt figure in a broad hat and long coat, a skull face, a scythe with the blade on backwards (so it cuts outward); he walks beside a creaking cart pulled by two skeletal horses | Scythe outward sweep 0.9 (300°); thrust 0.6; **cart run** 1.2 (the cart charges a lane, 100, a 1.0 s creak before it moves); **collect** 1.1 (crimson grab: he lifts you into the cart; break out by allies hitting the cart, 300 HP, within 4 s, or die) | **The cart's creak** is his tell for everything; stopping the cart (break both horses' bone harness, 600 HP each) leaves him on foot and slow | Cart wheel creak + horse whinny slowed + a long exhale |
| 5 | **The Mari Lwyd** (Wales) | A 6 m horse skull on a pole carried by an unseen bearer under a white sheet, ribbons and bells hanging from it, glass bottle-ends for eyes, snapping jaws on a wire; it lurches | Jaw snap 0.6 / 0.2 / 0.8; lurch 0.9 (8 m); bell shake 1.0 (stagger 0.6 in 6 m); **wassail grab** 1.1 (crimson) | **The pwnco (battle of verses):** every 30 s it sings a 3-note phrase on its bells. Answer by striking the arena's three bells in the same order within 6 s: it reels and is stance-broken (crit). A wrong or missed answer: it heals 3% and grows bolder (faster, 10 s) | Horse scream + sleigh bells + a men's choir humming |

**Sky islands**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Galeharrier Alpha** | Galeharrier at 1.5× | Talon dive 0.9; wing buffet 0.7 (knockback toward edges); **gale screech** 1.0 (cone, push 600); grab-and-drop 1.0 (crimson, drops you from 8 m) | Arrows to the wings ground it (6 hits) for 8 s | Falcon scream + wind gust |
| 2 | **Skyray Elder** | Skyray at 1.8× | Gust inhale 1.0 (pulls 400 to an edge); tail sting 0.6; **dive sweep** 1.0 | Its gill slits glow before the inhale; a heavy hit to them cancels it | Whale call pitched up (KEPT) + air rush |
| 3 | **The Lift Golem** | A 4 m construct of a Kitewright cage-lift: an iron cage body hung from a mast, chain arms with hook ends, a counterweight block for a fist; it rides a real chain run between two islands, its winch gears turning as it moves | Hook swing 0.7; counterweight slam 1.0 (`low`); **chain reel** 0.9 (hooks you and reels you in, grab, crimson); ride the chain 1.0 (moves to the other island along the chain, smoothly, its pulley wheels turning on the chain) | **The winch:** its power comes through the chain from a winch house on one island. Jam the winch with a dropped bar (Interact 2 s while the golem is on the far side): it's stuck mid-chain, hanging, and takes ×1.5 for 10 s | Chain clatter + iron groan + a gull |
| 4 | **The Stymphalian Matron** (Heracles' sixth labour) | A 5 m crane-like bird with bronze feathers, a bronze beak, and a flock of 6 smaller bronze birds | Beak stab 0.6 / 0.2 / 0.8; **feather volley** 1.0 W (6 bronze feathers, straight, 1,500 cm/s, never home); wing cut 0.8; flock dive (the flock dives one by one, 0.9 each) | **The krotala:** two bronze rattles hang on posts; shaking one (Interact 2 s) drives the flock up into the air and off for 12 s, and the matron lands, exposed (×1.3) | Crane bugle + scraping bronze |
| 5 | **The Ziz, Wing-of-Shadow** (Jewish legend) | A bird so vast its wings shade the whole island; feathers like storm clouds, a pale beak, eyes like suns; the arena is a ring island with four iron harpoon winches | Wing beat 1.2 (a gust across the island, 1,200 cm/s, 1.5 s whistle); talon sweep 1.0; **shadow pass** 1.3 (darkness over the island for 6 s, then a dive at the brightest light); beak strike 0.8 when perched | **Pin the wings:** four harpoon winches (Interact 3 s to fire, then crank) fire chained harpoons into its wings when it passes low (a 1.0 s low-pass tell). Two harpoons pin it to the island for 15 s (×1.5, its beak and chest are lock points); all four pin it for good | Albatross call + thunder rumble + a whale's sub-bass |

**Crystal caves**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Prismback Alpha** | Prismback at 1.5× | Its built set plus **prism beam** 1.0 W (line, 15 m, locks at windup end) and **crystal shell** 1.0 (×0.3 damage 5 s) | Strike hits break the shell early | Armadillo hiss + glass chime |
| 2 | **Echobat Colony-mother** | Echobat at 2× | Sonic cone 1.0 (stagger); claw pass 0.6; **echo call** 1.0 (4 bats) | Ringing the room's chime stone confuses her echolocation: she drops and stuns 4 s | Bat chirp pitched down + crystal ring |
| 3 | **The Chime Golem** | A 4 m golem of chime stones on cords, swinging as it walks, each stone a different note | Chime slam 0.8; sweep 0.9; **resonance** 1.1 (spikes in a ring); stone toss 0.9 W (arc) | Its stones ring before each attack: each attack has its own note, so players learn the song | Bell tones + stone grind |
| 4 | **The Smoking Mirror** (Tezcatlipoca) | A 5 m jaguar-man of obsidian, one foot replaced by a smoking obsidian mirror, jaguar-skin pelt, an attractive, fierce human face under a jaguar helm | Obsidian blade combo 0.55 / 0.15 / 0.45 / 0.9; pounce 0.9; **mirror strike** 1.0 (he steps into a floor mirror and comes out of another); smoke 1.0 (darkness 6 m) | **The mirror shows the future:** the obsidian mirror on his foot reflects where he will strike next (a reflected image of him 0.8 s ahead). Players who watch his foot read every attack | Jaguar cough + obsidian scrape + a conch blast |
| 5 | **The Sampo Engine** (Kalevala) | A 12 m crystal mill forged by the Wardcarvers: three great grinding stones (grain, salt, gold) turning on a central shaft, driven by a water wheel under the floor; its lid is carved with stars; arms of brass gearing sweep out from the shaft | Arm sweep 1.0 (the gearing arm swings, `low` and `high` alternating); **grind** 1.2 (it pulls everyone within 8 m toward a turning stone, 40/s if you touch it); salt hail 1.0 W (ballistic, landing glows); gold flood 1.3 (molten gold pours from a spout, a 6 m strip) | **Jam the stones:** each grinding stone has a gap that opens once per turn (every 6 s); dropping a crowbar from the racks into the gap (Interact 1.5 s) jams it. Each jammed stone stops one attack. All three jammed: the shaft cracks and the core shows (×2 damage). The water wheel below turns the shaft through visible bevel gears; nothing floats | Mill grind + water rush + a choir drone |

**Ocean & reefs**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Reefjaw Alpha** | Reefjaw at 1.5× | Bite 0.6; lunge from water 0.9; **thrash** 1.0 (ring); **drag under** 1.0 (grab, crimson) | It heals in water; bait it onto the reef shelf and it flounders 4 s | Seal roar + shark thrash splash |
| 2 | **Spinecrab Elder** | Spinecrab at 2× | Claw pair 0.6 / 0.2 / 0.8; spine burst 1.0 (ring); **claw grab** 1.0 (crimson) | Its claws are lock points (700 HP); break one and its grab is gone | Crab clack + walrus grunt |
| 3 | **Tidecoil Matron** | A 15 m Tidecoil remade, a barnacled sea serpent in a tidal chamber | Coil sweep 1.0; strike 0.7; **tide call** 1.2 (floods the chamber 4 s); spit 0.9 W (straight) | The chamber floods on a 40 s cycle; at low tide her coils lie on the floor as lock points | Moray hiss + surf |
| 4 | **The Each-Uisge** (Scottish water horse) | A 4 m black horse with a wet, glistening hide, weed in its mane, a mouth of sharp teeth; it walks out of a sea pool | Bite 0.6; rear and stamp 1.0; **gallop** 0.9 (12 m); **sticking hide** 1.0 (crimson: touch glues you to its side; it races for the pool to drown you; allies break it with 400 damage to its head, or die in the pool) | Fight it on the sand: every 20 s out of water its hide dries and it takes ×1.25. A tide surge (room trap) wets it again | Horse whinny + water gurgle + a seal |
| 5 | **The Umibōzu** (Japanese) | A 14 m smooth, black, shaven-headed sea giant rising from the water round a wrecked ship's hull, eyes like lamps; the arena is the deck and the hull | Hand slam 1.1 (on the deck, `low`); sweep 1.2; **capsize** 1.3 (rocks the hull: everyone slides 600 to the low side); **barrel demand** 1.0 (he asks for a barrel: a low drone) | **Give him a bottomless barrel:** when he asks, roll a barrel with no bottom (racks on deck; Interact to knock its bottom out, 2 s) to the hull edge. He scoops water into it in vain, stunned 10 s (×1.5). A barrel with a bottom: he fills it and sinks the deck section | Whale song slowed + ship's timbers + a temple bell |

**Fungal caverns**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Sporestalker Alpha** | Sporestalker at 1.5× | Its built set plus **spore burst** 1.0 (poison 40 in 4 m) and **ambush** 0.9 | In glow-cap light it can't hide | Leopard growl + wet puff |
| 2 | **Rotmaw Elder** | Rotmaw at 1.8× | Bite 0.6; rot vomit 1.0 W (arc, rot 40); **charge** 0.9 | Burning the rot pools it spits stops its regen | Boar squeal + squelch |
| 3 | **Webspinner Matriarch** | Webspinner at 2.2×, webs in the room | Fang strike 0.6; web shot 0.9 W (straight, snare 2 s); **drop from web** 1.0; spiderlings 1.0 | Her web anchors (4) hold the ceiling web: cut them and she falls | Hissing + silk tearing |
| 4 | **The Ring-Dancer** (fairy rings) | A 3 m elf of the deep: long-limbed, beautiful, pale with spore freckles, a crown of glow-caps; she dances | Dancing cuts ×3 0.45 / 0.15 / 0.4 / 0.8; leap 0.9; **fairy ring** 1.0 (a glowing ring springs up under a player: caught inside, you dance helpless for 3 s); spore kiss 0.9 W (straight, poison) | **The ring:** a dancer can be pulled out by an ally (Interact 1 s). Rings she raises stay on the floor and pulse; she gains ×1.2 damage while 4+ rings stand. Trampling a ring (sprinting across it outside its pulse) breaks it | Nightjar churr + a woman's laughter + pipes |
| 5 | **The Ophio Crown** (Ophiocordyceps) | A giant Webspinner host, dead-eyed, walking stiffly, with a 10 m fungal stalk bursting from its head that sways and steers it; the stalk's tip swells with spores | Host: leg stab 0.7, body slam 1.0 (`low`), climb the wall 1.1 (it carries the stalk high, as real hosts climb to fruit). Stalk: **spore rain** 1.2 W (falling spore clumps with growing shadows, poison 50), lash 0.9 | **Cut the stalk:** the stalk is a lock point (2,000 HP). When it's cut, the host goes wild and blind (×1.3 damage dealt, attacks at random, every tell still shown) for 20 s and then dies. If the stalk reaches the ceiling (climb fully, 3 times), it fruits: a poison flood of the arena, 20/s, until you break the fruiting body (600 HP) | Insect click + wet creak + a low organ note |

**Corrupted wasteland**

| Rank | Boss | Look | Attacks | Mechanic | Voice |
|---|---|---|---|---|---|
| 1 | **Hollowfang Alpha** | Hollowfang at 1.5× | Bite pair 0.5 / 0.15 / 0.7; blink flank 0.8 (tearing-paper sound 0.6 s before); **void howl** 1.0 (max HP drain 2/s in 6 m for 5 s); drag 0.9 (crimson) | Arcane hits pin its blink for 10 s (opposite rule) | Wolf howl reversed + static |
| 2 | **Riftmaw Elder** | Riftmaw at 1.8× | Burst from rift 1.0; bite 0.7; **swallow** 1.1 (crimson) | Arcane wards close its rifts (the KEPT Mother of Rifts rule); with no rifts it can't reposition | Reversed roar + wet tearing |
| 3 | **Nullwing Matriarch** | Nullwing at 2× | Dive 0.9; void trail 1.0 (null strip); **wing screech** 1.0 (cone, stagger) | Its trails cancel abilities; fight it from outside the trails | Barn owl reversed + hum |
| 4 | **The Backwards Man** | A 3 m grey figure who moves in reversed time: dust falls up off him, his footprints appear before he steps | Every attack plays backwards: the **result appears first** (the crack in the floor, the slash mark in the air) for 0.9 s, then he moves through it: crack slam 0.9; slash line 0.9; dash 0.9 (his after-image appears at the end point first); **unmake** 1.2 (crimson grab) | Reading the after-images is the fight. Standing on a mark before it fills cancels that attack and staggers him 2 s (he can't move into a space someone holds) | Every voice layer reversed: breath, a man's cry, a bell |
| 5 | **The Choir Unsung** | A 10 m standing ruin of voice pillars fused into a body: a cathedral of open mouths, a crown of balanced cairn stones, two long arms of rubble | Rubble arm sweep 1.1; slam 1.0 (`low`); **the unsung note** 1.3 (every mouth opens; sound cone 20 m, strips buffs, 60 damage); **choir pull** 1.2 (pulls everyone 600 toward it); cairn rain 1.2 W (falling cairn stones with shadows) | **Sing it back:** each of its 6 mouths (lock points, 600 HP) sings a part. Every mouth broken weakens the unsung note by a sixth. When its cairn crown is struck by a heavy hit at the top of its pull (it bows forward, 1.0 s), the crown falls and it kneels 8 s | A choir reversed + sub-bass + silence (a planned 1.5 s silence before the unsung note) |

**Portal boss loot rows.** Each boss drops its rank's boss roll (2.5.4) plus one trophy item for decor (comfort 1 at ranks 1 to 3, 2 at ranks 4 to 5), named for the boss (for example "Böðvildr's iron braid ring", "the Mari Lwyd's bell").

#### 2.5.7 The endless tower (DREAM, BP Dream list; designed so it's ready)

| Part | Rule |
|---|---|
| Where | **The Thirteenth Stair:** a stair cut into the Crown's south face beside the twelve broken stairs (world-map KEPT). It is built in no people's stone (plain grey). At its top, at 700 m where every other stair breaks off, a permanent portal of no colour opens into the tower |
| What it is | Floors of 3 rooms each, drawn from all twelve biome room kits (handmade rooms, random pick, never generated, DD rule kept). Every 5th floor is a portal boss: floors 5 to 25 take rank 1 to 5 bosses from a random biome; from floor 30 on, rank 5 bosses with +1 modifier every 10 floors (stacking up to 6) |
| Entry | Any party; begins at floor 1, or at any 10th floor your party has already reached (no skipping ahead of your own record) |
| Leaving | A run ends when the whole party is dead or leaves; it can't be saved mid-run |
| Leaderboard | Per server: deepest floor and time by band (solo, 2, 3 to 4, 5 to 7, 8 to 10, 11+), KEPT bands from records. Shown on a stone at the stair's foot and in the Journal |
| Rewards | Each 10th floor reached for the first time: a Legendary shard. Floors past 50: portal rank 5 boss rolls. No currency (DD) |
| Anti-cheat | Server-side: runs are timed and judged on the host; times from modded clients don't count (the host is the authority) |

### 2.6 Artifacts (8.3; SPEC frame, everything else GAP-COOKED)

#### 2.6.1 Raven's rules (SPEC, DD "How artifacts work")

Dropped by bosses and found in portals. Three dedicated artifact slots per character. In a party, the damage and loot multipliers are shared, using the highest in the party; weird rules apply only to the wearer. Penalties: take more damage, plus weird rules. Rewards: more loot, better rarity, more path XP, exclusive drops at high artifact levels. PvE only: artifacts never change damage from other players. Hidden third boss phases need artifacts (BO; BUILT `WRBoss.cpp` `AnyArtifactModifiers`, `phase3At` 0.25, `phase3DamageScale` 1.4). Weird rules only on the wearer is fixed on every server (SS). "Artifact modifier limits" is a full-release server setting (SS).

#### 2.6.2 The thirty artifacts (each one rule, written as a geis)

An artifact is a small worn object on a charm cord at the hero's belt (three hooks: the three slots). Each has a **Weight** from I to V (2.6.3) shown by its metal: **bone (I), bronze (II), silver (III), blackened iron (IV), starsteel (V)**. Its weird rule never changes with Weight; the Weight only sets how much it adds to the shared penalty and rewards. Raven's six example rules are the first six below.

| # | Artifact | Source | Geis (the weird rule, wearer only) | Look |
|---|---|---|---|---|
| 1 | **Wyrm's Stillness** | Skaldrith | No dodge roll. Backstep and jump still work (Raven's example) | A curled ice-white wyrm scale on a thong |
| 2 | **Hive Seed** | Mycelorn | Enemies you kill burst: 1.0 s swell, then 3 m, 40 damage + poison 30, to you only (Raven's example; it can't hurt allies, weird rules are wearer-only) | A dried spore pod in a wire cage |
| 3 | **Forge-brand** | Vulkarn | Your own path element hurts you: each path ability you use deals 8% of its damage to you, no status (Raven's example) | A smith's nail bent into a ring |
| 4 | **Leech Tooth** | Vharra | Healing potions heal half (Raven's example) | A three-pointed leech jaw in amber |
| 5 | **Omen of the End** | Vorhollow | When a boss targets you, it uses its next phase's attacks: phase-2 attacks in phase 1, hidden phase-3 attacks in phase 2 if the fight has phase 3 (Raven's example) | A pebble that floats a finger's width above its cord |
| 6 | **Mourner's Veil** | Nhal | Your light radius is halved: torches, lanterns, Wickbeetles, glow tames (Raven's example) | A scrap of black lace with a tiny silver bell |
| 7 | **Rod-crown** | Kharvox | Lightning finds you: outdoors in rain or storm, a strike lands on you every 40 to 60 s (1.0 s ground glow, 60 damage, 0.8 s stun). Grounded and grounding charms don't stop it | A ring of green copper wire with a fulgurite bead |
| 8 | **Gnomon Eye** | Ahrumet | Enemy health bars are hidden. You read their bodies instead: limping, cracked armour and laboured breath at 50% and 25% (combat cook wound stages) | A sunstone eye in a bronze lid |
| 9 | **Seed-heart** | Sylvaroth | Potions work only standing still: drinking roots you and takes 1.5 s instead of 0.9 s | A heartwood acorn |
| 10 | **Gale Feather** | Aeolith | Knockback on you ×2, and every heavy hit knocks you down for 1.2 s | A long white feather bound with silk |
| 11 | **Glass Guard** | Lyrathis | Blocking costs ×2 stamina | A violet crystal shard |
| 12 | **Undertow Pearl** | Thalassor | Your stamina regenerates only while you're moving | A black pearl |
| 13 | **Bellows Lung** | Volcanic portals | Stamina regen ×0.7 | A tiny leather bellows |
| 14 | **Rime Ring** | Tundra portals | A hit that lands while your stamina is below 30 freezes you 0.8 s | A ring of blue glacier ice that never melts |
| 15 | **Copper Tongue** | Storm portals | Your parry window is 30% shorter | A copper tongue-plate from a Rodwarden bell |
| 16 | **Open Vein** | Swamp portals | You can't be healed above 70% max HP | A bone hook with a red thread |
| 17 | **The Feather** | Desert portals | A hit taken within 2 s after you kill something deals ×2 to you (the Weighing of the Heart) | An ostrich feather in a bronze clip |
| 18 | **Thorn Oath** | Forest portals | You can't block. Parry still works | A twist of briar |
| 19 | **Kite String** | Sky portals | Fall damage starts at 3 m and kills at 10 m; Featherfall and gliders don't help | A coil of kite line on a bone reel |
| 20 | **Wardcarver's Chime** | Crystal portals | Every spell you cast rings out: creatures within 40 m hear it and come | A small chime stone |
| 21 | **Lonely Tide** | Ocean portals | Your tames won't fight; in a boss fight they wait at the arena door (outside the chase radius, so they don't add boss HP) | A Saltkin shell with a hole worn through |
| 22 | **Grave Count** | Graveyard portals | No run-back: if you die in a boss fight, you can't damage that boss again until the fight resets (Halo's Iron) | A lead coffin nail |
| 23 | **Mycelial Sight** | Fungal portals | Your own HUD is hidden: health, stamina and resource bars vanish. Your body tells you: breathing, a red vignette under 30% HP, a heartbeat in the rumble | A glow-cap button |
| 24 | **Hollow Coin** | Void portals | Every hit you take lowers your max HP by 1 until you rest | A coin with a hole where its face was |
| 25 | **Connla's first geis: never turn back** | Fire/Frost raid | You can't roll or backstep away from your lock-on target (unlocked: away from where the camera faces) | A knot of red and white cord |
| 26 | **Blood-price** | Storm/Blood raid | Every Wildbrand and spell also costs 5 HP | A drake's fang in copper wire |
| 27 | **Conaire's geis: never go sunwise** | Sun/Tide raid | Circling a target clockwise (sunwise) costs 6 stamina a second | A sun disc with a crack across it |
| 28 | **Connla's third geis: never tell your name** | Nature/Necro raid | Ally heals, buffs and revives don't reach you; your nameplate is hidden | A blank bone tag |
| 29 | **Connla's second geis: never refuse a challenge** | Wind/Spore raid | Every creature that sees you aggros; you can't sneak; bosses chase you to twice their radius (180 m for majors) | A wind-pole scrap knotted round a spore |
| 30 | **The Hag's Offer** (Cú Chulainn's geis) | Void/Arcane raid | The first time each fight that your HP falls under 50%, you drink a potion at once, whether you want to or not (it spends the slot and roots you 0.9 s) | A cracked clay cup on a cord |

Every rule is checked against the bar: none hides an enemy's tell, none adds homing, none makes another player weaker. Gnomon Eye and Mycelial Sight hide bars, never cues.

#### 2.6.3 The numbers: Artifact Level, penalty, rewards, party (GAP-COOKED)

**Artifact Level (AL)** = the sum of your three artifacts' Weights (I = 1 to V = 5). Range 0 to 15.

| AL effect | Per AL | At AL 5 | At AL 10 | At AL 15 | Shared with party? |
|---|---|---|---|---|---|
| **Damage taken from creatures and bosses** | +7% | +35% | +70% | +105% | Yes (highest in party) |
| **Loot quantity** (materials, ore, chests, shards) | +5% | +25% | +50% | +75% | Yes (highest in party) |
| **Better rarity**: boss part chance (BUILT `partChance` 0.25) | +1 point | 30% | 35% | 40% | Yes |
| Better rarity: crafted-quality odds on dropped gear | +2% chance per AL to step up one quality | +10% | +20% | +30% | Yes |
| **Path XP** | +4% | +20% | +40% | +60% (inside the ×2 cap, 2.2) | Yes |
| Artifact drop chance | +1 point | | | | Yes |
| **Exclusive drops** | | **AL 5+:** a dropped boss part is **Scarred** 25% of the time (its effect ×1.5, a scar mark on its icon) | **AL 10+:** artifacts drop at Weight III to V; boss soul chance up (2.7) | **AL 12+:** "Geis-burned" weapon glow, 5% per major kill (one per boss). **AL 15:** the Geis records' last step (2.6.5) | Yes |
| Hidden boss phase 3 | Opens at AL 1+ (2.6.4) | | | | By the top party |

**Who is "the party" (GAP-COOKED, so a stranger's artifacts never touch you).** Your party is you, your clan members in the same fight, and your **band**: a temporary group formed by holding Interact on another player and choosing "Invite to band" (they accept with A / click). A band has no size limit (DD: no party limits) and ends when everyone leaves it or logs off for 10 minutes. In a boss fight, the party is the members of your party inside the chase radius; in a portal or raid, the members inside the same instance. **Rival clans at a contested boss each use their own party's highest AL.** That keeps Raven's rule that no player makes a biome harder for anyone else.

**PvE only (SPEC), made exact.** Artifacts never change damage between players. If you deal or take player damage, all your artifact effects (penalty, rewards and weird rules) pause for 30 s, and the charms on your belt dim to grey so others see it. They come back on their own.

**Changing artifacts.** Hooking or unhooking a charm takes 2 s (the hero ties it on), only out of combat, never inside 90 m of an engaged boss and never inside a portal or raid (your artifacts are locked as you enter). Artifacts stay active everywhere else, including the overworld, except in safe zones, where they pause.

#### 2.6.4 Hidden phase 3 (BUILT flag; trigger GAP-COOKED)

Built today: phase 3 opens below 25% if **any** player within the chase radius has `bArtifactModifiers` (`WRBoss.cpp` 424 to 431, 719). That lets a rival's artifact change everyone's fight. **New rule:** phase 3 opens if **the party with the largest share of the damage ledger** has AL 1 or more. Phase 3's ×1.4 damage (BUILT) stacks with the AL penalty. Every major's phase 3 is already designed in its biome cook (all KEPT).

#### 2.6.5 Geis records: one-time rewards per boss per step (GAP-COOKED, the Hades bounty lesson)

For each major boss and each raid champion, your character remembers the **highest AL you've killed it at** (with your 10% dealt). Each new highest pays once:

| New highest AL on that boss | Reward |
|---|---|
| Any step (1 to 15) | Path XP 300 × that AL to your main path (AL 15 step: 4,500) |
| First kill at AL 5+ | A guaranteed Scarred boss part |
| First kill at AL 10+ | A guaranteed boss soul (2.7) |
| First kill at AL 15 | Title "<Boss>'s Bane" (for example "Vulkarn's Bane"), the boss's Geis-burned weapon glow, and a geis-knot border on that boss's monument plaque |

Twelve majors × 15 steps plus 12 champions × 15 steps is 360 one-time steps: the long climb, and each one is reachable from the last.

#### 2.6.6 Getting artifacts, and fusing them (GAP-COOKED)

| Source | Chance per looting player | Weight |
|---|---|---|
| **First artifact (onboarding):** the first kill of your 4th distinct major | 100%: that boss's artifact at Weight I, flying to you with the first-kill drop (KEPT 6 s rule), with a one-line card: "An artifact. Wear it, and the land fights harder, and pays more." | I |
| Major boss kill | 8% + 1 point per AL (AL 15: 23%); hidden phase 3 kills ×1.5 | Party AL 0 to 4: I 60% / II 30% / III 10%. AL 5 to 9: II / III / IV at 60 / 30 / 10. AL 10 to 15: III / IV / V at 60 / 30 / 10 |
| Portal rank 3 / 4 / 5 (KEPT desert chances) | 3% / 8% / 15% of that biome's portal artifact | Rank 3: I 70 / II 30. Rank 4: II 70 / III 30. Rank 5: III 60 / IV 30 / V 10 |
| Raid champion | 10% of that raid's artifact | III / IV / V at 60 / 30 / 10 |

**Fusing.** At the alchemy table (Hearth 4+): two of the same artifact at Weight n make one at Weight n+1, plus the artifact's biome signature resource ×5n (raid artifacts: both champions' resources ×3n each). Weight V is the top. **Salvage** a spare artifact at the alchemy table for that biome's path essence ×2 per Weight.

#### 2.6.7 Server settings (SS SPEC "artifact modifier limits"; values GAP-COOKED)

| Setting | Options | PvP standard | PvE | Casual co-op |
|---|---|---|---|---|
| Artifact level cap | Off, 5, 10, 15 | 15 | 15 | 10 |
| Artifact slots | 0 to 3 | 3 | 3 | 3 |

"Off" also closes hidden phase 3s; the world settings screen says so on the row. Fixed on every server (SPEC): weird rules apply only to the wearer.

### 2.7 Boss soul forging (DD Endgame; FRP R7; GAP-COOKED, the Elden Ring remembrance lesson)

**Souls.** Every major boss can drop its **soul** (an item: a coal-dark stone with the boss's sigil glowing inside, warm to the touch):

| Kill | Soul chance per looting player |
|---|---|
| Your first kill of that boss | 100% (with the first-kill drop) |
| Repeat kills | 10% + 1 point per party AL |
| Hidden phase 3 kills | ×2 (AL 15 phase 3: 50%) |
| First kill at AL 10+ | 100% (Geis record, 2.6.5) |

**Where you forge it: the boss's own altar** (BUILT `AWRBossAltar`, in each arena). The altar that gives a path also takes the soul. Interact → the altar screen gains a **Forge soul** page (2.16). You choose one of two; a second soul of the same boss buys the other. Further souls salvage at the altar into core shards ×5 (starsteel input, KEPT survival-crafting). No NPC: the land does it.

**Choice A: awaken the boss's weapon.** You need the boss's blueprint weapon (survival-crafting rule KEPT: every major has a boss-weapon blueprint) in your bag, any quality, any upgrade. The altar takes the weapon and the soul and gives it back **awakened**: its metal factor rises to starsteel (×2.8), it keeps its quality, sockets and upgrade level, its element is fixed to the boss's element, and it gains a **Soul art**: an ability you can put in any quick slot while the weapon is held (no new button, one verb per button kept). Soul arts cost 25 stamina and have a 30 s cooldown; they're weapon arts, not path Wildbrands, so they spend no path resource.

**Choice B: the soul talisman**, for the talisman slot (survival-crafting KEPT slots).

| Boss | Boss weapon (class) | Soul art (W / A / R s, damage ×, effect) | Soul talisman |
|---|---|---|---|
| Vulkarn | **Anvilsplitter** (axe; name GAP-COOKED here, none existed) | *Vulkarn's Flood:* overhead chop 0.75 / 0.2 / 0.6, ×2.2; a 6 m lava crack ahead for 4 s (20/s + burn 20) | **Forge-heart:** path resource gain +10% while above 70% HP |
| Skaldrith | **Rimefang** (sword; new) | *Pillar-breaker:* spinning cut 0.5 / 0.3 / 0.5, ×1.6 in 300°, frostbite 50, stance ×3 | **Rime-scale:** the first hit after 10 s unhit deals 30% less |
| Kharvox | **Talon of Kharvox** (dagger; new) | *Rod-call:* stab 0.3 / 0.15 / 0.4 marks the target; 1.0 s later lightning strikes it from above (1.0 s ground glow), ×1.8, shock 60 | **Storm-eye:** sprint speed +6%; the roll after a successful parry costs no stamina |
| Ahrumet | **Gnomon Maul** (great hammer; KEPT desert) | *Noon:* plant 0.8 / 0.3 / 0.7, a 6 m sundial ring for 6 s: allies inside crit +20%, enemies inside take blind 20/s | **Sun-dial:** +8% damage outdoors by day, −4% at night |
| Sylvaroth | **World-Root Maul** (great hammer; KEPT forest) | *Re-root:* slam 0.7 / 0.2 / 0.7, a 5 m root ring (roots 2 s; bosses slowed ×0.7 for 3 s); you heal 5% over 5 s if you stand still | **Heart-tree seed:** potions heal a further 15% over 6 s |
| Aeolith | **Galespine** (bow; new) | *Serpent's gale:* charged shot to 1.2 s, one arrow ×3.0 that pierces and pushes 600 (straight, never homes) | **Serpent's feather:** fall damage −50%; jump attacks +15% |
| Lyrathis | **Prismglass Spear** (spear; KEPT crystal) | *Prism split:* thrust 0.45 / 0.2 / 0.6 in three light-lines 15° apart, ×0.8 each, mark 30 | **Prism:** guard boost +10; a projectile blocked with a shield sends 25% of its damage straight back along its line |
| Thalassor | **Tidebreaker** (spear harpoon on a chain; KEPT ocean) | *Leviathan's hook:* throw 0.6 W (straight, 1,600 cm/s); pulls a small target to you, or you to a large one; ×1.4 | **Leviathan's breath:** breath underwater ×2; stamina regen +10% in rain or water |
| Vharra | **Three-Jaw Glaive** (class as the swamp cook set it; KEPT) | *Mire drink:* sweep 0.5 / 0.3 / 0.6 in 300°, ×1.5, bleed 50, heals you 10% of the damage dealt | **Mire-mother's tooth:** your bleed buildup +15% |
| Nhal | **Mourner's Scythe** (scythe; KEPT graveyard) | *Procession:* wide reap 0.6 / 0.3 / 0.7, ×1.7; each enemy hit gives 1 Soul (Necromancy) or 5 of your path resource (any other path) | **Mourner's candle:** light radius +40%; dark-hunters never get double aggro on you |
| Mycelorn | **Stipe Maul** (great hammer; KEPT fungal) | *Fruiting:* slam 0.8 / 0.2 / 0.8, ×2.0, plants a spore cap that bursts 1.5 s later (4 m, poison 60) | **Hive-heart:** poison and rot buildup on you −25% |
| Vorhollow | **Gnashblade** (greatsword; KEPT void) | *Swallow:* lunge 0.55 / 0.25 / 0.7 (4 m), ×2.0; a kill restores 10 max HP lost to void | **Void-heart's shell:** max HP lost to void comes back twice as fast out of combat |

Raid champions don't drop souls; they drop champion's marks (KEPT void, 2.9.1).

### 2.8 Summoning altars (DD Bosses: "offer items to fight a boss on demand"; FRP R5; GAP-COOKED)

| Part | Rule |
|---|---|
| Where | A **summoning stone** beside each major's run-back stone (40 to 60 m from the arena door, KEPT combat cook) and at the mouth of each mini-boss lair: a flat stone with the boss's sigil carved into it and a shallow bowl, ringed by that biome's waymark (a slag cairn in the Volcanic, an inuksuk on the tundra, and so on, world-map KEPT) |
| The offering | An **effigy**, crafted at the alchemy table. Its recipe is learned on your first kill of that boss. Major: the biome's signature resource ×15 + its path essence ×5 + a trophy of any of the biome's minis. Mini: signature resource ×5 + the mini's biome midland materials ×10 |
| Using it | Place the effigy in the bowl (Interact 2 s). If the boss is dead (its respawn timer running), it rises now in its arena with a sound from the arena (the boss's tell layer, heard within 300 m). If it's alive, the stone refuses: "<Boss> is already awake" |
| Whose fight | Like a key portal (SPEC rule), it belongs to the summoner: only the summoner's party (clan and band) can damage it for that fight. Others see a faint seal over its health bar. When it dies or resets, its normal respawn timer starts from that moment |
| Loot | Normal loot (10% rule) for the party, including the core. Duplicate cores feed starsteel (core shards, KEPT) |
| No tiers | One version of each boss. Harder fights come from artifacts (Raven's personal difficulty), never a separate "alpha" boss |

### 2.9 Raids (DD "raids for 6 to 10"; frame KEPT from biome-void 2.23.2; five raids GAP-COOKED)

#### 2.9.1 The frame (KEPT, with the open questions answered)

KEPT exactly from biome-void 2.23.2: a **war-hall** sealed under each pass, entered by a door in a pass fort's undercroft; one group at a time (any size, tuned for 8, never capped, DR #22 reading); the door seals when the first raider crosses the inner threshold and opens when the fight ends or the room is empty for 8 s; **two champions, each of one element, enemies of each other as well as of you** (each one's attacks hit the other at ×0.5; a champion that drives the other under 30% gains **Triumph**: +20% damage and faster); each champion 12,000 solo, +80% per extra player or tame (8 players: 79,200 each), stance 180, 3 parries, phase 2 at 50% by a transition attack, hidden phase 3 with artifacts; 15 to 20 minutes for 8; loot per champion by the 10% rule (path materials, a **Champion's mark**, upgrade shards ×3, a 3% raid cosmetic); server setting "raid respawn" 1 to 168 h, default 24 h; server death rules, bodies at the raid door; a run-back stone in the fort undercroft 40 m from the door. The Severance raid (the Cantor and the Hollowed Giantess) is KEPT in full.

| Open question | Answer |
|---|---|
| **No cross-biome endgame bosses (DD Ruled out)** | No champion mixes elements. Each is a single-element boss; the pair fight each other. Flagged to Raven by the void lane (LATER-COOKS); this cook keeps that reading |
| **The 10% rule with 10 players (LATER-COOKS)** | The rule is Raven's and fixed. Each champion keeps its own ledger, so a 10-player group has two pools to reach 10% in. Tuned for 8: four on each champion average 25% each; ten average 20%. Damage dealt by your tames and summons counts for you (BUILT for tames, `WRBoss.cpp` 677). Damage one champion deals to the other counts for nobody. The war-hall's design pushes a split (the two champions start 60 m apart at opposite ends), so the rule works without changing it |
| **Raid artifacts** | Each raid adds one artifact (2.6.2, #25 to #30), 10% per looter, Weight III to V |
| **Geis records** | Each champion has its own record steps (2.6.5) |
| **Music** | The pass duet (world-map KEPT) arranged for the hall: phase 1 alternates the two biome themes by which champion is nearer the camera; phase 2 plays both together in counterpoint; when one champion gains Triumph, its theme takes the lead. Ducks ≤4 dB under hits, never silent except a planned 1.5 s silence before each transition |

#### 2.9.2 The five other raids (GAP-COOKED; each pair's champions KEPT from world-map 2.12; gimmicks KEPT from biome-void's table, built out here)

**Raid 1: the Thaw Hall (Fire / Frost), under the north pass fort.** The south pass fort's undercroft has a matching door, bricked up from inside, with both champions' empty weapon racks before it (the story: one hall, two ways in, one walled). The hall is 110 × 70 m under a 40 m vault, its floor a checker of 4 m tiles of basalt and blue ice over a lava sump.

| Part | The Fallen Smith (Fire) | The Burnt Wyrm (Frost) |
|---|---|---|
| Look | A 9 m forge-giant skeleton (the fire-frost cook's bones, KEPT: frost-split, Rime-folk ice-spear heads in the ribs), its splits knitted with glowing molten iron seams; a hammer as long as a man; heat haze off the ribs | A 30 m wyrm skeleton (KEPT: charred black, smith-forged iron spikes in the skull), re-skinned in black glacier ice through which the char shows; frost smoke from the jaw |
| Phase 1 | Hammer combo ×3 0.65 / 0.2 / 0.5 hold 0.4 / 0.95 (80 / 80 / 120); **overhead slam** 1.1 (punish 1.2; flips every tile in 6 m to lava); bone-tongs grab 1.1 (crimson, 150); slag throw 0.9 W (arc, 1,200 cm/s, landing glow); bellows-rib cone 1.0 (8 m, burn 50) | Tail sweep 1.0 (`low`, flips tiles in its path to ice); bite lunge 0.8 (100); **frost breath** 1.2 (12 m cone, sweeps 60°, frostbite 60); wing buffet 0.9 (knockback 600); jaw grab 1.0 (crimson, shakes, 140) |
| Transition (50%) | **Rekindle** 1.4: crimson, his seams flare white and every ice tile within 12 m turns to lava | **Refreeze** 1.4: crimson, a roar of frost that turns every lava tile within 12 m to ice |
| Phase 2 adds | Delayed overhead (hold 0.6, the final cue 0.2 s before contact, combat rule); combo ×4 ending in a spin; arena change: lava rises in the sump gutters between tiles | Flight in the vault: dive 1.2 along a line (1.0 s shadow), air breath leaving a strip of ice; arena change: icicles form on the vault and fall (1.0 s crack tell) |
| Hidden phase 3 | **Quenching:** he drags himself through ice tiles to cool, each turning to water and steam (blind 20 in 3 m); dual hammers | **Whiteout:** frost fog fills the hall (vision 15 m); his breath freezes on contact (1.5 s) |
| How they meet the hall and each other | Lava tiles burn anyone standing on them (30/s + burn); ice tiles are slick (KEPT hazard). **Thaw in the hall:** a tile flipped twice within 3 s (lava→ice or ice→lava) bursts in a steam blast (the Fire+Frost Thaw reaction, KEPT combo), 150 damage to anything on or next to it, champions included. Players bait one champion's slam onto tiles the other just flipped. The Smith's slams melt the Wyrm's ice footing (it slips, stance damage ×1.5 for 4 s); the Wyrm's breath hardens the Smith's seams (his attacks slow ×0.85 for 6 s) | |
| Voice | Elephant rumble + cooling iron ticks + anvil ring | Crocodile bellow + glacier crack + wind |
| Weak to | Frost ×1.5 (opposite) | Fire ×1.5 |

**Raid 2: the Gut Hall (Lightning / Blood), under Stormgut.** A slot cavern 120 × 40 m under the gorge; three ceiling slits pour the gorge's waterfalls into bleedwater pools along the floor; eight copper rods stand in iron shoes along the walls.

| Part | The Rodwarden Roc (Lightning) | The Coven Leech-drake (Blood) |
|---|---|---|
| Look | The roc whose skeleton lies on Stormgut's swamp side (world-map KEPT), risen with feathers of dark storm cloud that crackle; the Coven's bone hooks still hang from its wings on cords; 18 m wingspan | The leech-drake whose skeleton lies burnt on the storm side (KEPT), risen from a pool: a 25 m glistening black-red body, a sucker mouth ringed with three jaws, short clawed forelimbs, the char still on its back |
| Phase 1 | Talon combo ×2 0.6 / 0.2 / 0.8; wing sweep 0.9; **chain lightning** 1.1 (a bolt jumps between any rods standing upright, 1.0 s rod glow, 60 + shock 60 to anyone within 3 m of a lit rod); dive 1.0 (12 m line); carry grab 1.1 (crimson: lifts you 8 m and drops you) | Drain bite 0.7 (latch, heals her 15/s until hit off, 300 damage frees); tail lash 0.8; **blood spray** 1.0 W (arc, landing glow, bleed 50); submerge 1.0 (into a pool, surfaces at another with a ripple tell 0.9 s); coil grab 1.0 (crimson, 150) |
| Transition | **Storm call** 1.4: crimson, every upright rod lights at once | **Blood tide** 1.4: crimson, the pools spread across half the floor |
| Phase 2 adds | Delayed talon (hold 0.6); hover and call strikes on marked circles (1.0 s ground glow); arena change: rain through the slits doubles (pools fill) | Delayed lunge (hold 0.5); a combo of bite, bite, tail; arena change: pools turn to bleedwater (10 bleed/s wading, KEPT number) |
| Hidden phase 3 | **Eye of the storm:** it never lands except to grab; rods relight every 8 s | **The water turns:** pools heal her 2%/s while she's in them |
| The hall and each other | **The rods are a machine you can work:** Interact 2 s to drive a rod's iron shoe down into the floor so it grounds: a grounded rod stops carrying the chain. **Lightning in water:** a roc strike landing in a pool shocks everything in it, the drake included (stunned 4 s, the opposites rule both ways); the drake's blood spray on a rod rusts it (it can't be grounded for 20 s) | |
| Voice | Harpy eagle scream + thunder + rope creak | Elephant seal roar + wet suction + a hiss |
| Weak to | Blood & curse ×1.5 | Lightning ×1.5 |

**Raid 3: the Salt Hall (Sun / Tide), under Saltgate.** A hall 130 × 60 m under the salt flat, its floor a dry white salt pan, its long walls holding four bronze sluice gates on the sea side. A tidal wheel in a side channel turns with the real tide outside; its shaft drives a cam through visible gearing that opens the sluices every 90 s for 30 s (flood) and closes them (drain through floor grilles over 15 s).

| Part | The Salt Serpent (Tide) | The Barnacled Colossus (Sun) |
|---|---|---|
| Look | The serpent skeleton from Saltgate's ocean side (KEPT, chalk-white), risen in salt-crusted scales, 40 m, a fin crest of bone | The colossus from the desert side (KEPT, eaten through by coral and barnacles), 11 m, sunstone eyes glowing amber, a gnomon blade in one hand |
| Phase 1 | Coil sweep 1.0 (`low`); strike 0.7 (100); **tide spit** 0.9 W (straight, 1,500 cm/s, soak 50); swallow lunge 1.1 (crimson, inside 4 s, 30/s, out with 400 damage to the throat); tail slam 1.0 | Gnomon-blade combo ×3 0.7 / 0.25 / 0.6 / 1.0; stomp 1.0 (`low` ring); **eye beam** 1.1 (a 30 m line that sweeps 45°, locks at windup end, blind 40); sand wave 1.0 (`low` line); grab 1.1 (crimson, crushes, 150) |
| Transition | **High water** 1.4: crimson, a wave across the hall | **Noon** 1.4: crimson, its eyes flare and the salt pan glares white (blind 30 to everyone facing it) |
| Phase 2 adds | In flood it swims: fast passes along the hall (1.0 s fin wake tell); delayed strike (hold 0.6) | Delayed stomp (hold 0.5); beam combo (two sweeps crossing); its barnacles shed (shrapnel ring, 0.9 s rattle tell) |
| Hidden phase 3 | **Maelstrom:** the flood spins; you're carried round the hall 600 cm/s | **Noon lock:** the beam never stops; its shadow is the only safe ground (the Ahrumet rule) |
| The hall and each other | In flood the colossus is slowed ×0.6 and the serpent fast; when drained the serpent flounders on the salt (stance ×2 taken for 6 s) and the colossus is quick. **Players can jam a sluice** (Interact 3 s on its gear rack with an iron bar from the racks) to keep that quarter of the hall dry for one cycle: the place to fight the serpent. The colossus's beam on wet salt flashes (blind 20 to anyone in the wet); the serpent's soak cracks the colossus's stone (soaked stone: Sun champion takes ×1.25 for 8 s, the opposites rule) | |
| Voice | Humpback song pitched down + surf | Stone grind + a camel's groan + wind over sand |
| Weak to | Sun & sand ×1.5 | Tide ×1.5 |

**Raid 4: the Root Crypt (Nature / Necromancy), under Rootgrave.** A crypt 120 × 70 m: rows of stone graves in the floor, the ceiling a mat of living roots that reach down as columns.

| Part | The Rotted Warden (Nature) | The Rooted Ossuary (Necromancy) |
|---|---|---|
| Look | The treant from Rootgrave's forest side (KEPT), 12 m, its roots rotted black and dripping, moss and white fungus on the bark, a face of knotholes | The bone giant from the graveyard side (KEPT), 10 m, roots grown through its ribs and skull, a lantern of grave-light in its chest, a tombstone for a shield |
| Phase 1 | Branch combo ×3 0.7 / 0.25 / 0.6 / 1.0; **root eruption** 1.1 (a line of roots bursting up, 1.0 s ground swell tell); seed bombs 1.0 W (arc, landing glow, bleed 30); vine snare 0.9 (root 1.5 s); grab 1.1 (crimson, crushes in branches) | Bone club combo 0.6 / 0.2 / 0.5 / 0.9; tombstone bash 0.8; **raise** 1.2 (opens 3 graves: 3 skeleton warriors rise, midland stats); grave-light bolt 0.9 W (straight, 1,400 cm/s, decay 40); grab 1.0 (crimson, 140) |
| Transition | **Uprooting** 1.4: crimson, it tears itself from the floor and the floor heaves | **The opening** 1.4: crimson, every grave in 15 m cracks |
| Phase 2 adds | It walks; floor sections rise as root platforms (they grow up smoothly over 3 s on visible root columns, and sink again over 3 s); delayed slam (hold 0.6) | Delayed club (hold 0.5); combo club, bash, stomp; arena change: grave mist (vision 25 m) |
| Hidden phase 3 | **Heartwood:** every root column it touches heals it 1%/s | **The procession:** raised dead come every 15 s, 4 at a time |
| The hall and each other | **Roots seal graves:** the Warden's root eruptions crossing a grave seal it for the fight (the opposites rule, KEPT forest/graveyard pattern). **Decay rots roots:** the Ossuary's grave-light bolts on a root column rot it (it can't heal the Warden). Players steer each champion's attacks across the other's ground: lure the Warden's roots over the graves, and the Ossuary's bolts into the root columns | |
| Voice | Creaking oak + moose bellow + rain on leaves | Bone rattle + a bull walrus roar + a tolling bell |
| Weak to | Necromancy ×1.5 | Nature ×1.5 |

**Raid 5: the Flue Shaft (Wind / Spore), at the floor of the Flue.** A 70 m-wide section of the Flue's throat, 60 m tall, with stone ledge rings at 0, 15, 30 and 45 m. Two warm updraft columns rise through it; spore clouds sink down its walls. **Two Kitewright cage-lifts** run on a single chain loop between the floor and the 45 m ledge, one going up while the other comes down, counterweighting each other; the loop runs over a top wheel driven by a **sail turbine** spinning in the updraft, so the cages move continuously and smoothly at 1.5 m/s and never stop or snap.

| Part | The Fallen Kitewright (Wind) | The Paper Titan (Spore) |
|---|---|---|
| Look | The winged champion that fell down the Flue (KEPT): a skeleton with a 20 m wingspan of bone and torn kite-silk, wrapped in mycelium that sheds as it flies; a rider's harness still on its back | The mushroom titan husk from the highest sky island (KEPT), 14 m, dried to paper and crackling; in the Flue's damp it swells and darkens as it soaks spores |
| Phase 1 | Talon pass 0.9; wing gust 1.0 (push 900 toward the shaft's centre, a 1.5 s whistle); **dive** 1.1 (line, 1.0 s shadow); silk-line lash 0.7; grab 1.1 (crimson, carries you up the shaft and drops you) | Cap slam 1.0 (`low` ring); gill burst 1.1 (spore cloud 6 m, poison 50); **paper shard rain** 1.0 W (shards fall with growing shadows); crushing step 0.8; grab 1.1 (crimson, engulfs in the gills, poison 40/s, 300 damage frees) |
| Transition | **Updraft** 1.4: crimson gust, everyone on a ledge is blown inward | **Fruiting** 1.4: crimson, its cap opens and the shaft fills with falling spores |
| Phase 2 adds | It hovers in an updraft and dives on cages; delayed talon (hold 0.6) | It climbs the walls (its stalk legs gripping ledges) and drops; delayed slam (hold 0.5) |
| Hidden phase 3 | **The fall:** it flies only; the cages are the only footing it can't reach | **The whole shaft:** spores fill the shaft from the top down, 1 ledge every 30 s |
| The shaft and each other | **Updrafts carry players up** (step in: rise to the next ledge in 3 s) and carry spores up with them. **Wind blows spores back:** the Kitewright's gusts push the Titan's clouds onto the Titan (it soaks and swells, ×1.25 damage taken, slower). **Spores weigh wings:** a Kitewright that flies through a spore cloud is grounded on the nearest ledge for 5 s (the opposites rule both ways, KEPT Aeolith pattern). The cages are footing for fighting the Kitewright high; anyone riding a cage can strike it as it dives | |
| Voice | Lammergeier whistle + silk tearing + wind in a chimney | Paper crackle + wet squelch + a cave drip echo |
| Weak to | Spore ×1.5 | Wind ×1.5 |

**Raid 6: the Severance (Void / Arcane): the Broken Choir.** KEPT in full from biome-void 2.23.2.

### 2.10 Mythic gear (DD "mythic gear that needs materials from every biome"; KEPT biome-void 2.23.3; one addition GAP-COOKED)

KEPT exactly: forged at **the Ring on the Crown** with no NPC and no menu station; offer 10 of each of the twelve signature resources on the twelve pillars, place 3 Champion's marks and a +10 masterwork starsteel weapon or ring on the starburst's centre stone, channel 3 minutes (every pillar lights its beacon, taking damage breaks the channel, a PvP hotspot by design); the result is mythic: ×1.15 over starsteel, 3 fixed sockets, upgrade cap +10, **two elements at once** (main and subclass, 50/50), and its own mythic Wildbrand. The ten mythics (Reachblade, Spinebreaker, Giantsfell, Crownspear, Wakebreaker, Tidewhisper, Harrowmoon, Delvewright, Farsight, Twelvefold band) and their Wildbrands are KEPT.

**Added: mythic armour (GAP-COOKED).** DD says "mythic gear", not only weapons. Any biome armour set piece at +10 can be made mythic at the Ring the same way, at 1 Champion's mark and 5 of each signature resource per piece. A mythic piece keeps its set and also counts as a piece of **one other biome set of your choice** (picked at the forging, shown on the card). Four mythic pieces can give you one set's 4-piece bonus and another's 2-piece bonus: the only way to blend sets. Its armour is ×1.15 over starsteel's factor. Look: the piece gains a thin seam of all twelve colours along its edges (1 nit), the Ring's starburst in miniature.

### 2.11 Prestige (DD "reset a path for permanent perks"; KEPT biome-void 2.23.4; the open question answered)

KEPT exactly: at a path's altar when the path is at 100 points; a held **Return** button (1.5 s); the path resets to 0, its XP curve restarts, freed points go back under the 150 cap, and its node layout is saved for one-click re-buying; one **Prestige mark** per return, max 3 per path; each mark picks one of three perks (that path's damage +5%; its resource capacity and gain +10%; its root passive works on the subclass slot at 50%, once per path); marks work while that path is equipped; gold threads on the path's body marks; at 3 marks, the title "<Path>-sworn".

**The open question (LATER-COOKS): "only while equipped" against DD's "permanent perks".** Answer: the marks and perks are **permanent** (never lost, never decay, survive every swap), and every perk is *about that path*, so it naturally applies when that path is in a slot. That's the reading of "permanent" that keeps the 150 cap meaningful: a character can carry at most 6 active marks (main and subclass), but owns up to 36 over a lifetime. This cook keeps it.

**Why prestige has a long tail (lesson 1).** A second climb to 100 costs the same 60,500 XP (about 80 h, less with artifacts' XP and Rested). Twelve paths × 3 marks = 36 marks: hundreds of hours of horizontal growth after level 50.

### 2.12 Records, achievements, titles, cosmetics, monuments (DD Endgame; KEPT biome-void 2.23.5 to 2.23.8; additions GAP-COOKED)

**KEPT exactly:** boss kill-time leaderboards for every major, mini and raid champion (first hit to death; bands solo, 2, 3 to 4, 5 to 7, 8 to 10, 11+, tames count; artifact runs in their own column with the AL; a stone tablet by each run-back stone with the top 3 and the full board on interact; a Journal Records tab; per server, never resets, a "records season" setting); 150 achievements in 10 categories with `Data/achievements.json` and 20 hidden; titles on the nameplate and in chat; cosmetics only from hard content, no store (weapon glows, armour dyes, claim banners, raid pieces, prestige threads); monuments (a major's trophy head on a plinth, comfort 3, a bronze plaque with names and kill time, the boss sigil on the claim's map icon).

**Added (GAP-COOKED):**

**The 150 achievements by category** (so the data file has a fixed shape):

| Category | Count | Rule that generates them |
|---|---|---|
| Explorer | 30 | 12 "every landmark in <biome>", 12 "enter <hidden sub-biome>", 6 "walk all of <pass>" |
| Hunter | 24 | 12 "kill every creature of <biome>", 12 "tame one of every tameable creature of <biome>" |
| Slayer | 30 | 12 "kill <major> solo", 12 "kill <major> with nobody in the group dying", 6 "kill every mini of a belt" |
| Path | 14 | 12 "<path> to 100", 1 "own 10 combo pairs", 1 "own all 66 combo pairs" (saved progress counts) |
| Life | 15 | One per life skill to 100 |
| Builder | 10 | First claim; Hearth 5; a full biome building set placed; 3 monuments; 12 monuments; comfort 20 in one room; a base with all four stations at Hearth 5; 50 tames housed; a working production chain of 3 machines; a turret line of 6 |
| Survivor | 6 | 100 game days alive (one life); unmade by rift drain and lived to rest; survived a creature raid with no wall lost; survived a portal break by killing its escaped boss; cleared a rank 5 portal with 3 modifiers; reached AL 15 |
| Raider | 8 | One per raid (6); "Triumph prevented" in any raid; a raid at AL 10+ |
| Mythic | 6 | First mythic weapon; first mythic armour piece; 4 mythic pieces; every mythic weapon class; a mythic Wildbrand kill on a raid champion; a mythic forged while another clan contested the Ring |
| Prestige | 7 | One per first prestige (any path) ×1, 3 marks on one path, 12 paths prestiged once, 36 marks, plus Geis records: first AL 5, 10 and 15 kills on any boss |
| **Total** | **150** | |

**Record tablets** stand beside every run-back stone (majors and minis), at each portal biome's Crown pillar (deepest rank and fastest rank-5 clear in that biome, by band) and at each raid door. Stone tablets with names cut in as the board changes (the letters shift with a soft grinding sound when a record falls, heard within 10 m).

**Monuments get one more line** (KEPT plinth): the plaque shows the highest AL the boss was killed at by the claim's owners, and the geis-knot border at AL 15 (2.6.5).

### 2.13 Repeatable goals (GAP-COOKED; the "no weekly challenges" rule kept)

#### 2.13.1 The rule

Raven rules out weekly challenges and world-wide escalation (DD). So nothing in Wildreach resets on a calendar, and there's no quest board of chores. Everything that repeats is **the world living on its own timers**: bosses come back, portals open and break, events roll in, invasions march, creatures breed. A player always has something to go and do because the world changed since they last looked, not because a list told them.

#### 2.13.2 What comes back, and how often (all server settings where marked)

| Thing | How often | What it's worth at hour 100+ | Source |
|---|---|---|---|
| Major bosses | Respawn 4 h default (1 to 24 h setting) | Parts (Scarred at AL 5+), souls, artifacts, Geis records, cores for starsteel | SPEC (BUILT) |
| Mini-bosses | Same timer | Upgrade shards (79 for one +10 item), trophies, effigy inputs | SPEC (BUILT) |
| Summoned bosses | On demand, one effigy each | A fight now, without waiting | 2.8 |
| Wild portals | 41 slots; each refills 2 to 6 h after use | Shards for keys, blueprints, artifacts, legendary rolls | KEPT desert |
| Portal breaks and escaped bosses | When a portal is ignored (1 to 18 days) | An escaped boss blocks its rank in that biome: a reason to go clear it | SPEC |
| Key portals | On demand | A chosen rank in a chosen biome | SPEC |
| Raids | 24 h default (1 to 168 h setting) | Champion's marks (3 per mythic piece), raid artifacts, cosmetics | KEPT void |
| Biome events | About every 3 real days per biome, one game day long | Signature resource at triple rate (mythic needs 10 to 60 of each) | FULL-GAME 3.3 |
| World events (Starfall) | Every 2 real days at night | Star iron for starsteel | World-map KEPT |
| Invasions | Off / Rare (1 a week per pair) / Normal (2) / Frequent (4) | Both biomes' path materials, 2,000 path XP | FULL-GAME 3.3 |
| Creature raids on bases | Off / rare / normal / frequent | Defence; turret and tame tests | SPEC (BUILT) |
| Legendary fish | Per biome, conditions-based | Elixir inputs, trophies | KEPT survival-crafting |
| **Scarred beasts** | 2% of highland spawns; at most 1 per biome alive at once | Scarred pelts and trophies, a strong tame line | 2.13.3 |

#### 2.13.3 Scarred beasts (GAP-COOKED; the Monster Hunter tempered lesson)

| Part | Rule |
|---|---|
| What | A highland creature that has survived too long: old wounds healed over in raised scars, one eye milky, a torn ear or broken horn, the muscle heavier. Its silhouette is its species' but 1.25× and heavier in the shoulders |
| Spawn | 2% of highland spawns in a biome, at most one alive per biome. Never inside 300 m of a claim or camp |
| Strength | HP ×2.5, damage ×1.25, stance ×1.5. It uses its species' attacks **plus the two moves of that species' portal Alpha or Elder** (2.5.6), so the animation set is shared |
| Tells | Its tells read as the species' own, a fraction heavier; it never gets a faster or hidden tell |
| Drops | A **Scarred pelt** or plate (×3 of the species' hide or shell, plus a dye: "scar-white"), a **Scarred trophy** (decor, comfort 2), and its species' normal loot ×2 |
| Taming | By its species' listed method only (DR #16 KEPT), taking ×2 as long. A tamed Scarred beast has +10% HP and damage; breeding passes the Scarred line to half its young (the trait shows as a scar pattern) |
| On the map | Nothing. Players find them by sight and by the Tracking skill (BUILT life skill): at Tracking 50+, a Scarred beast's tracks show darker |
| Voice | Its species' voice pitched 10% lower with a rasp layer (an old animal's breathing) |

### 2.14 What a player does at hour 100 and later (GAP-COOKED, built from everything above)

**Hour 100 (level 45, Hearth 5 at six majors, starsteel +8 weapon, AL 2 to 4).** A typical 3-hour evening with two clanmates:

1. Log in at the claim. The crafting queue finished overnight (BUILT). Check the Records tab: a rival clan took the fastest Ahrumet time in the 3-to-4 band.
2. A Rare portal opened in the Swamp (its blue pillar is visible from the claim tower). Run it (25 min): two modifiers, Black Annis's room kit, a found rune.
3. The Stormgut biome event (the Great Storm) is on today. Ride north and gather stormglass at triple rate for the mythic pillar offering.
4. Summon Kharvox with an effigy at AL 4, hidden phase 3 for the first time. Die twice to the Eye of the Storm, beat it on the third: a new Geis record (AL 4), an artifact at Weight II.
5. Fuse two Copper Tongues into Weight III at the alchemy table. AL goes to 5. Next time a boss part drops, it may be Scarred.

**Hour 300 (level 50, all twelve majors, two mythic weapons, AL 10 to 15, Fire main at 2 prestige marks, Frost sub, Lightning and Void paths saved at 60 and 40).**

1. Swap the subclass to Lightning at Kharvox's altar (its saved 60 points come back) to finish its 25/25 combo with Fire, one of the 66.
2. Raid night: the Gut Hall with eight. AL 10 on the drake side (the clan leader's AL counts for the clan). Triumph prevented: an achievement.
3. Soul forging at Vharra's altar with a second soul: the talisman this time.
4. The Ring on the Crown: forge mythic armour (a Rimeguard chest that also counts as Stormweave). A rival clan sees the twelve beacons light and comes for the channel: PvP on the Ring.
5. Hunt a Scarred Thunderhorn seen on the ridgeline; tame it for the breeding line.

**The long tail, counted:**

| Goal | Size |
|---|---|
| Paths mastered through altar swaps | 12 × 60,500 XP (about 80 h each) |
| Combo pairs owned | 66 pairs × 2 combos |
| Prestige marks | 36 |
| Geis records | 360 steps |
| Mythics | 10 weapons and rings + any armour pieces |
| Boss souls | 24 forgings |
| Monuments | 12 |
| Records | Every boss in 6 bands, plus AL columns, plus 12 portal biomes, plus raids |
| Scarred tames and breeding lines | Open-ended |
| PvP: clans, bounties, raiding, contested bosses and the Ring | Open-ended (multiplayer lane) |

### 2.15 Dream-tier endgame (BP Dream list; designed so they're ready, built after the full-release gate)

#### 2.15.1 Legendary weapons with unique movesets (DREAM)

The ten mythic weapons (2.10) are the vessels. When this tier is built, each mythic weapon gains a **unique moveset** in place of its class's (new light chain, heavy, running and jumping attacks, all on the same buttons: one verb per button kept), unlocked by forging the mythic a second time at the Ring with 3 more Champion's marks. Until then mythics use their class moveset plus their mythic Wildbrand (KEPT). Animation: each moveset is a full set of real mocap clips per class, retargeted to the MetaHuman rig (hero cook KEPT), never procedural.

#### 2.15.2 Secret super-bosses (DREAM)

Twelve **Elders**: the first creature each biome's beasts came from, sleeping in the deepest room of each hidden sub-biome, found only by following the place (a sound, a trail of shed scales, a door that opens at one hour). Single element each (DD rules out cross-biome endgame bosses). 30,000 HP solo, +80% per player or tame, 3 phases always (they don't need artifacts), stance 220. They drop no new power: a unique monument, a title, and a cosmetic. Seeds (each biome lane finishes the fight):

| Biome | Elder | Where it sleeps |
|---|---|---|
| Volcanic | The First Magmaw, as long as a lava river | The deepest lava tube |
| Storm peaks | The Roc-mother | Inside the Hollow Spire's top chamber |
| Desert | The Dune-father, a sand-swimmer that never surfaces fully | The buried temple city's lowest vault |
| Ancient forest | The Hollow Root's heart-bear | The Hollow Root's last chamber |
| Sky islands | The Undercloud whale | Inside the Undercloud bank |
| Crystal caves | The Geode Mother | The deepest geode hall |
| Frozen tundra | The First Wyrm's egg, hatched | Under the glacier bowl |
| Swamp | The Leech-mother of all leeches | The flooded sinkhole's floor |
| Ocean & reefs | The reef-making Leviathan | The deep trench past the reef shelf |
| Graveyard | The First Mourner's hound | The catacombs' sealed ossuary |
| Fungal caverns | The Mycelium itself, a whole cavern alive | The caverns' lowest chamber |
| Corrupted wasteland | The thing the Unmade Place was cut from | Inside the Unmade Place |

#### 2.15.3 Hybrid paths and the god path (DREAM), and DR #18

**DR #18 asks:** "Secret hybrid paths unlock when two paths reach mastery, but the 150 cap means two paths can't both be 100 at once. Does mastery count saved progress across swaps?" **Answer: yes.** A path is **mastered** once it has ever reached 100 points (saved per path, BUILT). That's the only reading that works under the fixed cap, and it rewards the altar-swap long tail.

| Part | Rule |
|---|---|
| Hybrid path | Unlocks for a pair whose both paths are mastered. A 30-point **hybrid branch** that can be bought only while both paths are equipped (main and sub), paid from the same 150 points (cap kept). Its capstone is the pair's combo turned into an active ability (DR #5 allows active combos) |
| God path | Unlocks when six paths are mastered. A 10-point **Twelvefold trunk** that can be equipped as a third, passive-only line on top of the two paths: +2% damage per mastered path (max +24%) and one root passive from any mastered path at 50%. Paid from the 150 points (cap kept) |
| Why Dream | Each hybrid is a balancing job for 66 pairs (BP). Built only with Raven |

### 2.16 Screens (mouse and pad on every one, F1 page, no dead states; ui-controls rules KEPT)

| Screen | Contents | Pad / mouse | Never a dead state |
|---|---|---|---|
| **Artifacts** (a page in the Journal's Inventory tab) | The three hooks on a drawn belt cord; each artifact's name, Weight, geis line; the AL number large; your shared penalty and rewards; your party's highest AL and whose it is | Stick or D-pad between hooks and the bag list, A / click to hook or unhook (2 s tie animation), Y / right-click to inspect; B / Esc back; F1 controls | Locked in combat, in portals and raids, and near engaged bosses: the hooks grey out with the reason ("Locked: inside a portal"). With no artifacts: "No artifacts yet. They drop from major bosses and portals rank 3+." |
| **Altar: Forge soul** | The soul, the two choices as cards (the awakened weapon with its Soul art; the talisman), "Owned" on a choice already taken | Stick or D-pad between cards, A hold 1 s / click-hold to forge; B / Esc back | No soul: the page says "Bring <Boss>'s soul"; choice A without the weapon: "Bring the <weapon> (its blueprint drops from <Boss>)" |
| **Altar: Return (prestige)** | KEPT void | KEPT | KEPT |
| **Summoning stone** | A world prompt: "Offer effigy" with the boss's name | Interact hold 2 s | No effigy: "Needs a <Boss> effigy (alchemy table)". Boss awake: "<Boss> is already awake" |
| **Portal prompt** | KEPT desert, plus "All folds are full: wait <time>" | KEPT | KEPT |
| **Raid door** | KEPT void | KEPT | KEPT |
| **Band invite** | Hold Interact on a player → "Invite to band"; their screen shows "<name> invites you to a band" with Accept / Decline | A / click accept, B / Esc decline; times out in 20 s with "Invite expired" | Already in a band: "Leave your band first" |
| **Journal → Records** | KEPT void (leaderboards, achievements, titles), plus a **Geis** sub-tab: every major and champion with your highest AL kill and the next step's reward, and an **Artifacts seen** list | LB / RB or Q / E between sub-tabs; lists by stick or wheel | Empty boards: "No kills yet" (KEPT) |
| **Main menu → Hall of Deeds** | Titles and achievements from every world on this PC | Stick / wheel; B / Esc back | Empty: "No deeds yet" |

### 2.17 How the endgame meets everything it touches (design with intention, Aaron 11-14)

| Thing | Why it's here (its job in the grand puzzle) | What it connects to | How it meets what it touches |
|---|---|---|---|
| **Portals** | Turn every biome into a place that keeps changing after its boss is dead; feed keys, blueprints and artifacts | Each biome's old people (their sealed rooms), the Crown pillars (legendary flicker), the escaped-boss block on that rank | The rift's edge is the biome's signature material; it stands on open ground ≥150 m from claims; its light pillar reads at night; its break spills that biome's creatures into their own land |
| **Artifacts** | Raven's personal difficulty; the endgame threat that never touches strangers | Bosses (phase 3, drops), portals (drops), raids (drops), the alchemy table (fusing) | Worn on the belt cord, visible to others; dim in PvP; locked inside instances |
| **Boss souls and altars** | Make every major worth killing again and give each boss a lasting piece of you | The altar that gave the path; the boss blueprint weapon; core shards for starsteel | The altar already stands in the arena; forging uses the same altar screen; no new building, no NPC |
| **Summoning stones** | Let players fight a boss now, without waiting for a timer someone else reset | The run-back stone beside it; the biome's minis (their trophies are in the effigy) | A flat stone ringed by the biome's waymark, beside the path to the arena door |
| **War-halls** | The group content Raven asked for, inside his single-element rule | The pass forts above, the champion skeletons in the pass (the tell), the Ring (marks for mythics) | Their machines are real: rods drive into iron shoes, the sluices run off a tidal wheel through gears, the Flue's cages hang on one chain loop over a turbine wheel in the updraft, root platforms rise on root columns. Nothing floats and nothing snaps |
| **The Ring on the Crown** | The one place every biome's materials meet; the endgame's visible heart | All twelve pillars (offerings), the raids (marks), the beacons (everyone sees forging) | The forging is a world act: offerings sit on stones, the channel can be broken, the pillars light |
| **Record tablets** | Tell the server's story in stone, with no announcements (DD) | The leaderboard data, monuments | Stand beside the run-back stones; letters shift with a grinding sound when a record falls |
| **Scarred beasts** | Keep the overworld hunt alive at hour 300 | The portal Alpha moves (shared clips), breeding, Tracking | They spawn like any highland creature and fight their own kind's way |

---

## 3. Plan: building it in Raven's repo

**Raven's patterns, always:** data first in `Data/*.json` with first-draft numbers; abilities as `UWRGameplayAbility` subclasses registered in `UWRAbilityRegistry::ClassForNode` (`WRAbilities.h` 213); **all damage through `UWRCombatLibrary::ApplyHit`** (`WRCombatLibrary.h` 22); server authority; runtime-spawned dressing from data so `L_Island` is touched as little as possible; tuning read through `WRTune`. Gates: compile, cook, package (Aaron's rule). No tests, no check scripts.

**What this scope builds on (must exist first):** combat C9 attack-row runner and C10 visible-contact boss damage (combat cook); the desert cook's portal system (`WRPortal`, `WRPortalRoom`, `UWRPortalSubsystem`, level-instance bays); the void cook's raid frame, Ring forging, prestige and records plan (biome-void 3.2); survival-crafting's slots (talisman, rings), blueprints and core shards; world-map's passes, forts and Crown pillars.

### 3.1 Data rows

| File | New or changed | Fields |
|---|---|---|
| **`Data/artifacts.json`** | New | `tuning {perAL {damageTaken 0.07, loot 0.05, partChance 0.01, qualityStep 0.02, pathXp 0.04, artifactChance 0.01}, thresholds {scarredPart 5, highWeights 10, glow 12, top 15}, scarredPartShare 0.25, scarredPartScale 1.5, pvpPauseSeconds 30, tieSeconds 2, lockRadius 9000, firstArtifactAtMajors 4, bossDrop 0.08, phase3DropMult 1.5, raidDrop 0.10, weightBands {"0-4": [1,2,3], "5-9": [2,3,4], "10-15": [3,4,5], split [0.6,0.3,0.1]}, portalWeights {3: [[1,0.7],[2,0.3]], 4: [[2,0.7],[3,0.3]], 5: [[3,0.6],[4,0.3],[5,0.1]]}, fuseSignaturePerWeight 5, salvageEssencePerWeight 2}`; `artifacts[30] {id, name, source {kind: boss|portal|raid, id}, geis {rule, params}, text, look {mesh, icon, metalByWeight}}` with the 2.6.2 rows. Rule ids: `no_roll, kill_burst, own_element, potion_half, next_phase_attacks, light_half, strike_seek, hide_enemy_bars, potion_still, knockback_x2, block_cost_x2, regen_moving_only, regen_scale, low_stamina_freeze, parry_window_scale, heal_cap, kill_window_double, no_block, fall_harsh, spell_noise, tames_idle, no_runback, hide_own_hud, hit_max_hp, no_turn_back, ability_hp_cost, sunwise_cost, no_ally_support, always_aggro, forced_drink` |
| `Data/portals.json` (desert's file) | Changed | `slots {volcanic 4, storm 4, desert 5, forest 5, sky 2, crystal 2, frost 4, swamp 3, ocean 2, graveyard 3, fungal 3, void 4}`, `mixBySlots {2: ["common","high"], 3: ["common","rare","high"], 4: ["common","uncommon","rare","high"], 5: ["common","common","uncommon","rare","high"]}`, `creatureRank {1: {tier: lowland}, 2: {tier: midland}, 3: {tier: midland, hp 1.25}, 4: {tier: highland}, 5: {tier: highland, hp 1.6, damage 1.3, stance 1.3}}`, `rewards` (the 2.5.4 table), `bays 8` |
| **`Data/portal_rooms/<biome>.json`** ×12 | New (desert's schema) | Rooms (32 each), graphs (4 per rank), traps and puzzles with tell, effect and answer params from 2.5.5, portal creatures (2 each), bosses (5 each, 2.5.6) with `attacks[]` in the combat cook's attack-row schema and `mechanic {id, params}` |
| `Data/creatures.json` | Changed | 22 portal-only creatures; 55 new portal bosses (Desert's 5 are the desert cook's); 10 raid champions (`tier: "champion"`, `rival`, `hall`); `scarredMoves` on each highland species (pointing at its portal Alpha/Elder's two added attacks); `part` id on every boss row (replaces the hard-coded part names in `WRBoss.cpp` OnDeath) |
| **`Data/souls.json`** | New | `drop {first 1.0, repeat 0.10, perAL 0.01, phase3Mult 2, geis10 1.0, salvageCoreShards 5}`, `bosses {<id>: {soul, weapon, weaponClass, art {id, windup, active, recovery, damage, effect{...}, stamina 25, cooldown 30}, talisman {id, effect{...}}}}` ×12 |
| `Data/raids.json` | New (void's frame, extended) | `halls[6] {pass, doorFort, size, champions[2], machines{...}}`, `crossDamage 0.5, triumphAt 0.3, triumphDamage 0.2, triumphSpeed 1.15`, machines: `flipTiles {size 400, lavaDps 30, thawWindow 3, thawDamage 150}`, `groundRods {count 8, driveSeconds 2, rustSeconds 20}`, `sluices {cycle 90, flood 30, drain 15, jamSeconds 3}`, `rootPlatforms {rise 3, hold 10, sink 3}`, `chainLift {speed 150, cages 2, top 4500}` |
| `Data/recipes.json` | Changed | Effigies (one per major and per mini; recipe learned on first kill: `unlock: "kill:<id>"`); artifact fusing (a rule row, `fuse: true`); soul weapon blueprints for the four new names (`anvilsplitter, rimefang, talon_of_kharvox, galespine`, Forge 4, steel tier) |
| `Data/items.json` | Changed | 30 artifact items (`category: "artifact"`, weight as an instance stat like quality); 12 souls; 12 soul talismans; effigies; awakened weapons as an instance flag `awakened: true`; Scarred pelts per highland species, Scarred trophies; 55 portal trophies; band-free, no currency items |
| **`Data/achievements.json`** | New (void's schema) | The 150 rows from 2.12's generator table |
| `Data/titles.json` | New | Titles from achievements, prestige, raids, Geis AL 15 ("<Boss>'s Bane") |
| `Data/tuning.json` | Changed | `upgrades {shardCost [1,2,3,4,6,8,10,12,15,18]}` (replacing the current curve's numbers); `paths.xpBoostCap 1.0`; `scarred {chance 0.02, perBiomeMax 1, minClaimDistance 30000, hp 2.5, damage 1.25, stance 1.5, scale 1.25, tameTimeMult 2, tameStatBonus 0.1, breedShare 0.5}`; `band {offlineMinutes 10, inviteSeconds 20}`; `summon {placeSeconds 2, awakeHeardRange 30000}`; `bosses.<portal boss and champion ids>` per-attack rows and mechanic params |
| `Data/server_presets.json` | Changed | `artifactLevelCap` (0/5/10/15; presets 15/15/10), `artifactSlots` (0 to 3; 3), `raidRespawnHours` (24), `portalBreak` and `portalBays` (desert), `recordsSeason` (void) |
| World save | New | `records.json` per world: boss kill counts, boards, first-kill names, kindled pillars, escaped portal bosses |
| Player save | Changed | `players/<name>.json` gains `artifacts {hooks[3], bag[]}`, `geis {<bossId>: highestAL}`, `souls {owned, forged {<bossId>: [A?, B?]}}`, `effigyRecipes[]`, `achievements[]`, `title`, `band` (not saved: bands end on logout) |
| Client profile | New | `Saved/Profile/deeds.json` (Hall of Deeds) |

### 3.2 C++ (classes and functions; Raven's patterns)

| Where | Add or change |
|---|---|
| **New `WRArtifacts.h/.cpp`** | `UWRArtifactComponent` on `AWRPlayerState` (beside `Paths`, `Inventory`): `Hooks[3]` (item id + weight), replicated; `GetLevel()`; `HasGeis(FName Rule)`; `GeisParam(FName Rule, FName Key)`; `IsPaused()` (`PauseUntil`); `Hook(Slot, ItemIndex)` / `Unhook(Slot)` server RPCs that refuse in combat, inside instances or within `lockRadius` of an engaged boss; on change adds or removes `Artifact.Geis.<Rule>` gameplay tags on the ASC. `FWRArtifacts::PartyLevel(PS)` (highest AL over `FWRSocial::PartyOf`). Keeps `bArtifactModifiers` as a derived value (`GetLevel() > 0`) so the dev command `WRArtifacts 1` still works |
| `WRSocial.cpp/.h` | **Bands:** `FWRBand {Id, Members}` held by the game mode; `AWRPlayerState::BandId` replicated; `InviteToBand`, `AcceptBand`, `LeaveBand`; `FWRSocial::PartyOf(PS, Out)` = PS + clan members + band members. Offline 10 min removes a member |
| `WRCombatLibrary.cpp` `ApplyHit` | When the target is a player and the instigator isn't player-controlled: damage × (1 + 0.07 × party AL) unless the target's artifacts are paused. When both sides are players: set `PauseUntil = Now + 30` on both. Parry window × `parry_window_scale`; block stamina × `block_cost_x2`; `no_block` refuses the guard; `low_stamina_freeze` applies Frozen 0.8 s; `kill_window_double` doubles; `hit_max_hp` lowers max HP by 1 (Void's built max-HP-loss path) |
| `WRAbilities.cpp` | Dodge roll `CanActivate` refuses with `no_roll`; roll and backstep refuse a direction away from the lock target with `no_turn_back`; Wildbrand and spell costs add 5 HP with `ability_hp_cost` (through `ApplyHit` as self-damage so it's one path); `own_element` deals 8% back through `ApplyHit`; spell cast emits a noise event (40 m) with `spell_noise`. **Soul arts:** 12 new `UWRGameplayAbility` subclasses (`UWRGA_SoulArt_<Boss>`), registered in `ClassForNode` under the pseudo-path `soul` so the quick-slot system can hold them; granted while the awakened weapon is held |
| `WRInventory.cpp` | Potion drink: `potion_half` halves heal; `potion_still` roots and lengthens to 1.5 s; `forced_drink` triggers once per fight under 50%; `heal_cap` clamps healing at 70%. Artifacts as items with a `weight` instance stat; souls, effigies, talismans |
| `WRAttributeSet.cpp` | Stamina regen × `regen_scale`; `regen_moving_only` gates regen on velocity > 50 cm/s; `sunwise_cost` drains 6/s while strafing clockwise round the lock target |
| `WRCharacterMovementComponent.cpp` / `WRCharacter.cpp` | `fall_harsh` fall-damage curve (3 m start, 10 m lethal); `knockback_x2`; heavy-hit knockdown 1.2 s |
| `WRCreature.cpp` / `WRWildlife.cpp` | `always_aggro` (sight aggro always, no sneak); `kill_burst` on deaths killed by the wearer; `tames_idle` sends tames to heel and, in boss fights, to the arena door; **Scarred:** a 2% roll at highland spawn (one per biome alive), multipliers and scale from `tuning.scarred`, `scarredMoves` appended to the species' attack rows, drops, ×2 tame time, `bScarred` saved on tames and passed to half their young |
| `WRBoss.cpp/.h` | `AnyArtifactModifiers()` → `TopPartyArtifactLevel() >= 1` (the party with the largest ledger share). `OnDeath`: per looter, loot quantities × (1 + 0.05 × party AL), part chance + 0.01 × AL, Scarred part at AL 5+, soul roll (`souls.json`), artifact roll (`artifacts.json`), Geis record update (rewards from 2.6.5), first-artifact guarantee at the 4th distinct major. Part id from `creatures.json` `part` (replacing the `vulkarn ? giants_ember : wyrm_scale` line). `next_phase_attacks`: when the chosen target has the tag, pick from the next phase's attack list. `no_runback`: a player with the tag who dies is added to `LockedOut` for the fight. `always_aggro` doubles chase radius for that target. **Summoned flag** (`SummonParty`): damage from non-party players is refused in `ModifyIncomingDamage` |
| `WRAltar.cpp/.h` | Altar screen pages: **Forge soul** (2.16) and **Return** (void's prestige). `ServerForgeSoul(Choice)` consumes the soul (and the weapon for choice A) and gives the awakened weapon (metal factor 2.8, element locked, `awakened`) or the talisman; salvage extra souls into core shards |
| **New `WRSummoning.cpp/.h`** | `AWRSummoningStone` (placed from `world_pois.json` beside each run-back stone and mini lair): Interact 2 s with an effigy; if the linked boss is dead, `AWRBoss::Respawn(SummonParty)`; else refuse with the line |
| Portals (desert's `WRPortal`, `WRPortalRoom`) | Slots from the new `slots` table; creature rank multipliers; rewards table; personal chests (`AWRPortalChest` spawns per-player loot); the "All folds are full" line. Trap and puzzle actors per biome kit: `AWRPulseGrate`, `AWRSlagChute`, `AWRQuenchTrough`, `AWRBellowsPuzzle`, `AWRIcicleFall`, `AWRThinIce`, `AWRBrazierOrder`, `AWRAuroraLock`, `AWRChargedPlate`, `AWRArcGate`, `AWRRodGrid`, `AWRThunderBells`, `AWRFloodRoom`, `AWRSwingHook`, `AWRTitheBasins`, `AWRHexMirror`, `AWRBriarWall`, `AWRDeadfall`, `AWRRootBridge`, `AWROghamStones`, `AWRSnuffCenser`, `AWROssuarySlide`, `AWRBoneCoffin`, `AWRBellVerse`, `AWRGustVent`, `AWRKiteLine`, `AWRSailWinch`, `AWRBalanceBeam`, `AWRWardField` (shared with the crystal biome), `AWRResonanceSpikes`, `AWRChimeTuning`, `AWRPrismPath` (shares the desert mirror relay's beam code), `AWRTideSurge`, `AWRClamDoor`, `AWRShellChimes`, `AWRTideValves`, `AWRPuffballMine`, `AWRMycelialNet`, `AWRFairyRings`, `AWRSporeLamp`, `AWRGravityFlip`, `AWRNullZone`, `AWRCairnStack`, `AWREchoPillars`. Each has a tell timeline (sound + visual ≥0.9 s), damage through `ApplyHit`, a disarm or counter-use, and hurts creatures too |
| **New `WRBossMechanics.h/.cpp`** | `UWRBossMechanics::ForId(FName)` registry (the `ClassForNode` pattern) of small mechanic handlers that portal bosses name in data: `quench_tanks`, `crucible_tip`, `bellows_vents`, `dawn_window`, `brazier_light`, `bone_revive`, `drum_ring`, `oak_heal`, `hut_kneel`, `cloak_turn`, `beheading_game`, `cart_harness`, `pwnco_bells`, `winch_jam`, `krotala`, `wing_harpoons`, `future_mirror`, `stone_jam`, `dry_hide`, `bottomless_barrel`, `fairy_ring`, `stalk_cut`, `reverse_time`, `mouth_parts`, plus the Alpha/Elder ones (`douse`, `clutches`, `drain_pool`, `salt_stun`, `walkway_cut`, `light_ring`, `bell_reveal`). Props use `AWRBossProp` kinds (BUILT class, `WRBoss.h` 57) |
| **New `WRRaid.cpp/.h`** (void's frame) | `AWRRaidDoor` (seal and open rules, sealed message), `AWRWarHall` (owns both champions, cross-damage ×0.5, Triumph at 30%, empty-room reset 8 s), `AWRChampion : AWRBoss` with `Rival`. Hall machines: `AWRFlipTile` grid (lava/ice state, Thaw burst through `ApplyHit`), `AWRGroundRod` (drive, rust), `AWRTidalWheel` + `AWRSluiceGate` (cam timing, jam), `AWRRootPlatform` (rise and sink on a timeline; the root columns are skeletal meshes that animate with it), `AWRChainLift` (two cages on one closed spline loop, constant speed, driven by an `AWRSailTurbine`; clients interpolate from a replicated loop phase so motion is smooth, never stepped) |
| Ring forging (void's `AWRRingForge`) | Accepts biome armour pieces at +10; asks for the second set on the card; mythic armour flag and a `secondSet` field |
| Records (void's plan) | `FWRRecords` reading and writing `records.json`; `AWRRecordTablet` (letters update with a grind sound); kindled pillars set on a biome's first major kill (`AWRCrownPillar::SetKindled`, world-map's pillar actor) |
| **New `WRAchievements.cpp/.h`** | `FWRAchievements::Notify(PS, Event, Params)` called from kills, tames, skills, building, raids, forging, prestige and records; unlocks from `achievements.json`; the 3 s toast; `ClientDeed` RPC writes the client's `deeds.json` |
| UI | `WRUIInventory.cpp`: the Artifacts page. `WRUIJournal.cpp`: a `records` tab via `WR_REGISTER_JOURNAL_TAB` with Boards, Achievements, Titles and Geis sub-tabs. `WRPlayerMenus.cpp`: altar pages. `WRMainMenu.cpp`: Hall of Deeds. `WRUIHud.cpp`: hide own bars with `hide_own_hud`, hide enemy bars with `hide_enemy_bars`, the band list, charm-cord icons dimming in PvP pause. Every screen with pad focus, mouse hover and click, B/Esc back, F1 page (ui-controls KEPT) |
| `WRPathComponent.cpp` | `AwardXP` × (1 + min(cap, artifacts + shrines + rested)); `Prestige(PathId, PerkId)` (void's rules); `IsMastered(PathId)` = ever reached 100 (for DR #18 and the Dream hybrids) |

### 3.3 Assets and the art route

**Route for every creature, boss and prop:** concept art (Aaron picks) → Hunyuan3D 2.1 base → Blender finish (retopology, UVs, PBR materials, real anatomy: muscle masses, joints that bend where bones would) → `Tools/art` rig scripts → real animation clips (Fab creature packs or mocap retargeted per species; MetaHuman clips for human-shaped bodies) → shown standing, still and at scale beside the hero before any behaviour is written (FULL-GAME 3.3 KEPT). No engine primitives, no unfinished AI meshes. Feet planted, gait speed matched to ground speed. Fab packs are used wherever a pack fits (licence rule: use unless explicitly barred).

| Item | Count | Route |
|---|---|---|
| Artifact charms | 30 (×5 metal looks by material swap) | Blender kitbash, small props on the belt-cord socket; icons rendered from the meshes |
| Portal Alphas and Elders (ranks 1 and 2) | 22 (Desert's are unique) | The species' existing rig and clips; 1.4 to 1.8× scale; a scar material layer (normal plus a rank-coloured emissive in the scars, 0.5 nits); 2 new clips each (shared with Scarred beasts) |
| Portal bosses ranks 3 to 5 | 33 | Rank 3: remade on an existing rig (Grimjaw, Hexcrow, Webspinner, Thornlynx, etc.) with new sculpt and parts. Ranks 4 and 5: new bodies. Human-shaped ones (Böðvildr, the Rime-Witch, Black Annis, the Leshy, the Green Knight, the Ankou, the Smoking Mirror, the Ring-Dancer, the Backwards Man): MetaHuman-proportion base scaled up, sculpted heads (attractive where the brief says people are: Böðvildr, the Rime-Witch, the Green Knight, the Smoking Mirror, the Ring-Dancer; terrifying where the creature is a horror: Black Annis, the Ankou), outfits made in Blender, Mannequin/MetaHuman clips retargeted. Animals and machines (the Hut, the Ziz, the Sampo Engine, the Lift Golem, the Mari Lwyd): Hunyuan base plus Blender hard-surface or creature finish, custom rigs |
| Portal-only creatures | 22 | Same route; the Selkie raider and the Woodwose on MetaHuman bases (attractive, athletic, real-world bodies; sealskin cloak, bark skin) |
| Room kits | 11 (Desert's is the desert cook's) × 32 rooms | A modular kit per biome in Blender on the 4 m grid (walls, floors, door frames, ceilings, props for that people), plus Fab packs where they match; each room a Packed Level Actor / level instance |
| Trap and puzzle props | About 45 actors | Blender hard-surface; every moving part on a visible mechanism (hinges, chains, gears) |
| Raid champions | 10 (the Cantor and the Giantess are void's) | New bodies on the same route; the Fallen Smith and the Burnt Wyrm reuse the fire-frost skeleton meshes as their base |
| War-halls | 5 (Severance is void's) | Each a hand-built cavern in the world (not an instance), under its pass fort, with the machines above |
| Soul weapons | 4 new boss weapons + 12 awakened looks | Blender hard-surface (combat cook's weapon route KEPT); awakened = a material layer (the boss's colour in the fullers and edges, 1 nit) plus Soul art FX |
| Souls, talismans, effigies | 12 souls (one mesh, 12 sigil decals), 12 talismans, 12 effigy meshes (one per biome, sigil swapped per boss) | Blender props |
| Summoning stones | 12 (one per biome, ringed by its waymark) | Blender, from the world-map waymark kits |
| Record tablets | 1 mesh + dynamic text | Blender; text on a render target |
| Scarred variants | One per highland species | Material set (scars, milky eye) + 1.25× scale on the species' mesh |
| Kindled pillar glow | 12 materials | Emissive mask on the pillar waymarks |

### 3.4 Sound and music

| Cue | Detail |
|---|---|
| Creature and boss voices | Every portal creature, portal boss and champion gets its own voice from the fused real animal calls listed in its row; four layers per attack (tell, strike, hit, pain), FULL-GAME 3.3 KEPT |
| Portal beds | One per biome: the biome theme through a long stone reverb, low; the combat layer on top in fights, ducking ≤4 dB |
| Rank-5 portal boss tracks | 12 composed tracks, one per rank-5 boss, in the biome's key and tempo (FULL-GAME 3.3 table) |
| Raid duets | 6 composed pieces (the pass duets arranged for raid phases, 2.9.1); planned 1.5 s silences only before transitions |
| Artifact sounds | A cord tie (leather creak and knot); a charm chime per metal (bone click, bronze, silver, iron, starsteel ring); a dull thud when artifacts pause in PvP |
| Soul forging | The boss's main motif, 6 s, played on the altar (a world sound within 20 m) |
| Summoning | The boss's tell layer from the arena, heard within 300 m; a low ritual drone at the stone |
| Record tablets | A stone grind when a record falls (within 10 m) |
| Mix | −18 LUFS integrated (calm −23, boss −15), true peak −1 dBTP (FULL-GAME 3.3 KEPT); all rows in `Data/Audio/sounds.json` |

### 3.5 Map dressing

| What | Where | From |
|---|---|---|
| Portal slot spawn masks | Every biome, by rank band (lowland, midland, highland, hidden sub-biome), excluding claims, camps, shrines, arenas and passes | `portals.json` + `world_pois.json` |
| Summoning stones | Beside each major's run-back stone (40 to 60 m from the arena door) and at each mini lair mouth | `world_pois.json` |
| Record tablets | Beside every run-back stone; at each raid door; one per biome at its Crown pillar for portal records | `world_pois.json` |
| War-halls | Under the north pass fort (Thaw Hall), Stormgut, Saltgate, Rootgrave, the Flue floor; the Severance is void's; the bricked door and empty weapon racks in the south pass undercroft | World-map lane's passes; terrain lane cuts the caverns |
| Kindled pillar materials | The Ring's twelve pillars | World-map's pillar actors |
| The Thirteenth Stair (DREAM) | The Crown's south face, beside the twelve stairs | Built with the tower |

### 3.6 Build order inside this scope (proof of one first; each step one cook into Aaron's build; Aaron tests through F8)

| Step | Round | Build | Proof it's one whole thing |
|---|---|---|---|
| **E1 (proof of one)** | R5 | `UWRArtifactComponent`, three hooks, AL, the damage / loot / part / XP multipliers in `ApplyHit`, `OnDeath` and `AwardXP`; party = clan for now; PvP pause; the new phase-3 trigger; **one artifact: Forge-brand from Vulkarn**, the first-artifact guarantee, the Artifacts page | A player hooks Forge-brand, fights Vulkarn at AL 1, takes +7%, sees phase 3, gets +5% loot |
| E2 | R5 | Bands; all artifact rows whose sources exist (Fire and Frost majors, the built biomes' portal artifacts as their portals ship); every geis rule; fusing and salvage; server settings; Geis records; Records data | Every built boss can drop its artifact and every rule works |
| E3 | R5 | Summoning stones and effigies for every built major and mini | Summon Vulkarn on demand |
| E4 | R7 | Soul forging proof: Vulkarn's soul at his altar; the Anvilsplitter blueprint; the awakened weapon and Vulkarn's Flood; the Forge-heart talisman | One soul, both choices |
| E5 | R7, then each round | Souls for every built major | |
| E6 | R5 | Portals: the desert cook's steps 12 to 14 (proof portal, full system, Desert rooms), then the Volcanic and Tundra kits and bosses, the slot table, rewards, personal chests | A Common Volcanic portal with the Cinderfang Alpha |
| E7 | Each biome's round | That biome's room kit, 5 portal bosses, 2 portal creatures, its portal artifact | |
| E8 | R2 on | Scarred beasts on every built biome | |
| E9 | R5 | Record tablets, the Journal Records tab with the Geis sub-tab | |
| E10 | R10 | Raids: void's Severance proof first (void plan), then the Thaw Hall | |
| E11 | R10 | The Gut Hall, the Salt Hall, the Root Crypt, the Flue Shaft | |
| E12 | R10 | Mythic armour (on void's Ring forging); prestige (void's plan) | |
| E13 | R10 | Achievements, titles, the Hall of Deeds, kindled pillars, the monument AL line | |
| E0 | Any round, small | The shard cost table and the XP boost cap in `tuning.json` | |
| E14 | After the full-release gate, with Raven | DREAM: the endless tower, the twelve Elders, unique mythic movesets, hybrid paths and the god path | |

### 3.7 Every build step, listed (for the build lane)

1. `Data/artifacts.json` with tuning and the Forge-brand row; `items.json` artifact item; `WRArtifacts.h/.cpp` component on the player state; save and load in `SavePlayer` / `LoadPlayer`.
2. `ApplyHit` damage-taken scale and PvP pause; `OnDeath` loot, part and artifact rolls; `AwardXP` scale with the cap.
3. `AnyArtifactModifiers` → top-party AL; part ids from data.
4. Artifacts page in `WRUIInventory.cpp`; HUD charm cord.
5. Forge-brand rule in `WRAbilities.cpp`; first-artifact guarantee in `OnDeath`.
6. Bands in `WRSocial` with the invite screen; `PartyOf`.
7. The other 29 artifact rows and their rules, each as its source ships; fusing and salvage recipes; server preset keys; Geis records and rewards.
8. `WRSummoning`: stones from `world_pois.json`, effigy recipes, summon party rule in `AWRBoss`.
9. `souls.json`; altar Forge soul page; soul art ability classes in `ClassForNode`; talismans; the four new boss-weapon blueprints.
10. Portal slot table, creature rank multipliers, rewards, personal chests, the "folds full" line (on the desert cook's portal code).
11. Per biome: room kit, traps and puzzles (actor list in 3.2), portal creatures, five bosses with `WRBossMechanics` handlers, the portal artifact, the rank-5 track and the portal bed.
12. Scarred spawns, stats, moves, drops, taming and breeding flag.
13. Records tablets and Journal Records tab with Geis.
14. Raid halls: door, hall, champions, rivalry, machines (flip tiles, rods, sluices and tidal wheel, root platforms, chain lift and turbine); raid artifacts; raid duets.
15. Mythic armour at the Ring; prestige.
16. Achievements, titles, Hall of Deeds, kindled pillars, monument AL line.
17. DREAM items after the gate.

### 3.8 Spot-check (read the code against this plan and the bar; no test suites)

| Check | Look at |
|---|---|
| Every damage number goes through `ApplyHit`, including traps, Thaw bursts, own-element and HP-cost geasa | Grep for direct `SetHealth` or attribute writes in the new files |
| No projectile homes; every projectile's windup + travel ≥0.9 s; nothing fired inside 4 m | The attack rows in `portal_rooms/*.json` and `raids.json`; the projectile spawn code |
| Every attack's damage frame is a contact frame on the visible weapon, claw or projectile (combat C10) | Notify windows on every new boss clip |
| Every trap and puzzle has a ≥0.9 s sound plus visual tell, can kill, hurts creatures and can be disarmed | Each trap actor's timeline |
| Weird rules touch only the wearer; artifacts never change player-vs-player damage; pause on PvP | `ApplyHit` branches; `HasGeis` call sites |
| A rival clan's AL never applies to you | `PartyOf` and `PartyLevel` |
| Every screen: pad focus, mouse, B/Esc back, F1, no blank state | Each new widget |
| Moving machines move continuously on a visible mechanism; clients interpolate | `AWRChainLift`, `AWRRootPlatform`, `AWRSluiceGate`, `AWRTidalWheel` |
| Music never drops out except the planned silences; ducks ≤4 dB | `WRAudioDirector` rules for portals and raids |
| No creature attacker cap | The AI token code: none added |

---

## 4. Out-of-scope findings (one line each appended to `C:\Users\acars\wildreach-night\LATER-COOKS.md`)

1. FULL-GAME 8.1's arc row "Hours 5 to 20: ... bronze" conflicts with CF (bronze needs Hearth 2 = one major core); this cook moves bronze to the first-major stage (endgame 2.1); coordinator should fix the FULL-GAME row.
2. Bands (temporary parties, endgame 2.6.3) are needed by artifacts' party rule; the multiplayer lane should own their full UI and chat integration.
3. Four majors had no boss weapon: this cook names Anvilsplitter (Vulkarn, axe), Rimefang (Skaldrith, sword), Talon of Kharvox (dagger), Galespine (Aeolith, bow); the fire-frost, lightning and sky lanes and the look lane should adopt or rename them, and the swamp lane should confirm the Three-Jaw Glaive's class.
4. The built `AnyArtifactModifiers` (`WRBoss.cpp` 424 to 431) lets any player near a boss open phase 3 for everyone; endgame 2.6.4 changes it to the top-damage party; whoever builds R5 must change it.
5. Scarred beasts (endgame 2.13.3) need each biome lane's highland species to carry a scarred look and the portal Alpha/Elder's two extra clips.
6. Portal rank 1 to 2 Alphas and Elders reuse built tundra creatures (Rimeclaw, Glacimaw) whose movesets aren't in any cook; the fire-frost lane should list their attacks so the Alpha adds fit.
7. Summoning stones need a placed point beside every run-back stone and mini lair in each biome's `world_pois.json`; each biome lane should add it.
