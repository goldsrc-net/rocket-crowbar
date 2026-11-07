# w2-shotgun-egon-crowbar: shotgun.cpp, egon.cpp, crowbar.cpp

Rocket Crowbar 1.9's three most changed weapons: the rocket crowbar, the scientist / space-rift
shotgun, and the tractor-beam / IsoBomb / mirv egon. Addresses are file addresses
(`reference/funcfile.tsv`; Ghidra address = file address + 0x10000). These files are mostly RC
code, so every function was compared against the decompile, and against the disassembly wherever
Ghidra split or truncated a function (marked "disasm" below).

## 1. Scope

Original source files: `crowbar.cpp` (31 functions), `shotgun.cpp` (40), `egon.cpp` (58), plus the
inline ammo classes the original puts in "(header)" but our tree keeps in these files
(`CShotgunAmmo` @ 0x252ac0-0x252bb0, `CEgonAmmo` @ 0x24bae0-0x24bbd0, ported here because they
use PREFS).

| File | What |
|---|---|
| `dlls/crowbar.cpp` | CCrowbar (all stock bodies brought in line, RC methods), CCrowbarRocket (RC class, no factory) |
| `dlls/shotgun.cpp` | CShotgun, CFlyinScientist (`pipebomb`), CSpaceRift (`shotgun_rift`, no factory), CShotgunAmmo, the g_iSuck*/g_iShot* sprite globals |
| `dlls/egon.cpp` | CEgon, CIsoBomb (`IsoBomb`), CEgonMirv (`egon_mirv`), CEgonAmmo, g_iTractorBeamSpr, g_fireAnims1/2 |
| `dlls/weapons.h` | inside `class CCrowbar`, `class CShotgun`, `class CEgon` only |
| `dlls/weapons.cpp` | the `CEgon::m_SaveData` table only |

## 2. Status

```
scripts/check_file.py crowbar.cpp --missing   31 of 31 present, 0 missing
scripts/check_file.py shotgun.cpp --missing   40 of 40 present, 0 missing
scripts/check_file.py egon.cpp --missing      58 of 58 present, 0 missing
scripts/build.sh i386                         builds (no warnings from these files)
scripts/build.sh amd64                        builds (no warnings from these files)
nm -D build-i386/dlls/hl.so                   weapon_crowbar weapon_shotgun pipebomb ammo_buckshot
                                              weapon_egon egon_mirv IsoBomb ammo_egonclip exported
```

Built only. Nothing has run in an engine. The client library (`cl_dll`, which also compiles these
files) was not built; RC code here is not guarded by `CLIENT_DLL`.

## 3. Function map

All `ok` = literal reading, checked against the disassembly where noted.

### crowbar.cpp

