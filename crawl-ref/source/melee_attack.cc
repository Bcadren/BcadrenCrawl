/**
 * @file
 * @brief melee_attack class and associated melee_attack methods
 */

#include "AppHdr.h"

#include "melee_attack.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <algorithm>

#include "externs.h"

#include "areas.h"
#include "art-enum.h"
#include "attitude-change.h"
#include "beam.h"
#include "bloodspatter.h"
#include "cloud.h"
#include "coordit.h"
#include "database.h"
#include "delay.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "fineff.h"
#include "fight.h"
#include "fprop.h"
#include "food.h"
#include "goditem.h"
#include "invent.h"
#include "items.h"
#include "itemname.h"
#include "itemprop.h"
#include "item_use.h"
#include "libutil.h"
#include "makeitem.h"
#include "message.h"
#include "misc.h"
#include "mon-abil.h"
#include "mon-behv.h"
#include "mon-cast.h"
#include "mon-clone.h"
#include "mon-death.h"
#include "mon-place.h"
#include "mon-poly.h"
#include "terrain.h"
#include "mgen_data.h"
#include "mon-util.h"
#include "mutation.h"
#include "options.h"
#include "ouch.h"
#include "player.h"
#include "random-var.h"
#include "religion.h"
#include "godconduct.h"
#include "shopping.h"
#include "skills.h"
#include "species.h"
#include "spl-clouds.h"
#include "spl-miscast.h"
#include "spl-summoning.h"
#include "spl-util.h"
#include "state.h"
#include "strings.h"
#include "target.h"
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "hints.h"
#include "unwind.h"
#include "view.h"
#include "shout.h"
#include "xom.h"

#ifdef NOTE_DEBUG_CHAOS_BRAND
    #define NOTE_DEBUG_CHAOS_EFFECTS
#endif

#ifdef NOTE_DEBUG_CHAOS_EFFECTS
    #include "notes.h"
#endif

// Odd helper function, why is this declared like this?
#define DID_AFFECT() \
{ \
    if (miscast_level == 0) \
        miscast_level = -1; \
    return; \
}

/*
 **************************************************
 *             BEGIN PUBLIC FUNCTIONS             *
 **************************************************
*/
melee_attack::melee_attack(actor *attk, actor *defn,
                           int attack_num, int effective_attack_num,
                           bool is_cleaving, bool is_jump_attack,
                           bool is_jump_blocked, coord_def attack_pos)
    :  // Call attack's constructor
    ::attack(attk, defn),

    attack_number(attack_num), effective_attack_number(effective_attack_num),
    cleaving(is_cleaving), jumping_attack(is_jump_attack),
    jump_blocked(is_jump_blocked)
{
    attack_occurred = false;
    init_attack(SK_UNARMED_COMBAT, attack_number);
    if (weapon && !using_weapon())
        wpn_skill = SK_FIGHTING;

    can_cleave = !jumping_attack && wpn_skill == SK_AXES && attacker != defender
        && !attacker->confused();

    if (jumping_attack)
        attack_position = attack_pos;
    else
        attack_position = attacker->pos();
}

bool melee_attack::can_reach()
{
    return (attk_type == AT_HIT && weapon && weapon_reach(*weapon))
           || attk_flavour == AF_REACH
           || attk_type == AT_REACH_STING;
}

bool melee_attack::handle_phase_attempted()
{
    // Skip invalid and dummy attacks.
    if (defender && (!adjacent(attack_position, defender->pos())
                     && !jumping_attack && !can_reach())
        || attk_type == AT_CONSTRICT
           && (!attacker->can_constrict(defender)
               || attacker->is_monster() && attacker->mid == MID_PLAYER))
    {
        --effective_attack_number;

        return false;
    }

    if (attacker->is_player() && defender && defender->is_monster())
    {
        // These checks are handled in fight_jump() for jump attacks
        if (!jumping_attack && weapon
            && is_unrandom_artefact(*weapon, UNRAND_DEVASTATOR))
        {
            const char* verb = "attack";
            string junk1, junk2;
            bool junk3 = false;
            if (defender)
            {
                verb = (bad_attack(defender->as_monster(),
                                   junk1, junk2, junk3)
                        ? "attack" : "attack near");
            }

            targetter_smite hitfunc(attacker, 1, 1, 1, false);
            hitfunc.set_aim(defender->pos());

            if (stop_attack_prompt(hitfunc, verb))
            {
                cancel_attack = true;
                return false;
            }
        }
        else if (can_cleave)
        {
            targetter_cleave hitfunc(attacker, defender->pos());
            if (stop_attack_prompt(hitfunc, "attack"))
            {
                cancel_attack = true;
                return false;
            }
        }
        // Jump attack did this check in jump_fight()
        else if (!jumping_attack
                 && stop_attack_prompt(defender->as_monster(), false,
                                       attack_position))
        {
            cancel_attack = true;
            return false;
        }
    }

    if (attacker->is_player())
    {
        // Handle jump_attack movement.  If the player is jump-attacking a
        // square with no visible target, defender may be empty, but
        // path-blocking events are handled first.
        if (jumping_attack)
        {
            if (defender && you.can_see(defender))
                mprf("You jump-attack %s!", defender->name(DESC_THE).c_str());
            else
                mpr("You jump-attack!");
            if (jump_blocked)
            {
                mpr("Something unseen blocks your movement!");
                return false;
            }
            else if (!defender)
            {
                mpr("There is nothing there, so you fail to move!");
                return false;
            }
            move_player_to_grid(attack_position, false);
        }

        // Set delay now that we know the attack won't be cancelled.
        you.time_taken = you.attack_delay(weapon);
        if (weapon)
        {
            if (weapon->base_type == OBJ_WEAPONS)
                if (is_unrandom_artefact(*weapon)
                    && get_unrand_entry(weapon->special)->type_name)
                {
                    count_action(CACT_MELEE, weapon->special);
                }
                else
                    count_action(CACT_MELEE, weapon->sub_type);
            else if (weapon->base_type == OBJ_RODS)
                count_action(CACT_MELEE, WPN_ROD);
            else if (weapon->base_type == OBJ_STAVES)
                count_action(CACT_MELEE, WPN_STAFF);
        }
        else
            count_action(CACT_MELEE, -1);
    }
    else
    {
        if (jumping_attack)
        {
            mprf("%s jump-attacks %s!", attacker->name(DESC_THE).c_str(),
                 defender->name(DESC_THE).c_str());
            if (jump_blocked)
            {
                mprf("%s is blocked by something %s can't see!",
                     attacker->name(DESC_THE).c_str(),
                     attacker->pronoun(PRONOUN_SUBJECTIVE).c_str());
                return false;
            }
        }

        // Only the first attack costs any energy.
        if (!effective_attack_number)
        {
            int energy = attacker->as_monster()->action_energy(EUT_ATTACK);
            int delay = attacker->attack_delay(weapon);
            dprf(DIAG_COMBAT, "Attack delay %d, multiplier %1.1f", delay, energy * 0.1);
            ASSERT(energy > 0);
            ASSERT(delay > 0);

            attacker->as_monster()->speed_increment
                -= div_rand_round(energy * delay, 10);
        }

        // Statues and other special monsters which have AT_NONE need to lose
        // energy, but otherwise should exit the melee attack now.
        if (attk_type == AT_NONE)
            return false;
    }

    if (attacker != defender)
    {
        // Allow setting of your allies' target, etc.
        attacker->attacking(defender);

        check_autoberserk();
    }

    // The attacker loses nutrition.
    attacker->make_hungry(3, true);

    // Xom thinks fumbles are funny...
    if (!jumping_attack && attacker->fumbles_attack())
    {
        // ... and thinks fumbling when trying to hit yourself is just
        // hilarious.
        xom_is_stimulated(attacker == defender ? 200 : 10);

        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        return false;
    }
    // Non-fumbled self-attacks due to confusion are still pretty funny, though.
    else if (attacker == defender && attacker->confused())
    {
        // And is still hilarious if it's the player.
        xom_is_stimulated(attacker->is_player() ? 200 : 100);
    }

    // Any attack against a monster we're afraid of has a chance to fail
    if (attacker->is_player() && you.afraid_of(defender->as_monster())
        && one_chance_in(3))
    {
        mprf("You attempt to attack %s, but flinch away in fear!",
             defender->name(DESC_THE).c_str());
        return false;
    }

    if (attk_flavour == AF_SHADOWSTAB && defender && !defender->can_see(attacker))
    {
        if (you.see_cell(attack_position))
        {
            mprf("%s strikes at %s from the darkness!",
                 attacker->name(DESC_THE, true).c_str(),
                 defender->name(DESC_THE).c_str());
        }
        to_hit = AUTOMATIC_HIT;
        needs_message = false;
    }
    else if (attacker->is_monster()
             && attacker->type == MONS_DROWNED_SOUL)
    {
        to_hit = AUTOMATIC_HIT;
    }

    attack_occurred = true;

    // Check for player practicing dodging
    if (one_chance_in(3) && defender->is_player())
        practise(EX_MONSTER_MAY_HIT);

    return true;
}

bool melee_attack::handle_phase_dodged()
{
    did_hit = false;

    const int ev = defender->melee_evasion(attacker);
    const int ev_nophase = defender->melee_evasion(attacker, EV_IGNORE_PHASESHIFT);

    if (ev_margin + (ev - ev_nophase) > 0)
    {
        if (needs_message && defender_visible)
        {
            mprf("%s momentarily %s out as %s "
                 "attack passes through %s%s",
                 defender->name(DESC_THE).c_str(),
                 defender->conj_verb("phase").c_str(),
                 atk_name(DESC_ITS).c_str(),
                 defender->pronoun(PRONOUN_OBJECTIVE).c_str(),
                 attack_strength_punctuation(damage_done).c_str());
        }
    }
    else
    {
        if (needs_message)
        {
            // TODO: Unify these, placed player_warn_miss here so I can remove
            // player_attack
            if (attacker->is_player())
                player_warn_miss();
            else
            {
                mprf("%s%s misses %s%s",
                     atk_name(DESC_THE).c_str(),
                     evasion_margin_adverb().c_str(),
                     defender_name(true).c_str(),
                     attack_strength_punctuation(damage_done).c_str());
            }
        }
    }

    if (attacker != defender && adjacent(defender->pos(), attack_position))
    {
        if (attacker->alive()
            && (defender->is_player() ?
                   you.species == SP_MINOTAUR :
                   mons_base_type(defender->as_monster()) == MONS_MINOTAUR)
            && defender->can_see(attacker)
            // Retaliation only works on the first attack in a round.
            // FIXME: player's attack is -1, even for auxes
            && effective_attack_number <= 0)
        {
            do_minotaur_retaliation();
        }

        // Retaliations can kill!
        if (!attacker->alive())
            return false;
    }

    return true;
}

static bool _flavour_triggers_damageless(attack_flavour flavour)
{
    return flavour == AF_CRUSH
           || flavour == AF_ENGULF
           || flavour == AF_PURE_FIRE
           || flavour == AF_SHADOWSTAB
           || flavour == AF_DROWN;
}

void melee_attack::apply_black_mark_effects()
{
    // Slightly weaker and less reliable effects for players.
    if (attacker->is_player()
        && you.mutation[MUT_BLACK_MARK]
        && one_chance_in(5))
    {
        if (you.hp < you.hp_max
            && !you.duration[DUR_DEATHS_DOOR]
            && !defender->as_monster()->is_summoned())
        {
            mpr("You feel better.");
            attacker->heal(random2(damage_done));
        }

        if (!defender->alive())
            return;

        switch (random2(3))
        {
            case 0:
                antimagic_affects_defender(damage_done * 8);
                break;
            case 1:
                defender->weaken(attacker, 2);
                break;
            case 2:
                defender->drain_exp(attacker);
                break;
        }
    }
    else if (attacker->is_monster()
             && attacker->as_monster()->has_ench(ENCH_BLACK_MARK))
    {
        monster* mon = attacker->as_monster();

        if (mon->heal(random2avg(damage_done, 2)))
            simple_monster_message(mon, " is healed.");

        if (!defender->alive())
            return;

        switch (random2(3))
        {
            case 0:
                antimagic_affects_defender(damage_done * 8);
                break;
            case 1:
                defender->slow_down(attacker, 5 + random2(7));
                break;
            case 2:
                defender->drain_exp(attacker, false, 10);
                break;
        }
    }
}

/* An attack has been determined to have hit something
 *
 * Handles to-hit effects for both attackers and defenders,
 * Determines damage and passes off execution to handle_phase_damaged
 * Also applies weapon brands
 *
 * Returns true if combat should continue, false if it should end here.
 */
