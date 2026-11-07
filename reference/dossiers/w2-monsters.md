# Dossier: w2-monsters

## 1. Scope

Original source files (wave 2, stock SDK files RC touched): monsters.cpp, schedule.cpp,
headcrab.cpp, scientist.cpp, bigmomma.cpp, talkmonster.cpp, squadmonster.cpp, barney.cpp,
houndeye.cpp, controller.cpp, agrunt.cpp, gargantua.cpp, ichthyosaur.cpp, hgrunt.cpp,
hassassin.cpp, bullsquid.cpp, zombie.cpp, islave.cpp, apache.cpp, xen.cpp, tentacle.cpp,
flyingmonster.cpp, aflock.cpp, gman.cpp, osprey.cpp, turret.cpp, nihilanth.cpp, leech.cpp,
roach.cpp, rat.cpp, h_cycler.cpp, soundent.cpp, nodes.cpp.

Touched: those `dlls/*.cpp`, plus `dlls/basemonster.h`, `dlls/headcrab.h`, `dlls/talkmonster.h`,
the new `dlls/scientist.h`, and the top-level `CMakeLists.txt` (coordinator's CLIENT_WEAPONS
change). `CKillerRat` is in satchel.cpp in the original (funcfile.tsv), so it is not here.

New tools, kept in `scripts/`:

- `skim.py <worktree> <file.cpp>...`: for every original function of a file, lists the decompile's
  strings, float constants, called names and member names that don't appear in our body of the
  same function (`REV=1`: our strings the decompile lacks). Noisy (inlined helpers, macros written
  as expressions, Ghidra's string mislabels), but it found every RC change listed below.
- `schedcmp.py <orig.so> <ours.so> [regex]`: compares every `sl*` Schedule_t and its Task_t list
  between the original and our i386 build, resolving the original's R_386_32 relocations. Now
  reports no differences for any table.

## 2. Status

Built (i386 and amd64, clean builds after the CLIENT_WEAPONS change). Not run in an engine.
`scripts/check_file.py <file> --missing`, i386:

```
monsters.cpp      123 of 123     headcrab.cpp     49 of 49     scientist.cpp    55 of 55
schedule.cpp       13 of 13      bigmomma.cpp     49 of 49     talkmonster.cpp  44 of 44
squadmonster.cpp   26 of 26      barney.cpp       36 of 36     houndeye.cpp     29 of 29
controller.cpp     49 of 49      agrunt.cpp       29 of 29     gargantua.cpp    49 of 49
ichthyosaur.cpp    43 of 43      hgrunt.cpp       46 of 46     hassassin.cpp    24 of 24
bullsquid.cpp      39 of 39      zombie.cpp       17 of 21 (*) islave.cpp       33 of 33
apache.cpp         32 of 32      xen.cpp          46 of 46     tentacle.cpp     28 of 28
flyingmonster.cpp  16 of 16      aflock.cpp       33 of 33     gman.cpp         17 of 17
osprey.cpp         24 of 24      turret.cpp       46 of 46     nihilanth.cpp    54 of 54
leech.cpp          24 of 24      roach.cpp        13 of 13     rat.cpp           6 of 6
h_cycler.cpp       36 of 36      soundent.cpp     19 of 19     nodes.cpp        57 of 57
```

(*) zombie.cpp's four are gcc 2.95 runtime (`__default_terminate`, `__terminate`,
`__throw_type_match`, `__empty`) that the linker placed after zombie.o; not game code.

check_file's "inl" match is by method name in any header, so a false positive was possible for
the small virtuals (`GetRCCaps` exists on CBaseEntity). Each class's declaration was checked
for its own override instead: every RC-added method now has its own definition. The ones still
inline are the SDK's own inline helpers in headers (HasConditions, TaskComplete, InSquad,
IsFollowing...), which the original happened to emit out of line.

Exports: `monster_killerbabycrab` and `sci_spawner` are in build-i386 `nm -D`.

## 3. Function map

RC-added functions (all `ok` unless noted):

