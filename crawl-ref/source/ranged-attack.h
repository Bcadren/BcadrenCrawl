#pragma once

#include "attack.h"
#include "food.h"

class ranged_attack : public attack
{
// Public Properties
public:
    int range_used;
    bool reflected;
    int force_range = 0;

// Public Methods
public:
    ranged_attack(actor *attacker, actor *defender, item_def *projectile,
                  bool teleport, actor *blame = 0);

    int calc_to_hit(bool random, bool player_aux = false) override;

    void set_path(bolt path);
    void ricochet();

    // Applies attack damage and other effects.
    bool attack();

private:
    /* Attack Phases */
    bool handle_phase_attempted() override;
    bool handle_phase_blocked() override;
    bool handle_phase_dodged() override;
    bool handle_phase_hit() override;
    bool handle_phase_end() override;
    bool ignores_shield(bool verbose) override;

    bolt the_path;
    string blocker = "";

    /* Combat Calculations */
    bool using_weapon() const override;
    int weapon_damage() override;
    int calc_base_unarmed_damage() override;
    int calc_mon_to_hit_base(bool random) override;
    int apply_damage_modifiers(int damage) override;
    bool apply_damage_brand(const char *what = nullptr) override;
    special_missile_type random_chaos_missile_brand();
    bool blowgun_check(special_missile_type type);
    int blowgun_duration_roll(special_missile_type type);
    bool apply_missile_brand();

    int player_apply_misc_modifiers(int damage) override;

    /* Weapon Effects */
    bool check_unrand_effects() override;

    int attack_count;

    /* Attack Effects */
    bool mons_attack_effects() override;
    void player_stab_check() override;
    bool player_good_stab() override;

    /* Output */
    void set_attack_verb(int damage) override;
    void announce_hit() override;

private:
    const item_def *projectile;
    bool teleport;
    int orig_to_hit;
    bool should_alert_defender;
    launch_retval launch_type;
};
