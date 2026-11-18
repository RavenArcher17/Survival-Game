# wild-answers-1115: building Aaron's answers to the 20 Wildreach question cards

Lane wild-answers-1115 (desktop, 2026-11-16). Source of the answers: `Saved/Handoff/wild-desktop/answers-wildreach-questions-1115.json`.
Source of the questions: the live page `review/wildreach-questions-1115` (read with the admin token, cards copied in
section 0 so nobody has to guess what a card said again).

## 0. The 20 cards (what each asked, what Aaron answered)

| Card | What it asked (built / the other choice) | Aaron | What this lane does |
|---|---|---|---|
| r-forest-ground | Forest floor is a runtime ground mesh from a 2 MB height map / import a real landscape with an auto-material (better ground texture, +64 MB) | SWITCH: "We already have the electric dreams env pack. Can't we use that to create an incredible forest biome? Or take some lessons we learned from Trails?" | A real textured auto-material on the forest ground (Trails' CC0 forest scans + the MW pack), layer weights per sub-zone and surface, height-blended, slope rock; contact skirts at trunks and boulders (Electric Dreams' contact decals). See 2.5 for why not a 64 MB landscape |
| r-flint | Hush arrows ask flint x2 but no flint exists, so they cost stone / a flint node on the cape's shingle | SWITCH: "Add flint as a material that can be mined and collected, occasionally getting a stack along with the stone you mine." | Flint item; every stone you mine has a chance to turn up a stack of flint with it; Hush arrows take flint; flint arrows |
| r-moss | Cloudback eats levistone (moss doesn't exist) / a moss node on the keel outcrops | SWITCH: "Yes add the moss to collect. Make it look good." | Levistone moss: modelled moss beds hanging on every keel outcrop, gathered by hand, the Cloudback's best food |
| r-talisman | Sky trophy talismans do the sky spec's single effects / keep both effects | KEEP: "Choose whats best for this game" | Keep single effects (50 of 56 talismans have one effect; readable). Proven wired |
| r-kyrra | Her Tempest stoop is the built dive (grounded 1.6 s + 4 m ring) / its own straight-line attack | KEEP: "Try it, make it good" | The stoop made into a real stoop: climb to a fold point during the wind-up, wings folded on the dive, a whistle and wind streak, the ground shadow, then mantled on the ground in her bring-down window |
| r-lens | Sunwatch lens works from the pack / make it a light-slot item | KEEP: "Figure it out, best user experience is priority 1" | Keep it in the pack (no slot juggling) and make the compass say what is going on in a storm: "no sun to read" + the lens hint when hidden, a lens tag when the lens is what shows it |
| r-dunewrap | Dunewrap recipe numbers: crafting lane's / the desert spec's 6/8/12/6 | KEEP: "Idk you aren't giving me any context or even full sentences" | Nothing to build; the card rule (section 4) so every future card has full context in full sentences |
| r-mirage | Matriarch mirages are real 1-health creatures / pure images | KEEP | nothing |
| r-recoil | Wall recoil on chained lights / off | KEEP: "Try it." | nothing; proven present |
| r-hurtedge | Hurt edge follows Camera shake / own toggle | KEEP | nothing |
| r-bossdmg | Boss damage total always shows / only with damage numbers | KEEP | nothing |
| r-eat | Committed eating (1.5 s) / instant from the bag | KEEP | nothing |
| r-lefthand | Food in the LEFT hand, shield/torch hidden / sheathe the weapon and eat with the right (Monster Hunter) | SWITCH: "Food should be on the item quick use or other toolbar. Not equipped in one of the hands to eat." | Eat with the right hand, the weapon put away for the bite and back after; the off hand keeps its shield or torch. Food and drink live on the quick-use bar: auto-placed, used with one press from the bar / the pad's belt button / the item wheel, never equipped |
| r-waterskin | Workbench makes an EMPTY waterskin / craft it full | KEEP | nothing |
| r-wet | Wet status built now / leave it for a later status pass | SWITCH: "We will do more design passes to make it really good" | Wet is switched off (one data switch, `wet.enabled` 0): no status, no multipliers, no HUD line. The code waits for the design passes |
| r-packup | Pack up a placed siege engine = Dodge twice / another button or a hold | SWITCH: "Figure it out and make sure it's the best cook it can be with research and planning" | Hold Interact 1.5 s at your engine (on a post or beside it), a ring filling round the key, cancelled by letting go, moving off, a hit on the engine or on you; reasons shown before you try |
| r-siege-pve | No ram or trebuchet on PvE worlds; the ballista can't hurt players / allow siege engines in PvE | SWITCH: "Allow it to practice siegeing on NPC strongholds" | Siege engines on every world. On PvE worlds they still can't hurt players or players' buildings, but they can break NPC strongholds: new forts on the island, held and defended, that fall, pay out and rebuild |
| r-colin | Both Colin accounts on the god-mode list / only one | KEEP + "Make it a command we need to enter in-game 'god-mode' in the existing command line." | God mode is switched on by typing `god-mode` in the chat line (the game's only command line, where /w /r /c already work). Off again the same way. Nothing for anyone else |
| r-pubserver | God mode refused on public dedicated servers / self-only powers there | KEEP + "We need Mod Mode still available for us in public servers." | On public servers the testers' god mode works too (after `god-mode`), and a tester in god mode can moderate any world (kick, ban, admin) like its host |
| r-oldcmds | Old test commands need a verified tester / leave open | KEEP | nothing (they now also need god-mode entered) |

## 1. REQUIREMENT LEDGER

| # | Requirement (one each) | Source |
|---|---|---|
| R1 | Read every card's real question; match ids to questions | brief |
| R2 | Full cook: research doc with named games/systems, plan before building | brief (EVERY LANE IS A FULL COOK) |
| R3 | God mode is opened by typing `god-mode` in the existing in-game command line | r-colin |
| R4 | Still only the three signed-in devs (server-verified), nothing for anyone else | r-colin, brief |
| R5 | God/mod mode works on public servers for the devs | r-pubserver |
| R6 | "Mod Mode": the devs can moderate public servers (kick/ban/admin) | r-pubserver ("Mod Mode") |
| R7 | Flint item that can be mined and collected | r-flint |
| R8 | Occasionally a stack of flint along with the stone you mine | r-flint |
| R9 | Flint has a use (Hush arrows ask for it) | r-flint card |
| R10 | Forest rebuilt with the Electric Dreams pack if present, else its lessons + Trails lessons | r-forest-ground |
| R11 | An incredible forest ground: real texture, not flat colour | r-forest-ground |
| R12 | Food on the quick-use bar / toolbar | r-lefthand |
| R13 | Food not equipped in a hand to eat (switch: eat with the right, weapon put away) | r-lefthand |
| R14 | Moss to collect (on the keel outcrops) | r-moss |
| R15 | Moss looks good | r-moss |
| R16 | Pack-up: research and plan the best input; build it | r-packup |
| R17 | Siege engines allowed on PvE worlds to practise on NPC strongholds | r-siege-pve |
| R18 | NPC strongholds exist to practise on (designed with intention: why, where, what it touches) | r-siege-pve, DESIGN WITH INTENTION |
| R19 | Wet: build the other choice (left for later passes) to a high standard | r-wet |
| R20 | Kyrra's stoop: keep, make it good | r-kyrra |
| R21 | Sunwatch lens: keep, best UX | r-lens |
| R22 | Wall recoil: keep ("try it"), genuinely good | r-recoil |
| R23 | Talismans: choose what's best | r-talisman |
| R24 | Every future question card has full context in full sentences | r-dunewrap |
| R25 | Wildreach is not a music game: remove any visible music UI / music mechanic in scope | lane note |
| R26 | Data-driven (tuning/items/recipes/sounds/fx rows), server authority, mouse AND pad | THE BAR |
| R27 | Compile clean (no new warnings); no tests run | GATES |
| R28 | CHANGELOG Unreleased + HANDOFF in the same commits; land in pieces | GIT |
| R29 | LANDED line on the share; later-cook lines; questions with full context | brief / overrides |
| R30 | Binary sizes reported | brief |

## 2. Research (what the best games do, and what we take)

### 2.1 Typing a cheat word into the chat (R3-R6)
- **Minecraft**: commands are typed into the chat line with `/`; `/gamemode creative` only works for operators, and the
  server says "Unknown or incomplete command" to everyone else: the command doesn't exist for them.
- **Valheim**: the F5 console needs `devcommands` typed first (a gate word), and on a dedicated server only admins on
  `adminlist.txt` may (server-checked). Typing it again turns it off.
- **Rust**: admin/moderator powers (`ownerid` / `moderatorid`) carry to every server the admin owns; moderation (kick,
  ban) is separate from god-like powers (`noclip`, `god`).
- **ARK**: `enablecheats <password>` once, then `admincheat ...`; others typing it get nothing.
- **Take**: a gate word typed in the line players already use (our chat box), checked by the server against the proven
  tester flag; a non-tester's `god-mode` is just a chat line like any other (nothing reveals the feature); typing it
  again turns it all off. A tester in god mode also has the host's moderation commands on any world (Rust's moderator
  idea), which is what "Mod Mode" asks for on public servers.

### 2.2 Flint with stone (R7-R9)
- **Minecraft**: gravel drops flint 10% of the time instead of gravel: a by-product you notice and stockpile.
- **Rust**: mining a stone node sometimes gives a bonus of the rarer ore on the last hit.
- **The Forest / Green Hell**: knapping flint-like stones gives sharp edges for arrows and blades.
- Real world: flint is nodules of chert inside chalk and limestone: grey-black glassy cores in a chalky white rind,
  broken by conchoidal (shell-like) fractures; used for arrowheads, blades and striking sparks off iron.
- **Take**: each stone-giving hit on a rock has a small chance (`gather.flint.chance` 0.15) of turning up a stack
  (`min`-`max` 1-3) of flint as well, with its own pickup line and a sharper click; ice-glazed boulders never (ice);
  resource nodes of stone the same. Uses: Hush arrows (the sky spec's flint x2) and new flint arrows (hand recipe: a
  sharper head than stone).

### 2.3 Moss on the keels (R14-R15)
- **Breath of the Wild**: materials are gathered in one press, with a small sparkle so you see where they are.
- **Subnautica**: hanging and glowing flora on cave ceilings, a cyan bioluminescent edge that tells you what is
  harvestable in the dark.
- **Valheim**: the Moder's / swamp moss readable as a colour against the rock.
- Real world: Spanish moss and old man's beard (Usnea) hang in pale grey-green strands from branches; cushion moss
  grows in rounded mats with a velvet nap; in damp shade mats creep over rock lips and drip.
- **Take**: under every keel outcrop's node, a bed of levistone moss: cushion mats on the rock's flank and lip, beard
  strands hanging 0.6-1.6 m, their tips glowing faint cyan at night (the levistone feeding it). Gathered by hand (E),
  2 per press, a bed gives 3 presses and grows back in a game day. It is the Cloudback's favourite food (taming food
  value x1.5 over levistone).