| Orig | Function | Status |
|---|---|---|
| 0x1126d0 / 0x112720 | CCrowbarRocket::Save / Restore | ok, 7 fields (the original calls CBaseMonster's; ours IMPLEMENT_SAVERESTORE on CGrenade, same chain) |
| 0x112770 | CCrowbarRocket::CreateRpgRocket | ok |
| 0x112820 | CCrowbarRocket::Spawn | ok, key "DefualtSpeed" (string at 0x26b070) |
| 0x112da0 | CCrowbarRocket::RocketTouch | ok |
| 0x112e10 | CCrowbarRocket::Precache | ok |
| 0x112e70 | CCrowbarRocket::FollowThink | ok, disasm for the int truncation |
| 0x113200 | weapon_crowbar | ok |
| 0x113230 | CCrowbar::GetBotWeight | ok, disasm |
| 0x1133f0 | CCrowbar::BotWeaponFrame | ok, disasm; discarded GetBotShootOrg calls kept |
| 0x113540 / 0x1135e0 | CCrowbar::BotFirePrimary / BotFireSecondary | ok |
| 0x113680 | CCrowbar::Spawn | ok, same as stock |
| 0x1136e0 | CCrowbar::Precache | ok, no event |
| 0x1137a0 | CCrowbar::GetItemInfo | ok, same as stock |
| 0x113820 / 0x113860 | CCrowbar::Deploy / Holster | ok, same as stock |
| 0x1138b0 | FindHullIntersection | ok; original takes float* mins/maxs, same traces, stock kept |
| 0x113ae0 | CCrowbar::PrimaryAttack | ok, SwingAgain after PrimarySebHitRefire |
| 0x113b50 | CCrowbar::SecondaryAttack | ok, disasm |
| 0x113c10 / 0x113c40 | CCrowbar::Smack / SwingAgain | ok |
| 0x113c70 | CCrowbar::Swing | ok, disasm (Ghidra split it into FUN_00123c81) |
| 0x114470 | CCrowbar::IsHornetGun | ok, returns true |
| 0x114480 | CCrowbar::Reload | ok, disasm |
| 0x114e40-0x114e60 | CCrowbar::BotUseWeapon / BotCanHaveItem / BotReloadWeapon | ok |
| 0x114e70 | CCrowbar::iItemSlot | ok (inline) |
| 0x114ed0 / 0x114f00 | CCrowbarRocket::ProMagApplyForce / ProMagEffected | ok |

### shotgun.cpp

| Orig | Function | Status |
|---|---|---|
| 0x20bcf0 | pipebomb | ok |
| 0x20bd20 | CFlyinScientist::PipeCreate | ok |
| 0x20bd70 | CFlyinScientist::Spawn | ok |
| 0x20bef0 | CFlyinScientist::AimThink | ok, disasm (Ghidra lost the speed branch) |
| 0x20c040 | CFlyinScientist::Precache | ok |
| 0x20c070 / 0x20c110 | CSpaceRift::DamageTouch / PreOpenDamageTouch | ok, disasm (9999 / 1 damage, DMG_ALWAYSGIB) |
| 0x20c1b0 | CSpaceRift::FinalBoom | ok, disasm; VERIFY null-owner guard (section 8) |
| 0x20c9b0 | CSpaceRift::DoRings | ok, disasm: its drawing loop is empty in the original |
| 0x20cec0 | CSpaceRift::DoEffects | ok, disasm for the flare phase and VecToAngles arg |
| 0x20e000 | CSpaceRift::SuckThink | ok, disasm |
| 0x20e5f0 | CSpaceRift::StartSuck | ok, disasm (Ghidra dropped the early return) |
| 0x20ed70 | CSpaceRift::RiftCreate | ok |
| 0x20edc0 | CSpaceRift::Spawn | ok |
| 0x20f000 | CSpaceRift::Decay | ok |
| 0x20f1c0 | CSpaceRift::PrepareOpenThink | ok |
| 0x20f270 | CSpaceRift::StartPreopen | ok, disasm |
| 0x20f8f0 | CSpaceRift::Grow | ok, disasm (Ghidra lost the bool return) |
| 0x20fb60 | weapon_shotgun | ok |
| 0x20fb90 / 0x20fbe0 | CShotgun::Save / Restore | ok, table in weapons.cpp unchanged (section 4) |
| 0x20fc30 | CShotgun::Spawn | ok |
| 0x20fcc0 | CShotgun::Precache | ok |
| 0x20ffc0 | CShotgun::AddToPlayer | ok, base call only (WeapPickup is central in halflife-updated) |
| 0x210050 | CShotgun::GetItemInfo | ok |
| 0x210110 | CShotgun::Deploy | ok, same as stock |
| 0x210150 | CShotgun::PrimaryAttack | ok, disasm |
| 0x2107e0 | CShotgun::SecondaryAttack | ok, disasm (Ghidra split it into FUN_002207f1) |
| 0x211660 | CShotgun::Reload | ok |
| 0x2118f0 | CShotgun::WeaponIdle | ok |
| 0x211b40 | ammo_buckshot | ok |
| 0x211b70 | CShotgun::BotWeaponFrame | ok, disasm |
| 0x211d00 / 0x211dc0 / 0x211dd0 | CShotgun::BotFireButton / BotFireSecondary / BotFirePrimary | ok, disasm |
| 0x211f80 | CShotgun::iItemSlot | ok (inline) |
| 0x211fe0 | CSpaceRift::EntIsBeingRemoved | ok |
| 0x212140 / 0x212240 / 0x212270 | CFlyinScientist::ProMagApplyForce / ProMagEffected / GetRCCaps | ok, GetRCCaps 0x6080001c |
| 0x252ac0-0x252bb0 | CShotgunAmmo::AddAmmo / Precache / Spawn | ok, SHOTGUN/MaxAmmo then BoxGive |

### egon.cpp

| Orig | Function | Status |
|---|---|---|
| 0x181b90 | egon_mirv | ok |
| 0x181bc0 / 0x181c10 | CEgonMirv::Save / Restore | ok, 3 fields |
| 0x181c60 | CEgonMirv::Precache | ok |
| 0x181cd0 | CEgonMirv::Spawn | ok, disasm |
| 0x182120 | CEgonMirv::MFTouch | ok, read from disasm (Ghidra had 444 of 4525 bytes) |
| 0x1832d0 | CEgonMirv::MFThink | ok |
| 0x183920 | IsoBomb | ok |
| 0x183950-0x183ad0 | CIsoBomb::IsoCreate / Spawn / Precache | ok |
| 0x183ae0 | CIsoBomb::PunchTrace | ok |
| 0x183dd0 / 0x183f60 | CIsoBomb::IsoRadus / IsoGoBoom | ok, disasm |
| 0x184550 / 0x184740 | CIsoBomb::UpdateBeams / DoRingPulse | ok |
| 0x184940 | CIsoBomb::DoArc | ok, disasm |
| 0x184d00 / 0x184fd0 / 0x185520 | CIsoBomb::IsoPostArmThink / InitBeams / ArmTouch | ok |
| 0x185710 | CIsoBomb::IsoBecomeSolid | VERIFY, null-owner crash guarded (section 8) |
| 0x185800 | weapon_egon | ok |
| 0x185830 / 0x185880 | CEgon::Save / Restore | ok, table in weapons.cpp |
| 0x1858d0 / 0x185950 / 0x1859a0 | CEgon::Spawn / ItemPreFrame / Precache | ok |
| 0x185b10 / 0x185c00 | CEgon::StartTractor / StopTractor | ok |
| 0x185c50 / 0x185f60 | CEgon::UpdateTractor / SecondaryAttack | ok, disasm |
| 0x186370-0x1864c0 | CEgon::Deploy / AddToPlayer / Holster / GetItemInfo | ok |
| 0x186570 / 0x1865a0 | CEgon::GetPulseInterval / GetDischargeInterval | ok |
| 0x1865d0 | CEgon::Attack | ok, disasm |
| 0x1868e0 | CEgon::PrimaryAttack | ok |
| 0x186910 | CEgon::Fire | VERIFY, uninitialised shake centre (section 8) |
| 0x186d30-0x187190 | CEgon::UpdateEffect / CreateEffect / DestroyEffect / WeaponIdle / EndAttack | ok |
| 0x187250 / 0x187260 | CEgon::IsHornetGun / Reload | ok, Reload from disasm |
| 0x187560 | ammo_egonclip | ok |
| 0x187590-0x187610 | CEgon::UseAmmo / HasAmmo / iItemSlot | ok |
| 0x187670-0x1876d0 | CIsoBomb::GetRealOwner / EntIsBeingRemoved / GetBotWeight / BotFireSecondary / BotFirePrimary | ok |
| 0x187730 / 0x187790 | CEgonMirv::ProMagApplyForce / ProMagEffected | ok |
| 0x24bae0-0x24bbd0 | CEgonAmmo::AddAmmo / Precache / Spawn | ok, ITEMS/CellsMax then EGON/AmmoBoxGive |

### What RC changed in the stock bodies

- **Crowbar.** Based on the pre-event SDK: no `events/crowbar.sc`, every sound from the server,
  miss animations (ATTACK1-3MISS) and the miss sound. Every hit does `PrimaryHitDamage` (35); the
  stock half-damage follow-up and the IsMultiplayer branch are gone. Refire times are rc.ini
  values times the player's curse refire scale. Sounds use CHAN_WEAPON for body hits (stock:
  CHAN_ITEM). `m_trHit` is copied on every hit, as in RC's SDK.
