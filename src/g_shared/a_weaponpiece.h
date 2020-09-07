// [Dusk] Added header guard here to prevent multi-definition errors.
// sv_commands.h refers to this file, and a lot of files refer to it.
#ifndef __A_WEAPONPIECE_H__
#define __A_WEAPONPIECE_H__

class AWeaponPiece : public AInventory
{
	DECLARE_CLASS (AWeaponPiece, AInventory)
	HAS_OBJECT_POINTERS
protected:
	bool PrivateShouldStay ();
public:
	void Serialize (FArchive &arc);
	bool TryPickup (AActor *&toucher);
	bool TryPickupRestricted (AActor *&toucher);
	bool ShouldStay ();
	virtual const char *PickupMessage ();
	virtual void PlayPickupSound (AActor *toucher);

	int PieceValue;
	const PClass * WeaponClass;
	TObjPtr<AWeapon> FullWeapon;
};

// an internal class to hold the information for player class independent weapon piece handling
// [BL] Needs to be available for SBarInfo to check weaponpieces
class AWeaponHolder : public AInventory
{
	DECLARE_CLASS(AWeaponHolder, AInventory)

public:
	int PieceMask;
	const PClass * PieceWeapon;

	void Serialize (FArchive &arc);
};

#endif // __A_WEAPONPIECE_H__