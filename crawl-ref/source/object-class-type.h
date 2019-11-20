#pragma once

enum object_class_type : uint8_t           // mitm[].base_type
{
    OBJ_WEAPONS,
    OBJ_MISSILES,
    OBJ_ARMOURS,
    OBJ_WANDS,
    OBJ_FOOD,
    OBJ_SCROLLS,
    OBJ_JEWELLERY,
    OBJ_POTIONS,
    OBJ_BOOKS,
    OBJ_STAVES,
    OBJ_ORBS,
    OBJ_MISCELLANY,
    OBJ_CORPSES,
    OBJ_GOLD,
#if TAG_MAJOR_VERSION == 34
    OBJ_RODS,
#endif
    OBJ_RUNES,
    OBJ_SHIELDS,
    NUM_OBJECT_CLASSES,
    OBJ_UNASSIGNED = 100,
    OBJ_RANDOM,      // used for blanket random sub_type .. see dungeon::items()
    OBJ_DETECTED,    // unknown item; item_info only
};
