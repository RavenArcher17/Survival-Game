# cookpass-audio: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-audio.out).

LANDED cookpass-audio 5757249 9cac442 75a95c2

The audio cook pass is on master in three commits. Merged master (including the combat lane's push that landed in between) compiles clean. It's compile-only: nothing was played or heard, so how each piece and recording sounds in place still needs your ears (F8).

The cook doc with the ledger, research, grades, plan, spot-check and proofs is `showcase-notes/notes/Wildreach/cooks/cookpass-audio.md`, pushed as 4f48b80 (plan, before any code) and 38a6a07 (proofs).

**What changed**
- **Music director.** Music now plays the whole time, with a few seconds between pieces instead of 2–5 minute silences. Changes wait for the playing piece's next beat (going into a fight, under a drum hit) or next bar line (everything else). Every duck is capped at 4 dB. Combat music starts only when the creatures on you add up to a real threat, or someone hits you while you're hunted, or in a fight with another player. When a boss dies its piece ends on the bar, a held brass chord plays, then a planned 12 s hush. The Kiln Roads and Blue Below have their own pieces, and the Eruption and Whiteout events add a music layer from their warning.
- **Spaces and mix.** Reverb now depends on where you are: caves, the hidden tubes, the Fungal halls and webs, the Crystal caves, the Coven, the Drowned Hall, portal folds, a room you built, under water. Open air stays dry. A master limiter at −1 dB is always on. A "Night" dynamic range setting compresses the mix.
- **Smaller additions.** Attack wind-ups and hazard warnings now rise in pitch. Ambient one-shots (owls, the crater, ravens, lake ice and so on) are table rows. Sleeping creatures breathe.
- **Sound captions.** Settings > Audio > Sound captions: Off / Important / All. Up to four lines bottom centre, each with an arrow toward the sound. Mouse and pad both work.
- **Content.**
  - 54 Kevin MacLeod pieces (CC-BY 4.0), real instruments only: anything listing synths, electric instruments, drum kit or guitar was filtered out.
  - They give Volcanic and Tundra their full music sets, and every land that borrowed music its own explore, night and combat pieces.
  - The synth tracks are out of every pool.
  - 72 Freesound recordings (66 CC0, 6 CC-BY 4.0) replace the stand-ins for the 10 Fire/Frost sub-zone beds and 16 hazard sounds. They also add 9 spot sounds, sleep breaths, the six creature families' alarm and bolt calls, and the stingers.
  - Everything is credited in `Docs/CREDITS.md`.

**Requirement ledger**

| # | Requirement | Done | Where |
|---|---|---|---|
| R1–R3 | Research, grades, plan before any code | yes | doc §2–4; plan pushed as 4f48b80 |
| R4–R6, R8 | Build to plan, spot-check, proofs | yes | the commits; doc §6–7 |
| R7 | Every placed sound designed with intention | yes | each spot row has a `_why`; the crater sound comes from the crater itself |
| R9 | Data-driven | yes | sounds.json `spaces`, `dynamics`, `captionDefaults`, `ambient.spots`, music numbers |
| R10 | Multiplayer / server authority | yes | client-side, from replicated state; who dealt a hit is read from the hit multicast, `WRCharacterBase.cpp:285` |
| R11 | Mouse and pad on new settings | yes | `WRUISettings.cpp:1440-1450` |
| R12 | Master boots | yes (compiled) | merged master compiles clean |
| R13 | No synth music | yes | filter in `Tools/audio/sources_cookpass.py` |
| D1 | Music always audible | yes | music gap breaths; `SwitchMode` at `WRAudioDirector.cpp:1419` |
| D2 | Ducks ≤ 4 dB | yes | `WRAudioDirector.cpp:124` |
| D3 | Every land: explore, night, combat, boss | yes | music pools per land |
| D4–D5 | Volcanic and Tundra cue sheets | yes | 26 cues, music pools |
| D6 | Combat in on the beat, boss phases on the bar | yes | `NextBoundary` at `WRAudioDirector.cpp:1393` |
| D7 | Loudness targets | yes | `Tools/audio/build_cookpass_audio.py` |
| D8 | Master volume before the limiter | yes | `WRSound.cpp:1249` |
| D9 | Biggest hits duck 2–4 dB | yes | existing hit ducks, now capped at 4 dB |
| D10 | Rising tells | yes | `WRSound.cpp:1225` |
| D11 | Credits | yes | `Docs/CREDITS.md` |
| L1–L2 | Real Fire/Frost beds and hazard sounds | yes | sounds.json rows, stand-in marks removed |
| L3 | Sleep breaths | yes | `WRAudioDirector.cpp:1275` |
| L4 | Species alert/flee/answer calls | partly | real calls per family only; each species' own calls are not done |
| L5 | Eruption / Whiteout music layers | yes | `WRAudioDirector.cpp:975` |
| L6 | Borrowed lands get their own music | yes | licensed real pieces, not commissioned |
| L7 | Fused per-species voices | no | added to LATER-COOKS |
| L8 | Proximity voice chat | no | added to LATER-COOKS |
| L9 | Portal "through stone" reverb | yes | `fold` space |

**Not finished**
- Each species' own calls and the fused per-species voices.
- The boss music for the borrowed lands, plus Vorhollow and Aeolith phase 3, are still marked stand-ins.
- Combat music switches to a second piece on the beat; it isn't true layered stems in the same key, because these pieces have no stems.
- Proximity voice chat.

All four are on LATER-COOKS.

**New binary files:** 157 OGG files, about 98 MB in Git LFS (102 MB uploaded). Music is about 90 MB of that.
EXIT 0