```
0x1aa670  monster_killerbabycrab           headcrab.cpp
0x1aa6a0  CKillerBabyCrab::Spawn           headcrab.cpp
0x1aa7d0  CKillerBabyCrab::StartTask
0x1aa870  CKillerBabyCrab::LeapyTouch
0x1aa9c0  CKillerBabyCrab::Precache
0x1aaa00  CKillerBabyCrab::SetYawSpeed
0x1aaa20  CKillerBabyCrab::CheckRangeAttack1
0x1aaa90  CKillerBabyCrab::Killed          VERIFY guard (NULL owner, see 8)
0x1aace0  CKillerBabyCrab::MonsterThink
0x1aaef0  CKillerBabyCrab::GetSoundVolue / 0x1aaf00 GetVoicePitch / 0x1aaf30 GetDamageAmount
0x1aafa0  CHeadCrab::GetBotShootOrg / 0x1ab000 GetRCCaps / 0x1ab010 GetGuassDistOut / 0x1ab020 GetGuassHull   headcrab.h
0x206460  CreateSciSpawner                  scientist.cpp (declared in scientist.h)
0x2065d0  sci_spawner; 0x206600/0x206650 CSciKillSpawner::Save/Restore; 0x2066a0 SKThink; 0x206880 Spawn; 0x208e20 ObjectCaps
0x206920  CScientist::IRelationship; 0x208e80 BotTargetRealseable; 0x208e90 GetBotTargetWeight (VERIFY bot flag); 0x208ed0 GetRCCaps; 0x208ee0 DisregardEnemy
0x1ca650  CBaseMonster::BotTargetRealseable / 0x1ca660 GetBotTargetWeight / 0x1ca700 GetBotShootOrg   basemonster.h
0x1c9e00  CBaseMonster::PlaySentence / 0x1c9e80 PlayScriptedSentence / 0x1c9ed0 SentenceStop / 0x1cab20 CanPlaySentence
0x21fae0  CTalkMonster::PlaySentence (replaces halflife-updated's PlaySentenceCore override)
0xf1430.. CBigMomma::CanLayCrab, GetNodeYaw/Range/Delay/Presequence/Sequence (moved out of line, bodies stock); 0xf1670 GetRCCaps
0x197c40  CGargantua::FlameIsOn (out of line); 0x197c00/0x197c20 Very/ExtreemeDamageFract 0.4/0.05; 0x197c60 GetRCCaps
0xebd10/0xebd30/0xebe20  CApache damage fractions 0.4/0.05, GetRCCaps
0xee290..0xee310  CBarney BotTargetRealseable, GetBotTargetWeight (VERIFY bot flag), GetRCCaps, GetGuass*
0x1b9800..0x1b9860  CIchthyosaur's seven new Is*Moveable/IsFreezeable/... virtuals, slots 150-156 in order
0x1a33a0  CCyclerSprite::ShouldAnimate; 0x2421c0 CXenSpore::Attack; 0x2423e0 CActAnimating::GetActivity (out of line)
GetRCCaps (+ GetGuassDistOut/GetGuassHull where listed) for CAGrunt, CBullsquid, CSquidSpit,
CController, CGMan, CHAssassin, CHGrunt, CHoundeye, CISlave, CZombie, CLeech, CNihilanth,
COsprey, CRat, CRoach, CBaseTurret, CTentacle: constants from the decompile, hull numbers named
(1 human_hull, 2 large_hull, 3 head_hull). COsprey's and CTentacle's caps Ghidra showed as
strings; read from the disassembly (0x3e088, 0x3c800).
```

Stock functions changed to the original's behavior (RC changes):

