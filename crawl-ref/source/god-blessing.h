/**
 * @file
 * @brief Functions for gods blessing followers.
**/

#pragma once

#include "mon-ench.h"
#include "player.h"

bool bless_follower(monster* follower = nullptr,
                    god_type god = you.religion,
                    bool force = false);

// BCADNOTE: Weird place for this. Consider moving to a different package.
bool increase_ench_duration(monster* mon, mon_enchant ench, const int increase);
