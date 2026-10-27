# Credits and third-party assets

Each area of the game lists the outside files it uses: where they came from, who made them and the licence.

## Animation

| What | Files in the repo | Source | Author | Licence |
| --- | --- | --- | --- | --- |
| Survivor animations: the Sword And Shield pack (idles, walks, runs, strafes, walking / running back, turns, slashes, attacks, blocks, casts, hits, deaths) and singles (dodge roll, overhead chop, gathering, pick up, swimming, treading water) | `Content/Characters/Survivor*/survivor_*Armature_*.uasset` | https://www.mixamo.com | Adobe Mixamo (motion capture) | Mixamo terms of use: royalty-free in personal and commercial projects, including games; the raw files may not be redistributed on their own |
| Survivor ranged animations from the Longbow pack: bow idle / walk / run / equip / draw / aim / recoil / aim-walk, and (added in the 0.4.0 polish pass) bow walk and run back / left / right, aim-walk back / left / right and turn 90 left / right; rifle aim / fire / reload; throw | `Content/Characters/Survivor*/survivor_*Armature_Bow*.uasset`, `..._Rifle*`, `..._Throw` | https://www.mixamo.com (downloaded on Mixamo's stock Y Bot, retargeted to each survivor with `Tools/art/survivor_build.py`) | Adobe Mixamo | Mixamo terms of use, as above |
| Humanoid boss animations (Creature Pack: idles, walking, run, swiping, punch, jump attack, roaring, dying) | `Content/Creatures/Vulkarn`, `Grimjaw`, `Obsidian_warden` | https://www.mixamo.com | Adobe Mixamo | Mixamo terms of use, as above |

The creature animations made by `Tools/art/rig_quadruped.py` and `rig_special.py` are procedural (written for this
game); the pack creatures (Gloomhowl, Scorchwing, Skaldrith) use the animations that came with the QuadrapedCreatures
Fab pack, which is not in the repository.