### 2.4 Eating with the right hand, food on the bar (R12-R13)
- **Monster Hunter**: the weapon is sheathed, the potion or meat comes out in the right hand; using an item is a
  committed animation from the item bar (radial or the item bar under the HUD).
- **Elden Ring**: flasks on the quick item slots (D-pad down / the belt), no equip into a hand.
- **Valheim**: food sits on the hotbar 1-8 and is eaten with its number.
- **Take**: the right hand eats: the weapon (or held tool) is put away for the use and comes back after; the left keeps
  its shield or torch (no more hiding them). Food and drink are on the quick-use bar automatically when picked up
  (the existing auto-fill), and a press of the slot's key, the pad's belt button or the item wheel eats. Nothing is
  ever equipped to eat.

### 2.5 The forest ground (R10-R11)
- The Electric Dreams pack is **not on this PC** (searched Content, the launcher's VaultCache and every drive; the
  sample was studied and removed on the laptop, `ELECTRIC-DREAMS-NOTES.md`). Its lessons are used.
- **Electric Dreams** (notes 1, 3): ground props sit on footprints that can hold them; contacts hidden by dirt / moss
  decals; the ground is layered assemblies (litter, roots, rocks), not one flat fill; no landscape grass.
- **Trails of Appalachia** (its terrain sets, CC0 ambientCG scans built by `terrain_surfaces.py`): leaf litter
  (Ground053), forest soil, moss ground, each colour + height (BCH) + normal, tiled at 2.5 m.
