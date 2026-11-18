# ship-7: final report of the desktop lane

Copied 2026-11-18 from the desktop's lane log (TrailsResume/wild-logs/ship-7.out).

The package is still cooking. I'll continue when its background job reports back.

Release script is ready (draft first, publish only after every asset's digest matches). Waiting on the package.

Repackaging with the corrected exclusion (`f6cf8ed` pushed). Waiting for it to finish.

Packaging. I'll check the cook log for material failures and run the gate once it finishes.

Pushed `581e191`; packaging again. I'll check the cook log and run the gate when it's done.

Pushed `ff115f9`; packaging the final candidate into `WildreachBuild4`.

Uploading (runs in the background; it publishes only if every digest matches).

The draft is being created and the 3.6 GB uploaded now (output arrives at the end). Waiting for it.

Uploading the remaining assets; the release stays a draft until I verify every digest.

Pushed `0f4fdd1`. Waiting for the live install to finish before sending the release emails.

LANDED ship-7 `380b236 c2f03aa f6cf8ed 1bcdeab 581e191 ff115f9 0f4fdd1`

**Wildreach 0.8.0 is live and both release emails went out.** The playable gate caught real bugs that would have broken the release, and they are fixed in the shipped build.

**Release**
- **Version:** 0.8.0, the next after 0.7.1. It's the whole game, so a minor bump rather than 0.7.2. The question page asks Aaron whether that's the right call.
- **URL:** https://github.com/RavenArcher17/Survival-Game/releases/tag/v0.8.0. It targets `ff115f9`, and `releases/latest` points to it.
- **Assets:** the setup is 3.6 GB and GitHub takes nothing over 2 GiB per file. So the release holds the setup in two parts (1.99 GB and 1.76 GB), a list of the parts, and a 28 KB `Wildreach-Setup.exe` that downloads the parts, checks them and installs. The digest of each of the 4 files on GitHub matches its local SHA-256.
- **Live check:** I downloaded the real `Wildreach-Setup.exe` from the release. It fetched 3,581 MB, checked the hash and installed 0.8.0 (exit 0, 135 s).
- **Players on 0.7.1:** their current updater runs the small download program, so they still update by themselves. It downloads the whole game once. I tested this offline with the real 0.7.1 updater (8 of 8 checks pass), and the repo's updater test still passes 44 of 44.

**Playable gate** (on the installed copy of the final package; the log lines come from markers I added):
- New Game: `F35READY menu`, then `F35BOOT new world 'Boot_check'`, then `F35READY play` 35 s after launch. Still running 4 minutes later.
- Continue on that save: `F35BOOT continue world 'Boot_check'`, then `F35READY play` after 30 s. Still running 2 minutes later.

**Fixed on the way.** The editor builds the lanes used never showed any of these; only the packaged game did.
- The packaged game crashed before it wrote a single log line. The map code read the game's settings before the game could load them.
- It crashed 11 s into play: a forest creature's mesh copied an array item into the same array. The same mistake was in the map's line drawing.
- It crashed 13 s into play: the achievements code read from data that had already been freed.
- Ten materials never compiled on UE 5.8 and would have drawn plain grey: all trees, the newer lands' ground, held tools, tame looks, fungal glow and veil, and the hero's armour and hair cards. I fixed the scripts that build them so a bad connection now stops the script instead of passing silently.
- Fixed a crystal-cave warning: the floor cracks stayed floating when the floor broke.
- Excluded 630 MB of engine content the game never uses.

**Emails and pages**
- **Release email to Aaron:** ok (id `01a1e3a9-5ef7…`).
- **Release email to Raven:** ok (id `01a1e3a9-5ffd…`). Neither mentions god mode or the questions.
- **Questions page (Aaron only):** https://freedom35studios.pages.dev/review/wildreach-open-questions/. It has 15 cards: the 3 open items from the questions file, plus picks the recent lanes made (siege, multiplayer, UI, world) and the version number. The 20 already-answered questions are left out. A test answer was saved, read back word for word, then blanked. The link went to Aaron only (ok, id `01a1e3a9-741e…`).
- **Proof files:** `RELEASED.txt` and `EMAILED.txt` are in `wild-desktop\`.

**Requirements**

| Requirement | Done? | Where |
|---|---|---|
| Gate: 12 biomes, all cookpass lanes, god mode, wild-answers-1115 landed | Done | All their commits are on origin/master |
| No visible music UI | Done | Nothing found. The forge bellows puzzle follows a hammer, not the soundtrack |
| Package a Development build | Done | `E:\Dev\WildreachBuild4`; only the newest two build folders kept |
| Feature list in `build.json` | Done | Lists 12 lands, 12 paths, 207 creatures, 1,092 items |
| Release published and verified | Done | See Release above |
| Playable gate | Done | See Playable gate above |
| Proof files and emails | Done | See Emails and pages above |

**Not finished**
- **No cook research doc:** I didn't write a cook research doc or requirement ledger in showcase-notes for this lane.
- **Shader cache:** the bundled shader cache wasn't recorded. It needs a real play session first.
- **Non-fatal log errors:** about 43 `Json Value of type 'String' used as a 'Object'` errors show up in play logs and haven't been traced to their source.
- **Hair-card motion:** it now follows the card's own texture layout instead of the hair data. Nobody has looked at it yet.
- **The hero material script** isn't in the repo, so those two materials were repaired by hand.
- **Not played:** no person has played this build.

These three are now lines in LATER-COOKS: the play-log errors, the shader cache, and the hair cards.

**New binaries:** 21 material `.uasset` updates, about 670 KB in LFS. No textures.
EXIT 0