bool melee_attack::handle_phase_hit()
{
    did_hit = true;
    perceived_attack = true;
    bool hit_woke_orc = false;

    if (attacker->is_player())
    {
        if (crawl_state.game_is_hints())
            Hints.hints_melee_counter++;

        // TODO: Remove this (placed here so I can get rid of player_attack)
        if (you_worship(GOD_BEOGH)
            && mons_genus(defender->mons_species()) == MONS_ORC
            && !defender->is_summoned()
            && !defender->as_monster()->is_shapeshifter()
            && !player_under_penance() && you.piety >= piety_breakpoint(2)
            && mons_near(defender->as_monster()) && defender->asleep())
        {
            hit_woke_orc = true;
        }
    }

    // Slimify does no damage and serves as an on-hit effect, handle it
    if (attacker->is_player() && you.duration[DUR_SLIMIFY]
        && mon_can_be_slimified(defender->as_monster())
        && !cleaving)
    {
        // Bail out after sliming so we don't get aux unarmed and
        // attack a fellow slime.
        damage_done = 0;
        slimify_monster(defender->as_monster());
        you.duration[DUR_SLIMIFY] = 0;

        return false;
    }

    // This does more than just calculate the damage, it also sets up
    // messages, etc.
    damage_done = calc_damage();

    if (attacker->is_player() && you.duration[DUR_INFUSION])
    {
        if (enough_mp(1, true, false))
        {
            // infusion_power is set when the infusion spell is cast
            const int pow = you.props["infusion_power"].get_int();
            const int dmg = 2 + div_rand_round(pow, 25);
            const int hurt = defender->apply_ac(dmg);

            dprf(DIAG_COMBAT, "Infusion: dmg = %d hurt = %d", dmg, hurt);

            if (hurt > 0)
            {
                damage_done += hurt;
                dec_mp(1);
            }
        }
    }

    bool stop_hit = false;
    // Check if some hit-effect killed the monster.
    if (attacker->is_player())
        stop_hit = !player_monattk_hit_effects();

    // check_unrand_effects is safe to call with a dead defender, so always
    // call it, even if the hit effects said to stop.
    if (stop_hit)
    {
        check_unrand_effects();
        return false;
    }

    if (damage_done > 0 || _flavour_triggers_damageless(attk_flavour))
    {
        if (!handle_phase_damaged())
            return false;

        // TODO: Remove this, (placed here to remove player_attack)
        if (attacker->is_player() && hit_woke_orc)
        {
            // Call function of orcs first noticing you, but with
            // beaten-up conversion messages (if applicable).
            beogh_follower_convert(defender->as_monster(), true);
        }
    }
    else if (needs_message)
    {
        attack_verb = attacker->is_player()
                      ? attack_verb
                      : attacker->conj_verb(mons_attack_verb());

        // TODO: Clean this up if possible, checking atype for do / does is ugly
        mprf("%s %s %s but %s no damage.",
             attacker->name(DESC_THE).c_str(),
             attack_verb.c_str(),
             defender_name(true).c_str(),
             attacker->is_player() ? "do" : "does");
    }

    // Check for weapon brand & inflict that damage too
    apply_damage_brand();

    if (check_unrand_effects())
        return false;

    if (damage_done > 0)
        apply_black_mark_effects();

    if (attacker->is_player())
    {
        // Always upset monster regardless of damage.
        // However, successful stabs inhibit shouting.
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def(), !stab_attempt);

        // [ds] Monster may disappear after behaviour event.
        if (!defender->alive())
            return true;
    }
    else if (defender->is_player())
    {
        // These effects (mutations right now) are only triggered when
        // the player is hit, each of them will verify their own required
        // parameters.
        do_passive_freeze();
#if TAG_MAJOR_VERSION == 34
        do_passive_heat();
#endif
        emit_foul_stench();
    }

    return true;
}

bool melee_attack::handle_phase_damaged()
{
    bool shroud_broken = false;

    // TODO: Move this somewhere else, this is a terrible place for a
    // block-like (prevents all damage) effect.
    if (attacker != defender
        && (defender->is_player() && you.duration[DUR_SHROUD_OF_GOLUBRIA]
            || defender->is_monster()
               && defender->as_monster()->has_ench(ENCH_SHROUD))
        && !one_chance_in(3))
    {
        // Chance of the shroud falling apart increases based on the
        // strain of it, i.e. the damage it is redirecting.
        if (x_chance_in_y(damage_done, 10+damage_done))
        {
            // Delay the message for the shroud breaking until after
            // the attack message.
            shroud_broken = true;
            if (defender->is_player())
                you.duration[DUR_SHROUD_OF_GOLUBRIA] = 0;
            else
                defender->as_monster()->del_ench(ENCH_SHROUD);
        }
        else
        {
            if (needs_message)
            {
                mprf("%s shroud bends %s attack away%s",
                     def_name(DESC_ITS).c_str(),
                     atk_name(DESC_ITS).c_str(),
                     attack_strength_punctuation(damage_done).c_str());
            }
            did_hit = false;
            damage_done = 0;

            return false;
        }
    }

    if (!attack::handle_phase_damaged())
        return false;

    if (shroud_broken && needs_message)
    {
        mprf(defender->is_player() ? MSGCH_WARN : MSGCH_PLAIN,
             "%s shroud falls apart!",
             def_name(DESC_ITS).c_str());
    }

    return true;
}

bool melee_attack::handle_phase_aux()
{
    if (attacker->is_player())
    {
        // returns whether an aux attack successfully took place
        // additional attacks from cleave don't get aux
        if (!defender->as_monster()->friendly()
            && adjacent(defender->pos(), attack_position)
            && !cleaving)
        {
            player_aux_unarmed();
        }

        print_wounds(defender->as_monster());
    }

    return true;
}

bool melee_attack::handle_phase_end()
{
    if (!cleave_targets.empty())
    {
        attack_cleave_targets(attacker, cleave_targets, attack_number,
                              effective_attack_number);
    }

    // Check for passive mutation effects.
    if (defender->is_player() && defender->alive() && attacker != defender)
    {
        mons_do_eyeball_confusion();
        tendril_disarm();
    }

    return attack::handle_phase_end();
}

/* Initiate the processing of the attack
 *
 * Called from the main code (fight.cc), this method begins the actual combat
 * for a particular attack and is responsible for looping through each of the
 * appropriate phases (which then can call other related phases within
 * themselves).
 *
 * Returns whether combat was completely successful
 *      If combat was not successful, it could be any number of reasons, like
 *      the defender or attacker dying during the attack? or a defender moving
 *      from its starting position.
 */
bool melee_attack::attack()
{
    if (!cleaving && !handle_phase_attempted())
        return false;

    if (attacker != defender && attacker->self_destructs())
        return did_hit = perceived_attack = true;

    if (can_cleave && !cleaving)
        cleave_setup();

    // Attacker might have died from effects of cleaving handled prior to this
    if (!attacker->alive())
        return false;

    // We might have killed the kraken target by cleaving a tentacle.
    if (!defender->alive())
    {
        handle_phase_killed();
        handle_phase_end();
        return attack_occurred;
    }

    // Apparently I'm insane for believing that we can still stay general past
    // this point in the combat code, mebe I am! --Cryptic

    // Calculate various ev values and begin to check them to determine the
    // correct handle_phase_ handler.
    const int ev = defender->melee_evasion(attacker);
    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked(true);

    // Stuff for god conduct, this has to remain here for scope reasons.
    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->is_player() && attacker != defender)
    {
        set_attack_conducts(conducts, defender->as_monster());

        if (player_under_penance(GOD_ELYVILON)
            && god_hates_your_god(GOD_ELYVILON)
            && ev_margin >= 0
            && one_chance_in(20))
        {
            simple_god_message(" blocks your attack.", GOD_ELYVILON);
            handle_phase_end();
            return false;
        }
        // Check for stab (and set stab_attempt and stab_bonus)
        player_stab_check();
        // Make sure we hit if we passed the stab check.
        if (stab_attempt && stab_bonus > 0)
        {
            ev_margin = AUTOMATIC_HIT;
            shield_blocked = false;
        }
    }

    if (shield_blocked)
        handle_phase_blocked();
    else
    {
        if (attacker != defender && adjacent(defender->pos(), attack_position))
        {
            // Check for defender Spines
            do_spines();

            // Spines can kill!
            if (!attacker->alive())
                return false;
        }

        if (ev_margin >= 0)
        {
            if (attacker != defender && attack_warded_off())
            {
                perceived_attack = true;
                handle_phase_end();
                return false;
            }

            bool cont = handle_phase_hit();

            attacker_sustain_passive_damage();

            if (!cont)
            {
                if (!defender->alive())
                    handle_phase_killed();
                handle_phase_end();
                return false;
            }
        }
        else
            handle_phase_dodged();
    }

    // Remove sanctuary if - through some attack - it was violated.
    if (env.sanctuary_time > 0 && attack_occurred && !cancel_attack
        && attacker != defender && !attacker->confused()
        && (is_sanctuary(attack_position) || is_sanctuary(defender->pos()))
        && (attacker->is_player() || attacker->as_monster()->friendly()))
    {
        remove_sanctuary(true);
    }

    if (attacker->is_player())
    {
        if (damage_brand == SPWPN_CHAOS)
            chaos_affects_attacker();

        do_miscast();
    }

    adjust_noise();
    // don't crash on banishment
    if (!defender->pos().origin())
        handle_noise(defender->pos());

    // Noisy weapons.
    if (attacker->is_player()
        && weapon
        && is_artefact(*weapon)
        && artefact_wpn_property(*weapon, ARTP_NOISES))
    {
        noisy_equipment();
    }

    alert_defender();

    if (!defender->alive())
        handle_phase_killed();

    handle_phase_aux();

    handle_phase_end();

    enable_attack_conducts(conducts);

    return attack_occurred;
}

/* Initialises the noise_factor
 *
 * For both players and monsters, separately sets the noise_factor for weapon
 * attacks based on damage type or attack_type/attack_flavour respectively.
 * Sets an average noise value for unarmed combat regardless of atype().
 */
// TODO: Unify attack_type and unarmed_attack_type, the former includes the latter
// however formerly, attack_type was mons_attack_type and used exclusively for monster use.
void melee_attack::adjust_noise()
{
    if (attacker->is_player() && weapon != NULL)
    {
        switch (single_damage_type(*weapon))
        {
        case DAM_BLUDGEON:
        case DAM_WHIP:
            noise_factor = 125;
            break;
        case DAM_SLICE:
            noise_factor = 100;
            break;
        case DAM_PIERCE:
            noise_factor = 75;
            break;
        }
    }
    else if (attacker->is_monster() && weapon == NULL)
    {
        switch (attk_type)
        {
        case AT_HEADBUTT:
        case AT_TENTACLE_SLAP:
        case AT_TAIL_SLAP:
        case AT_TRAMPLE:
        case AT_TRUNK_SLAP:
            noise_factor = 150;
            break;

        case AT_HIT:
        case AT_PUNCH:
        case AT_KICK:
        case AT_CLAW:
        case AT_GORE:
#if TAG_MAJOR_VERSION == 34
        case AT_SNAP:
        case AT_SPLASH:
#endif
        case AT_CHERUB:
            noise_factor = 125;
            break;

        case AT_BITE:
        case AT_PECK:
        case AT_CONSTRICT:
        case AT_POUNCE:
            noise_factor = 100;
            break;

        case AT_STING:
        case AT_SPORE:
        case AT_ENGULF:
        case AT_REACH_STING:
            noise_factor = 75;
            break;

        case AT_TOUCH:
            noise_factor = 0;
            break;

        case AT_WEAP_ONLY:
            break;

        // To prevent compiler warnings.
        case AT_NONE:
        case AT_RANDOM:
#if TAG_MAJOR_VERSION == 34
        case AT_SHOOT:
#endif
            die("Invalid attack type for noise_factor");
            break;

        default:
            die("%d Unhandled attack type for noise_factor", attk_type);
            break;
        }

        if (attk_flavour == AF_ELEC)
            noise_factor += 100;
    }
    else if (weapon == NULL)
        noise_factor = 150;
}

void melee_attack::check_autoberserk()
{
    if (attacker->is_player())
    {
        for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
        {
            const item_def *item = you.slot_item(static_cast<equipment_type>(i));
            if (!item)
                continue;

            if (!is_artefact(*item))
                continue;

            if (x_chance_in_y(artefact_wpn_property(*item, ARTP_ANGRY), 100))
            {
                attacker->go_berserk(false);
                return;
            }
        }
    }
    else
    {
        for (int i = MSLOT_WEAPON; i <= MSLOT_JEWELLERY; ++i)
        {
            const item_def *item =
                attacker->as_monster()->mslot_item(static_cast<mon_inv_type>(i));
            if (!item)
                continue;

            if (!is_artefact(*item))
                continue;

            if (x_chance_in_y(artefact_wpn_property(*item, ARTP_ANGRY), 100))
            {
                attacker->go_berserk(false);
                return;
            }
        }
    }
}

bool melee_attack::check_unrand_effects()
{
    if (unrand_entry && unrand_entry->melee_effects && weapon)
    {
        // Recent merge added damage_done to this method call
        unrand_entry->melee_effects(weapon, attacker, defender,
                                    !defender->alive(), damage_done);
        return !defender->alive();
    }

    return false;
}

/* Setup all unarmed (non attack_type) variables
 *
 * Clears any previous unarmed attack information and sets everything from
 * noise_factor to verb and damage. Called after player_aux_choose_uc_attack
 */
void melee_attack::player_aux_setup(unarmed_attack_type atk)
{
    noise_factor = 100;
    aux_attack.clear();
    aux_verb.clear();
    damage_brand = SPWPN_NORMAL;
    aux_damage = 0;
    int str_bite_damage = 0;

    switch (atk)
    {
    case UNAT_CONSTRICT:
        aux_attack = aux_verb = "grab";
        aux_damage = 0;
        noise_factor = 10; // extremely quiet?
        break;

    case UNAT_KICK:
        aux_attack = aux_verb = "kick";
        aux_damage = 5;

        if (you.has_usable_hooves())
        {
            // Max hoof damage: 10.
            aux_damage += player_mutation_level(MUT_HOOVES) * 5 / 3;
        }
        else if (you.has_usable_talons())
        {
            aux_verb = "claw";

            // Max talon damage: 9.
            aux_damage += 1 + player_mutation_level(MUT_TALONS);
        }
        else if (player_mutation_level(MUT_TENTACLE_SPIKE))
        {
            aux_attack = "tentacle spike";
            aux_verb = "pierce";

            // Max spike damage: 8.
            aux_damage += player_mutation_level(MUT_TENTACLE_SPIKE);
        }

        break;

    case UNAT_PECK:
        aux_damage = 6;
        noise_factor = 75;
        aux_attack = aux_verb = "peck";
        break;

    case UNAT_HEADBUTT:
        aux_damage = 5 + player_mutation_level(MUT_HORNS) * 3;
        aux_attack = aux_verb = "headbutt";
        break;

    case UNAT_TAILSLAP:
        aux_attack = aux_verb = "tail-slap";

        aux_damage = 6 * you.has_usable_tail();

        noise_factor = 125;

        if (player_mutation_level(MUT_STINGER) > 0)
        {
            aux_damage += player_mutation_level(MUT_STINGER) * 2 - 1;
            damage_brand = SPWPN_VENOM;
        }

        break;

    case UNAT_PUNCH:
        aux_attack = aux_verb = "punch";
        aux_damage = 5 + you.skill_rdiv(SK_UNARMED_COMBAT, 1, 2);

        if (you.form == TRAN_BLADE_HANDS)
        {
            aux_verb = "slash";
            aux_damage += 6;
            noise_factor = 75;
        }
        else if (you.has_usable_claws())
        {
            aux_verb = "claw";
            aux_damage += roll_dice(you.has_claws(), 3);
        }
        else if (you.has_usable_tentacles())
        {
            aux_attack = aux_verb = "tentacle-slap";
            noise_factor = 125;
        }

        break;

    case UNAT_BITE:
        aux_attack = aux_verb = "bite";
        aux_damage += you.has_usable_fangs() * 2;
        str_bite_damage += div_rand_round(max(you.strength()-10, 0), 5);
        aux_damage += str_bite_damage;
        noise_factor = 75;

        // prob of vampiric bite:
        // 1/4 when non-thirsty, 1/2 when thirsty, 100% when
        // bloodless
        if (_vamp_wants_blood_from_monster(defender->as_monster())
            && (you.hunger_state == HS_STARVING
                || you.hunger_state < HS_SATIATED && coinflip()
                || you.hunger_state >= HS_SATIATED && one_chance_in(4)))
        {
            damage_brand = SPWPN_VAMPIRISM;
        }

        if (player_mutation_level(MUT_ANTIMAGIC_BITE))
        {
            //Change formula to fangs_level*2 + 2*XL/3
            aux_damage -= str_bite_damage;
            aux_damage += div_rand_round(2 * you.get_hit_dice(), 3);
            damage_brand = SPWPN_ANTIMAGIC;
        }

        if (player_mutation_level(MUT_ACIDIC_BITE))
        {
            damage_brand = SPWPN_ACID;
            aux_damage += roll_dice(2,4);
        }

        break;

    case UNAT_PSEUDOPODS:
        aux_attack = aux_verb = "bludgeon";
        aux_damage += 4 * you.has_usable_pseudopods();
        noise_factor = 125;
        break;

        // Tentacles give you both a main attack (replacing punch)
        // and this secondary, high damage attack.
    case UNAT_TENTACLES:
        aux_attack = aux_verb = "squeeze";
        aux_damage += you.has_usable_tentacles() ? 12 : 0;
        noise_factor = 100; // quieter than slapping
        break;

    default:
        die("unknown aux attack type");
        break;
    }
}

