# bm-garg: drivebm.cpp and drivegarg.cpp

The drivable Big Momma (Gonarch) and Gargantua. Addresses below are file addresses
(`reference/funcfile.tsv`; Ghidra address = file address + 0x10000).

## 1. Scope

Original source files: `drivebm.cpp` (67 functions) and `drivegarg.cpp` (71 functions).

Written or touched here:

| File | What |
|---|---|
| `dlls/drivebm.cpp` | CDriveBM, CDriveBMPickup, CDrvBSpawner, CBMHeadcrabifyer, BMHCFCreate, CreateDrvBMSpawner; TEMP STUBs (section 5) |
| `dlls/drivegarg.cpp` | CDriveGarg, CDriveGargPickup, CDrvGSpawner, CGDSpiral, DGargSpawnExplosion, CreateDrvGargSpawner; TEMP STUB CountEntsInWorld |
| `dlls/drivebm.h`, `dlls/drivegarg.h` | new: the free functions other files call (CWorld calls the two Create*Spawner) |
| `dlls/driveent.h` | `CBaseDriveEnt::m_pOwner` (coordinator's merge decision) |
| `dlls/player.h` | `m_fDrvEntAlpha` in a `// bm-garg needs:` block |

## 2. Status

```
scripts/check_file.py drivebm.cpp --missing    67 of 67 present, 0 missing
scripts/check_file.py drivegarg.cpp --missing  71 of 71 present, 0 missing
scripts/build.sh i386                          builds
scripts/build.sh amd64                         builds
nm -D build-i386/dlls/hl.so                    drive_bm pickup_drive_bm drive_bm_spawner
                                               gonarch_headcrabifier drive_garg pickup_drive_garg
                                               drive_garg_spawner dg_streak_spiral all exported
```

Built only. Nothing has run in an engine; behavior is unchecked against the original.

## 3. Function map

All are `ok` (a literal reading of the decompile, checked against the disassembly where the
decompile lost arguments) unless marked. Pairs that are the same code in both files are listed
once with both addresses (BM / Garg).

### Shared by CDriveBM and CDriveGarg (identical code, own copy in each class)

| Function | BM | Garg | Note |
|---|---|---|---|
| factory `drive_bm` / `drive_garg` | 0x13aa90 | 0x1564d0 | LINK_ENTITY_TO_CLASS (see 5, AddToPrecachedList) |
| GetControllerName | 0x13aec0 | 0x156a60 | ok |
| CheckEHop | 0x13b7c0 | 0x1575e0 | ok |
| UpdateOwnerHUD | 0x13c540 | 0x158580 | ok |
| DEClientPreThink | 0x13e160 | 0x15af60 | VERIFY euser2/euser3 (6) |
| DEClientPostThink | 0x13e7f0 | 0x15b5f0 | ok |
| DEClientEnter | 0x13e810 | 0x15b610 | ok |
| DEClientLeave | 0x13ebf0 | 0x15b9f0 | ok |
| DEClientEject | 0x13ef00 | 0x15bd00 | ok; BM kills itself with GIB_ALWAYS, Garg with GIB_NORMAL |
| EntIsBeingRemoved | 0x141970 | 0x15d220 | ok |
| GetRCCaps | 0x1419f0 | 0x15d2a0 | 0x38088 / 0x2c088 |
| VeryHeavyDamageFract | 0x141a00 | 0x15d2b0 | 0.4 |
| ExtreemeDamageFract | 0x141a20 | 0x15d2d0 | 0.05 |
| GetCamRotOrg | 0x141a40 | 0x15d2f0 | Center() |
| SetObjectCollisionBox | 0x141a80 | 0x15d330 | ±95/190 BM, ±80/214 Garg |
| BloodColor | 0x141b50 | 0x15d400 | yellow |
| Classify | 0x141b60 | 0x15d410 | CLASS_ALIEN_MONSTER |
| DoMovement | 0x13bae0 | 0x157900 | VERIFY in-air run/walk thresholds (6) |

### CDriveBM

| Function | Addr | Note |
|---|---|---|
| Spawn | 0x13aac0 | ok |
| Precache | 0x13ad80 | ok |
| DBMThink | 0x13af30 | ok; idle RANDOM args from disasm |
| DoSlash | 0x13b230 | ok |
| DoConc | 0x13b410 | ok |
| DoHCF | 0x13b510 | ok |
| DoYaw | 0x13c3a0 | ok |
| DoRegen | 0x13c460 | ok |
| TraceAttack | 0x13c660 | ok |
| TakeDamage | 0x13c7d0 | ok |
| Killed | 0x13c8f0 | ok |
| DyingThink | 0x13cbc0 | ok |
| BMCheckTraceHullAttack | 0x13cdd0 | ok; nothing calls it in the original |
| CallDoSlash | 0x13cee0 | VERIFY stale gpGlobals vectors (6) |
| ConcSplash | 0x13d730 | ok |
| HandleAnimEvent | 0x13df90 | ok; sound args from disasm |

### CDriveBMPickup / CDriveGargPickup (identical but for model, size, prefs section)

| Function | BM | Garg | Note |
|---|---|---|---|
| factory | 0x13f310 | 0x15c520 | |
| Save / Restore | 0x13f340 / 0x13f390 | 0x15c550 / 0x15c5a0 | IMPLEMENT_SAVERESTORE |
| Spawn | 0x13f3e0 | 0x15c5f0 | ok |
| Precache | 0x13f620 | 0x15c830 | ok |
| GetSpawnOrg | 0x13f670 | 0x15c880 | ok |
| TakeDamage | 0x13f6f0 | 0x15c900 | returns false |
| PDGAnimThink | 0x13f700 | 0x15c910 | ok |
| Use | 0x13f8a0 | 0x15cab0 | uses pCaller (disasm: 3rd arg); VERIFY IsPlayer before null test |
| DoRespawn | 0x13faa0 | 0x15ccb0 | ok |
| DeathNotice | 0x13fb60 | 0x15cd70 | ok |
| ObjectCaps | 0x1417f0 | 0x15d0a0 | ok |
| SetObjectCollisionBox | 0x141810 | 0x15d0c0 | ok |
| GetRCCaps | 0x1418e0 | 0x15d190 | 0 |
| IsTeleFraggabe | 0x1418f0 | 0x15d1a0 | VERIFY return type (new virtual, slot 150) |
| BloodColor | 0x141900 | 0x15d1b0 | DONT_BLEED |
| Classify | 0x141910 | 0x15d1c0 | CLASS_NONE |

### CDrvBSpawner / CDrvGSpawner

| Function | BM | Garg | Note |
|---|---|---|---|
| CreateDrv{BM,Garg}Spawner | 0x13fbd0 | 0x15cde0 | free function, called from CWorld |
| factory | 0x13fc30 | 0x15ce40 | |
| Save / Restore | 0x13fc60 / 0x13fcb0 | 0x15ce70 / 0x15cec0 | |
| DrvAThink | 0x13fd00 | 0x15cf10 | creates `mp_drivebm`/`mp_drivegarg` pickups |
| Spawn | 0x13fd90 | 0x15cfa0 | ok |
| ObjectCaps | 0x141790 | 0x15d040 | FCAP_DONT_SAVE |

### CBMHeadcrabifyer (drivebm.cpp)

| Function | Addr | Note |
|---|---|---|
| factory `gonarch_headcrabifier` | 0x13fe30 | |
| SetupPoints | 0x13fe60 | ok; VecToAngles(movedir) from disasm |
| Spawn | 0x140380 | ok |
| Precache | 0x140830 | ok (does not precache models/rpgrocket.mdl, as in the original) |
| CreateHCF | 0x140890 | static: the disasm passes 4 args, the first is pevOwner, not `this` |
| KillEnts | 0x140960 | ok |
| BMHCFTouch | 0x140a30 | ok |
| BMHCFThink | 0x140c20 | VERIFY final DHCHeadcrabifyEnt call (6); the per-hit test is the file-static helper BMHCFCanHeadcrabify (inline in the original) |
| BMHCFCreate | 0x141650 | VERIFY enemy fallback (6) |

### CDriveGarg

| Function | Addr | Note |
|---|---|---|
| Spawn | 0x156500 | ok |
| Precache | 0x1568c0 | ok |
| DGThink | 0x156ad0 | ok |
| DoFlameWeapon | 0x156ef0 | ok |
| DoSlashWeapon | 0x157170 | ok |
| DoBiteWeapon | 0x1572e0 | ok |
| DoKickWeapon | 0x157460 | ok |
| DoYaw | 0x158220 | VERIFY time added twice (6) |
| EyeUpdate | 0x1586a0 | ok |
| DoBiteDamage | 0x1588a0 | ok; GetAttachment result unused, as in the original |
| DoKickDamage | 0x158cf0 | ok |
| DoFlameDamage | 0x159080 | VERIFY driver deref without null check |
| UpdateFlames | 0x1594f0 | ok |
| CreateFlames | 0x159990 | ok |
| DestoryFlames | 0x159c00 | VERIFY dangling beam pointers (6) |
| SetRandomAnim | 0x159ca0 | returns the hold time (decompile shows void; RANDOM_FLOAT in ST0 is returned) |
| TraceAttack | 0x159d80 | ok |
| TakeDamage | 0x159f30 | VERIFY driver deref without null check |
| DGargSpawnExplosion | 0x15a080 | free function (HL's SpawnExplosion) |
| Killed | 0x15a180 | ok |
| DyingThink | 0x15a3d0 | ok |
| GargCheckTraceHullAttack | 0x15a920 | ok |
| HandleAnimEvent | 0x15ab00 | ok; slash knock 180 (rodata), args from disasm |
| DeathEffect | 0x15adc0 | ok |

### CGDSpiral (drivegarg.cpp, HL's CSpiral)

| Function | Addr | Note |
|---|---|---|
| factory `dg_streak_spiral` | 0x1564a0 | |
| Spawn | 0x15c110 | ok |
| Create | 0x15c190 | ok |
| Think | 0x15c240 | ok; overrides CBaseEntity::Think like HL |
| ObjectCaps | 0x15d470 | FCAP_DONT_SAVE |

## 4. Classes

Bases from `hierarchy.tsv`: CDriveBM, CDriveGarg : CBaseDriveEnt; the pickups : CBaseMonster;
spawners, CBMHeadcrabifyer, CGDSpiral : CBaseEntity. No class introduces virtuals beyond its
base except the pickups' `IsTeleFraggabe` (slot 150).

Saved members (from `savefields.tsv`, in order): CDriveBMPickup `m_pGivenBM` (CLASSPTR, +0x27c),
`m_bTakeable` (BOOLEAN, +0x280); CDriveGargPickup `m_pGivenGarg`, `m_bTakeable` (same offsets);
CDrvBSpawner/CDrvGSpawner `m_bDummy` (BOOLEAN, +0x34). CDriveBM and CDriveGarg have no save table
in the original (no Save/Restore), so none here, and their `m_pOwner` is not saved either.

The driver at +0x27c is the inherited `CBaseDriveEnt::m_pOwner`. All other members are unsaved
and named from what they do (marked VERIFY name in the code where the name is a guess); the
original offsets are in comments next to each. Garg members follow HL's gargantua.cpp names where
the code is HL's (`m_flameX`, `m_flameY`, `m_streakTime`, `m_pEyeGlow`, `m_eyeBrightness`);
HL's global `gGargGibModel` is a member here (`m_iGibModel`, +0x304).

CDriveBM: +0x280 m_flInterval, +0x284 m_iLastHealth, +0x288 m_vecLastOrigin, +0x294
m_flLastOriginTime, +0x298 m_bCanMove, +0x299 m_bMoving, +0x29a m_bCanSlash, +0x29b m_bCanConc,
+0x29c m_bCanHCF, +0x2a0 m_flNextIdle, +0x2a4 m_bCanIdle, +0x2a5 m_bInSlash, +0x2a8 m_flNextSlash,
+0x2ac m_flLastSlash, +0x2b0 m_bSlashSide, +0x2b1 m_bInConc, +0x2b4 m_flNextConc, +0x2b8 m_bInHCF,
+0x2bc m_flNextHCF, +0x2c0 m_iHCFRingsLeft, +0x2c4 m_flNextHCFRing, +0x2c8 m_vecMoveVel, +0x2d4
m_flMoveTime, +0x2d8 m_flNextEHop, +0x2e0 m_flPainTime, +0x2e4 m_iShockwaveSprite.

CDriveGarg: +0x280..+0x294 as BM; +0x298 m_bCanTurn, +0x299 m_bCanMove, +0x29a m_bMoving, +0x29b
m_bCanIdle, +0x29c m_bCanFlame, +0x29d m_bCanSlash, +0x29e m_bCanBite, +0x29f m_bCanKick, +0x2a0
m_bCanYaw, +0x2a4 m_flNextIdle, +0x2a8 m_flNextFlameTime, +0x2ac/+0x2b0/+0x2b4 m_flNextSlash/Bite/
Kick, +0x2b8/+0x2bc m_flBite/KickDamageTime, +0x2c0..+0x2c2 m_bSlash/Bite/KickReady, +0x2c3
m_bInMelee, +0x2c4 m_vecMoveVel, +0x2d0 m_flMoveTime, +0x2d4 m_flNextEHop, +0x2d8 m_flameX, +0x2dc
m_flameY, +0x2e0 m_bFlameOn, +0x2e1 m_bFlaming, +0x2e4 m_flFlameMinTime, +0x2e8 m_streakTime,
+0x2ec m_pFlame[2], +0x2f4 m_pFlameInner[2], +0x2fc m_flDieTime, +0x300 m_flPainTime, +0x304
m_iGibModel, +0x308 m_pevKiller, +0x30c m_pEyeGlow, +0x310 m_eyeBrightness.

CBMHeadcrabifyer: +0x34 m_pBeams[8], +0x54 m_pRingEnts[8], +0x74 m_vecPoints[8], +0xd4
m_flRadius, +0xd8 m_bTraceToggle.

Data tables (from `.data`): `g_sDBM*`/`g_sDMBPainSounds`/`g_szDBMBlastSounds`,
`g_sDG*Sounds`, and `g_sDGIdleAnims` (struct `sDGAnimT`: name, weight, min, max hold time:
idle2 0.3 16-24, idle1 0.25 14-20, idle4 0.4 11-18).

## 5. Cross-file needs (merge work)

- `CBaseDriveEnt::m_pOwner` added to `dlls/driveent.h` per the coordinator.
- `CBasePlayer::m_fDrvEntAlpha` (0x674) in a `// bm-garg needs:` block in `dlls/player.h`.
- TEMP STUBs in `dlls/drivebm.cpp`: `PREFS_KeyLoopUpInt/Float/Bool` (return the default; real ones
  in Preff.cpp, same `const char*` signatures as Preff.h) and `DHCHeadcrabifyEnt(edict_t*, edict_t*)`
  (empty; real one in drivehc.cpp, chum-icky-hc, orig 0x161090). Until it is real, the
  headcrabifier ring hits nothing.
- TEMP STUB in `dlls/drivegarg.cpp`: `int CountEntsInWorld(const char*)` (combat.cpp, orig
  0x1058e0), with the original's body. The original takes `char*`; whoever does combat.cpp must
  match `const char*` or the declaration here changes.
- `StreakSplash` is declared extern from gargantua.cpp (the SDK's, non-static there).
- `CreateDrvBMSpawner()` / `CreateDrvGargSpawner()` are declared in the new headers; CWorld
  (world.cpp) has to call them on map start, as the original does.
- Every original factory also calls `AddToPrecachedList(classname)`; the tree's
  `LINK_ENTITY_TO_CLASS` (util.h) does not. That is one macro change for all packages, not made here.
- Uses `gmsgHealth`, `gmsgHideWeapon`, `gmsgSetFOV`, `gmsgBattery`, `gmsgFlashBattery`, `gmsgTrain`
  (UserMessages.h), `SetupVisibility` (client.h), cvars by name `mp_drivebm`, `mp_drivegarg`,
  `sv_allowglobaleject`, `sv_lowlag` (already registered in game.cpp).

## 6. VERIFY list

Besides the member names marked `VERIFY name`:

- Both DEClientPreThink: with an alpha in (0,1) the player gets `euser2 = monster`, otherwise the
  original clears `euser3`, not `euser2` (offsets 0x298 vs 0x29c, read from the disasm). Leave
  clears euser2. Kept literal; an engine run shows whether the client side cares.
- Both DoMovement, in the air: 30-120 u/s plays "run", 120 and up plays "walk". Literal; looks
  swapped but the disasm agrees.
- CDriveBM::CallDoSlash computes its own forward/right with pfnAngleVectors (used for the trace)
  but the damage direction and knockback use gpGlobals->v_forward/v_right, which still hold
  whatever the last UTIL_MakeVectors set (in practice DoHCF/DoMovement/DEClientPreThink's view
  angles). Runtime test settles which way players get knocked.
- CDriveGarg::DoYaw, on a fast turn-around: `m_flNextIdle = RANDOM_FLOAT(3,8) + time + time`
  (disasm `fadd st0,st0`). Effect: no idle animation for a long time after a 180.
- CBMHeadcrabifyer::BMHCFThink calls DHCHeadcrabifyEnt on whatever the last trace hit, qualified
  by the client/monster/cap test or not. Depends on DHCHeadcrabifyEnt's own checks.
- BMHCFCreate passes the owner's entvars as enemy when the enemy edict is NULL (gcc 2.95 output;
  never happens from DoHCF, which always passes the driver).
- DestoryFlames calls SUB_Remove on the beams directly and keeps the pointers; UpdateFlames only
  runs after CreateFlames remakes them, so nothing reads them, but they dangle.
- CDriveGarg::TakeDamage and DoFlameDamage dereference the driver without a null test (the
  original would crash if an empty garg were burned or damaged before DGThink kills it).
- Pickup Use calls `pCaller->IsPlayer()` before testing `pCaller` for NULL.
- Pickup `IsTeleFraggabe` return type is a guess (`bool`); it returns 0 and nothing calls it.
- Monster Killed/DyingThink call `CBaseEntity::Instance(pev->owner)->DeathNotice(pev)` with no
  null check on the instance, as the original.

## 7. Behavior worth testing

- Using a pickup: the player becomes an invisible camera 300 units behind; HUD health is the
  monster's percent; +forward walks (+speed/+use walks slower), turning follows the view, a quick
  180 when standing still.
- Big Momma: +attack alternates arm slashes, +attack2 alone the two-armed slash, +duck the stomp
  shockwave (beam cylinder, knockback through thin walls), +jump fires two headcrabifier rings;
  she regenerates per living drive_headcrab.
- Gargantua: +attack flamethrower with head aiming, +attack2 slash, +jump bite (heals), +duck kick;
  only blast/mortar/energy/crush damage hurts it, other hits ricochet.
- Eject (`AllowEject` in rc.ini or `sv_allowglobaleject 1`) throws the player backward and kills
  the monster; death of the monster kills the driver; the pickup respawns 8 s after (rc.ini
  `...RespawnDelayAfterDeath`) and kills an overdue monster after `...MaxDriveTime`.
- Pickups appear only on maps with `info_drive_bm_spawn` / `info_drive_garg_spawn`, 15 s after
  map start, `mp_drivebm`/`mp_drivegarg` of them.
