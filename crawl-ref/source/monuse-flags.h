#pragma once

enum monuse_flags
{
    MU_NOTHING       = 0x0000,
    MU_START_ONLY    = 0x0001,
    MU_DOOR          = 0x0002,
    MU_WEAPON_MELEE  = 0x0004,
    MU_WEAPON_RANGED = 0x0008,
    MU_THROW         = 0x0010,
    MU_THROW_ALWAYS  = 0x0020,
    MU_ARMOUR        = 0x0040,
    MU_SHIELD        = 0x0080,
    MU_WAND          = 0x0100,
    MU_JEWELS        = 0x0200,
    MU_CONSUMABLES   = 0x0400, // Scrolls/Potions
    MU_MISC          = 0x0800, // Phials of Floods, etc.

    MU_WEAPONS = MU_WEAPON_MELEE | MU_WEAPON_RANGED,
    MU_MELEE   = MU_DOOR | MU_WEAPON_MELEE | MU_ARMOUR | MU_SHIELD | MU_JEWELS,
    MU_ALL     = MU_MELEE | MU_WEAPON_RANGED | MU_WAND | MU_CONSUMABLES | MU_MISC,
};