```
0x1c4790  CBaseMonster::Look             sv_ignoreplayer != 0: players are skipped
0x1c4ce0  CBaseMonster::MonsterThink     clears pev->fuser4; FL_FROZEN: ClearSchedule, RouteClear, animtime, nothing else
0x1c7730  CBaseMonster::MoveExecute      pev->fuser4 = distance moved / flInterval (flInterval != 0)
0x1c7870  CBaseMonster::MonsterInit      no FAllowMonsters() check; AddToPrecachedList(classname) at the end
0x1c79e0  CBaseMonster::StartMonster     sv_monspawn 1: map monsters drop an info_player_deathmatch (see 7)
0x1c7e30  CBaseMonster::IRelationship    RC's table (PLAYER row) + bounds check on Classify()
0x1ca4f0  CBaseMonster::DropItem         nothing in multiplayer; drops get SF_NORESPAWN
0x2044c0  CBaseMonster::RunTask          TASK_DIE: corpses always fade in multiplayer
0x1a9cb0  CHeadCrab::Spawn               pev->dmg = 0
0x207210  CScientist::Spawn              mp_scikill: random head, no follow, provoked; head picked after Precache
0x207630  CScientist::TakeDamage         EF_NODRAW attacker / sv_loyalnpcs 1 don't provoke
0x207870  CScientist::Killed             mp_scikill frag + ScoreInfo (VERIFY, see 5)
0xed7b0   CBarney::TakeDamage            EF_NODRAW attacker ignored; sv_loyalnpcs 1: never mad
0x1b5bd0  CHoundeye::SonicAttack         spares drive_houndeye
0x1964e0  CGargantua::TakeDamage         assassin_gun bullets do half
0x10c830  CController::MoveExecute, 0x189b40 CFlyingMonster::MoveExecute   pev->fuser4 as above
0x1b87a0  CIchthyosaur::MonsterThink     no swim/blink while FL_FROZEN
0x1b7c50  CIchthyosaur::RunTask          TASK_DIE stamps a death time; floating corpse fades 10 s later
0x222df0  CTentacle::Cycle               FL_FROZEN: ClearSchedule, RouteClear, return
0x1dae90  CTestHull::DropDelay           "Node Graph out of Date. Rebuilding..." centre print to all
```

