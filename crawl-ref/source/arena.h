/**
 * @file
 * @brief Functions related to the monster arena (stage and watch fights).
**/

#pragma once

#include "enum.h"

class level_id;
class monster;
struct mgen_data;

struct coord_def;

struct newgame_def;

NORETURN void run_arena(const newgame_def& choice, const string &default_arena_teams);

monster_type arena_pick_random_monster(const level_id &place);

bool arena_veto_random_monster(monster_type type);

bool arena_veto_place_monster(const mgen_data &mg, bool first_band_member,
                              const coord_def& pos);

void arena_placed_monster(monster* mons);

void arena_split_monster(monster* split_from, monster* split_to);

void arena_monster_died(monster* mons, killer_type killer,
                        int killer_index, bool silent, const item_def* corpse);

int arena_cull_items();
