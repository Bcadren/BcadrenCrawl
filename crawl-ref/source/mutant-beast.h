/**
 * @file
 * @brief Mutant beast-related enums & values, plus small util functions.
 **/

#ifndef MUTANT_BEAST_H
#define MUTANT_BEAST_H

#define MUTANT_BEAST_TIER "mutant_beast_tier"
#define MUTANT_BEAST_FACETS "mutant_beast_facets"

/// types of mutant beast (MONS_MUTANT_BEAST)
enum beast_facet
{
    BF_NONE,
    BF_STING,
    BF_FIRST = BF_STING,
    BF_BAT,
    BF_FIRE,
    BF_GHOST,
    BF_SHOCK,
    BF_BEAR,
    BF_LAST = BF_BEAR,
    NUM_BEAST_FACETS,
};

/// tiers of mutant beast (correspond to HD)
enum beast_tier
{
    BT_NONE,
    BT_LARVAL,
    BT_FIRST = BT_LARVAL,
    BT_JUVENILE,
    BT_MATURE,
    BT_ELDER,
    BT_PRIMAL,
    NUM_BEAST_TIERS,
};

/// HD of mutant beast tiers
const int beast_tiers[] = { 0, 3, 9, 15, 21, 27, };
COMPILE_CHECK(ARRAYSZ(beast_tiers) == NUM_BEAST_TIERS);

/// names of beast facets
const string mutant_beast_facet_names[] = {
    "buggy", "sting", "bat", "fire", "ghost", "shock", "bear",
};
COMPILE_CHECK(ARRAYSZ(mutant_beast_facet_names) == NUM_BEAST_FACETS);

/// names of beast tiers
const char* const mutant_beast_tier_names[] = {
    "buggy", "larval", "juvenile", "mature", "elder", "primal",
};
COMPILE_CHECK(ARRAYSZ(mutant_beast_tier_names) == NUM_BEAST_TIERS);

#endif
