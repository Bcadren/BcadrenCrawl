/**
 * @file
 * @brief Divine retribution.
**/

#ifndef GODWRATH_H
#define GODWRATH_H

bool divine_retribution(god_type god, bool no_bonus = false, bool force = false);
bool do_god_revenge(conduct_type thing_done);
void ash_reduce_penance(int amount);

void gozag_incite(monster *mon);
#endif