- **RDR2 / Horizon**: forest floors read real because of three things: a height-blended mix (moss creeping into the
  litter's hollows, not an alpha fade), large-scale variation breaking the tiling, and wetness in low ground.
- **Why not the card's 64 MB landscape**: the forest is a runtime land with the Hollow Root cut through its ground
  (holes), underground ways and the island's own landscape beside it. A landscape import costs 64 MB of LFS on
  Raven's account and can't cut the Hollow Root's openings the way the runtime mesh does. The visible difference the card
  promised (a better ground texture) is the material, and that is built here on the runtime ground.
- **Take**: `M_ForestGround`: six layers (leaf litter, forest soil, moss; the MW pack's grass, stones and rock),
  weights per vertex from the land's zone and surface rows (data in `forest.mjs` → `forest.json`, UV channels 1-3),
  height-blended with each set's height, a 31 m macro variation, rock by slope, darker wet soil in low ground near
  water, the vertex colour kept as a gentle tint so each sub-zone keeps its palette. Plus contact skirts: a ring of
  litter and moss at the foot of every forest boulder and big trunk (Electric Dreams' contact decals).

### 2.6 Pack up (R16)
- **Rust**: pick up a deployable from the hammer's / hold-E radial, only when undamaged and you have building
  privilege.
- **ARK / Conan Exiles**: hold E for the radial, "Pick up"; a held prompt never fires by accident.
- **Valheim**: the hammer's middle click deconstructs (full refund).
- **Fortnite / Palworld**: a hold with a filling ring for anything destructive.
- Our own game already uses tap/hold on Interact (tame orders, a door's keypad): a hold is learned once.
- **Take**: hold Interact (E / pad X) 1.5 s at your engine, on a post or beside it; a ring fills round the key cap on
  the prompt; the server times it too (the start is recorded, the finish refused if early). Letting go, stepping away,
  any hit on you or the engine cancels. The prompt says why not before you hold ("Too damaged to pack up: 62%, needs
  90%", "Someone is on it", "Not yours"). Tap keeps its job (take a post / step off). Dodge no longer packs.

### 2.7 NPC strongholds to siege (R17-R18)
- **Mount & Blade**: castles with walls, a gate, towers; you build a ram or ladders and the garrison shoots back. Siege
  practice works because the walls *defend*: archers make you use the engines' range and the ram's roof.
- **Valheim**: Fuling villages have real wooden walls that break, a guarded centre, loot inside.
- **Kingdom Come: Deliverance 2**: trebuchets on a castle (Suchdol), the wall breaks in sections.
- **Conan Exiles**: NPC camps with chests that refill.
- **Take**: two strongholds on the Old Island, one per land, each a real fort built from our own build pieces (so every
  raid rule, siege hit, ram and charge works on it unchanged): a palisade ring with a gate (ram target), two guard
  towers, a stone keep (trebuchet target) with a reinforced door, bolt turrets on the towers that shoot at besiegers
  within 30 m (the reason to stand off with a trebuchet or put the ram's roof between you), defenders of the land
  inside, and a war chest in the keep that opens once the keep's door falls. Anyone may break it on any world. When it
  falls and nobody is within 150 m for `rebuildHours`, it rebuilds whole with fresh defenders and a refilled chest.
  - **Emberwatch** (volcanic): the Forge-wardens' old watch over the lava fields, taken by the cinder beasts; on a
    basalt shelf with a clear approach for engines (the trebuchet's 40 m pin from the shelf's edge).
  - **Rimehold** (tundra): a whalers' winter fort of stone on the ice shore, held by tundra beasts.
  Each is placed on flat ground the server checks (footprint test, Electric Dreams' lesson), facing its approach.

### 2.8 Kyrra's stoop (R20)
- **Real peregrine stoop**: climbs high above and behind the prey, folds the wings into a teardrop, drops at a steep
  angle at speed, strikes, then mantles over the kill with wings spread.
- **Monster Hunter's Rathian / Elden Ring's dragons**: a dive is read from the climb and the fold; the punish window is
  the landing, shown by the body sprawled.
- **Take**: during the 0.9 s wind-up she climbs to a fold point (up 9 m, back 6 m off the line) and her crest flares;
  on the drop her wings fold (the Stoop flag), a rising whistle and wind streak follow her down onto the shadow; on the
  ground the 4 m dust ring rolls out and she mantles (wings spread low, the Grounded flag) for the 1.6 s window in which
  she takes x1.25.

### 2.9 The Sunwatch lens (R21)
- **Breath of the Wild**: the Sheikah sensor and compass work from the slate (no slot); **Subnautica** shows "no
  signal" rather than hiding a tool silently. The best UX: never make the player juggle slots for a passive tool, and
  never hide something without saying why.
- **Take**: keep it working from the bag. In a storm the compass doesn't vanish: its ribbon is replaced by "Sandstorm:
  no sun to read" and, if you have no lens, "A Sunwatch lens in your bag shows the sun". With a lens (or a Scarab
  digger near) the ribbon shows with a small gold "Sunwatch lens" tag under it.

### 2.10 Wall recoil, talismans (R22-R23)
- Recoil: kept as built (Elden Ring / Dark Souls); proof `WRAbilities.cpp` (BladeSweep wall test) and tuning
  `combat.recoil`. Talismans: one clear effect each, as 50 of the 56 talismans have.

## 3. PLAN (every build step, in landing order)

**Part A, god-mode command and mod mode (R3-R6)**
1. `UWRGodComponent::bArmed` (replicated): god mode entered this connection. `Has()` / `AnyOn()` need it.
2. `AWRPlayerController::ServerChat`: a line that is exactly `god-mode` (or `/god-mode`) from a verified tester toggles
   `bArmed` (on: "God mode on: F9 / the pad's last journal page opens the panel"; off: every power off, AI unfrozen if
   they froze it, panel closed). From anyone else it is an ordinary chat line.
3. `WRGod::Block`: needs a tester **and** armed; the public-world refusal goes; on a public world the panel's banner
   says the world is marked tested by what you change there.
4. `ToggleGod` / the pad stop / the screen / the badge / the head tag: only while armed.
5. Mod mode: `admin kick|ban|unban|op` accept an armed tester like the host (server check in the admin command).
6. Old dev commands in a packaged build: tester and armed.

**Part B, wet off, flint, moss (R7-R9, R14-R15, R19)**
1. `wet.enabled` 0 in tuning; `IsWet` false and nothing set while off; the HUD line follows.
2. `flint` item (material, stack 100), icon; `gather.flint` tuning; `FWRGathering`'s stone hits roll it (rock
   dressing and stone resource nodes); its pickup line.
3. Hush arrows: flint 2 in place of stone 2; flint arrows (hand: wood 1, flint 1, fiber 1 → 3; damage between stone
   and copper); icon.
4. `levistone_moss` item + icon; moss beds modelled under every keel node (`WRSkySites` keel_outcrop / `WRSkyWorld`
   keel nodes): cushion mats, beard strands, night glow tips; a hand-gathered node (3 presses, 2 each, regrows in a
   game day); Cloudback food (first in its list, value x1.5).

**Part C, eating with the right hand, food on the bar (R12-R13)**
1. `FWRHeroAnimProxy` eat pose mirrored to the right arm; the food at `hand_r`.
2. The weapon / held tool put out of sight for the use, the shield and torch stay.
3. Food and drink always join the bar (auto-fill already includes food/drink: prove it); the hotbar slot, the pad's belt
   button and the item wheel all eat in one press; the inventory card says which key eats it.

**Part D, pack up and siege on PvE (R16-R18)**
1. Hold Interact at your engine: client hold (`siege.packHoldSeconds` 1.5) with a ring on the prompt's key cap;
   `siege packbegin` on the press, `siege pack` at the end; server refuses an early finish, a hit since begin, or a step
   out of reach. Tap keeps take / step off. Dodge on a post does nothing.
2. Siege engines on every world. `MayRaid`: stronghold pieces (owner `@hold:<id>`) may be raided on any world by
   anyone; players' pieces keep the PvP rule. Siege engines on PvE worlds can't be hurt by players.
3. `Data/strongholds.json` + `UWRStrongholdSubsystem` (`WRStronghold.cpp`): spawns each fort's pieces (palisade ring,
   gate, towers, keep foundation, walls, door, turrets, war chest) on checked ground at world start, the defenders (a
   leash to the fort), turrets always powered and loaded for the fort, the war chest locked until the keep door falls;
   state saved in the world store; rebuild after `rebuildHours` with nobody near; map landmark and a discovery line.
4. Turrets owned by a stronghold target survivors (any world) and their tames/engines.

**Part E, the forest ground (R10-R11)**
1. Import the three Trails CC0 sets (BCH + N, 1K) to `/Game/Terrain/Forest/`.
2. `Tools/terrain/build_forest_ground.py`: `M_ForestGround` as 2.5 says, and `M_ForestSkirt` (masked litter/moss ring).
3. `forest.mjs` zone / surface `layers` rows; regenerate `forest.json`.
4. `FWRLandDef`: per-vertex layer weights into UV1-UV3; `groundMaterial` per land; the Wet parameter kept.
5. Contact skirts at the foot of forest boulders and big trunks (dressing kinds), one instanced layer.

**Part F, Kyrra, the lens, the card rule (R20-R21, R24)**
1. Kyrra's climb to the fold point during the wind-up; Stoop flag + whistle and streak on the drop; Grounded flag for the window.
2. The compass's storm state and lens tag (`WRDesertHz::CompassState`).
3. The card rule in `Docs/HANDOFF.md` and at the top of the share's QUESTIONS-FOR-TOMORROW.md.

Edge cases decided: a tester who loses the connection is unarmed on rejoin (type it again); a stronghold half-broken
when the server restarts rebuilds whole (its state is "standing" or "fallen at"); engines left by a stronghold's ruins
are untouched; a moss bed under a drifting island is skipped like its keel node; flint never from ice rocks.

## 4. The question-card rule (R24)
Every question card says, in full sentences: what the thing is and where it is in the game, what is built now and how it
plays, what the other choice would change for the player, and why it was a question. No shorthand ("6/8/12/6"), no
internal names without saying what they are.

## 5. Proof (commits on Wildreach master: 654c783 (1), 857f3b1 (2), b99fa88 (3), 415bc08 (4), 9de926e (5), 2001108 (spot-check))

Plan changes made while building (each for a reason found in the code): the pack-up hold shows a brass bar filling under
the prompt (the prompt widget is text in a glass panel; a bar reads at a glance where a ring round the key cap would be
2 px wide at Desk size). The moss is a lump of levistone the moss has overgrown, floating beside each keel on its own
lift (levistone floats in this land), not a mat on the keel node: the node's look is a pack rock of unknown shape, and a
mat on it could float off it or sink into it; a self-contained float can't. Strongholds are held by the land's beasts
(cinderfang / magmaw, gloomhowl / rimeclaw): there are no humanoid NPC models outside the bosses.

| # | Done | Where |
|---|---|---|
| R1 | yes | section 0 (cards read from the live page with the admin token) |
| R2 | yes | this doc, sections 1-4, written before any code (commit order) |
| R3 | yes | `WRGod.cpp:628` ChatLine; `WRPlayerController.cpp:104` (and after the channel chip); tuning `god.gateWord` (654c783) |
| R4 | yes | ChatLine needs the server-verified `bTester` (`WRGod.cpp:628`); anyone else's line is ordinary chat; `Block` `WRGod.cpp:624` |
| R5 | yes | public-world refusal removed from `WRGod::Block` (`WRGod.cpp:620-626`); banner `WRUIGod.cpp` BannerText (654c783) |
| R6 | yes | `WRAccess.cpp:247` (a tester in god mode moderates as host: kick, ban, admins) (654c783) |
| R7 | yes | `items.json` flint; icon `Data/UI/Icons/flint.png` (654c783) |
| R8 | yes | `WRResourceNode.cpp:185` (15% a stone hit, 1-3, x1.5 heavy, x1.4 pick, never ice) (654c783) |
| R9 | yes | `recipes.json:447` Hush arrows flint 2; `recipes.json:10` flint arrows (654c783) |
| R10 | yes | Pack not on this PC (searched Content, VaultCache, every drive); Electric Dreams' lessons (height-blend layers, clustering, contact hiding) + Trails' CC0 forest scans used (415bc08) |
| R11 | yes | `M_ForestGround` (`Tools/terrain/build_forest_ground.py`), `WRLand.cpp:829` HfLayers, `Data/lands/forest.json` layers (415bc08) |
| R12 | yes | auto-fill puts food/drink on the bar `WRPlayerUI.cpp:588`; the item card's "Quick use" row `WRUIInventory.cpp:1786` (857f3b1) |
| R13 | yes | right-hand eat pose `WRHeroAnimInstance.cpp:499`, food at `hand_r` `WRHeldFood.cpp:465`, the weapon put away for the use (857f3b1) |
| R14 | yes | moss spots beside every keel `WRSkyWorld.cpp:1356`; Cloudback food `creatures.json` foodValue (654c783) |
| R15 | yes | `WRSkyMoss.cpp:85` (cushions by golden angle, skirt, 22 beards, crystal keel, night glow, bob and sway) (654c783) |
| R16 | yes | research 2.6; `WRPlayerUI.cpp:1178` SiegeKey; server-timed `WRSiege.cpp:519`; prompt reasons (b99fa88) |
| R17 | yes | `WRSiege.cpp:1292` (every world); stones skip players on PvE `WRSiege.cpp:1456`; `WRRaiding.cpp:319` (b99fa88) |
| R18 | yes | `WRStronghold.cpp:240` Build, `Data/strongholds.json` (Emberwatch, Rimehold), turrets `WRTurret.cpp:100`, save `WRGameMode.cpp:376` (b99fa88) |
| R19 | yes | `tuning wet.enabled` 0, `WRSurvival.cpp:1133` and IsWet (654c783) |
| R20 | yes | `WRSkyBosses.cpp:944` TickWindup (climb, fold, lock, scream), mantle on landing (9de926e, 2001108) |
| R21 | yes | `WRDesertHazards.cpp:227` CompassState; HUD storm text and lens tag (`WRUIHud.cpp` CompassAndBoss) (9de926e) |
| R22 | yes | kept on: `WRAbilities.cpp:1514` (switch `combat.recoil.enabled`) |
| R23 | yes | kept single effects; wired `WRCombatLibrary.cpp:813` vsAirborne, `:1028` stillPoise, `WRSkyAir.cpp:109` glideStamina |
| R24 | yes | section 4 here; `Docs/HANDOFF.md` (Aaron's answers paragraph); top of the share's QUESTIONS-FOR-TOMORROW.md |
| R25 | yes | nothing in this lane shows music or asks you to play to it (checked each change) |
| R26 | yes | every number in `tuning.json` (`god`, `gather.flint`, `skyWorld.moss`, `siege._pack`, `strongholds`, `wet`, `bosses.kyrra`), items / recipes / creatures / strongholds data; server decides (chat gate, pack timing, forts, flint); E and pad X for the hold |
| R27 | yes | editor target compiled after every part, no errors or new warnings |
| R28 | yes | CHANGELOG "Unreleased: Aaron's answers" + HANDOFF in each commit; 6 pushes |
| R29 | yes | LANDED.md, LATER-COOKS.md, QUESTIONS-FOR-TOMORROW.md on the share |
| R30 | yes | new binaries: 3 icons 32 KB; 6 textures + M_ForestGround 19.7 MB (LFS) |
