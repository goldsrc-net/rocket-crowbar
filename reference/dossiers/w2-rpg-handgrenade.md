# w2-rpg-handgrenade

## 1. Scope

Original files: `rpg.cpp` (96 functions: CRpg, CRpgRocket, CLaserSpot, CRpgAmmo, and RC's CSuckMine,
CConversation, CScientistBomb, CBarneyBomb) and `handgrenade.cpp` (41 functions: CHandGrenade and
RC's CImplosionGrenade, CBubleGrenade, CFictionTimer, DestroyFrictionTimer). Also the header-inline
grenade functions funcfile.tsv files under `(header)`: CBubleGrenade::GetRCCaps and
CFreezeGreande::ProMagApplyForce/ProMagEffected.

Written: `dlls/rpg.cpp`, `dlls/handgrenade.cpp` (both rewritten over halflife-updated's stock
versions). Touched: `dlls/weapons.h` (CRpg, CRpgRocket, CHandGrenade, and a new block after
CGrenade declaring CBubleGrenade, CFreezeGreande and DestroyFrictionTimer). No other shared file.

What RC made of these weapons: the RPG fires no rockets. Primary throws a scientist or barney
"bomb" that lands, waits for something to come near, says hello, rises spinning and explodes;
bombs near each other chat through a CConversation. Secondary throws a suck mine (a sentry model)
that ropes the best target in front of its barrel and reels it in. The hand grenade's primary is a
gravity grenade (rises, hangs sucking everything in, gibs what touches it, explodes after 5 s);
its secondary is a bubble grenade that pushes everything away and makes walkers slide on negative
friction (CFictionTimer). All tunables are rc.ini keys under [RPG] and [GRENADES].

## 2. Status

| file | check_file.py --missing |
|---|---|
| rpg.cpp | 96 of 96 present, 0 missing |
| handgrenade.cpp | 41 of 41 present, 0 missing |
| (header), this package's rows | CBubleGrenade::GetRCCaps ok; CFreezeGreande::ProMagApplyForce, ProMagEffected inl; CRpgAmmo::AddAmmo/Precache/Spawn ok |

All 11 entity factories are exported (`nm -D`): weapon_rpg, laser_spot, rpg_rocket, suck_mine,
scientistbomb, barneymine, ammo_rpgclip, weapon_handgrenade, friction_timer, bubgren, impgren.
Builds on i386 and amd64 (hl target), no errors or warnings from these files. Not run in an
engine: built only, behavior unchecked.

Addresses in `// orig` comments are funcfile.tsv (file) addresses; decompile headers are those
+ 0x10000.

## 3. Function map

Weapon files: every function was compared, stock ones included, against the decompile, and the
disassembly was read where Ghidra lost or garbled the code: CRpg::Reload, BotWeaponFrame,
BotFirePrimary; CSuckMine::LookForTarget, MoveGun, DoSucking (tail); CScientistBomb and
CBarneyBomb::SciMineActiveThink (Ghidra split both into FUN_ fragments) and
CBarneyBomb::SciBonceTouch; CBubleGrenade::ShootBubTimed, SlippyGoBoom; CImplosionGrenade::Suck,
DoRing, PreImplo; CFictionTimer::Spawn (its model string) and CHandGrenade::WeaponIdle (first
lines).

rpg.cpp:

| orig | function | status |
|---|---|---|
| 0x1fc0b0 | suck_mine (factory) | ok |
| 0x1fc0e0 | CSuckMine::Precache | ok |
| 0x1fc180 | CSuckMine::Spawn | ok |
| 0x1fc340 | CSuckMine::RunThink | ok |
| 0x1fc490 | CSuckMine::LookForTarget | ok (disassembly); VERIFY cap bit 0x200000 |
| 0x1fca40 | CSuckMine::MoveGun | ok (disassembly) |
| 0x1fcd00 | CSuckMine::DoSucking | ok |
| 0x1fd410 | CSuckMine::BecomeSolid | ok |
| 0x1fd4e0 | CSuckMine::ActiveTouch | ok |
| 0x1fd590 | CSuckMine::Killed | ok |
| 0x1fd780 | CSuckMine::DeathThink | ok |
| 0x1fdae0 | CConversation::ConvCreate | ok |
| 0x1fdb40 | CConversation::Spawn | ok |
| 0x1fdc30 | CConversation::ChatThink | VERIFY (stale slots, kept) |
| 0x1fdea0 | CConversation::AddMemeber | ok |
| 0x1fdef0 | CConversation::RemoveMemeber | ok |
| 0x1fdf80, 0x1feb10 | scientistbomb, barneymine (factories) | ok |
| 0x1fdfb0, 0x1feb40 | CScientistBomb/CBarneyBomb::SciCreate | ok (classname rpg_rocket) |
| 0x1fe000, 0x1feb90 | ::Spawn | ok |
| 0x1fe1a0, 0x1fed20 | ::Precache | ok |
| 0x1fe1e0, 0x1fed50 | ::SciPreTouchThink | ok |
| 0x1fe2e0, 0x1fee50 | ::SciMineExplode | ok |
| 0x1fe390, 0x1fef00 | ::SciMineActiveThink | VERIFY (null guard added, sentence format) |
| 0x1fe820, 0x1ff380 | ::SciBonceTouch | ok |
| 0x1fe9f0, 0x1ff550 | ::AskQuest | VERIFY sentence format |
| 0x1fea60, 0x1ff5c0 | ::AnswerQuest | VERIFY sentence format |
| 0x1fead0, 0x1ff630 | ::BloodColor | ok |
| 0x1feae0, 0x1ff640 | ::Classify | ok |
| 0x1feaf0, 0x1ff650 | ::Killed | ok |
| 0x2012b0, 0x2013e0 | ::GetRealOwner | ok |
| 0x2012c0, 0x2013f0 | ::EntIsBeingRemoved | ok |
| 0x201360-0x201380, 0x201490-0x2014b0 | ::GetRCCaps, GetGuassDistOut, GetGuassHull | ok |
| 0x1ff670, 0x1ff6a0, 0x1ff860, 0x2011d0 | laser_spot, weapon_rpg, rpg_rocket, ammo_rpgclip | ok |
| 0x1ff6d0-0x1ff830, 0x201250 | CLaserSpot (all 6) | ok, same as stock |
| 0x1ff890, 0x1ff8e0 | CRpgRocket::Save/Restore | ok (table is in weapons.cpp) |
| 0x1ff930 | CRpgRocket::CreateRpgRocket | ok, launcher count removed |
| 0x1ff9c0 | CRpgRocket::Spawn | ok, + m_flSpawnTime |
| 0x1ffb60-0x1ffc10 | RocketTouch, Precache, IgniteThink | ok, same as stock |
| 0x1ffda0 | CRpgRocket::FollowThink | ok, launcher and sky code removed |
| 0x200410 | CRpg::Reload | VERIFY (uninitialized iResult) |
| 0x2004b0 | CRpg::Spawn | ok |
| 0x200530 | CRpg::Precache | ok |
| 0x2005e0 | CRpg::GetItemInfo | ok |
| 0x2006a0 | CRpg::AddToPlayer | VERIFY (WeapPickup left to AddPlayerItem) |
| 0x200730-0x200790 | Deploy, CanHolster, Holster | ok |
| 0x2007f0 | CRpg::PrimaryAttack | ok |
| 0x200d30 | CRpg::SecondaryAttack | ok |
| 0x2010c0 | CRpg::WeaponIdle | ok |
| 0x2011c0 | CRpg::UpdateSpot | ok (empty) |
| 0x201560 | CRpg::GetBotWeight | ok |
| 0x2015c0 | CRpg::BotWeaponFrame | VERIFY (bot Center) |
| 0x201810 | CRpg::BotFireSecondary | ok |
| 0x201850 | CRpg::BotFirePrimary | VERIFY (distance bug kept) |
| 0x201930, 0x201940 | ShouldWeaponIdle, iItemSlot | ok |
| 0x2019a0-0x201c30 | CSuckMine GetRealOwner, GetPullOrg, EntIsBeingRemoved, GetRCCaps, GetGuassDistOut, GetGuassHull, Classify, BloodColor | ok |
| 0x251fd0-0x2520c0 | CRpgAmmo::AddAmmo, Precache, Spawn ("(header)") | ok |

handgrenade.cpp:

| orig | function | status |
|---|---|---|
| 0x1a3530 | friction_timer (factory) | ok |
| 0x1a3560, 0x1a35b0 | CFictionTimer::Save/Restore | ok (1 field) |
| 0x1a3600 | CFictionTimer::CreateOrUpdateTimer | ok |
| 0x1a3860 | DestroyFrictionTimer | ok |
| 0x1a3950 | CFictionTimer::Spawn | ok (model "") |
| 0x1a3a60 | CFictionTimer::DoUse | VERIFY (owner not checked, as original) |
| 0x1a3c20 | CFictionTimer::DoThink | ok |
| 0x1a81e0 | CFictionTimer::PlayerWasKilled | ok |
| 0x1a3f10 | bubgren (factory) | ok |
| 0x1a3f40 | CBubleGrenade::ShootBubTimed | ok |
| 0x1a40f0 | CBubleGrenade::SlippyGoBoom | ok (disassembly); VERIFY cap bit 0x800000 |
| 0x1a4a40 | CBubleGrenade::PreDetThink | ok |
| 0x1a4b90 | CBubleGrenade::SlippyTouch | ok |
| 0x24ddc0 | CBubleGrenade::GetRCCaps ("(header)") | ok |
| 0x1a4e70 | impgren (factory) | ok |
| 0x1a4ea0, 0x1a4ef0 | CImplosionGrenade::Save/Restore | ok (11 fields) |
| 0x1a4f40 | CImplosionGrenade::DamageTouch | ok |
| 0x1a4fb0 | CImplosionGrenade::FinalBoom | ok |
| 0x1a5580 | CImplosionGrenade::DoRing | ok (draws nothing, as original) |
| 0x1a5ad0 | CImplosionGrenade::DoEffects | VERIFY (shake amplitude) |
| 0x1a63a0 | CImplosionGrenade::Suck | ok (disassembly); VERIFY cap bit 4 |
| 0x1a6990 | CImplosionGrenade::InitBeams | ok |
| 0x1a6bd0 | CImplosionGrenade::PreImplo | ok (disassembly) |
| 0x1a7090 | CImplosionGrenade::PreDetThink | ok |
| 0x1a7180 | CImplosionGrenade::StickyTouch | ok (unused in the original) |
| 0x1a7350 | CImplosionGrenade::ShootImpTimed | ok |
| 0x1a8040, 0x1a8140, 0x1a8180 | ProMagApplyForce, ProMagEffected, GetRCCaps | ok |
| 0x1a7520 | weapon_handgrenade (factory) | ok |
| 0x1a7550-0x1a7810 | CHandGrenade Spawn, Precache, GetItemInfo, Deploy, CanHolster, Holster | ok |
| 0x1a78e0, 0x1a7990 | CHandGrenade::PrimaryAttack, SecondaryAttack | ok |
| 0x1a7a40 | CHandGrenade::WeaponIdle | ok |
| 0x1a7fe0 | CHandGrenade::iItemSlot | ok |
| 0x24e620, 0x24e720 | CFreezeGreande::ProMagApplyForce, ProMagEffected ("(header)") | ok |

Stock bodies changed to match the original (each halflife-updated difference is listed here):

- CRpg: no laser spot anywhere (UpdateSpot is empty; Spawn, Holster, SecondaryAttack, WeaponIdle,
  CanHolster no longer touch it); no PLAYBACK_EVENT and no events/rpg.sc precache (the original
  plays the fire sounds with EMIT_SOUND); rc.ini clip size, max ammo, pickup ammo; iFlags 0
  (halflife-updated: ITEM_FLAG_NOAUTOSWITCHTO), weight 10 (20); idle anims with RANDOM_FLOAT and
  3/6 s (halflife-updated: shared random, 6/6.1 s); Reload without the ammo and spot checks.
  Fire timers use the original's plain delays, not GetNextAttackDelay's creep correction.
- CRpgRocket: no destructor, no GetLauncher, no launcher count in CreateRpgRocket, no launcher or
  sky detonation code in FollowThink (all halflife-updated additions).
- CRpgAmmo: rc.ini BoxGive and MaxAmmo (stock: 1 or 2, ROCKET_MAX_CARRY).
- CHandGrenade: rc.ini ammo; weight 8 (5); Holster no longer clears m_flStartThrow (a
  halflife-updated fix); WeaponIdle's release check is `m_flReleaseThrow == 0` alone (as the old
  SDK), the throw is 5 s after the pin (3), ThrowScale/ThrowMax (6.5/1000), RefireRate times the
  curse refire scale, RANDOM_FLOAT instead of the shared random, and it throws an
  implosion or bubble grenade instead of CGrenade::ShootTimed.

