/***
*
*	Copyright (c) 1996-2001, Valve LLC. All rights reserved.
*
*	This product contains software technology licensed from Id
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/
#include "extdll.h"
#include "util.h"
#include "cbase.h"
#include "player.h"
#include "weapons.h"
#include "gamerules.h"

// Precaches the ammo and queues the ammo info for sending to clients
void AddAmmoNameToAmmoRegistry(const char* szAmmoname, const char* weaponName)
{
	// make sure it's not already in the registry
	for (int i = 0; i < MAX_AMMO_SLOTS; i++)
	{
		if (!CBasePlayerItem::AmmoInfoArray[i].pszName)
			continue;

		if (stricmp(CBasePlayerItem::AmmoInfoArray[i].pszName, szAmmoname) == 0)
			return; // ammo already in registry, just quite
	}


	giAmmoIndex++;
	ASSERT(giAmmoIndex < MAX_AMMO_SLOTS);
	if (giAmmoIndex >= MAX_AMMO_SLOTS)
		giAmmoIndex = 0;

	auto& ammoType = CBasePlayerItem::AmmoInfoArray[giAmmoIndex];

	ammoType.pszName = szAmmoname;
	ammoType.iId = giAmmoIndex; // yes, this info is redundant
	ammoType.WeaponName = weaponName;
}

bool CBasePlayerWeapon::CanDeploy()
{
	bool bHasAmmo = false;

	if (!pszAmmo1())
	{
		// this weapon doesn't use ammo, can always deploy.
		return true;
	}

	if (pszAmmo1())
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] != 0);
	}
	if (pszAmmo2())
	{
		bHasAmmo |= (m_pPlayer->m_rgAmmo[m_iSecondaryAmmoType] != 0);
	}
	if (m_iClip > 0)
	{
		bHasAmmo |= true;
	}
	if (!bHasAmmo)
	{
		return false;
	}

	return true;
}

bool CBasePlayerWeapon::DefaultReload(int iClipSize, int iAnim, float fDelay, int body)
{
	if (m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] <= 0)
		return false;

	int j = V_min(iClipSize - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

	if (j == 0)
		return false;

	m_pPlayer->m_flNextAttack = gpGlobals->time + fDelay;

	//!!UNDONE -- reload sound goes here !!!
	SendWeaponAnim(iAnim, body);

	m_fInReload = true;

	m_flTimeWeaponIdle = gpGlobals->time + 3;
	return true;
}

void CBasePlayerWeapon::ResetEmptySound()
{
	m_iPlayEmptySound = true;
}

bool CanAttack(float attack_time, float curtime, bool isPredicted)
{
#if defined(CLIENT_WEAPONS)
	if (!isPredicted)
#else
	if (1)
#endif
	{
		return (attack_time <= curtime) ? true : false;
	}
	else
	{
		return ((static_cast<int>(std::floor(attack_time * 1000.0)) * 1000.0) <= 0.0) ? true : false;
	}
}

// orig 0x23a250 (weapons.cpp), the original's whole: absolute times (gpGlobals->time), the hornet
// gun's reload key, and sv_ceasefire. halflife-updated's TabulateAmmo calls and predicted-time
// comparisons are not in it; its m_flLastFireTime reset is (GetNextAttackDelay's bookkeeping).
void CBasePlayerWeapon::ItemPostFrame()
{
	if ((m_fInReload) && (m_pPlayer->m_flNextAttack <= gpGlobals->time))
	{
		// complete the reload.
		int j = V_min(iMaxClip() - m_iClip, m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType]);

		// Add them to the clip
		m_iClip += j;
		m_pPlayer->m_rgAmmo[m_iPrimaryAmmoType] -= j;

		m_fInReload = false;
	}

	// halflife-updated's fix, kept (Steven 2026-11-07): GetNextAttackDelay's bookkeeping, a released
	// trigger starts the next shot's compensation afresh
	if ((m_pPlayer->pev->button & IN_ATTACK) == 0)
	{
		m_flLastFireTime = 0.0f;
	}

	// Rocket Crowbar 1.9: the hornet gun (IsHornetGun: the crowbar, egon, gauss, hornet gun)
	// reloads on its own whenever reload is held
	if ((m_pPlayer->pev->button & IN_RELOAD) != 0 && IsHornetGun())
	{
		Reload();
	}

	// Rocket Crowbar 1.9: sv_ceasefire stops all weapon fire
	const bool bCanFire = CVAR_GET_FLOAT("sv_ceasefire") == 0;

	if ((m_pPlayer->pev->button & IN_ATTACK2) != 0 && m_flNextSecondaryAttack <= gpGlobals->time && bCanFire)
	{
		if (pszAmmo2() && 0 == m_pPlayer->m_rgAmmo[SecondaryAmmoIndex()])
		{
			m_fFireOnEmpty = true;
		}

		SecondaryAttack();
		m_pPlayer->pev->button &= ~IN_ATTACK2;
	}
	else if ((m_pPlayer->pev->button & IN_ATTACK) != 0 && m_flNextPrimaryAttack <= gpGlobals->time && bCanFire)
	{
		if ((m_iClip == 0 && pszAmmo1()) || (iMaxClip() == -1 && 0 == m_pPlayer->m_rgAmmo[PrimaryAmmoIndex()]))
		{
			m_fFireOnEmpty = true;
		}

		PrimaryAttack();
	}
	else if ((m_pPlayer->pev->button & IN_RELOAD) != 0 && iMaxClip() != WEAPON_NOCLIP && !m_fInReload)
	{
		// reload when reload is pressed, or if no buttons are down and weapon is empty.
		Reload();
	}
	else if ((m_pPlayer->pev->button & (IN_ATTACK | IN_ATTACK2)) == 0)
	{
		// no fire buttons down

		m_fFireOnEmpty = false;

		if (!IsUseable() && m_flNextPrimaryAttack < gpGlobals->time)
		{
#ifndef CLIENT_DLL
			// weapon isn't useable, switch.
			if ((iFlags() & ITEM_FLAG_NOAUTOSWITCHEMPTY) == 0 && g_pGameRules->GetNextBestWeapon(m_pPlayer, this))
			{
				m_flNextPrimaryAttack = gpGlobals->time + 0.3;
				return;
			}
#endif
		}
		else
		{
			// weapon is useable. Reload if empty and weapon has waited as long as it has to after firing
			if (m_iClip == 0 && (iFlags() & ITEM_FLAG_NOAUTORELOAD) == 0 && m_flNextPrimaryAttack < gpGlobals->time)
			{
				Reload();
				return;
			}
		}

		WeaponIdle();
		return;
	}

	// catch all
	if (ShouldWeaponIdle())
	{
		WeaponIdle();
	}
}

void CBasePlayer::SelectLastItem()
{
	if (!m_pLastItem)
	{
		return;
	}

	if (m_pActiveItem && !m_pActiveItem->CanHolster())
	{
		return;
	}

	ResetAutoaim();

	// FIX, this needs to queue them up and delay
	if (m_pActiveItem)
		m_pActiveItem->Holster();

	CBasePlayerItem* pTemp = m_pActiveItem;
	m_pActiveItem = m_pLastItem;
	m_pLastItem = pTemp;

	auto weapon = m_pActiveItem->GetWeaponPtr();

	if (weapon)
	{
		weapon->m_ForceSendAnimations = true;
	}

	m_pActiveItem->Deploy();

	if (weapon)
	{
		weapon->m_ForceSendAnimations = false;
	}

	m_pActiveItem->UpdateItemInfo();
}
