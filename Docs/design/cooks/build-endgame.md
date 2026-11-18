# Wildreach build cook: ENDGAME (lane build-endgame)

Lane: build-endgame, 2026-11-14. Scope (FULL-GAME 8): prestige, a respec item and quest, boss soul forging, mythic gear
needing every biome's materials (data-ready), leaderboards with boss kill times, achievements, titles, cosmetics, boss
trophies as monuments, raids for 6 to 10.

Sources followed (KEPT unless this doc says otherwise): `cooks/endgame.md` 2.7 (souls), 2.9 (raids, the Thaw Hall),
2.10 (mythic), 2.11 (prestige), 2.12 (records, achievements, titles, cosmetics, monuments), 2.16 (screens);
`cooks/biome-void.md` 2.23.2 to 2.23.9 (the raid frame, mythic forging, prestige, boards, achievements, titles,
cosmetics, monuments, screens); Raven's DD ("Respec is possible but costly, through a rare item or a long quest") and
DR #9 ("Rare item / long quest only. Altar swaps keep ranks"), DR #22 (raids tuned for, not capped).

What the game has today that this builds on (read from the code, 2026-11-14): two majors (Vulkarn, Skaldrith), six
minis, ten portal bosses; altars that give a subclass (`WRAltar.cpp`); paths with saved progress (`WRPathComponent`);
artifacts with geis records, Bane titles and bands (`WRArtifacts`); summoning stones and path masters with quest chains
(`WRWorldSystems`, `Data/world.json`); the boss machine with data attack rows (`WRBossCombat.cpp`,
`Data/attacks.json`); the portal folds built at runtime in "bay space" (`WRPortalFold.cpp`); the world store
(`WRStore`, world documents); the stone-and-leather Slate UI with world screens and journal tabs.

---

## 0. Requirement ledger

Every requirement in the brief, one per row. "Plan" is the section that answers it; "Proof" is filled when built
(file:line in the commit).