/* Selects the unarmed attack type given by Unarmed Combat skill
 *
 * Currently the only possibility is an offhand punch. Other auxes are linked
 * directly to mutations and just depend on stats.
 */
unarmed_attack_type melee_attack::player_aux_choose_uc_attack()
{
    unarmed_attack_type uc_attack = coinflip() ? UNAT_PUNCH : UNAT_NO_ATTACK;
    // Octopodes get more tentacle-slaps.
    if (you.species == SP_OCTOPODE && coinflip())
        uc_attack = UNAT_PUNCH;
    // No punching with a shield or 2-handed wpn.
    // Octopodes aren't affected by this, though!
    if (you.species != SP_OCTOPODE && uc_attack == UNAT_PUNCH
        && !you.has_usable_offhand())
    {
        uc_attack = UNAT_NO_ATTACK;
    }

    if (_tran_forbid_aux_attack(uc_attack))
        uc_attack = UNAT_NO_ATTACK;

    return uc_attack;
}

bool melee_attack::player_aux_test_hit()
{
    // XXX We're clobbering did_hit
    did_hit = false;

    const int evasion = defender->melee_evasion(attacker);

    if (player_under_penance(GOD_ELYVILON)
        && god_hates_your_god(GOD_ELYVILON)
        && to_hit >= evasion
        && one_chance_in(20))
    {
        simple_god_message(" blocks your attack.", GOD_ELYVILON);
        return false;
    }

    bool auto_hit = one_chance_in(30);

    if (to_hit >= evasion || auto_hit)
        return true;

    const int phaseless_evasion =
        defender->melee_evasion(attacker, EV_IGNORE_PHASESHIFT);

    if (to_hit >= phaseless_evasion && defender_visible)
    {
        mprf("Your %s passes through %s as %s momentarily phases out.",
            aux_attack.c_str(),
            defender->name(DESC_THE).c_str(),
            defender->pronoun(PRONOUN_SUBJECTIVE).c_str());
    }
    else
    {
        mprf("Your %s misses %s.", aux_attack.c_str(),
             defender->name(DESC_THE).c_str());
    }

    return false;
}

/* Controls the looping on available unarmed attacks
 *
 * As the master method for unarmed player combat, this loops through
 * available unarmed attacks, determining whether they hit and - if so -
 * calculating and applying their damage.
 *
 * Returns (defender dead)
 */
bool melee_attack::player_aux_unarmed()
{
    unwind_var<brand_type> save_brand(damage_brand);

    /*
     * uc_attack is the auxiliary unarmed attack the player gets
     * for unarmed combat skill, which doesn't require a mutation to use
     * but still needs to pass the other checks in _extra_aux_attack().
     */
    unarmed_attack_type uc_attack = UNAT_NO_ATTACK;
    // Unarmed skill gives a chance at getting an aux even without the
    // corresponding mutation.
    if (attacker->fights_well_unarmed(attacker_armour_tohit_penalty
                                   + attacker_shield_tohit_penalty))
    {
        uc_attack = player_aux_choose_uc_attack();
    }

    for (int i = UNAT_FIRST_ATTACK; i <= UNAT_LAST_ATTACK; ++i)
    {
        if (!defender->alive())
            break;

        unarmed_attack_type atk = static_cast<unarmed_attack_type>(i);

        if (!_extra_aux_attack(atk, (uc_attack == atk)))
            continue;

        // Determine and set damage and attack words.
        player_aux_setup(atk);

        if (atk == UNAT_CONSTRICT && !attacker->can_constrict(defender))
            continue;

        to_hit = random2(calc_your_to_hit_unarmed(atk,
                         damage_brand == SPWPN_VAMPIRISM));

        handle_noise(defender->pos());
        alert_nearby_monsters();

        // [ds] kraken can flee when near death, causing the tentacle
        // the player was beating up to "die" and no longer be
        // available to answer questions beyond this point.
        // handle_noise stirs up all nearby monsters with a stick, so
        // the player may be beating up a tentacle, but the main body
        // of the kraken still gets a chance to act and submerge
        // tentacles before we get here.
        if (!defender->alive())
            return true;

        if (player_aux_test_hit())
        {
            // Upset the monster.
            behaviour_event(defender->as_monster(), ME_WHACK, attacker);
            if (!defender->alive())
                return true;

            if (attack_shield_blocked(true))
                continue;
            if (player_aux_apply(atk))
                return true;
        }
    }

    return false;
}

bool melee_attack::player_aux_apply(unarmed_attack_type atk)
{
    did_hit = true;

    aux_damage  = player_aux_stat_modify_damage(aux_damage);

    aux_damage  = random2(aux_damage);

    aux_damage  = player_apply_fighting_skill(aux_damage, true);

    aux_damage  = player_apply_misc_modifiers(aux_damage);

    aux_damage  = player_apply_slaying_bonuses(aux_damage, true);

    aux_damage  = player_apply_final_multipliers(aux_damage);

    const int pre_ac_dmg = aux_damage;
    const int post_ac_dmg = apply_defender_ac(aux_damage);

    if (atk == UNAT_CONSTRICT)
        aux_damage = 0;
    else
        aux_damage = post_ac_dmg;

    aux_damage = inflict_damage(aux_damage, BEAM_MISSILE);
    damage_done = aux_damage;

    switch (atk)
    {
        case UNAT_PUNCH:
            apply_bleeding = true;
            break;

        case UNAT_HEADBUTT:
        {
            const int horns = player_mutation_level(MUT_HORNS);
            const int stun = bestroll(min(damage_done, 7), 1 + horns);

            defender->as_monster()->speed_increment -= stun;
            break;
        }

        case UNAT_KICK:
        {
            if (you.has_usable_hooves() && pre_ac_dmg > post_ac_dmg)
            {
                const int hooves = player_mutation_level(MUT_HOOVES);
                const int dmg = bestroll(pre_ac_dmg - post_ac_dmg, hooves);
                // do some of the previously ignored damage in extra-damage
                damage_done += inflict_damage(dmg, BEAM_MISSILE);
            }

            break;
        }

        case UNAT_CONSTRICT:
            attacker->start_constricting(*defender);
            break;

        default:
            break;
    }

    if (damage_done > 0 || atk == UNAT_CONSTRICT)
    {
        player_announce_aux_hit();

        if (damage_brand == SPWPN_ACID)
        {
            mprf("%s is splashed with acid.",
                 defender->name(DESC_THE).c_str());
            defender->as_monster()->splash_with_acid(&you);
        }

        // TODO: remove this? Unarmed poison attacks?
        if (damage_brand == SPWPN_VENOM && coinflip())
            poison_monster(defender->as_monster(), &you);


        // Normal vampiric biting attack, not if already got stabbing special.
        if (damage_brand == SPWPN_VAMPIRISM && you.species == SP_VAMPIRE
            && (!stab_attempt || stab_bonus <= 0))
        {
            _player_vampire_draws_blood(defender->as_monster(), damage_done);
        }

        if (damage_brand == SPWPN_ANTIMAGIC && you.mutation[MUT_ANTIMAGIC_BITE]
            && damage_done > 0)
        {
            const bool spell_user = mons_antimagic_affected(defender->as_monster());

            antimagic_affects_defender(damage_done * 16);
            mprf("You drain %s %s.",
                 defender->as_monster()->pronoun(PRONOUN_POSSESSIVE).c_str(),
                 spell_user ? "magic" : "power");

            if (you.magic_points != you.max_magic_points
                && !defender->as_monster()->is_summoned()
                && !mons_is_firewood(defender->as_monster()))
            {
                int drain = random2(damage_done) + 1;
                //Augment mana drain--1.25 "standard" effectiveness at 0 mp,
                //.25 at mana == max_mana
                drain = (int)((1.25 - you.magic_points / you.max_magic_points)
                              * drain);
                if (drain)
                {
                    mpr("You feel invigorated.");
                    inc_mp(drain);
                }
            }
        }
    }
    else // no damage was done
    {
        mprf("You %s %s%s.",
             aux_verb.c_str(),
             defender->name(DESC_THE).c_str(),
             you.can_see(defender) ? ", but do no damage" : "");
    }

    if (defender->as_monster()->hit_points < 1)
    {
        handle_phase_killed();
        return true;
    }

    return false;
}

