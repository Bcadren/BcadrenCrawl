/**
 * @file
 * @brief Functions for spattering blood all over the place.
 **/

#ifndef BLOODSPATTER_H
#define BLOODSPATTER_H

void bleed_onto_floor(const coord_def& where, monster_type mon, int damage,
                      bool spatter = false, bool smell_alert = true,
                      const coord_def& from = INVALID_COORD,
                      const bool old_blood = false);
void blood_spray(const coord_def& where, monster_type mon, int level);
void generate_random_blood_spatter_on_level(
                                            const map_bitmask *susceptible_area = NULL);

// Set FPROP_BLOODY after checking bleedability.
bool maybe_bloodify_square(const coord_def& where);

#endif
