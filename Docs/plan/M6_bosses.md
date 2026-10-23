# M6: Bosses

**Goal:** two major bosses and four mini-bosses that are fun alone and in groups.

**Gate:** a group of 3 (plus a tame) beats Vulkarn and Skaldrith; each player who dealt ≥ 10% gets their
own loot; a solo player can beat each mini-boss with the intended counterplay.

Depends on: M1 (combat), M3 (arenas), M4 (tames as +1 player for scaling), M5 (loot items, Hearth cores).

## Shared boss framework
| System | Detail |
|---|---|
| `AWRBossBase` | Pawn with its own ASC; phase machine driven by HP thresholds; StateTree per phase |
| Arena | `AWRBossArena`: entry trigger, combat lock (optional fog wall), resets when empty, respawn timer, altar actor inside |
| HP scaling | At engage: HP × f(participants), each tame counts as a player; re-scales if someone joins mid-fight |
| Damage ledger | Server tracks damage per character; ≥ 10% of max HP → personal loot roll |
| Loot | Major: boss core, path materials, chance at a boss part. Mini: upgrade shards + trophy. Personal drops spawn as owner-only loot bags. |
| Death & rejoin | Dead players respawn and can run back in (the arena stays open) |
| Hidden phase 3 | Triggers only if the party has artifact modifiers active. Artifacts are Full release, so phase 3 is built and hidden behind a debug flag. |
| Telegraphing | Every big attack has a readable windup (anim + sound + ground decal) |

## Vulkarn, the Forge Giant (volcanic)
| Part | Build notes |
|---|---|
| Arena | Forge cavern with raised stone anvils. Lava flood = timed damage volume across the floor; anvils are safe platforms. |
| Phase 1 | 3-hit hammer combo; overhead slam leaving lava cracks (persistent damage decals); tongs grab (grab attack, parryable → stagger); anvil slam → floor flood |
| Phase 2 (< 50%) | White-hot armor (VFX + damage aura); bellows breath cone; thrown slag projectiles; floods more often; anvils melt (destructible platforms, fewer each flood) |
| Hidden phase 3 | Plunges into lava, returns dual-wielding; every slam floods |
| Weakness | Frost; parrying the tongs grab staggers him |
| Drops | Boss core, fire path materials, boss part *Giant's ember* (staggering an enemy sends out a small lava shockwave) |

## Skaldrith, the Frost Wyrm (tundra)
| Part | Build notes |
|---|---|
| Arena | Frozen mountain bowl with tall ice pillars. Pillars block breath (line-of-sight check), and his tail and charges shatter them (destructible). |
| Phase 1 | Frost breath sweep; claw swipes; pillar-breaking tail sweep; lunging bite |
| Phase 2 (< 50%) | Takes flight and dives (flying movement + dive attack); air breath leaves slick ice; ice spikes from the floor; few pillars left |
| Hidden phase 3 | Whiteout blizzard (fog + reduced visibility), hides in snow, breath freezes on contact |
| Weakness | Fire; hitting his wings while airborne grounds him (wing hitboxes with their own poise) |
| Drops | Boss core, frost path materials, boss part *Wyrm scale* (blocking an attack can freeze the attacker) |

## Mini-bosses
| Mini-boss | Biome | Counterplay to build |
|---|---|---|
| Obsidian Warden (golem) | Volcanic | Armored shell (damage reduction) → break it to expose core; Chill on the shell speeds cracking |
| Ignessa, Queen of Wisps | Volcanic | Flies out of reach, spawns exploding wisps; when wisps run out she lands to make more (the opening) |
| Grimjaw (frost troll) | Tundra | Regenerates unless Burning or Bleeding |
| The Pale Hunter (snow stalker) | Tundra | Invisible during blizzards; footprints in snow show it; fire or lightning hits reveal it |

All mini-bosses live in the midlands per the Map tab. Trophies: Obsidian heart, Wisp crown, Troll marrow, Stalker fang.

## Art & animation
Vulkarn and Skaldrith are the most expensive assets in the First Playable. Plan: build both fully on greybox
proxies (scaled capsules + simple rigs) until the fights pass playtests, then commission/kitbash.