void melee_attack::player_announce_aux_hit()
{
    mprf("You %s %s%s%s",
         aux_verb.c_str(),
         defender->name(DESC_THE).c_str(),
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}

string melee_attack::player_why_missed()
{
    const int ev = defender->melee_evasion(attacker);
    const int combined_penalty =
        attacker_armour_tohit_penalty + attacker_shield_tohit_penalty;
    if (to_hit < ev && to_hit + combined_penalty >= ev)
    {
        const bool armour_miss =
            (attacker_armour_tohit_penalty
             && to_hit + attacker_armour_tohit_penalty >= ev);
        const bool shield_miss =
            (attacker_shield_tohit_penalty
             && to_hit + attacker_shield_tohit_penalty >= ev);

        const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
        const string armour_name = armour ? armour->name(DESC_BASENAME)
                                          : string("armour");

        if (armour_miss && !shield_miss)
            return "Your " + armour_name + " prevents you from hitting ";
        else if (shield_miss && !armour_miss)
            return "Your shield prevents you from hitting ";
        else
            return "Your shield and " + armour_name
                   + " prevent you from hitting ";
    }

    return "You" + evasion_margin_adverb() + " miss ";
}

void melee_attack::player_warn_miss()
{
    did_hit = false;

    mprf("%s%s.",
         player_why_missed().c_str(),
         defender->name(DESC_THE).c_str());

    // Upset only non-sleeping non-fleeing monsters if we missed.
    if (!defender->asleep() && !mons_is_fleeing(defender->as_monster()))
        behaviour_event(defender->as_monster(), ME_WHACK, attacker);
}

int melee_attack::player_aux_stat_modify_damage(int damage)
{
    int dammod = 20;
    // Use the same str/dex weighting that unarmed combat does, for now.
    const int dam_stat_val = (7 * you.strength() + 3 * you.dex())/10;

    if (dam_stat_val > 10)
        dammod += random2(dam_stat_val - 9);
    else if (dam_stat_val < 10)
        dammod -= random2(11 - dam_stat_val);

    damage *= dammod;
    damage /= 20;

    return damage;
}

// A couple additive modifiers that should be applied to both unarmed and
// armed attacks.
int melee_attack::player_apply_misc_modifiers(int damage)
{
    if (you.duration[DUR_MIGHT] || you.duration[DUR_BERSERK])
        damage += 1 + random2(10);

    if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        damage -= random2(5);

    return damage;
}

// Multipliers to be applied to the final (pre-stab, pre-AC) damage.
// It might be tempting to try to pick and choose what pieces of the damage
// get affected by such multipliers, but putting them at the end is the
// simplest effect to understand if they aren't just going to be applied
// to the base damage of the weapon.
int melee_attack::player_apply_final_multipliers(int damage)
{
    //cleave damage modifier
    if (cleaving)
        damage = cleave_damage_mod(damage);

    // not additive, statues are supposed to be bad with tiny toothpicks but
    // deal crushing blows with big weapons
    if (you.form == TRAN_STATUE)
        damage = div_rand_round(damage * 3, 2);

    // Can't affect much of anything as a shadow.
    if (you.form == TRAN_SHADOW)
        damage = div_rand_round(damage, 2);

    if (you.duration[DUR_WEAK])
        damage = div_rand_round(damage * 3, 4);

    if (you.duration[DUR_CONFUSING_TOUCH] && wpn_skill == SK_UNARMED_COMBAT)
        return 0;

    return damage;
}

void melee_attack::set_attack_verb()
{
    if (!attacker->is_player())
        return;

    int weap_type = WPN_UNKNOWN;

    int damage_to_display = damage_done;
    if (Options.lang == LANG_GRUNT)
        damage_to_display = HIT_STRONG + 1;

    if (!weapon)
        weap_type = WPN_UNARMED;
    else if (weapon->base_type == OBJ_STAVES)
        weap_type = WPN_STAFF;
    else if (weapon->base_type == OBJ_RODS)
        weap_type = WPN_ROD;
    else if (weapon->base_type == OBJ_WEAPONS
             && !is_range_weapon(*weapon))
    {
        weap_type = weapon->sub_type;
    }

    // All weak hits with weapons look the same.
    if (damage_to_display < HIT_WEAK
        && weap_type != WPN_UNARMED)
    {
        if (weap_type != WPN_UNKNOWN)
            attack_verb = "hit";
        else
            attack_verb = "clumsily bash";
        return;
    }

    // Take normal hits into account.  If the hit is from a weapon with
    // more than one damage type, randomly choose one damage type from
    // it.
    monster_type defender_genus = mons_genus(defender->type);
    switch (weapon ? single_damage_type(*weapon) : -1)
    {
    case DAM_PIERCE:
        if (damage_to_display < HIT_MED)
            attack_verb = "puncture";
        else if (damage_to_display < HIT_STRONG)
            attack_verb = "impale";
        else
        {
            if (defender->is_monster()
                && defender_visible
                && defender_genus == MONS_HOG)
            {
                attack_verb = "spit";
                verb_degree = "like the proverbial pig";
            }
            else if (defender_genus == MONS_CRAB && Options.lang == LANG_GRUNT)
            {
                attack_verb = "attack";
                verb_degree = "'s weak point";
            }
            else
            {
                const char* pierce_desc[][2] = {{"spit", "like a pig"},
                                                {"skewer", "like a kebab"},
                                                {"stick", "like a pincushion"},
                                                {"perforate", "like a sieve"}};
                const int choice = random2(ARRAYSZ(pierce_desc));
                attack_verb = pierce_desc[choice][0];
                verb_degree = pierce_desc[choice][1];
            }
        }
        break;

    case DAM_SLICE:
        if (damage_to_display < HIT_MED)
            attack_verb = "slash";
        else if (damage_to_display < HIT_STRONG)
            attack_verb = "slice";
        else if (defender_genus == MONS_OGRE)
        {
            attack_verb = "dice";
            verb_degree = "like an onion";
        }
        else if (defender_genus == MONS_SKELETON)
        {
            attack_verb = "fracture";
            verb_degree = "into splinters";
        }
        else if (defender_genus == MONS_HOG)
        {
            attack_verb = "carve";
            verb_degree = "like the proverbial ham";
        }
        else if ((defender_genus == MONS_YAK || defender_genus == MONS_YAKTAUR)
                 && Options.lang == LANG_GRUNT)
            attack_verb = "shave";
        else
        {
            const char* pierce_desc[][2] = {{"open",    "like a pillowcase"},
                                            {"slice",   "like a ripe choko"},
                                            {"cut",     "into ribbons"},
                                            {"carve",   "like a ham"},
                                            {"chop",    "into pieces"}};
            const int choice = random2(ARRAYSZ(pierce_desc));
            attack_verb = pierce_desc[choice][0];
            verb_degree = pierce_desc[choice][1];
        }
        break;

    case DAM_BLUDGEON:
        if (damage_to_display < HIT_MED)
            attack_verb = one_chance_in(4) ? "thump" : "sock";
        else if (damage_to_display < HIT_STRONG)
            attack_verb = "bludgeon";
        else if (defender_genus == MONS_SKELETON)
        {
            attack_verb = "shatter";
            verb_degree = "into splinters";
        }
        else
        {
            const char* pierce_desc[][2] = {{"crush",   "like a grape"},
                                            {"beat",    "like a drum"},
                                            {"hammer",  "like a gong"},
                                            {"pound",   "like an anvil"},
                                            {"flatten", "like a pancake"}};
            const int choice = random2(ARRAYSZ(pierce_desc));
            attack_verb = pierce_desc[choice][0];
            verb_degree = pierce_desc[choice][1];
        }
        break;

    case DAM_WHIP:
        if (damage_to_display < HIT_MED)
            attack_verb = "whack";
        else if (damage_to_display < HIT_STRONG)
            attack_verb = "thrash";
        else
        {
            switch (defender->holiness())
            {
            case MH_HOLY:
            case MH_NATURAL:
            case MH_DEMONIC:
                attack_verb = "punish";
                verb_degree = ", causing immense pain";
                break;
            default:
                attack_verb = "devastate";
            }
        }
        break;

    case -1: // unarmed
        switch (you.form)
        {
        case TRAN_SPIDER:
        case TRAN_BAT:
        case TRAN_PIG:
        case TRAN_PORCUPINE:
            if (damage_to_display < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_to_display < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = "maul";
            break;
        case TRAN_BLADE_HANDS:
            if (damage_to_display < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_to_display < HIT_MED)
                attack_verb = "slash";
            else if (damage_to_display < HIT_STRONG)
                attack_verb = "slice";
            else
                attack_verb = "shred";
            break;
        case TRAN_TREE:
            if (damage_to_display < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_to_display < HIT_MED)
                attack_verb = "smack";
            else if (damage_to_display < HIT_STRONG)
                attack_verb = "pummel";
            else
                attack_verb = "thrash";
            break;
        case TRAN_DRAGON:
            if (damage_to_display < HIT_WEAK)
                attack_verb = "hit";
            else if (damage_to_display < HIT_MED)
                attack_verb = "claw";
            else if (damage_to_display < HIT_STRONG)
                attack_verb = "bite";
            else
                attack_verb = "maul";
            break;
        case TRAN_WISP:
            if (damage_to_display < HIT_WEAK)
                attack_verb = "touch";
            else if (damage_to_display < HIT_MED)
                attack_verb = "hit";
            else
                attack_verb = "engulf";
            break;
        case TRAN_FUNGUS:
            attack_verb = "release spores at";
            break;
        case TRAN_STATUE:
        case TRAN_LICH:
        case TRAN_NONE:
        case TRAN_APPENDAGE:
        case TRAN_ICE_BEAST:
        case TRAN_SHADOW:
#if TAG_MAJOR_VERSION == 34
        case TRAN_JELLY: // ?
#endif
            if (you.damage_type() == DVORP_CLAWING)
            {
                if (damage_to_display < HIT_WEAK)
                    attack_verb = "scratch";
                else if (damage_to_display < HIT_MED)
                    attack_verb = "claw";
                else if (damage_to_display < HIT_STRONG)
                    attack_verb = "mangle";
                else
                    attack_verb = "eviscerate";
            }
            else if (you.damage_type() == DVORP_TENTACLE)
            {
                if (damage_to_display < HIT_WEAK)
                    attack_verb = "tentacle-slap";
                else if (damage_to_display < HIT_MED)
                    attack_verb = "bludgeon";
                else if (damage_to_display < HIT_STRONG)
                    attack_verb = "batter";
                else
                    attack_verb = "thrash";
            }
            else
            {
                if (damage_to_display < HIT_WEAK)
                    attack_verb = "hit";
                else if (damage_to_display < HIT_MED)
                    attack_verb = "punch";
                else if (damage_to_display < HIT_STRONG)
                    attack_verb = "pummel";
                else if (defender->is_monster()
                         && (mons_genus(defender->type) == MONS_WORKER_ANT
                             || mons_genus(defender->type) == MONS_FORMICID))
                {
                    attack_verb = "squash";
                    verb_degree = "like the proverbial ant";
                }
                else
                {
                    const char* punch_desc[][2] =
                        {{"pound",     "into fine dust"},
                         {"pummel",    "like a punching bag"},
                         {"pulverise", ""},
                         {"squash",    "like an ant"}};
                    const int choice = random2(ARRAYSZ(punch_desc));
                    // XXX: could this distinction work better?
                    if (choice == 0
                        && defender->is_monster()
                        && mons_has_blood(defender->type))
                    {
                        attack_verb = "beat";
                        verb_degree = "into a bloody pulp";
                    }
                    else
                    {
                        attack_verb = punch_desc[choice][0];
                        verb_degree = punch_desc[choice][1];
                    }
                }
            }
            break;
        } // transformations
        break;

    case WPN_UNKNOWN:
    default:
        attack_verb = "hit";
        break;
    }
}

void melee_attack::player_exercise_combat_skills()
{
    if (defender->cannot_fight())
        return;

    int damage = 10; // Default for unarmed.
    if (weapon && is_weapon(*weapon) && !is_range_weapon(*weapon))
        damage = property(*weapon, PWPN_DAMAGE);

    // Slow down the practice of low-damage weapons.
    if (x_chance_in_y(damage, 20))
        practise(EX_WILL_HIT, wpn_skill);
}

/*
 * Applies god conduct for weapon ego
 *
 * Using speed brand as a chei worshipper, or holy/unholy weapons
 */
void melee_attack::player_weapon_upsets_god()
{
    if (weapon && weapon->base_type == OBJ_WEAPONS)
    {
        if (is_holy_item(*weapon))
            did_god_conduct(DID_HOLY, 1);
        else if (is_demonic(*weapon))
            did_god_conduct(DID_UNHOLY, 1);
        else if (get_weapon_brand(*weapon) == SPWPN_SPEED
                || weapon->sub_type == WPN_QUICK_BLADE)
        {
            did_god_conduct(DID_HASTY, 1);
        }
    }
    else if (weapon
             && weapon->base_type == OBJ_STAVES
             && weapon->sub_type == STAFF_FIRE)
    {
        did_god_conduct(DID_FIRE, 1);
    }
}

/* Apply player-specific effects as well as brand damage.
 *
 * Called after damage is calculated, but before unrand effects and before
 * damage is dealt.
 *
 * Returns true if combat should continue, false if it should end here.
 */
bool melee_attack::player_monattk_hit_effects()
{
    player_weapon_upsets_god();

    // Thirsty vampires will try to use a stabbing situation to draw blood.
    if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED
        && damage_done > 0 && stab_attempt && stab_bonus > 0
        && _player_vampire_draws_blood(defender->as_monster(),
                                       damage_done, true))
    {
        // No further effects.
    }
    else if (you.species == SP_VAMPIRE
             && damage_brand == SPWPN_VAMPIRISM
             && you.weapon()
             && _player_vampire_draws_blood(defender->as_monster(),
                                            damage_done, false, 5))
    {
        // No further effects.
    }

    if (!defender->alive())
        return false;

    // These effects apply only to monsters that are still alive:

    // Returns true if a head was cut off *and* the wound was cauterized,
    // in which case the cauterization was the ego effect, so don't burn
    // the hydra some more.
    //
    // Also returns true if the hydra's last head was cut off, in which
    // case nothing more should be done to the hydra.
    if (decapitate_hydra(damage_done))
        return defender->alive();

    // Mutually exclusive with (overrides) brand damage!
    special_damage = 0;
    apply_staff_damage();

    if (!defender->alive())
        return false;

    if (special_damage || special_damage_flavour)
    {
        dprf(DIAG_COMBAT, "Special damage to %s: %d, flavour: %d",
             defender->name(DESC_THE).c_str(),
             special_damage, special_damage_flavour);

        special_damage = inflict_damage(special_damage);
        if (special_damage > 0)
            defender->expose_to_element(special_damage_flavour, 2);
    }

    if (stab_attempt && stab_bonus > 0 && weapon
        && weapon->base_type == OBJ_WEAPONS && weapon->sub_type == WPN_CLUB
        && damage_done + special_damage > random2(defender->get_hit_dice())
        && defender->alive()
        && !defender->as_monster()->has_ench(ENCH_CONFUSION)
        && mons_class_is_confusable(defender->type))
    {
        if (!defender->as_monster()->check_clarity(false) &&
            defender->as_monster()->add_ench(mon_enchant(ENCH_CONFUSION, 0,
            &you, 20+random2(30)))) // 1-3 turns
        {
            mprf("%s is stunned!", defender->name(DESC_THE).c_str());
        }
    }

    return true;
}

void melee_attack::rot_defender(int amount, int immediate)
{
    if (defender->rot(attacker, amount, immediate, true))
    {
        // XXX: why is this message separate here?
        if (defender->is_player())
        {
            special_damage_message =
                make_stringf("You feel your flesh %s away!",
                             immediate > 0 ? "rotting" : "start to rot");
        }
        else if (defender->is_monster() && defender_visible)
        {
            special_damage_message =
                make_stringf(
                    "%s %s!",
                    defender_name(false).c_str(),
                    amount > 0 ? "rots" : "looks less resilient");
        }
    }
}

/**
 * Attempt to move a set of stairs from one place to another.
 *
 * @param orig      The current location of the stiars.
 * @param dest      The desired destination.
 * @return          Whether the stairs were moved.
 */
static bool _move_stairs(coord_def orig, coord_def dest)
{
    const dungeon_feature_type stair_feat = grd(orig);

    if (feat_stair_direction(stair_feat) == CMD_NO_CMD)
        return false;

    // The player can't use shops to escape, so don't bother.
    if (stair_feat == DNGN_ENTER_SHOP)
        return false;

    // Don't move around notable terrain the player is aware of if it's
    // out of sight.
    if (is_notable_terrain(stair_feat)
        && env.map_knowledge(orig).known() && !you.see_cell(orig))
    {
        return false;
    }

    return slide_feature_over(orig, dest);
}

void melee_attack::chaos_affects_attacker()
{
    if (miscast_level >= 1 || !attacker->alive())
        return;

    coord_def dest(-1, -1);
    // Prefer to send it under the defender.
    if (defender->alive() && defender->pos() != attack_position)
        dest = defender->pos();

    // Move stairs out from under the attacker.
    if (one_chance_in(100) && _move_stairs(attack_position, dest))
    {
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: move stairs"), true);
#endif
        DID_AFFECT();
    }

    // Create a colourful cloud.
    if (weapon && one_chance_in(1000))
    {
        mprf("Smoke pours forth from %s!", wep_name(DESC_YOUR).c_str());
        big_cloud(random_smoke_type(), &you, attack_position, 20,
                  4 + random2(8));
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
        take_note(Note(NOTE_MESSAGE, 0, 0,
                       "CHAOS affects attacker: smoke"), true);
#endif
        DID_AFFECT();
    }

    // Make a loud noise.
    if (weapon && player_can_hear(attack_position)
        && one_chance_in(200))
    {
        string msg = "";
        if (!you.can_see(attacker))
        {
            string noise = getSpeakString("weapon_noise");
            if (!noise.empty())
                msg = "You hear " + maybe_pick_random_substring(noise);
        }
        else
        {
            msg = getSpeakString("weapon_noises");
            string wepname = wep_name(DESC_YOUR);
            if (!msg.empty())
            {
                msg = maybe_pick_random_substring(msg);
                msg = replace_all(msg, "@Your_weapon@", wepname);
                msg = replace_all(msg, "@The_weapon@", wepname);
            }
        }

        if (!msg.empty())
        {
            mprf(MSGCH_SOUND, "%s", msg.c_str());
            noisy(15, attack_position, attacker->mindex());
#ifdef NOTE_DEBUG_CHAOS_EFFECTS
            take_note(Note(NOTE_MESSAGE, 0, 0,
                           "CHAOS affects attacker: noise"), true);
#endif
            DID_AFFECT();
        }
    }
}

// XXX:
//  * Noise should probably scale non-linearly with damage_done, and
//    maybe even non-linearly with noise_factor.
//
//  * Damage reduction via armour of the defender reduces noise,
//    but shouldn't.
//
//  * Damage reduction because of negative damage modifiers on the
//    weapon reduce noise, but probably shouldn't.
//
//  * Might want a different formula for noise generated by the
//    player.
//
//  Ideas:
//  * Each weapon type has a noise rating, like it does an accuracy
//    rating and base delay.
//
//  * For player, stealth skill and/or weapon skill reducing noise.
//
//  * Randart property to make randart weapons louder or softer when
//    they hit.
void melee_attack::handle_noise(const coord_def & pos)
{
    // Successful stabs make no noise.
    if (stab_attempt)
    {
        noise_factor = 0;
        return;
    }

    int level = noise_factor * damage_done / 100 / 4;

    if (noise_factor > 0)
        level = max(1, level);

    // Cap melee noise at shouting volume.
    level = min(12, level);

    if (level > 0)
        noisy(level, pos, attacker->mindex());

    noise_factor = 0;
}

// Returns true if the attack cut off a head *and* cauterized it.
bool melee_attack::chop_hydra_head(int dam,
                                   int dam_type,
                                   brand_type wpn_brand)
{
    // Monster attackers have only a 25% chance of making the
    // chop-check to prevent runaway head inflation.
    // XXX: Tentatively making an exception for spectral weapons
    if (attacker->is_monster() && attacker->type!= MONS_SPECTRAL_WEAPON
        && !one_chance_in(4))
    {
        return false;
    }

    // Only cutting implements.
    if (dam_type != DVORP_SLICING && dam_type != DVORP_CHOPPING
        && dam_type != DVORP_CLAWING || dam <= 0)
    {
        return false;
    }

    if (dam < 4 && wpn_brand != SPWPN_VORPAL && coinflip())
        return false;

    // Small claws are not big enough.
    if (dam_type == DVORP_CLAWING && attacker->has_claws() < 3)
        return false;

    const char *verb = NULL;

    if (dam_type == DVORP_CLAWING)
    {
        static const char *claw_verbs[] = { "rip", "tear", "claw" };
        verb = RANDOM_ELEMENT(claw_verbs);
    }
    else
    {
        static const char *slice_verbs[] =
        {
            "slice", "lop", "chop", "hack"
        };
        verb = RANDOM_ELEMENT(slice_verbs);
    }

    if (defender->as_monster()->number == 1) // will be zero afterwards
    {
        if (defender_visible)
        {
            mprf("%s %s %s last head off!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb(verb).c_str(),
                 apostrophise(defender_name(true)).c_str());
        }
        defender->as_monster()->number--;

        if (!defender->is_summoned())
        {
            bleed_onto_floor(defender->pos(), defender->type,
                             defender->as_monster()->hit_points, true);
        }

        defender->hurt(attacker, INSTANT_DEATH);

        return true;
    }
    else
    {
        if (defender_visible)
        {
            mprf("%s %s one of %s heads off!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb(verb).c_str(),
                 apostrophise(defender_name(true)).c_str());
        }
        defender->as_monster()->number--;

        // Only living hydras get to regenerate heads.
        if (defender->holiness() == MH_NATURAL)
        {
            unsigned int limit = 20;
            if (defender->type == MONS_LERNAEAN_HYDRA)
                limit = 27;

            if (wpn_brand == SPWPN_FLAMING)
            {
                if (defender_visible)
                    mpr("The flame cauterises the wound!");
                return true;
            }
            else if (defender->as_monster()->number < limit - 1)
            {
                simple_monster_message(defender->as_monster(),
                                       " grows two more!");
                defender->as_monster()->number += 2;
                defender->heal(8 + random2(8), true);
            }
        }
    }

    return false;
}

bool melee_attack::decapitate_hydra(int dam, int damage_type)
{
    if (defender->is_monster()
        && defender->as_monster()->has_hydra_multi_attack()
        && defender->type != MONS_SPECTRAL_THING)
    {
        const int dam_type = (damage_type != -1) ? damage_type
                                                 : attacker->damage_type();
        const brand_type wpn_brand = attacker->damage_brand();

        return chop_hydra_head(dam, dam_type, wpn_brand);
    }
    return false;
}

/**
 * Apply passive retaliation damage from hitting acid monsters.
 */
void melee_attack::attacker_sustain_passive_damage()
{
    // If the defender has been cleaned up, it's too late for anything.
    if (defender->type == MONS_PROGRAM_BUG)
        return;

    if (!mons_class_flag(defender->type, M_ACID_SPLASH))
        return;

    const int rA = attacker->res_acid();
    if (rA >= 3)
        return;

    const int acid_strength = resist_adjust_damage(attacker, BEAM_ACID, rA, 5);

    const item_def *weap = weapon ? weapon : attacker->slot_item(EQ_GLOVES);

    // Spectral weapons can't be corroded (but can take acid damage).
    const bool avatar = attacker->is_monster()
                        && mons_is_avatar(attacker->as_monster()->type);

    if (weap && !avatar)
    {
        if (x_chance_in_y(acid_strength + 1, 30))
            corrode_actor(attacker);
    }
    else if (attacker->is_player())
    {
        mprf("Your %s burn!", you.hand_name(true).c_str());
        ouch(roll_dice(1, acid_strength), defender->mindex(), KILLED_BY_ACID);
    }
    else
    {
        simple_monster_message(attacker->as_monster(), " is burned by acid!");
        attacker->hurt(defender, roll_dice(1, acid_strength), BEAM_ACID,
                       false);
    }
}

int melee_attack::staff_damage(skill_type skill)
{
    if (x_chance_in_y(attacker->skill(SK_EVOCATIONS, 200)
                    + attacker->skill(skill, 100), 3000))
    {
        return random2((attacker->skill(skill, 100)
                      + attacker->skill(SK_EVOCATIONS, 50)) / 80);
    }
    return 0;
}

void melee_attack::apply_staff_damage()
{
    if (!weapon)
        return;

    if (weapon->base_type == OBJ_RODS && weapon->sub_type == ROD_STRIKING)
    {
        if (weapon->plus < ROD_CHARGE_MULT)
            return;

        weapon->plus -= ROD_CHARGE_MULT;
        if (attacker->is_player())
            you.wield_change = true;

        special_damage = 1 + random2(attacker->skill(SK_EVOCATIONS, 150)) / 100;
        special_damage = apply_defender_ac(special_damage);
        return;
    }

    if (weapon->base_type != OBJ_STAVES)
        return;

    switch (weapon->sub_type)
    {
    case STAFF_AIR:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 defender->res_elec(),
                                 staff_damage(SK_AIR_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf("%s %s electrocuted!",
                             defender->name(DESC_THE).c_str(),
                             defender->is_player() ? "are" : "is");
            special_damage_flavour = BEAM_ELECTRICITY;
        }

        break;

    case STAFF_COLD:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 staff_damage(SK_ICE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s freeze%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str());
            special_damage_flavour = BEAM_COLD;
        }
        break;

    case STAFF_EARTH:
        special_damage = staff_damage(SK_EARTH_MAGIC);
        special_damage = apply_defender_ac(special_damage);

        if (special_damage > 0)
        {
            special_damage_message =
                make_stringf(
                    "%s crush%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "es",
                    defender->name(DESC_THE).c_str());
        }
        break;

    case STAFF_FIRE:
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 staff_damage(SK_FIRE_MAGIC));

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s burn%s %s!",
                    attacker->name(DESC_THE).c_str(),
                    attacker->is_player() ? "" : "s",
                    defender->name(DESC_THE).c_str());
            special_damage_flavour = BEAM_FIRE;
        }
        break;

    case STAFF_POISON:
    {
        if (random2(300) >= attacker->skill(SK_EVOCATIONS, 20) + attacker->skill(SK_POISON_MAGIC, 10))
            return;

        // Base chance at 50% -- like mundane weapons.
        if (coinflip() || x_chance_in_y(attacker->skill(SK_POISON_MAGIC, 10), 80))
        {
            defender->poison(attacker, 2, defender->has_lifeforce()
                & x_chance_in_y(attacker->skill(SK_POISON_MAGIC, 10), 160));
        }
        break;
    }

    case STAFF_DEATH:
        if (defender->res_negative_energy())
            break;

        special_damage = staff_damage(SK_NECROMANCY);

        if (special_damage)
        {
            special_damage_message =
                make_stringf(
                    "%s convulse%s in agony!",
                    defender->name(DESC_THE).c_str(),
                    defender->is_player() ? "" : "s");

            attacker->god_conduct(DID_NECROMANCY, 4);
        }
        break;

    case STAFF_SUMMONING:
        if (!defender->is_summoned())
            break;

        if (x_chance_in_y(attacker->skill(SK_EVOCATIONS, 20)
                        + attacker->skill(SK_SUMMONINGS, 10), 300))
        {
            cast_abjuration((attacker->skill(SK_SUMMONINGS, 100)
                            + attacker->skill(SK_EVOCATIONS, 50)) / 80,
                            defender->pos());
        }
        break;

    case STAFF_POWER:
    case STAFF_CONJURATION:
