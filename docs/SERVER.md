# Server reference

Everything Rocket Crowbar 1.9 adds to a Half-Life server's console and config: its cvars,
its commands, its impulses, the client userinfo keys it reads and the files it reads and
writes. What each feature does in play is in [GAMEPLAY.md](GAMEPLAY.md); the weapon and
monster tuning in `rc.ini` is in [RC_INI.md](RC_INI.md). Where the rebuild differs from RC 1.9
itself, [reference/DEVIATIONS.md](../reference/DEVIATIONS.md) says so.

`scripts/check_docs.py` fails if a cvar or command the code registers is missing from this
page.

## Cvars

All RC cvars are server cvars (`FCVAR_SERVER`), so they show in server browsers. "1" means the
value must be exactly 1 unless the entry says otherwise.

### Game modes

```
cvar                  default  what it does
mp_ctf                1        capture the flag in teamplay (mp_teamplay 1) with 2 to 6 teams; the
                               flags go at info_flagspawn, or spread over the deathmatch spawns
mp_flag_return_time   60       seconds a dropped flag lies before it goes home; 0 never
mp_points_per_capture 10       frags for the capturing player and points for his team
mp_curses             0        1 puts the eight curses into the map, 15 s in (GAMEPLAY.md)
mp_deathballs         0        how many deathballs roll around the map (info_deathball_spawn)
mp_deathballsonly     0        1: only deathballs do damage to players and monsters
mp_scikill            0        N: keep N scientists in a deathmatch map (from 15 s in), 150 health,
                               a frag each for killing one
```

### Weapons and specials

```
cvar                  default  what it does
mp_allowrifts         1        shotgun space rifts (anything else disables them)
mp_allowiso           1        the egon's IsoBomb
mp_allownukes         1        the nuke command
sv_ceasefire          0        1: no weapon fires (crowbar, RPG, snarks and the rest)
sv_allowgives         1        give, giveperm, giveitem and givefor exist at all; who may use them
                               is under Commands
```

### Drivable monsters

Each is how many pickups of that monster the map gets 15 s after it starts, at that monster's
`info_drive_*_spawn` points. A map without those points gets none.

```
cvar                  default  monster
mp_driveapache        1        Apache helicopter
mp_drivegarg          2        Gargantua
mp_driveassassin      2        female assassin
mp_driveicky          2        ichthyosaur
mp_drivepanth         2        panther
mp_drivehoundeye      2        houndeye
mp_driveslave         2        alien slave
mp_driveturret        1        turret
mp_drivebm            2        Big Momma
mp_driveagrunt        2        alien grunt
mp_drivecontroller    2        alien controller
mp_drivebullsquid     2        bullsquid
sv_allowglobaleject   0        1: every driven monster allows "eject"; otherwise only those whose
                               rc.ini AllowEject is set (by default the Apache), or any outside
                               multiplayer
sv_allowtransform     0        1: in multiplayer, the cheat impulses 33-46 turn a player into a
                               monster (needs sv_cheats 1)
```

### Players and teams

```
cvar                  default  what it does
sv_pregameobserver    1        a joining player starts as an observer ("Free mode") and spawns
                               with jump
sv_ingameobservers    1        spectate/observe is allowed mid-game (admins may always)
mp_teamannoy          1        1: the non-damage tricks (impaling, the egon tractor, the gauss
                               teleport, freezing) work on anyone; otherwise only on players you
                               could also damage
mp_teampush           0        1: teammates' hits push each other without damage
sv_noexit             1        in deathmatch, a trigger_changelevel gibs whoever touches it
                               instead of changing the map
```

### Monsters

```
cvar                  default  what it does
sv_monspawn           0        1: each monster the map places leaves a deathmatch spawn point
                               where it stands (out of water, where a player fits)
sv_ignoreplayer       0        not 0: monsters don't notice players
sv_loyalnpcs          0        1: shooting scientists and Barneys doesn't turn them against you
sv_pcall              0        1: precache 15 monster types at map start, for .lec files and
                               commands that make them mid-game
sv_pcstuff            0        1: precache a smaller set of 9 of them
```

### Votes

