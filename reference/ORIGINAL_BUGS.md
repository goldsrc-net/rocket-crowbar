# Bugs in the original Rocket Crowbar 1.9

Bugs and oddities in `rc_i386.so` found while reconstructing it. The rebuild keeps each one as
the original has it unless the **Ours** column says otherwise; anything we changed is also in
`DEVIATIONS.md`. Each is marked `VERIFY` at its spot in the code, and the package's dossier
(`reference/dossiers/<package>.md`) has the full reasoning. The entries from the stock-function
audit (2026-11-07, below each table's rule) are SDK 2.1 code RC kept; their reasoning is in the
audit rows (`reference/audit/r1.md`-`r6.md`), and halflife-updated's guards among them carry no
`VERIFY` unless the line says so. Add new ones here as they turn up.

Status: **kept** = same behavior as the original; **guarded** = we avoid a crash the original has;
**changed** = we differ for another reason, said in the column.
Policy since 2026-11-07: every crash gets guarded (the smallest check, nothing else changed).
Addresses are file addresses in `rc_i386.so` where the dossier or audit row gives one.

## Crashes

```
Where                                   What                                                  Ours
Preff.cpp PREFS_LoadFile                a line of only "=" gives a NULL token -> crash        guarded
                                        (also terminates the token, not the key copy: kept)
vote_kick/vote_map VK_/VM_InitVote      dereference a NULL initiator (server console) after   guarded
                                        testing it for NULL                                   (world entity)
drivehc.cpp DHCHeadcrabifyEnt           no NULL check on the monster_headcrab it creates      guarded
driveicky.cpp CDriveIcky::TakeDamage    writes through m_pOwner with no driver                guarded
driveturret.cpp CDriveTurret::TakeDmg   writes m_pOwner->m_fDrvEntEjectTime with no driver    guarded
drivegarg.cpp TakeDamage, DoFlameDamage dereference the driver with no null test              guarded
driveapache.cpp FlyTouch, DEClientEject dereference m_pOwner unchecked                        guarded
DyingThink / Killed (all 14 drive       CBaseEntity::Instance(pev->owner)->DeathNotice with   guarded
  monsters)                             no null check on the instance
drivehoundeye.cpp CDHEEyeSprite::ESThink dereferences its owner unchecked                      guarded
drivecontroll.cpp CDriveController::    writes through a null owner                           guarded
  TakeDamage
driveaslave.cpp CASLRechargeBall::      calls through a null target                           guarded
  FlyToOwner
driveaslave.cpp CASLEngBall::EBThink    reads an unfilled trace (unused: DoBallAttack empty)  guarded
shotgun.cpp CSpaceRift::FinalBoom       reads through its owner after it disconnected         guarded
egon.cpp CIsoBomb::IsoBecomeSolid       reads through its owner after it disconnected         guarded
flag.cpp GetFlagTeamColor               team outside 1-6: sprintf into the read-only ""       guarded
                                        literal it returns (.rodata 0x265d66)
bot_prefs.h CVEC::~CVEC, Resize         scalar delete on new[] memory: heap corruption at     changed
                                        shutdown                                              (delete[])
---- stock-function audit, 2026-11-07
maprules.cpp CGameScore::Use (0x1bfb20), calls IsPlayer() through a NULL activator            guarded
  CGamePlayerEquip::EquipPlayer
  (0x1c1580)
triggers.cpp CBaseTrigger::CounterUse   reads a NULL activator                                guarded
  (0x226da0)
maprules.cpp CGamePlayerEquip::KeyValue UTIL_StripToken with no destination size: a long     guarded
  (0x1c1350), triggers.cpp              keyvalue overruns the buffer
  CMultiManager::KeyValue (0x224f00)
nihilanth.cpp NextActivity (0x1d21d0),  sprintf of a keyvalue name + number into 64/36 bytes   guarded
  HandleAnimEvent (0x1d3590)                                                                  (128 bytes)
nodes.cpp CGraph::                      scalar delete on four new[] arrays (Valve's SDK)      changed
  ComputeStaticRoutingTables (0x1de750)                                                       (delete[])
func_break.cpp CBreakable::Precache     material 9 (matNone), which KeyValue allows:          guarded
  (0x18aa80), DamageSound (0x18ac90)    uninitialized gib model name and sound count          (wood)
plats.cpp CPlatTrigger::Touch           reads its platform after the platform is removed      guarded
  (0x1e44f0)
soundent.cpp CSoundEnt::ISoundsInList   unknown list type walks an uninitialized index        guarded
  (0x216d80)
sound.cpp CSpeaker::SpeakerThink        preset outside 1-12: uses an uninitialized name       guarded
  (0x216320)                            pointer                                               ("")
effects.cpp CLightning::StrikeThink     ring beam with a point entity: returns with the       guarded
  (0x17b5c0)                            SVC_TEMPENTITY message open
bullsquid.cpp CBullsquid::              spit reads a NULL enemy (VERIFY)                      guarded
  HandleAnimEvent (0xf54f0)
hgrunt.cpp CHGrunt::CheckMeleeAttack1   no enemy: Classify() through an uninitialized         guarded
  (0x1ac300)                            pointer (VERIFY)
talkmonster.cpp CTalkMonster::RunTask   STARE with no edict-1 player reads a NULL player      guarded
  (0x21e5e0)                            (halflife-updated's fix: the task fails)
talkmonster.cpp CTalkMonster::CanFollow m_pCine is NULL just after a script ends (VERIFY)     guarded
  (0x2203f0)
hornet.cpp CHornet::DieTouch (0x1b0710) null attacker when the owner is gone (VERIFY)         guarded
osprey.cpp COsprey::FlyThink (0x1e0f00) null m_pGoalEnt (VERIFY)                              guarded
player.cpp CBasePlayer::                removing the active item leaves m_pLastItem pointing  guarded
  RemovePlayerItem (0x1f4050)           at it
player.cpp CBasePlayer::SwitchWeapon    no NULL weapon test                                   guarded
  (0x1f5fe0)
---- wave 2
combat.cpp CBaseMonster::TakeDamage     mp_deathballsonly 1 reads the inflictor unchecked: a  guarded
  (0x107b30)                            NULL inflictor or one without an edict crashes
player.cpp CBasePlayer::                impulses 33-39 use Create's result unchecked          guarded
  CheatImpulseCommands (0x1f2f70)
```

## Logic bugs

```
Where                                   What                                                  Ours
Preff.cpp (Linux build)                 doesn't strip \r: the shipped CRLF rc.ini loads no    changed
                                        headings, so Linux servers ran on built-in defaults   (accepts CRLF)
vote_map.cpp VM_InitVote (0x237e85)     tests the kick vote, reports "Vote map in progress"   kept
vote_map.cpp VM_InitVote                allowedmaps.txt scan stops at a name whose first      kept (overflow
                                        char is <= 12; its 64-byte buffer overflows by one    guarded: 65 bytes)
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
items.cpp CItem::Spawn (0x1bb750)       an item placed in a wall or outside the map is kept,  kept
                                        floating (no "fell out of level" removal)
---- stock-function audit, 2026-11-07
monsterstate.cpp CBaseMonster::RunAI    tests SF_MONSTER_GAG (2) against pev->flags, which    changed (halflife-
  (0x1cacf0)                            is FL_SWIM: a gagged monster still idles aloud, a     updated's fix,
                                        swimming one never does                               Steven 2026-11-07)
doors.cpp CBaseDoor::Blocked (0x11b980) realigns a linked door only when its avelocity        kept
                                        equals this door's velocity (the wrong field)
doors.cpp CMomentaryDoor::Use           starts the move sound and never stops it: a           changed (halflife-
  (0x11c5a0)                            momentary_door loops it after it stops                updated's fix)
triggers.cpp CChangeLevel::             sets dmgtime 0.2 s ahead but still tests it with ==,  kept
  ChangeLevelNow (0x2276e0)             so the once-a-frame guard never matches (VERIFY)
sound.cpp CAmbientGeneric               plays on CHAN_STATIC via EMIT_SOUND_DYN, so a loop    kept
  (RampThink 0x212b10, Precache,        started at map load (Precache) may reach no client
  ToggleUse)                            (VERIFY)
monsters.cpp CBaseMonster::             no IsLockedByMaster test: monsters open master-locked kept
  OpenDoorAndWait (0x1c6400)            doors
monsters.cpp CBaseMonster::             state names lack PlayDead: PLAYDEAD prints "Dead",    changed (halflife-
  ReportAIState (0x1c9170)              DEAD prints none                                      updated's fix)
buttons.cpp CBaseButton::ButtonSpark    a MOVETYPE_PUSH button thinks on pev->ltime but the   changed (halflife-
  (0xf8460), ButtonBackHome (0xf89f0)   next spark is set from gpGlobals->time, so each spark updated's fix)
                                        comes late by the server time the button spawned at
                                        (VERIFY in an engine run), and it sparks at pev->mins,
                                        not absmin (off for a button with an origin brush); a
                                        func_rot_button with its X axis flag (64) sparks too
buttons.cpp CMomentaryRotButton::Use    retriggering sends the target the current angle's     changed (halflife-
  (0xf93f0)                             value, so the first impulse goes the wrong way        updated's fix)
effects.cpp CBlood::BloodPosition       "player" blood without a player activator bleeds at   changed (halflife-
  (0x17f480)                            edict 1, even when slot 1 is empty                    updated's fix)
func_tank.cpp CFuncTank::               target nearer than the barrel offset: sqrt of a       changed (skips the
  AdjustAnglesForBarrel (0x18eea0)      negative, NaN aim                                     correction; Steven
                                                                                              decides, audit r7)
```

## Wave 2 (SDK files)

Not folded into the tables above yet: each wave-2 dossier's section 8 lists the bugs found in
its files (`reference/dossiers/w2-*.md`). They get merged here when wave 2 is complete; the two
crashes they had kept are guarded and already in the Crashes table. Some of those are no longer kept: where halflife-updated had fixed one (the weapon
respawn time, FallThink on a held weapon, the grenade sequence timing, the osprey's NaN flight and
smoke puffs, CWreckage's int time, the zombie's unused pitch), its fix is back; mod-rc2's
`reference/audit/FIXES.md` (mod-rockcrowbar wave 2) lists each.

## Dead code

`VK_IsValidIp`, `BP_GetConfArgLine` (stores every token as ""), `ClientConnecting` (reads an
unregistered `sv_reserved`), `LC_ReadLine`, the chumtoad's `DoJump`/`RunJumpPhysics`,
`CDriveASlave::DoBallAttack` (empty, so `CASLEngBall` and `CreateEB` never run), `m_bCanEyeAttack` (houndeye) and `m_bMoving` (grunt) written but never read.