#if TAG_MAJOR_VERSION == 34
    case STAFF_ENCHANTMENT:
#endif
    case STAFF_ENERGY:
    case STAFF_WIZARDRY:
        break;

    default:
        die("Invalid staff type: %d", weapon->sub_type);
    }
}

/**
 * Calculate the to-hit for an attacker
 *
 * @param random If false, calculate average to-hit deterministically.
 */
int melee_attack::calc_to_hit(bool random)
{
    int mhit = attack::calc_to_hit(random);

    if (attacker->is_player())
    {
        // other stuff
        if (!weapon)
        {
            if (you.duration[DUR_CONFUSING_TOUCH])
            {
                // Just trying to touch is easier that trying to damage.
                mhit += maybe_random2(you.dex(), random);
            }

            // TODO: Review this later (transformations getting extra hit
            // almost across the board seems bad) - Cryp71c
            switch (you.form)
            {
            case TRAN_SPIDER:
            case TRAN_ICE_BEAST:
            case TRAN_DRAGON:
            case TRAN_LICH:
            case TRAN_FUNGUS:
            case TRAN_TREE:
            case TRAN_WISP:
                mhit += maybe_random2(10, random);
                break;
            case TRAN_BAT:
            case TRAN_BLADE_HANDS:
                mhit += maybe_random2(12, random);
                break;
            case TRAN_STATUE:
                mhit += maybe_random2(9, random);
                break;
            case TRAN_PORCUPINE:
            case TRAN_PIG:
            case TRAN_APPENDAGE:
#if TAG_MAJOR_VERSION == 34
            case TRAN_JELLY:
#endif
            case TRAN_SHADOW:
            case TRAN_NONE:
                break;
            }
        }
    }

    return mhit;
}

void melee_attack::player_stab_check()
{
    if (jumping_attack)
    {
        stab_attempt = false;
        stab_bonus = 0;
        return;
    }

    attack::player_stab_check();
}

/**
 * How good of a stab can we get with this weapon?
 */
int melee_attack::player_stab_tier()
{
    if (wpn_skill == SK_SHORT_BLADES
        || player_equip_unrand(UNRAND_BOOTS_ASSASSIN)
           && (!weapon || is_melee_weapon(*weapon)))
    {
        return 2;
    }
    if (wpn_skill == SK_LONG_BLADES
        || weapon && weapon->base_type == OBJ_WEAPONS
             && (weapon->sub_type == WPN_CLUB
                 || weapon->sub_type == WPN_SPEAR
                 || weapon->sub_type == WPN_TRIDENT
                 || weapon->sub_type == WPN_DEMON_TRIDENT
                 || weapon->sub_type == WPN_TRISHULA)
        || !weapon && you.species == SP_FELID)
    {
        return 1;
    }
    return 0;
}

bool melee_attack::attack_warded_off()
{
    const int WARDING_CHECK = 60;

    if (defender->warding()
        && attacker->is_summoned()
        && attacker->as_monster()->check_res_magic(WARDING_CHECK) <= 0)
    {
        if (needs_message)
        {
            mprf("%s tries to attack %s, but flinches away.",
                 atk_name(DESC_THE).c_str(),
                 defender_name(true).c_str());
        }
        return true;
    }

    return false;
}

bool melee_attack::attack_ignores_shield(bool verbose)
{
    if (attacker->is_monster() && attacker->type == MONS_PHANTASMAL_WARRIOR)
    {
        if (needs_message && verbose)
        {
            mprf("%s blade passes through %s shield.",
                atk_name(DESC_ITS).c_str(),
                def_name(DESC_ITS).c_str());
            return true;
        }
    }

    return false;
}

/* Select the attack verb for attacker
 *
 * If klown, select randomly from klown_attack, otherwise check for any special
 * case attack verbs (tentacles or door/fountain-mimics) and if all else fails,
 * select an attack verb from attack_types based on the ENUM value of attk_type.
 *
 * Returns (attack_verb)
 */
string melee_attack::mons_attack_verb()
{
    static const char *klown_attack[] =
    {
        "hit",
        "poke",
        "prod",
        "flog",
        "pound",
        "slap",
        "tickle",
        "defenestrate",
        "sucker-punch",
        "elbow",
        "pinch",
        "strangle-hug",
        "squeeze",
        "tease",
        "eye-gouge",
        "karate-kick",
        "headlock",
        "wrestle",
        "trip-wire",
        "kneecap"
    };

    if (attacker->type == MONS_KILLER_KLOWN && attk_type == AT_HIT)
        return RANDOM_ELEMENT(klown_attack);

    if (attk_type == AT_TENTACLE_SLAP
        && (attacker->type == MONS_KRAKEN_TENTACLE
            || attacker->type == MONS_ELDRITCH_TENTACLE
            || attacker->type == MONS_MNOLEG_TENTACLE))
    {
        return "slap";
    }

    static const char *attack_types[] =
    {
        "",
        "hit",         // including weapon attacks
        "bite",
        "sting",

        // spore
        "release spores at",

        "touch",
        "engulf",
        "claw",
        "peck",
        "headbutt",
        "punch",
        "kick",
        "tentacle-slap",
        "tail-slap",
        "gore",
        "constrict",
        "trample",
        "trunk-slap",
        "snap closed at",
        "splash",
        "pounce on",
        "sting",
    };

    ASSERT(attk_type < (int)ARRAYSZ(attack_types));
    return attack_types[attk_type];
}