Timers (coordinator decision, wave 2): CRpg and CHandGrenade have no UseDecrement override (the
original has none), so their weapon timers are absolute gpGlobals->time as in the original. The one
exception is the player's m_flNextAttack (Holster), which halflife-updated's player.cpp always
counts down under CLIENT_WEAPONS; it stays UTIL_WeaponTimeBase() + 0.5. Note for whoever owns
weapons_shared.cpp: DefaultDeploy and DefaultReload still set m_flTimeWeaponIdle and
m_flNextAttack from UTIL_WeaponTimeBase() (0 under CLIENT_WEAPONS), so after a deploy or reload
these weapons idle at once instead of after 3 s / the reload delay.

## 4. Classes

Sizes are the original's (GetClassPtr allocation). Base classes from hierarchy.tsv. Members marked
VERIFY name were named from use; the original offsets are in comments.

- CRpg : CBasePlayerWeapon (0x94). +0x8c m_cActiveRockets (reused stock name: bombs and mines fired,
  decremented when a bomb triggers; mines are never uncounted), +0x90 m_flBotNextFire (VERIFY
  name). No save table in the original. m_fSpotActive stays (not in the original, always false)
  because client.cpp and cl_dll read it; m_pSpot and m_usRpg are gone.
- CRpgRocket : CGrenade (0x290). +0x280 m_iTrail, +0x284 m_flIgniteTime, +0x288 the launcher (saved
  m_pLauncher FIELD_CLASSPTR; ours keeps halflife-updated's EHANDLE m_hLauncher), +0x28c
  m_flSpawnTime (set in Spawn, never read; VERIFY name).
- CLaserSpot : CBaseEntity. Stock.
- CSuckMine : CGrenade (0x2ac). +0x280 m_pRealOwner, +0x284 m_hEnemy (EHANDLE), +0x28c m_pBeam,
  +0x290 m_vecGunAng, +0x29c m_flInterval, +0x2a0 m_flMaxRange, +0x2a4 m_flMinRange,
  +0x2a8 m_flNextLook. All from code, VERIFY names.
- CConversation : CBaseMonster (0x388). +0x27c m_flNextChat, +0x280 m_iAsk, +0x284
  m_pMembers[64] (edicts), +0x384 m_iMembers. From code. No factory; found by classname
  "conversation".
- CScientistBomb, CBarneyBomb : CGrenade (0x28c). +0x280 m_pConv (CConversation), and the shooter's
  edict m_pRealOwner and the CRpg m_pLauncher at +0x288/+0x284 (scientist) or +0x284/+0x288
  (barney). From code.
- CHandGrenade : CBasePlayerWeapon (0x98). +0x8c m_flStartThrow and +0x90 m_flReleaseThrow are
  CBaseEntity members in halflife-updated, used as they are; +0x94 m_bBubble (byte, VERIFY name).
- CImplosionGrenade : CGrenade (0x2d0). Save table (11 fields, savefields.tsv order and types):
  m_iDetStage, m_flDetTime, m_flSpitTime, m_flRingRad, m_flShakeTime, m_flBeamTime, m_flSuckRad,
  m_flSuckVel, vecRingAng, m_pSpot, m_pRingBeams[8]. m_flSuckVel, m_pSpot and m_pRingBeams are
  never used by code. New virtual ProMagEffected (slot 151).
- CBubleGrenade : CGrenade (0x280, no members). Declared in weapons.h (its GetRCCaps is a header
  inline).
- CFictionTimer : CBaseEntity (0x38). Save table: m_fSetFrict (+0x34). Keeps the owner's
  renderamt/renderfx/rendermode/friction in its own pev; pev->dmg is the end time.
- CFreezeGreande : CGrenade (0x28c). Declared in weapons.h with the two header inlines; +0x280
  Vector m_vecFreezeAVel (VERIFY name, read in FreezeBounceTouch). New virtual ProMagEffected
  (slot 151). Its other methods belong to hornetgun.cpp.

## 5. Cross-file needs

- weapons.h (this package's own classes only): CRpg (members as above, AddToPlayer and the four
  bot overrides), CRpgRocket (destructor and GetLauncher removed, m_flSpawnTime added),
  CHandGrenade (SecondaryAttack, m_bBubble), and a new block after CGrenade: CBubleGrenade,
  CFreezeGreande, `extern void DestroyFrictionTimer(entvars_t* pev)`.
- **CFreezeGreande is declared here** (the brief gave this package its header inlines). Its
  methods FreezeDet, PreDetThink, FreezeBounceTouch and ShootFreezeTimed are declared but defined
  in hornetgun.cpp by w2-crossbow-hornet; that package must not declare the class again.
  m_vecFreezeAVel is a guessed name for its +0x280 Vector; rename at merge if hornetgun calls it
  something else.
- **player.cpp (w2-player-core)**: CBasePlayer::Spawn's original (decompile CBasePlayer.c, next to
  DestroyFreezeTimer) calls DestroyFrictionTimer(pev); it is declared in weapons.h.
