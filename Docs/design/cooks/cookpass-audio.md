# Cook pass: all sound and music (cookpass-audio, desktop, 2026-11-15 night)

Scope: every sound and music piece already built (Data/Audio/sounds.json, `WRSound.cpp`, `WRAudioDirector.cpp`, the audio
settings page) graded against THE BAR and raised. Sources read first: the brief, FULL-GAME.md 3.3 "Music (all biomes)",
COMBAT-AND-LESSONS L1 / L13, cooks/fire-frost.md 3.5-3.6 + 4.4 (the Volcanic and Tundra cue sheets), the biome cooks' 2.15-2.16
music sections (void, graveyard, crystal, fungal, sky, ocean, desert), cookpass-world 3.2 (the ten ff.* sub-zone beds),
cookpass-creatures 3.4, and the share's LATER-COOKS + AARON-CHAT-NOTES.

## 1. Requirement ledger

Proof column filled at the end (section 7). "R" rows come from the brief and Aaron's quotes, "D" rows from the design docs
the brief points at, "L" rows from LATER-COOKS lines that touch this scope.

| # | Requirement | Source |
|---|---|---|
| R1 | Research how the best games do sound and music; write the specific details used | brief, EVERY LANE 1 |
| R2 | Grade every built piece of sound and music against THE BAR | brief SCOPE |
| R3 | Design the raise in writing (plan) before any build; every design gap solved | brief, EVERY LANE 2 |
| R4 | Build exactly the plan at full detail; no placeholder presented as finished (stand-ins stay marked) | EVERY LANE 3, BAR |
| R5 | Spot-check own work against plan + BAR by reading code; fix before pushing | EVERY LANE 4 |
| R6 | "Already done" only with file:line proof | EVERY LANE 5 |
| R7 | Design with intention: every placed sound says why it is there, what it connects to, how it meets what it touches | Aaron 11-14 |
| R8 | Requirement ledger with proof per row; final message lists every row | Aaron 11-14 |
| R9 | Data-driven: rows and numbers in Data/Audio/sounds.json and tuning, never in code | BAR |
| R10 | Multiplayer: audio is per client, driven only by replicated state; nothing on a dedicated server | BAR |
| R11 | Any new setting works by mouse AND pad | BAR |
| R12 | Never leave master unable to boot to menu and into play | site rule 11-15 |
| R13 | No synth sounds: only real instruments in the music (Aaron's F8 taste, fb_ec58c820 / fb_0d5c4bbd on Waveraider, applied here) | AARON-CHAT-NOTES 19:59 |
| D1 | Music always audible: no engine silences between pieces; planned silences only where a design writes one | FULL-GAME 3.3, fire-frost 3.6 |
| D2 | Music ducks at most 4 dB (combat, creature calls, moments) | FULL-GAME 3.3, fire-frost 3.5-3.6 |
| D3 | Each biome: explore theme, night variation, combat layer, boss track; real composed pieces, never generated | FULL-GAME 3.3 |
| D4 | Volcanic cue sheet (66 BPM D minor: taiko, low brass, anvil, drone): explore A/B/C, night A/B, combat, mini, Vulkarn P1-P3, Kiln Roads, Eruption layer, camp | fire-frost 3.6 |
| D5 | Tundra cue sheet (58 BPM A minor: sparse piano, low strings, wordless vocal, bowed strings, bowed glass): explore A/B/C, night A/B, combat, mini, Skaldrith P1-P3, Blue Below, Whiteout layer, camp | fire-frost 3.6 |
| D6 | Combat comes in on the next bar (adds to the music instead of cutting it); boss phases resequence on the bar | fire-frost 3.6 |
| D7 | Loudness: music −18 LUFS integrated (calm −23, boss −15), true peak −1 dBTP | FULL-GAME 3.3, CAL L13 |
| D8 | Master volume before the dynamics; the limiter acts < 1% of the time | CAL L13 |
| D9 | Music ducks 2-4 dB on the biggest hits instead of both hitting the limiter | CAL L1 / L13 |
| D10 | Tells mixed 3 dB over the bed; every tell over 0.5 s rises in pitch ("sound before sight") | fire-frost 3.5, CAL 1.13 |
| D11 | Credits for every piece in Docs/CREDITS.md | fire-frost 3.6 (Raven's rule) |
| L1 | The ten Fire/Frost sub-zone beds (ff.V1..T5) are stand-ins: real recordings wanted (crater roar, lava, glass-edge wind, hiss + bubbling, wind + snow hiss, creaking trunks, lake-ice booming, ice groans + meltwater, katabatic howl) | LATER-COOKS cookpass-world |
| L2 | The 16 ff.* hazard sounds are stand-ins: real hiss, geyser, serac crack, lake-ice, katabatic howl | LATER-COOKS cookpass-world |
| L3 | A sleep breath per size class (sleep is silent) | LATER-COOKS cookpass-creatures |
| L4 | Species' own alert / flee / answer calls (today pitched stand-ins via voiceFallback); tames answer the whistle with their own call | LATER-COOKS cookpass-creatures, cookpass-taming, build-finish |
| L5 | Event music layers: VOL_Eruption (and the Whiteout's) | LATER-COOKS build-world-systems |
| L6 | Composed cues for the Void, Graveyard, Crystal, Fungal (today borrowed tracks); same for the Sky, Ocean and Desert stand-in pools | LATER-COOKS build-biome-void / graveyard / crystal / fungal |
| L7 | Fused creature and boss voices (void, graveyard, crystal, fungal, portal creatures, Fire/Frost recipes) | LATER-COOKS, fire-frost 3.5 |
| L8 | Proximity voice chat (not in any doc) | LATER-COOKS multiplayer |
| L9 | The portal music's "through stone" reverb send | LATER-COOKS build-portals-artifacts |

## 2. Research: how the best games do it

**Continuous, place-led scores.**
- *Hades* (Darren Korb): the music never stops between rooms; each fight's track is built in stems and the end of a fight
  is a written ending on the bar, never a fade on an arbitrary frame. Detail used: transitions wait for the next bar line of
  the piece playing (we know each piece's tempo), so a change lands where a musician would put it.
- *Red Dead Redemption 2* (Woody Jackson): exploration is a bed of sparse stems that thicken as danger rises; combat is
  entered with a percussive hit that hides the switch. Detail used: a one-shot "entry stinger" (a real drum hit) masks the
  move from explore to combat, and the combat piece starts on the explore piece's next beat (≤ 1 beat late).
- *The Legend of Zelda: Breath of the Wild* (Manaka Kataoka): small enemies get a short skirmish cue, real threats the
  full battle theme; a lone critter does not start battle music. Detail used: a **threat score** (size, role) decides
  whether a fight is worth combat music; a snowpuff pecking you is not.
- *Dark Souls*: the boss music stops on the killing blow; a short sting and then a held breath of quiet before the world
  comes back ("victory" earns silence). Detail used: a boss's fall plays a cadence stinger, the boss piece ends on the bar,
  and a written 12 s hush follows (the only engine silence left, planned).
- *Monster Hunter World*: each big monster has its own theme, and the quest-clear fanfare is a stinger in the same key.
  Detail used: every major boss gets its own pool per phase (boss.<id>.p1-3), minis share the land's mini cue.
- *Skyrim* (Jeremy Soule): explore, dungeon and combat sets per region, with long silences. FULL-GAME rejects the
  silences, so we keep the per-place sets and drop the gaps (breath of 4-10 s between pieces).
- *Valheim*: home music at your base, each biome's air its own bed, and caves wet with reverb. Detail used: camp cues at
  camps, reverb by space.

**Mix, spaces and dynamics.**
- *Battlefield / Frostbite HDR audio* ("War Tapes"): a loudness window, quiet sounds drop out under loud ones, and a "Night"
  (narrow range) mode for quiet rooms. Detail used: a master limiter always (-1 dBTP, the L13 rule) and a **Dynamic range:
  Wide / Night** setting (a gentle compressor with make-up gain ahead of the limiter).
- *The Last of Us Part II*: reverb follows the space you are in (rooms, tunnels, open air), changing with a short fade as
  you cross a door. Detail used: data rows of **spaces** (cave, lava tube, long hall, tight web, hollow root, coven, drowned
  hall, crystal, fold, built room, underwater) with their own decay, damping and wetness, switched by where the listener is.
- *Ghost of Tsushima*: wind and weather carry the outdoor mix; the music dips under the big moments but never vanishes.
  Detail used: the 4 dB cap on every duck.

**Accessibility.**
- *The Last of Us Part II* (awards for its 60+ options): **sound captions with direction** ("[Clicker] clicking" with an
  arrow), and the "Enhanced listen" cue. *Fortnite*: "Visualize sound effects", a ring showing where footsteps, chests and
  shots come from. Detail used: **Sound captions: Off / Important / All**: a caption row per event in sounds.json
  ("{who} snarls"), drawn bottom centre with an 8-way arrow toward the source, merged when repeated, faded by distance.
  Important = danger only (aggro, wind-ups, tells, explosions, boss roars).

**Sound before sight.** *Sekiro* and *Elden Ring* tells are audible before the swing. CAL 1.13 says a rising cue on every
wind-up over 0.5 s. Detail used: event rows can **rise** in pitch over their tell (`rise: [from, to, seconds]`).

**Real-world reference (soundscape ecology, Bernie Krause).** A place's sound is geophony (wind, water, rock), biophony
(animals, each in its own frequency niche) and anthrophony. The beds carry the geophony; the life and the events are
**spot sounds**: one-shots placed in 3D round the listener now and then, as RDR2's ambient emitters and Skyrim's random
ambient sounds do. The Fire and Frost real-world details used:
- Volcanoes: fumaroles hiss steadily and roar when they vent; mud pots plop; cooling pāhoehoe crust cracks with a glassy
  tinkle; craters "chuff" (gas puffs) and rumble in infrasound; scree runs off steep obsidian ridges.
- Cold lands: lake ice "sings" (dispersive pew-pew chirps) and booms as it contracts when the air cools at dusk and night;
  trees "frost crack" at night in hard cold with a rifle-shot report; glaciers groan and calve; katabatic wind pours off
  ice sheets with a rising howl; ravens follow travellers across open snow.
- Sleep: a large animal breathes 6-10 times a minute when asleep, a small one 20-40; a sleeping animal is the quietest
  thing in a scene and is heard only within a few metres.

**Loudness standards.** EBU R128 / ASWG-R001: −23 LUFS (TV), games often −18 to −16 for loud mixes, true peak −1 dBTP.
The repo's pipeline already measures BS.1770; music is normalised to −18 (calm cues −21, boss −16 short-term ceiling).

**Music sources (no AI, real instruments).** Kevin MacLeod's incompetech catalogue (CC-BY 4.0, already the Storm / Forest /
Swamp source and credited) lists each piece's BPM, instruments and feel in `pieces.json`, so each cue is picked by tempo,
instruments and mood, with **anything listing synths / electric / drum kit / guitar, or whose notes mention synths,
filtered out** (R13). Recordings: Freesound CC0 (licence read per sound), the HQ preview as the earlier sound passes did.

## 3. Grades of what is built (against THE BAR)

| Piece | Where | Grade | Why |
|---|---|---|---|
| Playback engine (OGG at runtime, procedural waves, 3D attenuation, LPF with distance, variants, min gap, voice caps) | WRSound.cpp:93-768 | A- | Solid; no reverb send set, no rising tells |
| Event table (1,180 events, 447 marked stand-ins) | sounds.json | B | Rich; Fire/Frost hazards and many biome rows are stand-ins |
| Music director: modes | WRAudioDirector.cpp:626-987 | C | 120-300 s silent gaps on the island (D1 broken); any aggro critter starts combat music; changes on any frame (no bar); no entry stinger; boss death just fades into 40-90 s of silence; ducks up to 16.5 dB (boss.intro 0.85, D2 broken) |
| Fire / Frost music | sounds.json pools explore/night/combat/boss | D | 8 shared CC0 tracks, no volcanic or tundra pools, no night/boss/mini/camp/cave/event cues; `music/boss` is a synth chiptune (R13) |
| Storm / Forest / Swamp music | pools *.storm_peaks, *.forest, *.swamp | B+ | Own licensed sets per zone, phases, layers |
| Desert / Ocean / Fungal / Sky / Graveyard / Crystal / Void music | pools | D | Every pool borrows another land's tracks (marked `*` stand-ins); Crystal has no explore pool at all |
| Event music layers | WRAudioDirector.cpp:905-938 | B- | Storm, swamp, forest, ocean, desert, fungal, sky layers exist; Eruption and Whiteout have none (L5) |
| Island beds | ambient.beds volcanic / tundra / lava / coast / day / night / cave / high | B- | Real recordings, crossfaded by place and time |
| ff.V1..T5 sub-zone beds | ambient.beds ff.* | D | Every one a stand-in borrowed bed (L1) |
| ff.* hazard sounds | events ff.* | D | 16 stand-ins pitched from unrelated files (L2) |
| Spot sounds | WRAudioDirector.cpp:557-566 | D | One hard-coded owl; no life or geology in the island air |
| Reverb / spaces | none | F | Caves, lava tubes, halls, the Coven, the Drowned Hall all sound like open air |
| Mix dynamics | none | D | No master limiter (L13); no night mode; ducks uncapped |
| Tells | creature.windup | C | Static pitch; CAL wants a rising cue |
| Creature voices | WRSound.cpp:879-943 | C+ | Families with per-animal pitch; alert / flee pitched stand-ins (L4); sleep silent (L3) |
| Accessibility | none | F | No sound captions or direction cues for deaf / hard-of-hearing players |
| Audio settings | WRUISettings.cpp:1400-1441 | C | Five sliders; hint text says "Music stays quiet most of the time" (wrong once D1 lands); no captions or range setting |

## 4. The plan (every detail decided before building)

### 4.1 Music director (WRAudioDirector)

1. **Always audible (D1).** `music.gap`, `firstGap`, `afterCombatGap` become breaths: [4, 10], [3, 8], [4, 9] s. The
   swamp / forest special cases fold into the one rule. Planned silences stay exactly as their cooks wrote them: Dead calm,
   the Silent Vault, the Fall Field's 20 s, and the new boss-fall hush (4.1.6).
2. **4 dB cap (D2).** `Duck()` clamps every request to `music.duckMax` = 0.37 (−4 dB) everywhere (was swamp / forest only).
3. **Threat-gated combat (BotW).** Each creature on you within `music.combatRange` adds a weight: critter 0.25, small
   0.5, medium 1, large 1.5, a mini or elite 3 (`music.threat.*`). Combat music when the sum ≥ `music.combatThreat` (1.0)
   or you were hit (OnHitShown on your pawn) while anything is on you. A lone snowpuff never starts it; two foxes do.
4. **On the beat / bar (Hades, fire-frost 3.6 D6).** Pool rows may carry `bpm` (and `beats` per bar, default 4, `lead`
   seconds to the first downbeat). The director counts each stream's own play time (paused time excluded). Rules:
   - explore → combat: the switch waits for the explore piece's **next beat** (≤ 1 beat), then the combat piece starts at
     full speed (fade-in `music.combatFadeIn` 0.6 s) under a real drum-hit **entry stinger** (`music.sting.combat`, a
     taiko / frame-drum hit, 2D, music category). The explore piece releases over 2 s from that beat.
   - combat → explore, explore → next piece, boss phase → next phase: the old piece fades from its **next bar line**.
   - pieces with no `bpm`: as now (immediate).
   - wait cap `music.syncMax` 4 s (a very slow bar never holds a fight back).
5. **Places.** New lookups in StartMusic: `sub.kiln_roads` / `sub.blue_below` inside the hidden tubes (AWRHiddenTube,
   within `music.tubeReach` of the route); `layer.eruption` while the eruption is on (AWRWorldState event `eruption`,
   stage ≤ 1) in its biome; `layer.whiteout` likewise. Camp cues already come from POI camps (`camp.<land>`).
6. **Boss fall (Dark Souls / MH).** The engaged boss the music followed dies → its piece ends on the next bar (2 s
   release), `music.sting.boss_fall` plays (a brass / choir cadence cut from a real piece), and the music holds a planned
   hush of `music.bossHush` 12 s, then the land's explore resumes. The `boss.kill` impact stays as is (WRFxCombat).
7. **Death.** While your pawn is dead the music dips 4 dB (cap) and no new piece starts until you are back.

### 4.2 Music content (D3-D5, L6, R13): the cue sheets

All MacLeod (incompetech, CC-BY 4.0), filtered to real instruments, normalised by `Tools/audio/build_cookpass_audio.py`
(−18 LUFS, calm cues −21, boss −16; true peak −1 dBTP), Vorbis q1.5 (the music stays inside ~1.8 MB a piece), capped at
4:00 with a 6 s fade. `bpm` in each pool row from the catalogue.

**Volcanic (66 BPM D minor, brass, taiko, anvil):**
| Pool | Piece | Why it fits |
|---|---|---|
| explore.volcanic (A) | Death of Kings (60) | Horns, trombones, cimbasso over a drum: slow brass over taiko |
| explore.volcanic (B) | Steel Rods (69) | Struck steel in imitative counterpoint: the anvil ostinato |
| explore.volcanic.c (V4 Solfatara) | Drums of the Deep (77) | Sparse drums under low strings: C for V4 |
| night.volcanic | Mary Celeste (50), Oppressive Gloom (58) | Low brass and bass clarinet drone; death-march strings |
| combat.volcanic | The Descent (103) | Horns, trombones, tuba over driving percussion, no busy melody |
| mini.volcanic | Colossus (86) | Brass and percussion, culminating: Warden, Ignessa, Cinderhorn |
| boss.vulkarn.p1 / p2 / p3 | Industrial Revolution (140) / Death and Axes (140) / Stoneworld Battle (110) | Pipe organ, trombones and anvil-like percussion (the forge-giant's march); full orchestra doubles the drive; the giant battle finale |
| sub.kiln_roads | Tenebrous Brothers Carnival - Act Three (78) | Metallic percussion "like a giant mechanism": the forge rhythm in the tubes |
| layer.eruption | StompDance (80) | War drums that accelerate, low in the mix over whatever plays |
| camp.volcanic | Peaceful Desolation (54) | Low brass and timpani at rest, opening up at the end |

**Tundra (58 BPM A minor, piano, low strings, voice, glass):**
| Pool | Piece | Why it fits |
|---|---|---|
| explore.tundra (A) | Wounded (50) | Piano and strings, rain-on-the-window sparse |
| explore.tundra (B) | For Originz (55) | Strings with a sliding violin and a woman's wordless voice: the joik line |
| explore.tundra.c (T4, T5) | Frost Waltz (99, 3/4) | Celesta and glockenspiel over strings: bowed-glass shimmer for the ice shelves |
| night.tundra | At Rest (57), Blue Feather (42) | A voice over piano and strings; solo piano with a falling figure |
| combat.tundra | Gloom Horizon (100) | Basses, cellos, violins, percussion: the low string ostinato |
| mini.tundra | The Snow Queen (96) | Strings, bassoon, bass clarinet, celesta, timpani: fairy-tale menace (Grimjaw, Pale Hunter, Old Tusker) |
| boss.skaldrith.p1 / p2 / p3 | The Escalation (96) / Hero Down (110) / Darkling (140) | One long string build (the march); horns rise with the flight; pulsing doom for the whiteout |
| sub.blue_below | Relent (56) | Harp and clarinet, very down: the ice caves |
| layer.whiteout | Spirit of the Girl (54) | Low drums and bells at the tundra's tempo, held low under the wind |
| camp.tundra | Water Lily (62) | Piano, basses, cellos: the lullaby |

**The borrowed-pool lands (L6): each its own explore, night and combat (boss phases stay as their cooks' stand-ins):**
| Land | explore | night | combat | mini |
|---|---|---|---|---|
| Desert (santur / qanun / drone) | Hidden Wonders (75), Opium (90) | Nightdreams (54) | Return of the Mummy (140) | Curse of the Scarab |
| Graveyard (organ, choir) | Halls of the Undead (90), Rites | Agnus Dei X (60) | Grim Idol (140) | Nightmare Machine (140) |
| Crystal (glass harmonica, harp) | Tenebrous Brothers Carnival - Mermaid, The Other Side of the Door (95) | Gone Beyond (118) | Serpentine Trek (75) | (keeps its own) |
| Sky (flutes, choir) | Angevin B (120), Willow and the Light (90) | Unpromised (56) | Journey To Ascend (116) | (keeps its own) |
| Ocean (strings, harp) | Finding Movement (70), Myst (85) | Plaint (65) | Crossing the Chasm (100) | (keeps its own) |
| Fungal (clarinets, mallets, drones) | Confused State (95) | Vulcan (78) | Ignosi (79) | (keeps its own) |
| Wasteland (atonal strings, waterphone) | This House (74) | Colorless Aura (50) | Unlight (151) | (keeps its own) |

Sub-zone pools of those lands (explore.desert.a etc.) point at the land's new pieces so no borrowed track plays as the
land's main theme. Generic `boss` / `combat` (synth chiptune / hybrid) leave the pools that used them (Vorhollow, Aeolith P3,
the island default) for real pieces: Vorhollow → Unlight's land combat set is wrong for a boss, so Vorhollow keeps its
stand-in marked; the island default `combat` / `boss` pools become The Descent / Colossus.

**Stingers** (cut from the pieces above by the build script, 2D, music category): `music.sting.combat.volcanic` (the first
taiko hit of The Descent's intro), `music.sting.combat.tundra` (a low drum hit of Gloom Horizon), `music.sting.combat`
(fallback), `music.sting.boss_fall` (the final cadence of Death of Kings).

### 4.3 Spaces: reverb by where you are (TLOU2, Valheim)

`sounds.json` `spaces` rows (decay s, density, diffusion, gain, gainHF, decayHFRatio, reflectionsGain, reflectionsDelay,
lateGain, lateDelay, airAbsorptionHF, wet, fade). Chosen from the listener's state each tick, highest first:
| Space | When | Feel |
|---|---|---|
| underwater | head under water (UnderBlend > 0.5) | 1.0 s, almost no highs (HF 0.08), very wet |
| vault | the Silent Vault (WRCrystal::InVault) | dry: reverb off (sound "dies") |
| drowned_hall | WROcean::InHall | 3.4 s, dark (HF 0.3), wet |
| coven | the Sunken Coven (SwampCoven > 0.5) | 2.2 s, wet peat, HF 0.35 |
| hollow_root | the forest's Hollow Root (ForestZone[6] > 0.5) | 1.6 s, woody (HF 0.45) |
| fungal_hall | Fungal F3 (Glow-moss Halls) | 4.5 s long tail (the cook's "long tails in the Halls") |
| fungal_web | Fungal F4 (Mycelium Webs) | 0.55 s, tight (the cook's "tight in the Webs") |
| fungal_cave | Fungal F2 / F5 / HG | 1.8 s |
| tube | inside a hidden tube (Kiln Roads / Blue Below) | 2.6 s, lava-tube basalt, long late tail |
| fold | inside a portal fold | 1.4 s ("through stone": L9) |
| cave | island caves (Cave > 0.5) | 1.7 s |
| room | under a built roof with walls round (traces up and to four sides hit build pieces within 6 m) | 0.45 s, small wooden room |
| (open air) | otherwise | reverb off |
Applied with `UGameplayStatics::ActivateReverbEffect` (one runtime `UReverbEffect` per row, tag `WRSpace`, the row's fade);
open air deactivates it. 3D sounds send to it by distance (attenuation reverb send 0.25 near → 0.6 at range); 2D music,
UI and beds stay dry.

### 4.4 Mix and dynamics (L13, D8, D9)

- A **master limiter** (AudioMixer dynamics processor, limiter, −1 dB threshold, 5 ms look-ahead, 120 ms release) added to
  the master submix once per audio device. Volumes are per-source gains (before it), as L13 asks.
- **Dynamic range: Wide / Night** (Settings > Audio, pref `AudioRange`): Night adds a compressor ahead of the limiter
  (−26 dB threshold, 3:1, 6 dB knee, 15 ms attack, 250 ms release, +7 dB make-up): quiet steps and calls come up, booms
  stay down.
- Big hits already carry ducks (hit.counter 0.29, crit.stab 0.37, stance.break 0.29, ...): within 2-4 dB after the cap.

### 4.5 Tells rise (D10)

Event rows may carry `rise: [fromPitch, toPitch, seconds]`. The subsystem keeps playing rising components and moves their
pitch each tick (ease-in, so the end of the tell is the steepest: the hit is coming). Rows: `creature.windup` [0.9, 1.15,
0.9], `ff.fumarole_tell` [0.85, 1.2, 1.0], `ff.geyser_tell` [0.9, 1.2, 1.0], `ff.serac_tell` [0.9, 1.1, 1.2],
`ff.gust_howl` [0.8, 1.25, 1.0], `ff.bridge_creak` [0.9, 1.1, 0.9]. `creature.windup` volume +3 dB over the bed rule (0.8 → 1.0).

### 4.6 Spot sounds (Krause; RDR2 ambient emitters): the island's life and geology

`ambient.spots` rows replace the hard-coded owl. Each: `event`, `layer` (any bed layer name), `min` (layer weight to
play), `every` [s], `dist` [m], `up` [m], optional `at` [x, y] m (the sound comes from that place, within `reach`).
| Spot | Layer | Why it's here / what it meets |
|---|---|---|
| amb.owl (kept) | night | Tawny owls off in the dark, as before |
| ff.spot.crater_chuff | ffzone.V5, ffzone.V2 | The crater (−786, 0 m) breathes: a gas chuff + infrasound rumble from the crater itself, so the plume you see is the sound you hear |
| ff.spot.rockfall | ffzone.V3, ffzone.V5 | Obsidian scree running off the ridges 40-90 m away, up-slope |
| ff.spot.crust_crack | ffzone.V2 | Cooling pāhoehoe cracking with a glassy tinkle 10-30 m off across the lava fields |
| ff.spot.mudpot | ffzone.V4 | Mud pots plopping 8-25 m off in the Solfatara between the fumaroles |
| ff.spot.raven | ffzone.V1 / ffzone.T1 by day | Ravens calling overhead 40-80 m out, 15-35 m up: the scavengers of open ground in both lands |
| ff.spot.lake_ice | ffzone.T3 at night | Lake ice singing and booming 60-150 m out as the night cools it (the Frozen Lakes) |
| ff.spot.frost_crack | ffzone.T2 at night | A pine splitting in the cold with a rifle-shot report 30-80 m off (the Pine Hollows) |
| ff.spot.ice_groan | ffzone.T4, ffzone.T5 | Glacier ice groaning 60-150 m out under the seracs and the plateau |
| ff.spot.howl_far | tundra.night | A far wolf howl 150-250 m off; captions say "Distant howl" (never a real pack's position) |
Layer names already exist (ffzone.<zone>, night, tundra.night, volcanic.day); a `layer` joined with `+day` / `+night`
multiplies by daylight / night.

### 4.7 Beds and hazards (L1, L2): real recordings

From Freesound CC0, built by `build_cookpass_audio.py` (beds: −23 LUFS, seamless equal-power loop, mono-to-stereo
decorrelated; one-shots: −16 LUFS by loudest 400 ms):
| Bed | Recording kind |
|---|---|
| ff.V1 Ash Plains | wind over open ground + distant rumble (kept: amb/volcanic_rumble under it) |
| ff.V2 Lava Fields | lava / magma bubbling and crackle |
| ff.V3 Obsidian Ridges | wind whistling over edges |
| ff.V4 Solfatara | fumarole / steam vent hiss with mud bubbling |
| ff.V5 Crater Highlands | low roar of a volcano / deep rumble |
| ff.T1 Snow Flats | steady cold wind with blowing snow hiss |
| ff.T2 Pine Hollows | trees creaking in wind |
| ff.T3 Frozen Lakes | lake ice sounds (booms / pings) |
| ff.T4 Ice Shelves | glacier ice creaking + meltwater trickle |
| ff.T5 Glacier Plateau | howling wind |
Hazards: fumarole hiss/roar, geyser gurgle/eruption, serac crack/fall (ice calving), ice crack/creak/break (lake ice
cracking, ice into water), bridge creak/fall/reform (snow collapse), gust howl, bellows whump (furnace / blowtorch whoosh),
cup light (match / torch ignite). Each row loses `_standin` once its recording is in.

### 4.8 Voices (L3, L4)

- **Sleep breath (L3):** `creature.small.sleep`, `creature.medium.sleep`, `creature.large.sleep` (real breathing / soft
  snoring), radius 900-1500 cm (heard only close, as in nature). WatchBodies plays one every `ambient.sleepEvery.<size>`
  (small 2.2 s, medium 3.4 s, large 5.5 s ± 20%) for an asleep creature within `ambient.sleepRange` (1200 cm). Families
  resolve as every voice does, so a species can get its own later.
- **Family alert / flee (L4):** real calls per family replace the pitched-idle fallback at family level:
  canine (alarm bark / yelp), herd (snort / bleat-bolt), flyer (alarm screech / wing burst), critter (squeak / squeal),
  beast (huff-growl / roar-away), bug (click / hiss). Rows `creature.<family>.alert|flee`. voiceFallback stays for the
  rest (boss, golem, wisp, humanoid).
- **Species' own calls and fused voices (L4 species level, L7):** NOT in this pass: a species-by-species recording job
  (≈ 150 species × 3 fused animals) for the voice pass; recorded on LATER-COOKS.

### 4.9 Sound captions (TLOU2, Fortnite)

- sounds.json event fields `caption` ("{who} snarls"; {who} = the creature's name) and `danger` (true: shown in
  Important). Shared voice rows and every tell / hazard / boss / explosion row get one.
- Never for the local player's own sounds or 2D sounds. Within the event's radius only; alpha falls with distance.
- HUD (new WRUICaptions.cpp, on the HUD overlay, bottom centre above the pickups): up to 4 lines, 2.6 s each with a 0.4 s
  fade, newest at the bottom, the same text merges (refreshes, "×2"); an 8-way arrow (←↖↑↗→↘↓↙ from the camera's yaw to the
  source) left of the text; danger lines in the ember colour, others ink. Plate: dark leather at 70% for contrast on snow.
- Setting: Settings > Audio > Sound captions: Off / Important / All (pref `Captions`, default Off), Segmented control
  (mouse click, pad cursor + A).

### 4.10 Settings page

Audio page: the five sliders, then "Sound captions" and "Dynamic range" rows; hint text now: "Music plays through the
whole game and dips at most 4 dB under the biggest moments." Volume sub-texts updated (music: "The menu theme, each
land's exploring, night, combat and boss pieces").

### 4.11 Edge cases decided

- Listen server / standalone: the director runs where there's a local player; dedicated server: nothing loads (kept).
- Paused (single player menu): play-time counting stops with the audio; bar sync stays true.
- A boss killed by someone else while you are in range: same fall stinger (the music followed it).
- A boss that despawns (leash reset) is not a fall: only `!IsAlive()` counts.
- Two spaces overlap: the table order is the priority (water first, open air last).
- Spot sounds never during the main menu, never inside caves unless the layer says so.
- Captions de-duplicate per event and source within 0.25 s (a stampede is one line).
- A pool row with no `bpm` behaves as before (immediate).
- Missing recording files: the table logs once and stays silent (existing rule), never crashes.

### 4.12 Build steps (each compiled, committed, pushed)

1. Doc (this) pushed.
2. Code part 1: director (4.1), spaces (4.3), limiter + Night (4.4), rising tells (4.5), spots (4.6), sleep breaths (4.8),
   captions store (4.9) in WRSound / WRAudio / WRAudioDirector; table rows that need no new files (gaps, duck cap, threat,
   spaces, rises, spots for existing files, caption text). Compile.
3. Code part 2: captions HUD widget (WRUICaptions.cpp + HUD slot), settings rows, prefs. Compile.
4. Content: Tools/audio/sources_cookpass.py + build_cookpass_audio.py; fetch, build; pools / beds / hazard / spot / voice
   rows; CREDITS; sizes reported.
5. Spot-check against this plan; ledger proofs; CHANGELOG + HANDOFF in each commit.

## 6. Spot-check (read against the plan and THE BAR)

- Found and fixed: the hidden tubes' pools were named `sub.kiln_roads` / `sub.blue_below`, but `AWRHiddenTube::TubeId` is
  the world.json id, so the director asks for `sub.kilnroads` / `sub.bluebelow` (commit 75a95c2).
- Checked: a pending change cancels if the want flips back before its beat; no tempo or no playing piece means
  immediate; forced music (WRMusic) is immediate; the boss fall only counts `!IsAlive()` (a leash reset is not a fall);
  `MusicBoss` clears on any non-fall switch; sleep breaths skip the first schedule (no breath the instant a body is
  seen); captions skip 2D sounds and the local pawn / its mount; spots never play in a planned silence, the main menu,
  or (for ffzone layers) inside caves (LayerWeight's `Out`); every file the table names exists on disk (script check);
  sounds.json parses after every edit (sj.py validates on save); merged master compiles (after the combat lane's push).
- Not verifiable here: how each recording / piece actually sounds in place (compile-only lane; Aaron listens, F8).

## 7. Ledger proofs (commits 5757249, 9cac442, 75a95c2 in RavenArcher17/Survival-Game)

| # | Done | Where |
|---|---|---|
| R1 | yes | this doc, section 2 |
| R2 | yes | section 3 |
| R3 | yes | section 4 (pushed before code: showcase-notes 4f48b80) |
| R4 | yes | commits above; remaining stand-ins stay `_standin` (Vorhollow, Aeolith P3, other lands' boss pools) |
| R5 | yes | section 6 |
| R6 | yes | this table |
| R7 | yes | spot rows carry `_why` (sounds.json ambient.spots); crater spot sounds from the crater (at [-786,0]); beds per zone |
| R8 | yes | sections 1 and 7 |
| R9 | yes | sounds.json `spaces`, `dynamics`, `captionDefaults`, `ambient.spots`, music numbers; no numbers only in code |
| R10 | yes | all audio client-side from replicated state (boss `bEngaged`/`Phase`, `AWRWorldState` events, `HitSourceShown` from the hit multicast WRCharacterBase.cpp:285); no audio on a dedicated server (unchanged guards) |
| R11 | yes | Segmented rows WRUISettings.cpp:1440-1450 (mouse click; pad cursor + A via WRPadInput) |
| R12 | yes | merged master compiled clean after the last push |
| R13 | yes | music filter in sources_cookpass.py header; synth `music/boss` and hybrid `music/combat` out of every pool |
| D1 | yes | music.gap [4,10] etc. (sounds.json), WRAudioDirector.cpp explore branch (~line 905), SwitchMode :1419 |
| D2 | yes | WRAudioDirector.cpp:124 (`music.duckMax` 0.37 everywhere), death dip :870 |
| D3 | yes (bosses of the borrowed lands still stand-ins) | sounds.json pools explore/night/combat.<land> for every land |
| D4 | yes | pools *.volcanic, boss.vulkarn.p1-3, sub.kilnroads, layer.eruption (WRAudioDirector.cpp:975), camp.volcanic |
| D5 | yes | pools *.tundra, boss.skaldrith.p1-3, sub.bluebelow, layer.whiteout, camp.tundra |
| D6 | yes | NextBoundary :1393, pending switch in UpdateMusic, boss phase on the bar |
| D7 | yes | build_cookpass_audio.py (music -18 / calm -21 / boss -16 / layers -24, TP -1) |
| D8 | yes | ApplyDynamics WRSound.cpp:1249 (per-source volumes ahead of the master limiter) |
| D9 | yes | existing hit ducks now capped 4 dB (:124) |
| D10 | yes | `rise` rows + StepRises WRSound.cpp:1225; creature.windup volume 1.0 |
| D11 | yes | Docs/CREDITS.md "Audio: the audio cook pass" (54 pieces, 72 recordings) |
| L1 | yes | beds ff.V10..T510 -> amb/ff_v1..t5 (real recordings) |
| L2 | yes | 16 ff.* events -> world/ff_* (real recordings), `_standin` removed |
| L3 | yes | creature.<size>.sleep rows; WRAudioDirector.cpp:1275 |
| L4 | partly | family-level real alert / flee calls (creature.<family>.alert|flee); species-level calls and per-species whistle answers NOT done (LATER-COOKS) |
| L5 | yes | layer.eruption / layer.whiteout (WRAudioDirector.cpp:975) |
| L6 | yes (licensed real pieces, not commissioned) | explore/night/combat for desert, graveyard, crystal, sky, ocean, fungal, wasteland |
| L7 | no | fused species voices: a species-by-species job, LATER-COOKS |
| L8 | no | proximity voice chat: needs EOS RTC, its own cook (LATER-COOKS) |
| L9 | yes | `fold` space (sounds.json spaces; SpaceNow WRAudioDirector.cpp:1529) |

Extra built from research: reverb spaces (UpdateSpace :1550), threat-gated combat (ThreatOf :1467), PvP combat music
(:1176), boss-fall stinger and hush (:1446), combat entry stingers, spot sounds (:1592), sound captions
(WRUICaptions.cpp, WRSound.cpp:1326), Night dynamic range.
