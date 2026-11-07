# Bugs in the original Rocket Crowbar 1.9

Bugs and oddities in `rc_i386.so` found while reconstructing it. The rebuild keeps each one as
the original has it unless the **Ours** column says otherwise; anything we changed is also in
`DEVIATIONS.md`. Each is marked `VERIFY` at its spot in the code, and the package's dossier
(`reference/dossiers/<package>.md`) has the full reasoning. Add new ones here as they turn up.

Status: **kept** = same behavior as the original; **guarded** = we avoid a crash the original has.
Addresses are file addresses in `rc_i386.so` where the dossier gives one.

## Crashes

```
Where                                   What                                                  Ours
Preff.cpp PREFS_LoadFile                a line of only "=" gives a NULL token -> crash        kept
                                        (also terminates the token, not the key copy)
vote_kick/vote_map VK_/VM_InitVote      dereference a NULL initiator (server console) after   kept
                                        testing it for NULL
drivehc.cpp DHCHeadcrabifyEnt           no NULL check on the monster_headcrab it creates      kept
driveicky.cpp CDriveIcky::TakeDamage    writes through m_pOwner with no driver                guarded
driveturret.cpp CDriveTurret::TakeDmg   writes m_pOwner->m_fDrvEntEjectTime with no driver    kept
drivegarg.cpp TakeDamage, DoFlameDamage dereference the driver with no null test              kept
driveapache.cpp FlyTouch, DEClientEject dereference m_pOwner unchecked                        kept
DyingThink / Killed (Apache, assassin,  CBaseEntity::Instance(pev->owner)->DeathNotice with   kept
  BM, garg)                             no null check
drivehoundeye.cpp CDHEEyeSprite::ESThink dereferences its owner unchecked                      kept
drivecontroll.cpp CDriveController::    writes through a null owner                           guarded
  TakeDamage
driveaslave.cpp CASLRechargeBall::      calls through a null target                           guarded
  FlyToOwner
driveaslave.cpp CASLEngBall::EBThink    reads an unfilled trace (unused: DoBallAttack empty)  guarded
flag.cpp GetFlagTeamColor               team outside 1-6: sprintf into the read-only ""       guarded
                                        literal it returns (.rodata 0x265d66)
bot_prefs.h CVEC::~CVEC, Resize         scalar delete on new[] memory: heap corruption at     changed
                                        shutdown                                              (delete[])
```

## Logic bugs

```
Where                                   What                                                  Ours
Preff.cpp (Linux build)                 doesn't strip \r: the shipped CRLF rc.ini loads no    changed
                                        headings, so Linux servers ran on built-in defaults   (accepts CRLF)
vote_map.cpp VM_InitVote (0x237e85)     tests the kick vote, reports "Vote map in progress"   kept
vote_map.cpp VM_InitVote                allowedmaps.txt scan stops at a name whose first      kept
                                        char is <= 12; its 64-byte buffer overflows by one
pregame.cpp GetRandomObserverSpawn      spot 1 twice as likely, the last spot never chosen    kept
drivehoundeye.cpp CDrvHESpawner::Spawn  names itself drive_panth_spawner (0x2636e3), so       kept
                                        CreateDrvHESpawner never finds an existing one
driveagrunt.cpp FireBigGun (0x120f0e)   flags &= FL_ONGROUND, almost surely meant &= ~        kept
driveagrunt.cpp SlashFire (0x11fde8)    both hands knock the target the same way              kept
driveagrunt.cpp Spawn                   m_bloodColor red, BloodColor() returns yellow         kept
driveagrunt.cpp, drivebullsquid.cpp     CheckEHop never advances its own next-check timer     kept
  CheckEHop
drivepanth.cpp                          classname "alien_slave" and pref key                  kept
                                        SlaveWaterFriction, copied from the aslave
DoYaw: drivegarg, drivepanth,           next idle time = time + time + random (fadd st0,st0): kept
  drivehoundeye, drivehc                no idle animation for a long time after a turn
drivegarg/drivebm/drivepanth DoMovement in the air, 30-120 u/s plays "run", 120+ "walk"       kept
drivehc.cpp DoMovement                  in the air, the chumtoad's run/walk choice reversed   kept
drivebm.cpp CallDoSlash                 damage direction and knockback use stale gpGlobals    kept
                                        vectors, not the ones it computed
DEClientPreThink: driveicky, drivebm,   sets euser2 but clears euser3                         kept
  drivegarg
driveturret.cpp DEClientLeave           never SET_VIEWs the driver back                       kept
driveapache.cpp DoFlight                "close enough" tests always true: the avelocity-      kept
                                        zeroing branch never runs
driveapache.cpp DoCloak                 fade-out can overshoot (compares with a negative rate) kept
driveapache.cpp DoJump                  uses whatever v_forward/v_right the last caller left  kept
driveapache.cpp DoControl               reuses ApacheForDecel/BackDecel keys for vertical     kept
                                        decel with other defaults (200/150)
driveassassin.cpp DEClientEject         ejecting gibs the assassin (GIB_ALWAYS)               kept
drivecontroll.cpp thrown ball           the second fork deals dmg*0.5, not its pref           kept
driveaslave.cpp water physics           gravity pushes the slave up in water                  kept
pickups' Use (all drive monsters)       test pCaller, not pActivator, and call IsPlayer()     kept (houndeye,
                                        before the NULL test                                  agrunt: NULL first)
curses.cpp curse_spawner                never creates drunkcurse                              kept
curses.cpp CBaseCurse::TakeDamage       sets damage conditions twice, health stays 50         kept
curses.cpp CCurseDrunk                  FOV only reaches halfway (cos(3t) squared)            kept
nm.cpp CNukeM::NukeRadus                everyone hit takes full damage; distance only picks   kept
                                        who is hit
nm.cpp CNukeM::NukeThink                stops steering on any think at full speed; camera     kept
                                        pitch wrap compares with 180 both times
wog.cpp WOGCommand                      strikes the looked-at spot when CMD_ARGC() <= 41      kept
wog.cpp CWOGSpawner                     never removed after it fires                          kept
levelcustom.cpp FindOtherSameSpawner    can return the calling respawner itself              kept
drivebm.cpp DestoryFlames               keeps dangling beam pointers (never read)             kept
```

## Dead code

`VK_IsValidIp`, `BP_GetConfArgLine` (stores every token as ""), `ClientConnecting` (reads an
unregistered `sv_reserved`), `LC_ReadLine`, the chumtoad's `DoJump`/`RunJumpPhysics`,
`CDriveASlave::DoBallAttack` (empty, so `CASLEngBall` and `CreateEB` never run), `m_bCanEyeAttack` (houndeye) and `m_bMoving` (grunt) written but never read.