- **weapons.cpp (shared)**: holds CRpg's save table (m_fSpotActive, m_cActiveRockets); the original
  CRpg has none. Kept compiling by keeping CRpg::Save/Restore and m_fSpotActive; whoever owns
  weapons.cpp can drop the table (and these declarations with it). CRpgRocket's table there saves
  m_hLauncher as FIELD_EHANDLE (original: m_pLauncher FIELD_CLASSPTR); harmless.
- **client.cpp / cl_dll/hl/hl_weapons.cpp** read CRpg::m_fSpotActive and m_cActiveRockets for weapon
  prediction; kept so they compile.
- **WeapPickup**: the original CRpg::AddToPlayer sends gmsgWeapPickup itself (so does the
  original CHgun, CCrossbow, CEgon, CGauss, CMP5, CPython, CShotgun); its CBasePlayer::AddPlayerItem
  does not. halflife-updated's AddPlayerItem sends it for every non-exhaustible weapon except
  while spawning, so ours does not send it again. If w2-player-core makes AddPlayerItem match the
  original, CRpg::AddToPlayer must send it again (and the other weapons likewise).
- Used from elsewhere, all present: PREFS_KeyLoopUp* (Preff.h), CountEntsClaimedByEnt,
  CountEntsInWorld, GetPullCenter, CBaseEntity::RemoveTouch and the RC virtuals (cbase.h),
  m_fCurseRefireScale (player.h), FixedUnsigned16 (util.cpp, declared extern as curses.cpp does),
  gmsgShake.
