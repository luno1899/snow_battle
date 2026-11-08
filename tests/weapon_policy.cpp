#include "weapon_policy.h"
#include <cstdio>

int main()
{
	const char* blocked[] = {"weapon_glock", "weapon_usp_silencer", "weapon_hkp2000",
		"weapon_deagle", "weapon_revolver", "weapon_ak47", "weapon_m4a1",
		"weapon_awp", "weapon_negev", "weapon_hegrenade", "weapon_molotov"};
	const char* allowed[] = {"weapon_decoy", "weapon_knife", "weapon_knife_t",
		"weapon_knife_karambit", "weapon_bayonet", "decoy_projectile", "cs_gamerules", "", nullptr};
	for (const char* name : blocked)
		if (!SnowballBlocksWeapon(name))
			return std::fprintf(stderr, "Weapon not blocked: %s\n", name), 1;
	for (const char* name : allowed)
		if (SnowballBlocksWeapon(name))
			return std::fprintf(stderr, "Unexpected weapon block: %s\n", name), 1;
	const char* grenades[] = {"weapon_decoy", "weapon_flashbang", "weapon_hegrenade",
		"weapon_smokegrenade", "weapon_molotov", "weapon_incgrenade", "weapon_tagrenade",
		"weapon_healthshot", "weapon_snowball"};
	for (const char* name : grenades)
		if (!SnowballIsGrenadeWeapon(name))
			return std::fprintf(stderr, "Grenade not recognized: %s\n", name), 1;
	const char* nonGrenades[] = {"weapon_glock", "weapon_knife", "decoy_projectile",
		"smokegrenade_projectile", "", nullptr};
	for (const char* name : nonGrenades)
		if (SnowballIsGrenadeWeapon(name))
			return std::fprintf(stderr, "Unexpected grenade: %s\n", name ? name : "null"), 1;
	return 0;
}
