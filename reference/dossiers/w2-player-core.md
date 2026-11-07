# w2-player-core: the player, and RC's changes in combat, subs, cbase, util, animation, sound, chargers, pm_shared

## 1. Scope

Original files: player.cpp, combat.cpp, subs.cpp, cbase.cpp (remaining stock differences),
util.cpp, animation.cpp, sound.cpp, h_battery.cpp, healthkit.cpp, pm_shared.c, pm_math.c, and the
header-inline CGib::GetRCCaps.

Touched here: `dlls/player.cpp`, `dlls/player.h`, `dlls/combat.cpp`, `dlls/subs.cpp`,
`dlls/cbase.cpp`, `dlls/cbase.h`, `dlls/util.cpp`, `dlls/util.h`, `dlls/animation.cpp`,
`dlls/animation.h`, `dlls/sound.cpp`, `dlls/h_battery.cpp`, `dlls/healthkit.cpp`,
`dlls/monsters.h`, `pm_shared/pm_shared.cpp`, `pm_shared/pm_math.cpp`.

## 2. Status

Built on i386 and amd64, not run in an engine. `scripts/check_file.py <file> --missing`:

```
player.cpp     134/134   0 missing
combat.cpp      39/39    0 missing
subs.cpp        53/53    0 missing
cbase.cpp      117/123   6 missing (see below)
util.cpp       128/128   0 missing
animation.cpp   18/18    0 missing
sound.cpp       45/45    0 missing
h_battery.cpp   13/13    0 missing
healthkit.cpp   19/19    0 missing
pm_shared.c     51/51    0 missing
pm_math.c       20/20    0 missing
```

The six cbase.cpp misses are stock SDK functions whose gcc2 names check_file can't match: EHANDLE's
`operator CBaseEntity*`, `operator=` and `operator->` exist under their real names;
`operator int`, `operator new(size_t, entvars_t*)` and `eoffset` were removed by halflife-updated
and nothing calls them, so they weren't added back.

`scripts/build.sh i386 client` fails, at HEAD too: `cl_dll/particleman/CMiniMem.h: fatal error:
memory_resource` (the toolchain image lacks the header). pm_shared.cpp and pm_math.cpp were
compiled alone for the client target (CLIENT_DLL) and build.

The TEMP STUB for `CBasePlayer::PlayerDoID` (from the panth-squid-turret merge) is replaced by the
real function; its caller in driveturret.cpp needed no change.

## 3. Function map

### player.cpp

Compared against the decompile and, where Ghidra lost code or the logic mattered, the
disassembly: every RC-added function, every rc_touch function, and the ones below marked
"compared". The rest were read in the decompile and found stock ("skimmed").

