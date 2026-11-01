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