string melee_attack::mons_attack_desc()
{
    if (!you.can_see(attacker))
        return "";

    string ret;
    int dist = (attack_position - defender->pos()).abs();
    if (dist > 2)
    {
        ASSERT(can_reach());
        ret = " from afar";
    }

    if (weapon && attacker->type != MONS_DANCING_WEAPON && attacker->type != MONS_SPECTRAL_WEAPON)
        ret += " with " + weapon->name(DESC_A);

    return ret;
}

void melee_attack::announce_hit()
{
    if (!needs_message || attk_flavour == AF_CRUSH)
        return;

    if (attacker->is_monster())
    {
        mprf("%s %s %s%s%s%s",
             atk_name(DESC_THE).c_str(),
             attacker->conj_verb(mons_attack_verb()).c_str(),
             defender_name(true).c_str(),
             debug_damage_number().c_str(),
             mons_attack_desc().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }
    else
    {
        if (!verb_degree.empty() && verb_degree[0] != ' '
            && verb_degree[0] != ',' && verb_degree[0] != '\'')
        {
            verb_degree = " " + verb_degree;
        }

        mprf("You %s %s%s%s%s",
             attack_verb.c_str(),
             defender->name(DESC_THE).c_str(),
             verb_degree.c_str(), debug_damage_number().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }
}

// Returns if the target was actually poisoned by this attack
bool melee_attack::mons_do_poison()
{
    int amount = 1;
    bool force = false;

    if (attk_flavour == AF_POISON_STRONG)
    {
        amount = random_range(attacker->get_hit_dice() * 11 / 3,
                              attacker->get_hit_dice() * 13 / 2);

        // strong poison pierces monster rpois (at half strength)
        // (players have the usual 2/3rds chance to resist)
        // XXX: do we really need the has_lifeforce() check...? force doesn't
        // override rpois+++
        if (defender->res_poison() > 0 && defender->has_lifeforce()
            && defender->is_monster())
        {
            amount /= 2;
            force = true;
        }
    }
    else
    {
        amount = random_range(attacker->get_hit_dice() * 2,
                              attacker->get_hit_dice() * 4);
    }

    if (!defender->poison(attacker, amount, force))
        return false;

    if (needs_message)
    {
        mprf("%s poisons %s!",
                atk_name(DESC_THE).c_str(),
                defender_name(true).c_str());
        if (force)
        {
            mprf("%s partially resist%s.",
                defender_name(false).c_str(),
                defender->is_player() ? "" : "s");
        }
    }

    return true;
}

void melee_attack::mons_do_napalm()
{
    if (defender->res_sticky_flame())
        return;

    if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
    {
        if (needs_message)
        {
            mprf("%s %s covered in liquid flames%s",
                 defender_name(false).c_str(),
                 defender->conj_verb("are").c_str(),
                 attack_strength_punctuation(special_damage).c_str());
        }

        if (defender->is_player())
            napalm_player(random2avg(7, 3) + 1, atk_name(DESC_A));
        else
        {
            napalm_monster(
                defender->as_monster(),
                attacker,
                min(4, 1 + random2(attacker->get_hit_dice())/2));
        }
    }
}

void melee_attack::splash_defender_with_acid(int strength)
{
    if (defender->is_player())
    {
        mpr("You are splashed with acid!");
        splash_with_acid(strength, attacker->mindex());
    }
    else
    {
        special_damage += roll_dice(2, 4);
        if (defender_visible)
            mprf("%s is splashed with acid.", defender->name(DESC_THE).c_str());
        defender->as_monster()->splash_with_acid(attacker);
    }
}

static void _print_resist_messages(actor* defender, int base_damage,
                                   beam_type flavour)
{
    // check_your_resists is used for the player case to get additional
    // effects such as Xom amusement, melting of icy effects, etc.
    // mons_adjust_flavoured is used for the monster case to get all of the
    // special message handling ("The ice beast melts!") correct.
    // XXX: there must be a nicer way to do this, especially because we're
    // basically calculating the damage twice in the case where messages
    // are needed.
    if (defender->is_player())
        (void)check_your_resists(base_damage, flavour, "");
    else
    {
        bolt beam;
        beam.flavour = flavour;
        (void)mons_adjust_flavoured(defender->as_monster(),
                                    beam,
                                    base_damage,
                                    true);
    }
}

bool melee_attack::mons_attack_effects()
{
    // Monsters attacking themselves don't get attack flavour.
    // The message sequences look too weird.  Also, stealing
    // attacks aren't handled until after the damage msg. Also,
    // no attack flavours for dead defenders
    if (attacker != defender && defender->alive())
    {
        mons_apply_attack_flavour();

        if (needs_message && !special_damage_message.empty())
            mprf("%s", special_damage_message.c_str());

        if (special_damage > 0)
        {
            inflict_damage(special_damage, special_damage_flavour);
            special_damage = 0;
            special_damage_message.clear();
            special_damage_flavour = BEAM_NONE;
        }

        apply_staff_damage();

        if (needs_message && !special_damage_message.empty())
            mprf("%s", special_damage_message.c_str());

        if (special_damage > 0
            && inflict_damage(special_damage, special_damage_flavour))
        {
            defender->expose_to_element(special_damage_flavour, 2);
        }
    }

    if (defender->is_player())
        practise(EX_MONSTER_WILL_HIT);

    // decapitate_hydra() returns true if the wound was cauterized or the
    // last head was removed.  In the former case, we shouldn't apply
    // the brand damage (so we return here).  If the monster was killed
    // by the decapitation, we should stop the rest of the attack, too.
    if (decapitate_hydra(damage_done, attacker->damage_type(attack_number)))
        return defender->alive();

    if (attacker != defender && attk_type == AT_TRAMPLE)
        do_knockback();

    special_damage = 0;
    special_damage_message.clear();
    special_damage_flavour = BEAM_NONE;

    const bool chaos_attack = damage_brand == SPWPN_CHAOS
                              || (attk_flavour == AF_CHAOS
                                  && attacker != defender);

    // Defender banished.  Bail since the defender is still alive in the
    // Abyss.
    if (defender->is_banished())
    {
        if (chaos_attack && attacker->alive())
            chaos_affects_attacker();

        do_miscast();
        return false;
    }

    if (!defender->alive())
    {
        if (chaos_attack && attacker->alive())
            chaos_affects_attacker();

        do_miscast();
        return true;
    }

    // Yredelemnul's injury mirroring can kill the attacker.
    // Also, bail if the monster is attacking itself without a
    // weapon, since intrinsic monster attack flavours aren't
    // applied for self-attacks.
    if (!attacker->alive() || (attacker == defender && !weapon))
    {
        if (miscast_target == defender)
            do_miscast();
        return false;
    }

    if (!defender->alive())
    {
        if (chaos_attack && attacker->alive())
            chaos_affects_attacker();

        do_miscast();
        return true;
    }

    if (chaos_attack && attacker->alive())
        chaos_affects_attacker();

    if (miscast_target == defender)
        do_miscast();

    // Yredelemnul's injury mirroring can kill the attacker.
    if (!attacker->alive())
        return false;

    if (miscast_target == attacker)
        do_miscast();

    // Miscast might have killed the attacker.
    if (!attacker->alive())
        return false;

    return true;
}

void melee_attack::mons_apply_attack_flavour()
{
    // Most of this is from BWR 4.1.2.
    int base_damage = 0;

    attack_flavour flavour = attk_flavour;
    if (flavour == AF_CHAOS)
        flavour = random_chaos_attack_flavour();

    switch (flavour)
    {
    default:
        // Just to trigger special melee damage effects for regular attacks
        // (e.g. Qazlal's elemental adaptation).
        defender->expose_to_element(BEAM_MISSILE, 2);
        break;

    case AF_MUTATE:
        if (one_chance_in(4))
        {
            defender->malmutate(you.can_see(attacker) ?
                apostrophise(attacker->name(DESC_PLAIN)) + " mutagenic touch" :
                "mutagenic touch");
        }
        break;

    case AF_POISON:
    case AF_POISON_STRONG:
        if (one_chance_in(3))
            mons_do_poison();
        break;

    case AF_ROT:
        if (one_chance_in(20) || (damage_done > 2 && one_chance_in(3)))
            rot_defender(2 + random2(3), damage_done > 5 ? 1 : 0);
        break;

    case AF_FIRE:
        base_damage = attacker->get_hit_dice()
                      + random2(attacker->get_hit_dice());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 base_damage);
        special_damage_flavour = BEAM_FIRE;

        if (needs_message && base_damage)
        {
            mprf("%s %s engulfed in flames%s",
                 defender_name(false).c_str(),
                 defender->conj_verb("are").c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_COLD:
        base_damage = attacker->get_hit_dice()
                      + random2(2 * attacker->get_hit_dice());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_COLD,
                                 defender->res_cold(),
                                 base_damage);
        special_damage_flavour = BEAM_COLD;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("freeze").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_COLD);
        }

        defender->expose_to_element(BEAM_COLD, 2);
        break;

    case AF_ELEC:
        base_damage = attacker->get_hit_dice()
                      + random2(attacker->get_hit_dice() / 2);

        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_ELECTRICITY,
                                 defender->res_elec(),
                                 base_damage);
        special_damage_flavour = BEAM_ELECTRICITY;

        if (needs_message && base_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("shock").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

            _print_resist_messages(defender, base_damage, BEAM_ELECTRICITY);
        }

        dprf(DIAG_COMBAT, "Shock damage: %d", special_damage);
        defender->expose_to_element(BEAM_ELECTRICITY, 2);
        break;

    case AF_VAMPIRIC:
        // Only may bite non-vampiric monsters (or player) capable of bleeding.
        if (!defender->can_bleed())
            break;

        // Disallow draining of summoned monsters since they can't bleed.
        // XXX: Is this too harsh?
        if (defender->is_summoned())
            break;

        if (x_chance_in_y(defender->res_negative_energy(), 3))
            break;

        if (defender->stat_hp() < defender->stat_maxhp())
        {
            if (attacker->heal(1 + random2(damage_done), coinflip())
                && needs_message)
            {
                mprf("%s %s strength from %s injuries!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb("draw").c_str(),
                     def_name(DESC_ITS).c_str());
            }
        }
        break;

    case AF_DRAIN_STR:
    case AF_DRAIN_INT:
    case AF_DRAIN_DEX:
        if ((one_chance_in(20) || (damage_done > 0 && one_chance_in(3)))
            && defender->res_negative_energy() < random2(4))
        {
            stat_type drained_stat = (flavour == AF_DRAIN_STR ? STAT_STR :
                                      flavour == AF_DRAIN_INT ? STAT_INT
                                                              : STAT_DEX);
            defender->drain_stat(drained_stat, 1, attacker);
        }
        break;

    case AF_HUNGER:
        if (defender->holiness() == MH_UNDEAD)
            break;

        if (one_chance_in(20) || damage_done > 0)
            defender->make_hungry(you.hunger / 4, false);
        break;

    case AF_BLINK:
        // blinking can kill, delay the call
        if (one_chance_in(3))
            (new blink_fineff(attacker))->schedule();
        break;

    case AF_CONFUSE:
        if (attk_type == AT_SPORE)
        {
            if (defender->is_unbreathing())
                break;

            monster *attkmon = attacker->as_monster();
            attkmon->set_hit_dice(attkmon->get_experience_level() - 1);
            if (attkmon->get_hit_dice() <= 0)
                attacker->as_monster()->suicide();

            if (defender_visible)
            {
                mprf("%s %s engulfed in a cloud of spores!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("are").c_str());
            }
        }

        if (one_chance_in(10)
            || (damage_done > 2 && one_chance_in(3)))
        {
            defender->confuse(attacker,
                              1 + random2(3+attacker->get_hit_dice()));
        }
        break;

    case AF_DRAIN_XP:
        if (one_chance_in(30) || (damage_done > 5 && coinflip()))
            drain_defender();
        break;

    case AF_PARALYSE:
    {
        // Only wasps at the moment, so Zin vitalisation
        // protects from the paralysis and slow.
        if (you.duration[DUR_DIVINE_STAMINA] > 0)
        {
            mpr("Your divine stamina protects you from poison!");
            break;
        }

        // doesn't affect poison-immune enemies
        if (defender->res_poison() >= 3
            || defender->is_monster() && defender->res_poison() >= 1)
        {
            break;
        }

        if (attacker->type == MONS_RED_WASP || one_chance_in(3))
        {
            int dmg = random_range(attacker->get_hit_dice() * 3 / 2,
                                   attacker->get_hit_dice() * 5 / 2);
            defender->poison(attacker, dmg);
        }

        int paralyse_roll = (damage_done > 4 ? 3 : 20);
        if (attacker->type == MONS_YELLOW_WASP)
            paralyse_roll += 3;

        const int flat_bonus  = attacker->type == MONS_RED_WASP ? 1 : 0;
        const bool strong_result = one_chance_in(paralyse_roll);

        if (strong_result && defender->res_poison() <= 0)
            defender->paralyse(attacker, flat_bonus + roll_dice(1, 3));
        else if (strong_result || defender->res_poison() <= 0)
            defender->slow_down(attacker, flat_bonus + roll_dice(1, 3));

        break;
    }

    case AF_ACID:
        splash_defender_with_acid(3);
        break;

    case AF_DISTORT:
        distortion_affects_defender();
        break;

    case AF_RAGE:
        if (!one_chance_in(3) || !defender->can_go_berserk())
            break;

        if (needs_message)
        {
            mprf("%s %s %s!",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("infuriate").c_str(),
                 defender_name(true).c_str());
        }

        defender->go_berserk(false);
        break;

    case AF_STICKY_FLAME:
        mons_do_napalm();
        break;

    case AF_CHAOS:
        chaos_affects_defender();
        break;

    case AF_STEAL:
        // Ignore monsters, for now.
        if (!defender->is_player())
            break;

        attacker->as_monster()->steal_item_from_player();
        break;

    case AF_HOLY:
        if (defender->holy_wrath_susceptible())
            special_damage = attk_damage * 0.75;

        if (needs_message && special_damage)
        {
            mprf("%s %s %s%s",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("sear").c_str(),
                 defender_name(true).c_str(),
                 attack_strength_punctuation(special_damage).c_str());

        }
        break;

    case AF_ANTIMAGIC:
        antimagic_affects_defender(attacker->get_hit_dice() * 12);

        if (mons_genus(attacker->type) == MONS_VINE_STALKER
            && attacker->is_monster())
        {
            const bool spell_user =
                defender->is_player()
                || mons_antimagic_affected(defender->as_monster());

            if (you.can_see(attacker) || you.can_see(defender))
            {
                mprf("%s drains %s %s.",
                     attacker->name(DESC_THE).c_str(),
                     defender->pronoun(PRONOUN_POSSESSIVE).c_str(),
                     spell_user ? "magic" : "power");
            }

            monster* vine = attacker->as_monster();
            if (vine->has_ench(ENCH_ANTIMAGIC)
                && (defender->is_player()
                    || (!defender->as_monster()->is_summoned()
                        && !mons_is_firewood(defender->as_monster()))))
            {
                mon_enchant me = vine->get_ench(ENCH_ANTIMAGIC);
                vine->lose_ench_duration(me, random2(damage_done) + 1);
                simple_monster_message(attacker->as_monster(),
                                       spell_user
                                       ? " looks very invigorated."
                                       : " looks invigorated.");
            }
        }
        break;

    case AF_PAIN:
        pain_affects_defender();
        break;

    case AF_ENSNARE:
        if (one_chance_in(3))
            ensnare(defender);
        break;

    case AF_CRUSH:
        if (needs_message)
        {
            mprf("%s %s %s.",
                 atk_name(DESC_THE).c_str(),
                 attacker->conj_verb("grab").c_str(),
                 defender_name(true).c_str());
        }
        attacker->start_constricting(*defender);
        // if you got grabbed, interrupt stair climb and passwall
        if (defender->is_player())
            stop_delay(true);
        break;

    case AF_ENGULF:
        if (x_chance_in_y(2, 3) && attacker->can_constrict(defender))
        {
            if (defender->is_player() && !you.duration[DUR_WATER_HOLD]
                && !you.duration[DUR_WATER_HOLD_IMMUNITY])
            {
                you.duration[DUR_WATER_HOLD] = 10;
                you.props["water_holder"].get_int() = attacker->as_monster()->mid;
            }
            else if (defender->is_monster()
                     && !defender->as_monster()->has_ench(ENCH_WATER_HOLD))
            {
                defender->as_monster()->add_ench(mon_enchant(ENCH_WATER_HOLD, 1,
                                                             attacker, 1));
            }
            else
                return; //Didn't apply effect; no message

            if (needs_message)
            {
                mprf("%s %s %s in water!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb("engulf").c_str(),
                     defender_name(true).c_str());
            }
        }

        defender->expose_to_element(BEAM_WATER, 0);
        break;

    case AF_PURE_FIRE:
        if (attacker->type == MONS_FIRE_VORTEX)
            attacker->as_monster()->suicide(-10);

        special_damage = (attacker->get_hit_dice() * 3 / 2
                          + random2(attacker->get_hit_dice()));
        special_damage = defender->apply_ac(special_damage, 0, AC_HALF);
        special_damage = resist_adjust_damage(defender,
                                              BEAM_FIRE,
                                              defender->res_fire(),
                                              special_damage);

        if (needs_message && special_damage)
        {
            mprf("%s %s %s!",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("burn").c_str(),
                    defender_name(true).c_str());

            _print_resist_messages(defender, special_damage, BEAM_FIRE);
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;

    case AF_DRAIN_SPEED:
        if (x_chance_in_y(3, 5) && !defender->res_negative_energy())
        {
            if (needs_message)
            {
                mprf("%s %s %s vigor!",
                     atk_name(DESC_THE).c_str(),
                     attacker->conj_verb("drain").c_str(),
                     def_name(DESC_ITS).c_str());
            }

            special_damage = 1 + random2(damage_done) / 2;
            defender->slow_down(attacker, 5 + random2(7));
        }
        break;

    case AF_VULN:
        if (one_chance_in(3))
        {
            bool visible_effect = false;
            if (defender->is_player())
            {
                if (!you.duration[DUR_LOWERED_MR])
                    visible_effect = true;
                you.increase_duration(DUR_LOWERED_MR, 20 + random2(20), 40);
            }
            else
            {
                if (!defender->as_monster()->has_ench(ENCH_LOWERED_MR))
                    visible_effect = true;
                mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, attacker,
                                       (20 + random2(20)) * BASELINE_DELAY);
                defender->as_monster()->add_ench(lowered_mr);
            }

            if (needs_message && visible_effect)
            {
                mprf("%s magical defenses are stripped away!",
                     def_name(DESC_ITS).c_str());
            }
        }
        break;

    case AF_WEAKNESS_POISON:
        if (coinflip() && mons_do_poison())
            defender->weaken(attacker, 12);
        break;

    case AF_SHADOWSTAB:
        attacker->as_monster()->del_ench(ENCH_INVIS, true);
        break;

    case AF_DROWN:
        if (attacker->type == MONS_DROWNED_SOUL)
            attacker->as_monster()->suicide(-1000);

        if (defender->res_water_drowning() <= 0)
        {
            special_damage = attacker->get_hit_dice() * 3 / 4
                            + random2(attacker->get_hit_dice() * 3 / 4);
            special_damage_flavour = BEAM_WATER;

            if (needs_message)
            {
                mprf("%s %s %s%s",
                    atk_name(DESC_THE).c_str(),
                    attacker->conj_verb("drown").c_str(),
                    defender_name(true).c_str(),
                    attack_strength_punctuation(special_damage).c_str());
            }
        }
        break;

    case AF_FIREBRAND:
        base_damage = attacker->get_hit_dice()
                      + random2(attacker->get_hit_dice());
        special_damage =
            resist_adjust_damage(defender,
                                 BEAM_FIRE,
                                 defender->res_fire(),
                                 base_damage);
        special_damage_flavour = BEAM_FIRE;

        if (base_damage)
        {
            if (needs_message)
            {
                mprf("The air around %s erupts in flames!",
                    defender_name(false).c_str());

                for (adjacent_iterator ai(defender->pos()); ai; ++ai)
                {
                    if (!cell_is_solid(*ai)
                        && (env.cgrid(*ai) == EMPTY_CLOUD
                            || env.cloud[env.cgrid(*ai)].type == CLOUD_FIRE))
                    {
                        // Don't place clouds under non-resistant allies
                        const actor* act = actor_at(*ai);
                        if (act && mons_aligned(attacker, act)
                            && act->res_fire() < 1)
                        {
                            continue;
                        }

                        place_cloud(CLOUD_FIRE, *ai, 4 + random2(9), attacker);
                    }
                }

                _print_resist_messages(defender, base_damage, BEAM_FIRE);
            }
        }

        defender->expose_to_element(BEAM_FIRE, 2);
        break;
    }
}

