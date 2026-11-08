#pragma once

#include <cstring>

inline bool SnowballBlocksWeapon(const char* name)
{
	if (!name || std::strncmp(name, "weapon_", 7))
		return false;
	return std::strncmp(name, "weapon_knife", 12) != 0 &&
		std::strcmp(name, "weapon_bayonet") != 0 &&
		std::strcmp(name, "weapon_decoy") != 0;
}

inline bool SnowballIsGrenadeWeapon(const char* name)
{
	if (!name)
		return false;
	return std::strcmp(name, "weapon_decoy") == 0 ||
		std::strcmp(name, "weapon_flashbang") == 0 ||
		std::strcmp(name, "weapon_hegrenade") == 0 ||
		std::strcmp(name, "weapon_smokegrenade") == 0 ||
		std::strcmp(name, "weapon_molotov") == 0 ||
		std::strcmp(name, "weapon_incgrenade") == 0 ||
		std::strcmp(name, "weapon_tagrenade") == 0 ||
		std::strcmp(name, "weapon_healthshot") == 0 ||
		std::strcmp(name, "weapon_snowball") == 0;
}
