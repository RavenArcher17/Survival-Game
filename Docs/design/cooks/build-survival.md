# Wildreach build lane: SURVIVAL (FULL-GAME 6.1)

Lane `build-survival`, 2026-11-14. Scope: FULL-GAME 6.1: Rested from sleeping, weather that matters (storms, blizzards,
heat waves) under Raven's no-temperature rule, darkness and caves needing light, corruption in dark biomes with
path-matching buffs. The design cook for this already exists (`cooks/survival-crafting.md` 2.2 to 2.6, plan 3.1 to 3.2).
This doc follows it, adds the research for the details it left open, solves the gaps it left for the build, and
records what was built where.

---

## 0. REQUIREMENT LEDGER

Split from the brief, FULL-GAME 6.1 and survival-crafting 2.2 to 2.6. "Proof" is filled in at the end (section 5).

| # | Requirement | Source |
|---|---|---|
| R1 | Rested buff from sleeping | FULL-GAME 6.1 (DD Survival) |
| R2 | "Rest" = finishing a sleep in a bed or bedroll you or your clan own; one Rest event other systems hang off (Void max HP, Deathless re-arm, Decay, corruption) | FULL-GAME 6.1, DR #13 default, SC 2.2 |
| R3 | Sleep is a 10 s commitment: lie down, the screen dims, cancelled by damage, hunger/thirst at 0, moving; time never skips (multiplayer) | SC 2.2 |
| R4 | Rested numbers: stamina regen +20%, hunger and thirst drain x0.8, life-skill XP +10%; bedroll 10 min, bed 20 min, +1 min per comfort (max 20) | SC 2.2 |
| R5 | Comfort: decor within 12 m under a roof, highest per group, cap 20; comfort numbers shown on decor in the build bar; HUD shows comfort near your bed | SC 2.2 |
| R6 | HUD: Rested icon under the vitals with minutes left, pulses at 1 min | SC 2.2 |
| R7 | Weather that matters, rolled per biome every 30 min, all 12 biome rows as data | FULL-GAME 6.1, SC 2.4 |
| R8 | Storms, blizzards and heat waves specifically (Thunderstorm with strikes, Blizzard, Heat wave), plus Ashfall (Fire's biome) | Brief, SC 2.4 |
| R9 | Raven's rule: NO temperature meter; every weather effect is a gear check or a visibility change with a clear counter | Brief, DD Ruled out, SC 2.4 |
| R10 | Weather signs 60 s ahead (sky / horizon, rising ambient bed) | SC 2.4 |
| R11 | Weather drawn and heard in the biome's own look; music never stops for weather | SC 2.4 |
| R12 | Darkness is dangerous: unlit players are hunted (dark-hunters x2 aggro range) | FULL-GAME 6.1, SC 2.3 |
| R13 | Caves need light: the cave interior is unlit unless a light reaches you | FULL-GAME 6.1, SC 2.3 |
| R14 | Lights: torch (held, 6 m, 20 min, off-hand so no shield, out underwater, rain halves its burn), lantern (9 m, fuel, hands free on the belt), placed torch / brazier, Hearth, light tames | SC 2.3 |
| R15 | Corruption in dark biomes: a 0-100 meter (Graveyard, Corrupted wasteland), build/fall rules, stops by fires, halved by a lantern, halved by Rest | FULL-GAME 6.1, SC 2.5 |
| R16 | Corruption steps: 25 heal -10%, 50 stamina regen -15% and treated as unlit, 75 max stamina -20, 100 Marked (biome creatures x2 aggro, path resources fill 25% slower) | SC 2.5 |
| R17 | Path-matching buffs: Necromancer in the Graveyard, Void walker in the Wasteland: no penalties, path damage +3/6/10/15% at 25/50/75/100 | FULL-GAME 6.1, SC 2.5 |
| R18 | Corruption is labelled "Corruption", never confused with the Void path's resource; ring shown only above 0 | SC 2.5 |
| R19 | No floors: weather strikes, corruption, darkness and deprivation can kill | CAL T10, SC 2.6 |
| R20 | Data-driven: numbers in Data/*.json and tuning; sounds and fx as table rows | Brief (THE BAR) |
| R21 | Multiplayer: server authority, replication to every machine, save/load of the meters | Brief |
| R22 | Mouse and pad on every screen touched (new Light slot in the bag) | Brief (THE BAR) |
| R23 | Nothing unfinished shown as finished; unfinished stays hidden | Brief |
| R24 | Design with intention: every object's job, connections and contacts (the torch in the hand, the lantern on the hip, the sleeper on the bed, lights against corruption) | Brief (11-14) |
| R25 | Full cook: research, plan before build, spot-check against the plan | Brief (11-13) |
| R26 | Git rules, CHANGELOG Unreleased + HANDOFF in the same commit, no release, LFS sizes reported, LANDED line | Brief |

---

## 1. RESEARCH

### 1.1 Rest and sleeping

| Game | What it does, specifically | What we take |
|---|---|---|
| **Valheim** (Rested, Comfort) | Rested comes from sitting by a fire under a roof; base 8 min, +1 min per comfort level; comfort counts only the best piece in each furniture group (one chair counts, ten chairs don't), within ~10 m; Rested gives +stamina regen and +skill XP; "Sheltered" needs a roof straight above you. Sleeping in a bed at night needs every player asleep to skip the night, and you can't sleep with enemies close ("You can't sleep now, enemies are nearby") | Comfort by **group, best piece only** (variety over stacking). The **roof** check (a trace up hits a build piece). The **enemies nearby** refusal. No night skip (our multiplayer rule) |
| **Enshrouded** | Comfort from furniture raises Rested length up to a cap; Rested shows as a buff with time | A visible minute count on the HUD |
| **Don't Starve Together** | Sleeping in a tent restores health but drains hunger; you can't sleep with a monster nearby or when starving | Refuse when hunger or thirst is empty ("Too hungry to sleep") |
| **Breath of the Wild** | Sleeping at an inn restores and gives bonus hearts; it is a short dim-to-black ritual | A short, dimmed sleep (10 s, dim to 30%) rather than a long blackout |
| **Real world** | Sleep debt is restored by a full cycle; interrupted sleep gives little benefit | Any interruption gives no Rest at all |

### 1.2 Weather that matters, with no temperature meter

| Game | What it does | What we take |
|---|---|---|
| **Breath of the Wild** | Thunderstorms: metal gear draws lightning; the game warns with crackling on the metal and a light flicker about 1 s before the strike lands; rain makes cliffs slippery; the weather forecast on the Sheikah slate. Cold/heat use a temperature gauge (we don't) | The **strike tell** (a ground glow 1 s before), metal-on-high-ground logic, the **forecast** idea: a warning toast 60 s ahead |
| **Monster Hunter Wilds** | The "Inclemency" phase: sandstorms/thunderstorms roll in with a visible wall on the horizon and an audio swell before they arrive; visibility drops; specific monsters thrive in it | The **60 s approach**: fog creeping in from the far distance and the bed rising before the effect starts; the weather has one clear mechanical identity per biome |
| **Valheim** | Mountain cold and Deep North freezing debuffs are gear checks (frost resistance mead, wolf armour) rather than a meter; rain gives "Wet" which halves comfort/regen until you dry by a fire or under a roof; storms reduce visibility | Every effect a **gear check with a named counter**; **roof** as a universal counter; no meter |
| **Subnautica / The Long Dark** | The Long Dark uses temperature (ruled out for us) but its **blizzard** is the model for "visibility collapses, the wind pushes, find shelter": horizontal snow, white-out fog at 30-40 m, howling wind bed | Blizzard look: sideways snow, white fog at ~40 m, howl bed; we keep only the visibility and stamina parts |
| **Real world** | Volcanic ashfall: grey-out of the sky, gritty fall, people wear cloth masks; ash dries the throat. Heat waves: dehydration in direct sun, shade fixes it. Lightning: strikes the highest conductor; ground glow and hair standing are real precursors | Ashfall = vision + thirst outdoors, counter shelter / face cover; heat wave = thirst x1.5 in **direct sun** only (sun trace); strikes pick the **highest player** |

### 1.3 Darkness and light

| Game | What it does | What we take |
|---|---|---|
| **Minecraft / Terraria** | Darkness spawns danger; torches you place are the cave route's breadcrumbs | Placed torches already exist (pieces.json); they light you |
| **Valheim** | The torch is held in the off hand in place of the shield; it burns out (durability) and is cheap to make; it's dark in caves and at night | Torch in the **off hand, no shield**, burns down, hand-crafted from wood + fiber |
| **Enshrouded / Don't Starve** | A lantern on the belt keeps both hands free and burns fuel; DST's darkness ("Charlie") kills you if you stay unlit | **Lantern on the hip**, fuelled; **unlit = hunted** rather than instant death |
| **Real world** | Horn lanterns ("lanthorns"): thin scraped horn panes in a metal frame before glass was cheap; tallow (rendered animal fat) candles and lamps | **Horn lantern** (bronze frame + horn panes from hide), burns **tallow** rendered from raw meat |

### 1.4 Corruption

| Game | What it does | What we take |
|---|---|---|
| **Darkest Dungeon** | Light meter: low light raises stress and enemy strength but raises loot | Light pushes corruption back (fires stop it, a lantern halves it) |
| **Elden Ring** (Madness / Frenzied Flame) | Buildup meter with a visible ring; leaving the source drains it | A **ring** that builds and falls, shown only above 0 |
| **Bloodborne** (Insight / Frenzy) | Some builds gain from the thing that harms others | **Path match**: Necromancy in the Graveyard, Void in the Wasteland gain damage instead of penalties |

### 1.5 Lessons (short)
1. Every weather has **one identity** and **one named counter** (Valheim, MH Wilds). No meter (Raven).
2. Warn before it bites: 60 s approach, 1 s strike glow (BotW, MH Wilds).
3. Best-piece-per-group comfort, roof checks, enemies-nearby refusal (Valheim).
4. Light is a resource you carry and place; darkness is a threat, not a screen filter (DST, Valheim).
5. Meters build and fall visibly; the matching build gains (Elden Ring, Bloodborne).

---

## 2. PLAN (design gaps solved, then the build steps)

### 2.1 Where things live (architecture)
- **`UWRSurvivalComponent`** (new, `WRSurvival.h/.cpp`) on the **PlayerState** (survives death like corruption must;
  saves with the player JSON). Server ticks every 0.25 s: sleep channel, Rested, comfort near the bed, lit/unlit,
  weather on this player, corruption, the held light's burn. Replicated to everyone: `bSleeping`, `RestedUntil`,
  `RestedMinutes`, `Comfort`, `bUnlit`, `Corruption`, `LightItem` + `bLightOn` (others see your torch).
- **`FWRBiomes::At(World, Pos)`**: the biome at a point. Today's island: west of `sky.biomeCentre` is volcanic, east is
  tundra (the rule fishing, sky and audio already use); `AWRCave::IsInside(Pos)` for underground. One function, so the
  world-map lane swaps in the 12-wedge lookup in one place.
- **`UWRLightSubsystem`** (world subsystem): `IsLit(Pos)`, `IsWarded(Pos)` (fires that stop corruption). Sources found by
  scan (cheap, every 0.5 s on the server): build pieces of kind `light` (torch 6 m, brazier 10 m), Hearths (10 m),
  creatures whose `utility` is `light` (Glowmoth 12 m, wild or tamed), and players with a lit held light. Radii in
  tuning `lights.*`, scaled by the weather's `lightRadiusMult`.
- **`UWRWeatherSubsystem`** (world subsystem, server rolls) + **`AWRGameState::Weather`** (replicated array, one row per
  biome: kind, warnAt, startAt, endAt in server seconds). Every machine reads strength from the game state, so the
  look, sound and HUD agree with the server's effects.
- Hooks into the existing code, all reading the component: `TickRegen` (stamina), `TickSurvival` (drains),
  `UWRLifeSkillsComponent::AwardXP` (XP), `HealingTakenScale` (heal), `ApplyToCharacter` (max stamina),
  `GainResource` (path resources), `ApplyHit` (path damage), `PickTarget` (aggro), `GetSpeedMultiplier` (deep snow via
  a replicated tag), crop growth (Ashfall pause).

### 2.2 Sleep and Rest (R1 to R4, R24)
- **E on your bed** (or bedroll) sets it as before **and lies you down**. Refused with a reason: combat-tagged ("In
  combat: not now", existing), a hostile creature after you within 15 m ("You can't sleep with enemies close",
  Valheim's rule), hunger or thirst empty ("Too hungry to sleep" / "Too thirsty to sleep"), swimming, mounted.
- **The body on the bed**: the server stands you on the bed's top surface (`WRBeds::SpotAbove`), turned along the
  bed's length with the head at the head end, and you lie down. Clip: `player_art.json anims.sleep` (key added, no
  hero clip on disk yet), falling back to the **last pose of the death clip held** (the hero on his back) so the body
  lies on the mattress instead of standing on it. Breathing comes from the held pose's idle layer. A real lie-down
  clip is logged to later-cooks (needs a retarget in the editor).
- **The 10 s**: the owner's screen dims to 30% over 2 s (`rest.dimSeconds`), a "Sleeping" bar fills under the
  crosshair with "Move to get up". Any move key or jump sends `ServerWake`; the server also cancels on damage (health
  drop), hunger or thirst reaching 0, being moved > 1 m, the bed going away, death.
- **On finishing**: `FWRRest` fires once: Decay's lost max HP returns (`RestoreDecay`), corruption x0.5, Rested
  starts at `bed 20 / bedroll 10 + comfort x 1` minutes (comfort counted at lie-down), and the native delegate
  `UWRSurvivalComponent::OnRest` broadcasts (Deathless and Void bind to it when those paths land). A toast "Rested" with
  the minutes and the comfort that set it; sound `rest.wake`. Sleeping in a bedroll doesn't use it up (built rule stays).
- **Rested** effects are read where they act (stamina regen x1.2, drains x0.8, life-skill XP x1.1); a new sleep
  restarts it at the new length; death ends it (Valheim); it saves across log-out (seconds left).

### 2.3 Comfort (R5)
- `pieces.json` rows gain `"comfort": {"group", "value"}` for what exists today: bed (bed 1), chair (seat 1),
  table (table 1), rug (floor 1), banner (wall 1), trophy mount (trophy 1, up to 3 different mounts), torch (light 1),
  brazier (fire 1), Hearth (fire 2). Future decor adds rows, no code.
- Counted at the bed when you lie down (and every 2 s for the HUD while you're within 12 m of a bed you can use):
  every placed (not ghost) piece within `comfort.radius` 1200 cm whose top has a build piece above it (trace up 15 m,
  hits an `AWRBuildPiece`), best value per group, trophies summed to 3, total capped at `comfort.cap` 20.
- Build bar: every decor card shows "Comfort +N (group)".
- HUD: within 12 m of your bed the Rested pip shows "Comfort 14" beside it even when not rested.

### 2.4 Darkness and light (R12 to R14)
- **Unlit** (server, every 0.25 s): no light reaches you, and you are inside a cave (`AWRCave::IsInside`) or in a biome
  whose row has `darkNights` at night or `underground` (Fungal caverns, Crystal caves interiors) — data in `weather.json`.
- **Dark-hunters**: `creatures.json "darkHunter": true` (Gloomhowl today; Echobat, Gravehound, Wraithmoth, Cryptstalker,
  Hollowfang, Riftmaw, Rotmaw, Webspinner when their biomes land). Their aggro radius x`darkness.darkHunterAggroMult` 2
  on an unlit player. HUD pip "Dark" (eye glyph) while unlit, with a one-time hint "Unlit: dark-hunters smell you
  from twice as far. Carry a torch or lantern".
- **Hand torch** (`hand_torch`, hand recipe: wood 1, fiber 2): use it (bag or belt) and it goes **lit into the Light
  slot**, held in the left hand (`torch_l` socket, `bow_l` fallback); the shield comes off the arm while it's lit
  (`HasShield` false: you guard with the weapon, combat.md 2.4.0). Burn 20 min as durability (`lights.torch.burnMinutes`);
  Forest rain (`burnMult` 2) halves it. Swimming puts it out (it stays in the slot, unlit; use it again to relight).
  Burned out: used up, "Your torch burned out". Model: turned ash-wood haft, a pitch-soaked rag head bound with a tube
  wrap (`FWRToolMesh::Torch`), flame + smoke from `fx.json "held.torch"`, a warm flickering point light 6 m.
- **Horn lantern** (`horn_lantern`, workbench L2: bronze ingot 2, hide 1, tallow 1): lit, it **hangs at the left hip**
  (`hip_l`), both hands free, the shield stays. 9 m light. Burns **tallow** (`tallow`, workbench: raw meat 2 → 1;
  rendered fat, the real lamp fuel): one tallow = 90 min; at empty it takes the next from the bag; none left = it goes
  dark and says so. Model: bronze base and cap, four amber horn panes, a ring bail (`FWRToolMesh::Lantern`); light 9 m,
  steadier flicker than the torch.
- **Light slot**: `EWREquipSlot::Light` + `EquippedLight` on the inventory; shown as a 4th equipment cell in the bag
  (mouse and pad through the existing slot handling); use or unequip puts it out and back in the bag.
- **Placed lights, Hearths, Glowmoths** light you too (subsystem scan).

### 2.5 Weather (R7 to R11, R19)
- **`Data/weather.json`**: `rollMinutes 30, warnSeconds 60, rampSeconds 20`, and per biome (all 12): `darkNights`,
  `underground`, `corruption {path}`, and `kinds[]` with `id, name, chance, minutes [min,max], dayOnly/nightOnly`,
  `effects` (`vision` m, `thirstMult`, `outdoorsOnly`, `sunOnly`, `staminaRegenMult`, `runMult`, `lightRadiusMult`,
  `corruptionMult`, `burnMult`, `swimStaminaMult`, `aggroMult` + `aggroCreatures`, `cropsPause`, `strikes{}`),
  `counters` (`roofWithin` cm, `resist` frost/lava (existing infused armour), `armor` ids, `pathRoot` node ids, `items`),
  and `look` (`fx` emitter, `fxRate`, `fogDensity`, `fogColor`, `skyDim`, `bed` sound layer). The 12 rows are
  survival-crafting 2.4 exactly.
- **Rolls** (server): each biome every 30 min; a roll that hits schedules `warnAt = now`, `startAt = now + 60 s`,
  `endAt = startAt + minutes`. Strength ramps 0→1 over 20 s from `startAt` and back over the last 30 s. `Approach`
  0→1 over the 60 s warning drives the signs.
- **Effects on a player** (server, every 0.25 s, only where their biome has weather and strength > 0.5): sheltered =
  a trace up within `roofWithin` (default 15 m) hits anything static; countered = sheltered (if allowed) or a counter
  matches. Uncountered effects multiply: thirst (TickSurvival), stamina regen (TickRegen), run speed (tag
  `Weather.DeepSnow`, read in `GetSpeedMultiplier` on both machines), the light radius scale for the whole biome
  (subsystem), corruption build, held torch burn, swim stamina, creature aggro on that player.
- **Strikes** (Thunderstorm, Squall): every 20-40 s per 60 m cell, the highest uncountered player above `minAltitude`
  gets a 1.0 s ground glow (`weather.strikeWarn` fx + sound) at their feet; then the bolt (`weather.strike` fx, thunder)
  hits whoever is still within 3 m through **ApplyHit** (60 lightning damage, stagger 0.8 s). It can kill (no floors).
- **Look** (each client, from the game state + its own biome): Blizzard: snow rate x5 and a sideways `snow.blizzard`
  layer; fog density to the row's value (vision 40 m) in a white-blue; sky dims 25%. Ashfall: ash x4, brown-grey fog
  (vision 60 m), sky dims 35%. During the 60 s approach the fog density and the bed rise along `Approach`, so the
  far distance closes in first: the horizon "wall" coming (MH Wilds). Under a roof or in a cave the falling snow/ash
  stops (already built), the fog stays.
- **Sound**: beds `weather.blizzard` (tundra wind recording, louder), `weather.ashfall` (volcanic rumble + high wind),
  layered by `LayerWeight("weather.<kind>")` = strength or approach, x(1 - cave). Music untouched (never stops, R11).
- **HUD**: a weather pip (glyph per kind) with the name and minutes left; its line says the counter state ("Sheltered",
  "Frost gear holds it", or what it's doing to you). 60 s ahead: a toast "A blizzard is coming" with what it does and
  the counter.
- **Ashfall pauses outdoor crop plots** (no build piece above the plot).

### 2.6 Corruption (R15 to R18)
- `Corruption` 0..100 on the survival component, replicated, saved, carried through death.
- Builds only in biomes with a `corruption` row (Graveyard, Corrupted wasteland): +1 per 6 s at night, underground
  or in a rift (`rift` hazard kind when it lands), +1 per 15 s by day on the surface; x weather `corruptionMult`;
  stops within a warded fire (brazier / placed torch / Hearth: `lights.*.ward`); a lit lantern or a light tame
  following you halves it. Falls 1 per 3 s outside those biomes; Rest halves it; `use.corruption` on an item lowers
  it (the Cleansing draught row lands with the alchemy L3 round, it needs asphodel / sunstone / slime).
- Steps (`corruption.steps` in tuning): 25 heal x0.9; 50 stamina regen x0.85 and dark-hunters treat you as unlit;
  75 max stamina -20 (re-applied when the step changes); 100 **Marked**: creatures of that biome x2 aggro on you,
  path resources fill x0.75.
- **Matching path** (`corruption.path` of the biome = your main path): no penalties; path damage (spells, Wildbrands)
  +3 / 6 / 10 / 15%.
- HUD: a thin violet ring under the Food/Water gauges labelled "Corruption NN", only above 0; step changes toast.
- **On the island today** neither corrupt biome exists, so the meter never builds; the rows, code and HUD are live and
  switch on when the Graveyard and Wasteland land. Said plainly in the CHANGELOG.

### 2.7 Every object's job and contacts (R24)
- **The torch** is the cave route's cheap light: it trades the shield for safety in the dark, burns down so you plan
  the trip, dies in water (you see it go out), and its flame, smoke and light come from its rag head, not the hand.
  It rests in the left fist; the haft runs down past the little finger like a real torch grip; it never clips the
  body because the off arm hangs it out from the hip.
- **The lantern** hangs by its bail from the hip socket, swings with the body (attached to the hip bone), lights
  9 m, and asks for tallow, which ties hunting (raw meat) to exploring.
- **The sleeper** lies on the mattress, not in it (SpotAbove = top of the bed's bounds), along the bed, head to the
  head end; the room around the bed (roof, chairs, table, rug, fire) is what makes the sleep worth more. A bedroll
  in the open is a short Rest, a furnished room under a roof is a long one: the reason to build a real home.
- **Fires** (brazier, placed torch, Hearth) are the anchors against corruption and darkness: a base's light is both
  a comfort piece and a ward.
- **Weather** lands on the biome it belongs to: snow from the tundra's sky, ash from the volcano's, signs from the
  horizon in, stopped by a roof the way real shelter works.

### 2.8 Build steps
1. `WRSurvival.h/.cpp`: FWRBiomes, UWRLightSubsystem, UWRWeatherSubsystem (+ FWRWeatherDefs from weather.json),
   UWRSurvivalComponent (sleep, rest, rested, comfort, lit, weather effects, corruption, held light burn, save/load).
2. GameState weather array; PlayerState component; GameMode save/load.
3. Beds: E lies you down; character sleep pose and wake; controller ServerWake + move cancels it.
4. Hooks: regen, drains, XP, heal, max stamina, resource gain, path damage, aggro (dark-hunter, Marked, weather),
   speed tag, crop pause.
5. Held light: Light equip slot, inventory use/unequip, burn, meshes (FWRToolMesh::Torch / Lantern), attach, point
   light + fx, shield off with a torch, bag UI 4th cell.
6. Data: weather.json (12 biomes), tuning blocks (rest, rested, comfort, darkness, lights, corruption, weather fx),
   items + recipes (hand_torch, horn_lantern, tallow), pieces comfort, creatures darkHunter, fx rows, sound rows, beds.
7. Client look: fx weather density, fog/sky in WRSky, audio layers.
8. HUD: status row (Rested + comfort, weather, Dark, Corruption ring), sleeping dim + bar, toasts/hints, build-bar
   comfort text. Glyphs drawn with `Tools/ui_glyphs.mjs`; item icons drawn the same way.
9. Docs: CHANGELOG Unreleased, HANDOFF, this ledger's proof column. Compile. Spot-check. Commit, push.

---

## 3. NOT IN THIS LANE'S REACH (logged to later-cooks, said in the final report)
- A real lie-down / sleeping clip for the hero (needs an editor retarget).
- The 10 other biomes' terrain: their weather rows, dark nights, corruption and strikes are live in data and code but
  only volcanic and tundra exist on the island, so only Ashfall and Blizzard can happen today.
- Cleansing draught item (needs asphodel, sunstone, slime), grounding charm / Rodcrest / Grounded counters (their
  items and creatures land with the Storm peaks round), gusts on marked cliff paths, irrigation (rain irrigates),
  tracks, spore clouds, flying mounts' stamina: effects whose host systems don't exist yet.

---

## 4. SPOT-CHECK (read against the plan and THE BAR)
- Every number is data: `tuning rest / rested / comfort / darkness / lights / corruption / weather`, `Data/weather.json`, `pieces.json comfort`, `creatures.json darkHunter`, `items.json` / `recipes.json` rows, `fx.json` and `sounds.json` rows.
- Server authority: sleep, Rest, Rested, comfort, lit/unlit, weather effects, burn, corruption, strikes and gusts all run on the server (`UWRSurvivalComponent::TickComponent` returns on clients); clients read replicated state; the owner's wake is a Server RPC the server also enforces by distance.
- All damage through ApplyHit (strikes). No floors added (deprivation while asleep wakes you; nothing holds HP).
- Fixed on the read-through: weather texts claimed effects with no system behind them (crop watering, tracks, boats, flyer stamina, muffled steps): removed; gusts claimed by Gale/Thunderstorm: built (`effects.gust`); the death pose would have been cancelled when a sleeper died: kept; path match was "any corrupt biome": now the biome the corruption came from; pad prompts named an unbound key: now the belt.

## 5. PROOF (requirement → where)
| # | Done | Where |
|---|---|---|
| R1 | yes | `WRSurvival.cpp` FinishSleep (Rested), StaminaRegenMult / HungerDrainMult / SkillXpMult; hooks `WRStatusComponent.cpp` TickRegen / TickSurvival, `WRLifeSkills.cpp` AwardXP |
| R2 | yes | `WRSurvival.cpp` FinishSleep: RestoreDecay, corruption x restKeeps, `OnRest.Broadcast`; bed / bedroll use `WRBuilding.cpp` bed Use → TryStartSleep |
| R3 | yes | `WRSurvival.cpp` TryStartSleep (refusals, the pose on the mattress), TickSleep (cancels), `WRHeldLight.cpp` TickSurvivalLook (pose, wake input), ServerWakeUp; `WRUIHud.cpp` SleepCover (dim, bar) |
| R4 | yes | `tuning.json rested`; FinishSleep length = base + comfort |
| R5 | yes | `WRSurvival.cpp` FWRComfort::At, TickComfort; `pieces.json comfort`; `WRUIBuild.cpp` Describe ("Comfort +N"); HUD Rested/Comfort line |
| R6 | yes | `WRUIHud.cpp` Conditions (Rested line, minutes, pulse under 60 s) |
| R7 | yes | `Data/weather.json` (12 biomes), `WRSurvival.cpp` UWRWeatherSubsystem::Tick / Roll, `AWRGameState::Weather` |
| R8 | yes | weather.json: thunderstorm (strikes, gusts), blizzard, heatwave (sun trace), ashfall; `WRSurvival.cpp` TickStrikes, TickWeather (sun, gusts) |
| R9 | yes | no meter anywhere; every kind has `counters`; TickWeather counter / shelter checks |
| R10 | yes | `WRUIFlow.cpp` CheckSurvival (toast 60 s ahead), `WRSky.cpp` ApplyAtmosphere (approach fog), `WRAudioDirector.cpp` LayerWeight `weather.*` (approach bed) |
| R11 | yes | `fx.json snow.blizzard / ash.fall`, `WRFxAmbient.cpp` TickWeather, `WRSky.cpp` fog colour/density + sun dim, `sounds.json weather.*` beds; music untouched |
| R12 | yes | `WRSurvival.cpp` TickLight (unlit), AggroMultFor; `WRCreature.cpp` PickTarget; `creatures.json gloomhowl darkHunter` |
| R13 | yes | `WRSurvival.cpp` WRBiome::IsUnderground (chamber + inner tunnel) → unlit |
| R14 | yes | `UWRLightSubsystem` (pieces, Hearth, creatures, held); `EWREquipSlot::Light` (`WRInventory.*`); `FWRToolMesh::Torch / Lantern`; `WRHeldLight.cpp`; `WRWeapons.cpp` HasShield, `WRRanged.cpp` shield off; burn / rain / water / tallow in TickLight; items + recipes |
| R15 | yes (dormant on the island: no Graveyard / Wasteland land yet) | `WRSurvival.cpp` TickCorruption; weather.json `corruption` rows |
| R16 | yes | HealingTakenMult (`WRStatusComponent.cpp`), StaminaRegenMult, MaxStaminaPenalty (`WRPathComponent.cpp`), AggroMultFor (Marked), ResourceGainMult (`WRCombatLibrary.cpp` GainResource) |
| R17 | yes | PathDamageMult (`WRCombatLibrary.cpp` ApplyHit), MatchesCorruption (CorruptionFrom) |
| R18 | yes | HUD "Corruption NN" only above 0.5 (`WRUIHud.cpp` Conditions) |
| R19 | yes | strikes via ApplyHit, deprivation wakes, nothing floors HP |
| R20 | yes | see section 4 |
| R21 | yes | replication in `UWRSurvivalComponent::GetLifetimeReplicatedProps`, `AWRGameState::Weather`, `UWRInventoryComponent EquippedLight`; save in `WRGameMode.cpp` Save/LoadPlayer `survival` |
| R22 | yes | bag Light cell `WRUIInventory.cpp` LightBlock (SWRSlot: pad-navigable), L key `WRKeyBindings.cpp` / `WRPlayerController.cpp` LightTogglePressed, belt toggle `WRPlayerUI.cpp` UseHotbar |
| R23 | yes | unfinished parts listed in CHANGELOG and later-cooks; no UI shows an unbuilt effect on the island |
| R24 | yes | section 2.7; torch upright in the off hand, lantern plumb at the hip, sleeper on the mattress along the bed, fires as wards |
| R25 | yes | sections 1-4 of this doc |
| R26 | yes | commit on master, CHANGELOG Unreleased + HANDOFF in it, no release; LFS: 17 PNGs, 73 KB |