- **Shotgun.** Primary throws a CFlyinScientist instead of buckshot (shell eject, biggun2 sound,
  kick back `PrimaryKick` 400 and `PrimaryAngleKick` 5). Secondary uses one shell and fires the
  rift beam. Clip size, max ammo and pickup ammo come from rc.ini. No events, no barrel sounds.
  `m_flNextReload` gates Reload. The pump sound is back in WeaponIdle and CShotgun::ItemPostFrame
  is gone (the original has none).
- **Egon.** No damage beam: a tractor beam on primary, an IsoBomb on secondary, a mirv on reload.
  Details in the fork notes below (section 4, 7).
- **All three:** halflife-updated's `UseDecrement()` override is removed. The server build
  defines CLIENT_WEAPONS, so with the override the player decrements the weapon timers every
  frame, while RC sets them as absolute `gpGlobals->time + x`; the original has no override.
  `m_pPlayer->m_flNextAttack` stays `UTIL_WeaponTimeBase() + x` because halflife-updated's player
  always decrements it (the shotgun reload start was changed to that).

## 4. Classes

**CCrowbar** (CBasePlayerWeapon). `m_iSwing` +0x8c, `m_trHit` +0x90 (stock names). `m_usCrowbar`
removed (no event). New overrides: SecondaryAttack, Reload, IsHornetGun, GetBotWeight and the bot
interface.