```
cvar                  default  what it does
sv_kickfract          0.66     a kick vote passes when yes/total goes over this
sv_kickbantime        10       minutes a vote-kicked player is banned (banid)
sv_mapfract           0.51     a map vote passes when yes/total reaches this
sv_votefreq           120      seconds before a player may start another vote
```

### Server

```
cvar                  default  what it does
sv_serverclient       -1       admin client indices, separated by ; : space tab or newline. The
                               listen server's own player is always admin. -1 means none
sv_prefsfile          rc.ini   the prefs file, in the game directory (RC_INI.md)
sv_mapcfg             1        not 0: run changelevel.cfg and <map>.cfg at each map start in
                               deathmatch, and the next map's .cfg before each map change
sv_lowlag             0        1: drop blood drips, fake gibs, texture sounds and some weapon
                               effects to save bandwidth
sv_chglevel           0        registered, read by nothing (the original too)
Rocket_Crowbar_Version 19      the version, for server browsers; read by nothing
```

### Stock Half-Life cvars RC treats differently

```
cvar                  how
mp_falldamage         no effect: there is no fall damage in multiplayer
mp_chattime           no effect: the intermission is 6 s (halflife-updated's cvar, not RC's)
allow_spectators      no effect: RC's own observer mode is used (sv_ingameobservers)
sk_plr_hornet_dmg1-3  no effect: the hivehand's damage is fixed at 7
sv_cheats             also set by the fcheats command; gates the give commands for non-admins,
                               the fov command and the cheat impulses
```

The other stock cvars (`mp_teamplay`, `mp_fraglimit`, `mp_timelimit`, `mp_friendlyfire`,
`mp_weaponstay`, `mp_forcerespawn`, `mp_flashlight`, `mp_autocrosshair`, `decalfrequency`,
`mp_teamlist`, `mp_teamoverride`, `mp_defaultteam`, `mp_allowmonsters`, `displaysoundlist`,
the `sk_*` skill cvars) keep their Half-Life defaults and meaning. Many `sk_*` values are overridden in
practice by `rc.ini` keys for the same thing.

### Added by Half-Life Updated, not RC

RC 1.9 doesn't have these; the rebuild keeps them from its base.

```
cvar                          default  what it does
sv_allowbunnyhopping          1        bunny hopping; 1 here because RC 1.9 has no speed cap
mp_fragsleft, mp_timeleft     -        read-only, for server browsers
sv_alltalk                    0        no effect: the voice manager lets everyone hear everyone
voice_serverdebug             0        voice manager debug output
sv_busters                    0        Half-Life's "busters" mode, off
sv_pushable_fixed_tick_fudge  15       halflife-updated's pushable speed fix
```

## Commands

Typed in the client console (or bound). "Admin" means the player is in `sv_serverclient` or is
the listen server's own player. Admin commands are echoed to the server console and log as
`Server Cmd (<name>): <command>`.

### Everyone

```
command                      what it does
say_private, sayprivate      say_private <name or index> <text>: a message to one player
showmotd, serverhelp         show the server's MOTD
nuke                         fire a nuke (mp_allownukes 1; rc.ini [NUKE] RefireTime, 90 s,
                             between them)
eject                        leave the monster you're driving, if it allows it
spectate, observe            become an observer (sv_ingameobservers 1, or admin); costs no frag
cl_gibcount [4-259]          how many gibs you see; no argument prints it
cl_de_alpha [0-1]            how see-through the monster you drive is to you
RCV, RCBV                    print RC's version
listclients                  print each client's index, user id and name
votekick [index]             start a kick vote; no argument lists the indices
vkyes, vkno                  vote in a kick vote (keys 1 and 2 are bound to these while it runs)
votemap [map]                start a map vote (the map must be in allowedmaps.txt, if it has any)
vmyes, vmno                  vote in a map vote
showmaps                     list the maps in allowedmaps.txt that exist
dumpheads                    list the rc.ini headings
dumpprefs <heading>          list a heading's keys and values
prefsfind <heading> <key>    print one value; also findpref, searchprefs, prefslookup
flagstatus                   in CTF, where each flag is
fov [value]                  with sv_cheats, set your field of view; otherwise print it
wog <password> ...           RC's "wrath of god" lightning, guarded by a password hash in the
                             code; a wrong password answers "Unknown command"
```

