/*
** a_weapons.cpp
** Implements weapon handling
**
**---------------------------------------------------------------------------
** Copyright 2000-2016 Randy Heit
** Copyright 2006-2016 Cheistoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <string.h>

#include "a_pickups.h"
#include "gi.h"
#include "d_player.h"
#include "s_sound.h"
#include "i_system.h"
#include "r_state.h"
#include "p_pspr.h"
#include "c_dispatch.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "cmdlib.h"
#include "templates.h"
#include "sbar.h"
#include "doomstat.h"
#include "g_level.h"
#include "d_net.h"
#include "serializer.h"
#include "thingdef.h"
#include "virtual.h"
#include "a_ammo.h"

#define BONUSADD 6

extern FFlagDef WeaponFlagDefs[];

IMPLEMENT_CLASS(AWeapon, false, true)

IMPLEMENT_POINTERS_START(AWeapon)
	IMPLEMENT_POINTER(Ammo1)
	IMPLEMENT_POINTER(Ammo2)
	IMPLEMENT_POINTER(SisterWeapon)
	IMPLEMENT_POINTER(AmmoType1)
	IMPLEMENT_POINTER(AmmoType2)
	IMPLEMENT_POINTER(SisterWeaponType)
IMPLEMENT_POINTERS_END

DEFINE_FIELD(AWeapon, WeaponFlags)
DEFINE_FIELD(AWeapon, AmmoType1)
DEFINE_FIELD(AWeapon, AmmoType2)	
DEFINE_FIELD(AWeapon, AmmoGive1)
DEFINE_FIELD(AWeapon, AmmoGive2)	
DEFINE_FIELD(AWeapon, MinAmmo1)
DEFINE_FIELD(AWeapon, MinAmmo2)		
DEFINE_FIELD(AWeapon, AmmoUse1)
DEFINE_FIELD(AWeapon, AmmoUse2)		
DEFINE_FIELD(AWeapon, Kickback)
DEFINE_FIELD(AWeapon, YAdjust)				
DEFINE_FIELD(AWeapon, UpSound)
DEFINE_FIELD(AWeapon, ReadySound)	
DEFINE_FIELD(AWeapon, SisterWeaponType)		
DEFINE_FIELD(AWeapon, ProjectileType)	
DEFINE_FIELD(AWeapon, AltProjectileType)	
DEFINE_FIELD(AWeapon, SelectionOrder)				
DEFINE_FIELD(AWeapon, MinSelAmmo1)
DEFINE_FIELD(AWeapon, MinSelAmmo2)	
DEFINE_FIELD(AWeapon, MoveCombatDist)			
DEFINE_FIELD(AWeapon, ReloadCounter)				
DEFINE_FIELD(AWeapon, BobStyle)					
DEFINE_FIELD(AWeapon, BobSpeed)					
DEFINE_FIELD(AWeapon, BobRangeX)
DEFINE_FIELD(AWeapon, BobRangeY)		
DEFINE_FIELD(AWeapon, Ammo1)
DEFINE_FIELD(AWeapon, Ammo2)
DEFINE_FIELD(AWeapon, SisterWeapon)
DEFINE_FIELD(AWeapon, FOVScale)
DEFINE_FIELD(AWeapon, Crosshair)					
DEFINE_FIELD(AWeapon, GivenAsMorphWeapon)
DEFINE_FIELD(AWeapon, bAltFire)
DEFINE_FIELD_BIT(AWeapon, WeaponFlags, bDehAmmo, WIF_DEHAMMO)

//===========================================================================
//
//
//
//===========================================================================

FString WeaponSection;
TArray<FString> KeyConfWeapons;
FWeaponSlots *PlayingKeyConf;

TArray<PClassWeapon *> Weapons_ntoh;
TMap<PClassWeapon *, int> Weapons_hton;

static int ntoh_cmp(const void *a, const void *b);

IMPLEMENT_CLASS(PClassWeapon, false, false)

//===========================================================================
//
//
//
//===========================================================================

PClassWeapon::PClassWeapon()
{
	SlotNumber = -1;
	SlotPriority = INT_MAX;
}

//===========================================================================
//
//
//
//===========================================================================

void PClassWeapon::DeriveData(PClass *newclass)
{
	assert(newclass->IsKindOf(RUNTIME_CLASS(PClassWeapon)));
	Super::DeriveData(newclass);
	PClassWeapon *newc = static_cast<PClassWeapon *>(newclass);

	newc->SlotNumber = SlotNumber;
	newc->SlotPriority = SlotPriority;
}


//===========================================================================
//
//
//
//===========================================================================

void PClassWeapon::Finalize(FStateDefinitions &statedef)
{
	Super::Finalize(statedef);
	FState *ready = FindState(NAME_Ready);
	FState *select = FindState(NAME_Select);
	FState *deselect = FindState(NAME_Deselect);
	FState *fire = FindState(NAME_Fire);

	// Consider any weapon without any valid state abstract and don't output a warning
	// This is for creating base classes for weapon groups that only set up some properties.
	if (ready || select || deselect || fire)
	{
		if (!ready)
		{
			I_Error("Weapon %s doesn't define a ready state.", TypeName.GetChars());
		}
		if (!select)
		{
			I_Error("Weapon %s doesn't define a select state.", TypeName.GetChars());
		}
		if (!deselect)
		{
			I_Error("Weapon %s doesn't define a deselect state.", TypeName.GetChars());
		}
		if (!fire)
		{
			I_Error("Weapon %s doesn't define a fire state.", TypeName.GetChars());
		}
	}
}


//===========================================================================
//
// AWeapon :: Serialize
//
//===========================================================================

void AWeapon::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = (AWeapon*)GetDefault();
	arc("weaponflags", WeaponFlags, def->WeaponFlags)
		("ammogive1", AmmoGive1, def->AmmoGive1)
		("ammogive2", AmmoGive2, def->AmmoGive2)
		("minammo1", MinAmmo1, def->MinAmmo1)
		("minammo2", MinAmmo2, def->MinAmmo2)
		("ammouse1", AmmoUse1, def->AmmoUse1)
		("ammouse2", AmmoUse2, def->AmmoUse2)
		("kickback", Kickback, Kickback)
		("yadjust", YAdjust, def->YAdjust)
		("upsound", UpSound, def->UpSound)
		("readysound", ReadySound, def->ReadySound)
		("selectionorder", SelectionOrder, def->SelectionOrder)
		("ammo1", Ammo1)
		("ammo2", Ammo2)
		("sisterweapon", SisterWeapon)
		("givenasmorphweapon", GivenAsMorphWeapon, def->GivenAsMorphWeapon)
		("altfire", bAltFire, def->bAltFire)
		("reloadcounter", ReloadCounter, def->ReloadCounter)
		("bobstyle", BobStyle, def->BobStyle)
		("bobspeed", BobSpeed, def->BobSpeed)
		("bobrangex", BobRangeX, def->BobRangeX)
		("bobrangey", BobRangeY, def->BobRangeY)
		("fovscale", FOVScale, def->FOVScale)
		("crosshair", Crosshair, def->Crosshair)
		("minselammo1", MinSelAmmo1, def->MinSelAmmo1)
		("minselammo2", MinSelAmmo2, def->MinSelAmmo2);
		/* these can never change
		("ammotype1", AmmoType1, def->AmmoType1)
		("ammotype2", AmmoType2, def->AmmoType2)
		("sisterweapontype", SisterWeaponType, def->SisterWeaponType)
		("projectiletype", ProjectileType, def->ProjectileType)
		("altprojectiletype", AltProjectileType, def->AltProjectileType)
		("movecombatdist", MoveCombatDist, def->MoveCombatDist)
		*/

}

