# How Rocket Crowbar differs from Half-Life

Everything Rocket Crowbar 1.9 changes about Half-Life deathmatch, as the rebuild plays it. The
cvars and commands named here are in [SERVER.md](SERVER.md); every number marked "rc.ini" can
be changed there ([RC_INI.md](RC_INI.md) lists each key and its default). Where the rebuild
differs from RC 1.9 itself, [reference/DEVIATIONS.md](../reference/DEVIATIONS.md) says so, and
[reference/ORIGINAL_BUGS.md](../reference/ORIGINAL_BUGS.md) records RC's own bugs and which
were kept. The per-file detail is in [reference/dossiers/](../reference/dossiers/).

RC has no client library: it runs on Half-Life's own client, with RC's models, sounds and
sprites. All of the below is server side.

## Weapons

Every weapon keeps its Half-Life model and slot and does something else. Damage, refire times,
ammo and limits all come from rc.ini, one heading per weapon.

```
weapon        primary                        secondary                      reload
crowbar       swing (35)                     swing and fire a rocket; some  shove things away, or
                                             turn back on the shooter, and  bounce yourself off a
                                             wobble when "drunk"            wall
glock         a bouncing acid gib (30)       a small blast out the back     -
python        a laser-guided rocket (80)     six rockets at once, four of   -
                                             them corkscrewing
mp5           a lightning beam that forks    a blast out the back that      -
              to targets near where it       launches you (AR grenades)
              misses
shotgun       throw a screaming scientist    open a space rift on a wall:   -
              that explodes on touch         it sucks in and gibs, then
                                             blows (mp_allowrifts)
crossbow      the impaler: skewers a player  zoom; zoomed fire is a sniper  -
              and drags him, bleeding, on a  shot (4000) that bends off
              rope, pinning him to walls     reflectors
rpg           launch a scientist or Barney   a suck mine that ropes a       -
              bomb that greets whoever       target and pulls it in
              comes near, then explodes
gauss         beam: teleports what it hits   throw a beacon; with one out,  retrieve the beacon, or
              to your beacon                 teleport yourself to it        without one teleport
                                                                            to a spawn point
egon          tractor beam: grab and carry   an IsoBomb that sticks to      a mirv that splits
              things                         walls and blasts through them  into fragments
                                             (mp_allowiso)
hivehand      freeze gun: charge and fire a  a freeze grenade               zoom
              beam through walls; what it
              hurts is frozen for seconds
hand grenade  an implosion grenade: rises,   a friction grenade: whoever    -
              pulls everything in, then      it hits slides and glows
              blows (120)
satchel       throw a roach bomb that runs   the radio: arm the roaches,    -
              at the nearest living thing    3 s to the blast
tripmine      plant a mine whose laser       throw a headcrab on a rope     -
              burns and bounces off some     that sticks and pulls you to
              surfaces                       it; again to set it off
snark         snarks that also shoot lasers  stick a snark cam to a wall;   look through a cam and
                                             in a cam, next cam             back; fire in a cam
                                                                            shoots its laser
```

Also:

- `sv_ceasefire 1` stops every weapon.
- Dropped weapon boxes vanish after 30 s.
- A picked-up hivehand comes full (24) in multiplayer.
- There's no client weapon prediction. RC 1.9 was built without it, so the server sends every
  animation and sound. A client with `cl_lw 1` predicts Half-Life's own weapons and shows them
  wrong locally; that was true of RC 1.9 as well.

## Specials

- **Nuke** (`nuke`, `mp_allownukes`): a guided rocket you ride as a camera. It punches through
  walls and ends in a white flash and shake. Refire time, cost and blast are under rc.ini [NUKE].
- **Wrath of god** (`wog`): password-guarded lightning on a spot, a player or every entity of a
  class. It burns within 256 units and blinds everyone within 1200.

## Drivable monsters

Pickups appear 15 s into a multiplayer map at each monster's `info_drive_*_spawn` points, as
many as its `mp_drive*` cvar says. `+use` on the ghostly pickup puts you in the monster:

- The camera sits behind it, and the HUD health shows the monster's health in percent.
- Holding reload looks around without steering.
- `cl_de_alpha` sets how see-through the monster is to you.
- When the monster dies, you die. Its pickup comes back a few seconds later (rc.ini
  `...RespawnDelayAfterDeath`).
- A drive past rc.ini `...MaxDriveTime` (6 to 10 minutes) kills the monster.
- `eject` gets you out where allowed (`sv_allowglobaleject`, rc.ini AllowEject), and the monster
  dies.
- Kills read as the monster's own attack (`apache_rocket`, `headcrab_leap` and so on).

```
monster          attacks
Apache           chin gun with autoaim; volleys of four steerable rockets
Gargantua        flamethrower, slash, a bite that heals, kick; only blast, mortar, energy
                 and crush damage hurt it
female assassin  charged jump, a double-tap pistol, kick, cooked grenade; cloak drains armor;
                 regenerates when idle
ichthyosaur      bite, double bite, charge; a bite throws players and spins their view
panther          claws, double claw, pounce; regenerates when still
houndeye         sonic blast; a charged eye beam through walls; sleep to regenerate
alien slave      charged zap, split over two bolts; a sap attack that drains health; claw
turret           spins up and fires; ammo regenerates while retracted; only lives on a
                 floor or ceiling
Big Momma        slashes, a stomp shockwave, a ring that turns its target into a headcrab;
                 regenerates per living driven headcrab
alien grunt      burst gun, slashes, a bouncing big gun; "hyper" mode glows, speeds up and
                 regenerates, then hurts when it runs out
alien controller homing balls that fork; a charged laser ball; kamikaze, a blast through walls
bullsquid        bite that heals; whip; its spit turns a player into a chumtoad
headcrab         charged leap that heals on a hit (made by Big Momma's ring)
chumtoad         a forking lightning zap; under water it discharges and dies (made by the
                 bullsquid's spit)
```