**CCrowbarRocket** (CGrenade). Saved (names from the save table): `m_blDisHonest` +0x284,
`m_flDisTime` +0x288, `m_flDisOwnTime` +0x28c, `m_flAngSpread` +0x290, `m_blDrunk` +0x294,
`m_pRealOwn` +0x298, `vecRealDir` +0x29c. Recovered: `m_iTrail` +0x280 (smoke.spr, as CRpgRocket).
New virtual `ProMagEffected`.

**CShotgun** (CBasePlayerWeapon). Saved per the original: `m_fInReload` +0x8c, `m_flNextReload`
+0x90 (twice, as in the SDK), `m_flPumpTime` +0x98. The weapons.cpp table is left as it was: its
`m_fInSpecialReload` (base class in halflife-updated) is the state the original keeps in its own
`m_fInReload`, and `m_flPumpTime` moved to the base; same fields. `m_iShell` +0x94. Recovered:
`m_flBotRiftTime` +0x9c and `m_vecBotRiftSpot` +0xa0 (written for bots, never read in this
library, VERIFY names), `m_flBotNextThrow` +0xac (BotFirePrimary's hold-off).

**CFlyinScientist** (CGrenade, `pipebomb`). Recovered: `m_flScreamTime` +0x280, `m_blScreamed`
+0x284. Overrides GetRCCaps, ProMagApplyForce; new virtual ProMagEffected.

**CSpaceRift** (CGrenade, classname `shotgun_rift`). No save table in the original; all recovered
and named from use: `m_flHits` +0x280, `m_pFlare` +0x284 (the `rift_flare` sprite entity),
`m_pOwner` +0x288 (edict), `m_vecNormal` +0x28c, `m_flSuckEndTime` +0x298, `m_flNextExpel`
+0x29c, `m_flNextBigExpel` +0x2a0, `m_flNextShake` +0x2a4, `m_vecRingAngles` +0x2a8,
`m_flRingRadius` +0x2b4, `m_flNextRings` +0x2b8, `m_flNextSuckPulse` +0x2bc, `m_flSuckRange`
+0x2c0, `m_flSuckVelo` +0x2c4 (read, never used), `m_blSucking` +0x2c8, `m_blPreOpen` +0x2c9,
`m_pPreOpenBeams[8]` +0x2cc, `m_pOtherBeams[8]` +0x2ec (cleaned up, never assigned: VERIFY name),
`m_pSuckBeam` +0x30c. The pre-open timer uses `pev->dmgtime` / `pev->dmg_save`.

**CEgon** (from the fork). Saved, in the original's order: m_flAmmoUseTime, m_shootTime, m_pBeam,
m_pNoise, m_pSprite, m_fireState, m_fireMode, m_shakeTime, m_pTrackEnt (+0xb0), m_flEntDist
(+0xb8), m_flOldGrav (+0xbc), m_iOldMoveTyp (+0xc0), m_flTractTime (+0xc4). `m_pTrackEnt` is an
EHANDLE in code and saved here as FIELD_EHANDLE (original FIELD_CLASSPTR, section 8).
`m_deployed` +0xac is an int. Removed: ShouldWeaponIdle, CanHolster, UseDecrement,
m_usEgonFire/Stop.

**CIsoBomb** (CGrenade). No save table; members named from use (VERIFY): m_pMainBeam +0x280,
m_pBeams[5] +0x284, m_vecBeamDir[5] +0x298, m_pRealOwner +0x2d4, m_flNextRingTime +0x2d8, m_flRed
+0x2dc, m_flBlue +0x2e0, m_flLastThinkTime +0x2e4, m_flNextArcTime +0x2e8. New virtuals
BotFirePrimary, BotFireSecondary, GetBotWeight.

**CEgonMirv** (CGrenade). Saved m_flSpitTime +0x288, m_vecTossTo +0x28c, m_bInToss +0x298;
recovered m_iTrail +0x280, m_iFlare +0x284. New virtual ProMagEffected.

**RC caps used** (`HasRCCap` bits, names ours): 0x40 crowbar reload shove, 0x8 rift pull, 0x10
tractor beam. Bot object offsets read (as in weapons.cpp): +0x698 fire gate, +0x69c ally flag
(byte), +0x6a0 skill (int); its virtuals Center (slot 50) and IRelationship (slot 86).

## 5. Cross-file needs (merge work)

- **weapons.cpp `CBasePlayerWeapon::ItemPostFrame`** (owner of weapons.cpp): the original calls
  `Reload()` when IN_RELOAD is held and `IsHornetGun()` returns 1, before the clip logic.
  Without it `CCrowbar::Reload` (the shove) and `CEgon::Reload` (the mirv) never run. The original
  also only calls WeaponIdle when no attack button is held (or ShouldWeaponIdle), which is what
  ends the egon beam on release.
- **weapons.cpp**: `CEgon::m_SaveData` rewritten here (13 fields). Nothing else touched.
- **weapons.h**: edits only inside CCrowbar, CShotgun, CEgon.
- Used as-is from the tree: PREFS_* (`Preff.h`), CountEntsInWorld (combat.cpp via cbase.h),
  FixedUnsigned16 (util.cpp, declared extern), `CBasePlayer::m_fCurseRefireScale`,
  `m_fShotgunRiftTime`, `m_fIsoBombTime`, `CGameRules::CanPlayerAnnoy`, `CBaseEntity::RemoveTouch`,
  CBeam. No TEMP STUBs added; none of the existing four belongs to these files.
- Cvars read by name through CVAR_GET_FLOAT, as the original does: `mp_allowrifts`, `mp_allowiso`,
  `sv_lowlag`, `sv_ceasefire`.

## 6. VERIFY list (13 in these files)

| Where | What would settle it |
|---|---|
| crowbar.cpp, RC_CAP_CROWBAR_RELOAD_MOVE 0x40 | name only; the binary has the bit (cap tables of other classes may suggest names, e.g. IsCrowbarReloadMoveable on CHornet) |
| crowbar.cpp, RCBotHasAllies (+0x69c), RCBotSkill (+0x6a0) | names only; need the RC bot's CBot layout |
| shotgun.cpp, RC_CAP_SHOTGUN_RIFT_MOVEABLE 0x8 | name only |
| shotgun.cpp, RCBotFireGate (+0x698) | name only (same as weapons.cpp) |
| shotgun.cpp, CSpaceRift::m_pOtherBeams | never assigned in the binary; name is a guess |
| shotgun.cpp, FinalBoom owner guard | engine test: does the original crash when the rift owner disconnects before the boom? (section 8) |
| weapons.h, CShotgun m_flBotRiftTime / m_vecBotRiftSpot | nothing in rc_i386.so reads them; probably for the bot library |
| egon.cpp, RC_CAP_TRACTOR_BEAM 0x10 | name only |
| egon.cpp, CIsoBomb member names | names only |
| egon.cpp, IsoBecomeSolid null owner | engine test (section 8) |
| egon.cpp, Fire shake centre | the original reads an unfilled trace; engine test |

## 7. Behavior worth testing

- **Crowbar:** secondary swings and fires a rocket; about 1 in 5 drunk rockets (1 in 3 sober)
  turn back on the shooter after 0.3-1 s at 1000 speed; drunk rockets wobble; the rocket can hit
  its owner after 0.3 s and the kill is credited to him as "crowbar". Reload (needs the
  ItemPostFrame port) shoves movable things away at 800 or bounces the player back off walls
  (500) and immovable things (300). sv_ceasefire 1 blocks reload.
- **Shotgun:** primary throws a screaming scientist that explodes on touch (100 dmg); kicks the
  player back. Secondary on a wall opens a rift; two more hits start a 3 s pre-open with beams,
  then a 10 s suck that pulls and gibs, shakes nearby players, spits debris, then a blast
  (RiftFinalBlastDamage 150). Rift owner can't open another for 30 s; mp_allowrifts 0 disables.
  A second rift opening while one sucks blows both. A hit on an opening or open rift blows it.
  A rift left alone decays away. sv_lowlag 1 cuts debris and the beam sparks.
- **Egon** (fork): tractor holds a cap-0x10 entity and lets go above TractorMaxVel; release ends
  the beam; IsoBomb arms on walls not on its thrower, 6 s fuse, damages through 52 units of wall,
  max 5; mirv splits ReloadMaxFrags - 1 times; magnet makes frags toss toward it.
- The two guarded/kept owner-disconnect cases (rift FinalBoom, unarmed IsoBomb).

**Client prediction.** A stock HL client with `cl_lw 1` predicts its own crowbar, shotgun and
egon from its event code: it will play stock swing/fire animations, sounds and decals locally
(crowbar half/full damage swings, buckshot and shells from the shotgun events, a damaging egon
beam) that RC's server never sends, and RC's server-side sounds will play on top. Mispredicted
viewmodel animations are likely (the shotgun's single-shell secondary, the crowbar miss
animations, the egon's lack of events).

## 8. Original bugs

1. **CCrowbarRocket::FollowThink** uses int `abs()` on the float wobble offsets, so the forward
   component is 1 for any spread below 1. Kept.
2. **CCrowbarRocket::ProMagApplyForce** stores a direction in `vecRealDir`, which FollowThink uses
   as angles. A magnetised drunk rocket wobbles around a meaningless heading. Kept.
3. **CShotgun::PrimaryAttack** sets `m_flTimeWeaponIdle = 0.75` (absolute) on an empty clip,
   instead of time + 0.75. Kept.
4. **CSpaceRift::FinalBoom** writes `pevOwner->pContainingEntity` with pevOwner NULL when the
   rift's owner is gone (EntIsBeingRemoved clears it on disconnect): a server crash. Changed:
   guarded (owner set to NULL), marked VERIFY, following the DEVIATIONS "Crashes in the original"
   precedent; the coordinator should add it there.
5. **CSpaceRift::DoEffects** shakes players harder the farther they are (amplitude =
   dist / 1512 * 200). Kept.
6. **CSpaceRift::DoRings** computes the ring points and draws nothing; its only effect is
   `UTIL_MakeVectors`. Kept.
7. **CSpaceRift::m_pOtherBeams** and `m_flSuckVelo` are cleaned up / read but never used. Kept.
8. **CSpaceRift::StartPreopen** draws `RANDOM_LONG(6, 8)` again on every loop pass. Kept.
9. **CSpaceRift::Decay** removes the flare without clearing `m_pFlare`, then removes itself. Kept.
10. **CShotgun::SecondaryAttack** calls `CBaseEntity::Instance(tr.pHit)->IsBSPModel()` without a
    NULL check (as stock code does elsewhere). Kept.
11. **CShotgun::BotFirePrimary** checks the toss from the weapon entity's own `Center()`, not the
    bot's. Kept.
12. **Egon (from the fork):** IsoGoBoom's second blast is centred at origin + tr.vecEndPos -
    (0,0,32), far away (kept); DoArc sends MSG_PVS with a NULL origin (kept); IsoBecomeSolid
    reads a NULL m_pRealOwner after the thrower disconnects, a crash (guarded, VERIFY); Fire shakes
    around an unfilled trace (changed: zeroed, VERIFY); ItemPreFrame dereferences m_pPlayer right
    after a NULL test (kept, harmless); m_pTrackEnt EHANDLE saved as FIELD_CLASSPTR (changed to
    FIELD_EHANDLE); MFTouch/MFThink cone with stale v_up/v_right (kept); UpdateTractor and Reload
    discard computed values (kept as `(void)` calls); m_flOldGrav, m_iOldMoveTyp,
    g_iTractorBeamSpr, m_pSprite never used (kept); flying_expel debris is invisible except its
    trail (kept); IsoBecomeSolid rethinks every 0.3 s until armed (kept).

IsoBecomeSolid and FinalBoom both guard the same disconnect case (DEVIATIONS.md, Crashes in the
original).
