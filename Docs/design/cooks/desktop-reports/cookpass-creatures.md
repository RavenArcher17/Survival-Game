# cookpass-creatures: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-creatures.out).

LANDED cookpass-creatures 2e77a00 02063ce b8ca56d 1c42d17 c44214f 8039632 (plus the cook doc in showcase-notes: 723098a for the plan, 91f43d8 for the final)

The creature cook pass is in master as six commits. Each one compiled clean on the editor target and was pushed to master with CHANGELOG (Unreleased) and HANDOFF updated. I haven't played any of it or run any tests. The doc is `showcase-notes/notes/Wildreach/cooks/cookpass-creatures.md`. It has the requirement ledger, the research, the grades, the plan, a read-through check and all 202 creature rows graded.

**What changed**
1. **Body language** on every creature and boss that has a skeleton (`WRCreatureBody.cpp`, applied in `WRAnimInstance.cpp`):
   - The head turns toward what the creature is paying attention to: its target, the person it's wary of, its owner, a nearby survivor now and then, or where its rider is looking.
   - Every hit that lands makes the body flinch away from the side it came from.
   - Creatures pant after running or fighting, carry the head low below 35% health, slump during a stance break, and droop the head when asleep or (for tames) hungry.
2. **Moods** decided on the server and replicated to every player:
   - When idle, a creature does something that fits its diet: grazing (with head-up checks), sniffing, scanning or preening.
   - Passive and neutral animals stop and stare at a nearby survivor. A passive one bolts when someone comes straight at it faster than a slow walk, and its herd follows a moment later. Walking slowly up to offer food still works.
   - An aggressive creature holds a short threat display (0.4–1.2 s) before its first charge. It's cut short if you close in or hit it.
3. **Attack gestures:** each attack's motion (bite, sweep, slam, howl and so on, 28 in all) now bends the body differently over the creature's single attack clip, so different attacks no longer look identical.
4. **Voices:**
   - Each animal's voice is pitched slightly differently from others of its kind, so a pack no longer sounds like clones.
   - There are new alarm, flee and answer calls, plus packmates answering each other's calls. The whistle and "Call your mount" now use the answer call.
5. **Proportions:** 73 species that share a mesh now get their own head, neck, tail and wing proportions, each based on its description in `creatures.json`.
6. **Two fixes found while reading the code:**
   - The attack wind-up glow was erasing overlay looks when it ended. That stripped the colouring from 13 portal creatures on shared rigs and the mutations from every mutated tame after their first attack. It now puts the overlay back.
   - Raised minions of kinds with no colour scheme of their own now turn bone-grey instead of keeping their living colours.

**Requirement ledger**
| # | Status | Where |
|---|---|---|
| Q1 research | done | doc section 1 |
| Q2 grade everything | done | doc sections 2 and 6 (all 202 rows) |
| Q3 plan before building | done | doc section 3, pushed in 723098a before any code |
| Q4 build to plan, nothing unfinished shown | done | the six commits; sleep stays silent rather than using a stand-in sound |
| Q5 check against plan and bar | done | doc section 4; two problems found and fixed |
| Q6 file:line proof | done | every ledger row in the doc |
| Q7 designed with intention | done | each pose, motion and shape carries its reason in code comments and data notes; feet are never touched; nothing snaps |
| Q8 data-driven | done | `tuning.json` (`anim.*`, `creatureAI.*`, `necromancy.raisedLook`), `sounds.json` (`voiceFallback`, `ambient`), `creatures.json` (`shape`) |
| Q9 multiplayer | done | moods set only on the server (`WRCreature.cpp:3202`, `:3264`, `:3323`) and replicated; gestures sent to everyone (`WRCreatureCombat.cpp:730`); flinch from the replicated hit (`WRCharacterBase.cpp:283`) |
| Q10 mouse and pad screens | not applicable | no screens in this lane |
| Q11 land in pieces | done | six compiled, pushed commits with CHANGELOG and HANDOFF |
| Q12 unity build / git rules / boot-safe | done | own namespaces, rebase only, files added by name, no assets or maps touched |
| Q13 share notes | done | LANDED line and 3 LATER-COOKS lines written |
| L1 distinct attack motions | partly (procedural) | `WRCreatureCombat.cpp:730`, `WRCreatureBody.cpp:477`; real per-attack clips are still an art job |
| L2 stance-break slump | done (procedural) | `WRCreatureBody.cpp:404` |
| L3 hungry look / grazing farm animals | done | `WRCreatureBody.cpp:466`, `WRCreature.cpp:3729` |
| L4 each species' own whistle answer | partly | answer call wired (`WRTaming.cpp:2459`, `WRPlayerMenus.cpp:1094`); each species' own recording is still needed |
| L5 Rimeclaw / Glacimaw movesets | already built | `attacks.json` line 70 |
| L6 raised minions keep their colour | done | `WRGraveCreatures.cpp:414` |
| L7 one shared hearing model | not built | still on LATER-COOKS |
| L8 recorded voices and composed music | not built | needs recordings |
| L9 sculpted bodies | not built | needs the art pipeline |
| L10 riding / courtship clips | not built | needs art |
| G1–G9 grade findings | done | file:lines in the doc ledger |

**Not finished**
- The 34 code-built bodies (desert, ocean and sky creatures) get none of the body language.
- Bosses get head aim and flinch, but no attack gestures.
- The new alarm, flee and answer calls play stand-in sounds until each species has its own recordings; sleep is silent.
- Real per-attack and riding clips, sculpted bodies, and one shared hearing model are not built.
- Eleven shared-mesh species have no description to design proportions from, so they keep the base mesh's proportions.

New binary files: none (code and JSON only, 0 bytes added to LFS).
EXIT 0