Stock commands RC keeps: `say`, `say_team`, `drop`, `use`, `weapon_*`, `lastinv`, `menuselect`.

### Gives

With `sv_allowgives 1`, for an admin, or for anyone alive, not frozen and not observing when
`fcheats` is on:

```
command                give <classname>: an item_, weapon_ or ammo_ is spawned on you and
give, giveperm         picked up; anything else goes through GiveNamedItem
giveitem <classname>   spawned on you and touched, whatever it is
givefor <classname>    spawned 256 units in front of you
```

### Admin

```
command                          what it does
fcheats <0|1>                    set cheats for gives, fov and impulses (also sets sv_cheats)
sndcmd <index> <cmd> [args]      run a console command on one client, or on all for an index
                                 that isn't a client
remall <classname>               remove every non-player entity of that class
rement                           remove the entity you look at (not players)
useent [off|on|set|toggle] [v]   Use() the entity you look at
addspawn                         add a deathmatch spawn point where you stand
setpax, setpay, setpaz <value>   set the punch angle of the entity you look at
setpref <heading> <key> <value>  change an rc.ini value in memory, and tell everyone
saveprefs                        write the in-memory prefs to sv_prefsfile
loadprefs                        reload sv_prefsfile
```

The level-custom commands append to the current map's `.lec` file (Files, below), so a map's
setup can be built by walking it:

```
command                     line written
lcputent <class> [any]      create <class> x y z ax ay az (your origin; your view angles, or
                            only your yaw with a second argument)
lcrespawnent <class> [any]  respawncreate ... (the entity comes back 1 s after it dies)
lcrement                    entremove <class> x y z  for the entity you look at
lckillent                   entkill <class> x y z
lcuseent                    useent <class> x y z
lcaddcommand "<line>"       the line as given
lccleanfile                 empties the file
```

### Server console

None of RC's own. Half-Life Updated adds `sv_load_all_maps` and `sv_stop_loading_all_maps`
(load every map in turn, to build node graphs); RC 1.9 doesn't have them.

### Added by Half-Life Updated, not RC

Answered so a current client works: `fullupdate` (re-sends the HUD), `closemenus` (ignored),
and the voice manager's `vban` and `VModEnable`.

## Impulses

The cheat impulses need `sv_cheats 1`. RC replaced the SDK's debug impulses (76, 102-107,
195-199, 202 are gone).

```
impulse  what it does
33       become the Apache (256 units above you)
34       become a Gargantua
35       become a female assassin
36       become a turret on the floor or ceiling you look at
37       become an ichthyosaur
38       become a houndeye
39       become an alien slave
40       become a panther
41       become Big Momma
42       become a headcrab
43       become an alien grunt
44       become an alien controller
45, 46   become a bullsquid, a chumtoad (listen server's own player only)
101      all weapons, as in Half-Life
111      all ammo
112      the throwables: hand grenade, tripmine, satchel, snark
```

33-44 also need `sv_allowtransform 1` in multiplayer. Stock impulses kept: 100 (flashlight),
201 (spray).

## Client userinfo keys

Set with `setinfo`; `cl_gibcount` and `cl_de_alpha` set the first and last for you.

```
key            what it does
extragibs      extra gibs this client sees, 0-255 (cl_gibcount minus 4)
bloodscale     read and saved with the player; nothing uses it
driveentalpha  how see-through the monster you drive is, 0-1
```

## Files

In the game directory:

```
file                 read or written
rc.ini               all weapon, monster and player tuning (sv_prefsfile; RC_INI.md). Loaded at
                     each map start; saveprefs writes it
maps/<map>.lec       the map's level-custom commands, run 2 s into the map: removeall, killall,
                     entremove, entkill, move, create, respawncreate, useent, '#' comments
                     (dlls/levelcustom.cpp has the syntax). The lc* commands append to it
<map>.cfg            run at map start and before changing to that map (sv_mapcfg)
changelevel.cfg      run at each map start in deathmatch, before <map>.cfg (sv_mapcfg)
allowedmaps.txt      maps votemap accepts and showmaps lists; empty or missing means all
server.cfg           run from the world's precache, as RC 1.9 does
```