| # | Requirement (source) | Plan | Proof |
|---|---|---|---|
| R1 | Prestige: reset a path for permanent perks (brief, DD) | 2.1 | `WRDeeds.cpp:424` Prestige; `WRUIEndgame.cpp:433` altar Return page |
| R2 | Prestige at a path's altar when the path is at 100, held Return 1.5 s (void 2.23.4) | 2.1 | `WRDeeds.cpp:406` PrestigeBlock (100 pts, at its altar); `WRUIEndgame.cpp:179` SHoldButton 1.5 s (tuning prestige.holdSeconds) |
| R3 | Prestige resets to 0, XP restarts, freed points back under 150, node layout saved for one-click re-buy (void 2.23.4) | 2.1 | `WRPathComponent` ReturnPath/RestoreLayout; `WRDeeds.cpp:460`; Paths tab button + gold dots `WRUIPaths.cpp:843` |
| R4 | One mark per return, max 3 per path; pick 1 of 3 perks (+5% path damage; +10% resource capacity and gain; root passive on the subclass at 50%, once per path) (void 2.23.4) | 2.1 | `WRDeeds.cpp:365-402`; `WRCombatLibrary.cpp:127,493,497,619`; `WRPathComponent.cpp:587` |
| R5 | Marks permanent, work while that path is equipped (endgame 2.11) | 2.1 | `WRDeeds.cpp:373,382` IsActivePath checks; saved `WRDeeds.cpp` ToJson/FromJson, `WRGameMode.cpp` deeds field |
| R6 | Gold threads on the path's body marks; title "<Path>-sworn" at 3 marks (void 2.23.4) | 2.1, 2.7 | `WRHeroLook.cpp:616` gold lerp; `WRDeeds.cpp:453` sworn title; `Data/achievements.json` titles.sworn |
| R7 | Respec: costly, through a rare item (DD, DR #9) | 2.2 | `Data/items.json` heron_draught; drops `WRDeedsAchieve.cpp:513,576`, `WRWarHall.cpp` OnChampionDied |
| R8 | Respec: or through a long quest (DD, DR #9) | 2.2 | `Data/world.json` The Unlearning x2 (4 steps each); title `WRDeeds::OnQuestStep` hooked `WRWorldSystems.cpp:2206` |
| R9 | Respec keeps altar-swap rules (ranks kept on swap) and isn't prestige (DR #9) | 2.2 | `WRDeeds.cpp:474-508` (active paths only, XP kept); respec screen `WRUIEndgame.cpp:683`; Use from bag `WRInventory.cpp:927` |
| R10 | Boss soul forging: boss drops into unique items (brief, DD) | 2.3 | `Data/souls.json`; `WRDeeds.cpp:535` ForgeSoul |
| R11 | Soul drops: first kill 100%, repeat 10% + 1%/AL, ×2 phase 3, first AL 10+ kill 100% (endgame 2.7) | 2.3 | `WRDeedsAchieve.cpp:449` OnBossKilled (souls.json drop rows) |
| R12 | Forged at the boss's own altar; choice A awakened boss weapon with a Soul art, choice B soul talisman; second soul buys the other; extras salvage to core shards (endgame 2.7) | 2.3 | `WRDeeds.cpp:535-600` (own altar, A/B, salvage to core shards via GiveOrBag); Forge page `WRUIEndgame.cpp` ForgePage |
| R13 | Soul arts as UWRGameplayAbility + ClassForNode, in a quick slot, no new button, 25 stamina, 30 s (endgame 2.7, brief) | 2.3 | `WRAbilities.cpp:1309,1363` UWRGA_SoulArt*, ClassForNode `WRAbilities.cpp:1278`; quick slot `WRPathComponent.cpp` (SoulDisplaced restore) |
| R14 | Souls data for all 12 majors; only built bosses show (brief "data-ready"; "unfinished stays hidden") | 2.3 | `Data/souls.json` 12 rows; `WRDeeds.cpp:108` SoulRow live + registry check |
| R15 | Mythic gear needing every biome's materials, data-ready (brief, DD) | 2.4 | `Data/mythic.json`; `WRDeeds.cpp:604` MythicCheck; item Mark/Element2 `WRInventory.h` |
| R16 | Mythic: ×1.15 over starsteel, 3 fixed sockets, two elements 50/50, mythic Wildbrand; mythic armour counting as a second set (void 2.23.3, endgame 2.10) | 2.4 | `WRInventory.cpp:717` metal factor; `WRCombatTypes.h:129` NextElement (alternating); item card `WRUIInventory.cpp` QualityLine |
| R17 | Mythic stays hidden until the Ring and the twelve resources exist (brief) | 2.4 | `Data/mythic.json` live:false; no screen reads it; achievements need `mythic` |
| R18 | Leaderboards incl. boss kill times: first hit to death, bands, artifact column with AL (void 2.23.5) | 2.5 | `WRRecords.cpp:195` Note; FirstHitAt `WRBoss.cpp:582`; bands `Data/tuning.json records.bands` |
| R19 | Boards per server, never reset; "records season" setting keeps old boards read-only (void 2.23.5) | 2.5 | `WRRecords.cpp:43` Season; settings `WRUISettings.cpp:1879`, `WRWorlds.cpp` RuleKeys |
| R20 | Stone record tablet by each boss's run-back stone (top 3), interact for the full board; Journal Records tab (void 2.23.5, endgame 2.12) | 2.5 | `WRRecords.cpp` PlaceTablets/AWRRecordTablet; interact `WRWarHall.cpp` Prompt/ClientInteract; `WRUIEndgame.cpp:887`; Deeds > Records |
| R21 | Letters grind when a record falls (endgame 2.12) | 2.5 | `WRRecords.cpp:436` OnRep_Lines grind |
| R22 | Achievements: 150 in 10 categories, `Data/achievements.json`, 20 hidden, 3 s toast (void 2.23.6, endgame 2.12) | 2.6 | `Data/achievements.json` 150/10 cats/20 hidden; `WRDeedsAchieve.cpp:425`; toast `WRUIEndgame.cpp:1324` in HUD `WRUIHud.cpp` |
| R23 | Achievements for content not built yet stay hidden (brief) | 2.6 | `WRDeedsAchieve.cpp:305` IsLive |
| R24 | Titles: shown under your name and before it in chat; chosen in Journal (void 2.23.7) | 2.7 | `WRHUD.cpp:421` nameplate; `WRPlayerController.cpp:101` chat; Titles page `WRUIEndgame.cpp` SDeedsTab::Titles |
| R25 | Titles from achievements, prestige, raids, Banes, path masters' last quests (void 2.23.7) | 2.7 | `WRDeedsAchieve.cpp` Unlock (row titles); `WRDeeds.cpp:453` sworn; `WRDeeds.cpp:651` Banes; OnQuestStep |
| R26 | Cosmetics only from hard content, no store (void 2.23.8) | 2.8 | `Data/cosmetics.json` (sources only from bosses/raids) |
| R27 | Weapon glows (one per major, deathless kill: faint element edge) (void 2.23.8) | 2.8 | `WREndgameLook.cpp:311` ApplyDeedsLook; `WRRanged.cpp` held glow; deathless grant in OnBossKilled |
| R28 | Armour dyes (raids) (void 2.23.8) | 2.8 | `WREndgameLook.cpp:311` (TrimColor) |
| R29 | Claim banners with boss sigils, sigil on the claim's map icon (void 2.23.8) | 2.8 | `WREndgameLook.cpp:251` DressBanner; map `WRUIMap.cpp:90` ClaimSigil |
| R30 | Raid pieces (2 per raid) (void 2.23.8) | 2.8 | `WREndgameLook.cpp:300` SeamMark; `Data/cosmetics.json` marks |
| R31 | Prestige gold threads (void 2.23.8) | 2.8 | = R6 |
| R32 | Monuments: a major's trophy head on a stone plinth, comfort 3, bronze plaque with names and kill time, boss sigil on the claim's map icon (void 2.23.8) | 2.9 | `Data/pieces.json` monument_* (12 rows, unbuilt bosses left out `WRBuilding.cpp` registry check); `WREndgameLook.cpp:86`; map sigil `WRUIMap.cpp:90` |
| R33 | Plaque shows highest AL killed by the claim's owners, geis-knot border at AL 15 (endgame 2.12) | 2.9 | `WREndgameLook.cpp:199` RefreshPlaque (knots at AL 15); server line `WRBuilding.cpp:422` SetPlaqueLine |
| R34 | Raids for 6 to 10: a war-hall, one group at a time, tuned for 8, never capped by default (DD, DR #22, void 2.23.2) | 2.10 | `WRWarHall.cpp:181` Start; group cap `WRWarHall.cpp:1372` EnterBlock |
| R35 | Two single-element champions, rivals: cross-damage ×0.5, Triumph at 30% (+20% damage, faster) (void 2.23.2) | 2.10 | `WRWarHall.cpp:1165` IsRival/Incoming; Triumph `WRWarHall.cpp:1110` |
| R36 | Champions 12,000 solo, +80% per extra player or tame, stance 180, phase 2 at 50% by a transition, hidden phase 3 with artifacts (void 2.23.2) | 2.10 | `Data/creatures.json` fallen_smith/burnt_wyrm, `Data/attacks.json`, tuning bosses.* |
| R37 | Door seals when the first raider crosses; opens when the fight ends or the room is empty 8 s (void 2.23.2) | 2.10 | `WRWarHall.cpp:781` Tick (seal on crossing, living-only empty check, openSeconds limit) |
| R38 | Loot by the 10% rule per champion: path materials, Champion's mark, upgrade shards ×3, 3% raid cosmetic (void 2.23.2) | 2.10 | `WRWarHall.cpp:1286` OnChampionDied (10% rule, cosmetic, draught via GiveOrBag; creatures.json loot) |
| R39 | Server setting "raid respawn" 1 to 168 h, default 24 (void 2.23.2) | 2.10 | `Data/server_presets.json` raidRespawnHours; `WRUISettings.cpp:1876`; `WRWarHall.cpp` Won |
| R40 | Raid group cap / tuned settings shown once war-halls exist (server-raids lane) | 2.10 | `Data/tuning.json raiding.warHallsLive` 1; `WRUISettings.cpp:1871` |
| R41 | Death: server rules; bodies at the raid door if the group leaves; run-back stone 40 m from the door (void 2.23.2) | 2.10 | `WRWarHall.cpp:957` Clear (bags replicated to the gate); run-back stone in Start |
| R42 | The Thaw Hall: lava / ice tiles flipped by slams and sweeps, Thaw steam burst on a double flip within 3 s, slams melt the Wyrm's footing, breath hardens the Smith's seams (endgame 2.9.2) | 2.10 | `WRWarHall.cpp:1031` FlipTile, `:1062` Thaw (once per body per blow), Aftermath; slip/harden `WRWarHall.cpp` RecoveryScale |
| R43 | The other five raids: data-ready, hidden until their biomes exist (scope = full game; brief "hidden") | 2.10 | `Data/raids.json` 6 halls, 5 live:false |
| R44 | Raid door screen: name, sigils, group in range, Enter; "Sealed: a group is inside" + time; never blank (void 2.23.9) | 2.11 | `WRUIEndgame.cpp:776` SWarHallCard |
| R45 | Every screen mouse AND pad, no dead states, F1 (brief, BAR) | 2.11 | every screen in `WRUIEndgame.cpp` built of SWRButton / SHoldButton / Card (pad A = synthesised click, held while held), Esc/B back, F1 |
| R46 | Data-driven: numbers in Data/*.json and tuning; all damage through ApplyHit; abilities via ClassForNode; screens via WR_REGISTER (brief) | 3 | spot-check 4 (below) |
| R47 | Server authority and replication for every path (brief) | 3 | spot-check 4 (below) |
| R48 | Design with intention: every object's job, what it connects to, how it meets what it touches; machines engineered, nothing passes through (Aaron 11-14) | 2.12 | 2.12 table; gate grounding `WRWarHall.cpp:181` |
| R49 | Research names real games and real-world reference, specific details used (Aaron 11-13) | 1 | section 1 |
| R50 | Requirement ledger with proof per row (Aaron 11-14) | 0 | this table |
| R51 | CHANGELOG Unreleased + HANDOFF in the same commit; LFS sizes reported (brief) | 3.8 | CHANGELOG Unreleased + HANDOFF in the landing commit |
| R52 | Out-of-scope findings appended to LATER-COOKS (brief) | 5 | `wildreach-night/LATER-COOKS.md` 6 lines 2026-11-14 build-endgame |

---

## 1. Research

### 1.1 Games, the specific system, what we take

| Game / system | What it does and why it works | What we take (and where) |
|---|---|---|
| **Call of Duty Prestige (CoD4, 2007)** and **Borderlands 3 Guardian Rank** | CoD4: at level 55 you trade everything for a new emblem and restart; the emblem is the point (status, not power). Borderlands 3 splits the reward into small permanent stat tokens that keep growing after the cap | Prestige gives a visible mark (gold thread, title) **and** a small permanent perk. Small numbers (+5%, +10%) so a prestiged player isn't a different species (2.1) |
| **Path of Exile Atlas / Ascendancy re-pick** and **Diablo IV respec** | PoE: Orb of Regret refunds one point, Book of Regrets is a quest reward; Diablo IV makes respec a per-node gold cost and a "reset all" button | Respec is one path at a time (never your whole character); refunds every point of that path; XP stays (2.2) |
| **Elden Ring Rennala's rebirth (Larval Tears)** | A rebirth costs one Larval Tear; tears are rare drops from specific glowing enemies and a few placed spots, so respec is possible but each one is a decision | The **Heron's draught** is a rare drop from major and high portal bosses, never sold or crafted (2.2) |
| **Conan Exiles Yellow Lotus Potion**, **Ark Mindwipe Tonic** | Crafted resets made respec routine; Ark limits it with a cooldown on official servers | What not to do: Raven wants respec rare, so no recipe (DR #9) |
| **Dark Souls 3 Rosaria (Pale Tongue)** | Respec behind a covenant and a quest line, limited count | The **long quest**: each path master's final arc, "The Unlearning", pays one draught once (2.2) |
| **Elden Ring remembrances / Enia** | A boss's remembrance buys one of two unique items; the other needs a second remembrance | Souls (2.3) — KEPT endgame 2.7 |
| **Dark Souls 1 boss soul weapons (Giant Blacksmith)** | The boss soul + a +5 boss weapon line makes a unique weapon with its own moveset | Awakening needs the boss's own blueprint weapon (Cinderbrand, Rimefang): the soul makes *that* weapon stronger with a Soul art (2.3) |
| **Destiny 2 raids (Vault of Glass, Last Wish)** | Encounters built on roles you discover; the room's architecture is the instruction; checkpoints per encounter | The Thaw Hall's tile floor is the mechanic and its instruction: you see a tile change colour, you learn the Thaw (2.10) |
| **WoW Twin Emperors (AQ40) and Twin Val'kyr** | Two bosses that must be kept apart and even; if one dies far before the other, the other enrages | Triumph at 30%: keep the two champions even; split the raid (2.10) |
| **Monster Hunter Arena / Challenge Quest timers, Speedrun boards** | A kill time per quest with a party-size band; times shown on an in-world board | Boards banded by group size (tames count), artifact column with AL (2.5) |
| **Valheim boss trophies** | A boss trophy mounted on an item stand reads as "I beat that" to every visitor | Monuments: the real head on a plinth with a bronze plaque (2.9) |
| **Skyrim / Elder Scrolls Online trophies; Monster Hunter Rise room decor** | Trophies count toward the room's décor | Monuments give comfort 3 in the "trophy" group (2.9) |
| **Xbox 360 achievements (2005) and Steam achievements** | Short names, one line of how-to, a toast at the top; hidden ones show "?" until earned | Toast top-centre for 3 s; hidden ones counted but not shown (2.6) |
| **Guild Wars 2 titles** | Titles from achievements, shown under the name; one at a time; chosen from a list | Same (2.7) |
| **Halo Reach armour effects, Destiny shaders** | Cosmetics from the hardest content say "I did that" across the room | Weapon glows from deathless major kills, dyes and raid marks from raids (2.8) |
| **Valheim / Rust banners** | A banner on your base is identity | Banner sigils woven from a boss's sigil (2.8) |

### 1.2 Real world

| Source | What we use |
|---|---|
| **Hávamál 13: the heron of forgetfulness (óminnishegri)** "hovers over ale-feasts and steals men's wits" | The respec item is the **Heron's draught**: a corked horn of dark mead with a grey heron feather bound to it. Drink it and one path's learning is gone (2.2) |
| **Medieval dyes**: woad (blue, *Isatis tinctoria*), madder (red, *Rubia tinctorum*), weld (yellow), kermes (crimson, used only as the crimson law forbids: no), oak-gall iron black, walnut-hull brown, lichen orchil (purple) | The dye list (2.8). Colour law kept: no gold, no crimson dye |
| **Bronze casting and engraved plaques** (lost-wax casting; Roman tabulae ansatae, the "dovetail" plaque) | The monument plaque is a tabula ansata in bronze, names cut into it (2.9) |
| **Runestones (Jelling, Rök)** | Record tablets: a grey granite runestone with a band of carved text and the boss sigil on top (2.5) |
| **Taxidermy trophy mounts; the Celtic tête coupée (severed-head cult, Roquepertuse portico)** | A boss head on a stone plinth is an old human idea: the Roquepertuse sanctuary had niches for heads. The plinth gets a carved niche frame (2.9) |
| **Bloomery quench and steam explosions (water meeting molten metal: a real steam explosion hazard)** | The Thaw burst: a tile flipped from lava to ice (or back) within 3 s bursts in steam (2.10) |
| **Ice on basalt (Iceland's Vatnajökull over volcanic beds)** | The Thaw Hall's floor: basalt flags, blue ice, lava under (2.10) |

### 1.3 Lessons used below

1. Status first, power small (prestige, cosmetics, titles).
2. Respec rare, one path at a time, never sold or crafted.
3. The world is the menu: altar, tablets, plinths, the war-hall gate.
4. Rival bosses are a roles puzzle you see on the floor.
5. Hide what isn't built; data for all twelve biomes is in place so each later biome lights its rows up.

---

## 2. Design (every gap solved; numbers in data)

### 2.1 Prestige (R1 to R6)

- **Where:** the path's altar. The altar now opens a screen (`altar`) with pages: **Path** (take as subclass / your
  main / your subclass: Raven's built rule), **Return** (prestige), **Forge soul** (2.3). With `-oldui` the old one-line
  interact still works.
- **Return page:** the path's points (x / 100), its marks (0 to 3, shown as three gold knots), the three perk cards
  (A *Keener*: that path's damage +5%; B *Deeper*: its resource capacity and gain +10%; C *Rooted*: its root passive on
  the subclass slot at 50%, once per path). Pick a card, then hold **Return** 1.5 s (mouse hold or pad A hold; a ring
  fills; releasing early cancels). Greyed with the reason: "Reach 100 points in Fire (now 63)", "Three marks: this path
  is sworn", "Only at the Fire altar" (never blank).
- **What happens (server):** path XP → 0, ranks → the starter only; the old ranks are saved as that path's **layout**;
  `Marks[path]` += perk. The character re-applies (level, HP, abilities). Notice + sound (the boss altar's flame roars,
  `altar.return`). Achievement events.
- **Re-buy:** the Paths tab gets a **Restore layout** button (shown when a saved layout exists): buys the saved nodes in
  tree order as far as points allow (one click), and nodes in the layout carry a gold dot.
- **Perk effects (only while the path is main or sub):** A → `ApplyHit` × (1 + 0.05 × A-marks) on hits whose
  `Hit.PathId` is that path or whose element is the path's element from that player. B → max Heat/Chill × (1 + 0.1 ×
  B-marks) in `ApplyToCharacter`, gain × same in `GainResource`. C → the subclass's root passive tag at half strength:
  Fire *Burning blood* half (your burn and lava damage taken ×0.5), Frost *Permafrost* half (+7.5% vs slowed).
- **Shows:** gold threads: the skin path-mark colour lerps toward gold by 0.12 per mark (max 0.36), a 1-nit gold edge
  (colour law: gold = yours). At 3 marks the title "Ember-sworn" / "Rime-sworn" (data `paths.<id>.sworn`).
- Numbers in `Data/tuning.json prestige` (maxMarks 3, damage 0.05, resource 0.1, rootShare 0.5, holdSeconds 1.5,
  goldPerMark 0.12).

### 2.2 Respec (R7 to R9)

- **The rare item: the Heron's draught** (`heron_draught`): a horn of dark mead stoppered with wax, a grey heron feather
  tied to it with red thread. Category `consumable`, stack 1. Not craftable, not sold.
- **Drops:** major boss repeat kills 3% (+0.5% per party AL), portal bosses rank 3 / 4 / 5: 2% / 4% / 6%. First-kill and
  every-kill tables untouched.
- **The long quest: "The Unlearning"**, four steps appended to each path master's line (after their sixth), so it opens
  only after the master's whole line (long by design):
  - Sigrun (Fire): *Cold ashes* (bring 20 obsidian, 10 ember essence), *Hot iron, cold water* (quench a fire weapon 3
    times), *What the giant knew* (show Vulkarn's core), *The heron's cup* (bring 5 frost essence: the opposite land's,
    so it's drunk cold) → pays the draught and the title **Unlearned**.
  - Áilu (Frost): *Thin ice* (bring 20 ice crystal... see data), *Breath held* (sneak by a sleeping Rimeclaw), *What the
    wyrm knew* (show Skaldrith's core), *The heron's cup* (bring 5 ember essence) → the draught, title **Unlearned**.
- **Using it:** from the bag (Use) → a screen (`respec`): your main and subclass paths as two cards (points spent,
  saved progress); pick one, hold **Forget** 1.5 s. That path's ranks go (the starter stays, free), XP stays, so every
  point is back to spend at once. Inactive paths (swapped out at an altar) can't be chosen (they keep their ranks:
  DR #9 "altar swaps keep ranks"). Greyed: "No points spent in Frost".
- Not a prestige: no mark, no XP lost.

### 2.3 Soul forging (R10 to R14)

- **Souls:** `soul_<boss>` items (category `soul`): a coal-dark stone with the sigil glowing inside. Drop per looting
  player: first kill 100%, repeat 10% + 1% per party AL, ×2 in hidden phase 3, first kill at AL 10+ 100%.
- **At the boss's own altar, Forge soul page:** two cards. **A: Awaken** the boss weapon (Cinderbrand for Vulkarn,
  Rimefang for Skaldrith: the built blueprints; endgame 2.7's Anvilsplitter becomes Cinderbrand since the code already
  has Vulkarn's weapon). Needs the weapon in the bag (any quality / upgrade); it comes back **awakened**: metal factor
  starsteel, quality, roll, sockets, upgrade kept, element fixed to the boss's, and its **Soul art**. **B: the soul
  talisman.** A second soul buys the other card; after both, souls salvage at the altar into 5 core shards each (wait:
  core shards don't exist in items.json; salvage pays the boss's core ×1 instead, the starsteel input in this build).
- **Soul arts** (weapon arts, not path Wildbrands; one quick slot; 25 stamina; 30 s): granted while an awakened weapon
  is equipped by putting `soul.<boss>` in the first empty quick slot (or slot 8 if full, remembered and restored when
  unequipped). `UWRGA_SoulArt_Vulkarn` *Vulkarn's Flood*: overhead chop W 0.75 / A 0.2 / R 0.6, ×2.2, then a 6 m crack
  of lava ahead for 4 s (20/s + burn 20) through `AWRHazard` (ApplyHit inside). `UWRGA_SoulArt_Skaldrith`
  *Pillar-breaker*: spinning cut W 0.5 / A 0.3 / R 0.5, ×1.6 in 300°, frostbite 50, stance ×3. Rows in tuning
  `wildbrands.soul_vulkarn` / `soul_skaldrith` (the Wildbrand machinery, so the hit stack, coil and buffer are the
  built ones).
- **Talismans:** *Forge-heart* (path resource gain +10% while above 70% HP: worn `resourceGainHighHp`), *Rime-scale*
  (the first hit after 10 s unhit deals 30% less: worn `firstHitGuard`).
- The other ten majors: rows in `souls.json` (weapon, art, talisman, effect lines) with `live` false until the boss
  exists; nothing shows for them.

### 2.4 Mythic gear, data-ready (R15 to R17)

- `Data/mythic.json`: the Ring rule (10 of each of the 12 signature resources: sulfur_crystal, lichen, stormglass,
  sunstone, heartwood, levistone, resonant_crystal, bloodmoss, tidecoral, soulstone, deepcap, voidglass), 3 champion's
  marks, a +10 masterwork starsteel weapon or ring, channel 180 s, damage ×1.15, sockets 3, both elements; the ten
  mythic weapons with their Wildbrand rows; mythic armour (1 mark + 5 of each, a second set).
- **Code that's live now:** item instances carry `Mark` (`awakened` / `mythic`) and `Element2`; equipment reads the
  metal factor (`weapon.metal.mythic` = 3.22) and alternates the two elements per swing; the item card shows both. The
  forging function `WRDeeds::ForgeMythic` validates the full recipe.
- **Hidden:** `mythic.json live: false` until the Ring on the Crown and all twelve resources exist (the resources for 10
  biomes aren't items yet). No screen shows it.

### 2.5 Leaderboards and kill times (R18 to R21)

- **Timed:** every major, mini, portal boss and raid (both champions, from the first hit on either to the second's
  death). `AWRBoss::FirstHitAt` set on the first player damage, cleared on reset.
- **Entry:** names (everyone with damage in the ledger), group size (Participants: players + tames), band (solo, 2, 3–4,
  5–7, 8–10, 11+), seconds, AL (party AL of the top ledger name), unix time.
- **Store:** world document `records`: `{season, seasons: {"1": {bosses: {<id>: {kills, first: {names, at}, boards:
  {"<band>": [10 best], "<band>.al": [10 best]}}}}}}`. Season = server setting `recordsSeason` (1 to 20); a higher one
  starts empty boards, the old season stays readable.
- **Tablets:** a granite runestone (SimpleRocks slab, cut face) 1.9 m tall beside each major's shrine-stone (600 cm to
  its left, mirroring the summoning stone on its right) and beside each mini's summoning stone (350 cm to its right),
  and at the war-hall gate. A carved top band with the boss sigil (woven from the sigil grid) and three lines of cut text
  (TextRender, pale, unlit: "1  Aaron, Colin  3:12"). Interact → the full board screen. When the board changes the text
  updates with a stone grind sound (`records.grind`) heard within 10 m.
- **Journal → Deeds → Records:** every boss with kills, your best, the top 10 per band, a band picker and an
  "artifact runs" toggle; "No kills yet" when empty.

### 2.6 Achievements (R22, R23)

- `Data/achievements.json`: 150 rows, `{id, name, text, cat, event, params, needs[], hidden, title?, cosmetic?}`, by the
  endgame 2.12 generator (Explorer 30, Hunter 24, Slayer 30, Path 14, Life 15, Builder 10, Survivor 6, Raider 8, Mythic 6,
  Prestige 7). 20 hidden.
- **Live rows:** a row is live when every `needs` item exists (`boss:<id>`, `biome:<id>` = any creature of it,
  `path:<id>`, `paths:<n>`, `skill:<id>`, `raid:<id>`, `mythic`, `pass:<poi name>`). Not-live rows never show, never
  unlock.
- **Events (server):** kill (species, boss, solo, deathless), tame, poi (landmark visited within 60 m), pass (crossed
  the ridge within 300 m of a pass), hidden (entered a hidden tube), path100, combo, skill100, census (every 30 s:
  claims, Hearth level, monuments, tames, turrets, comfort, set pieces), days (alive game days), raid, triumph, prestige,
  geis (AL), portal (rank / modifiers), escaped (escaped boss killed), artifactAL.
- **Toast:** top-centre, 3 s, a brass-edged card: category glyph, name, line; queued; click or pad B dismisses.

### 2.7 Titles (R24, R25)

- Titles come from: achievements with `title`, prestige (sworn), raids, Banes (artifacts' existing list is read in),
  quests (*Unlearned*, and *Ashhand's hand* / *Ice-patient* for finishing Sigrun's / Áilu's lines).
- `UWRDeedsComponent::ActiveTitle` replicated to everyone. Nameplate: a second, smaller line in pale brass under the
  name. Chat: "‹Title› Name: text".
- Journal → Deeds → Titles: list, A / click to wear, "Wear none".

### 2.8 Cosmetics (R26 to R31)

`Data/cosmetics.json` (sources only from hard content; no store):

| Kind | Rows live now | How it shows |
|---|---|---|
| Weapon glow | *Forge-ember* (Vulkarn deathless kill), *Wyrm-rime* (Skaldrith deathless) + 10 data rows | `M_WindupGlow` overlay on the held weapon (sword mesh or modelled one): element colour, Wash 0 (edge only), Glow 0.9, Opacity 0.16 |
| Armour dye | *Quench-black* (oak-gall iron black) and *Thaw-blue* (woad), 3% from the Thaw Hall's champions + 12 data rows | outfit MIDs `TrimColor` |
| Raid mark (raid piece) | *Smith's seams* (molten seam glow, `PathGlow` with ember colour), *Wyrm's rime* (ice inlay, `InlayNits` with pale blue), 3% each champion + 10 data rows | outfit MIDs |
| Banner sigil | each boss you've killed: its sigil woven on your claim banners and drawn on your claim's map icon | build look banner emblem from the sigil grid; `WRUIMap.cpp` |
| Prestige threads | from marks | 2.1 |

Worn choices (`WornGlow`, `WornDye`, `WornMark`, `BannerSigil`) replicated to all; Journal → Deeds → Cosmetics.

### 2.9 Monuments (R32, R33)

- Pieces `monument_vulkarn`, `monument_skaldrith` (data rows for all 12; unbuilt bosses' rows `hidden`). Cost: the
  boss's **trophy head** (`trophy_head_<boss>`: every looter's first kill 100%, then 25%) + stone 30 + bronze 4.
  Comfort 3 (group "trophy"). Built in build mode like any decor.
- **The look:** a 1.3 m granite plinth on a stepped base (two courses of ashlar, a chamfered top), the front cut into a
  Roquepertuse niche frame; on top, **the boss's real head**: the boss's own skeletal mesh posed so every bone but the
  head and its children collapses to the neck (a `UPoseableMeshComponent`, component-space transforms from root to
  leaves), at `monument.scale` (Vulkarn 0.45, Skaldrith 0.3), facing out, in its own materials. Below it a **bronze
  tabula ansata plaque** with the boss name, the claim owners' best kill time (from the records board), the names of
  that kill, and their highest AL; at AL 15 a knot border. Interact: the plaque screen.
- **Map:** a claim with a monument draws that boss's sigil on its claim icon.

### 2.10 Raids: the war-hall frame and the Thaw Hall (R34 to R43)

**The gate (world, at the North pass):** the island has no pass fort yet (a later cook), so the gate stands where the
fort's undercroft door will be: cut into the ridge rock 18 m east of the North pass road, facing the road. A sunken stair
of 6 basalt steps between two dry-stone cheek walls leads down to a doorway: a heavy oak double door on iron strap
hinges in a basalt frame, a lintel carved with the two champions' sigils (the hammer left, the wyrm right). A brazier
on each side (fire left, a cold blue lamp right). The run-back stone stands 40 m back up the road; the record tablet
beside the stair head. Interact with the door → the gate screen.

**The hall (runtime-built in hall space, X = −1,500,000, like the portal bays):**
- 112 × 72 m floor of 4 m flags (28 × 18 = 504 tiles), basalt by default; under them, a lava sump seen through 1 m
  gutters every 5th row (glow + heat haze; falling in = lava damage). Walls: coursed basalt ashlar on the west half,
  glacier ice on the east (the two lands meet in the hall); a 40 m barrel vault on 8 pairs of ribs. A vestibule (12 ×
  10 m) at the south with the exit arch and the inner threshold line (a bronze strip in the floor). Lighting: lava glow
  from the gutters, braziers on the west wall, cold blue light through ice on the east.
- **Tiles:** state per tile (basalt / lava / ice), replicated as a byte array; three instanced meshes (basalt flag, a
  lava flag that glows, an ice flag). Lava: 30/s + burn 20/s to anyone standing on it (players and champions; the Smith
  is immune to lava, the Wyrm isn't). Ice: slick (the built ice hazard: a slide). Flipped tiles revert to basalt after
  25 s. **Thaw:** a tile flipped from lava to ice or ice to lava within 3 s bursts in steam: 150 damage (ApplyHit,
  element physical, impact 3) to anything on it or touching it, champions included, plus Blind 20.
- **The Fallen Smith** (Fire): Vulkarn's rig at 1.4× with a bone-pale overlay and molten glow lines (the forge-giant
  skeleton knitted with molten iron), voice: Vulkarn's set pitched down. Rows: hammer combo ×3; **overhead slam** (flips
  every tile in 6 m to lava); bone-tongs grab (crimson); slag throw (lands: 2 m of tiles to lava); bellows cone; phase 2
  at 50% by **Rekindle** (1.4 s crimson: every ice tile within 12 m → lava), adds the delayed overhead and combo ×4;
  hidden phase 3 **Quenching**: dual hammers, and each ice tile he steps on hisses to basalt with a steam puff (Blind 20
  in 3 m).
- **The Burnt Wyrm** (Frost): Skaldrith's rig at 1.15× with a char-black overlay, ice glow lines; Skaldrith's flight
  cycle and wing grounding (the built logic, now for both wyrms). Rows: tail sweep (`low`, flips the tiles it sweeps to
  ice); bite lunge; **frost breath** (12 m cone: tiles in it → ice); wing buffet; jaw grab (crimson); phase 2 by
  **Refreeze** (every lava tile within 12 m → ice), flight, dive, air breath (a strip of ice tiles); hidden phase 3
  **Whiteout** (the built one: hidden in the storm, breath freezes on contact).
- **Rivals:** each is hostile to the other; their blows hit the other ×0.5 (nobody's ledger); each picks the other as
  target only when it's within 7 m and nearer than any player, at most every 12 s. **Triumph:** when one falls under 30%
  while the other is above 30%, the other gains Triumph (+20% damage, ×1.15 speed, an ember / rime aura, a roar) until
  the low one climbs back… it can't heal, so Triumph lasts until the fight ends. "Triumph prevented" = both die with no
  Triumph.
- **Each other's land:** the Smith's slam on ice under the Wyrm → the Wyrm slips (stance taken ×1.5 for 4 s). The
  Wyrm's breath on the Smith → seams harden (his attack speed ×0.85 for 6 s).
- **Numbers (tuning `bosses.fallen_smith`, `bosses.burnt_wyrm`, `raids`):** 12,000 solo each, +80% per extra player
  or tame (Raven, built); stance 180; damage × clamp(raidGroupTuned / 8, 0.5, 1.5). Fight 15 to 20 min for 8.
- **Door:** before the seal anyone of the first entrant's party (clan or band) may enter; the inner threshold seals it
  when the first raider crosses; sealed, only the sealed group may re-enter (after a death) and no one else ("Sealed: a
  group is inside · 6:40"). Group cap: `raidGroupMax` (0 = none). Opens when both champions are dead (then 2 minutes of
  loot time) or when no raider has been in the hall for 8 s (champions reset). A reset moves any death bags in the hall
  to the gate.
- **Respawn:** `raidRespawnHours` (1 to 168, default 24; playtest 1), saved in the `warhalls` world document. Resting:
  "The champions lie still. They rise again in 18 h 20 m."
- **Loot per champion, 10% rule each:** the champion's path materials (ember / frost essence ×8, obsidian or ice
  crystal ×6), **Champion's mark** ×1, upgrade shard ×3, 3% one raid cosmetic (a dye or a raid mark), the Heron's
  draught 3% (2.2), its soul? no: champions drop marks, not souls (endgame 2.7).
- **Records:** the raid's board (`thaw_hall`) times the whole fight.
- **The other five halls:** `Data/raids.json` rows (pass, champions, size, machines) with `live: false`.

### 2.11 Screens (R44, R45): mouse and pad on every one

All built with `SWRButton` (the pad's input processor drives buttons as the mouse does), B / Esc back, F1 controls (the
built Help key works over every screen), no blank states.

| Screen | Contents | Never blank |
|---|---|---|
| `altar` (world) | Tabs: Path · Return · Forge soul | each page's reason line |
| `respec` (world, from the bag) | the two path cards, hold Forget | "No points spent" |
| `warhall` (world) | name, champions and their sigils, state, your group within 10 m, cap, Enter / Back | "Sealed…", "Resting…", "Ready" |
| `records` (world, tablet) | the boss's board by band, artifact toggle | "No kills yet" |
| `plaque` (world, monument) | boss, kill, names, AL | "No kill of yours on the board yet" |
| Journal **Deeds** tab | sub-tabs Records · Achievements · Titles · Cosmetics · Marks | each list's empty line |
| Toast | achievement card | — |

### 2.12 How it meets everything it touches (design with intention)

| Thing | Its job in the grand puzzle | What it connects to | How it meets what it touches |
|---|---|---|---|
| Altar pages | One place per path for everything about that path: take it, return it, forge its boss's soul | Path component, souls, the boss arena | The altar stands in the arena behind the boss: you earn the right to use it; the flame roars on Return and forging |
| Heron's draught | A rare second chance | Bosses (drops), path masters (quest) | A thing in the bag; drunk standing still (a 1.5 s drink, the built drink state) |
| Monument | Proof of a kill, comfort, the base's story | Records (plaque time), the trophy head drop, comfort, the map | Stands on a floor or the ground on a 1.4 m footprint; the head sits in the plinth's cup, its neck stump hidden in a bronze collar |
| Record tablet | The server's story in stone | Records doc | Beside the shrine-stone and summoning stone: the three read as one approach to the arena |
| War-hall gate | The way to the raid where the two lands meet | The pass road, the run-back stone, the tablet | Cut into the ridge rock; stair treads sit on the ground trace; the door swings on its hinges when entered |
| The hall floor | The raid's machine and its instruction | Champions' blows, players, steam | Tiles sit flush in the floor grid; the lava sump glows through gutters; flips are instant state changes with a hiss / crack and an FX burst |
| Champions | Two single-element bosses, rivals | Tiles, each other, players | Their blows change the floor; their rivalry is visible (they turn on each other when close) |

---

## 3. Plan: build steps (in order)

1. **Data:** `Data/souls.json`, `Data/mythic.json`, `Data/cosmetics.json`, `Data/achievements.json`, `Data/raids.json`;
   rows in `items.json` (souls, soul talismans, heron_draught, champion_mark, trophy heads), `pieces.json` (monuments),
   `creatures.json` (fallen_smith, burnt_wyrm), `attacks.json` (their rows), `tuning.json` (prestige, deeds, records,
   warhall, wildbrands soul rows, bosses.fallen_smith / burnt_wyrm, weapon.metal.mythic, raiding.warHallsLive 1),
   `server_presets.json` (raidRespawnHours, recordsSeason), `world.json` quests (The Unlearning ×2).
2. **`WRDeeds.h/.cpp`**: `UWRDeedsComponent` on the player state (marks, perks, layouts, souls forged, achievements,
   counters, titles, cosmetics, worn choices, visited POIs, kills, tames); save / load; commands `deeds ...`; prestige,
   respec, soul forging, mythic validation; achievement engine (events, census, live rows); hooks called from boss
   death, creature death, taming, artifacts, raids, path XP.
3. **`WRRecords.h/.cpp`**: the records document, boards, seasons, tablets.
4. **`WRWarHall.h/.cpp`**: gate, hall (geometry, tiles, door, vestibule), champion brain, rivalry, Triumph, loot,
   respawn doc.
5. **`WREndgameLook.cpp`**: monument look (plinth, poseable head, plaque), sigils.
6. **Engine hooks:** `WRBoss` (first hit time, deathless, soul / draught / head drops, IsHostileTo rival, wyrm flier,
   Aftermath hook, records), `WRCombatLibrary` (prestige damage, rooted passives, resource), `WRPathComponent`
   (prestige reset, rebuy, resource capacity, rooted tags), `WRInventory` (Mark, Element2, metal factor, soul art slot),
   `WRAbilities` (soul arts, ClassForNode soul), `WRHeroLook` (gold threads, dye, raid mark), weapon glow,
   `WRHUD` nameplate title, chat title, `WRSurvival` (hall underground/biome), `WRPlayerMenus` (interact order),
   `WRWorlds` + `WRUISettings` (settings rows), `WRUIMap` (sigils).
7. **UI `WRUIEndgame.cpp`**: altar, respec, warhall, records, plaque screens; Deeds journal tab; toast; key binding
   "Deeds" (`.`); tab order.
8. Compile, fix every warning in my files.
9. Spot-check (section 4) by reading the code; fix.
10. CHANGELOG Unreleased, HANDOFF, commit by name, push; LANDED line.

## 4. Spot-check list

- Every damage number through `ApplyHit` (Thaw bursts, lava tiles, steam, soul art lava, rival blows).
- Server authority on every command (`deeds ...` validated server-side; screens only send intent).
- Replication: tiles, gate state, titles, worn cosmetics, tablets' text.
- Pad and mouse on every screen; B / Esc; no blank state.
- Hidden: unbuilt bosses' souls / monuments / achievements / raids / mythic never show.
- No placeholder presented as finished.

## 5. Out of scope (appended to LATER-COOKS)

- The pass forts (the war-hall gate stands where the north fort's undercroft door will be).
- Composed raid duets (the Thaw Hall plays the built boss pool).
- The Ring on the Crown (mythic forging's place) and the twelve signature resources' items.
- The hero transmog layer (cosmetics apply as overlays now).
- The other five raids' halls, champions and art.
- Steam / EOS achievements mapping when M7 lands.


## 6. Build record (2026-11-14, resumed lane)

Built on master from the WIP commit 7bb0f42 plus the finishing work. Gates: editor compile only (clean). Spot-check
(section 4) done by reading the code with an independent reviewer pass; its findings were fixed: a blow's Thaws hurt each
body once; only living raiders hold the hall, and a group that never crosses the line is walked out after openSeconds;
the plaque line is set on dedicated servers; a flying champion's hover is kept inside the walls, which now rise to the
vault; bags moved to the gate replicate; the raid board keeps the first champion's fighters; hardened seams slow the
Smith's attacks (the pause between strings), not his walk; soul arts' ground effects don't come after an interrupted
swing; Rime-scale ignores burns and lava ticks; full bags never lose a draught or salvage; Triumph speeds the Wyrm's
wings; the soul art's displaced quick slot comes back; Smith capsule 263 / 455.

Deliberate changes from the plan (recorded, not dropped):
- **The gate's stair goes three steps up over a buried foot, not six down**: a sunken stair would need the landscape
  heightfield cut at runtime, which the engine can't do; the facade is grounded at its lowest corner and heaped ridge rock
  hides the slope.
- **Pillar-breaker is a spear thrust with a 4 m line of ice spikes, not a 300 degree spinning cut**: Rimefang is a spear;
  one verb per weapon class, and the thrust reads at range. Numbers in tuning `wildbrands.soul_skaldrith`.
- **Two lava gutters** (y = +-12 m) rather than one every fifth row: the floor between reads as three fighting lanes.
