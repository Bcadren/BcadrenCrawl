/**
 * @file
 * @brief ranged_attack class and associated ranged_attack methods
 */

#include "AppHdr.h"

#include "ranged_attack.h"

#include "actor.h"
#include "art-enum.h"
#include "beam.h"
#include "coord.h"
#include "exercise.h"
#include "godconduct.h"
#include "itemname.h"
#include "itemprop.h"
#include "libutil.h"
#include "mon-behv.h"
#include "mon-message.h"
#include "monster.h"
#include "player.h"
#include "random.h"
#include "stuff.h"
#include "teleport.h"
#include "traps.h"

ranged_attack::ranged_attack(actor *attk, actor *defn, item_def *proj,
                             bool tele) :
                             ::attack(attk, defn), range_used(0),
                             reflected(false), projectile(proj), teleport(tele),
                             orig_to_hit(0), should_alert_defender(true)
{
    init_attack(SK_THROWING, 0);
    kill_type = KILLED_BY_BEAM;

    string proj_name = projectile->name(DESC_PLAIN);

    // [dshaligram] When changing bolt names here, you must edit
    // hiscores.cc (scorefile_entry::terse_missile_cause()) to match.
    if (attacker->is_player())
    {
        kill_type = KILLED_BY_SELF_AIMED;
        aux_source = proj_name;
    }
    else if (is_launched(attacker, weapon, *projectile) == LRET_LAUNCHED)
    {
        aux_source = make_stringf("Shot with a%s %s by %s",
                 (is_vowel(proj_name[0]) ? "n" : ""), proj_name.c_str(),
                 attacker->name(DESC_A).c_str());
    }
    else
    {
        aux_source = make_stringf("Hit by a%s %s thrown by %s",
                 (is_vowel(proj_name[0]) ? "n" : ""), proj_name.c_str(),
                 attacker->name(DESC_A).c_str());
    }

    needs_message = defender_visible;

    if (!using_weapon())
        wpn_skill = SK_THROWING;
}

int ranged_attack::calc_to_hit(bool random)
{
    orig_to_hit = attack::calc_to_hit(random);
    if (teleport)
    {
        orig_to_hit +=
            (attacker->is_player())
            ? maybe_random2(you.attribute[ATTR_PORTAL_PROJECTILE] / 4, random)
            : 3 * attacker->as_monster()->hit_dice;
    }

    int hit = orig_to_hit;
    const int defl = defender->missile_deflection();
    if (defl)
    {
        if (random)
            hit = random2(hit / defl);
        else
            hit = (hit - 1) / (2 * defl);
    }

    return hit;
}