void melee_attack::do_passive_freeze()
{
    if (you.mutation[MUT_PASSIVE_FREEZE]
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos()))
    {
        bolt beam;
        beam.flavour = BEAM_COLD;
        beam.thrower = KILL_YOU;

        monster* mon = attacker->as_monster();

        const int orig_hurted = random2(11);
        int hurted = mons_adjust_flavoured(mon, beam, orig_hurted);

        if (!hurted)
            return;

        simple_monster_message(mon, " is very cold.");

#ifndef USE_TILE_LOCAL
        flash_monster_colour(mon, LIGHTBLUE, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_COLD, orig_hurted);
            print_wounds(mon);

            const int cold_res = mon->res_cold();

            if (cold_res <= 0)
            {
                const int stun = (1 - cold_res) * random2(7);
                mon->speed_increment -= stun;
            }
        }
    }
}

#if TAG_MAJOR_VERSION == 34
void melee_attack::do_passive_heat()
{
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_PASSIVE_HEAT)
        && attacker->alive()
        && grid_distance(you.pos(), attacker->as_monster()->pos()) == 1)
    {
        bolt beam;
        beam.flavour = BEAM_FIRE;
        beam.thrower = KILL_YOU;

        monster* mon = attacker->as_monster();

        const int orig_hurted = random2(5);
        int hurted = mons_adjust_flavoured(mon, beam, orig_hurted);

        if (!hurted)
            return;

        simple_monster_message(mon, " is singed by your heat.");

#ifndef USE_TILE
        flash_monster_colour(mon, LIGHTRED, 200);
#endif

        mon->hurt(&you, hurted);

        if (mon->alive())
        {
            mon->expose_to_element(BEAM_FIRE, orig_hurted);
            print_wounds(mon);
        }
    }
}
#endif

void melee_attack::mons_do_eyeball_confusion()
{
    if (you.mutation[MUT_EYEBALLS]
        && attacker->alive()
        && adjacent(you.pos(), attacker->as_monster()->pos())
        && x_chance_in_y(player_mutation_level(MUT_EYEBALLS), 20))
    {
        const int ench_pow = player_mutation_level(MUT_EYEBALLS) * 30;
        monster* mon = attacker->as_monster();

        if (mon->check_res_magic(ench_pow) <= 0
            && mons_class_is_confusable(mon->type))
        {
            mprf("The eyeballs on your body gaze at %s.",
                 mon->name(DESC_THE).c_str());

            if (!mon->check_clarity(false))
            {
                mon->add_ench(mon_enchant(ENCH_CONFUSION, 0, &you,
                                          30 + random2(100)));
            }
        }
    }
}

void melee_attack::tendril_disarm()
{
    monster *mon = attacker->as_monster();
    item_def *mons_wpn = mon->mslot_item(MSLOT_WEAPON);

    // is it ok to move the weapon into your tile (w/o destroying it?)
    const bool your_tile_ok = (grd(you.pos()) != DNGN_DEEP_WATER
                                || species_likes_water(you.species))
                               && grd(you.pos()) != DNGN_LAVA;
    // what about their tile?
    const bool mon_tile_ok = grd(mon->pos()) != DNGN_LAVA
                             && (your_tile_ok
                                 || grd(mon->pos()) != DNGN_DEEP_WATER
                                 || species_likes_water(you.species));
    // XXX: refactor the above to use player.cc functions

    if (!you.mutation[MUT_TENDRILS]
        || !mons_wpn
        || mons_wpn->cursed()
        || !attacker->alive()
        || mons_class_is_animated_weapon(mon->type)
        || !adjacent(you.pos(), mon->pos())
        || !you.can_see(mon)
        || !mon_tile_ok
        || !one_chance_in(5))
    {
        return;
    }

    const int mon_hd = mon->get_hit_dice();
    const int adj_mon_hd = mons_class_flag(mon->type, M_FIGHTER) ? mon_hd * 3/2
                                                                 : mon_hd;
    // some rounding errors here, but not significant

    if (random2(you.dex()) <= adj_mon_hd
        && random2(you.strength()) <= adj_mon_hd)
    {
        return;
    }

    mprf("Your tendrils lash around %s %s and pull it to the ground!",
        apostrophise(mon->name(DESC_THE)).c_str(),
         mons_wpn->name(DESC_PLAIN).c_str());

    mon->drop_item(MSLOT_WEAPON, false);
    if (your_tile_ok)
    {
        move_top_item(mon->pos(), you.pos());
        // assumes nothing's re-ordering items - e.g. gozag gold
        // (but that shouldn't interact with jiyva tendrils...?)
    }
}

void melee_attack::do_spines()
{
    // Monsters only get struck on their first attack per round
    if (attacker->is_monster() && effective_attack_number > 0)
        return;

    if (defender->is_player())
    {
        const int mut = (you.form == TRAN_PORCUPINE) ? 3
                        : player_mutation_level(MUT_SPINY);

        if (mut && attacker->alive()
            && x_chance_in_y(2, (13 - (mut * 2)) * 3))
        {
            int dmg = roll_dice(2 + div_rand_round(mut - 1, 2), 5);
            int hurt = attacker->apply_ac(dmg);

            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt <= 0)
                return;

            simple_monster_message(attacker->as_monster(),
                                   " is struck by your spines.");

            attacker->hurt(&you, hurt);
        }
    }
    else if (defender->as_monster()->spiny_degree() > 0)
    {
        // Thorn hunters can attack their own brambles without injury
        if (defender->type == MONS_BRIAR_PATCH
            && attacker->type == MONS_THORN_HUNTER
            // Dithmenos' shadow can't take damage, don't spam.
            || attacker->type == MONS_PLAYER_SHADOW)
        {
            return;
        }

        const int degree = defender->as_monster()->spiny_degree();

        if (attacker->alive() && x_chance_in_y(degree, 15))
        {
            int dmg = roll_dice(degree, 4);
            int hurt = attacker->apply_ac(dmg);
            dprf(DIAG_COMBAT, "Spiny: dmg = %d hurt = %d", dmg, hurt);

            if (hurt <= 0)
                return;
            if (you.can_see(defender) || attacker->is_player())
            {
                mprf("%s %s struck by %s %s.", attacker->name(DESC_THE).c_str(),
                     attacker->conj_verb("are").c_str(),
                     defender->name(DESC_ITS).c_str(),
                     defender->type == MONS_BRIAR_PATCH ? "thorns"
                                                        : "spines");
            }
            if (attacker->is_player())
                ouch(hurt, defender->mindex(), KILLED_BY_SPINES);
            else
                attacker->hurt(defender, hurt);
        }
    }
}

void melee_attack::emit_foul_stench()
{
    monster* mon = attacker->as_monster();

    if (you.mutation[MUT_FOUL_STENCH]
        && attacker->alive()
        && adjacent(you.pos(), mon->pos()))
    {
        const int mut = player_mutation_level(MUT_FOUL_STENCH);

        if (one_chance_in(3))
            mon->sicken(50 + random2(100));

        if (damage_done > 4 && x_chance_in_y(mut, 5)
            && !cell_is_solid(mon->pos())
            && env.cgrid(mon->pos()) == EMPTY_CLOUD)
        {
            mpr("You emit a cloud of foul miasma!");
            place_cloud(CLOUD_MIASMA, mon->pos(), 5 + random2(6), &you);
        }
    }
}

