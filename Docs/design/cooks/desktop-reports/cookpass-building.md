# cookpass-building: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/cookpass-building.out).

LANDED cookpass-building d98a437 ebbb16d 6597147

All three parts compile (editor target) and are pushed to master, including after the final pull. **None of it has been played.** I didn't run any tests, as the brief said. The research, the grade of what was already built, the plan and the proof table are in `showcase-notes/notes/Wildreach/cooks/cookpass-building.md`. I wrote and pushed that doc before building anything.

**What's new**
- **d98a437, build mode:**
  - **Pad controls:** RT places, LB/RB turn the piece (holding repeats), the D-pad picks the piece and the category, X copies. Before this, RB placed and the D-pad still fired abilities.
  - **Copy:** middle mouse (or X on a pad) picks up the piece you aim at, turned the way it is.
  - **Survival tab:** the build bar opens on it until you own a claim, then on the tab you used last.
  - **Placement chip:** says why a piece won't go in 1–4 words ("Missing 3 Wood", "Needs support", "Someone's claim"), with the full reason underneath.
  - **Chests pay:** your chests within 20 m inside your claim count toward a piece's cost (the bag pays first).
  - **Lean-to:** a new piece and the first night's roof; a bedroll under it counts as under a roof.
- **ebbb16d, raiding and bodies:**
  - **Area spells hit buildings:** eight area spells from the newer paths (Thunderclap, Arcane blast, Starfall, Corpse explosion, Collapse, Maw, Sunburst, Blight) now damage buildings. Until now only Fire/Frost spells and bolts did, so the lightning/spore/void/arcane raid bonuses never applied.
  - **Ladder climbing:** climbers go hand over hand in time with the rungs instead of hanging in the idle pose.
  - **Hot spring bath:** it can now be placed at the tundra's Steam Ground as well as by lava.
- **6597147, siege engines:** the battering ram, trebuchet and ballista. They were specced but never built. Each is an item you set down and crew at its posts, with every moving part modelled and moving smoothly. They are saved with the world, and the Siege engineer perk now works.

**Requirement ledger** (file:line for each row is in section 7 of the cook doc)
- L1 research: done (doc section 1)
- L2 plan before building: done (sections 3–4, pushed first)
- L3 grade of built pieces: done (section 2)
- L4 build at full detail: done (the three commits above)
- L5 spot-check: done (section 6; eight real problems found and fixed)
- L6 proof for "already done": done
- L7 design with intention: done (sections 3.3 and 3.6)
- L8 engineered machines: done (`WRSiegeLook.cpp`)
- L9 smooth motion, no snapping: done (`WRSiege.cpp:937`)
- L10 ledger with proof: done
- L11 no music on screen: nothing found in my scope (UI files searched; captions cover only world sounds)
- L12 data-driven: done
- L13 damage through ApplyHit: done
- L14 mouse and pad: done
- L15 sounds and effects as table rows: done
- L16 server authority and replication: done
- L17 nothing unfinished shown: done
- L18 compile gate: done
- L19 git rules: done
- L20 sizes, LANDED, LATER-COOKS: done
- L21 battering ram: done (`WRSiege.cpp:590`)
- L22 trebuchet: done (`WRSiege.cpp:742`, arc drawn at `:1133`)
- L23 ballista: done (`WRSiege.cpp:810`)
- L24 haulers push rams: done (`WRSiege.cpp:506`, `:1382`)
- L25 Aegis stops bolts, a stone breaks the dome: done
- L26 Siege engineer perk: done (`lifeskills.json:87`)
- L27 every path raids: done (7 call sites across 6 files)
- L28 pad build mode: done
- L29 copy: done
- L30 building from chests: done
- L31 Survival tab: done
- L32 short chip: done
- L33 lean-to: done
- L34 hot spring: done
- L35 climbing pose: done
- L36 crew poses: done

**Decisions I made that you may want to change**
- **Packing up** needs Dodge pressed twice, so a reflex dodge mid-raid never packs a ram away.
- **On PvE worlds** the ram and trebuchet can't be placed. The ballista can, and players can't damage it there.
- **Ballista vs players:** players take the normal PvP share of its 220 damage (about 130), not the spec's separate 120.
- **Trebuchet stone** is 70 cm across; I read the spec's "2 m" as the size of the area it hits.
- **Area spells left off walls:** Blood boil, the Tide water fields and Solar flare don't damage buildings, because none of them is a physical blast.

**Not finished**
- Nothing here has been played.
- The siege sounds reuse existing recordings, pitched down.
- The climbing and crew poses are built in code; there are no animation clips for them.
- Tames can't be sent to attack siege engines.
- Crafting from chests is still the crafting cook's job.

I added those last three to LATER-COOKS.

**New binary files:** none (0 bytes).
EXIT 0