- No TEMP STUBs made or removed.

## 6. VERIFY list

41 marks (32 rpg.cpp, 4 handgrenade.cpp, 5 weapons.h). Most are member names chosen from use
(settle by: nothing; there are no names in the binary). The others:

1. RC cap bits passed to HasRCCap: 0x200000 (suck mine targets), 0x20000 (sets off bombs), 0x800000
   (bubble grenade victims), 4 (gravity grenade victims). Values are exact; their names are not
   known. A shared enum would help once the cap bits are named.
2. Bomb sentences (`RCBombSentence`): the original's format strings are "%!SC_HELLO%d" etc.
   Windows' CRT prints "!SC_HELLO3" (a sentence); glibc keeps "%!" (checked: glibc prints
   "%!SC_HELLO3"), so the original Linux server played no hello, question or answer lines. Ours
   plays them, as the Windows build did (same reasoning as DEVIATIONS.md's CRLF rc.ini decision).
   Coordinator: confirm and add to DEVIATIONS.md.
3. Guards (coordinator decision): CScientistBomb/CBarneyBomb::SciMineActiveThink decrement
   m_pLauncher->m_cActiveRockets with no null check; EntIsBeingRemoved nulls m_pLauncher when the
   RPG is removed (the RPG of a player who died goes into a weaponbox and is removed with it), so a
   bomb that triggers afterwards crashed the original server. CRpg::BotWeaponFrame calls
   pEnemy->GetBotShootOrg() before anything else, so a NULL enemy crashed it. Both guarded,
   marked `VERIFY: guard, the original crashes here`. For DEVIATIONS.md "Crashes in the original".
4. CRpg::Reload: `iResult` uninitialized in the original when the clip is not empty. Ours is false.
   Only matters when rc.ini sets ClipSize above 1.
5. CRpg::AddToPlayer: WeapPickup (section 5).
6. CRpg::BotWeaponFrame: the original calls the bot's own Center() through its vtable; ours takes
   the default Center() from the bot's entvars (the bot object is the original i386 bot's, as in
   weapons.cpp). RCBotInt6a0 reads an int at +0x6a0 of unknown meaning.