| orig | function | status |
|---|---|---|
| 0x1e9970 | LinkUserMessages (in UserMessages.cpp here) | compared; not changed, differences in section 5 |
| 0x1e9e00, 0x1ea040 | Pain, DeathSound | skimmed, stock |
| 0x1ea160 | TraceAttack | skimmed, stock |
| 0x1ea2c0 | TakeDamage | compared; RC: a refused (teammate) hit still pushes the player with mp_teampush 1 |
| 0x1eabc0, 0x1eaf20 | PackDeadPlayerItems, RemoveAllItems | skimmed, stock |
| 0x1eb0c0 | StartDyingCam | ok (disassembly); the "dying_cam" camera |
| 0x1eb6a0 | Killed | compared (disassembly); see "differences" below |
| 0x1ebbe0 | SetAnimation | compared, stock |
| 0x1ec080, 0x1ec590 | WaterMove, IsOnLadder | skimmed, stock |
| 0x1ec5b0 | PlayerDeathThink | compared; RC: SET_VIEW to the dying cam, death angles held, FL_FROZEN cleared |
| 0x1ec910 | StartDeathCam | compared (disassembly); RC: SET_VIEW back to the player first |
| 0x1ecb50 | StartObserver | compared; the old SDK's 9-line version |
| 0x1ecc10 | PlayerUse | compared; RC: pickup_drive_apache found by an 80-unit trace |
| 0x1ed140 | Jump | ok (decompile + disassembly); server-side jump from rc.ini, long jump starts the butt stomp |
| 0x1ed4d0 | FixPlayerCrouchStuck | stock |
| 0x1ed530 | Duck | ok (disassembly); the old SDK's server-side duck |
| 0x1ed990 | Classify | ok; observer CLASS_NONE, driver takes the driven monster's class |
| 0x1eda10, 0x1edb40 | AddPoints, AddPointsToTeam | compared; ScoreInfo kept at halflife-updated's 9 bytes (section 5) |
| 0x1edbf0 | PlayStepSound | ok |
| 0x1ee1a0 | MapTextureTypeStepType | ok |
| 0x1ee260 | UpdateStepSound | ok (disassembly); no sound, see bug 3 |
| 0x1ee750 | PlayerDoID | ok (decompile + disassembly) |
| 0x1eef90 | DoPlayerShockwaveEffect | ok |
| 0x1ef190 | ButStompTouch | ok (disassembly); VERIFY the brush-model test |
| 0x1ef420 | PreThink | compared; observer, respawn, drive hand-off, ID, curse messages, regeneration, stomp |
| 0x1efbf0 | BloodColor | ok (inline, player.h) |
| 0x1efc10..0x1f0060 | CheckTimeBasedDamage, UpdateGeigerCounter, CheckSuitUpdate, SetSuitUpdate | skimmed, stock |
| 0x1f0260 | CheckPowerups | ok; made non-static |
| 0x1f02a0 | UpdatePlayerSound | compared; sv_ignoreplayer already in the tree |
| 0x1f0520 | PostThink | compared (disassembly); no fall damage, see differences |
| 0x1f0970 | IsSpawnPointValid | ok; anything alive within 64 |
| 0x1f09f0 | PickRandomSpawnOfClassname | ok |
| 0x1f0c10 | DoEntSpawnPointStuff | ok; VERIFY self-telefrag |
| 0x1f0d70 | EntSelectSpawnPoint | ok |
| 0x1f1000 | Spawn | ok (disassembly, Ghidra split it into fragments) |
| 0x1f1d00 | Precache | compared; node graph check is back here, m_iTrain assigned |
| 0x1f1dd0..0x1f22e0 | Save, RenewItems, Restore, Select*, HasWeapons, TeamID | skimmed, stock |
| 0x1f2310..0x1f2640 | CSprayCan, CBloodSplat | skimmed, stock |
| 0x1f2720 | GiveNamedItem | ok; precache-list and classname-table checks |
| 0x1f28d0 | FindEntityForward | ok; no caller in the original |
| 0x1f2a30..0x1f2c70 | Flashlight*, ForceClientDllUpdate, ImpulseCommands | skimmed, stock |
| 0x1f2f10 | AllowToTransform | ok |
| 0x1f2f70 | CheatImpulseCommands | ok (disassembly, jump table decoded) |
| 0x1f3e90..0x1f43d0 | AddPlayerItem, RemovePlayerItem, GiveAmmo, ItemPre/PostFrame, AmmoInventory, GetAmmoIndex, SendAmmoUpdate | skimmed, stock |
| 0x1f44a0 | UpdateClientData | compared (disassembly for the init-HUD part, health, train) |
| 0x1f4ea0, 0x1f4eb0 | GetBotTargetWeight (7), BotTargetRealseable (true) | ok (inline) |
| 0x1f4ec0..0x1f5fe0 | FBecomeProne .. SwitchWeapon, autoaim, DropPlayerItem | skimmed, stock (DropPlayerItem returns the box in the original; nothing uses it) |
| 0x1f6060..0x1f6c40 | CDeadHEV, CStripWeapons, CRevertSaved, CInfoIntermission | skimmed, stock |
| 0x1f6cc0..0x1f6d60 | CRevertSaved::Set*/Get* | ok; out of line, as gcc 2.95 emitted them |
| 0x1f6ee0 | GetRCCaps | ok; 0 while acting as cam, else 0x7fe3fff |
| 0x1f6f20, 0x1f6f30 | GetGuassDistOut (56), GetGuassHull (human_hull) | ok |
| 0x1f6f40 | IsAlive | ok; false while observing or driving |
| 0x1f6fa0 | IsActingAsCam | ok (already in the tree) |
| 0x1f6fc0 | GetBotShootOrg | ok; Center() - 18 z |
| 0x1f7030..0x1f7110 | ObjectCaps .. BodyTarget | skimmed, stock |

