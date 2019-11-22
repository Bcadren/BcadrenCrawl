/**
 * @file
 * @brief Player related functions.
**/


#pragma once

#include "game-type.h"
#include "god-type.h"
#include "species.h"
#ifdef USE_TILE
#include "tiledoll.h"
#endif

struct player_save_info
{
    string name;
    unsigned int experience;
    int experience_level;
    bool wizard;
    species_type species;
    string species_name;
    string class_name;
    god_type religion;
    string god_name;
    string jiyva_second_name;
    game_type saved_game_type;

#ifdef USE_TILE
    dolls_data doll;
#endif

    bool save_loadable;
    string filename;

    player_save_info& operator=(const player& rhs);
    bool operator<(const player_save_info& rhs) const;
    string short_desc() const;
    string really_short_desc() const;
};
