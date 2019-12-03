#pragma once

enum monuse_flags
{
    MU_NOTHING       = 0x0000,
    MU_START_ONLY    = 0x0001,
    MU_DOOR          = 0x0002,
    MU_WEAPON_MELEE  = 0x0004,
    MU_WEAPON_RANGED = 0x0008,
    MU_ARMOUR        = 0x0010,
    MU_SHIELD        = 0x0020,
    MU_WAND          = 0x0040,
    MU_JEWELS        = 0x0080,
    MU_CONSUMABLES   = 0x0100, // Scrolls/Potions
    MU_MISC          = 0x0200, // Phials of Floods, etc.

    // unused        = 0x0400,
    // unused        = 0x0800,

    MU_THROW_ROCK    = 0x1000,
    MU_THROW_NET     = 0x2000,
    MU_THROW_JAVELIN = 0x4000,
    MU_THROW_BLOWGUN = 0x8000,

    MU_THROW_MASK    = 0xf000,
    MU_WIELD_MASK    = MU_WEAPON_MELEE | MU_WEAPON_RANGED | MU_ARMOUR | MU_SHIELD 
        | MU_WAND | MU_JEWELS | MU_CONSUMABLES | MU_MISC,
        // Mask to check if it has any equipment slots at all.

    MU_WEAPONS = MU_WEAPON_MELEE | MU_WEAPON_RANGED,
    MU_MELEE   = MU_DOOR | MU_WEAPON_MELEE | MU_ARMOUR | MU_SHIELD | MU_JEWELS,
    MU_EVOKE   = MU_JEWELS | MU_WAND | MU_CONSUMABLES | MU_MISC,
    MU_ALL     = MU_MELEE | MU_WEAPON_RANGED | MU_EVOKE,
};

#define muf(int) (monuse_flags)(int)