//===========================================================================
//
// AWeapon :: MarkPrecacheSounds
//
//===========================================================================

void AWeapon::MarkPrecacheSounds() const
{
	Super::MarkPrecacheSounds();
	UpSound.MarkUsed();
	ReadySound.MarkUsed();
}

//===========================================================================
//
// AWeapon :: TryPickup
//
// If you can't see the weapon when it's active, then you can't pick it up.
//
//===========================================================================

bool AWeapon::TryPickupRestricted (AActor *&toucher)
{
	// Wrong class, but try to pick up for ammo
	if (CallShouldStay())
	{ // Can't pick up weapons for other classes in coop netplay
		return false;
	}

	bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
	gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
	if (gaveSome)
	{
		GoAwayAndDie ();
	}
	return gaveSome;
}


//===========================================================================
//
// AWeapon :: TryPickup
//
//===========================================================================

bool AWeapon::TryPickup (AActor *&toucher)
{
	FState * ReadyState = FindState(NAME_Ready);
	if (ReadyState != NULL &&
		ReadyState->GetFrame() < sprites[ReadyState->sprite].numframes)
	{
		return Super::TryPickup (toucher);
	}
	return false;
}

//===========================================================================
//
// AWeapon :: Use
//
// Make the player switch to this weapon.
//
//===========================================================================

bool AWeapon::Use (bool pickup)
{
	AWeapon *useweap = this;

	// Powered up weapons cannot be used directly.
	if (WeaponFlags & WIF_POWERED_UP) return false;

	// If the player is powered-up, use the alternate version of the
	// weapon, if one exists.
	if (SisterWeapon != NULL &&
		SisterWeapon->WeaponFlags & WIF_POWERED_UP &&
		Owner->FindInventory (RUNTIME_CLASS(APowerWeaponLevel2), true))
	{
		useweap = SisterWeapon;
	}
	if (Owner->player != NULL && Owner->player->ReadyWeapon != useweap)
	{
		Owner->player->PendingWeapon = useweap;
	}
	// Return false so that the weapon is not removed from the inventory.
	return false;
}

//===========================================================================
//
// AWeapon :: Destroy
//
//===========================================================================

void AWeapon::Destroy()
{
	AWeapon *sister = SisterWeapon;

	if (sister != NULL)
	{
		// avoid recursion
		sister->SisterWeapon = NULL;
		if (sister != this)
		{ // In case we are our own sister, don't crash.
			sister->Destroy();
		}
	}
	Super::Destroy();
}

//===========================================================================
//
// AWeapon :: HandlePickup
//
// Try to leach ammo from the weapon if you have it already.
//
//===========================================================================

bool AWeapon::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		if (static_cast<AWeapon *>(item)->PickupForAmmo (this))
		{
			item->ItemFlags |= IF_PICKUPGOOD;
		}
		if (MaxAmount > 1) //[SP] If amount<maxamount do another pickup test of the weapon itself!
		{
			return Super::HandlePickup (item);
		}
		return true;
	}
	return false;
}

//===========================================================================
//
// AWeapon :: PickupForAmmo
//
// The player already has this weapon, so try to pick it up for ammo.
//
//===========================================================================

bool AWeapon::PickupForAmmo (AWeapon *ownedWeapon)
{
	bool gotstuff = false;

	// Don't take ammo if the weapon sticks around.
	if (!CallShouldStay ())
	{
		int oldamount1 = 0;
		int oldamount2 = 0;
		if (ownedWeapon->Ammo1 != NULL) oldamount1 = ownedWeapon->Ammo1->Amount;
		if (ownedWeapon->Ammo2 != NULL) oldamount2 = ownedWeapon->Ammo2->Amount;

		if (AmmoGive1 > 0) gotstuff = AddExistingAmmo (ownedWeapon->Ammo1, AmmoGive1);
		if (AmmoGive2 > 0) gotstuff |= AddExistingAmmo (ownedWeapon->Ammo2, AmmoGive2);

		AActor *Owner = ownedWeapon->Owner;
		if (gotstuff && Owner != NULL && Owner->player != NULL)
		{
			if (ownedWeapon->Ammo1 != NULL && oldamount1 == 0)
			{
				static_cast<APlayerPawn *>(Owner)->CheckWeaponSwitch(ownedWeapon->Ammo1->GetClass());
			}
			else if (ownedWeapon->Ammo2 != NULL && oldamount2 == 0)
			{
				static_cast<APlayerPawn *>(Owner)->CheckWeaponSwitch(ownedWeapon->Ammo2->GetClass());
			}
		}
	}
	return gotstuff;
}

//===========================================================================
//
// AWeapon :: CreateCopy
//
//===========================================================================

AInventory *AWeapon::CreateCopy (AActor *other)
{
	AWeapon *copy = static_cast<AWeapon*>(Super::CreateCopy (other));
	if (copy != this)
	{
		copy->AmmoGive1 = AmmoGive1;
		copy->AmmoGive2 = AmmoGive2;
	}
	return copy;
}

//===========================================================================
//
// AWeapon :: CreateTossable
//
// A weapon that's tossed out should contain no ammo, so you can't cheat
// by dropping it and then picking it back up.
//
//===========================================================================

AInventory *AWeapon::CreateTossable ()
{
	// Only drop the weapon that is meant to be placed in a level. That is,
	// only drop the weapon that normally gives you ammo.
	if (SisterWeapon != NULL && 
		((AWeapon*)GetDefault())->AmmoGive1 == 0 &&
		((AWeapon*)GetDefault())->AmmoGive2 == 0 &&
		(((AWeapon*)SisterWeapon->GetDefault())->AmmoGive1 > 0 ||
		 ((AWeapon*)SisterWeapon->GetDefault())->AmmoGive2 > 0))
	{
		return SisterWeapon->CallCreateTossable ();
	}
	AWeapon *copy = static_cast<AWeapon *> (Super::CreateTossable ());

	if (copy != NULL)
	{
		// If this weapon has a sister, remove it from the inventory too.
		if (SisterWeapon != NULL)
		{
			SisterWeapon->SisterWeapon = NULL;
			SisterWeapon->Destroy ();
		}
		// To avoid exploits, the tossed weapon must not have any ammo.
		copy->AmmoGive1 = 0;
		copy->AmmoGive2 = 0;
	}
	return copy;
}

//===========================================================================
//
// AWeapon :: AttachToOwner
//
//===========================================================================

