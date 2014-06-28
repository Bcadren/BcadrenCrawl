/**
 * @file
 * @brief Misc religion related functions.
**/

#ifndef RELIGION_H
#define RELIGION_H

#include "enum.h"
#include "externs.h"
#include "player.h"
#include "mgen_data.h"

#include "religion-enum.h"

#define MAX_PIETY      200
#define HALF_MAX_PIETY (MAX_PIETY / 2)

#define MAX_PENANCE    200

#define NUM_VEHUMET_GIFTS 13

bool is_evil_god(god_type god);
bool is_good_god(god_type god);
bool is_chaotic_god(god_type god);
bool is_unknown_god(god_type god);

// Returns true if the god is not present in the current game. This is
// orthogonal to whether the player can worship the god in question.
bool is_unavailable_god(god_type god);

god_type random_god(bool available = true);

void simple_god_message(const char *event, god_type which_deity = you.religion);
int piety_breakpoint(int i);
string god_name(god_type which_god, bool long_name = false);
string god_name_jiyva(bool second_name = false);
god_type str_to_god(const string &name, bool exact = true);

string get_god_powers(god_type which_god);
string get_god_likes(god_type which_god, bool verbose = false);
string get_god_dislikes(god_type which_god, bool verbose = false);

bool active_penance(god_type god);
void dec_penance(int val);
void dec_penance(god_type god, int val);

void excommunication(god_type new_god = GOD_NO_GOD);

bool gain_piety(int pgn, int denominator = 1, bool should_scale_piety = true);
void dock_piety(int pietyloss, int penance);
void god_speaks(god_type god, const char *mesg);
void lose_piety(int pgn);
void set_piety(int piety);
void handle_god_time(int time_delta);
int god_colour(god_type god);
colour_t god_message_altar_colour(god_type god);
int gozag_service_fee();
bool player_can_join_god(god_type which_god);
void god_pitch(god_type which_god);

static inline bool you_worship(god_type god)
{
    return you.religion == god;
}

int had_gods();
int piety_rank(int piety = -1);
int piety_scale(int piety_change);
bool god_likes_your_god(god_type god, god_type your_god = you.religion);
bool god_hates_your_god(god_type god, god_type your_god = you.religion);
bool god_hates_cannibalism(god_type god);
bool god_hates_killing(god_type god, const monster* mon);
bool god_likes_fresh_corpses(god_type god);
bool god_likes_butchery(god_type god);
bool god_disallows_corpsefood(monster_type mc, god_type god);
bool god_likes_spell(spell_type spell, god_type god);
bool god_hates_spell(spell_type spell, god_type god,
                     bool rod_spell = false);
bool god_loathes_spell(spell_type spell, god_type god);
bool god_hates_ability(ability_type ability, god_type god);
bool god_can_protect_from_harm(god_type god);
int elyvilon_lifesaving();
bool god_protects_from_harm();
bool jiyva_is_dead();
void set_penance_xp_timeout();
bool fedhas_protects(const monster* target);
bool fedhas_neutralises(const monster* target);
void print_sacrifice_message(god_type, const item_def &,
                             piety_gain_t, bool = false);
void nemelex_death_message();

bool tso_unchivalric_attack_safe_monster(const monster* mon);

void mons_make_god_gift(monster* mon, god_type god = you.religion);
bool mons_is_god_gift(const monster* mon, god_type god = you.religion);

int yred_random_servants(unsigned int threshold, bool force_hostile = false);
bool is_yred_undead_slave(const monster* mon);
bool is_orcish_follower(const monster* mon);
bool is_fellow_slime(const monster* mon);
bool is_follower(const monster* mon);

// Vehumet gift interface.
bool vehumet_is_offering(spell_type spell);
void vehumet_accept_gift(spell_type spell);

bool god_hates_attacking_friend(god_type god, const actor *fr);
bool god_likes_item(god_type god, const item_def& item);
bool god_likes_items(god_type god, bool greedy_explore = false);

void religion_turn_start();
void religion_turn_end();

int get_tension(god_type god = you.religion);
int get_monster_tension(const monster* mons, god_type god = you.religion);
int get_fuzzied_monster_difficulty(const monster *mons);

typedef void (*delayed_callback)(const mgen_data &mg, monster *&mon, int placed);

void delayed_monster(const mgen_data &mg, delayed_callback callback = NULL);
void delayed_monster_done(string success, string failure,
                          delayed_callback callback = NULL);

bool do_god_gift(bool forced = false);

vector<god_type> temple_god_list();
vector<god_type> nontemple_god_list();

extern const char* god_gain_power_messages[NUM_GODS][MAX_GOD_ABILITIES];
string adjust_abil_message(const char *pmsg, bool allow_upgrades = true);
#endif