With `sv_cheats 1` (and `sv_allowtransform 1` in multiplayer) impulses 33-46 turn you into one
directly (SERVER.md).

## Curses

With `mp_curses 1`, eight curses spawn 15 s into the map and hop around. One leaps onto a
player who comes near, and "Cursed by:" shows at the top of the screen. When its host dies it
moves on, and it respawns later (rc.ini [CURSES]).

```
curse        effect on its host
Reversal     controls reversed
Slowness     slower movement
Slowrefire   weapons refire slower
DmgMirror    damage the host deals also hits the host
DmgScale     damage the host deals is halved
Drunkness    the view sways and the field of view swims (only from a map or .lec file)
ProMagnet    draws grenades and other projectiles toward the host
SlowDeath    steady damage over time
```

## Game modes

- **Capture the flag** (`mp_teamplay 1`, `mp_ctf 1`, 2 to 6 teams). Each team has a flag at
  `info_flagspawn`, or spread over the deathmatch spawns. The carrier glows in his team's color.
  A capture is worth `mp_points_per_capture`. A dropped flag goes home after
  `mp_flag_return_time`. `flagstatus` shows where each flag is.
- **Deathballs** (`mp_deathballs N`): N big balls roll around the map and crush what they hit,
  with the kill credited to whoever last pushed or shot one. With `mp_deathballsonly 1` nothing
  else does damage.
- **Scientist kill** (`mp_scikill N`): N scientists keep spawning at deathmatch spawn points,
  run for cover, and give a frag each.

## Players

- **Observer mode.** You join as an observer ("Free mode", `sv_pregameobserver`) and spawn
  with jump. While observing:
  - attack cycles free, chase and target views
  - attack2 picks the next target
  - reload cycles the draw modes
  - forward and back zoom
  `spectate` goes back to it mid-game (`sv_ingameobservers`).
- **No fall damage** in multiplayer. The fall pain sounds still play.
- **Jumping.** Jump height is rc.ini [PLAYER] JumpHeight. The long jump works when you duck
  within 1 s, with the module. Landing from one is a butt stomp that damages around you
  (LongJumpStompDmg).
- **Ducking** takes 0.4 s on the server, and you can't stand up under a low ceiling.
- **Regeneration** of health and armor, off by default. Start health and armor, maximum
  health and armor, and starting with the long jump are all set under rc.ini [PLAYER]. Battery
  and charger amounts come from [ITEMS].
- **Spawning** picks a random free spot and telefrags on a full map. In teamplay you spawn at
  your team's `info_teamspawn`.
- **Death camera.** After death the view moves to a point up to 200 units away, looking at
  the body.
- **Player ID.** Aiming at a player or a driven monster within 4096 units shows its name in
  orange.
- **Team push** (`mp_teampush 1`): teammates' hits push without hurting.
- **Gibs and blood.** `cl_gibcount` adds gibs for you; RC adds fake gibs and blood trails
  unless `sv_lowlag 1`.
- **Votes.** `votekick` and `votemap` show a tally on screen, and keys 1 and 2 vote while a
  vote runs.
- **Map exits.** In deathmatch, a level's `trigger_changelevel` gibs you instead
  (`sv_noexit`).

## Monsters

- They spawn in deathmatch, drop no weapons there, and their corpses fade.
- `sv_monspawn 1` turns each map-placed monster's spot into a deathmatch spawn point.
- `sv_ignoreplayer` blinds them to players, and `sv_loyalnpcs` keeps scientists and Barneys on
  your side however you treat them.
- Freezing stops monsters, tentacles and ichthyosaurs too.
- `monster_killerbabycrab` is a red baby headcrab that explodes after 30 s or when killed.
- `func_tankgib` is a mounted gun that fires gibs. `func_tankmortar` explosions credit whoever
  is at the controls.

## Maps

- **Level-custom files** (`maps/<map>.lec`) add, remove, move, kill and trigger entities 2 s
  into a map, so stock maps get RC's spawns and pickups without recompiling. Admins build them
  in-game with the `lc*` commands.
- **Per-map configs:** `<map>.cfg` and `changelevel.cfg` run at map start (`sv_mapcfg`).
- **Intermission** is 6 s, and the map cycle goes in `mapcycle.txt` order.

## Entities RC adds

Placeable in maps or `.lec` files:

```
info_flagspawn  info_teamspawn  info_deathball_spawn  info_semi_null
info_drive_{agrunt,apache,assassin,bm,bullsquid,controller,garg,houndeye,icky,panth,slave,turret}_spawn
pickup_drive_{same twelve}  drive_* (the driven monsters themselves)
monster_killerbabycrab  func_tankgib  env_fadeone
```

The rest are made by the game: `flag`, `deathball`, the curses (`reversecurse`, `slowcurse`,
`slowrefirecurse`, `dmgmirrorcurse`, `dmgscalecurse`, `drunkcurse`, `promagcurse`, `sdcurse`), and
the weapons' projectiles. `reference/entities-rc.txt` lists every class the original exports.

## Half-Life entities RC doesn't have

- `crossbow_bolt`: the crossbow fires `impail_arrow`.
- `func_vehicle` and `func_vehiclecontrols`: these come from Half-Life Updated. The rebuild
  keeps them; RC 1.9 has neither.