void AWeapon::AttachToOwner (AActor *other)
{
	Super::AttachToOwner (other);

	Ammo1 = AddAmmo (Owner, AmmoType1, AmmoGive1);
	Ammo2 = AddAmmo (Owner, AmmoType2, AmmoGive2);
	SisterWeapon = AddWeapon (SisterWeaponType);
	if (Owner->player != NULL)
	{
		if (!Owner->player->userinfo.GetNeverSwitch() && !(WeaponFlags & WIF_NO_AUTO_SWITCH))
		{
			Owner->player->PendingWeapon = this;
		}
		if (Owner->player->mo == players[consoleplayer].camera)
		{
			StatusBar->ReceivedWeapon (this);
		}
	}
	GivenAsMorphWeapon = false; // will be set explicitly by morphing code
}

//===========================================================================
//
// AWeapon :: AddAmmo
//
// Give some ammo to the owner, even if it's just 0.
//
//===========================================================================

AAmmo *AWeapon::AddAmmo (AActor *other, PClassActor *ammotype, int amount)
{
	AAmmo *ammo;

	if (ammotype == NULL)
	{
		return NULL;
	}

	// [BC] This behavior is from the original Doom. Give 5/2 times as much ammo when
	// we pick up a weapon in deathmatch.
	if (( deathmatch ) && ( gameinfo.gametype & GAME_DoomChex ))
		amount = amount * 5 / 2;

	// extra ammo in baby mode and nightmare mode
	if (!(this->ItemFlags&IF_IGNORESKILL))
	{
		amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
	}
	ammo = static_cast<AAmmo *>(other->FindInventory (ammotype));
	if (ammo == NULL)
	{
		ammo = static_cast<AAmmo *>(Spawn (ammotype));
		ammo->Amount = MIN (amount, ammo->MaxAmount);
		ammo->AttachToOwner (other);
	}
	else if (ammo->Amount < ammo->MaxAmount)
	{
		ammo->Amount += amount;
		if (ammo->Amount > ammo->MaxAmount)
		{
			ammo->Amount = ammo->MaxAmount;
		}
	}
	return ammo;
}

//===========================================================================
//
// AWeapon :: AddExistingAmmo
//
// Give the owner some more ammo he already has.
//
//===========================================================================
EXTERN_CVAR(Bool, sv_unlimited_pickup)

bool AWeapon::AddExistingAmmo (AAmmo *ammo, int amount)
{
	if (ammo != NULL && (ammo->Amount < ammo->MaxAmount || sv_unlimited_pickup))
	{
		// extra ammo in baby mode and nightmare mode
		if (!(ItemFlags&IF_IGNORESKILL))
		{
			amount = int(amount * G_SkillProperty(SKILLP_AmmoFactor));
		}
		ammo->Amount += amount;
		if (ammo->Amount > ammo->MaxAmount && !sv_unlimited_pickup)
		{
			ammo->Amount = ammo->MaxAmount;
		}
		return true;
	}
	return false;
}

//===========================================================================
//
// AWeapon :: AddWeapon
//
// Give the owner a weapon if they don't have it already.
//
//===========================================================================