void melee_attack::do_minotaur_retaliation()
{
    if (defender->cannot_act()
        || defender->confused()
        || !attacker->alive()
        || defender->is_player() && you.duration[DUR_LIFESAVING])
    {
        return;
    }

    if (!defender->is_player())
    {
        // monsters have no STR or DEX
        if (x_chance_in_y(2, 5))
        {
            int hurt = attacker->apply_ac(random2(21));
            if (you.see_cell(defender->pos()))
            {
                const string defname = defender->name(DESC_THE);
                mprf("%s furiously retaliates!", defname.c_str());
                if (hurt <= 0)
                {
                    mprf("%s headbutts %s, but does no damage.", defname.c_str(),
                         attacker->name(DESC_THE).c_str());
                }
                else
                {
                    mprf("%s headbutts %s%s", defname.c_str(),
                         attacker->name(DESC_THE).c_str(),
                         attack_strength_punctuation(hurt).c_str());
                }
            }
            if (hurt > 0)
            {
                if (attacker->is_player())
                    ouch(hurt, defender->mindex(), KILLED_BY_HEADBUTT);
                else
                    attacker->hurt(defender, hurt);
            }
        }
        return;
    }

    if (!form_keeps_mutations())
    {
        // You are in a non-minotaur form.
        return;
    }
    // This will usually be 2, but could be 3 if the player mutated more.
    const int mut = player_mutation_level(MUT_HORNS);

    if (5 * you.strength() + 7 * you.dex() > random2(600))
    {
        // Use the same damage formula as a regular headbutt.
        int dmg = 5 + mut * 3;
        dmg = player_aux_stat_modify_damage(dmg);
        dmg = random2(dmg);
        dmg = player_apply_fighting_skill(dmg, true);
        dmg = player_apply_misc_modifiers(dmg);
        dmg = player_apply_slaying_bonuses(dmg, true);
        dmg = player_apply_final_multipliers(dmg);
        int hurt = attacker->apply_ac(dmg);

        mpr("You furiously retaliate!");
        dprf(DIAG_COMBAT, "Retaliation: dmg = %d hurt = %d", dmg, hurt);
        if (hurt <= 0)
        {
            mprf("You headbutt %s, but do no damage.",
                 attacker->name(DESC_THE).c_str());
            return;
        }
        else
        {
            mprf("You headbutt %s%s",
                 attacker->name(DESC_THE).c_str(),
                 attack_strength_punctuation(hurt).c_str());
            attacker->hurt(&you, hurt);
        }
    }
}

bool melee_attack::do_knockback(bool trample)
{
    do
    {
        if (defender->is_stationary())
            return false; // don't even print a message

        int size_diff =
            attacker->body_size(PSIZE_BODY) - defender->body_size(PSIZE_BODY);
        if (!x_chance_in_y(size_diff + 3, 6))
            break;

        coord_def old_pos = defender->pos();
        coord_def new_pos = defender->pos() + defender->pos() - attack_position;

        // need a valid tile
        if (grd(new_pos) < DNGN_SHALLOW_WATER
            && !defender->is_habitable_feat(grd(new_pos)))
        {
            break;
        }

        // don't trample into a monster - or do we want to cause a chain
        // reaction here?
        if (actor_at(new_pos))
            break;

        // Prevent trample/drown combo when flight is expiring
        if (defender->is_player() && need_expiration_warning(new_pos))
            break;

        if (needs_message)
        {
            mprf("%s %s backwards!",
                 defender_name(false).c_str(),
                 defender->conj_verb("stumble").c_str());
        }

        // Schedule following _before_ actually trampling -- if the defender
        // is a player, a shaft trap will unload the level.  If trampling will
        // somehow fail, move attempt will be ignored.
        if (trample)
            (new trample_follow_fineff(attacker, old_pos))->schedule();

        if (defender->as_player())
            move_player_to_grid(new_pos, false);
        else
            defender->move_to_pos(new_pos);

        // Interrupt stair travel and passwall.
        if (defender->is_player())
            stop_delay(true);

        return true;
    } while (0);

    if (needs_message)
    {
        mprf("%s %s %s ground!",
             defender_name(false).c_str(),
             defender->conj_verb("hold").c_str(),
             defender->pronoun(PRONOUN_POSSESSIVE).c_str());
    }

    return false;
}

// Cleave covers the 8 adjacent cells, but is stopped by solid features.
// Allies are passed through without harm.
void melee_attack::cleave_setup()
{
    if (cell_is_solid(defender->pos()))
        return;

    // Don't cleave on a self-attack.
    if (attacker->pos() == defender->pos())
        return;

    int dir = coinflip() ? -1 : 1;
    bool behind = coinflip();
    get_cleave_targets(attacker, defender->pos(), dir, cleave_targets, behind);
    cleave_targets.reverse();
    attack_cleave_targets(attacker, cleave_targets, attack_number,
                          effective_attack_number);

    // We need to get the list of the remaining potential targets now because
    // if the main target dies, its position will be lost.
    get_cleave_targets(attacker, defender->pos(), -dir, cleave_targets, !behind);
}

// cleave damage modifier for additional attacks: 75% of base damage
int melee_attack::cleave_damage_mod(int dam)
{
    return div_rand_round(dam * 3, 4);
}

void melee_attack::chaos_affect_actor(actor *victim)
{
    melee_attack attk(victim, victim);
    attk.weapon = NULL;
    attk.fake_chaos_attack = true;
    attk.chaos_affects_defender();
    attk.do_miscast();
    if (!attk.special_damage_message.empty()
        && you.can_see(victim))
    {
        mprf("%s", attk.special_damage_message.c_str());
    }
}

bool melee_attack::_tran_forbid_aux_attack(unarmed_attack_type atk)
{
    switch (atk)
    {
    case UNAT_KICK:
    case UNAT_PECK:
    case UNAT_HEADBUTT:
    case UNAT_PUNCH:
        return you.form == TRAN_ICE_BEAST
               || you.form == TRAN_DRAGON
               || you.form == TRAN_SPIDER
               || you.form == TRAN_BAT;

    case UNAT_CONSTRICT:
        return !form_keeps_mutations();

    default:
        return false;
    }
}

bool melee_attack::_extra_aux_attack(unarmed_attack_type atk, bool is_uc)
{
    // No extra unarmed attacks for disabled mutations.
    if (_tran_forbid_aux_attack(atk))
        return false;

    if (atk == UNAT_CONSTRICT)
    {
        return is_uc
                || you.species == SP_NAGA && you.experience_level > 12
                || you.species == SP_OCTOPODE && you.has_usable_tentacle();
    }

    if (you.strength() + you.dex() <= random2(50))
        return false;

    switch (atk)
    {
    case UNAT_KICK:
        return is_uc
                || you.has_usable_hooves()
                || you.has_usable_talons()
                || player_mutation_level(MUT_TENTACLE_SPIKE);

    case UNAT_PECK:
        return (is_uc
                || player_mutation_level(MUT_BEAK))
               && !one_chance_in(3);

    case UNAT_HEADBUTT:
        return (is_uc
                || player_mutation_level(MUT_HORNS))
               && !one_chance_in(3);

    case UNAT_TAILSLAP:
        return (is_uc
                || you.has_usable_tail())
               && coinflip();

    case UNAT_PSEUDOPODS:
        return (is_uc
                || you.has_usable_pseudopods())
               && !one_chance_in(3);

    case UNAT_TENTACLES:
        return (is_uc
                || you.has_usable_tentacles())
               && !one_chance_in(3);

    case UNAT_BITE:
        return ((is_uc
                 || you.has_usable_fangs()
                 || player_mutation_level(MUT_ACIDIC_BITE))
                && x_chance_in_y(2, 5))
               || you.mutation[MUT_ANTIMAGIC_BITE];

    case UNAT_PUNCH:
        return is_uc && !one_chance_in(3);

    default:
        return false;
    }
}

// TODO: Potentially move this, may or may not belong here (may not
// even belong as its own function, could be integrated with the general
// to-hit method
// Returns the to-hit for your extra unarmed attacks.
// DOES NOT do the final roll (i.e., random2(your_to_hit)).
int melee_attack::calc_your_to_hit_unarmed(int uattack, bool vampiric)
{
    int your_to_hit;

    your_to_hit = 1300
                + you.dex() * 60
                + you.strength() * 15
                + you.skill(SK_FIGHTING, 30);
    your_to_hit /= 100;

    if (you.inaccuracy())
        your_to_hit -= 5;

    if (player_mutation_level(MUT_EYEBALLS))
        your_to_hit += 2 * player_mutation_level(MUT_EYEBALLS) + 1;

    // Vampires know how to bite and aim better when thirsty.
    if (you.species == SP_VAMPIRE && uattack == UNAT_BITE)
    {
        your_to_hit += 1;

        if (vampiric)
        {
            if (you.hunger_state == HS_STARVING)
                your_to_hit += 2;
            else if (you.hunger_state < HS_SATIATED)
                your_to_hit += 1;
        }
    }
    else if (you.species != SP_VAMPIRE && you.hunger_state == HS_STARVING)
        your_to_hit -= 3;

    your_to_hit += slaying_bonus();

    return your_to_hit;
}

bool melee_attack::using_weapon()
{
    return weapon && is_melee_weapon(*weapon);
}

int melee_attack::weapon_damage()
{
    if (!using_weapon())
        return 0;

    return property(*weapon, PWPN_DAMAGE);
}

int melee_attack::calc_mon_to_hit_base()
{
    const bool fighter = attacker->is_monster()
                         && attacker->as_monster()->is_fighter();
    const int hd_mult = fighter ? 25 : 15;
    return 18 + attacker->get_hit_dice() * hd_mult / 10;
}

/**
 * Add modifiers to the base damage.
 * Currently only relevant for monsters.
 */
int melee_attack::apply_damage_modifiers(int damage, int damage_max,
                                         bool &half_ac)
{
    ASSERT(attacker->is_monster());
    monster *as_mon = attacker->as_monster();

    int frenzy_degree = -1;

    // Berserk/mighted monsters get bonus damage.
    if (as_mon->has_ench(ENCH_MIGHT)
        || as_mon->has_ench(ENCH_BERSERK))
    {
        damage = damage * 3 / 2;
    }
    else if (as_mon->has_ench(ENCH_BATTLE_FRENZY))
        frenzy_degree = as_mon->get_ench(ENCH_BATTLE_FRENZY).degree;
    else if (as_mon->has_ench(ENCH_ROUSED))
        frenzy_degree = as_mon->get_ench(ENCH_ROUSED).degree;
    else
    {
        frenzy_degree = as_mon->aug_amount();
        if (frenzy_degree <= 0)
            frenzy_degree = -1;
    }

    if (frenzy_degree != -1)
    {
#ifdef DEBUG_DIAGNOSTICS
        const int orig_damage = damage;
#endif

        damage = damage * (115 + frenzy_degree * 15) / 100;

        dprf(DIAG_COMBAT, "%s frenzy damage: %d->%d",
             attacker->name(DESC_PLAIN).c_str(), orig_damage, damage);
    }

    if (as_mon->has_ench(ENCH_WEAK))
        damage = damage * 2 / 3;

    half_ac = (as_mon->type == MONS_PHANTASMAL_WARRIOR);

    // If the defender is asleep, the attacker gets a stab.
    if (defender && (defender->asleep()
                     || (attk_flavour == AF_SHADOWSTAB
                         &&!defender->can_see(attacker))))
    {
        damage = damage * 5 / 2;
        dprf(DIAG_COMBAT, "Stab damage vs %s: %d",
             defender->name(DESC_PLAIN).c_str(),
             damage);
    }

    if (cleaving)
        damage = cleave_damage_mod(damage);

    return damage;
}

int melee_attack::calc_damage()
{
    // Constriction deals damage over time, not when grabbing.
    if (attk_flavour == AF_CRUSH)
        return 0;

    return attack::calc_damage();
}

/* TODO: This code is only used from melee_attack methods, but perhaps it
 * should be ambigufied and moved to the actor class
 * Should life protection protect from this?
 *
 * Should eventually remove in favor of player/monster symmetry
 *
 * Called when stabbing, for bite attacks, and vampires wielding vampiric weapons
 * Returns true if blood was drawn.
 */
bool melee_attack::_player_vampire_draws_blood(const monster* mon, const int damage,
                                               bool needs_bite_msg, int reduction)
{
    ASSERT(you.species == SP_VAMPIRE);

    if (!_vamp_wants_blood_from_monster(mon) ||
        (!adjacent(defender->pos(), attack_position) && needs_bite_msg))
    {
        return false;
    }

    const corpse_effect_type chunk_type = mons_corpse_effect(mon->type);

    // Now print message, need biting unless already done (never for bat form!)
    if (needs_bite_msg && you.form != TRAN_BAT)
    {
        mprf("You bite %s, and draw %s blood!",
             mon->name(DESC_THE, true).c_str(),
             mon->pronoun(PRONOUN_POSSESSIVE).c_str());
    }
    else
    {
        mprf("You draw %s blood!",
             apostrophise(mon->name(DESC_THE, true)).c_str());
    }

    // Regain hp.
    if (you.hp < you.hp_max)
    {
        int heal = 1 + random2(damage);
        if (chunk_type == CE_CLEAN)
            heal += 1 + random2(damage);
        if (heal > you.experience_level)
            heal = you.experience_level;

        // Decrease healing when done in bat form.
        if (you.form == TRAN_BAT)
            heal /= 2;

        if (heal > 0 && !you.duration[DUR_DEATHS_DOOR])
        {
            inc_hp(heal);
            mprf("You feel %sbetter.", (you.hp == you.hp_max) ? "much " : "");
        }
    }

    // Gain nutrition.
    if (you.hunger_state != HS_ENGORGED)
    {
        int food_value = 0;
        if (chunk_type == CE_CLEAN)
            food_value = 30 + random2avg(59, 2);
        else if (chunk_is_poisonous(chunk_type))
            food_value = 15 + random2avg(29, 2);

        // Bats get rather less nutrition out of it.
        if (you.form == TRAN_BAT)
            food_value /= 2;

        food_value /= reduction;

        lessen_hunger(food_value, false);
    }

    did_god_conduct(DID_DRINK_BLOOD, 5 + random2(4));

    return true;
}

bool melee_attack::_vamp_wants_blood_from_monster(const monster* mon)
{
    if (you.species != SP_VAMPIRE)
        return false;

    if (you.hunger_state == HS_ENGORGED)
        return false;

    if (mon->is_summoned())
        return false;

    if (!mons_has_blood(mon->type))
        return false;

    const corpse_effect_type chunk_type = mons_corpse_effect(mon->type);

    // Don't drink poisonous or mutagenic blood.
    return chunk_type == CE_CLEAN
           || (chunk_is_poisonous(chunk_type) && player_res_poison());
}
