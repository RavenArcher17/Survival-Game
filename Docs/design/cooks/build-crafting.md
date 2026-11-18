# Wildreach cook: BUILD-CRAFTING (FULL-GAME 6.3)

Lane build-crafting, 2026-11-14. Scope: steel (Hearth 4) and starsteel (Hearth 5) tech tiers, runes, salvage,
blueprints from bosses and portals, biome armour set bonuses, talismans and rings. Follows the crafting cook
`cooks/survival-crafting.md` 2.13 / 2.14 (numbers, set table, rune table, talisman table) and the hero cook
`cooks/hero.md` 2.5 (the 12 set looks + Ascendant, already in `player_art.json`). Built to compile; Aaron tests.

---

## 0. REQUIREMENT LEDGER

| # | Requirement (source) | Done where |
|---|---|---|
| R1 | Steel tier at Hearth 4 (4 distinct major cores): steel ingot (2 iron + 2 charcoal), steel plate (1 ingot → 2) (FULL-GAME 6.3, SC 2.13) | `Data/recipes.json` steel_ingot / steel_plate; `TechLevelForCores` unchanged (0/1/2/4/6) |
| R2 | Charcoal from a charcoal kiln (workbench L2: stone 20, clay 10 → we use stone 20 + copper 4; 10 wood → 4 charcoal, 2 min) (SC 2.13) | `Data/pieces.json` charcoal_kiln, recipe charcoal, `WRBuildLook` kiln |
| R3 | Steel weapons, every class, ×2.2 (tuning already has it) (CF L4) | `Data/items.json` steel_* (6 classes), recipes |
| R4 | Steel armour = the 12 biome sets, 4 pieces each, Hearth 4 (CF L4 "steel armor, biome set bonuses"; hero 2.5 tier 5) | `items.json` <set>_head/_armor/_hands/_legs; `Data/gear.json sets` |
| R5 | Starsteel at Hearth 5: 2 steel ingot + 1 star iron + 1 core shard (CF "forged with boss cores") | recipes starsteel_ingot |
| R6 | Core shards: a spare major boss core → 10 shards at the forge; a core in a Hearth can't be broken | forge salvage of `core` items (`FWRSalvage`) |
| R7 | Star iron: band 4 ore, the deepest highland chambers (SC 2.13 ores by depth) | `WRWorldDressing.cpp` AWRCave star-iron vein in caves above `caves.starIronMinZ`; `WRResourceLook` meteoric look |
| R8 | Starsteel weapons ×2.8 and starsteel armour = Ascendant biome sets (CF L5; hero 2.5 tier 6) | items starsteel_*, <set>_ascendant_*; armorLooks ascendant |
| R9 | Upgrade caps +8 / +10 at Hearth 4 / 5 (CF) | `UpgradeCap` already Hearth×2; UI cap min(10) |
| R10 | Gear slots: Armor becomes Head, Chest, Hands, Legs; + Talisman ×1, Ring ×2 (SC 2.14; needed by Raven's "Set crafter: set bonuses need one fewer piece") | `WRInventory.h/.cpp` EWREquipSlot + equipped fields |
| R11 | Old saves migrate: the old armour becomes the chest piece and the matching 4-piece kit is made (SC 2.14) | `UWRInventoryComponent::FromJson` gearSlots 2 migration |
| R12 | Each worn piece shows its own look on the hero (4 visual slots, hero 2.5) | `WRHeroLook.cpp` per-slot looks |
| R13 | Biome set bonuses at 2 and 4 pieces; Set crafter at 1 and 3; the 4-piece always passes its land's hazard (SC 2.14) | `gear.json sets`, `UWRInventoryComponent::SetPieces / GearStat` |
| R14 | Forgeplate: 2 = burn buildup ×0.5; 4 = walk lava unhurt, Heat fills 15% faster | buildupTaken.burn, passLava, heatGain hooks |
| R15 | Rimeguard: 2 = frostbite buildup ×0.5 + blizzard holds off; 4 = grip on slick ice, Chill fills 15% faster | buildupTaken.frost, weather `sets`, passIce, chillGain |
| R16 | The other 10 sets in data with their bonuses, hidden until their land is live | `gear.json` + items `biome` |
| R17 | Runes: crafted at the alchemy table L3+ from resonant crystal + a biome material, or found in portals; socket like a gem; one of each per item; 8 runes with their effects (SC 2.13) | items rune_*, recipes, `UseSlot` rune rule, `ApplyHit` hooks |
| R18 | Salvage at the matching station: 40% of metal and main inputs (+10% a quality step above crude), 25% of upgrade shards; path material and socketed extras destroyed with a crimson warning (CF, SC 2.13) | `FWRSalvage`, command `salvage`, Crafting tab Salvage category |
| R19 | Extras can be removed, removing one destroys it (CF Extras); Careful hands 50% saves it (Raven's perk) | command `unsocket`, crafting tab, lifeskills careful_hands live |
| R20 | Blueprints are items; reading one teaches its recipe forever (journal "Known blueprints"); craft at any station of its tier (SC 2.13; CF "unlock special recipes at any level that meets their tier") | `FWRBlueprints`, PlayerState KnownBlueprints (saved), recipes `blueprint`, Crafting tab Blueprints category |
| R21 | Blueprint drops: major boss first kill per player 100%, repeat kills 20%; portal bosses rank 1 10% … rank 5 60%; legendary blueprints only rank 4-5 portals | `WRBoss::OnDeath`; `gear.json blueprints.portalOdds` + `FWRBlueprints::PortalDrop` (portals don't exist yet) |
| R22 | Each major boss has 2 blueprints: its boss weapon and its biome's decor pack (SC 2.13) | boss weapons built (Vulkarn's Cinderbrand, Skaldrith's Rimefang); decor packs → later-cooks (building sets not built) |
| R23 | Rings: forge, a band of any metal + one gem, 60% of the gem's socket value, +10% per metal tier above copper, 2 slots, two of the same don't stack (SC 2.14) | items ring_<metal>_<gem>, recipes, `GearStat` unique rule |
| R24 | Talismans: every mini-boss trophy → its talisman at the armorer L2+ (trophy + 2 of the land's signature resource), 1 slot; 40 effects (SC 2.14) | items talisman_*, recipes; live hooks for the 6 island ones |
| R25 | Hazard charms that are talismans (grounding charm, ward charm, nullward) | items + recipes, hidden by land |
| R26 | Smithing XP: salvage 5, steel / starsteel ingots 5 each (SC 2.23) | `lifeskills.json` / tuning xp keys |
| R27 | Mouse AND pad on every new screen element; server authority; replication | equip slots are SWRSlot (pad-ready), crafting entries are list cards |
| R28 | Numbers in data (`Data/gear.json`, tuning), sounds/fx as table rows | `gear.json`, `sounds.json` rows salvage / learn |
| R29 | No placeholders shown: unfinished stays hidden | land gating (`world.liveBiomes`), portal odds dormant |
| R30 | Full cook: research → plan → build → spot-check (Aaron 11-13); design with intention (11-14) | this doc |

---

## 1. RESEARCH

### 1.1 Games

| Game | Place / system | What makes it work | What we take |
|---|---|---|---|
| **Valheim** | Charcoal kiln (wood → coal, fed by hand, 1 coal per wood ~), smelter, forge upgrades by adding workbench extensions; metal tiers bronze → iron → silver → black metal gated by biome bosses; every worn armour piece wears on each hit; set bonuses (Troll leather: +15% sneak, Root harnesk: pierce resist, Fenris: run speed). Three-piece sets. | The tier is a material you can see in the world; a kiln is a *thing* you build that makes coal, so steel has a real chain. Set bonuses are small identity perks, not raw stats. | Charcoal kiln as a placed piece; armour pieces each wear when you're hit; bonuses are identity (lava walking, ice grip) not +X% damage. |
| **Monster Hunter World / Rise** | Armour set skills by piece count (2 and 4, e.g. "Rathalos Mastery"); every set *is* its monster; decorations slot into gear | The set reads as the creature you beat and the bonus is that creature's trick | Each biome set's 4-piece passes that biome's own hazard: the set is the answer to its land. |
| **Diablo II** | Runes socketed into gear (one per socket, order matters), set items with partial and full bonuses shown in green; Horadric cube | A rune is a small physical object with a clear one-line effect; partial set bonus text greys in as you wear more | Runes as socket items with one line each; the item card lists the set bonuses, worn ones lit, unworn grey ("2/4 worn"). |
| **Diablo III / IV** | Blacksmith salvage: gear → materials by rarity; IV's extraction of aspects destroys the item | Salvage turns dead gear into progress; one confirm, clear return | Salvage card shows exactly what comes back and, in crimson, what's destroyed. |
| **Elden Ring** | Talismans (one effect each; duplicates and +1/+2 variants don't stack); upgrade tiers by smithing stones | Each talisman is a build choice; a no-stacking rule keeps choices honest | Two ring slots; two of the same ring don't stack. One talisman slot: a real choice between a trophy talisman and a hazard charm. |
| **Dark Souls 1-3** | Two ring slots; rings from bosses and hidden places | Ring choice is part of the build and visible on the menu doll | Rings on the equipment doll beside the hands. |
| **ARK** | Engrams and blueprint drops (a blueprint item is tradeable and crafts a fixed recipe); Tek tier unlocked by boss kills | A blueprint as an *item* means it can be traded to a clanmate | Blueprints are items; reading consumes it; unread ones can be handed over. |
| **Terraria** | Hardmode ores appear after the Wall of Flesh; meteorite ore lands from a meteor event; boss-gated tiers | The best metal comes from a special place or event, not the common rock | Star iron only in the high caves' deepest chambers (and later meteor craters). |
| **Enshrouded** | Armour sets with 2/3/4-piece bonuses; recipes unlocked by rescuing NPCs / finding blueprints in chests | Visible partial progress keeps you hunting the next piece | Set progress line on the item card. |

### 1.2 Real world

- **Steel**: iron with 0.2-2% carbon. Bloomery iron is carburised in charcoal (cementation) or melted with charcoal (crucible / wootz steel). Charcoal is made by burning wood with little air in a clamp or a domed brick kiln for days; mass yield is ~20-30% (10 wood → 4 charcoal by count is fine for a game). A kiln reads as: a squat brick dome, a fire-mouth arch at the base, small vent holes round the crown, a chimney stub, a stack of split logs beside it and a pile of black lump charcoal.
- **Steel plate**: hammered sheet; reads as a stack of rectangular bars with a forge scale blue-grey sheen.
- **Meteoric iron** (Tutankhamun's dagger, the Cape York meteorite): nickel-iron, near-black with a blue-grey shine, and etched shows the Widmanstätten pattern (criss-cross bands). Our star iron: a black-blue boulder with pale silver-blue lumps that catch light; starsteel blades darker than steel with a faint blue sheen.
- **Rings**: a metal band with a bezel-set stone (signet style). Colour = metal, the stone's colour on top.
- **Runes**: Norse runestones; a rune here is a palm-sized crystal tablet with one carved glyph lit from inside.
- **Talismans**: amulets on a cord, made from trophies (a claw, a fang, a horn tip bound in metal wire): exactly how hunters wore trophy amulets.
- **Blueprints**: rolled parchment sealed with wax; ours: a rolled blue-ink scroll with the boss's wax seal colour.

---

## 2. PLAN (every gap solved)

### 2.1 Tech tiers and materials

| Item | Station, level | Recipe | Notes |
|---|---|---|---|
| Charcoal | Charcoal kiln (Hearth 2 piece) | wood 10 → 4, 120 s | The kiln is its own station piece |
| Steel ingot | Forge 4 | iron ingot 2, charcoal 2 → 1, 8 s | +5 smithing XP (`lifeSkills.xp.steel`) |
| Steel plate | Forge 4 | steel ingot 1 → 2, 4 s | |
| Star iron | Forge 4 | star iron ore 2 → 1, 8 s | |
| Starsteel ingot | Forge 5 | steel ingot 2, star iron 1, core shard 1 → 1, 12 s | +5 XP |
| Core shard | Forge salvage, 5 | a boss core in the bag → 10 | Cores in a Hearth are on the Hearth, never in a bag: safe by construction |

**Charcoal kiln piece** (workbench / Hearth 2, `pieces.json`: stone 20, copper ingot 4, wood 10; hp 800; freeform utility; station type `charcoal_kiln`). Look (FarmBlocks style in `WRBuildLook`, engine shapes, the stone material the stations use): a 2.0 m wide brick dome (a sphere half-sunk on a 30 cm stone plinth ring), a 70 cm arched fire mouth facing the front (a dark recessed box with a half-cylinder arch top), 6 small vent holes round the crown (dark discs), a 40 cm chimney stub on top, a 1.2 m stack of split logs on the right, a heap of black charcoal lumps on the left. Collision fitted to the plinth, the dome and the log stack (`WRBuilding.cpp` Parts). Its fire stays inside the dark mouth (no light: the forge alone glows, as before).

**Star iron** (R7): a vein spawns in each cave whose origin is above `caves.starIronMinZ` (20000 cm: the two tundra highland caves, z 24688 and 28906, the island's highest), at the back of the cave (local −650, −280; ≥ 280 cm from every other node, floor-traced like them). Ore look: `star_iron_ore` ends in `_ore` so it gets the ore boulder; its rock is near-black blue (0.12, 0.13, 0.17) with pale silver-blue lumps (2.2, 2.4, 2.9). It's a rare highland find; Kilnmites don't mine it.

### 2.2 Weapons

Steel and starsteel in every class with items: sword, dagger, spear, greatsword, great hammer, scythe (12 items). Weights ×1.05 / ×1.0 of iron's; durability 420 / 520 for a sword (others +80 each tier step like iron's pattern). Recipes: forge 4 / 5; inputs = iron's count in steel ingots (+ wood); starsteel uses starsteel ingots. Held-tool colour: steel cool grey (0.3, 0.31, 0.33) (exists), starsteel blue-black (0.06, 0.07, 0.11).

**Boss weapons (blueprints)**: *Cinderbrand* (Vulkarn, great hammer, steel, innate fire + `staggerShockwave` — a stagger sends out the lava shockwave like Giant's ember, but always), *Rimefang* (Skaldrith, spear, steel, innate frost + `blockFreeze`). Forge 4, steel ingot 6/4, ember/frost essence 6, wood 3. Innate element and innate stats live on the item (`equip.element`, `equip.innate`), so they never take a socket. Unique movesets → later-cooks (combat lane).

### 2.3 Gear slots (R10, R11)

`EWREquipSlot`: Weapon, Armor (= the Chest), Ranged, Light, **Head, Hands, Legs, Talisman, Ring, Ring2**. Items say `"slot": "head" | "armor" | "hands" | "legs" | "talisman" | "ring"`. A ring equips into the first empty ring slot, else swaps Ring; `equipto <bag> <slot>` puts it in a chosen one (drag onto Ring 2). All equipped stacks replicate owner-only.

Armour values, poise, resist and load sum over the four armour slots (broken pieces give nothing). A hit wears **every** worn armour piece by 1 (Valheim). Weight: pieces count against equip load and the bag weight; Light weave lightens all four.

**Old tiers split** (15 / 40 / 15 / 30 of the old totals; each piece keeps the old durability):

| Tier | Head | Chest (old id) | Hands | Legs |
|---|---|---|---|---|
| Hide (12 / 8 / 4 kg) | Hide hood | Hide vest (`hide_armor`) | Hide bracers | Hide leggings |
| Copper (22 / 18 / 8) | Copper cap | Copper scale vest (`copper_armor`) | Copper-studded bracers | Copper leggings |
| Bronze (34 / 24 / 9) | Bronze helm | Bronze cuirass (`bronze_armor`) | Bronze cuffs | Bronze lamellar skirt |
| Iron (48 / 32 / 11) | Iron helm | Iron hauberk (`iron_armor`) | Iron gauntlets | Iron greaves |

Recipes split the old inputs the same way (each at least 1). **Migration**: a save without `gearSlots: 2` that has an armour: it stays as the Chest; the matching Head, Hands, Legs are made at its quality and roll (fresh durability) and put on. Chests holding old armour keep a chest piece only.

### 2.4 The hero's look (R12)

`FWRHeroLook` gets per-slot `Looks[4]`, `Elements[4]`, `AscendantMask`. The server sets each from the piece in Head / Chest / Hands / Legs via `armorLooks[<item's look key>]` (each piece names its family's key with `equip.look`, so `hide_head` looks as `hide_armor`, with the element's Cinderhide / Snowhide). `ApplyOutfit` loads each slot's mesh from that slot's look, the trim from that slot's element, the inlay from its Ascendant bit; hair hides by the Head's look; hide-helmet takes the Head off; body masks per slot from the slot's own look. An empty slot wears the Wraps piece (bare skin is never a hole in the body). The Ascendant crest needs an Ascendant head.

### 2.5 Biome sets (R13-R16)

`Data/gear.json sets`: id, name, biome, load class, poise, armour, the look, and `bonus: { "2": { stats, text }, "4": { stats, text } }`. Pieces of the set and of its Ascendant version both count. **Set crafter** (stat `setCrafter`) counts one more piece once any is worn. Stats:

| Stat | Meaning | Hooked in |
|---|---|---|
| `buildupTaken.<status>` | −0.5 = half buildup | `UWRStatusComponent::AddStatusBuildup` |
| `passLava` / `passIce` | hazard pass | `FWRBiomeHazards::Resists` |
| `heatGain` / `chillGain` | resource fills faster | `GainResource` |
| `passBlizzard` etc. | weather counters | `weather.json counters.sets` (set → pieces needed) |
| `lifesteal` | share of damage dealt healed | `ApplyHit` |
| everything else (`fallDamage`, `breath`, `spellCost` …) | read by its land's system when it lands | listed on the card; hidden with the set |

Pieces (per set): Head / Chest / Hands / Legs = 15 / 40 / 15 / 30 of the set's armour, poise and weight; inputs per the SC table split the same way (at least 1 each). Steel sets at the armorer 4; Ascendant at the armorer 5 = the steel piece's inputs with the steel plates replaced by starsteel ingots at half count (min 1) + 1 core shard for the chest. Live now: **Forgeplate** (steel plate, sulfur, obsidian, Magmaw hide) and **Rimeguard** (steel plate, lichen, Rimeclaw spines, wool). New drops: Magmaw → Magmaw hide 1; Rimeclaw → Rimeclaw spine 2; Basaltback → obsidian 2 (the plates on its back); Obsidian Warden → obsidian 6.

Item card: "Forgeplate set · 2/4 worn" then each bonus line, lit when active, grey when not.

### 2.6 Talismans and rings (R23-R25)

**Rings**: 15 items `ring_<metal>_<gem>` (copper, bronze, iron, steel, starsteel × ruby, topaz, onyx) at the forge at the metal's level: ingot 2 + the gem. Effect = gem value × `gear.rings.share` (0.6) × (1 + 0.1 × tier above copper). Two of the same ring id count once. Ruby → damage, topaz → crit chance, onyx → poise.

**Talismans**: `gear.json talismans` rows: trophy, talisman id, stats, text, biome. Recipe at the armorer L2: trophy 1 + 2 of the land's signature resource (volcanic: sulfur crystal, tundra: lichen). The trophy is used. Live talismans and hooks:

| Talisman | Effect | Hook |
|---|---|---|
| Obsidian heart talisman | +10 poise while guarding | `GetWornPoise` adds `guardPoise` while `State.Blocking` |
| Wisp crown talisman | your spells' burn buildup +5% | `ApplyHit` burn for `bSpell`/path hits |
| Cinderhorn talisman | sprint attacks +15% stance damage | `ApplyHit` stance when attacker sprinting |
| Troll marrow talisman | out of combat (8 s since hurt) heal 0.5 HP/s | `UWRStatusComponent` tick |
| Stalker fang talisman | blizzards don't cut your vision | weather vision |
| Old Tusker talisman | hauler tames +15% carry | creature carry |

The rest of the 40 are rows with stats named for their systems (several hit existing life-skill stats: woodYield, miningYield, rideSpeed, swimSpeed, blockStamina, hungerDrain, backstab) and hidden with their land. **Gear stats feed `UWRLifeSkillsComponent::StatFor`**, so any talisman or set stat named like a life-skill stat works wherever that stat is read.

Hazard charms: grounding charm (armorer 2), ward charm (alchemy 3), nullward (armorer 4) as talismans with `pass*` stats, hidden with their land.

### 2.7 Runes (R17)

8 runes, category `rune`, socket like gems (`gem` stats), alchemy table 3, resonant crystal 2 + the land's material 2 (all hidden until the Crystal caves are live, their only source of resonant crystal). A weapon can't take two of the same rune. Stats: `lifesteal` 0.03, `buildupDealt` 0.15, `stanceDealt` 0.10, `staminaOnHit` 3, `thorns` 0.10, `killResource` 5, `critDamage` 0.10, `armorPen` 0.10. All hooked in `ApplyHit` (thorns as a reaction hit so it never loops).

### 2.8 Salvage and removing extras (R18, R19)

Crafting tab → **Salvage** category: one card per piece of gear in the bag (not worn: take it off first; the card says so) at its station (weapons and rings at the forge, armour and talismans at the armorer, tools and bows at the workbench). Returns: for each recipe input that is a metal (ingot / plate) or a main input (the biggest count besides the path essence): floor(n × (0.4 + 0.1 × quality step)) at least 1; plus floor(upgrade shards spent × 0.25) (shards spent = sum 1..Upgrade). Destroyed: the element (essence) and every socket — the card lists them in crimson. Command `salvage <bag slot>`; server re-checks station type, Hearth not broken, owner. +5 smithing / armorcraft XP. A boss core in the bag salvages at the forge (Hearth 5) into 10 core shards.

**Remove an extra**: Repair category → "Remove: Ruby" cards per socket of the held weapon at the forge; destroys it, Careful hands (stat `extraSave` 0.5) gives it back half the time. Command `unsocket <index>`.

### 2.9 Blueprints (R20-R22)

`gear.json blueprints`: id, item, recipe(s), tier, boss. Blueprint item category `blueprint`; RMB → `FWRBlueprints::Learn`: adds to `AWRPlayerState::KnownBlueprints` (replicated owner-only, saved with the player), consumes the item, plays `learn` sound, "You learned Cinderbrand". Known already → "You already know this — give it to a clan mate". Recipes with `"blueprint": "<id>"` are hidden (UI and server) until known. Crafting tab: **Blueprints** category lists the known ones. Drops in `AWRBoss::OnDeath` per looter: first kill (BossKills before FirstKillDrops adds it) 100%, else `gear.blueprints.repeatChance` 0.2; one of the boss's blueprints the player doesn't know yet (else a random one: tradeable). Portal odds rows `[0.1, 0.2, 0.3, 0.45, 0.6]` and `FWRBlueprints::PortalDrop(rank)` for the portals lane.

### 2.10 Edge cases

- Equipping a ring when both slots hold rings swaps Ring 1. Two of the same ring: second one counts zero, the card says "doesn't stack with your other".
- A broken piece (0 durability) counts for nothing: not for armour, poise, nor its set.
- Unequip into a full bag: refused ("inventory full"), as today.
- Death penalty / loot bags (WRSocial 356) iterate every equip slot: all 10.
- Salvage of a worn item: not offered (the server refuses bag-less instances).
- Salvage needs the Hearth not broken (`CanUse`).
- Blueprint learning on a client: server only; KnownBlueprints replicate so the tab updates.
- Starsteel requires a Hearth of 5 (6 different major bosses): today's island has 2 bosses, so starsteel is shown with "Needs tier 5" (the honest gate, CF says so).

### 2.11 Build steps

1. Data: items, recipes, gear.json, creatures loot, weather counters, armorLooks, lifeskills (set_crafter, careful_hands live), pieces (kiln), tuning (caves.starIronMinZ, xp keys), sounds rows.
2. Inventory: slots, stats, set counting, GearStat, StatFor hook, rings, wear, save + migration.
3. Hero look per slot.
4. Crafting: salvage, unsocket, blueprints, core shards, kiln station; boss drops.
5. Combat / status / hazards / weather hooks.
6. UI: equipment panel (4 armour + talisman + 2 rings), item card set lines, Crafting tab Salvage / Remove / Blueprints, old text menu.
7. Star iron vein + look; kiln look; held starsteel colour.
8. Icons (`Tools/art/make_gear_icons.py`).
9. CHANGELOG, HANDOFF, compile, spot-check.

---

## 3. BUILD STATUS (spot-check against the ledger, 2026-11-14)

| # | Status | Where |
|---|---|---|
| R1 | Done | `Data/recipes.json` steel_ingot, steel_plate (forge 4) |
| R2 | Done | `Data/pieces.json` charcoal_kiln (Hearth 2); `WRBuildLook.cpp:639`; `WRBuilding.cpp:602`; recipe charcoal |
| R3 | Done | `Data/items.json` steel_* (6 classes); `WRHeldTool.cpp` metal colours |
| R4 | Done | items `<set>_head/_armor/_hands/_legs` (equip.set, equip.look), recipes armorer 4 |
| R5 | Done | recipe starsteel_ingot (forge 5) |
| R6 | Done | `WRGear.cpp:240` AWRStation::Salvage (core branch) |
| R7 | Done | `WRWorldDressing.cpp:1269`; `WRResourceLook.cpp:70, 209`; tuning caves.starIronMinZ |
| R8 | Done | items starsteel_*, `<set>_ascendant_*`; `WRHeroLook.cpp:76` AscendantMask |
| R9 | Done (existing rule) | `FWRCraftingRules::UpgradeCap` = Hearth x 2, capped 10 |
| R10 | Done | `WRInventory.h` EWREquipSlot + WRGearSlots; `WRInventory.cpp` GetEquippedMutable / GetEquipped |
| R11 | Done | `WRInventory.cpp:1343` migration (gearSlots 2) |
| R12 | Done | `WRHeroLook.cpp:76` SetWornArmor, `:180` ApplyOutfit per slot |
| R13 | Done | `WRInventory.cpp:565` SetPieces (Set crafter), `:584` GearStat; `Data/gear.json sets` |
| R14 | Done | `WRStatusComponent.cpp:273` buildupTaken; `WRWorldDressing.cpp:196` passLava; `WRCombatLibrary.cpp:118` heatGain |
| R15 | Done | buildupTaken.frost; `WRSurvival.cpp:946` weather sets; passIce; chillGain |
| R16 | Done (hidden by land) | gear.json + items `biome` + recipes `biome` |
| R17 | Done (hidden until Crystal caves) | items rune_*, `WRInventory.cpp:939` one-of-each; `WRCombatLibrary.cpp:651, 728, 791, 805, 898, 966, 1062` |
| R18 | Done | `WRGear.cpp:187` SalvageReturns, `:240` Salvage; `WRUICrafting.cpp` Salvage category, crimson Destroyed list |
| R19 | Done | `WRGear.cpp:313` RemoveExtra; lifeskills careful_hands stat extraSave |
| R20 | Done | `WRGear.cpp:365` Learn; `WRPlayerState.h` KnownBlueprints (saved `WRGameMode.cpp`); `WRCrafting.cpp` IsOpenTo; Blueprints category |
| R21 | Done (boss) / ready (portals) | `WRBoss.cpp:1200`; `WRGear.cpp:398` BossDrop, `:417` PortalDrop |
| R22 | Boss weapons done; decor packs to LATER-COOKS | items cinderbrand / rimefang (innate), bp_* |
| R23 | Done | items ring_*; `WRInventory.cpp:553` RingStat; unique rule in GearStat |
| R24 | Done (5 live; Old Tusker waits: recipe waitsFor) | items talisman_*; `WRCombatLibrary.cpp:839` guardPoise, `:898` sprintStance, `:982` spellBurn; `WRStatusComponent.cpp:720` regen; `WRSky.cpp:383` blizzardSight |
| R25 | Done (hidden by land) | grounding_charm, ward_charm, nullward; worn counts for weather (`WRSurvival.cpp`) |
| R26 | Done | tuning lifeSkills.xp salvage 5, steel 5; recipes "xp": "steel" |
| R27 | Done | SWRSlot equip places and list cards (WRPadInput drives them); all actions are server commands |
| R28 | Done | `Data/gear.json`, tuning `gear`, sounds.json salvage / socket.remove / blueprint.learn |
| R29 | Done | land gating, `waitsFor`, portal odds dormant |
| R30 | Done | this doc: research, plan, build, spot-check |

Spot-check fixes made before push: hazard items now count when worn (a grounding charm in the talisman slot); the Hearth
screen shows a Hearth's own level past the world's bosses (test worlds); the Old Tusker talisman hidden (no carry
system); `gear.outOfCombatSeconds` in tuning.
