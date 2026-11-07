/***
*
*	Rocket Crowbar 1.9: hornetgun.cpp's freeze timer, for the files that thaw players.
*
****/

#pragma once

// Thaws pevOwner: every freeze_timer on it is told to finish (CBasePlayer::Killed calls this).
void DestroyFreezeTimer(entvars_t* pevOwner); // orig 0x1b0c60