AWeapon *AWeapon::AddWeapon (PClassWeapon *weapontype)
{
	AWeapon *weap;

	if (weapontype == NULL || !weapontype->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
	{
		return NULL;
	}
	weap = static_cast<AWeapon *>(Owner->FindInventory (weapontype));
	if (weap == NULL)
	{
		weap = static_cast<AWeapon *>(Spawn (weapontype));
		weap->AttachToOwner (Owner);
	}
	return weap;
}

//===========================================================================
//
// AWeapon :: ShouldStay
//
//===========================================================================

bool AWeapon::ShouldStay ()
{
	if (((multiplayer &&
		(!deathmatch && !alwaysapplydmflags)) || (dmflags & DF_WEAPONS_STAY)) &&
		!(flags & MF_DROPPED))
	{
		return true;
	}
	return false;
}

//===========================================================================
//
// AWeapon :: CheckAmmo
//
// Returns true if there is enough ammo to shoot.  If not, selects the
// next weapon to use.
//
//===========================================================================

bool AWeapon::CheckAmmo (int fireMode, bool autoSwitch, bool requireAmmo, int ammocount)
{
	int altFire;
	int count1, count2;
	int enough, enoughmask;
	int lAmmoUse1;

	if ((dmflags & DF_INFINITE_AMMO) || (Owner->player->cheats & CF_INFINITEAMMO))
	{
		return true;
	}
	if (fireMode == EitherFire)
	{
		bool gotSome = CheckAmmo (PrimaryFire, false) || CheckAmmo (AltFire, false);
		if (!gotSome && autoSwitch)
		{
			barrier_cast<APlayerPawn *>(Owner)->PickNewWeapon (NULL);
		}
		return gotSome;
	}
	altFire = (fireMode == AltFire);
	if (!requireAmmo && (WeaponFlags & (WIF_AMMO_OPTIONAL << altFire)))
	{
		return true;
	}
	count1 = (Ammo1 != NULL) ? Ammo1->Amount : 0;
	count2 = (Ammo2 != NULL) ? Ammo2->Amount : 0;

	if ((WeaponFlags & WIF_DEHAMMO) && (Ammo1 == NULL))
	{
		lAmmoUse1 = 0;
	}
	else if (ammocount >= 0 && (WeaponFlags & WIF_DEHAMMO))
	{
		lAmmoUse1 = ammocount;
	}
	else
	{
		lAmmoUse1 = AmmoUse1;
	}

	enough = (count1 >= lAmmoUse1) | ((count2 >= AmmoUse2) << 1);
	if (WeaponFlags & (WIF_PRIMARY_USES_BOTH << altFire))
	{
		enoughmask = 3;
	}
	else
	{
		enoughmask = 1 << altFire;
	}
	if (altFire && FindState(NAME_AltFire) == NULL)
	{ // If this weapon has no alternate fire, then there is never enough ammo for it
		enough &= 1;
	}
	if (((enough & enoughmask) == enoughmask) || (enough && (WeaponFlags & WIF_AMMO_CHECKBOTH)))
	{
		return true;
	}
	// out of ammo, pick a weapon to change to
	if (autoSwitch)
	{
		barrier_cast<APlayerPawn *>(Owner)->PickNewWeapon (NULL);
	}
	return false;
}

DEFINE_ACTION_FUNCTION(AWeapon, CheckAmmo)
{
	PARAM_SELF_PROLOGUE(AWeapon);
	PARAM_INT(mode);
	PARAM_BOOL(autoswitch);
	PARAM_BOOL_DEF(require);
	PARAM_INT_DEF(ammocnt);
	ACTION_RETURN_BOOL(self->CheckAmmo(mode, autoswitch, require, ammocnt));
}

//===========================================================================
//
// AWeapon :: DepleteAmmo
//
// Use up some of the weapon's ammo. Returns true if the ammo was successfully
// depleted. If checkEnough is false, then the ammo will always be depleted,
// even if it drops below zero.
//
//===========================================================================

bool AWeapon::DepleteAmmo (bool altFire, bool checkEnough, int ammouse)
{
	if (!((dmflags & DF_INFINITE_AMMO) || (Owner->player->cheats & CF_INFINITEAMMO)))
	{
		if (checkEnough && !CheckAmmo (altFire ? AltFire : PrimaryFire, false, false, ammouse))
		{
			return false;
		}
		if (!altFire)
		{
			if (Ammo1 != NULL)
			{
				if (ammouse >= 0 && (WeaponFlags & WIF_DEHAMMO))
				{
					Ammo1->Amount -= ammouse;
				}
				else
				{
					Ammo1->Amount -= AmmoUse1;
				}
			}
			if ((WeaponFlags & WIF_PRIMARY_USES_BOTH) && Ammo2 != NULL)
			{
				Ammo2->Amount -= AmmoUse2;
			}
		}
		else
		{
			if (Ammo2 != NULL)
			{
				Ammo2->Amount -= AmmoUse2;
			}
			if ((WeaponFlags & WIF_ALT_USES_BOTH) && Ammo1 != NULL)
			{
				Ammo1->Amount -= AmmoUse1;
			}
		}
		if (Ammo1 != NULL && Ammo1->Amount < 0)
			Ammo1->Amount = 0;
		if (Ammo2 != NULL && Ammo2->Amount < 0)
			Ammo2->Amount = 0;
	}
	return true;
}

DEFINE_ACTION_FUNCTION(AWeapon, DepleteAmmo)
{
	PARAM_SELF_PROLOGUE(AWeapon);
	PARAM_BOOL(altfire);
	PARAM_BOOL_DEF(checkenough);
	PARAM_INT_DEF(ammouse);
	ACTION_RETURN_BOOL(self->DepleteAmmo(altfire, checkenough, ammouse));
}


//===========================================================================
//
// AWeapon :: PostMorphWeapon
//
// Bring this weapon up after a player unmorphs.
//
//===========================================================================

void AWeapon::PostMorphWeapon ()
{
	DPSprite *pspr;
	if (Owner == nullptr)
	{
		return;
	}
	Owner->player->PendingWeapon = WP_NOCHANGE;
	Owner->player->ReadyWeapon = this;
	Owner->player->refire = 0;

	pspr = Owner->player->GetPSprite(PSP_WEAPON);
	pspr->y = WEAPONBOTTOM;
	pspr->ResetInterpolation();
	pspr->SetState(GetUpState());
}

//===========================================================================
//
// AWeapon :: EndPowerUp
//
// The Tome of Power just expired.
//
//===========================================================================

void AWeapon::EndPowerup ()
{
	if (SisterWeapon != NULL && WeaponFlags&WIF_POWERED_UP)
	{
		if (GetReadyState() != SisterWeapon->GetReadyState())
		{
			if (Owner->player->PendingWeapon == NULL ||
				Owner->player->PendingWeapon == WP_NOCHANGE)
				Owner->player->PendingWeapon = SisterWeapon;
		}
		else
		{
			DPSprite *psp = Owner->player->FindPSprite(PSP_WEAPON);
			if (psp != nullptr && psp->GetCaller() == Owner->player->ReadyWeapon)
			{
				// If the weapon changes but the state does not, we have to manually change the PSprite's caller here.
				psp->SetCaller(SisterWeapon);
				Owner->player->ReadyWeapon = SisterWeapon;
			}
			else
			{
				// Something went wrong. Initiate a regular weapon change.
				Owner->player->PendingWeapon = SisterWeapon;
			}
		}
	}
}

DEFINE_ACTION_FUNCTION(AWeapon, EndPowerup)
{
	PARAM_SELF_PROLOGUE(AWeapon);
	self->EndPowerup();
	return 0;
}

void AWeapon::CallEndPowerup()
{
	IFVIRTUAL(AWeapon, EndPowerup)
	{
		// Without the type cast this picks the 'void *' assignment...
		VMValue params[1] = { (DObject*)this };
		GlobalVMStack.Call(func, params, 1, nullptr, 0, nullptr);
	}
	else EndPowerup();
}


//===========================================================================
//
// AWeapon :: GetUpState
//
//===========================================================================

FState *AWeapon::GetUpState ()
{
	IFVIRTUAL(AWeapon, GetUpState)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetDownState
//
//===========================================================================

FState *AWeapon::GetDownState ()
{
	IFVIRTUAL(AWeapon, GetDownState)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetReadyState
//
//===========================================================================

FState *AWeapon::GetReadyState ()
{
	IFVIRTUAL(AWeapon, GetReadyState)
	{
		VMValue params[1] = { (DObject*)this };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 1, &ret, 1, nullptr);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetAtkState
//
//===========================================================================

FState *AWeapon::GetAtkState (bool hold)
{
	IFVIRTUAL(AWeapon, GetAtkState)
	{
		VMValue params[2] = { (DObject*)this, hold };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetAtkState
//
//===========================================================================

FState *AWeapon::GetAltAtkState (bool hold)
{
	IFVIRTUAL(AWeapon, GetAltAtkState)
	{
		VMValue params[2] = { (DObject*)this, hold };
		VMReturn ret;
		FState *retval;
		ret.PointerAt((void**)&retval);
		GlobalVMStack.Call(func, params, 2, &ret, 1, nullptr);
		return retval;
	}
	return nullptr;
}

//===========================================================================
//
// AWeapon :: GetStateForButtonName
//
//===========================================================================

FState *AWeapon::GetStateForButtonName (FName button)
{
	return FindState(button);
}


/* Weapon giver ***********************************************************/

IMPLEMENT_CLASS(AWeaponGiver, false, false)

DEFINE_FIELD(AWeaponGiver, DropAmmoFactor);

void AWeaponGiver::Serialize(FSerializer &arc)
{
	Super::Serialize(arc);
	auto def = (AWeaponGiver *)GetDefault();
	arc("dropammofactor", DropAmmoFactor, def->DropAmmoFactor);
}

bool AWeaponGiver::TryPickup(AActor *&toucher)
{
	DDropItem *di = GetDropItems();
	AWeapon *weap;

	if (di != NULL)
	{
		PClassWeapon *ti = dyn_cast<PClassWeapon>(PClass::FindClass(di->Name));
		if (ti != NULL)
		{
			if (master == NULL)
			{
				master = weap = static_cast<AWeapon*>(Spawn(di->Name));
				if (weap != NULL)
				{
					weap->ItemFlags &= ~IF_ALWAYSPICKUP;	// use the flag of this item only.
					weap->flags = (weap->flags & ~MF_DROPPED) | (this->flags & MF_DROPPED);

					// If our ammo gives are non-negative, transfer them to the real weapon.
					if (AmmoGive1 >= 0) weap->AmmoGive1 = AmmoGive1;
					if (AmmoGive2 >= 0) weap->AmmoGive2 = AmmoGive2;

					// If DropAmmoFactor is non-negative, modify the given ammo amounts.
					if (DropAmmoFactor > 0)
					{
						weap->AmmoGive1 = int(weap->AmmoGive1 * DropAmmoFactor);
						weap->AmmoGive2 = int(weap->AmmoGive2 * DropAmmoFactor);
					}
					weap->BecomeItem();
				}
				else return false;
			}

			weap = barrier_cast<AWeapon*>(master);
			bool res = weap->CallTryPickup(toucher);
			if (res)
			{
				GoAwayAndDie();
				master = NULL;
			}
			return res;
		}
	}
	return false;
}

/* Weapon slots ***********************************************************/

//===========================================================================
//
// FWeaponSlot :: AddWeapon
//
// Adds a weapon to the end of the slot if it isn't already in it.
//
//===========================================================================

bool FWeaponSlot::AddWeapon(const char *type)
{
	return AddWeapon(static_cast<PClassWeapon *>(PClass::FindClass(type)));
}

bool FWeaponSlot::AddWeapon(PClassWeapon *type)
{
	unsigned int i;
	
	if (type == NULL)
	{
		return false;
	}
	
	if (!type->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
	{
		Printf("Can't add non-weapon %s to weapon slots\n", type->TypeName.GetChars());
		return false;
	}

	for (i = 0; i < Weapons.Size(); i++)
	{
		if (Weapons[i].Type == type)
			return true;	// Already present
	}
	WeaponInfo info = { type, -1 };
	Weapons.Push(info);
	return true;
}

//===========================================================================
//
// FWeaponSlot :: AddWeaponList
//
// Appends all the weapons from the space-delimited list to this slot.
// Set clear to true to remove any weapons already in this slot first.
//
//===========================================================================

void FWeaponSlot :: AddWeaponList(const char *list, bool clear)
{
	FString copy(list);
	char *buff = copy.LockBuffer();
	char *tok;

	if (clear)
	{
		Clear();
	}
	tok = strtok(buff, " ");
	while (tok != NULL)
	{
		AddWeapon(tok);
		tok = strtok(NULL, " ");
	}
}

//===========================================================================
//
// FWeaponSlot :: LocateWeapon
//
// Returns the index for the specified weapon in this slot, or -1 if it isn't
// in this slot.
//
//===========================================================================

int FWeaponSlot::LocateWeapon(PClassWeapon *type)
{
	unsigned int i;

	for (i = 0; i < Weapons.Size(); ++i)
	{
		if (Weapons[i].Type == type)
		{
			return (int)i;
		}
	}
	return -1;
}

//===========================================================================
//
// FWeaponSlot :: PickWeapon
//
// Picks a weapon from this slot. If no weapon is selected in this slot,
// or the first weapon in this slot is selected, returns the last weapon.
// Otherwise, returns the previous weapon in this slot. This means
// precedence is given to the last weapon in the slot, which by convention
// is probably the strongest. Does not return weapons you have no ammo
// for or which you do not possess.
//
//===========================================================================

AWeapon *FWeaponSlot::PickWeapon(player_t *player, bool checkammo)
{
	int i, j;

	if (player->mo == NULL)
	{
		return NULL;
	}
	// Does this slot even have any weapons?
	if (Weapons.Size() == 0)
	{
		return player->ReadyWeapon;
	}
	if (player->ReadyWeapon != NULL)
	{
		for (i = 0; (unsigned)i < Weapons.Size(); i++)
		{
			if (Weapons[i].Type == player->ReadyWeapon->GetClass() ||
				(player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP &&
				 player->ReadyWeapon->SisterWeapon != NULL &&
				 player->ReadyWeapon->SisterWeapon->GetClass() == Weapons[i].Type))
			{
				for (j = (i == 0 ? Weapons.Size() - 1 : i - 1);
					j != i;
					j = (j == 0 ? Weapons.Size() - 1 : j - 1))
				{
					AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory(Weapons[j].Type));

					if (weap != NULL && weap->IsKindOf(RUNTIME_CLASS(AWeapon)))
					{
						if (!checkammo || weap->CheckAmmo(AWeapon::EitherFire, false))
						{
							return weap;
						}
					}
				}
			}
		}
	}
	for (i = Weapons.Size() - 1; i >= 0; i--)
	{
		AWeapon *weap = static_cast<AWeapon *> (player->mo->FindInventory(Weapons[i].Type));

		if (weap != NULL && weap->IsKindOf(RUNTIME_CLASS(AWeapon)))
		{
			if (!checkammo || weap->CheckAmmo(AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
	}
	return player->ReadyWeapon;
}

//===========================================================================
//
// FWeaponSlot :: SetInitialPositions
//
// Fills in the position field for every weapon currently in the slot based
// on its position in the slot. These are not scaled to [0,1] so that extra
// weapons can use those values to go to the start or end of the slot.
//
//===========================================================================

void FWeaponSlot::SetInitialPositions()
{
	unsigned int size = Weapons.Size(), i;

	if (size == 1)
	{
		Weapons[0].Position = 0x8000;
	}
	else
	{
		for (i = 0; i < size; ++i)
		{
			Weapons[i].Position = i * 0xFF00 / (size - 1) + 0x80;
		}
	}
}

//===========================================================================
//
// FWeaponSlot :: Sort
//
// Rearranges the weapons by their position field.
//
//===========================================================================

void FWeaponSlot::Sort()
{
	// This does not use qsort(), because the sort should be stable, and
	// there is no guarantee that qsort() is stable. This insertion sort
	// should be fine.
	int i, j;

	for (i = 1; i < (int)Weapons.Size(); ++i)
	{
		int pos = Weapons[i].Position;
		PClassWeapon *type = Weapons[i].Type;
		for (j = i - 1; j >= 0 && Weapons[j].Position > pos; --j)
		{
			Weapons[j + 1] = Weapons[j];
		}
		Weapons[j + 1].Type = type;
		Weapons[j + 1].Position = pos;
	}
}

//===========================================================================
//
// FWeaponSlots - Copy Constructor
//
//===========================================================================

FWeaponSlots::FWeaponSlots(const FWeaponSlots &other)
{
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i] = other.Slots[i];
	}
}

//===========================================================================
//
// FWeaponSlots :: Clear
//
// Removes all weapons from every slot.
//
//===========================================================================

void FWeaponSlots::Clear()
{
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].Clear();
	}
}

//===========================================================================
//
// FWeaponSlots :: AddDefaultWeapon
//
// If the weapon already exists in a slot, don't add it. If it doesn't,
// then add it to the specified slot.
//
//===========================================================================

ESlotDef FWeaponSlots::AddDefaultWeapon (int slot, PClassWeapon *type)
{
	int currSlot, index;

	if (!LocateWeapon (type, &currSlot, &index))
	{
		if (slot >= 0 && slot < NUM_WEAPON_SLOTS)
		{
			bool added = Slots[slot].AddWeapon (type);
			return added ? SLOTDEF_Added : SLOTDEF_Full;
		}
		return SLOTDEF_Full;
	}
	return SLOTDEF_Exists;
}

//===========================================================================
//
// FWeaponSlots :: LocateWeapon
//
// Returns true if the weapon is in a slot, false otherwise. If the weapon
// is found, it can also optionally return the slot and index for it.
//
//===========================================================================

bool FWeaponSlots::LocateWeapon (PClassWeapon *type, int *const slot, int *const index)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; i++)
	{
		j = Slots[i].LocateWeapon(type);
		if (j >= 0)
		{
			if (slot != NULL) *slot = i;
			if (index != NULL) *index = j;
			return true;
		}
	}
	return false;
}

//===========================================================================
//
// FindMostRecentWeapon
//
// Locates the slot and index for the most recently selected weapon. If the
// player is in the process of switching to a new weapon, that is the most
// recently selected weapon. Otherwise, the current weapon is the most recent
// weapon.
//
//===========================================================================

static bool FindMostRecentWeapon(player_t *player, int *slot, int *index)
{
	if (player->PendingWeapon != WP_NOCHANGE)
	{
		return player->weapons.LocateWeapon(player->PendingWeapon->GetClass(), slot, index);
	}
	else if (player->ReadyWeapon != NULL)
	{
		AWeapon *weap = player->ReadyWeapon;
		if (!player->weapons.LocateWeapon(weap->GetClass(), slot, index))
		{
			// If the current weapon wasn't found and is powered up,
			// look for its non-powered up version.
			if (weap->WeaponFlags & WIF_POWERED_UP && weap->SisterWeaponType != NULL)
			{
				return player->weapons.LocateWeapon(weap->SisterWeaponType, slot, index);
			}
			return false;
		}
		return true;
	}
	else
	{
		return false;
	}
}

//===========================================================================
//
// FWeaponSlots :: PickNextWeapon
//
// Returns the "next" weapon for this player. If the current weapon is not
// in a slot, then it just returns that weapon, since there's nothing to
// consider it relative to.
//
//===========================================================================

AWeapon *FWeaponSlots::PickNextWeapon(player_t *player)
{
	int startslot, startindex;
	int slotschecked = 0;

	if (player->mo == NULL)
	{
		return NULL;
	}
	if (player->ReadyWeapon == NULL || FindMostRecentWeapon(player, &startslot, &startindex))
	{
		int slot;
		int index;

		if (player->ReadyWeapon == NULL)
		{
			startslot = NUM_WEAPON_SLOTS - 1;
			startindex = Slots[startslot].Size() - 1;
		}

		slot = startslot;
		index = startindex;
		do
		{
			if (++index >= Slots[slot].Size())
			{
				index = 0;
				slotschecked++;
				if (++slot >= NUM_WEAPON_SLOTS)
				{
					slot = 0;
				}
			}
			PClassWeapon *type = Slots[slot].GetWeapon(index);
			AWeapon *weap = static_cast<AWeapon *>(player->mo->FindInventory(type));
			if (weap != NULL && weap->CheckAmmo(AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
		while ((slot != startslot || index != startindex) && slotschecked <= NUM_WEAPON_SLOTS);
	}
	return player->ReadyWeapon;
}

//===========================================================================
//
// FWeaponSlots :: PickPrevWeapon
//
// Returns the "previous" weapon for this player. If the current weapon is
// not in a slot, then it just returns that weapon, since there's nothing to
// consider it relative to.
//
//===========================================================================

AWeapon *FWeaponSlots::PickPrevWeapon (player_t *player)
{
	int startslot, startindex;
	int slotschecked = 0;

	if (player->mo == NULL)
	{
		return NULL;
	}
	if (player->ReadyWeapon == NULL || FindMostRecentWeapon (player, &startslot, &startindex))
	{
		int slot;
		int index;

		if (player->ReadyWeapon == NULL)
		{
			startslot = 0;
			startindex = 0;
		}

		slot = startslot;
		index = startindex;
		do
		{
			if (--index < 0)
			{
				slotschecked++;
				if (--slot < 0)
				{
					slot = NUM_WEAPON_SLOTS - 1;
				}
				index = Slots[slot].Size() - 1;
			}
			PClassWeapon *type = Slots[slot].GetWeapon(index);
			AWeapon *weap = static_cast<AWeapon *>(player->mo->FindInventory(type));
			if (weap != NULL && weap->CheckAmmo(AWeapon::EitherFire, false))
			{
				return weap;
			}
		}
		while ((slot != startslot || index != startindex) && slotschecked <= NUM_WEAPON_SLOTS);
	}
	return player->ReadyWeapon;
}

//===========================================================================
//
// FWeaponSlots :: AddExtraWeapons
//
// For every weapon class for the current game, add it to its desired slot
// and position within the slot. Does not first clear the slots.
//
//===========================================================================

void FWeaponSlots::AddExtraWeapons()
{
	unsigned int i;

	// Set fractional positions for current weapons.
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].SetInitialPositions();
	}

	// Append extra weapons to the slots.
	for (unsigned int i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClass *cls = PClassActor::AllActorClasses[i];

		if (!cls->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			continue;
		}
		PClassWeapon *acls = static_cast<PClassWeapon *>(cls);
		if ((acls->GameFilter == GAME_Any || (acls->GameFilter & gameinfo.gametype)) &&
			acls->Replacement == NULL &&		// Replaced weapons don't get slotted.
			!(((AWeapon *)(acls->Defaults))->WeaponFlags & WIF_POWERED_UP) &&
			!LocateWeapon(acls, NULL, NULL)		// Don't duplicate it if it's already present.
			)
		{
			int slot = acls->SlotNumber;
			if ((unsigned)slot < NUM_WEAPON_SLOTS)
			{
				FWeaponSlot::WeaponInfo info = { acls, acls->SlotPriority };
				Slots[slot].Weapons.Push(info);
			}
		}
	}

	// Now resort every slot to put the new weapons in their proper places.
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		Slots[i].Sort();
	}
}

//===========================================================================
//
// FWeaponSlots :: SetFromGameInfo
//
// If neither the player class nor any defined weapon contain a
// slot assignment, use the game's defaults
//
//===========================================================================

void FWeaponSlots::SetFromGameInfo()
{
	unsigned int i;

	// Only if all slots are empty
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		if (Slots[i].Size() > 0) return;
	}

	// Append extra weapons to the slots.
	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		for (unsigned j = 0; j < gameinfo.DefaultWeaponSlots[i].Size(); j++)
		{
			PClassWeapon *cls = dyn_cast<PClassWeapon>(PClass::FindClass(gameinfo.DefaultWeaponSlots[i][j]));
			if (cls == NULL)
			{
				Printf("Unknown weapon class '%s' found in default weapon slot assignments\n",
					gameinfo.DefaultWeaponSlots[i][j].GetChars());
			}
			else
			{
				Slots[i].AddWeapon(cls);
			}
		}
	}
}

//===========================================================================
//
// FWeaponSlots :: StandardSetup
//
// Setup weapons in this order:
// 1. Use slots from player class.
// 2. Add extra weapons that specify their own slots.
// 3. If all slots are empty, use the settings from the gameinfo (compatibility fallback)
//
//===========================================================================

void FWeaponSlots::StandardSetup(PClassPlayerPawn *type)
{
	SetFromPlayer(type);
	AddExtraWeapons();
	SetFromGameInfo();
}

//===========================================================================
//
// FWeaponSlots :: LocalSetup
//
// Setup weapons in this order:
// 1. Run KEYCONF weapon commands, affecting slots accordingly.
// 2. Read config slots, overriding current slots. If WeaponSection is set,
//    then [<WeaponSection>.<PlayerClass>.Weapons] is tried, followed by
//    [<WeaponSection>.Weapons] if that did not exist. If WeaponSection is
//    empty, then the slots are read from [<PlayerClass>.Weapons].
//
//===========================================================================

void FWeaponSlots::LocalSetup(PClassActor *type)
{
	P_PlaybackKeyConfWeapons(this);
	if (WeaponSection.IsNotEmpty())
	{
		FString sectionclass(WeaponSection);
		sectionclass << '.' << type->TypeName.GetChars();
		if (RestoreSlots(GameConfig, sectionclass) == 0)
		{
			RestoreSlots(GameConfig, WeaponSection);
		}
	}
	else
	{
		RestoreSlots(GameConfig, type->TypeName.GetChars());
	}
}

//===========================================================================
//
// FWeaponSlots :: SendDifferences
//
// Sends the weapon slots from this instance that differ from other's.
//
//===========================================================================

void FWeaponSlots::SendDifferences(int playernum, const FWeaponSlots &other)
{
	int i, j;

	for (i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		if (other.Slots[i].Size() == Slots[i].Size())
		{
			for (j = (int)Slots[i].Size(); j-- > 0; )
			{
				if (other.Slots[i].GetWeapon(j) != Slots[i].GetWeapon(j))
				{
					break;
				}
			}
			if (j < 0)
			{ // The two slots are the same.
				continue;
			}
		}
		// The slots differ. Send mine.
		if (playernum == consoleplayer)
		{
			Net_WriteByte(DEM_SETSLOT);
		}
		else
		{
			Net_WriteByte(DEM_SETSLOTPNUM);
			Net_WriteByte(playernum);
		}
		Net_WriteByte(i);
		Net_WriteByte(Slots[i].Size());
		for (j = 0; j < Slots[i].Size(); ++j)
		{
			Net_WriteWeapon(Slots[i].GetWeapon(j));
		}
	}
}

//===========================================================================
//
// FWeaponSlots :: SetFromPlayer
//
// Sets all weapon slots according to the player class.
//
//===========================================================================

void FWeaponSlots::SetFromPlayer(PClassPlayerPawn *type)
{
	Clear();
	for (int i = 0; i < NUM_WEAPON_SLOTS; ++i)
	{
		if (!type->Slot[i].IsEmpty())
		{
			Slots[i].AddWeaponList(type->Slot[i], false);
		}
	}
}

//===========================================================================
//
// FWeaponSlots :: RestoreSlots
//
// Reads slots from a config section. Any slots in the section override
// existing slot settings, while slots not present in the config are
// unaffected. Returns the number of slots read.
//
//===========================================================================

int FWeaponSlots::RestoreSlots(FConfigFile *config, const char *section)
{
	FString section_name(section);
	const char *key, *value;
	int slotsread = 0;

	section_name += ".Weapons";
	if (!config->SetSection(section_name))
	{
		return 0;
	}
	while (config->NextInSection (key, value))
	{
		if (strnicmp (key, "Slot[", 5) != 0 ||
			key[5] < '0' ||
			key[5] > '0'+NUM_WEAPON_SLOTS ||
			key[6] != ']' ||
			key[7] != 0)
		{
			continue;
		}
		Slots[key[5] - '0'].AddWeaponList(value, true);
		slotsread++;
	}
	return slotsread;
}

//===========================================================================
//
// CCMD setslot
//
//===========================================================================

void FWeaponSlots::PrintSettings()
{
	for (int i = 1; i <= NUM_WEAPON_SLOTS; ++i)
	{
		int slot = i % NUM_WEAPON_SLOTS;
		if (Slots[slot].Size() > 0)
		{
			Printf("Slot[%d]=", slot);
			for (int j = 0; j < Slots[slot].Size(); ++j)
			{
				Printf("%s ", Slots[slot].GetWeapon(j)->TypeName.GetChars());
			}
			Printf("\n");
		}
	}
}

CCMD (setslot)
{
	int slot;

	if (argv.argc() < 2 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf("Usage: setslot [slot] [weapons]\nCurrent slot assignments:\n");
		if (players[consoleplayer].mo != NULL)
		{
			FString config(GameConfig->GetConfigPath(false));
			Printf(TEXTCOLOR_BLUE "Add the following to " TEXTCOLOR_ORANGE "%s" TEXTCOLOR_BLUE
				" to retain these bindings:\n" TEXTCOLOR_NORMAL "[", config.GetChars());
			if (WeaponSection.IsNotEmpty())
			{
				Printf("%s.", WeaponSection.GetChars());
			}
			Printf("%s.Weapons]\n", players[consoleplayer].mo->GetClass()->TypeName.GetChars());
		}
		players[consoleplayer].weapons.PrintSettings();
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != NULL)
	{
		PlayingKeyConf->Slots[slot].Clear();
		for (int i = 2; i < argv.argc(); ++i)
		{
			PlayingKeyConf->Slots[slot].AddWeapon(argv[i]);
		}
	}
	else
	{
		if (argv.argc() == 2)
		{
			Printf ("Slot %d cleared\n", slot);
		}

		Net_WriteByte(DEM_SETSLOT);
		Net_WriteByte(slot);
		Net_WriteByte(argv.argc()-2);
		for (int i = 2; i < argv.argc(); i++)
		{
			Net_WriteWeapon(dyn_cast<PClassWeapon>(PClass::FindClass(argv[i])));
		}
	}
}

//===========================================================================
//
// CCMD addslot
//
//===========================================================================

void FWeaponSlots::AddSlot(int slot, PClassWeapon *type, bool feedback)
{
	if (type != NULL && !Slots[slot].AddWeapon(type) && feedback)
	{
		Printf ("Could not add %s to slot %d\n", type->TypeName.GetChars(), slot);
	}
}

CCMD (addslot)
{
	unsigned int slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslot <slot> <weapon>\n");
		return;
	}

	PClassWeapon *type= dyn_cast<PClassWeapon>(PClass::FindClass(argv[2]));
	if (type == NULL)
	{
		Printf("%s is not a weapon\n", argv[2]);
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != NULL)
	{
		PlayingKeyConf->AddSlot(int(slot), type, false);
	}
	else
	{
		Net_WriteByte(DEM_ADDSLOT);
		Net_WriteByte(slot);
		Net_WriteWeapon(type);
	}
}