halflife-updated changes reverted to the original (each checked against the decompile or the
original's data):

```
monsters.cpp   ChangeYaw frametime*10 (343bbb2/95fdebe); SetActivity order (ec1f016); MonsterUse sets ALERT (08975b8);
               IRelationship HUMANPASSIVE row (75bf517); allow_item_dropping keyvalue removed (b624a61);
               sentences back in CBaseMonster, no deadflag test (85467c5/5ee9711/3d8b933)
talkmonster    PlaySentence doesn't test IsAlive; RunTask/GetScheduleOfType use edict 1 (55a1e89/f2c8e23);
               Touch without interrupt-mask test (d667a24); TakeDamage friend test (e306d7a); CanFollow (3f68a77)
scientist      Scream and TASK_SAY_FEAR test FOkToSpeak; scream rates; ISoundMask without scents;
               GetSchedule/GetIdealState without the disregard fixes; tlFear/tlScientistCover/tlFollowScared
               tables; TalkInit body % 3; no _sv_override_scientist_mdl (Marphy commits, 0a73020, c3b58b1)
houndeye       HOUNDEYE_EYE_FRAMES 4; tlHoundGuardPack TASK_GUARD; no random leader look (e5f9c39, c718785)
hgrunt         tlGruntVictoryDance (ff37029); HGRUNT_AE_DROP_GUN without GetBodygroup test (bc7d6e3)
controller     TASK_WAIT_FACE_ENEMY faces without an enemy test (271d503)
gargantua      CStomp uses frametime, no save table (cb09dc7)
ichthyosaur    ChangePitch/ChangeYaw fixed 220*0.1 step; TwitchDie floats, no slFloat/SetActivity (a47bddd, 343bbb2, 95fdebe)
flyingmonster  ChangeYaw 220*frametime (95fdebe also had a 220*220 bug)
zombie         IdleSound/AttackSound pitch 100+-5 (f919b94, 022b5ea)
islave         no SF_BEAM_TEMPORARY on beams (ac808a8)
apache         398591d reverted (no max_health, NullThink/FCheckAITrigger/Listen/state/damage conditions)
osprey         765c1f5, 5d8aed7 (left/right smoke puffs swapped back), 2e0607e, 2bf0ea1 reverted
nihilanth      no FL_FLY (29e93ca); tentacle no EFLAG_SLERP (a02eb83); aflock undamageable, health 1 (ab037f6)
h_cycler       CWreckage::m_flStartTime an int (cb51706); no IsAllowedToSpeak on CCycler (3d8b933)
agrunt         no ag_fire sound in HandleAnimEvent (the original plays it in CHornet::Spawn; coordinator note)
```

Kept from halflife-updated, behavior the same or a crash guard: the node graph file I/O through
the engine filesystem and GAMECONFIG path (1f2f27a/609eafd/04c63d0; same file, same place), the
64-bit `.nod64` name (870fe1c), `delete[]` fixes (c9f8080), RANDOM_SOUND_ARRAY / PRECACHE_SOUND_ARRAY
(same ranges), the HL25 `[[maybe_unused]]`-style and float-specifier commits, and the guards in 8.

Compared in full against the decompile: every function above, every rc_touch.tsv function for
these files (CBaseMonster Look/Move/MovementComplete/RunTask/StartTask, CHeadCrab::RunTask,
CScientist RunTask/StartTask/Spawn/TakeDamage, CBigMomma RunTask/StartTask, CTalkMonster
RunTask/StartTask, CHoundeye RunTask/SonicAttack, CController Move/StartTask, CAGrunt::StartTask,
CGargantua RunTask/StartTask, CIchthyosaur::RunTask, CHGrunt::StartTask, CHAssassin
RunTask/StartTask, CBullsquid::StartTask; those whose only lead was the out-of-line
HasConditions/SetConditions are stock), and every function a behavioral halflife-updated commit
touched. Skimmed (skim.py leads, each lead read): all remaining functions of the 33 files; all
AI tables compared by schedcmp.py. Not compared line by line: the big stock bodies with no lead
(CBaseMonster::StartTask beyond its leads, CTestHull::BuildNodeGraph, CApache/COsprey/CNihilanth
flight code, CIchthyosaur::Swim).

## 4. Classes

- `CKillerBabyCrab : CHeadCrab` (hierarchy.tsv). No save table in the original. Members recovered
  from code: `m_pevOwner` (+0x27c, entvars_t*, used as attacker in Killed and LeapyTouch; nothing
  in the original writes it; VERIFY name), `m_flDieTime` (+0x280, Spawn sets time+30, MonsterThink
  kills it after; VERIFY name). Its vtable overrides match vtables.tsv (Spawn, Precache, Killed,
  MonsterThink, CheckRangeAttack1, StartTask, SetYawSpeed, the three CHeadCrab virtuals).
- `CSciKillSpawner : CBaseEntity`. Saved `m_bDummy` (+0x34, FIELD_BOOLEAN, savefields.tsv), never used.
- `CScientist` gains `m_bKillSci` (+0x2f8) and `m_bFragGiven` (+0x2f9), bools from Spawn/Killed/
  GetBotTargetWeight. Not saved. Names VERIFY.
- `CIchthyosaur` gains `m_flDeathTime` (+0x2c0, unsaved; VERIFY name) and its seven new virtuals.
- `CBaseMonster` loses `m_AllowItemDropping` and its save field; gains the three bot overrides and
  the sentence overrides (`basemonster.h`, CBaseMonster's own declaration).
- `CStomp` loses `m_flLastThinkTime` and its save table (the original has none).

## 5. Cross-file needs

- `CreateSciSpawner()` (`scientist.h`) is called at map start by the original's world.cpp
  (CWorld, decompile CWorld.c:279) and from several `_free.c` callers; whoever owns world.cpp
  calls it.
- `EntSelectSpawnPoint` (player.cpp) declared extern in scientist.cpp.
- ScoreInfo: the original registers it with 5 bytes (`_free.c`, RegUserMsg("ScoreInfo",5)) and
  CScientist::Killed writes byte, short, short. Our UserMessages.cpp registers the 9-byte SDK
  message. Whoever owns UserMessages/multiplay_gamerules decides; until they agree, an
  mp_scikill frag sends a short message (VERIFY in scientist.cpp).
- scripted.cpp (not mine): the original CScriptedSentence calls `CanPlaySentence(spawnflags & 4)`;
  ours calls HL25's `IsAllowedToSpeak()`. CBaseMonster::IsAllowedToSpeak now returns IsAlive(),
  the original CanPlaySentence(true) result.
- The bot target weights read a bool at +0x69c of the bot (`BotHuntsMonsters`, static in
  barney.cpp and scientist.cpp), like weapons.cpp's `RCBotPev`; only meaningful for rcbot.dll.
- No members added to other packages' classes; no TEMP STUBs added or removed.

## 6. VERIFY list

15 VERIFY marks added. Settle by:

- Member names: `m_pevOwner`, `m_flDieTime`, `m_bKillSci`, `m_bFragGiven`, `m_flDeathTime`,
  `BotHuntsMonsters` (rcbot.dll's CBot layout would name the flag).
- ScoreInfo size (5): coordinate with the user message owner.
- The five crash guards in 8: confirm in an engine that each guarded path is the crash.

## 7. Behavior worth testing

- `sv_ignoreplayer 1`: monsters don't see players. `sv_monspawn 1`: each map-placed monster
  leaves a DM spawn point (not in water, human hull fits); `mp_scikill N` in DM: N scientists
  appear at spawn points 15 s in, run for cover, frags for killing them.
- `sv_loyalnpcs 1`: shooting scientists/barneys doesn't turn them; invisible players never provoke.
- Multiplayer: monsters drop no weapons, all corpses fade, monsters spawn in DM.
- Freeze effects (FL_FROZEN) stop monsters, tentacles, ichthyosaurs.
- Killer babycrab (`monster_killerbabycrab`): red, explodes after 30 s or on death, named tripmine.
- Player-vs-player relationship R_NM, players allied with scientists: monster targeting around players.
- Gargantua takes half from assassin_gun; houndeyes don't hurt drive_houndeye.
- Node graph rebuild prints the centre message.

## 8. Original bugs

- **CKillerBabyCrab::Killed dereferences a NULL owner** (0x1aaa90): `pev->owner = m_pevOwner->pContainingEntity`
  with no test, and nothing sets m_pevOwner, so every killer babycrab crashed the server on death.
  Guarded (owner only set when there is one).
- **CScientist::StartTask, TASK_MOVE_TO_TARGET_RANGE_SCARED** (0x206a90): tests `m_hTargetEnt == NULL`
  before the distance, so with a target it never completes early, and without one it reads the
  NULL target. The always-move part kept; the NULL read guarded (TaskFail).
- **CTalkMonster::RunTask TASK_TLK_CLIENT_STARE** (0x21e5e0): reads edict 1 even when it's NULL. Guarded.
- **CTalkMonster::CanFollow** (0x2203f0): `m_pCine->CanInterrupt()` with m_pCine NULL right after a
  script. Guarded (halflife-updated's test).
- **CBullsquid::HandleAnimEvent spit** (0xf54f0): no m_hEnemy test. Guarded (halflife-updated's).
- **CHGrunt::CheckMeleeAttack1** (0x1ac300): uninitialized enemy pointer with no enemy. Guarded (halflife-updated's).
- **COsprey::FlyThink** (0x1e0f00): NULL m_pGoalEnt dereference. Guarded (halflife-updated's).
- Kept, not crashes: scientists picking a random head after TalkInit get voice pitch 105; the
  scientist cover schedule and fear tables lack the SDK 2.3 tasks; COsprey swaps left/right smoke
  puffs; COsprey::Flight divides by a zero m_dTime when it has no target (moves to a NaN origin);
  CWreckage keeps a time in an int; CTentacle::CanPlaySequence(int) doesn't override the base's
  two-argument virtual; zombie IdleSound rolls an unused pitch; the killer babycrab's MonsterThink
  keeps thinking after Killed in the same frame; CSciKillSpawner makes the spawn point the
  scientist's enemy.