Differences from halflife-updated in player.cpp, original's behavior taken:

- **Killed**: no Holster of the active weapon; drops the CTF flag (message, screen fade); takes
  the driver out of his monster (DEClientLeave, the monster is killed if alive and the player is
  hidden at its origin); returns early when `pev->iuser4 == 1`; no `pev->modelindex` reset;
  `takedamage = DAMAGE_NO`; the dying cam; an invisible player neither gibs nor plays a death
  sound; `v_angle` (not `angles`) x/z zeroed and saved in `m_vecDeathAng`; SOLID_NOT always (the
  hl-updated version only did that in multiplayer).
- **PlayerDeathThink**: counts 120 think frames (the original's m_iRespawnFrames), not 4 s of
  frametime; no copy to the body queue or MOVETYPE_NONE for a finished corpse; `pev->frame = 255`;
  no spectator (`iuser1`) early-out.
- **StartObserver**: the old death-camera version (PFLAG_OBSERVER, view, no model), not
  halflife-updated's spectator mode. client.cpp's "spectate" now gets that.
- **PlayerUse**: no IsObserver early-out.
- **Jump / Duck**: back on the server (old SDK code), see section 7.
- **PreThink**: m_afButtonLast is set at the start; no m_bResetViewEntity handling, no node graph
  check (it's in Precache, as in the original), no Observer_* buttons.
- **PostThink**: no fall damage; fall pain sounds and a landing step sound.
- **EntSelectSpawnPoint**: random free spots, telefrags, info_teamspawn, monster fallbacks; no
  g_pLastSpawn, no gpGlobals->startspot.
- **Spawn**: see Spawn's comment. Kept from halflife-updated: the FL_PROXY/FL_FAKECLIENT
  preservation (the original sets flags to FL_CLIENT), the "bj" physics key, m_bIsSpawning,
  m_flStartCharge, m_ClientSndRoomtype.
- **UpdateClientData**: no status bar, no "no weapon" CurWeapon message, health not clamped to
  short or bumped to 1, train HUD always 0. Kept: the Weapons bits message, the m_bRestored
  flashlight resend and the room type message (halflife-updated infrastructure, invisible to a
  stock client or singleplayer-only).
- **CheatImpulseCommands**: only 33-46 (drive transforms), 101, 111, 112.
- Not in the original, left in place and now uncalled: InitStatusBar/UpdateStatusBar, the
  halflife-updated Observer_* functions.
- **TakeDamage**: the HLTV director message (MSG_SPEC) kept; it only reaches HLTV proxies.

### Other files (by the forked worker)

| orig | function | file | status |
|---|---|---|---|
| 0x1058e0 | CountEntsInWorld | combat.cpp | ok |
| 0x105960 | ThrowFakeGibs | combat.cpp | ok; sv_lowlag 1 returns; per PVS net client with m_iGibExtra > 0 (capped 254): MSG_ONE_UNRELIABLE TE_BREAKMODEL, BREAK_FLESH |
| 0x105fa0, 0x106320, 0x1066c0 | CGib::SpawnStickyGibs/SpawnHeadGib/SpawnRandomGibs | combat.cpp | ok; RC gib count caps (45/44) |
| 0x106b80 | CBaseMonster::GibMonster | combat.cpp | ok; ThrowFakeGibs after the random gibs |
| 0x107b30 | CBaseMonster::TakeDamage | combat.cpp | ok; negative damage heals; mp_deathballsonly; curse damage scale and mirror; inflictor offset z 18; drive monsters skip the enemy update |
| 0x21c470, 0x21c490 | CSemiNullEntity::Spawn, info_semi_null | subs.cpp | ok |
| 0x21c550-0x21c7f0 | info_*_spawn, info_flagspawn, info_teamspawn | subs.cpp | ok, all CBaseDMStart |
| 0x21c820, 0x21c9d0 | CBaseDMStart::KeyValue, Spawn | subs.cpp | ok; team_no into pev->button |
| 0x21cac0 | CBaseEntity::SUB_Remove | subs.cpp | ok; DispatchRemoveEnt, TE_KILLBEAM |
| 0xfc050 | CBaseEntity::Create | cbase.cpp | ok; precache list, classname table |
| 0xfa7d0 | DispatchThink | cbase.cpp | ok; skips dead or inconsistent edicts |
| 0xfb1a0 | EHANDLE::Get | cbase.cpp | ok |
| 0xfb2c0, 0xfb370 | CBaseEntity::TakeHealth, TakeDamage | cbase.cpp | ok; negative amounts swap |
| 0x2319e0 | UTIL_GetNextBestWeapon | util.cpp | ok, no caller |
| 0x2349d0, 0x234ac0 | UTIL_Remove, UTIL_PrecacheOther | util.cpp | ok |
| 0xe8740-0xe87c0 | GetSequenceCount, PrecacheAllSeq, DumpAllSeq | animation.cpp | ok |
| 0x2156a0, 0x215a30, 0x215ae0 | memfgets, TEXTURETYPE_MkTeAndPlaySound, TEXTURETYPE_PlaySound | sound.cpp | ok |
| 0x1a1370-0x1a1a10 | CRecharge Spawn/Recharge/Use/Bot* | h_battery.cpp | ok; rc.ini ITEMS keys; VERIFY bot field |
| 0x1ab180-0x1abc40 | CHealthKit/CWallHealth MyTouch/Spawn/Recharge/Use/Bot* | healthkit.cpp | ok; rc.ini ITEMS keys; VERIFY bot field |
| 0xde700, 0xde7e0, 0xe1290 | GetChaseOrigin, PM_SpectatorMove, PM_GetInfo | pm_shared.cpp | ok; server build uses the SDK 2.0 spectator move |
| 0xdfaf0, 0xe0170 | PM_Jump, PM_CheckFalling | pm_shared.cpp | ok; server build: no bunny-hop cap, no frozen test; SDK 2.0 falling |
| 0xdb170-0xdb200 | _DotProduct, _VectorSubtract, _VectorAdd, _VectorCopy | pm_math.cpp | ok |
| 0x257e00 | CGib::GetRCCaps | monsters.h | ok (0x5c) |

PM_PlayStepSound, PM_UpdateStepSound, PM_MapTextureTypeStepType, PM_CatagorizeTextureType were
compared against the disassembly: stock. The rc_touch hits on GetBodygroup, EMIT_GROUPNAME_SUIT,
TEXTURETYPE_Find and PM_Init are false leads. The rest of these files was skimmed.

halflife-updated behavior replaced in those files: CHealthKit::MyTouch's dead-player check is
gone; TEXTURETYPE_PlaySound no longer asks PlayTextureSounds, so deathmatch plays texture sounds;
info_player_start is a CBaseDMStart.

## 4. Classes

- **CBasePlayer** (player.h). New declarations, in the class's RC block: StartDyingCam,
  PlayStepSound, UpdateStepSound, ButStompTouch, and inline overrides BloodColor, GetRCCaps,
  GetGuassDistOut, GetGuassHull, GetBotShootOrg, GetBotTargetWeight, BotTargetRealseable.
  IsAlive now also requires !IsActingAsCam(). Member offsets used, from the save table or code:
  m_blIsObserver 0x600, m_blWasObserver 0x601, m_blObserverRespawn 0x602, m_flNextVoteTime 0x620,
  m_iGibExtra 0x684 (from the "extragibs" userinfo, 0-255), m_flBloodScale 0x680 ("bloodscale"),
  m_fDrvEntAlpha 0x674 ("driveentalpha", 0-1). +0x688 is written once in Spawn (time) and read
  nowhere; mapped to halflife-updated's m_flNextChatTime (VERIFY). m_chTextureType is +0x431 and
  m_szTextureName +0x424 (from UpdateStepSound's strncasecmp of 12).
- **CRevertSaved**: accessors moved out of line. No other class changes in player.cpp.
- **CSemiNullEntity : CBaseEntity** (subs.cpp), no members. CBaseDMStart gained Spawn.
- **CRecharge, CHealthKit, CWallHealth**: non-virtual Bot* methods.
- **CGib**: inline GetRCCaps (monsters.h).

## 5. Cross-file needs

- **User messages (UserMessages.cpp, not changed; the coordinator's call).** The original's
  LinkUserMessages differs from the tree: it registers `SelAmmo` (4), `Health` as 1 byte (tree 2),
  `ScoreInfo` as 5 bytes (tree 9), and has no `TeamNames`, `StatusText`, `StatusValue` or
  `Weapons`. The drive monsters (drive*.cpp) already write Health as one byte into the 2-byte
  registration. player.cpp keeps the tree's formats (Health as a short, 9-byte ScoreInfo) so it
  agrees with the registration; whoever settles the wire formats should make all writers match.
- **TEMP STUBs at the bottom of player.cpp**: `DestroyFreezeTimer(entvars_t*)` (hornetgun.cpp,
  orig 0x1b0c60) and `DestroyFrictionTimer(entvars_t*)` (handgrenade.cpp, orig 0x1a3860), both
  empty. Killed calls them.
- **TEMP STUB at the bottom of combat.cpp**: `short g_iHGibModel, g_iAGibModel`; real ones belong
  in world.cpp (CWorld::Precache 0x23f450 sets them).
- **Used through other packages' stubs**: `GetTeamNameForIndex` (flag.cpp stub, real one in
  teamplay_gamerules.cpp), `IsClientListenServer` (vote_kick.cpp stub, real one in client.cpp;
  until then impulses 45/46 never work).
- **Defined here for others**: `int g_iPlayerShockwave` (player.cpp, extern in player.h);
  client.cpp's ClientPrecache (0x103690) must set it to `PRECACHE_MODEL("sprites/shockwave.spr")`,
  else the stomp ring uses model 0. `ThrowFakeGibs` (cbase.h) for client.cpp's
  CPopOMatic::POMThink (0xff0a0). cbase.cpp's `g_pFakeConstClassnames`, `g_iCntFakeClassnames`,
  `g_bUnsafeCreate` (cbase.h): client.cpp's ClientCommand sets g_bUnsafeCreate, world.cpp's
  CWorld::Spawn resets the count and CWorld::Precache clears g_bUnsafeCreate.
- **Wave-1 section 5 callers wired here**: PreThink → ObserverFrame, DrawCursedMsg,
  DEClientPreThink, PlayerDoID, PREFS F/I/IC; Spawn → InitObserver, PREFS B/F/I; UpdateClientData →
  PrintRocketCrowbarMsg, m_flNextVoteTime; Jump, ButStompTouch, PostThink → PREFS F; Killed →
  DoesEntHaveFlag, FlagDrop, GetFlagTeamColor, GetFlagBaseColor, DEClientLeave,
  DispatchPlayerKilled; CheatImpulseCommands → DEClientEnter; PostThink → DEClientPostThink
  (unreachable, bug 1); m_fDrvEntAlpha from userinfo in Spawn.
- **Suggestion (game.cpp, not mine)**: the server pm_shared no longer caps bunny-hops, so
  `sv_allowbunnyhopping` defaulting to 1 would keep HL25 clients from mispredicting.

## 6. VERIFY list

1. Killed returns early when `pev->iuser4 == 1`; nothing in RC's code sets iuser4 on a player.
   Settle with a grep of the other wave-2 files once merged.
2. Health is a byte in the original, a short here (section 5). Two marks (Killed, UpdateClientData).
3. ButStompTouch: a trace that misses (fraction 1) still counts as ground when its pHit is a
   brush model. Literal reading of the disassembly; runtime test.
4. DoEntSpawnPointStuff kills everything alive within 64 units of a forced spot, which could be
   the spawning player himself. Runtime test.
5. +0x688 mapped to m_flNextChatTime.
6. Spawn treats the active item as a weapon (the original writes weapon fields unconditionally);
   ours skips the writes for a non-weapon item.
7. CheatImpulseCommands 33-39 don't check Create's result (crash as the original if it fails).
8. h_battery.cpp / healthkit.cpp GetBotWeight compare a field at bot+0x2c with 0; settle against
   the RC bot binary. BotCanHaveItem casts a CBot* to CBaseEntity* (no caller here).
9. info_semi_null: nothing in the original looks it up; presumably for maps.

## 7. Behavior worth testing

- Dying camera: after death the view jumps to a point up to 200 units away looking at the body,
  and comes back at the death cam / respawn.
- Player ID: aiming at a player or a driven monster within 4096 shows its name in orange.
- Pregame observer (sv_pregameobserver 1): joining puts the player in observer; jumping spawns him.
- Jumping: JumpHeight, long jump (ducking, with the module, within 1 s of ducking) and the butt
  stomp on landing (LongJumpStompDmg, the ring and gar_stomp1).
- Ducking: server-side 0.4 s duck; standing up blocked under low ceilings.
- No fall damage; fall pain sounds (both sounds on one of the two random picks).
- Regeneration ([PLAYER] HealthRegenTime/Max/Amount, ArmorRegenTime/Max/Amount, MaxArmor).
- Start health, armor, max health, StartWithlongJump from rc.ini.
- Spawn: random free spots, telefrag on a full map, info_teamspawn by team in teamplay.
- mp_teampush 1: teammates push each other without damage.
- Impulses 33-46 with sv_cheats and sv_allowtransform 1, 111, 112.
- Using a pickup_drive_apache from in front of it.
- Flag carrier death drops the flag with the "lost the flag" message.
- Train HUD stays blank.
- The extragibs/bloodscale/driveentalpha userinfo keys.

## 8. Original bugs

1. **PostThink's drive hand-off is dead code.** It returns when `!IsAlive()` before calling
   `DEClientPostThink`, and IsAlive is false for a driver (IsActingAsCam). No drive monster's
   DEClientPostThink is ever called from the player. Kept.
2. **PostThink's fall pain switch has no break**: one of the two random picks plays both
   pl_fallpain2 and pl_fallpain3. Kept.
3. **UpdateStepSound plays nothing**: its PlayStepSound call is gone in the original (the step
   type and volumes compile away), so only pm_shared plays walking steps. Kept.
4. **Falling never hurts**: PostThink asks FlPlayerFallDamage only to decide the splat sound. Kept.
5. **PickRandomSpawnOfClassname caps its count at 63** in a 64-slot table, so past 63 spots the
   last slot is overwritten and never picked. Kept.
6. **PlayerDoID, Jump, PostThink** discard a RANDOM_LONG result (Jump's long jump, PostThink's
   landing). Kept, the calls stay for RNG parity.
7. **UpdateClientData always writes 0 to the Train message**, so the train speed HUD never shows.
   Kept.
8. **CheatImpulseCommands 33-39** use Create's result without checking it. Kept.
9. **DoEntSpawnPointStuff** can telefrag the spawning player. Kept.
10. From the other files: CBaseMonster::TakeDamage with mp_deathballsonly 1 reads
    `pevInflictor->pContainingEntity` unchecked (NULL inflictor crashes); CBaseDMStart::KeyValue
    marks "master" unhandled after storing it; CRecharge::BotCanHaveItem clamps MaxArmor to 250
    where Use clamps to 999, CHealthKit::BotCanHaveItem tests health < 100 not max_health; BotUseMe
    discards BotCanHaveItem's result; GetBotWeight compares a field the game never writes;
    "sound_emiiter" classname typo; PM_CheckFalling sets punchangle[2] but clamps punchangle[0]
    (stock SDK); Create test-spawns an entity for every call; UTIL_GetNextBestWeapon,
    GetSequenceCount and DumpAllSeq have no callers. All kept.