//===========================================================================
//
// CCMD weaponsection
//
//===========================================================================

CCMD (weaponsection)
{
	if (argv.argc() > 1)
	{
		WeaponSection = argv[1];
	}
}

//===========================================================================
//
// CCMD addslotdefault
//
//===========================================================================
void FWeaponSlots::AddSlotDefault(int slot, PClassWeapon *type, bool feedback)
{
	if (type != NULL && type->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
	{
		switch (AddDefaultWeapon(slot, type))
		{
		case SLOTDEF_Full:
			if (feedback)
			{
				Printf ("Could not add %s to slot %d\n", type->TypeName.GetChars(), slot);
			}
			break;

		default:
		case SLOTDEF_Added:
			break;

		case SLOTDEF_Exists:
			break;
		}
	}
}

CCMD (addslotdefault)
{
	PClassWeapon *type;
	unsigned int slot;

	if (argv.argc() != 3 || (slot = atoi (argv[1])) >= NUM_WEAPON_SLOTS)
	{
		Printf ("Usage: addslotdefault <slot> <weapon>\n");
		return;
	}

	type = dyn_cast<PClassWeapon>(PClass::FindClass(argv[2]));
	if (type == NULL)
	{
		Printf ("%s is not a weapon\n", argv[2]);
		return;
	}

	if (ParsingKeyConf)
	{
		KeyConfWeapons.Push(argv.args());
	}
	else if (PlayingKeyConf != NULL)
	{
		PlayingKeyConf->AddSlotDefault(int(slot), type, false);
	}
	else
	{
		Net_WriteByte(DEM_ADDSLOTDEFAULT);
		Net_WriteByte(slot);
		Net_WriteWeapon(type);
	}
}

//===========================================================================
//
// P_PlaybackKeyConfWeapons
//
// Executes the weapon-related commands from a KEYCONF lump.
//
//===========================================================================

void P_PlaybackKeyConfWeapons(FWeaponSlots *slots)
{
	PlayingKeyConf = slots;
	for (unsigned int i = 0; i < KeyConfWeapons.Size(); ++i)
	{
		FString cmd(KeyConfWeapons[i]);
		AddCommandString(cmd.LockBuffer());
	}
	PlayingKeyConf = NULL;
}

//===========================================================================
//
// P_SetupWeapons_ntohton
//
// Initializes the ntoh and hton maps for weapon types. To populate the ntoh
// array, weapons are sorted first by game, then lexicographically. Weapons
// from the current game are sorted first, followed by weapons for all other
// games, and within each block, they are sorted by name.
//
//===========================================================================

void P_SetupWeapons_ntohton()
{
	unsigned int i;
	PClassWeapon *cls;

	Weapons_ntoh.Clear();
	Weapons_hton.Clear();

	cls = NULL;
	Weapons_ntoh.Push(cls);		// Index 0 is always NULL.
	for (i = 0; i < PClassActor::AllActorClasses.Size(); ++i)
	{
		PClassActor *cls = PClassActor::AllActorClasses[i];

		if (cls->IsDescendantOf(RUNTIME_CLASS(AWeapon)))
		{
			Weapons_ntoh.Push(static_cast<PClassWeapon *>(cls));
		}
	}
	qsort(&Weapons_ntoh[1], Weapons_ntoh.Size() - 1, sizeof(Weapons_ntoh[0]), ntoh_cmp);
	for (i = 0; i < Weapons_ntoh.Size(); ++i)
	{
		Weapons_hton[Weapons_ntoh[i]] = i;
	}
}

//===========================================================================
//
// ntoh_cmp
//
// Sorting comparison function used by P_SetupWeapons_ntohton().
//
// Weapons that filter for the current game appear first, weapons that filter
// for any game appear second, and weapons that filter for some other game
// appear last. The idea here is to try to keep all the weapons that are
// most likely to be used at the start of the list so that they only need
// one byte to transmit across the network.
//
//===========================================================================

static int ntoh_cmp(const void *a, const void *b)
{
	PClassWeapon *c1 = *(PClassWeapon **)a;
	PClassWeapon *c2 = *(PClassWeapon **)b;
	int g1 = c1->GameFilter == GAME_Any ? 1 : (c1->GameFilter & gameinfo.gametype) ? 0 : 2;
	int g2 = c2->GameFilter == GAME_Any ? 1 : (c2->GameFilter & gameinfo.gametype) ? 0 : 2;
	if (g1 != g2)
	{
		return g1 - g2;
	}
	return stricmp(c1->TypeName.GetChars(), c2->TypeName.GetChars());
}

//===========================================================================
//
// P_WriteDemoWeaponsChunk
//
// Store the list of weapons so that adding new ones does not automatically
// break demos.
//
//===========================================================================

void P_WriteDemoWeaponsChunk(BYTE **demo)
{
	WriteWord(Weapons_ntoh.Size(), demo);
	for (unsigned int i = 1; i < Weapons_ntoh.Size(); ++i)
	{
		WriteString(Weapons_ntoh[i]->TypeName.GetChars(), demo);
	}
}

//===========================================================================
//
// P_ReadDemoWeaponsChunk
//
// Restore the list of weapons that was current at the time the demo was
// recorded.
//
//===========================================================================

void P_ReadDemoWeaponsChunk(BYTE **demo)
{
	int count, i;
	PClassWeapon *type;
	const char *s;

	count = ReadWord(demo);
	Weapons_ntoh.Resize(count);
	Weapons_hton.Clear(count);

	Weapons_ntoh[0] = type = NULL;
	Weapons_hton[type] = 0;

	for (i = 1; i < count; ++i)
	{
		s = ReadStringConst(demo);
		type = dyn_cast<PClassWeapon>(PClass::FindClass(s));
		// If a demo was recorded with a weapon that is no longer present,
		// should we report it?
		Weapons_ntoh[i] = type;
		if (type != NULL)
		{
			Weapons_hton[type] = i;
		}
	}
}

//===========================================================================
//
// Net_WriteWeapon
//
//===========================================================================

void Net_WriteWeapon(PClassWeapon *type)
{
	int index, *index_p;

	index_p = Weapons_hton.CheckKey(type);
	if (index_p == NULL)
	{
		index = 0;
	}
	else
	{
		index = *index_p;
	}
	// 32767 weapons better be enough for anybody.
	assert(index >= 0 && index <= 32767);
	if (index < 128)
	{
		Net_WriteByte(index);
	}
	else
	{
		Net_WriteByte(0x80 | index);
		Net_WriteByte(index >> 7);
	}
}

//===========================================================================
//
// Net_ReadWeapon
//
//===========================================================================

PClassWeapon *Net_ReadWeapon(BYTE **stream)
{
	int index;

	index = ReadByte(stream);
	if (index & 0x80)
	{
		index = (index & 0x7F) | (ReadByte(stream) << 7);
	}
	if ((unsigned)index >= Weapons_ntoh.Size())
	{
		return NULL;
	}
	return Weapons_ntoh[index];
}

//===========================================================================
//
// A_ZoomFactor
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AWeapon, A_ZoomFactor)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_FLOAT_DEF(zoom);
	PARAM_INT_DEF(flags);

	if (self->player != NULL && self->player->ReadyWeapon != NULL)
	{
		zoom = 1 / clamp(zoom, 0.1, 50.0);
		if (flags & 1)
		{ // Make the zoom instant.
			self->player->FOV = float(self->player->DesiredFOV * zoom);
		}
		if (flags & 2)
		{ // Disable pitch/yaw scaling.
			zoom = -zoom;
		}
		self->player->ReadyWeapon->FOVScale = float(zoom);
	}
	return 0;
}

//===========================================================================
//
// A_SetCrosshair
//
//===========================================================================

DEFINE_ACTION_FUNCTION(AWeapon, A_SetCrosshair)
{
	PARAM_ACTION_PROLOGUE(AActor);
	PARAM_INT(xhair);

	if (self->player != NULL && self->player->ReadyWeapon != NULL)
	{
		self->player->ReadyWeapon->Crosshair = xhair;
	}
	return 0;
}