bool ranged_attack::attack()
{
    if (!handle_phase_attempted())
        return false;

    // XXX: Can this ever happen?
    if (!defender->alive())
    {
        handle_phase_killed();
        handle_phase_end();
        return true;
    }

    const int ev = defender->melee_evasion(attacker);
    ev_margin = test_hit(to_hit, ev, !attacker->is_player());
    bool shield_blocked = attack_shield_blocked(false);

    god_conduct_trigger conducts[3];
    disable_attack_conducts(conducts);

    if (attacker->is_player() && attacker != defender)
    {
        set_attack_conducts(conducts, defender->as_monster());
        player_stab_check();
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
        if (ev_margin >= 0)
        {
            if (!handle_phase_hit())
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

    // TODO: sanctuary

    // TODO: adjust_noise

    if (should_alert_defender)
        alert_defender();

    if (!defender->alive())
        handle_phase_killed();

    if (attacker->is_player() && defender->is_monster()
        && !shield_blocked && ev_margin >= 0)
    {
        print_wounds(defender->as_monster());
    }

    handle_phase_end();

    enable_attack_conducts(conducts);

    return attack_occurred;
}

// XXX: Are there any cases where this might fail?
bool ranged_attack::handle_phase_attempted()
{
    attacker->attacking(defender, true);
    attack_occurred = true;

    return true;
}

bool ranged_attack::handle_phase_blocked()
{
    ASSERT(!attack_ignores_shield(false));
    string punctuation = ".";
    string verb = "block";
    if (defender_shield && is_shield(*defender_shield)
        && shield_reflects(*defender_shield))
    {
        reflected = true;
        verb = "reflect";
        if (defender->observable())
        {
            punctuation = " off " + defender->pronoun(PRONOUN_POSSESSIVE)
                          + " " + defender_shield->name(DESC_PLAIN).c_str()
                          + "!";
            ident_reflector(defender_shield);
        }
        else
            punctuation = "!";
    }
    else
        range_used = BEAM_STOP;

    if (needs_message)
    {
        mprf("%s %s %s%s",
             def_name(DESC_THE).c_str(),
             defender->conj_verb(verb).c_str(),
             projectile->name(DESC_THE).c_str(),
             punctuation.c_str());
    }

    return attack::handle_phase_blocked();
}

bool ranged_attack::handle_phase_dodged()
{
    did_hit = false;

    const int ev = defender->melee_evasion(attacker);

    const int orig_ev_margin =
        test_hit(orig_to_hit, ev, !attacker->is_player());

    if (defender->missile_deflection() && orig_ev_margin >= 0)
    {
        if (needs_message && defender_visible)
        {
            if (defender->missile_deflection() >= 2)
            {
                mprf("%s %s %s!",
                     defender->name(DESC_THE).c_str(),
                     defender->conj_verb("deflect").c_str(),
                     projectile->name(DESC_THE).c_str());
            }
            else
                mprf("%s is repelled.", projectile->name(DESC_THE).c_str());
        }

        return true;
    }

    const int ev_nophase = defender->melee_evasion(attacker,
                                                   EV_IGNORE_PHASESHIFT);
    if (ev_margin + (ev - ev_nophase) > 0)
    {
        if (needs_message && defender_visible)
        {
            mprf("%s momentarily %s out as %s "
                 "passes through %s%s",
                 defender->name(DESC_THE).c_str(),
                 defender->conj_verb("phase").c_str(),
                 projectile->name(DESC_THE).c_str(),
                 defender->pronoun(PRONOUN_OBJECTIVE).c_str(),
                 attack_strength_punctuation(damage_done).c_str());
        }

        return true;
    }

    if (needs_message)
    {
        mprf("%s%s misses %s%s",
             projectile->name(DESC_THE).c_str(),
             evasion_margin_adverb().c_str(),
             defender_name().c_str(),
             attack_strength_punctuation(damage_done).c_str());
    }

    return true;
}

bool ranged_attack::handle_phase_hit()
{
    // XXX: this kind of hijacks the shield block check
    if (!attack_ignores_shield(false))
        range_used = BEAM_STOP;

    if (projectile->base_type == OBJ_MISSILES
        && projectile->sub_type == MI_NEEDLE)
    {
        int dur = blowgun_duration_roll(get_ammo_brand(*projectile));
        set_attack_verb();
        int stab = player_stab(dur);
        damage_done = dur + (stab - dur) / 10;
        announce_hit();
    }
    else if (projectile->base_type == OBJ_MISSILES
             && projectile->sub_type == MI_THROWING_NET)
    {
        set_attack_verb();
        announce_hit();
        if (defender->is_player())
            player_caught_in_net();
        else
            monster_caught_in_net(defender->as_monster(), attacker);
    }
    else
    {
        damage_done = calc_damage();
        if (damage_done > 0
            || projectile->base_type == OBJ_MISSILES
               && projectile->sub_type == MI_NEEDLE)
        {
            if (!handle_phase_damaged())
                return false;
        }
        else if (needs_message)
        {
            mprf("%s %s %s but does no damage.",
                 projectile->name(DESC_THE).c_str(),
                 attack_verb.c_str(),
                 defender->name(DESC_THE).c_str());
        }
    }

    if (using_weapon()
        || is_launched(attacker, weapon, *projectile) == LRET_THROWN)
    {
        if (using_weapon()
            && apply_damage_brand(projectile->name(DESC_THE).c_str()))
        {
            return false;
        }
        if (apply_missile_brand())
            return false;
    }

    // XXX: unify this with melee_attack's code
    if (attacker->is_player() && defender->is_monster()
        && should_alert_defender)
    {
        behaviour_event(defender->as_monster(), ME_WHACK, attacker,
                        coord_def(), !stab_attempt);
    }

    return true;
}

bool ranged_attack::using_weapon()
{
    return weapon
           && is_launched(attacker, weapon, *projectile) == LRET_LAUNCHED;
}

int ranged_attack::weapon_damage()
{
    if (is_launched(attacker, weapon, *projectile) == LRET_FUMBLED)
        return 0;

    int dam = property(*projectile, PWPN_DAMAGE);
    if (projectile->base_type == OBJ_MISSILES
        && get_ammo_brand(*projectile) == SPMSL_STEEL)
    {
        dam = div_rand_round(dam * 13, 10);
    }
    if (using_weapon())
        dam += property(*weapon, PWPN_DAMAGE);
    else
        dam += calc_base_unarmed_damage();

    return dam;
}

/**
 * For ranged attacked, "unarmed" is throwing damage.
 */
int ranged_attack::calc_base_unarmed_damage()
{
    // No damage bonus for throwing non-throwing weapons.
    if (is_launched(attacker, weapon, *projectile) == LRET_FUMBLED)
        return 0;

    // Darts and stones get half bonus; everything else gets full bonus.
    return div_rand_round(attack::calc_base_unarmed_damage()
                          * min(4, property(*projectile, PWPN_DAMAGE)), 4);
}

int ranged_attack::calc_mon_to_hit_base()
{
    ASSERT(attacker->is_monster());
    const int hd_mult = attacker->as_monster()->is_archer() ? 15 : 9;
    return 18 + attacker->get_experience_level() * hd_mult / 6;
}

int ranged_attack::apply_damage_modifiers(int damage, int damage_max,
                                          bool &half_ac)
{
    ASSERT(attacker->is_monster());
    if (attacker->as_monster()->is_archer())
    {
        const int bonus = attacker->get_experience_level() * 4 / 3;
        damage += random2avg(bonus, 2);
    }
    half_ac = false;
    return damage;
}

bool ranged_attack::attack_ignores_shield(bool verbose)
{
    if (is_launched(attacker, weapon, *projectile) != LRET_FUMBLED
            && projectile->base_type == OBJ_MISSILES
            && get_ammo_brand(*projectile) == SPMSL_PENETRATION
        || using_weapon() && get_weapon_brand(*weapon) == SPWPN_PENETRATION)
    {
        if (verbose)
        {
            mprf("%s pierces through %s %s!",
                 projectile->name(DESC_THE).c_str(),
                 apostrophise(defender_name()).c_str(),
                 defender_shield ? defender_shield->name(DESC_PLAIN).c_str()
                                 : "shielding");
        }
        return true;
    }

    return false;
}

bool ranged_attack::apply_damage_brand(const char *what)
{
    if (!weapon || !is_range_weapon(*weapon))
        return false;

    const brand_type brand = get_weapon_brand(*weapon);

    // No stacking elemental brands, unless you're Nessos.
    if (attacker->type != MONS_NESSOS
        && projectile->base_type == OBJ_MISSILES
        && get_ammo_brand(*projectile) != SPMSL_NORMAL
        && (brand == SPWPN_FLAMING
            || brand == SPWPN_FREEZING
            || brand == SPWPN_HOLY_WRATH
            || brand == SPWPN_ELECTROCUTION
            || brand == SPWPN_VENOM
            || brand == SPWPN_CHAOS))
    {
        return false;
    }

    damage_brand = brand;
    return attack::apply_damage_brand(what);
}

special_missile_type ranged_attack::random_chaos_missile_brand()
{
    special_missile_type brand = SPMSL_NORMAL;
    // Assuming the chaos to be mildly intelligent, try to avoid brands
    // that clash with the most basic resists of the defender,
    // i.e. its holiness.
    while (true)
    {
        brand = (random_choose_weighted(
                    10, SPMSL_FLAME,
                    10, SPMSL_FROST,
                    10, SPMSL_POISONED,
                    10, SPMSL_CHAOS,
                     5, SPMSL_PARALYSIS,
                     5, SPMSL_SLOW,
                     5, SPMSL_SLEEP,
                     5, SPMSL_FRENZY,
                     2, SPMSL_CURARE,
                     2, SPMSL_CONFUSION,
                     2, SPMSL_DISPERSAL,
                     0));

        if (one_chance_in(3))
            break;

        bool susceptible = true;
        switch (brand)
        {
        case SPMSL_FLAME:
            if (defender->is_fiery())
                susceptible = false;
            break;
        case SPMSL_FROST:
            if (defender->is_icy())
                susceptible = false;
            break;
        case SPMSL_POISONED:
            if (defender->holiness() == MH_UNDEAD)
                susceptible = false;
            break;
        case SPMSL_DISPERSAL:
            if (defender->no_tele(true, false, true))
                susceptible = false;
            break;
        case SPMSL_CONFUSION:
            if (defender->holiness() == MH_PLANT)
            {
                susceptible = false;
                break;
            }
            // fall through
        case SPMSL_SLOW:
        case SPMSL_SLEEP:
        case SPMSL_PARALYSIS:
            if (defender->holiness() == MH_UNDEAD
                || defender->holiness() == MH_NONLIVING)
            {
                susceptible = false;
            }
            break;
        case SPMSL_FRENZY:
            if (defender->holiness() == MH_UNDEAD
                || defender->holiness() == MH_NONLIVING
                || defender->is_player()
                   && !you.can_go_berserk(false, false, false)
                || defender->is_monster()
                   && !defender->as_monster()->can_go_frenzy())
            {
                susceptible = false;
            }
            break;
        default:
            break;
        }

        if (susceptible)
            break;
    }
#ifdef NOTE_DEBUG_CHAOS_BRAND
    string brand_name = "CHAOS missile: ";
    switch (brand)
    {
    case SPMSL_NORMAL:          brand_name += "(plain)"; break;
    case SPMSL_FLAME:           brand_name += "flame"; break;
    case SPMSL_FROST:           brand_name += "frost"; break;
    case SPMSL_POISONED:        brand_name += "poisoned"; break;
    case SPMSL_CURARE:          brand_name += "curare"; break;
    case SPMSL_CHAOS:           brand_name += "chaos"; break;
    case SPMSL_DISPERSAL:       brand_name += "dispersal"; break;
    case SPMSL_SLOW:            brand_name += "slow"; break;
    case SPMSL_SLEEP:           brand_name += "sleep"; break;
    case SPMSL_CONFUSION:       brand_name += "confusion"; break;
    case SPMSL_FRENZY:          brand_name += "frenzy"; break;
    default:                    brand_name += "(other)"; break;
    }

    // Pretty much duplicated by the chaos effect note,
    // which will be much more informative.
    if (brand != SPMSL_CHAOS)
        take_note(Note(NOTE_MESSAGE, 0, 0, brand_name.c_str()), true);
#endif
    return brand;
}

bool ranged_attack::blowgun_check(special_missile_type type)
{
    if (defender->holiness() == MH_UNDEAD
        || defender->holiness() == MH_NONLIVING)
    {
        if (needs_message)
        {
            if (defender->is_monster())
            {
                simple_monster_message(defender->as_monster(),
                                       " is unaffected.");
            }
            else
                canned_msg(MSG_YOU_UNAFFECTED);
        }
        return false;
    }

    if (stab_attempt)
        return true;

    const int enchantment = using_weapon() ? weapon->plus : 0;

    if (attacker->is_monster())
    {
        int chance = 85 - ((defender->get_experience_level()
                            - attacker->get_experience_level()) * 5 / 2);
        chance += enchantment * 4;
        chance = min(95, chance);

        if (type == SPMSL_FRENZY)
            chance = chance / 2;
        else if (type == SPMSL_PARALYSIS || type == SPMSL_SLEEP)
            chance = chance * 4 / 5;

        return x_chance_in_y(chance, 100);
    }

    const int skill = you.skill_rdiv(SK_THROWING);

    // You have a really minor chance of hitting with no skills or good
    // enchants.
    if (defender->get_experience_level() < 15 && random2(100) <= 2)
        return true;

    const int resist_roll = 2 + random2(4 + skill + enchantment);

    dprf("Brand rolled %d against defender HD: %d.",
         resist_roll, defender->get_experience_level());

    if (resist_roll < defender->get_experience_level())
    {
        if (needs_message)
        {
            if (defender->is_monster())
                simple_monster_message(defender->as_monster(), " resists.");
            else
                canned_msg(MSG_YOU_RESIST);
        }
        return false;
    }

    return true;

}

int ranged_attack::blowgun_duration_roll(special_missile_type type)
{
    if (type == SPMSL_POISONED)
        return 6 + random2(8);

    if (type == SPMSL_CURARE)
        return 2;

    const int base_power = (attacker->is_monster())
                           ? attacker->get_experience_level()
                           : attacker->skill_rdiv(SK_THROWING);

    const int plus = using_weapon() ? weapon->plus : 0;

    // Scale down nastier needle effects against players.
    // Fixed duration regardless of power, since power already affects success
    // chance considerably, and this helps avoid effects being too nasty from
    // high HD shooters and too ignorable from low ones.
    if (defender->is_player())
    {
        switch (type)
        {
            case SPMSL_PARALYSIS:
                return 3 + random2(4);
            case SPMSL_SLEEP:
                return 5 + random2(5);
            case SPMSL_CONFUSION:
                return 2 + random2(4);
            case SPMSL_SLOW:
                return 5 + random2(7);
            default:
                return 5 + random2(5);
        }
    }
    else
        return 5 + random2(base_power + plus);
}

bool ranged_attack::apply_missile_brand()
{
    if (projectile->base_type != OBJ_MISSILES)
        return false;

    special_missile_type brand = get_ammo_brand(*projectile);
    if (brand == SPMSL_CHAOS)
        brand = random_chaos_missile_brand();

    switch (brand)
    {
    default:
        break;
    case SPMSL_FLAME:
        if (using_weapon()
            && get_weapon_brand(*weapon) == SPWPN_FREEZING)
        {
            break;
        }
        calc_elemental_brand_damage(BEAM_FIRE, defender->res_fire(),
                                    defender->is_icy() ? "melt" : "burn",
                                    projectile->name(DESC_THE).c_str());
        defender->expose_to_element(BEAM_FIRE);
        attacker->god_conduct(DID_FIRE, 2);
        break;
    case SPMSL_FROST:
        if (using_weapon()
            && get_weapon_brand(*weapon) == SPWPN_FLAMING)
        {
            break;
        }
        calc_elemental_brand_damage(BEAM_COLD, defender->res_fire(), "freeze",
                                    projectile->name(DESC_THE).c_str());
        defender->expose_to_element(BEAM_COLD, 2);
        break;
    case SPMSL_POISONED:
        if (stab_attempt
            || (projectile->base_type == OBJ_MISSILES
                && projectile->sub_type == MI_NEEDLE
                && using_weapon()
                && damage_done > 0)
            || !one_chance_in(4))
        {
            int old_poison;

            if (defender->is_player())
                old_poison = you.duration[DUR_POISONING];
            else
            {
                old_poison =
                    (defender->as_monster()->get_ench(ENCH_POISON)).degree;
            }

            defender->poison(attacker,
                             projectile->base_type == OBJ_MISSILES
                             && projectile->sub_type == MI_NEEDLE
                             ? damage_done
                             : 6 + random2(8) + random2(damage_done * 3 / 2));

            if (defender->is_player()
                   && old_poison < you.duration[DUR_POISONING]
                || !defender->is_player()
                   && old_poison <
                      (defender->as_monster()->get_ench(ENCH_POISON)).degree)
            {
                obvious_effect = true;
            }

        }
        break;
    case SPMSL_CURARE:
        obvious_effect = curare_actor(attacker, defender,
                                      damage_done,
                                      projectile->name(DESC_PLAIN),
                                      atk_name(DESC_PLAIN));
        break;
    case SPMSL_CHAOS:
        chaos_affects_defender();
        break;
    case SPMSL_DISPERSAL:
        if (damage_done > 0)
        {
            if (defender->no_tele(true, true))
            {
                if (defender->is_player())
                    canned_msg(MSG_STRANGE_STASIS);
            }
            else
            {
                coord_def pos, pos2;
                const bool no_sanct = defender->kill_alignment() == KC_OTHER;
                if (random_near_space(defender, defender->pos(), pos, false,
                                      no_sanct)
                    && random_near_space(defender, defender->pos(), pos2, false,
                                         no_sanct))
                {
                    const coord_def from = attacker->pos();
                    if (distance2(pos2, from) > distance2(pos, from))
                        pos = pos2;

                    if (defender->is_player())
                        defender->blink_to(pos);
                    else
                        defender->as_monster()->blink_to(pos, false, false);
                }
            }
        }
        break;
    case SPMSL_SILVER:
        special_damage = silver_damages_victim(defender, damage_done,
                                               special_damage_message);
        break;
    case SPMSL_PARALYSIS:
        if (!blowgun_check(brand))
            break;
        defender->paralyse(attacker, damage_done);
        break;
    case SPMSL_SLOW:
        if (!blowgun_check(brand))
            break;
        defender->slow_down(attacker, damage_done);
        break;
    case SPMSL_SLEEP:
        if (!blowgun_check(brand))
            break;
        defender->put_to_sleep(attacker, damage_done);
        should_alert_defender = false;
        break;
    case SPMSL_CONFUSION:
        if (!blowgun_check(brand))
            break;
        defender->confuse(attacker, damage_done);
        break;
    case SPMSL_FRENZY:
        if (!blowgun_check(brand))
            break;
        if (defender->is_monster())
        {
            monster* mon = defender->as_monster();
            // Wake up the monster so that it can frenzy.
            if (mon->behaviour == BEH_SLEEP)
                mon->behaviour = BEH_WANDER;
            mon->go_frenzy(attacker);
        }
        else
            defender->go_berserk(false);
        break;
    }

    if (needs_message && !special_damage_message.empty())
    {
        mprf("%s", special_damage_message.c_str());

        special_damage_message.clear();
        // Don't do message-only miscasts along with a special
        // damage message.
        if (miscast_level == 0)
            miscast_level = -1;
    }

    if (special_damage > 0)
        inflict_damage(special_damage, special_damage_flavour);

    return !defender->alive();
}

bool ranged_attack::check_unrand_effects()
{
    return false;
}

bool ranged_attack::mons_attack_effects()
{
    return true;
}

void ranged_attack::player_stab_check()
{
    if (player_stab_tier() > 0)
    {
        attack::player_stab_check();
        // Sometimes the blowgun of the Assassin lets you stab an aware target.
        if (!stab_attempt && is_unrandom_artefact(*weapon)
            && weapon->special == UNRAND_BLOWGUN_ASSASSIN
            && one_chance_in(3))
        {
            stab_attempt = true;
            stab_bonus = 1;
        }
    }
    else
    {
        stab_attempt = false;
        stab_bonus = 0;
    }
}

int ranged_attack::player_stab_tier()
{
    if (using_weapon()
        && projectile->base_type == OBJ_MISSILES
        && projectile->sub_type == MI_NEEDLE)
    {
        return 2;
    }

    return 0;
}

void ranged_attack::adjust_noise()
{
}

void ranged_attack::set_attack_verb()
{
    attack_verb = attack_ignores_shield(false) ? "pierces through" : "hits";
}

void ranged_attack::announce_hit()
{
    if (!needs_message)
        return;

    mprf("%s %s %s%s%s%s",
         projectile->name(DESC_THE).c_str(),
         attack_verb.c_str(),
         defender_name().c_str(),
         damage_done > 0 && stab_attempt && stab_bonus > 0
             ? " in a vulnerable spot"
             : "",
         debug_damage_number().c_str(),
         attack_strength_punctuation(damage_done).c_str());
}