7. CRpg::BotFirePrimary: kept the original's distance (enemy Center to enemy GetBotShootOrg).
8. CConversation::ChatThink: dead or invalid members keep their slot uncounted and can be picked
   to speak (GetClassPtr on that edict). Kept.
9. CFictionTimer::DoUse(USE_OFF) writes to pev->owner without a check; every caller has an owner.
10. CImplosionGrenade::DoEffects: the screen shake gets stronger with distance (frac*200). Kept.

## 7. Behavior worth testing

- RPG primary: scientist or barney (SciMineProbability 0.5) flies out with +100 up for a client,
  lands, arms; walking within 192 units in sight makes it say hello (check the sentence plays),
  rise spinning and explode 0.75 s later for 250. Ten rpg_rocket-class entities per player at most.
- Two bombs landing within 512 units chat: a question, then an answer 6-10 s and 4-7 s apart.
- RPG secondary: suck mine (5 per player), deploys on touch, ropes a target in front of its barrel
  (not its owner by default, SuckMineNoSuckOwner), pulls it to 64 units, dies after 120 s or 75
  damage with smoke and sparks, then fades.
- Kill the RPG's owner while his bombs are armed, wait for the weaponbox to be picked up or removed,
  then set a bomb off: the original crashed here (VERIFY 3).
