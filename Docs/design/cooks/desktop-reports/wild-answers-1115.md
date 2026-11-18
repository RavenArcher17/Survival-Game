# wild-answers-1115: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/wild-answers-1115.out).

LANDED wild-answers-1115 654c783 857f3b1 b99fa88 415bc08 9de926e 2001108

All of Aaron's switch and "make it good" answers are built and pushed to master in six commits. The editor target compiled clean after every part. I ran no tests and played nothing; Aaron tests.

To get the real questions, I read the live card page using the admin token, since the share didn't have them. The full text of all 20 cards is now in `cooks/wild-answers-1115.md`, along with the requirement ledger, research, plan and proof. That doc is pushed to showcase-notes as 149a464.

**Requirement ledger (all done):**
- **R1, read every card:** cards copied into section 0 of the cook doc.
- **R2, research and plan before building:** cook doc sections 1–4, committed before any code.
- **R3–R4, god mode by typing `god-mode` in chat:** works for the three server-verified testers only (`WRGod.cpp:628`, `WRPlayerController.cpp:104`). From anyone else it's an ordinary chat line. Typing it again turns everything off. F9, the badge and all `god`/dev commands now wait for it. (654c783)
- **R5–R6, mod mode on public servers:** god mode is no longer refused on public servers. A tester in god mode can also kick, ban and make admins like a host (`WRAccess.cpp:247`). (654c783)
- **R7–R9, flint:** a new item with its own icon. Each stone hit has a 15% chance of also giving 1–3 flint, higher on a heavy blow or with a pick, and never from ice rock (`WRResourceNode.cpp:185`). Hush arrows now cost flint, and there's a new flint arrow. (654c783)
- **R10–R11, forest ground:** the Electric Dreams pack is not on this PC. It was studied on the laptop and deleted, and I searched Content, the launcher cache and every drive. I used its lessons plus Trails' free-to-use forest scans (leaf litter, soil, moss) and the existing grass/stone/sand textures. The new `M_ForestGround` blends six layers by height, puts rock on steep ground, and grows moss where the ground meets rocks and trunks. Layer mixes per area are in data (`WRLand.cpp:829`). The forest's height data is unchanged. (415bc08)
- **R12–R13, food:** eaten with the right hand; the weapon is put away for the bite and the shield or torch stays. Food is used from the quick-use bar, and the item card says which key eats it. Nothing is ever equipped to eat (`WRHeroAnimInstance.cpp:499`, `WRHeldFood.cpp:465`, `WRUIInventory.cpp:1786`). (857f3b1)
- **R14–R15, levistone moss:** beside every keel outcrop a mossy lump of levistone floats on its own lift. It has modelled cushions, hanging beards, crystal underneath, and a cyan glow at night. Hold E to peel it. It's the Cloudback's favourite food (`WRSkyMoss.cpp`, `WRSkyWorld.cpp:1356`). (654c783)
- **R16, pack-up:** now "hold Interact 1.5 s" next to your own engine, with a bar filling under the prompt. A tap still takes or leaves a crew post. The server times the hold and refuses it (with the reason shown first) if the engine is damaged, someone else is on it, or it was hit. Dodge no longer packs. (b99fa88)
- **R17–R18, siege on PvE, NPC strongholds:** siege engines now work on every world. On PvE worlds they still can't hurt players, their tames or their buildings. There were no strongholds, so I built two: Emberwatch (Ash Plains) and Rimehold (Snow Flats). Each sits on the flattest clear ground picked from the heightmap and has:
  - a log wall with a barred gate
  - two towers, each with a bolt-thrower that shoots players within 30 m
  - a stone keep whose war chest opens when the keep is broken into
  - the land's beasts guarding the yard

  They rebuild after 12 game hours if nobody is near, and what's broken is saved with the world (`WRStronghold.cpp`). (b99fa88)
- **R19, Wet:** switched off with one data setting (`wet.enabled` 0), so the design passes can turn it back on unchanged. (654c783)
- **R20, Kyrra:** her dive now climbs to a point above and behind you during the wind-up, then locks her line on your position. She folds her wings, screams, dives steeply, and lies spread on the ground for her 1.6 s punish window (`WRSkyBosses.cpp:944`). (9de926e, 2001108)
- **R21, Sunwatch lens:** stays working from the bag. In a sandstorm the compass now says "no sun to read" and that the lens would show it, instead of just vanishing. When the lens or a Scarab is what's keeping it, a gold line says so (`WRDesertHazards.cpp:227`). (9de926e)
- **R22, wall recoil:** kept on, with one setting to turn it off if corridors feel bad (`WRAbilities.cpp:1514`).
- **R23, talismans:** kept one effect each, which matches 50 of the 56 talismans; all three sky effects are confirmed wired.
- **R24, question cards:** the full-context, full-sentences rule is at the top of the share's QUESTIONS-FOR-TOMORROW.md, in `Docs/HANDOFF.md` and in the cook doc.
- **R25, not a music game:** nothing in this lane shows music or asks you to play to it.
- **R26–R30, process rules:** all met. Numbers are in data, the server decides everything, and the new holds work on mouse and pad. The changelog and HANDOFF were updated in every commit. The LANDED, LATER-COOKS and QUESTIONS lines are on the share.

**Not finished:**
- The Electric Dreams assets themselves aren't in the game. I put a question on the share about installing the pack.
- The strongholds are held by beasts, not people, because there are no human enemy models outside the bosses. Also asked on the share.
- A moss bed you've already gathered still looks full to you until it regrows.
- The forest's contact moss won't appear on graphics settings without distance fields.
- Nothing has been played; everything is compile-only.

**New binaries:** 3 icons (32 KB); 6 forest textures plus `M_ForestGround` (19.7 MB, LFS).
EXIT 0