- Hand grenade primary: rises after 5 s, hangs 5 s pulling everything with cap 4 within 512,
  flying_expel rockets, screen shake, then a 120 blast. Touching it gibs. With sv_lowlag 1 no
  flying_expel.
- Hand grenade secondary: pink push, players slide (friction -0.1) and glow up to 9 s, cannot stay
  on ladders; the slide ends on death and on respawn (DestroyFrictionTimer from player.cpp).
- Six grenades in the world block further throws.
- Prediction (stock HL client, cl_lw 1): UseDecrement is false, as in the original, so the server
  sends every weapon animation. Check that a stock client with cl_lw 1 does not also predict
  Valve's RPG (laser, rocket fire) and 3 s grenade fuse on top.
- Idle right after deploy or reload (see section 3, Timers).

## 8. Original bugs

- **Bomb sentences never play on Linux** (rpg.cpp, Ask/AnswerQuest, SciMineActiveThink): "%!" in the
  sprintf formats. Changed to the Windows behavior (VERIFY 2).
- **Null m_pLauncher crash** (rpg.cpp, both SciMineActiveThink): crashes when a bomb triggers after
  its RPG was removed. Guarded (VERIFY 3).
- **Null enemy crash** (rpg.cpp, CRpg::BotWeaponFrame): the first statement calls through pEnemy.
  Guarded: returns when pEnemy is NULL (VERIFY 3).
- **m_cActiveRockets only grows for suck mines** and for bombs destroyed before they trigger; after
  10 the RPG's bot code stops firing until the weapon is respawned. Kept.
- **RPG bot range test measures the enemy against itself** (BotFirePrimary): never true with an
  enemy, so the bot only fires the RPG with no enemy. Kept.
- **Bomb facing has no effect** (SciMineActiveThink): computes a yaw toward the target, then zeroes
  angles.x and .y. Kept.
- **Uninitialized iResult** in CRpg::Reload (from Valve's SDK). Initialized to false.
- **Gravity grenade stages 3 and 4 are unreachable** (Suck): the looping egon_run3 sound never
  plays. Kept.
- **DoRing draws nothing**: computes the ring's points, loops over them doing nothing;
  m_pRingBeams, m_pSpot and m_flSuckVel are saved but unused. Kept.
- **Screen shake grows with distance** from the gravity grenade (DoEffects). Kept.
- **CFictionTimer is spawned twice** (CreateOrUpdateTimer calls Spawn after CBaseEntity::Create).
  Harmless, kept.
- **A frozen sliding player keeps the pink glow** (CFictionTimer::DoThink restores only friction).
  Kept.
- **The bubble grenade's sounds are not precached** by the hand grenade (debris/beamstart10.wav,
  debris/beamstart4.wav); they play only if something else precached them. Kept.
- **CConversation stale slots** (ChatThink): a dead member can be picked to talk. Kept.
- **CImplosionGrenade::StickyTouch is never used.** Kept.
