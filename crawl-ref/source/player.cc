/**
 * @file
 * @brief Player related functions.
**/

#include "AppHdr.h"

#include "player.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include <sstream>
#include <algorithm>

#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "branch.h"
#ifdef DGL_WHEREIS
 #include "chardump.h"
#endif
#include "cloud.h"
#include "clua.h"
#include "coord.h"
#include "coordit.h"
#include "delay.h"
#include "directn.h"
#include "effects.h"
#include "env.h"
#include "errors.h"
#include "exercise.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "godpassive.h"
#include "godwrath.h"
#include "hints.h"
#include "hiscores.h"
#include "invent.h"
#include "item_use.h"
#include "itemname.h"
#include "itemprop.h"
#include "items.h"
#include "kills.h"
#include "libutil.h"
#include "macro.h"
#include "melee_attack.h"
#include "message.h"
#include "misc.h"
#include "mon-stuff.h"
#include "mon-util.h"
#include "mutation.h"
#include "notes.h"
#include "options.h"
#include "ouch.h"
#include "output.h"
#include "player-stats.h"
#include "potion.h"
#include "quiver.h"
#include "random.h"
#include "religion.h"
#include "shopping.h"
#include "shout.h"
#include "skills.h"
#include "skills2.h"
#include "species.h"
#include "spl-damage.h"
#include "spl-other.h"
#include "spl-selfench.h"
#include "spl-transloc.h"
#include "spl-util.h"
#include "sprint.h"
#include "stairs.h"
#include "stash.h"
#include "state.h"
#include "status.h"
#include "stuff.h"
#include "terrain.h"
#include "throw.h"
#ifdef USE_TILE
 #include "tileview.h"
#endif
#include "transform.h"
#include "traps.h"
#include "travel.h"
#include "view.h"
#include "viewgeom.h"
#include "xom.h"

static void _moveto_maybe_repel_stairs()
{
    const dungeon_feature_type new_grid = env.grid(you.pos());
    const command_type stair_dir = feat_stair_direction(new_grid);

    if (stair_dir == CMD_NO_CMD
        || new_grid == DNGN_ENTER_SHOP
        ||  !you.duration[DUR_REPEL_STAIRS_MOVE])
    {
        return;
    }

    int pct = you.duration[DUR_REPEL_STAIRS_CLIMB] ? 29 : 50;

    // When the effect is still strong, the chance to actually catch
    // a stair is smaller. (Assuming the duration starts out at 1000.)
    const int dur = max(0, you.duration[DUR_REPEL_STAIRS_MOVE] - 700);
    pct += dur/10;

    if (x_chance_in_y(pct, 100))
    {
        if (slide_feature_over(you.pos(), coord_def(-1, -1), false))
        {
            string stair_str = feature_description_at(you.pos(), false,
                                                      DESC_THE, false);
            string prep = feat_preposition(new_grid, true, &you);

            mprf("%s slides away as you move %s it!", stair_str.c_str(),
                 prep.c_str());

            if (player_in_a_dangerous_place() && one_chance_in(5))
                xom_is_stimulated(25);
        }
    }
}

bool check_moveto_cloud(const coord_def& p, const string &move_verb,
                        bool *prompted)
{
    const int cloud = env.cgrid(p);
    if (cloud != EMPTY_CLOUD && !you.confused())
    {
        const cloud_type ctype = env.cloud[ cloud ].type;
        // Don't prompt if already in a cloud of the same type.
        if (is_damaging_cloud(ctype, true)
            && (env.cgrid(you.pos()) == EMPTY_CLOUD
                || ctype != env.cloud[ env.cgrid(you.pos()) ].type)
            && (!YOU_KILL(env.cloud[ cloud ].killer)
                || !you_worship(GOD_QAZLAL)
                || player_under_penance())
            && !crawl_state.disables[DIS_CONFIRMATIONS])
        {
            // Don't prompt for steam unless we're at uncomfortably low hp.
            if (ctype == CLOUD_STEAM)
            {
                int threshold = 20;
                if (player_res_steam() < 0)
                    threshold = threshold * 3 / 2;
                threshold = threshold * you.time_taken / BASELINE_DELAY;
                // Do prompt if we'd lose icemail, though.
                if (you.hp > threshold && !you.mutation[MUT_ICEMAIL])
                    return true;
            }

            if (prompted)
                *prompted = true;
            string prompt = make_stringf("Really %s into that cloud of %s?",
                                         move_verb.c_str(),
                                         cloud_name_at_index(cloud).c_str());
            learned_something_new(HINT_CLOUD_WARNING);

            if (!yesno(prompt.c_str(), false, 'n'))
            {
                canned_msg(MSG_OK);
                return false;
            }
        }
    }
    return true;
}

bool check_moveto_trap(const coord_def& p, const string &move_verb,
                       bool *prompted)
{
    // If there's no trap, let's go.
    trap_def* trap = find_trap(p);
    if (!trap || env.grid(p) == DNGN_UNDISCOVERED_TRAP)
        return true;

    if (trap->type == TRAP_ZOT && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string msg = (move_verb == "jump-attack"
                      ? "Do you really want to %s when you might land in "
                      "the Zot trap?"
                      : "Do you really want to %s into the Zot trap?");
        string prompt = make_stringf(msg.c_str(), move_verb.c_str());

        if (prompted)
            *prompted = true;
        if (!yes_or_no("%s", prompt.c_str()))
        {
            canned_msg(MSG_OK);
                return false;
        }
    }
    else if (!trap->is_safe() && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt;

        if (prompted)
            *prompted = true;
        if (move_verb == "jump-attack")
        {
            prompt = make_stringf("Really jump when you might land on that %s?",
                                  feature_description_at(p, false,
                                                         DESC_BASENAME,
                                                         false).c_str());
        }
        else
        {
            prompt = make_stringf("Really %s %s that %s?",
                                  move_verb.c_str(),
                                  (trap->type == TRAP_ALARM
                                   || trap->type == TRAP_PLATE) ? "onto"
                                  : "into",
                                  feature_description_at(p, false,
                                                         DESC_BASENAME,
                                                         false).c_str());
        }
        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

static bool _check_moveto_dangerous(const coord_def& p, const string& msg)
{
    if (you.can_swim() && feat_is_water(env.grid(p))
        || you.airborne() || !is_feat_dangerous(env.grid(p)))
    {
        return true;
    }

    if (msg != "")
        mpr(msg.c_str());
    else if (species_likes_water(you.species) && feat_is_water(env.grid(p)))
        mpr("You cannot enter water in your current form.");
    else if (species_likes_lava(you.species) && feat_is_lava(env.grid(p)))
        mpr("You cannot enter lava in your current form.");
    else
        canned_msg(MSG_UNTHINKING_ACT);
    return false;
}

bool check_moveto_terrain(const coord_def& p, const string &move_verb,
                          const string &msg, bool *prompted)
{
    if (!_check_moveto_dangerous(p, msg))
        return false
;
    if (!need_expiration_warning() && need_expiration_warning(p)
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        string prompt;

        if (prompted)
            *prompted = true;

        if (msg != "")
            prompt = msg + " ";

        prompt += "Are you sure you want to " + move_verb;

        if (you.ground_level())
            prompt += " into ";
        else
            prompt += " over ";

        prompt += env.grid(p) == DNGN_DEEP_WATER ? "deep water" : "lava";

        prompt += need_expiration_warning(DUR_FLIGHT, p)
            ? " while you are losing your buoyancy?"
            : " while your transformation is expiring?";

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

bool check_moveto_exclusion(const coord_def& p, const string &move_verb,
                            bool *prompted)
{
    string prompt;

    if (is_excluded(p)
        && !is_stair_exclusion(p)
        && !is_excluded(you.pos())
        && !crawl_state.disables[DIS_CONFIRMATIONS])
    {
        if (prompted)
            *prompted = true;
        prompt = make_stringf("Really %s into a travel-excluded area?",
                              move_verb.c_str());

        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }
    }
    return true;
}

bool check_moveto(const coord_def& p, const string &move_verb, const string &msg)
{
    return check_moveto_terrain(p, move_verb, msg)
           && check_moveto_cloud(p, move_verb)
           && check_moveto_trap(p, move_verb)
           && check_moveto_exclusion(p, move_verb);
}

static void _splash()
{
    if (you.can_swim())
        noisy(4, you.pos(), "Floosh!");
    else if (!beogh_water_walk())
        noisy(8, you.pos(), "Splash!");
}

void moveto_location_effects(dungeon_feature_type old_feat,
                             bool stepped, const coord_def& old_pos)
{
    const dungeon_feature_type new_grid = env.grid(you.pos());

    // Terrain effects.
    if (is_feat_dangerous(new_grid))
    {
        // Lava and dangerous deep water (ie not merfolk).
        const coord_def& entry = (stepped) ? old_pos : you.pos();

        // If true, we were shifted and so we're done.
        if (fall_into_a_pool(entry, new_grid))
            return;
    }

    if (you.ground_level())
    {
        if (player_likes_lava(false))
        {
            if (feat_is_lava(new_grid) && !feat_is_lava(old_feat))
            {
                if (!stepped)
                    noisy(4, you.pos(), "Gloop!");

                mprf("You %s lava.",
                     (stepped) ? "slowly immerse yourself in the" : "fall into the");

                // Extra time if you stepped in.
                if (stepped)
                    you.time_taken *= 2;
#if TAG_MAJOR_VERSION == 34
                // This gets called here because otherwise you wouldn't heat
                // until your second turn in lava.
                if (temperature() < TEMP_FIRE)
                    mpr("The lava instantly superheats you.");
                you.temperature = TEMP_MAX;
#endif
            }

            else if (!feat_is_lava(new_grid) && feat_is_lava(old_feat))
            {
                mpr("You slowly pull yourself out of the lava.");
                you.time_taken *= 2;
            }
        }

        if (you.species == SP_MERFOLK)
        {
            if (feat_is_water(new_grid) // We're entering water
                // We're not transformed, or with a form compatible with tail
                && (you.form == TRAN_NONE
                    || you.form == TRAN_APPENDAGE
                    || you.form == TRAN_BLADE_HANDS))
            {
                merfolk_start_swimming(stepped);
            }
            else if (!feat_is_water(new_grid) && !is_feat_dangerous(new_grid))
                merfolk_stop_swimming();
        }

        if (feat_is_water(new_grid) && !stepped)
            _splash();

        if (feat_is_water(new_grid) && !you.can_swim() && !beogh_water_walk())
        {
            if (stepped)
            {
                you.time_taken *= 13 + random2(8);
                you.time_taken /= 10;
            }

            if (!feat_is_water(old_feat))
            {
                mprf("You %s the %s water.",
                     stepped ? "enter" : "fall into",
                     new_grid == DNGN_SHALLOW_WATER ? "shallow" : "deep");
            }

            if (new_grid == DNGN_DEEP_WATER && old_feat != DNGN_DEEP_WATER)
                mpr("You sink to the bottom.");

            if (!feat_is_water(old_feat))
            {
                mpr("Moving in this stuff is going to be slow.");
                if (you.invisible())
                    mpr("...and don't expect to remain undetected.");
            }
        }
    }

    // Traps go off.
    if (trap_def* ptrap = find_trap(you.pos()))
        ptrap->trigger(you, !stepped); // blinking makes it hard to evade

    if (stepped)
        _moveto_maybe_repel_stairs();
}

// Use this function whenever the player enters (or lands and thus re-enters)
// a grid.
//
// stepped     - normal walking moves
void move_player_to_grid(const coord_def& p, bool stepped)
{
    ASSERT(!crawl_state.game_is_arena());
    ASSERT_IN_BOUNDS(p);

    if (!stepped)
        tornado_move(p);

    // assuming that entering the same square means coming from above (flight)
    const coord_def old_pos = you.pos();
    const bool from_above = (old_pos == p);
    const dungeon_feature_type old_grid =
        (from_above) ? DNGN_FLOOR : grd(old_pos);

    // Really must be clear.
    ASSERT(you.can_pass_through_feat(grd(p)));

    // Better not be an unsubmerged monster either.
    ASSERT(!monster_at(p) || monster_at(p)->submerged()
           || fedhas_passthrough(monster_at(p))
           || mons_is_player_shadow(monster_at(p)));

    // Move the player to new location.
    you.moveto(p, true);
    viewwindow();

    moveto_location_effects(old_grid, stepped, old_pos);
}

bool is_feat_dangerous(dungeon_feature_type grid, bool permanently,
                       bool ignore_flight)
{
    if (!ignore_flight
        && (you.permanent_flight() || you.airborne() && !permanently))
    {
        return false;
    }
    else if (grid == DNGN_DEEP_WATER && !player_likes_water(permanently)
             || grid == DNGN_LAVA && !player_likes_lava(permanently))
    {
        return true;
    }
    else
        return false;
}

bool is_map_persistent()
{
    return !testbits(env.level_flags, LFLAG_NO_MAP);
}

bool player_in_hell()
{
    return is_hell_subbranch(you.where_are_you);
}

bool player_in_connected_branch()
{
    return is_connected_branch(you.where_are_you);
}

bool player_likes_water(bool permanently)
{
    return !permanently && beogh_water_walk()
           || (species_likes_water(you.species) || !permanently)
               && form_likes_water();
}

bool player_likes_lava(bool permanently)
{
    return (species_likes_lava(you.species) || !permanently)
           && form_likes_lava();
}

bool player_can_open_doors()
{
    return you.form != TRAN_BAT;
}

// TODO: get rid of this.
bool player_genus(genus_type which_genus, species_type species)
{
    if (species == SP_UNKNOWN)
        species = you.species;

    return species_genus(species) == which_genus;
}

// If transform is true, compare with current transformation instead
// of (or in addition to) underlying species.
// (See mon-data.h for species/genus use.)
bool is_player_same_genus(const monster_type mon, bool transform)
{
    if (transform)
    {
        switch (you.form)
        {
        // Unique monsters.
        case TRAN_BAT:
            return mon == MONS_BAT;
        case TRAN_ICE_BEAST:
            return mon == MONS_ICE_BEAST;
        case TRAN_TREE:
            return mon == MONS_ANIMATED_TREE;
        case TRAN_PORCUPINE:
            return mon == MONS_PORCUPINE;
        case TRAN_WISP:
            return mon == MONS_INSUBSTANTIAL_WISP;
        case TRAN_SHADOW:
            return mons_genus(mon) == MONS_SHADOW;
        // Compare with monster *species*.
        case TRAN_LICH:
            return mons_species(mon) == MONS_LICH;
        // Compare with monster *genus*.
        case TRAN_FUNGUS:
            return mons_genus(mon) == MONS_FUNGUS;
        case TRAN_SPIDER:
            return mons_genus(mon) == MONS_SPIDER;
        case TRAN_DRAGON:
            return mons_genus(mon) == MONS_DRAGON;
        case TRAN_PIG:
            return mons_genus(mon) == MONS_HOG;
#if TAG_MAJOR_VERSION == 34
        case TRAN_JELLY:
            return mons_genus(mon) == MONS_JELLY;
#endif
        case TRAN_STATUE:
        case TRAN_BLADE_HANDS:
        case TRAN_NONE:
        case TRAN_APPENDAGE:
            break; // Check real (non-transformed) form.
        }
    }

    // Genus would include necrophage and rotting hulk.
    if (you.species == SP_GHOUL)
        return mons_species(mon) == MONS_GHOUL;

    // Note that these are currently considered to be the same genus:
    // * humans, demigods, and demonspawn
    // * ogres and two-headed ogres
    // * trolls, iron trolls, and deep trolls
    // * kobolds and big kobolds
    // * dwarves and deep dwarves
    // * all elf races
    // * all orc races
    return mons_genus(mon) == mons_genus(player_mons(false));
}

void update_player_symbol()
{
    you.symbol = Options.show_player_species ? player_mons() : transform_mons();
}

monster_type player_mons(bool transform)
{
    monster_type mons;

    if (transform)
    {
        mons = transform_mons();
        if (mons != MONS_PLAYER)
            return mons;
    }

    mons = player_species_to_mons_species(you.species);

    if (mons == MONS_ORC)
    {
        if (you_worship(GOD_BEOGH))
        {
            mons = (you.piety >= piety_breakpoint(4)) ? MONS_ORC_HIGH_PRIEST
                                                      : MONS_ORC_PRIEST;
        }
    }
    else if (mons == MONS_OGRE)
    {
        const skill_type sk = best_skill(SK_FIRST_SKILL, SK_LAST_SKILL);
        if (sk >= SK_SPELLCASTING && sk < SK_INVOCATIONS)
            mons = MONS_OGRE_MAGE;
    }

    return mons;
}

void update_vision_range()
{
    you.normal_vision = LOS_RADIUS;
    int nom   = 1;
    int denom = 1;

    // Nightstalker gives -1/-2/-3.
    if (player_mutation_level(MUT_NIGHTSTALKER))
    {
        nom *= LOS_RADIUS - player_mutation_level(MUT_NIGHTSTALKER);
        denom *= LOS_RADIUS;
    }

    // Lantern of shadows.
    if (you.attribute[ATTR_SHADOWS])
        nom *= 3, denom *= 4;

    // the Darkness spell.
    if (you.duration[DUR_DARKNESS])
        nom *= 3, denom *= 4;

    // robe of Night.
    if (player_equip_unrand(UNRAND_NIGHT))
        nom *= 3, denom *= 4;

    you.current_vision = (you.normal_vision * nom + denom / 2) / denom;
    ASSERT(you.current_vision > 0);
    set_los_radius(you.current_vision);
}

// Checks whether the player's current species can
// use (usually wear) a given piece of equipment.
// Note that EQ_BODY_ARMOUR and EQ_HELMET only check
// the ill-fitting variant (i.e., not caps and robes).
// If special_armour is set to true, special cases
// such as bardings, light armour and caps are
// considered. Otherwise, these simply return false.
// ---------------------------------------------------
bool you_can_wear(int eq, bool special_armour)
{
    // Amulet provides another slot
    if (eq == EQ_RING_AMULET && player_equip_unrand(UNRAND_FINGER_AMULET))
        return true;

    if (you.species == SP_FELID)
        return eq == EQ_LEFT_RING || eq == EQ_RIGHT_RING || eq == EQ_AMULET;

    // Octopodes can wear soft helmets, eight rings, and an amulet.
    if (you.species == SP_OCTOPODE)
    {
        if (special_armour && eq == EQ_HELMET)
            return true;
        else
            return eq >= EQ_RING_ONE && eq <= EQ_RING_EIGHT
                   || eq == EQ_AMULET || eq == EQ_SHIELD || eq == EQ_WEAPON;
    }

    switch (eq)
    {
    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_AMULET:
    case EQ_CLOAK:
        return true;

    case EQ_GLOVES:
        if (player_mutation_level(MUT_CLAWS, false) == 3)
            return false;
        // These species cannot wear gloves.
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE)
        {
            return false;
        }
        return true;

    case EQ_BOOTS:
        // Bardings.
        if (you.species == SP_NAGA || you.species == SP_CENTAUR)
            return special_armour;
        if (player_mutation_level(MUT_HOOVES, false) == 3
            || player_mutation_level(MUT_TALONS, false) == 3)
        {
            return false;
        }
        // These species cannot wear boots.
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
#if TAG_MAJOR_VERSION == 34
            || you.species == SP_DJINNI
#endif
            || you.species == SP_OGRE)
        {
            return false;
        }
        return true;

    case EQ_BODY_ARMOUR:
        if (player_genus(GENPC_DRACONIAN))
            return false;

    case EQ_SHIELD:
        // Most races can wear robes or a buckler/shield.
        if (special_armour)
            return true;
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE)
        {
            return false;
        }
        return true;

    case EQ_HELMET:
        // No caps or hats with Horns 3 or Antennae 3.
        if (player_mutation_level(MUT_HORNS, false) == 3
            || player_mutation_level(MUT_ANTENNAE, false) == 3)
        {
            return false;
        }
        // Anyone else can wear caps.
        if (special_armour)
            return true;
        if (player_mutation_level(MUT_HORNS, false)
            || player_mutation_level(MUT_BEAK, false)
            || player_mutation_level(MUT_ANTENNAE, false))
        {
            return false;
        }
        if (you.species == SP_TROLL
            || you.species == SP_SPRIGGAN
            || you.species == SP_OGRE
            || player_genus(GENPC_DRACONIAN))
        {
            return false;
        }
        return true;

    case EQ_WEAPON:
    case EQ_STAFF:
        return true; // kittehs were handled earlier

    default:
        return false;
    }
}

bool player_has_feet(bool temp)
{
    if (you.species == SP_NAGA
        || you.species == SP_FELID
        || you.species == SP_OCTOPODE
#if TAG_MAJOR_VERSION == 34
        || you.species == SP_DJINNI
#endif
        || you.fishtail && temp)
    {
        return false;
    }

    if (player_mutation_level(MUT_HOOVES, temp) == 3
        || player_mutation_level(MUT_TALONS, temp) == 3)
    {
        return false;
    }

    return true;
}

bool player_wearing_slot(int eq)
{
    ASSERT(you.equip[eq] != -1 || !you.melded[eq]);
    return you.equip[eq] != -1 && !you.melded[eq];
}

bool you_tran_can_wear(const item_def &item)
{
    switch (item.base_type)
    {
    case OBJ_WEAPONS:
        return you_tran_can_wear(EQ_WEAPON);

    case OBJ_JEWELLERY:
        return you_tran_can_wear(jewellery_is_amulet(item) ? EQ_AMULET
                                                           : EQ_RINGS);
    case OBJ_ARMOUR:
        if (item.sub_type == ARM_NAGA_BARDING)
            return you.species == SP_NAGA && you_tran_can_wear(EQ_BOOTS);
        else if (item.sub_type == ARM_CENTAUR_BARDING)
            return you.species == SP_CENTAUR && you_tran_can_wear(EQ_BOOTS);

        if (fit_armour_size(item, you.body_size()) != 0)
            return false;

        return you_tran_can_wear(get_armour_slot(item), true);

    default:
        return true;
    }
}

bool you_tran_can_wear(int eq, bool check_mutation)
{
    if (eq == EQ_NONE)
        return true;

    if (you.form == TRAN_PORCUPINE
#if TAG_MAJOR_VERSION == 34
        || you.form == TRAN_JELLY
#endif
        || you.form == TRAN_WISP)
    {
        return false;
    }

    if (eq == EQ_STAFF)
        eq = EQ_WEAPON;
    else if (eq >= EQ_RINGS && eq <= EQ_RINGS_PLUS2)
        eq = EQ_RINGS;

    // Everybody but porcupines and wisps can wear at least some type of armour.
    if (eq == EQ_ALL_ARMOUR)
        return true;

    // Not a transformation, but also temporary -> check first.
    if (check_mutation)
    {
        if (eq == EQ_GLOVES && you.has_claws(false) == 3)
            return false;

        if (eq == EQ_HELMET && player_mutation_level(MUT_HORNS) == 3)
            return false;

        if (eq == EQ_HELMET && player_mutation_level(MUT_ANTENNAE) == 3)
            return false;

        if (eq == EQ_BOOTS
            && (you.fishtail
                || player_mutation_level(MUT_HOOVES) == 3
                || you.has_talons(false) == 3))
        {
            return false;
        }
    }

    // No further restrictions.
    if (you.form == TRAN_NONE
        || you.form == TRAN_LICH
        || you.form == TRAN_APPENDAGE)
    {
        return true;
    }

    // Bats and pigs cannot wear anything except amulets.
    if ((you.form == TRAN_BAT || you.form == TRAN_PIG) && eq != EQ_AMULET)
        return false;

    // Everyone else can wear jewellery...
    if (eq == EQ_AMULET || eq == EQ_RINGS
        || eq == EQ_LEFT_RING || eq == EQ_RIGHT_RING
        || eq == EQ_RING_ONE || eq == EQ_RING_TWO
        || eq == EQ_RING_AMULET)
    {
        return true;
    }

    // ...but not necessarily in all slots.
    if (eq >= EQ_RING_THREE && eq <= EQ_RING_EIGHT)
    {
        return you.species == SP_OCTOPODE
               && (form_keeps_mutations() || you.form == TRAN_SPIDER);
    }

    // These cannot use anything but jewellery.
    if (you.form == TRAN_SPIDER || you.form == TRAN_DRAGON)
        return false;

    if (you.form == TRAN_BLADE_HANDS)
    {
        if (eq == EQ_WEAPON || eq == EQ_GLOVES || eq == EQ_SHIELD)
            return false;
        return true;
    }

    if (you.form == TRAN_ICE_BEAST)
    {
        if (eq != EQ_CLOAK)
            return false;
        return true;
    }

    if (you.form == TRAN_STATUE)
    {
        if (eq == EQ_WEAPON || eq == EQ_SHIELD
            || eq == EQ_CLOAK || eq == EQ_HELMET)
        {
            return true;
        }
        return false;
    }

    if (you.form == TRAN_FUNGUS)
        return eq == EQ_HELMET;

    if (you.form == TRAN_TREE)
        return eq == EQ_WEAPON || eq == EQ_SHIELD || eq == EQ_HELMET;

    return true;
}

bool player_weapon_wielded()
{
    if (you.melded[EQ_WEAPON])
        return false;

    const int wpn = you.equip[EQ_WEAPON];

    if (wpn == -1)
        return false;

    if (!is_weapon(you.inv[wpn]))
        return false;

    return true;
}

// Returns false if the player is wielding a weapon inappropriate for Berserk.
bool berserk_check_wielded_weapon()
{
    const item_def * const wpn = you.weapon();
    if (wpn && (wpn->defined() && (!is_melee_weapon(*wpn)
                                   || needs_handle_warning(*wpn, OPER_ATTACK))
                || you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED]))
    {
        string prompt = "Do you really want to go berserk while wielding "
                        + wpn->name(DESC_YOUR) + "?";

        if (!yesno(prompt.c_str(), true, 'n'))
        {
            canned_msg(MSG_OK);
            return false;
        }

        you.attribute[ATTR_WEAPON_SWAP_INTERRUPTED] = 0;
    }

    return true;
}

// Looks in equipment "slot" to see if there is an equipped "sub_type".
// Returns number of matches (in the case of rings, both are checked)
int player::wearing(equipment_type slot, int sub_type, bool calc_unid) const
{
    int ret = 0;

    const item_def* item;

    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if (weapon()
            && weapon()->base_type == OBJ_WEAPONS
            && weapon()->sub_type == sub_type)
        {
            ret++;
        }
        break;

    case EQ_STAFF:
        // Like above, but must be magical staff.
        if (weapon()
            && weapon()->base_type == OBJ_STAVES
            && weapon()->sub_type == sub_type
            && (calc_unid || item_type_known(*weapon())))
        {
            ret++;
        }
        break;

    case EQ_RINGS:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; slots++)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = slot_item(static_cast<equipment_type>(slots)))
                && item->sub_type == sub_type
                && (calc_unid
                    || item_type_known(*item)))
            {
                ret++;
            }
        }
        break;

    case EQ_RINGS_PLUS:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; slots++)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = slot_item(static_cast<equipment_type>(slots)))
                && item->sub_type == sub_type
                && (calc_unid
                    || item_type_known(*item)))
            {
                ret += item->plus;
            }
        }
        break;

    case EQ_RINGS_PLUS2:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; ++slots)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = slot_item(static_cast<equipment_type>(slots)))
                && item->sub_type == sub_type
                && (calc_unid
                    || item_type_known(*item)))
            {
                ret += item->plus2;
            }
        }
        break;

    case EQ_ALL_ARMOUR:
        // Doesn't make much sense here... be specific. -- bwr
        die("EQ_ALL_ARMOUR is not a proper slot");
        break;

    default:
        if (! (slot > EQ_NONE && slot < NUM_EQUIP))
            die("invalid slot");
        if ((item = slot_item(slot))
            && item->sub_type == sub_type
            && (calc_unid || item_type_known(*item)))
        {
            ret++;
        }
        break;
    }

    return ret;
}

// Looks in equipment "slot" to see if equipped item has "special" ego-type
// Returns number of matches (jewellery returns zero -- no ego type).
// [ds] There's no equivalent of calc_unid or req_id because as of now, weapons
// and armour type-id on wield/wear.
int player::wearing_ego(equipment_type slot, int special, bool calc_unid) const
{
    int ret = 0;

    const item_def* item;
    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if ((item = slot_item(EQ_WEAPON))
            && item->base_type == OBJ_WEAPONS
            && get_weapon_brand(*item) == special)
        {
            ret++;
        }
        break;

    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_AMULET:
    case EQ_STAFF:
    case EQ_RINGS:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
        // no ego types for these slots
        break;

    case EQ_ALL_ARMOUR:
        // Check all armour slots:
        for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_ARMOUR; i++)
        {
            if ((item = slot_item(static_cast<equipment_type>(i)))
                && get_armour_ego_type(*item) == special
                && (calc_unid || item_type_known(*item)))
            {
                ret++;
            }
        }
        break;

    default:
        if (slot < EQ_MIN_ARMOUR || slot > EQ_MAX_ARMOUR)
            die("invalid slot: %d", slot);
        // Check a specific armour slot for an ego type:
        if ((item = slot_item(static_cast<equipment_type>(slot)))
            && get_armour_ego_type(*item) == special
            && (calc_unid || item_type_known(*item)))
        {
            ret++;
        }
        break;
    }

    return ret;
}

// Returns true if the indicated unrandart is equipped
// [ds] There's no equivalent of calc_unid or req_id because as of now, weapons
// and armour type-id on wield/wear.
bool player_equip_unrand(int unrand_index)
{
    const unrandart_entry* entry = get_unrand_entry(unrand_index);
    equipment_type   slot  = get_item_slot(entry->base_type,
                                           entry->sub_type);

    item_def* item;

    switch (slot)
    {
    case EQ_WEAPON:
        // Hands can have more than just weapons.
        if ((item = you.slot_item(slot))
            && item->base_type == OBJ_WEAPONS
            && is_unrandom_artefact(*item)
            && item->special == unrand_index)
        {
            return true;
        }
        break;

    case EQ_RINGS:
        for (int slots = EQ_LEFT_RING; slots < NUM_EQUIP; ++slots)
        {
            if (slots == EQ_AMULET)
                continue;

            if ((item = you.slot_item(static_cast<equipment_type>(slots)))
                && is_unrandom_artefact(*item)
                && item->special == unrand_index)
            {
                return true;
            }
        }
        break;

    case EQ_NONE:
    case EQ_STAFF:
    case EQ_LEFT_RING:
    case EQ_RIGHT_RING:
    case EQ_RINGS_PLUS:
    case EQ_RINGS_PLUS2:
    case EQ_ALL_ARMOUR:
        // no unrandarts for these slots.
        break;

    default:
        if (slot <= EQ_NONE || slot >= NUM_EQUIP)
            die("invalid slot: %d", slot);
        // Check a specific slot.
        if ((item = you.slot_item(slot))
            && is_unrandom_artefact(*item)
            && item->special == unrand_index)
        {
            return true;
        }
        break;
    }

    return false;
}

// Given an adjacent monster, returns true if the player can hit it (the
// monster should not be submerged, or be submerged in shallow water if
// the player has a polearm).
bool player_can_hit_monster(const monster* mon)
{
    if (!mon->submerged())
        return true;

    if (grd(mon->pos()) != DNGN_SHALLOW_WATER)
        return false;

    const item_def *weapon = you.weapon();
    return weapon && weapon_skill(*weapon) == SK_POLEARMS;
}

bool player_can_hear(const coord_def& p, int hear_distance)
{
    return !silenced(p)
           && !silenced(you.pos())
           && you.pos().distance_from(p) <= hear_distance;
}

int player_teleport(bool calc_unid)
{
    ASSERT(!crawl_state.game_is_arena());

    // Don't allow any form of teleportation in Sprint.
    if (crawl_state.game_is_sprint())
        return 0;

    // Short-circuit rings of teleport to prevent spam.
    if (you.species == SP_FORMICID)
        return 0;

    int tp = 0;

    // rings (keep in sync with _equip_jewellery_effect)
    tp += 8 * you.wearing(EQ_RINGS, RING_TELEPORTATION, calc_unid);

    // randart weapons only
    if (you.weapon()
        && you.weapon()->base_type == OBJ_WEAPONS
        && is_artefact(*you.weapon()))
    {
        tp += you.scan_artefacts(ARTP_CAUSE_TELEPORTATION, calc_unid);
    }

    // mutations
    tp += player_mutation_level(MUT_TELEPORT) * 3;

    return tp;
}

// Computes bonuses to regeneration from most sources. Does not handle
// slow healing, vampireness, or Trog's Hand.
static int _player_bonus_regen()
{
    int rr = 0;

    // Trog's Hand is handled separately so that it will bypass slow healing,
    // and it overrides the spell.
    if (you.duration[DUR_REGENERATION]
        && !you.duration[DUR_TROGS_HAND])
    {
        rr += 100;
    }

    // Rings.
    rr += 40 * you.wearing(EQ_RINGS, RING_REGENERATION);

    // Artefacts
    rr += you.scan_artefacts(ARTP_REGENERATION);

    // Troll leather (except for trolls).
    if ((you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_LEATHER_ARMOUR)
         || you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_HIDE))
        && you.species != SP_TROLL)
    {
        rr += 40;
    }

    // Fast heal mutation.
    rr += player_mutation_level(MUT_REGENERATION) * 20;

    // Powered By Death mutation, boosts regen by 10 per corpse in
    // a mutation_level * 3 (3/6/9) radius, to a maximum of 7
    // corpses.  If and only if the duration of the effect is
    // still active.
    if (you.duration[DUR_POWERED_BY_DEATH])
        rr += handle_pbd_corpses() * 100;

    return rr;
}

// Slow healing mutation: slows or stops regeneration when monsters are
// visible at level 1 or 2 respectively, stops regeneration at level 3.
static int _slow_heal_rate()
{
    if (player_mutation_level(MUT_SLOW_HEALING) == 3)
        return 0;

    for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
    {
        if (!mons_is_firewood(*mi)
            && !mi->wont_attack()
            && !mi->neutral())
        {
            return 2 - player_mutation_level(MUT_SLOW_HEALING);
        }
    }
    return 2;
}

int player_regen()
{
    int rr = you.hp_max / 3;

    if (rr > 20)
        rr = 20 + ((rr - 20) / 2);

    // Add in miscellaneous bonuses
    rr += _player_bonus_regen();

    // Before applying other effects, make sure that there's something
    // to heal.
    rr = max(1, rr);

    // Healing depending on satiation.
    // The better-fed you are, the faster you heal.
    if (you.species == SP_VAMPIRE)
    {
        if (you.hunger_state == HS_STARVING)
            rr = 0;   // No regeneration for starving vampires.
        else if (you.hunger_state == HS_ENGORGED)
            rr += 20; // More bonus regeneration for engorged vampires.
        else if (you.hunger_state < HS_SATIATED)
            rr /= 2;  // Halved regeneration for hungry vampires.
        else if (you.hunger_state >= HS_FULL)
            rr += 10; // Bonus regeneration for full vampires.
    }
#if TAG_MAJOR_VERSION == 34

    // Compared to other races, a starting djinni would have regen of 4 (hp)
    // plus 17 (mp).  So let's compensate them early; they can stand getting
    // shafted on the total regen rates later on.
    if (you.species == SP_DJINNI)
        if (you.hp_max < 100)
            rr += (100 - you.hp_max) / 6;
#endif

    // Slow heal mutation.
    if (player_mutation_level(MUT_SLOW_HEALING) > 0)
    {
        rr *= _slow_heal_rate();
        rr /= 2;
    }
    if (you.stat_zero[STAT_STR])
        rr /= 4;

    if (you.disease)
        rr = 0;

    // Trog's Hand.  This circumvents the slow healing effect.
    if (you.duration[DUR_TROGS_HAND])
        rr += 100;

    return rr;
}

int player_hunger_rate(bool temp)
{
    int hunger = 3;

    if (temp && you.form == TRAN_BAT)
        return 1;

    if (you.species == SP_TROLL)
        hunger += 3;            // in addition to the +3 for fast metabolism

    if (temp
        && (you.duration[DUR_REGENERATION]
            || you.duration[DUR_TROGS_HAND])
        && you.hp < you.hp_max)
    {
        hunger += 4;
    }

    if (temp)
    {
        if (you.duration[DUR_INVIS])
            hunger += 5;

        // Berserk has its own food penalty - excluding berserk haste.
        // Doubling the hunger cost for haste so that the per turn hunger
        // is consistent now that a hasted turn causes 50% the normal hunger
        // -cao
        if (you.duration[DUR_HASTE])
            hunger += haste_mul(5);
    }

    if (you.species == SP_VAMPIRE)
    {
        switch (you.hunger_state)
        {
        case HS_STARVING:
        case HS_NEAR_STARVING:
            hunger -= 3;
            break;
        case HS_VERY_HUNGRY:
            hunger -= 2;
            break;
        case HS_HUNGRY:
            hunger--;
            break;
        case HS_SATIATED:
            break;
        case HS_FULL:
            hunger++;
            break;
        case HS_VERY_FULL:
            hunger += 2;
            break;
        case HS_ENGORGED:
            hunger += 3;
        }
    }
    else
    {
        hunger += player_mutation_level(MUT_FAST_METABOLISM)
                - player_mutation_level(MUT_SLOW_METABOLISM);
    }

    if (you.hp < you.hp_max
        && player_mutation_level(MUT_SLOW_HEALING) < 3)
    {
        // rings
        hunger += 3 * you.wearing(EQ_RINGS, RING_REGENERATION);

        // troll leather
        if (you.species != SP_TROLL
            && (you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_LEATHER_ARMOUR)
                || you.wearing(EQ_BODY_ARMOUR, ARM_TROLL_HIDE)))
        {
            hunger += coinflip() ? 2 : 1;
        }
    }

    // If Cheibriados has slowed your life processes, you will hunger less.
    if (you_worship(GOD_CHEIBRIADOS) && you.piety >= piety_breakpoint(0))
        hunger = hunger * 3 / 4;

    if (hunger < 1)
        hunger = 1;

    return hunger;
}

int player_spell_levels()
{
    int sl = you.experience_level - 1 + you.skill(SK_SPELLCASTING, 2, true);

    bool fireball = false;
    bool delayed_fireball = false;

    if (sl > 99)
        sl = 99;

    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        if (you.spells[i] == SPELL_FIREBALL)
            fireball = true;
        else if (you.spells[i] == SPELL_DELAYED_FIREBALL)
            delayed_fireball = true;

        if (you.spells[i] != SPELL_NO_SPELL)
            sl -= spell_difficulty(you.spells[i]);
    }

    // Fireball is free for characters with delayed fireball
    if (fireball && delayed_fireball)
        sl += spell_difficulty(SPELL_FIREBALL);

    // Note: This can happen because of level drain.  Maybe we should
    // force random spells out when that happens. -- bwr
    if (sl < 0)
        sl = 0;

    return sl;
}

int player_likes_chunks(bool permanently)
{
    return you.gourmand(true, !permanently)
           ? 3 : player_mutation_level(MUT_CARNIVOROUS);
}

// If temp is set to false, temporary sources or resistance won't be counted.
int player_res_fire(bool calc_unid, bool temp, bool items)
{
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
        return 4; // full immunity

#endif
    int rf = 0;

    if (items)
    {
        // rings of fire resistance/fire
        rf += you.wearing(EQ_RINGS, RING_PROTECTION_FROM_FIRE, calc_unid);
        rf += you.wearing(EQ_RINGS, RING_FIRE, calc_unid);

        // rings of ice
        rf -= you.wearing(EQ_RINGS, RING_ICE, calc_unid);

        // Staves
        rf += you.wearing(EQ_STAFF, STAFF_FIRE, calc_unid);

        // body armour:
        rf += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
        rf += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR);
        rf -= you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_ARMOUR);
        rf += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_HIDE);
        rf += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_HIDE);
        rf -= you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_HIDE);

        // ego armours
        rf += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_FIRE_RESISTANCE);
        rf += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_RESISTANCE);

        // randart weapons:
        rf += you.scan_artefacts(ARTP_FIRE, calc_unid);

        // dragonskin cloak: 0.5 to draconic resistances
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            rf++;
        }
    }

    // species:
    if (you.species == SP_MUMMY)
        rf--;

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
    {
        if (temperature_effect(LORC_FIRE_RES_I))
            rf++;
        if (temperature_effect(LORC_FIRE_RES_II))
            rf++;
        if (temperature_effect(LORC_FIRE_RES_III))
            rf++;
    }
#endif

    // mutations:
    rf += player_mutation_level(MUT_HEAT_RESISTANCE, temp);
    rf -= player_mutation_level(MUT_HEAT_VULNERABILITY, temp);
    rf += player_mutation_level(MUT_MOLTEN_SCALES, temp) == 3 ? 1 : 0;

    // divine intervention:
    if (you.attribute[ATTR_DIVINE_FIRE_RES]
        && !player_under_penance(GOD_QAZLAL))
    {
        rf++;
    }

    // spells:
    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            rf++;

        if (you.duration[DUR_FIRE_SHIELD])
            rf += 2;

        if (you.duration[DUR_QAZLAL_FIRE_RES])
            rf++;

        // transformations:
        switch (you.form)
        {
        case TRAN_ICE_BEAST:
            rf--;
            break;
        case TRAN_WISP:
            rf += 2;
            break;
        case TRAN_DRAGON:
        {
            monster_type drag = dragon_form_dragon_type();
            if (drag == MONS_FIRE_DRAGON)
                rf += 2;
            else if (drag == MONS_ICE_DRAGON)
                rf--;
            break;
        }
        default:
            break;
        }
    }

    if (rf > 3)
        rf = 3;
    if (temp && you.duration[DUR_FIRE_VULN])
        rf--;
    if (rf < -3)
        rf = -3;

    return rf;
}

int player_res_steam(bool calc_unid, bool temp, bool items)
{
    int res = 0;
    const int rf = player_res_fire(calc_unid, temp, items);

    if (you.species == SP_PALE_DRACONIAN)
        res += 2;

    if (items && you.wearing(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_ARMOUR))
        res += 2;

    if (items && you.wearing(EQ_BODY_ARMOUR, ARM_STEAM_DRAGON_HIDE))
        res += 2;

    res += (rf < 0) ? rf
                    : (rf + 1) / 2;

    if (res > 3)
        res = 3;

    return res;
}

int player_res_cold(bool calc_unid, bool temp, bool items)
{
    int rc = 0;

    if (temp)
    {
        if (you.duration[DUR_RESISTANCE])
            rc++;

        if (you.duration[DUR_FIRE_SHIELD])
            rc -= 2;

        if (you.duration[DUR_QAZLAL_COLD_RES])
            rc++;

        // transformations:
        switch (you.form)
        {
        case TRAN_ICE_BEAST:
            rc += 3;
            break;
        case TRAN_WISP:
            rc += 2;
            break;
        case TRAN_DRAGON:
        {
            monster_type form = dragon_form_dragon_type();
            if (form == MONS_FIRE_DRAGON)
                rc--;
            else if (form == MONS_ICE_DRAGON)
                rc += 2;
            break;
        }
        case TRAN_LICH:
            rc++;
            break;
        default:
            break;
        }

        if (you.species == SP_VAMPIRE)
        {
            if (you.hunger_state <= HS_NEAR_STARVING)
                rc += 2;
            else if (you.hunger_state < HS_SATIATED)
                rc++;
        }

#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_LAVA_ORC && temperature_effect(LORC_COLD_VULN))
            rc--;
#endif
    }

    if (items)
    {
        // rings of cold resistance/ice
        rc += you.wearing(EQ_RINGS, RING_PROTECTION_FROM_COLD, calc_unid);
        rc += you.wearing(EQ_RINGS, RING_ICE, calc_unid);

        // rings of fire
        rc -= you.wearing(EQ_RINGS, RING_FIRE, calc_unid);

        // Staves
        rc += you.wearing(EQ_STAFF, STAFF_COLD, calc_unid);

        // body armour:
        rc += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_ARMOUR);
        rc += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR);
        rc -= you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_ARMOUR);
        rc += 2 * you.wearing(EQ_BODY_ARMOUR, ARM_ICE_DRAGON_HIDE);
        rc += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_HIDE);
        rc -= you.wearing(EQ_BODY_ARMOUR, ARM_FIRE_DRAGON_HIDE);

        // ego armours
        rc += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_COLD_RESISTANCE);
        rc += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_RESISTANCE);

        // randart weapons:
        rc += you.scan_artefacts(ARTP_COLD, calc_unid);

        // dragonskin cloak: 0.5 to draconic resistances
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
            rc++;
    }

#if TAG_MAJOR_VERSION == 34
    // species:
    if (you.species == SP_DJINNI)
        rc--;
#endif
    // mutations:
    rc += player_mutation_level(MUT_COLD_RESISTANCE, temp);
    rc -= player_mutation_level(MUT_COLD_VULNERABILITY, temp);
    rc += player_mutation_level(MUT_ICY_BLUE_SCALES, temp) == 3 ? 1 : 0;
    rc += player_mutation_level(MUT_SHAGGY_FUR, temp) == 3 ? 1 : 0;

    // divine intervention:
    if (you.attribute[ATTR_DIVINE_COLD_RES]
        && !player_under_penance(GOD_QAZLAL))
    {
        rc++;
    }

    if (rc < -3)
        rc = -3;
    else if (rc > 3)
        rc = 3;

    return rc;
}

bool player::res_corr(bool calc_unid, bool items) const
{
    if (religion == GOD_JIYVA && piety >= piety_breakpoint(2))
        return true;

    if (form == TRAN_WISP)
        return 1;

    if (items)
    {
        // dragonskin cloak: 0.5 to draconic resistances
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN)
            && coinflip())
        {
            return true;
        }
    }

    if ((form_keeps_mutations() || form == TRAN_DRAGON)
        && species == SP_YELLOW_DRACONIAN)
    {
        return true;
    }

    if (form_keeps_mutations()
        && player_mutation_level(MUT_YELLOW_SCALES) >= 3)
    {
        return true;
    }

    return actor::res_corr(calc_unid, items);
}

int player_res_acid(bool calc_unid, bool items)
{
    if (you.form == TRAN_WISP)
        return 3;

    return you.res_corr(calc_unid, items) ? 1 : 0;
}

// Returns a factor X such that post-resistance acid damage can be calculated
// as pre_resist_damage * X / 100.
int player_acid_resist_factor()
{
    int rA = player_res_acid();
    if (rA >= 3)
        return 0;
    else if (rA >= 1)
        return 50;
    return 100;
}

int player_res_electricity(bool calc_unid, bool temp, bool items)
{
    int re = 0;

    if (items)
    {
        // staff
        re += you.wearing(EQ_STAFF, STAFF_AIR, calc_unid);

        // body armour:
        re += you.wearing(EQ_BODY_ARMOUR, ARM_STORM_DRAGON_ARMOUR);
        re += you.wearing(EQ_BODY_ARMOUR, ARM_STORM_DRAGON_HIDE);

        // randart weapons:
        re += you.scan_artefacts(ARTP_ELECTRICITY, calc_unid);

        // dragonskin cloak: 0.5 to draconic resistances
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
            re++;
    }

    // mutations:
    re += player_mutation_level(MUT_THIN_METALLIC_SCALES, temp) == 3 ? 1 : 0;
    re += player_mutation_level(MUT_SHOCK_RESISTANCE, temp);
    re -= player_mutation_level(MUT_SHOCK_VULNERABILITY, temp);

    // divine intervention:
    if (you.attribute[ATTR_DIVINE_ELEC_RES]
        && !player_under_penance(GOD_QAZLAL))
    {
        re++;
    }

    if (temp)
    {
        if (you.attribute[ATTR_DIVINE_LIGHTNING_PROTECTION])
            return 3;

        if (you.duration[DUR_RESISTANCE])
            re++;

        if (you.duration[DUR_QAZLAL_ELEC_RES])
            re++;

        // transformations:
        if (you.form == TRAN_STATUE || you.form == TRAN_WISP)
            re += 1;

        if (re > 1)
            re = 1;
    }

    return re;
}

bool player_control_teleport(bool temp)
{
    return (temp && you.duration[DUR_CONTROL_TELEPORT])
           || crawl_state.game_is_zotdef();
}

int player_res_torment(bool, bool temp)
{
    return player_mutation_level(MUT_TORMENT_RESISTANCE)
           || you.form == TRAN_LICH
           || you.form == TRAN_FUNGUS
           || you.form == TRAN_TREE
           || you.form == TRAN_WISP
           || you.form == TRAN_SHADOW
           || you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING
           || you.petrified()
           || (temp && player_mutation_level(MUT_STOCHASTIC_TORMENT_RESISTANCE)
               && coinflip());
}

// Kiku protects you from torment to a degree.
int player_kiku_res_torment()
{
    return you_worship(GOD_KIKUBAAQUDGHA)
           && !player_under_penance()
           && you.piety >= piety_breakpoint(3)
           && !you.gift_timeout; // no protection during pain branding weapon
}

// If temp is set to false, temporary sources or resistance won't be counted.
int player_res_poison(bool calc_unid, bool temp, bool items)
{
    if ((you.is_undead == US_SEMI_UNDEAD ? you.hunger_state == HS_STARVING
            : you.is_undead && (temp || you.form != TRAN_LICH))
        || you.is_artificial()
        || (temp && you.form == TRAN_SHADOW)
        || player_equip_unrand(UNRAND_OLGREB)
        || you.duration[DUR_DIVINE_STAMINA])
    {
        return 3;
    }

    int rp = 0;

    if (items)
    {
        // rings of poison resistance
        rp += you.wearing(EQ_RINGS, RING_POISON_RESISTANCE, calc_unid);

        // Staves
        rp += you.wearing(EQ_STAFF, STAFF_POISON, calc_unid);

        // ego armour:
        rp += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_POISON_RESISTANCE);

        // body armour:
        rp += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_ARMOUR);
        rp += you.wearing(EQ_BODY_ARMOUR, ARM_SWAMP_DRAGON_ARMOUR);
        rp += you.wearing(EQ_BODY_ARMOUR, ARM_GOLD_DRAGON_HIDE);
        rp += you.wearing(EQ_BODY_ARMOUR, ARM_SWAMP_DRAGON_HIDE);

        // randart weapons:
        rp += you.scan_artefacts(ARTP_POISON, calc_unid);

        // dragonskin cloak: 0.5 to draconic resistances
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
            rp++;
    }

    // mutations:
    rp += player_mutation_level(MUT_POISON_RESISTANCE, temp);
    rp += player_mutation_level(MUT_SLIMY_GREEN_SCALES, temp) == 3 ? 1 : 0;

    // Only thirsty vampires are naturally poison resistant.
    if (you.species == SP_VAMPIRE && you.hunger_state < HS_SATIATED)
        rp++;

    if (temp)
    {
        // potions/cards:
        if (you.duration[DUR_RESISTANCE])
            rp++;

        // transformations:
        switch (you.form)
        {
        case TRAN_ICE_BEAST:
        case TRAN_STATUE:
        case TRAN_DRAGON:
        case TRAN_FUNGUS:
        case TRAN_TREE:
        case TRAN_WISP:
            rp++;
            break;
        default:
            break;
        }

        if (you.petrified())
            rp++;
    }

    // Give vulnerability for Spider Form, and only let one level of rP to make
    // up for it (never be poison resistant in Spider Form).
    rp = (rp > 0 ? 1 : rp);

    if (temp)
    {
        if (you.form == TRAN_SPIDER)
            rp--;

        if (you.duration[DUR_POISON_VULN])
            rp--;
    }

    return rp;
}

int player_res_sticky_flame(bool calc_unid, bool temp, bool items)
{
    int rsf = 0;

    if (you.species == SP_MOTTLED_DRACONIAN)
        rsf++;

    if (items && you.wearing(EQ_BODY_ARMOUR, ARM_MOTTLED_DRAGON_ARMOUR))
        rsf++;
    if (items && you.wearing(EQ_BODY_ARMOUR, ARM_MOTTLED_DRAGON_HIDE))
        rsf++;

    // dragonskin cloak: 0.5 to draconic resistances
    if (items && calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
        rsf++;

    if (you.form == TRAN_WISP)
        rsf++;

    if (rsf > 1)
        rsf = 1;

    return rsf;
}

int player_spec_death()
{
    int sd = 0;

    // Staves
    sd += you.wearing(EQ_STAFF, STAFF_DEATH);

    // species:
    if (you.species == SP_MUMMY)
    {
        if (you.experience_level >= 13)
            sd++;
        if (you.experience_level >= 26)
            sd++;
    }

    // transformations:
    if (you.form == TRAN_LICH)
        sd++;

    return sd;
}

int player_spec_fire()
{
    int sf = 0;

    // staves:
    sf += you.wearing(EQ_STAFF, STAFF_FIRE);

    // rings of fire:
    sf += you.wearing(EQ_RINGS, RING_FIRE);

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_FIRE_BOOST))
        sf++;
#endif

    if (you.duration[DUR_FIRE_SHIELD])
        sf++;

    return sf;
}

int player_spec_cold()
{
    int sc = 0;

    // staves:
    sc += you.wearing(EQ_STAFF, STAFF_COLD);

    // rings of ice:
    sc += you.wearing(EQ_RINGS, RING_ICE);

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC
        && (temperature_effect(LORC_LAVA_BOOST)
            || temperature_effect(LORC_FIRE_BOOST)))
    {
        sc--;
    }
#endif

    return sc;
}

int player_spec_earth()
{
    int se = 0;

    // Staves
    se += you.wearing(EQ_STAFF, STAFF_EARTH);

    return se;
}

int player_spec_air()
{
    int sa = 0;

    // Staves
    sa += you.wearing(EQ_STAFF, STAFF_AIR);

    return sa;
}

int player_spec_conj()
{
    int sc = 0;

    // Staves
    sc += you.wearing(EQ_STAFF, STAFF_CONJURATION);

    return sc;
}

int player_spec_hex()
{
    int sh = 0;

    // Unrands
    if (player_equip_unrand(UNRAND_BOTONO))
        sh++;

    return sh;
}

int player_spec_charm()
{
    // Nothing, for the moment.
    return 0;
}

int player_spec_summ()
{
    int ss = 0;

    // Staves
    ss += you.wearing(EQ_STAFF, STAFF_SUMMONING);

    return ss;
}

int player_spec_poison()
{
    int sp = 0;

    // Staves
    sp += you.wearing(EQ_STAFF, STAFF_POISON);

    if (player_equip_unrand(UNRAND_OLGREB))
        sp++;

    return sp;
}

int player_energy()
{
    int pe = 0;

    // Staves
    pe += you.wearing(EQ_STAFF, STAFF_ENERGY);

    return pe;
}

// If temp is set to false, temporary sources of resistance won't be
// counted.
int player_prot_life(bool calc_unid, bool temp, bool items)
{
    int pl = 0;

    // Hunger is temporary, true, but that's something you can control,
    // especially as life protection only increases the hungrier you
    // get.
    if (you.species == SP_VAMPIRE)
    {
        switch (you.hunger_state)
        {
        case HS_STARVING:
        case HS_NEAR_STARVING:
            pl = 3;
            break;
        case HS_VERY_HUNGRY:
        case HS_HUNGRY:
            pl = 2;
            break;
        case HS_SATIATED:
            pl = 1;
            break;
        default:
            break;
        }
    }

    // Same here. Your piety status, and, hence, TSO's protection, is
    // something you can more or less control.
    if (you_worship(GOD_SHINING_ONE))
    {
        if (you.piety >= piety_breakpoint(1))
            pl++;
        if (you.piety >= piety_breakpoint(3))
            pl++;
        if (you.piety >= piety_breakpoint(5))
            pl++;
    }

    if (temp)
    {
        // Now, transformations could stop at any time.
        switch (you.form)
        {
        case TRAN_STATUE:
            pl++;
            break;
        case TRAN_FUNGUS:
        case TRAN_TREE:
        case TRAN_WISP:
        case TRAN_LICH:
        case TRAN_SHADOW:
            pl += 3;
            break;
        default:
           break;
        }

        // completely stoned, unlike statue which has some life force
        if (you.petrified())
            pl += 3;
    }

    if (items)
    {
        if (you.wearing(EQ_AMULET, AMU_WARDING, calc_unid))
            pl++;

        // rings
        pl += you.wearing(EQ_RINGS, RING_LIFE_PROTECTION, calc_unid);

        // armour (checks body armour only)
        pl += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_POSITIVE_ENERGY);

        // pearl dragon counts
        pl += you.wearing(EQ_BODY_ARMOUR, ARM_PEARL_DRAGON_ARMOUR);
        pl += you.wearing(EQ_BODY_ARMOUR, ARM_PEARL_DRAGON_HIDE);

        // randart wpns
        pl += you.scan_artefacts(ARTP_NEGATIVE_ENERGY, calc_unid);

        // dragonskin cloak: 0.5 to draconic resistances
        // this one is dubious (no pearl draconians)
        if (calc_unid && player_equip_unrand(UNRAND_DRAGONSKIN) && coinflip())
            pl++;

        pl += you.wearing(EQ_STAFF, STAFF_DEATH, calc_unid);
    }

    // undead/demonic power
    pl += player_mutation_level(MUT_NEGATIVE_ENERGY_RESISTANCE, temp);

    pl = min(3, pl);

    return pl;
}

// New player movement speed system... allows for a bit more than
// "player runs fast" and "player walks slow" in that the speed is
// actually calculated (allowing for centaurs to get a bonus from
// swiftness and other such things).  Levels of the mutation now
// also have meaning (before they all just meant fast).  Most of
// this isn't as fast as it used to be (6 for having anything), but
// even a slight speed advantage is very good... and we certainly don't
// want to go past 6 (see below). -- bwr
int player_movement_speed()
{
    int mv = 10;

    // transformations
    if (you.form == TRAN_BAT)
        mv = 5; // but allowed minimum is six
    else if (you.form == TRAN_PIG)
        mv = 7;
    else if (you.form == TRAN_PORCUPINE || you.form == TRAN_WISP)
        mv = 8;
    else if (you.fishtail)
        mv = 6;

    // moving on liquefied ground takes longer
    if (you.liquefied_ground())
        mv += 3;

    // armour
    if (you.run())
        mv -= 1;

    mv += you.wearing_ego(EQ_ALL_ARMOUR, SPARM_PONDEROUSNESS);

    // Cheibriados
    if (you_worship(GOD_CHEIBRIADOS))
        mv += 2 + min(div_rand_round(you.piety, 20), 8);

    // Tengu can move slightly faster when flying.
    if (you.tengu_flight())
        mv--;

    if (you.duration[DUR_FROZEN])
        mv += 4;

    if (you.duration[DUR_GRASPING_ROOTS])
        mv += 3;

    // Mutations: -2, -3, -4, unless innate and shapechanged.
    if (int fast = player_mutation_level(MUT_FAST))
        mv -= fast + 1;

    if (int slow = player_mutation_level(MUT_SLOW))
    {
        mv *= 10 + slow * 2;
        mv /= 10;
    }

    if (you.duration[DUR_SWIFTNESS] > 0 && !you.in_liquid())
    {
        if (you.attribute[ATTR_SWIFTNESS] > 0)
          mv = div_rand_round(3*mv, 4);
        else if (mv >= 8)
          mv = div_rand_round(3*mv, 2);
        else if (mv == 7)
          mv = div_rand_round(7*6, 5); // balance for the cap at 6
    }

    // We'll use the old value of six as a minimum, with haste this could
    // end up as a speed of three, which is about as fast as we want
    // the player to be able to go (since 3 is 3.33x as fast and 2 is 5x,
    // which is a bit of a jump, and a bit too fast) -- bwr
    // Currently Haste takes 6 to 4, which is 2.5x as fast as delay 10
    // and still seems plenty fast. -- elliptic
    if (mv < 6)
        mv = 6;

    return mv;
}

// This function differs from the above in that it's used to set the
// initial time_taken value for the turn.  Everything else (movement,
// spellcasting, combat) applies a ratio to this value.
int player_speed()
{
    int ps = 10;

    // When paralysed, speed is irrelevant.
    if (you.cannot_act())
        return ps;

    for (int i = 0; i < NUM_STATS; ++i)
        if (you.stat_zero[i])
            ps *= 2;

    if (you.duration[DUR_SLOW])
        ps = haste_mul(ps);

    if (you.duration[DUR_BERSERK] && !you_worship(GOD_CHEIBRIADOS))
        ps = berserk_div(ps);
    else if (you.duration[DUR_HASTE])
        ps = haste_div(ps);

    if (you.form == TRAN_STATUE || you.duration[DUR_PETRIFYING])
    {
        ps *= 15;
        ps /= 10;
    }

    return ps;
}

// Get level of player mutation, ignoring mutations with an activity level
// less than minact.
static int _mut_level(mutation_type mut, mutation_activity_type minact)
{
    const int mlevel = you.mutation[mut];

    const mutation_activity_type active = mutation_activity_level(mut);

    if (active >= minact)
        return mlevel;

    return 0;
}

// Output level of player mutation.  If temp is true (the default), take into
// account the suppression of mutations by changes of form.
int player_mutation_level(mutation_type mut, bool temp)
{
    return _mut_level(mut, temp ? MUTACT_PARTIAL : MUTACT_INACTIVE);
}

static int _player_armour_beogh_bonus(const item_def& item)
{
    if (item.base_type != OBJ_ARMOUR)
        return 0;

    int bonus = 0;

    if (you_worship(GOD_BEOGH) && !player_under_penance())
    {
        if (you.piety >= piety_breakpoint(5))
            bonus = 10;
        else if (you.piety >= piety_breakpoint(4))
            bonus = 8;
        else if (you.piety >= piety_breakpoint(2))
            bonus = 6;
        else if (you.piety >= piety_breakpoint(0))
            bonus = 4;
        else
            bonus = 2;
    }

    return bonus;
}

bool is_effectively_light_armour(const item_def *item)
{
    return !item
           || (abs(property(*item, PARM_EVASION)) < 5);
}

bool player_effectively_in_light_armour()
{
    const item_def *armour = you.slot_item(EQ_BODY_ARMOUR, false);
    return is_effectively_light_armour(armour);
}

// This function returns true if the player has a radically different
// shape... minor changes like blade hands don't count, also note
// that lich transformation doesn't change the character's shape
// (so we end up with Naga-liches, Spiggan-liches, Minotaur-liches)
// it just makes the character undead (with the benefits that implies). - bwr
bool player_is_shapechanged()
{
    if (you.form == TRAN_NONE
        || you.form == TRAN_BLADE_HANDS
        || you.form == TRAN_LICH
        || you.form == TRAN_SHADOW
        || you.form == TRAN_APPENDAGE)
    {
        return false;
    }

    return true;
}

// An evasion factor based on the player's body size, smaller == higher
// evasion size factor.
static int _player_evasion_size_factor()
{
    // XXX: you.body_size() implementations are incomplete, fix.
    const size_type size = you.body_size(PSIZE_BODY);
    return 2 * (SIZE_MEDIUM - size);
}

// Determines racial shield penalties (formicids get a bonus compared to
// other medium-sized races)
static int _player_shield_racial_factor()
{
    return max(1, 5 + (you.species == SP_FORMICID ? -2 // Same as trolls/centaurs/etc.
                                                  : _player_evasion_size_factor()));
}

// The total EV penalty to the player for all their worn armour items
// with a base EV penalty (i.e. EV penalty as a base armour property,
// not as a randart property).
static int _player_adjusted_evasion_penalty(const int scale)
{
    int piece_armour_evasion_penalty = 0;

    // Some lesser armours have small penalties now (barding).
    for (int i = EQ_MIN_ARMOUR; i < EQ_MAX_ARMOUR; i++)
    {
        if (i == EQ_SHIELD || !player_wearing_slot(i))
            continue;

        // [ds] Evasion modifiers for armour are negatives, change
        // those to positive for penalty calc.
        const int penalty = (-property(you.inv[you.equip[i]], PARM_EVASION))/3;
        if (penalty > 0)
            piece_armour_evasion_penalty += penalty;
    }

    return piece_armour_evasion_penalty * scale +
           you.adjusted_body_armour_penalty(scale);
}

// EV bonuses that work even when helpless.
static int _player_para_evasion_bonuses(ev_ignore_type evit)
{
    int evbonus = 0;

    if (you.duration[DUR_PHASE_SHIFT] && !(evit & EV_IGNORE_PHASESHIFT))
        evbonus += 8;

    if (player_mutation_level(MUT_DISTORTION_FIELD) > 0)
        evbonus += player_mutation_level(MUT_DISTORTION_FIELD) + 1;

    return evbonus;
}

// Player EV bonuses for various effects and transformations. This
// does not include tengu/merfolk EV bonuses for flight/swimming.
static int _player_evasion_bonuses(ev_ignore_type evit)
{
    int evbonus = _player_para_evasion_bonuses(evit);

    if (you.duration[DUR_AGILITY])
        evbonus += AGILITY_BONUS;

    evbonus += you.wearing(EQ_RINGS_PLUS, RING_EVASION);

    if (you.wearing_ego(EQ_WEAPON, SPWPN_EVASION))
        evbonus += 5;

    evbonus += you.scan_artefacts(ARTP_EVASION);

    // mutations
    if (_mut_level(MUT_ICY_BLUE_SCALES, MUTACT_FULL) > 1)
        evbonus--;
    if (_mut_level(MUT_MOLTEN_SCALES, MUTACT_FULL) > 1)
        evbonus--;
    evbonus += max(0, player_mutation_level(MUT_GELATINOUS_BODY) - 1);

    // transformation penalties/bonuses not covered by size alone:
    if (you.form == TRAN_STATUE)
        evbonus -= 10;               // stiff and slow

    return evbonus;
}

// Player EV scaling for being flying tengu or swimming merfolk.
static int _player_scale_evasion(int prescaled_ev, const int scale)
{
    if (you.duration[DUR_PETRIFYING] || you.caught())
        prescaled_ev /= 2;
    else if  (you.duration[DUR_GRASPING_ROOTS])
        prescaled_ev = prescaled_ev * 2 / 3;

    switch (you.species)
    {
    case SP_MERFOLK:
        // Merfolk get an evasion bonus in water.
        if (you.fishtail)
        {
            const int ev_bonus = min(9 * scale,
                                     max(2 * scale, prescaled_ev / 4));
            return prescaled_ev + ev_bonus;
        }
        break;

    case SP_TENGU:
        // Flying Tengu get an evasion bonus.
        if (you.flight_mode())
        {
            const int ev_bonus = min(9 * scale,
                                     max(1 * scale, prescaled_ev / 5));
            return prescaled_ev + ev_bonus;
        }
        break;

    default:
        break;
    }
    return prescaled_ev;
}

// Total EV for player using the revised 0.6 evasion model.
int player_evasion(ev_ignore_type evit)
{
    const int size_factor = _player_evasion_size_factor();
    // Repulsion fields and size are all that matters when paralysed or
    // at 0 dex.
    if ((you.cannot_move() || you.stat_zero[STAT_DEX] || you.form == TRAN_TREE)
        && !(evit & EV_IGNORE_HELPLESS))
    {
        const int paralysed_base_ev = 2 + size_factor / 2;
        const int repulsion_ev = _player_para_evasion_bonuses(evit);
        return max(1, paralysed_base_ev + repulsion_ev);
    }

    const int scale = 100;
    const int size_base_ev = (10 + size_factor) * scale;

    const int adjusted_evasion_penalty =
        _player_adjusted_evasion_penalty(scale);

    // The last two parameters are not important.
    const int ev_dex = stepdown_value(you.dex(), 10, 24, 72, 72);

    const int dodge_bonus =
        (70 + you.skill(SK_DODGING, 10) * ev_dex) * scale
        / (20 - size_factor) / 10;

    // [ds] Dodging penalty for being in high EVP armour, almost
    // identical to v0.5/4.1 penalty, but with the EVP discount being
    // 1 instead of 0.5 so that leather armour is fully discounted.
    // The 1 EVP of leather armour may still incur an
    // adjusted_evasion_penalty, however.
    const int armour_dodge_penalty = max(0,
        (10 * you.adjusted_body_armour_penalty(scale, true)
         - 30 * scale)
        / max(1, (int) you.strength()));

    // Adjust dodge bonus for the effects of being suited up in armour.
    const int armour_adjusted_dodge_bonus =
        max(0, dodge_bonus - armour_dodge_penalty);

    const int adjusted_shield_penalty = you.adjusted_shield_penalty(scale);

    const int prestepdown_evasion =
        size_base_ev
        + armour_adjusted_dodge_bonus
        - adjusted_evasion_penalty
        - adjusted_shield_penalty;

    const int poststepdown_evasion =
        stepdown_value(prestepdown_evasion, 20*scale, 30*scale, 60*scale, -1);

    const int evasion_bonuses = _player_evasion_bonuses(evit) * scale;

    const int prescaled_evasion =
        poststepdown_evasion + evasion_bonuses;

    const int final_evasion =
        _player_scale_evasion(prescaled_evasion, scale);

    return unscale_round_up(final_evasion, scale);
}

// Returns the spellcasting penalty (increase in spell failure) for the
// player's worn body armour and shield.
int player_armour_shield_spell_penalty()
{
    const int scale = 100;

    const int body_armour_penalty =
        max(25 * you.adjusted_body_armour_penalty(scale), 0);

    const int total_penalty = body_armour_penalty
                 + 25 * you.adjusted_shield_penalty(scale)
                 - 20 * scale;

    return max(total_penalty, 0) / scale;
}

int player_wizardry()
{
    return you.wearing(EQ_RINGS, RING_WIZARDRY)
           + you.wearing(EQ_STAFF, STAFF_WIZARDRY);
}

int player_shield_class()
{
    int shield = 0;
    int stat = 0;

    if (you.incapacitated())
        return 0;

    if (player_wearing_slot(EQ_SHIELD))
    {
        const item_def& item = you.inv[you.equip[EQ_SHIELD]];
        int size_factor = (you.body_size(PSIZE_TORSO) - SIZE_MEDIUM)
                        * (item.sub_type - ARM_LARGE_SHIELD);
        int base_shield = property(item, PARM_AC) * 2 + size_factor;

        int beogh_bonus = _player_armour_beogh_bonus(item);

        // bonus applied only to base, see above for effect:
        shield += base_shield * 50;
        shield += base_shield * you.skill(SK_SHIELDS, 5) / 2;
        shield += base_shield * beogh_bonus * 10 / 6;

        shield += item.plus * 200;

        if (item.sub_type == ARM_BUCKLER)
            stat = you.dex() * 38;
        else if (item.sub_type == ARM_LARGE_SHIELD)
            stat = you.dex() * 12 + you.strength() * 26;
        else
            stat = you.dex() * 19 + you.strength() * 19;
        stat = stat * (base_shield + 13) / 26;
    }
    else
    {
        if (you.duration[DUR_MAGIC_SHIELD])
            shield += 900 + you.skill(SK_EVOCATIONS, 75);

        if (!you.duration[DUR_FIRE_SHIELD]
            && you.duration[DUR_CONDENSATION_SHIELD])
        {
            shield += 800 + you.skill(SK_ICE_MAGIC, 60);
        }
    }

    if (you.duration[DUR_DIVINE_SHIELD])
    {
        shield += you.attribute[ATTR_DIVINE_SHIELD] * 150;
        stat = max(stat, int(you.attribute[ATTR_DIVINE_SHIELD] * 300));
    }

    if (shield + stat > 0
        && (player_wearing_slot(EQ_SHIELD) || you.duration[DUR_DIVINE_SHIELD]))
    {
        shield += you.skill(SK_SHIELDS, 38)
                + min(you.skill(SK_SHIELDS, 38), 3 * 38);
    }

    // mutations
    // +3, +6, +9
    shield += (player_mutation_level(MUT_LARGE_BONE_PLATES) > 0
               ? player_mutation_level(MUT_LARGE_BONE_PLATES) * 300
               : 0);

    stat += qazlal_sh_boost() * 100;

    return (shield + stat + 50) / 100;
}

bool player_sust_abil(bool calc_unid)
{
    return you.wearing(EQ_RINGS, RING_SUSTAIN_ABILITIES, calc_unid)
           || you.scan_artefacts(ARTP_SUSTAB);
}

void forget_map(bool rot)
{
    ASSERT(!crawl_state.game_is_arena());

    // If forgetting was intentional, clear the travel trail.
    if (!rot)
        clear_travel_trail();

    // Labyrinth and the Abyss use special rotting rules.
    const bool rot_resist = player_in_branch(BRANCH_LABYRINTH)
                                && you.species == SP_MINOTAUR
                            || player_in_branch(BRANCH_ABYSS)
                                && you_worship(GOD_LUGONU);
    const double geometric_chance = 0.99;
    const int radius = (rot_resist ? 200 : 100);

    const int scalar = 0xFF;
    for (rectangle_iterator ri(0); ri; ++ri)
    {
        const coord_def &p = *ri;
        if (!env.map_knowledge(p).known() || you.see_cell(p))
            continue;

        if (rot)
        {
            const int dist = distance2(you.pos(), p);
            int chance = pow(geometric_chance,
                             max(1, (dist - radius) / 40)) * scalar;
            if (x_chance_in_y(chance, scalar))
                continue;
        }

        if (you.see_cell(p))
            continue;

        env.map_knowledge(p).clear();
        if (env.map_forgotten.get())
            (*env.map_forgotten.get())(p).clear();
        StashTrack.update_stash(p);
#ifdef USE_TILE
        tile_forget_map(p);
#endif
    }

    ash_detect_portals(is_map_persistent());
#ifdef USE_TILE
    tiles.update_minimap_bounds();
#endif
}

static void _remove_temp_mutation()
{
    int num_remove = min(you.attribute[ATTR_TEMP_MUTATIONS],
        max(you.attribute[ATTR_TEMP_MUTATIONS] * 5 / 12 - random2(3),
        2 + random2(3)));

    if (num_remove >= you.attribute[ATTR_TEMP_MUTATIONS])
        mprf(MSGCH_DURATION, "You feel the corruption within you wane completely.");
    else
        mprf(MSGCH_DURATION, "You feel the corruption within you wane somewhat.");

    for (int i = 0; i < num_remove; ++i)
        delete_temp_mutation();

    if (you.attribute[ATTR_TEMP_MUTATIONS] > 0)
    {
        you.attribute[ATTR_TEMP_MUT_XP] +=
            min(you.experience_level, 17) * (350 + roll_dice(5, 350)) / 17;
    }
}

int get_exp_progress()
{
    if (you.experience_level >= 27)
        return 0;

    const int current = exp_needed(you.experience_level);
    const int next    = exp_needed(you.experience_level + 1);
    if (next == current)
        return 0;
    return (you.experience - current) * 100 / (next - current);
}

void gain_exp(unsigned int exp_gained, unsigned int* actual_gain)
{
    if (crawl_state.game_is_arena())
        return;

    if (crawl_state.game_is_zotdef())
    {
        you.zot_points += exp_gained;
        // All XP, for some reason Sprint speeds up only skill training,
        // but not levelling, Ash skill transfer, etc.
        exp_gained *= 2;
    }

    if (player_under_penance(GOD_ASHENZARI))
        ash_reduce_penance(exp_gained);

    const unsigned int old_exp = you.experience;

    dprf("gain_exp: %d", exp_gained);

    if (you.transfer_skill_points > 0)
    {
        // Can happen if the game got interrupted during target skill choice.
        if (is_invalid_skill(you.transfer_to_skill))
        {
            you.transfer_from_skill = SK_NONE;
            you.transfer_skill_points = 0;
            you.transfer_total_skill_points = 0;
        }
        else
        {
            int amount = exp_gained * 10
                                / calc_skill_cost(you.skill_cost_level);
            if (amount >= 20 || one_chance_in(20 - amount))
            {
                amount = max(20, amount);
                transfer_skill_points(you.transfer_from_skill,
                                      you.transfer_to_skill, amount, false);
            }
        }
    }

    if (you.experience + exp_gained > (unsigned int)MAX_EXP_TOTAL)
        you.experience = MAX_EXP_TOTAL;
    else
        you.experience += exp_gained;

    you.attribute[ATTR_EVOL_XP] += exp_gained;
    for (int i = GOD_NO_GOD; i < NUM_GODS; ++i)
    {
        if (active_penance((god_type) i))
        {
            you.attribute[ATTR_GOD_WRATH_XP] -= exp_gained;
            while (you.attribute[ATTR_GOD_WRATH_XP] < 0)
            {
                you.attribute[ATTR_GOD_WRATH_COUNT]++;
                set_penance_xp_timeout();
            }
            break;
        }
    }

    if (crawl_state.game_is_sprint())
        exp_gained = sprint_modify_exp(exp_gained);

    you.exp_available += exp_gained;

    train_skills();
    while (check_selected_skills()
           && you.exp_available >= calc_skill_cost(you.skill_cost_level))
    {
        train_skills();
    }

    if (you.exp_available >= calc_skill_cost(you.skill_cost_level))
        you.exp_available = calc_skill_cost(you.skill_cost_level);

    level_change();

    if (actual_gain != NULL)
        *actual_gain = you.experience - old_exp;

    if (you.attribute[ATTR_TEMP_MUTATIONS] > 0)
    {
        you.attribute[ATTR_TEMP_MUT_XP] -= exp_gained;
        if (you.attribute[ATTR_TEMP_MUT_XP] <= 0)
            _remove_temp_mutation();
    }

    recharge_xp_evokers(exp_gained);

    if (you.attribute[ATTR_XP_DRAIN])
    {
        int loss = div_rand_round(exp_gained * 3 / 2,
                                  calc_skill_cost(you.skill_cost_level));

        // Make it easier to recover from very heavy levels of draining
        // (they're nasty enough as it is)
        loss = loss * (1 + (you.attribute[ATTR_XP_DRAIN] / 250.0f));

        dprf("Lost %d of %d draining points", loss, you.attribute[ATTR_XP_DRAIN]);

        you.attribute[ATTR_XP_DRAIN] -= loss;
        // Regaining skills may affect AC/EV.
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        if (you.attribute[ATTR_XP_DRAIN] <= 0)
        {
            you.attribute[ATTR_XP_DRAIN] = 0;
            mprf(MSGCH_RECOVERY, "Your life force feels restored.");
        }
    }
}

static void _draconian_scale_colour_message()
{
    switch (you.species)
    {
    case SP_RED_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a fiery red colour.");
        perma_mutate(MUT_HEAT_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_WHITE_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on an icy white colour.");
        perma_mutate(MUT_COLD_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_GREEN_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a lurid green colour.");
        perma_mutate(MUT_POISON_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_YELLOW_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a golden yellow colour.");
        break;

    case SP_GREY_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a dull iron-grey colour.");
        perma_mutate(MUT_UNBREATHING, 1, "draconian maturity");
        break;

    case SP_BLACK_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a glossy black colour.");
        perma_mutate(MUT_SHOCK_RESISTANCE, 1, "draconian maturity");
        break;

    case SP_PURPLE_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a rich purple colour.");
        break;

    case SP_MOTTLED_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start taking on a weird mottled pattern.");
        break;

    case SP_PALE_DRACONIAN:
        mprf(MSGCH_INTRINSIC_GAIN, "Your scales start fading to a pale cyan-grey colour.");
        break;

    case SP_BASE_DRACONIAN:
        mpr("");
        break;

    default:
        break;
    }
}

bool will_gain_life(int lev)
{
    if (lev < you.attribute[ATTR_LIFE_GAINED] - 2)
        return false;

    return you.lives + you.deaths < (lev - 1) / 3;
}

static void _felid_extra_life()
{
    if (will_gain_life(you.max_level)
        && you.lives < 2)
    {
        you.lives++;
        mprf(MSGCH_INTRINSIC_GAIN, "Extra life!");
        you.attribute[ATTR_LIFE_GAINED] = you.max_level;
        // Should play the 1UP sound from SMB...
    }
}

/**
 * Handle the effects from a player's change in XL.
 * @param aux                     A string describing the cause of the level
 *                                change.
 * @param skip_attribute_increase If true and XL has increased, don't process
 *                                stat gains.
 */
void level_change(int source, const char* aux, bool skip_attribute_increase)
{
    const bool wiz_cmd = crawl_state.prev_cmd == CMD_WIZARD
                      || crawl_state.repeat_cmd == CMD_WIZARD;

    // necessary for the time being, as level_change() is called
    // directly sometimes {dlb}
    you.redraw_experience = true;

    while (you.experience < exp_needed(you.experience_level))
        lose_level(source, aux);

    while (you.experience_level < 27
           && you.experience >= exp_needed(you.experience_level + 1))
    {
        if (!skip_attribute_increase && !wiz_cmd)
        {
            crawl_state.cancel_cmd_all();

            if (is_processing_macro())
                flush_input_buffer(FLUSH_ABORT_MACRO);
        }

        // [ds] Make sure we increment you.experience_level and apply
        // any stat/hp increases only after we've cleared all prompts
        // for this experience level. If we do part of the work before
        // the prompt, and a player on telnet gets disconnected, the
        // SIGHUP will save Crawl in the in-between state and rob the
        // player of their level-up perks.

        const int new_exp = you.experience_level + 1;

        if (new_exp <= you.max_level)
        {
            mprf(MSGCH_INTRINSIC_GAIN,
                 "Welcome back to level %d!", new_exp);

            // No more prompts for this XL past this point.

            you.experience_level = new_exp;
        }
        else  // Character has gained a new level
        {
            // Don't want to see the dead creature at the prompt.
            redraw_screen();
            // There may be more levels left to gain.
            you.redraw_experience = true;

            if (new_exp == 27)
                mprf(MSGCH_INTRINSIC_GAIN, "You have reached level 27, the final one!");
            else
            {
                mprf(MSGCH_INTRINSIC_GAIN, "You have reached level %d!",
                     new_exp);
            }

            if (!(new_exp % 3) && !skip_attribute_increase)
                if (!attribute_increase())
                    return; // abort level gain, the xp is still there

            crawl_state.stat_gain_prompt = false;
            you.experience_level = new_exp;
            you.max_level = you.experience_level;

#ifdef USE_TILE_LOCAL
            // In case of intrinsic ability changes.
            tiles.layout_statcol();
            redraw_screen();
#endif

            switch (you.species)
            {
            case SP_HUMAN:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_HIGH_ELF:
                if (!(you.experience_level % 3))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEEP_ELF:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_INT, 1, false, "level gain");
                break;

            case SP_SLUDGE_ELF:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEEP_DWARF:
                if (you.experience_level == 14)
                {
                    mprf(MSGCH_INTRINSIC_GAIN, "You feel somewhat more resistant.");
                    perma_mutate(MUT_NEGATIVE_ENERGY_RESISTANCE, 1, "level up");
                }

                if (you.experience_level == 9
                    || you.experience_level == 18)
                {
                    perma_mutate(MUT_PASSIVE_MAPPING, 1, "level up");
                }

                if (!(you.experience_level % 4))
                {
                    modify_stat(coinflip() ? STAT_STR
                                           : STAT_INT, 1, false,
                                "level gain");
                }
                break;

            case SP_HALFLING:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_DEX, 1, false, "level gain");
                break;

            case SP_KOBOLD:
                if (!(you.experience_level % 5))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_HILL_ORC:
#if TAG_MAJOR_VERSION == 34
            case SP_LAVA_ORC:
#endif
                if (!(you.experience_level % 5))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_MUMMY:
                if (you.experience_level == 13 || you.experience_level == 26)
                    mprf(MSGCH_INTRINSIC_GAIN, "You feel more in touch with the powers of death.");

                if (you.experience_level == 13)  // level 13 for now -- bwr
                {
                    mprf(MSGCH_INTRINSIC_GAIN, "You can now infuse your body with "
                                               "magic to restore decomposition.");
                }
                break;

            case SP_VAMPIRE:
                if (you.experience_level == 3)
                {
                    if (you.hunger_state > HS_SATIATED)
                    {
                        mprf(MSGCH_INTRINSIC_GAIN, "If you weren't so full you "
                             "could now transform into a vampire bat.");
                    }
                    else
                    {
                        mprf(MSGCH_INTRINSIC_GAIN, "You can now transform into "
                             "a vampire bat.");
                    }
                }
                else if (you.experience_level == 6)
                    mprf(MSGCH_INTRINSIC_GAIN, "You can now bottle potions of blood from corpses.");
                break;

            case SP_NAGA:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");

                if (!(you.experience_level % 3))
                {
                    mprf(MSGCH_INTRINSIC_GAIN, "Your skin feels tougher.");
                    you.redraw_armour_class = true;
                }

                if (you.experience_level == 13)
                {
                    mprf(MSGCH_INTRINSIC_GAIN,
                         "Your tail grows strong enough to constrict your enemies.");
                }
                break;

            case SP_TROLL:
                if (!(you.experience_level % 3))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_OGRE:
                if (!(you.experience_level % 3))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_BASE_DRACONIAN:
                if (you.experience_level >= 7)
                {
                    you.species = random_draconian_player_species();

                    // We just changed our aptitudes, so some skills may now
                    // be at the wrong level (with negative progress); if we
                    // print anything in this condition, we might trigger a
                    // --More--, a redraw, and a crash (#6376 on Mantis).
                    //
                    // Hence we first fix up our skill levels silently (passing
                    // do_level_up = false) but save the old values; then when
                    // we want the messages later, we restore the old skill
                    // levels and call check_skill_level_change() again, this
                    // time passing do_update = true.

                    uint8_t saved_skills[NUM_SKILLS];
                    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
                    {
                        saved_skills[i] = you.skills[i];
                        check_skill_level_change(static_cast<skill_type>(i),
                                                 false);
                    }
                    // The player symbol depends on species.
                    update_player_symbol();
#ifdef USE_TILE
                    init_player_doll();
#endif
                    _draconian_scale_colour_message();

                    // Produce messages about skill increases/decreases. We
                    // restore one skill level at a time so that at most the
                    // skill being checked is at the wrong level.
                    for (int i = SK_FIRST_SKILL; i < NUM_SKILLS; ++i)
                    {
                        you.skills[i] = saved_skills[i];
                        check_skill_level_change(static_cast<skill_type>(i));
                    }

                    redraw_screen();
                }
            case SP_RED_DRACONIAN:
            case SP_WHITE_DRACONIAN:
            case SP_GREEN_DRACONIAN:
            case SP_YELLOW_DRACONIAN:
            case SP_GREY_DRACONIAN:
            case SP_BLACK_DRACONIAN:
            case SP_PURPLE_DRACONIAN:
            case SP_MOTTLED_DRACONIAN:
            case SP_PALE_DRACONIAN:
                if (!(you.experience_level % 3))
                {
                    mprf(MSGCH_INTRINSIC_GAIN, "Your scales feel tougher.");
                    you.redraw_armour_class = true;
                }

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");

                if (you.experience_level == 14)
                {
                    switch (you.species)
                    {
                    case SP_GREEN_DRACONIAN:
                        perma_mutate(MUT_STINGER, 1, "draconian growth");
                        break;
                    case SP_YELLOW_DRACONIAN:
                        perma_mutate(MUT_ACIDIC_BITE, 1, "draconian growth");
                        break;
                    case SP_BLACK_DRACONIAN:
                        perma_mutate(MUT_BIG_WINGS, 1, "draconian growth");
                        mprf(MSGCH_INTRINSIC_GAIN, "You can now fly continuously.");
                        break;
                    default:
                        break;
                    }
                }
                break;

            case SP_CENTAUR:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEMIGOD:
                if (!(you.experience_level % 2))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_SPRIGGAN:
                if (!(you.experience_level % 5))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_MINOTAUR:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }
                break;

            case SP_DEMONSPAWN:
            {
                bool gave_message = false;
                int level = 0;
                mutation_type first_body_facet = NUM_MUTATIONS;

                for (unsigned i = 0; i < you.demonic_traits.size(); ++i)
                {
                    if (is_body_facet(you.demonic_traits[i].mutation))
                    {
                        if (first_body_facet < NUM_MUTATIONS
                            && you.demonic_traits[i].mutation
                                != first_body_facet)
                        {
                            if (you.experience_level == level)
                            {
                                mprf(MSGCH_MUTATION, "You feel monstrous as your "
                                     "demonic heritage exerts itself.");
                                mark_milestone("monstrous", "is a "
                                               "monstrous demonspawn!");
                            }
                            break;
                        }

                        if (first_body_facet == NUM_MUTATIONS)
                        {
                            first_body_facet = you.demonic_traits[i].mutation;
                            level = you.demonic_traits[i].level_gained;
                        }
                    }
                }

                for (unsigned i = 0; i < you.demonic_traits.size(); ++i)
                {
                    if (you.demonic_traits[i].level_gained
                        == you.experience_level)
                    {
                        if (!gave_message)
                        {
                            mprf(MSGCH_INTRINSIC_GAIN, "Your demonic ancestry asserts itself...");

                            gave_message = true;
                        }
                        perma_mutate(you.demonic_traits[i].mutation, 1,
                                     "demonic ancestry");
                    }
                }

                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;
            }

            case SP_GHOUL:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_STR, 1, false, "level gain");
                break;

            case SP_TENGU:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");

                if (you.experience_level == 5)
                    mprf(MSGCH_INTRINSIC_GAIN, "You have gained the ability to fly.");
                else if (you.experience_level == 14)
                    mprf(MSGCH_INTRINSIC_GAIN, "You can now fly continuously.");
                break;

            case SP_MERFOLK:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

            case SP_FELID:
                if (!(you.experience_level % 5))
                {
                    modify_stat((coinflip() ? STAT_INT
                                            : STAT_DEX), 1, false,
                                "level gain");
                }

                if (you.experience_level == 6 || you.experience_level == 12)
                {
                    perma_mutate(MUT_SHAGGY_FUR, 1, "growing up");
                    perma_mutate(MUT_JUMP, 1, "growing up");
                }
                _felid_extra_life();
                break;

            case SP_OCTOPODE:
                if (!(you.experience_level % 5))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

#if TAG_MAJOR_VERSION == 34
            case SP_DJINNI:
                if (!(you.experience_level % 4))
                    modify_stat(STAT_RANDOM, 1, false, "level gain");
                break;

#endif
            case SP_FORMICID:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_INT), 1, false,
                                "level gain");
                }
                break;

            case SP_GARGOYLE:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_INT), 1, false,
                                "level gain");
                }

                if (you.experience_level == 14)
                {
                    perma_mutate(MUT_BIG_WINGS, 1, "gargoyle growth");
                    mprf(MSGCH_INTRINSIC_GAIN, "You can now fly continuously.");
                }
                break;

            case SP_VINE_STALKER:
                if (!(you.experience_level % 4))
                {
                    modify_stat((coinflip() ? STAT_STR
                                            : STAT_DEX), 1, false,
                                "level gain");
                }

                if (you.experience_level == 6)
                    perma_mutate(MUT_REGENERATION, 1, "vine stalker growth");

                if (you.experience_level == 8)
                    perma_mutate(MUT_FANGS, 1, "vine stalker growth");

                if (you.experience_level == 12)
                    perma_mutate(MUT_REGENERATION, 1, "vine stalker growth");
                break;

            default:
                break;
            }
        }

        // zot defence abilities; must also be updated in ability.cc when these levels are changed
        if (crawl_state.game_is_zotdef())
        {
            if (you.experience_level == 2)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of oklob saplings.");
            if (you.experience_level == 3)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of arrow traps.");
            if (you.experience_level == 4)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of plants.");
            if (you.experience_level == 4)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through removing curses.");
            if (you.experience_level == 5)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of burning bushes.");
            if (you.experience_level == 6)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of altars and grenades.");
            if (you.experience_level == 7)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of oklob plants.");
            if (you.experience_level == 8)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of net traps.");
            if (you.experience_level == 9)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of ice statues.");
            if (you.experience_level == 10)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of spear traps.");
            if (you.experience_level == 11)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of alarm traps.");
            if (you.experience_level == 12)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of mushroom circles.");
            if (you.experience_level == 13)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of bolt traps.");
            if (you.experience_level == 14)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of orange crystal statues.");
            if (you.experience_level == 15)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of needle traps.");
            if (you.experience_level == 16)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through self-teleportation.");
            if (you.experience_level == 17)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through making water.");
            if (you.experience_level == 19)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of lightning spires.");
            if (you.experience_level == 20)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of silver statues.");
            // gold and bazaars gained together
            if (you.experience_level == 21)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of bazaars.");
            if (you.experience_level == 21)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through acquiring gold.");
            if (you.experience_level == 22)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of oklob circles.");
            if (you.experience_level == 23)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through invoking Sage effects.");
            if (you.experience_level == 24)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through acquirement.");
            if (you.experience_level == 25)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of blade traps.");
            if (you.experience_level == 26)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of curse skulls.");
#if 0
            if (you.experience_level == 27)
                mprf(MSGCH_INTRINSIC_GAIN, "Your Zot abilities now extend through the making of teleport traps.");
#endif
        }

        const int old_hp = you.hp;
        const int old_maxhp = you.hp_max;
        const int old_mp = you.magic_points;
        const int old_maxmp = you.max_magic_points;

        // recalculate for game
        calc_hp();
        calc_mp();

        set_hp(old_hp * you.hp_max / old_maxhp);
        set_mp(old_maxmp > 0 ? old_mp * you.max_magic_points / old_maxmp
               : you.max_magic_points);

        // Get "real" values for note-taking, i.e. ignore Berserk,
        // transformations or equipped items.
        const int note_maxhp = get_real_hp(false, false);
        const int note_maxmp = get_real_mp(false);

        char buf[200];
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_DJINNI)
            // Djinn don't HP/MP
            sprintf(buf, "EP: %d/%d",
                    min(you.hp, note_maxhp + note_maxmp),
                    note_maxhp + note_maxmp);
        else
#endif
            sprintf(buf, "HP: %d/%d MP: %d/%d",
                    min(you.hp, note_maxhp), note_maxhp,
                    min(you.magic_points, note_maxmp), note_maxmp);
        take_note(Note(NOTE_XP_LEVEL_CHANGE, you.experience_level, 0, buf));

        xom_is_stimulated(12);

        learned_something_new(HINT_NEW_LEVEL);
    }

    while (you.experience >= exp_needed(you.max_level + 1))
    {
        ASSERT(you.experience_level == 27);
        ASSERT(you.max_level < 127); // marshalled as an 1-byte value
        you.max_level++;
        if (you.species == SP_FELID)
            _felid_extra_life();
    }

    you.redraw_title = true;

#ifdef DGL_WHEREIS
    whereis_record();
#endif

    // Hints mode arbitrarily ends at xp 7.
    if (crawl_state.game_is_hints() && you.experience_level >= 7)
        hints_finished();
}

void adjust_level(int diff, bool just_xp)
{
    ASSERT((uint64_t)you.experience <= (uint64_t)MAX_EXP_TOTAL);

    if (you.experience_level + diff < 1)
        you.experience = 0;
    else if (you.experience_level + diff >= 27)
        you.experience = max(you.experience, exp_needed(27));
    else
    {
        while (diff < 0 && you.experience >= exp_needed(27))
        {
            // Having XP for level 53 and going back to 26 due to a single
            // card would mean your felid is not going to get any extra lives
            // in foreseable future.
            you.experience -= exp_needed(27) - exp_needed(26);
            diff++;
        }
        int old_min = exp_needed(you.experience_level);
        int old_max = exp_needed(you.experience_level + 1);
        int new_min = exp_needed(you.experience_level + diff);
        int new_max = exp_needed(you.experience_level + 1 + diff);
        dprf("XP before: %d\n", you.experience);
        dprf("%4.2f of %d..%d to %d..%d",
             (you.experience - old_min) * 1.0 / (old_max - old_min),
             old_min, old_max, new_min, new_max);

        you.experience = ((int64_t)(new_max - new_min))
                       * (you.experience - old_min)
                       / (old_max - old_min)
                       + new_min;
        dprf("XP after: %d\n", you.experience);
    }

    ASSERT((uint64_t)you.experience <= (uint64_t)MAX_EXP_TOTAL);

    if (!just_xp)
        level_change();
}

// Here's a question for you: does the ordering of mods make a difference?
// (yes) -- are these things in the right order of application to stealth?
// - 12mar2000 {dlb}
int check_stealth()
{
    ASSERT(!crawl_state.game_is_arena());
    // Extreme stealthiness can be enforced by wizmode stealth setting.
    if (crawl_state.disables[DIS_MON_SIGHT])
        return 1000;

    if (you.attribute[ATTR_SHADOWS] || you.berserk() || you.stat_zero[STAT_DEX])
        return 0;

    int stealth = you.dex() * 3;

    int race_mod = 0;
    if (player_genus(GENPC_DRACONIAN))
        race_mod = 12;
    else
    {
        switch (you.species) // why not use body_size here?
        {
        case SP_TROLL:
        case SP_OGRE:
        case SP_CENTAUR:
#if TAG_MAJOR_VERSION == 34
        case SP_DJINNI:
#endif
            race_mod = 9;
            break;
        case SP_MINOTAUR:
            race_mod = 12;
            break;
        case SP_VAMPIRE:
            // Thirsty/bat-form vampires are (much) more stealthy
            if (you.hunger_state == HS_STARVING)
                race_mod = 21;
            else if (you.form == TRAN_BAT
                     || you.hunger_state <= HS_NEAR_STARVING)
            {
                race_mod = 20;
            }
            else if (you.hunger_state < HS_SATIATED)
                race_mod = 19;
            else
                race_mod = 18;
            break;
        case SP_HALFLING:
        case SP_KOBOLD:
        case SP_SPRIGGAN:
        case SP_NAGA:       // not small but very good at stealth
        case SP_FELID:
        case SP_OCTOPODE:
            race_mod = 18;
            break;
        default:
            race_mod = 15;
            break;
        }
    }

    switch (you.form)
    {
    case TRAN_FUNGUS:
    case TRAN_SHADOW: // You slip into the shadows.
        race_mod = 30;
        break;
    case TRAN_TREE:
        race_mod = 27; // masquerading as scenery
        break;
    case TRAN_SPIDER:
#if TAG_MAJOR_VERSION == 34
    case TRAN_JELLY:
#endif
    case TRAN_WISP:
        race_mod = 21;
        break;
    case TRAN_ICE_BEAST:
        race_mod = 15;
        break;
    case TRAN_STATUE:
        race_mod -= 3; // depends on the base race
        break;
    case TRAN_DRAGON:
        race_mod = 6;
        break;
    case TRAN_PORCUPINE:
        race_mod = 12; // small but noisy
        break;
    case TRAN_PIG:
        race_mod = 9; // trotters, oinking...
        break;
    case TRAN_BAT:
        if (you.species != SP_VAMPIRE)
            race_mod = 17;
        break;
    case TRAN_BLADE_HANDS:
        if (you.species == SP_FELID && !you.airborne())
            stealth -= 50; // a constant penalty
        break;
    case TRAN_NONE:
    case TRAN_APPENDAGE:
    case TRAN_LICH:
        break;
    }

    stealth += you.skill(SK_STEALTH, race_mod);

    if (you.confused())
        stealth /= 3;

    const item_def *arm = you.slot_item(EQ_BODY_ARMOUR, false);
    const item_def *boots = you.slot_item(EQ_BOOTS, false);

    if (arm)
    {
        // [ds] New stealth penalty formula from rob: SP = 6 * (EP^2)
        // Now 2 * EP^2 / 3 after EP rescaling.
        const int ep = -property(*arm, PARM_EVASION);
        const int penalty = 2 * ep * ep / 3;
#if 0
        dprf("Stealth penalty for armour (ep: %d): %d", ep, penalty);
#endif
        stealth -= penalty;
    }

    stealth += 50 * you.scan_artefacts(ARTP_STEALTH);

    stealth += 50 * you.wearing(EQ_RINGS, RING_STEALTH);
    stealth -= 50 * you.wearing(EQ_RINGS, RING_LOUDNESS);

    if (you.duration[DUR_STEALTH])
        stealth += 80;

    if (you.duration[DUR_AGILITY])
        stealth += 50;

    if (!you.airborne())
    {
        if (you.in_water())
        {
            // Merfolk can sneak up on monsters underwater -- bwr
            if (you.fishtail || you.species == SP_OCTOPODE)
                stealth += 50;
            else if (!you.can_swim() && !you.extra_balanced())
                stealth /= 2;       // splashy-splashy
        }

        else if (boots && get_armour_ego_type(*boots) == SPARM_STEALTH)
            stealth += 50;

        else if (player_mutation_level(MUT_HOOVES) > 0)
            stealth -= 5 + 5 * player_mutation_level(MUT_HOOVES);

        else if (you.species == SP_FELID && (!you.form || you.form == TRAN_APPENDAGE))
            stealth += 20;  // paws
    }

    // Radiating silence is the negative complement of shouting all the
    // time... a sudden change from background noise to no noise is going
    // to clue anything in to the fact that something is very wrong...
    // a personal silence spell would naturally be different, but this
    // silence radiates for a distance and prevents monster spellcasting,
    // which pretty much gives away the stealth game.
    // this penalty is dependent on the actual amount of ambient noise
    // in the level -doy
    if (you.duration[DUR_SILENCE])
        stealth -= 50 + current_level_ambient_noise();

    // Mutations.
    stealth += 40 * player_mutation_level(MUT_NIGHTSTALKER);
    stealth += 25 * player_mutation_level(MUT_THIN_SKELETAL_STRUCTURE);
    stealth += 40 * player_mutation_level(MUT_CAMOUFLAGE);
    if (player_mutation_level(MUT_TRANSLUCENT_SKIN) > 1)
        stealth += 20 * (player_mutation_level(MUT_TRANSLUCENT_SKIN) - 1);

    // it's easier to be stealthy when there's a lot of background noise
    stealth += 2 * current_level_ambient_noise();

    // If you've been tagged with Corona or are Glowing, the glow
    // makes you extremely unstealthy.
    if (you.backlit())
        stealth = stealth * 2 / 5;
    // On the other hand, shrouding has the reverse effect, if you know
    // how to make use of it:
    if (you.umbra())
    {
        int umbra_multiplier = 1;
        if (you_worship(GOD_DITHMENOS) || you_worship(GOD_YREDELEMNUL))
            umbra_multiplier = (you.piety + MAX_PIETY) / MAX_PIETY;
        if (player_equip_unrand(UNRAND_SHADOWS))
            umbra_multiplier = max(umbra_multiplier, 3 / 2);
        stealth *= umbra_multiplier;
    }
    // If you're surrounded by a storm, you're inherently pretty conspicuous.
    if (you_worship(GOD_QAZLAL) && !player_under_penance()
        && you.piety >= piety_breakpoint(0))
    {
        stealth = stealth
                  * (MAX_PIETY - min((int)you.piety, piety_breakpoint(5)))
                  / (MAX_PIETY - piety_breakpoint(0));
    }
    // The shifting glow from the Orb, while too unstable to negate invis
    // or affect to-hit, affects stealth even more than regular glow.
    if (orb_haloed(you.pos()))
        stealth /= 3;

    stealth = max(0, stealth);

    return stealth;
}

// Returns the medium duration value which is usually announced by a special
// message ("XY is about to time out") or a change of colour in the
// status display.
// Note that these values cannot be relied on when playing since there are
// random decrements precisely to avoid this.
int get_expiration_threshold(duration_type dur)
{
    switch (dur)
    {
    case DUR_PETRIFYING:
        return 1 * BASELINE_DELAY;

    case DUR_QUAD_DAMAGE:
        return 3 * BASELINE_DELAY; // per client.qc

    case DUR_FIRE_SHIELD:
    case DUR_SILENCE: // no message
        return 5 * BASELINE_DELAY;

    case DUR_REGENERATION:
    case DUR_RESISTANCE:
    case DUR_SWIFTNESS:
    case DUR_INVIS:
    case DUR_HASTE:
    case DUR_BERSERK:
    case DUR_ICY_ARMOUR:
    case DUR_CONDENSATION_SHIELD:
    case DUR_PHASE_SHIFT:
    case DUR_CONTROL_TELEPORT:
    case DUR_DEATH_CHANNEL:
    case DUR_SHROUD_OF_GOLUBRIA:
    case DUR_INFUSION:
    case DUR_SONG_OF_SLAYING:
    case DUR_TROGS_HAND:
    case DUR_QAZLAL_FIRE_RES:
    case DUR_QAZLAL_COLD_RES:
    case DUR_QAZLAL_ELEC_RES:
    case DUR_QAZLAL_AC:
        return 6 * BASELINE_DELAY;

    case DUR_FLIGHT:
    case DUR_TRANSFORMATION: // not on status
    case DUR_DEATHS_DOOR:    // not on status
    case DUR_SLIMIFY:
        return 10 * BASELINE_DELAY;

    // These get no messages when they "flicker".
    case DUR_CONFUSING_TOUCH:
        return 20 * BASELINE_DELAY;

    case DUR_ANTIMAGIC:
        return you.hp_max; // not so severe anymore

    default:
        return 0;
    }
}

// Is a given duration about to expire?
bool dur_expiring(duration_type dur)
{
    const int value = you.duration[dur];
    if (value <= 0)
        return false;

    return value <= get_expiration_threshold(dur);
}

static void _output_expiring_message(duration_type dur, const char* msg)
{
    if (you.duration[dur])
    {
        const bool expires = dur_expiring(dur);
        mprf("%s%s", expires ? "Expiring: " : "", msg);
    }
}

static void _display_vampire_status()
{
    string msg = "At your current hunger state you ";
    vector<string> attrib;

    switch (you.hunger_state)
    {
        case HS_STARVING:
            attrib.push_back("resist poison");
            attrib.push_back("significantly resist cold");
            attrib.push_back("strongly resist negative energy");
            attrib.push_back("resist torment");
            attrib.push_back("do not heal.");
            break;
        case HS_NEAR_STARVING:
            attrib.push_back("resist poison");
            attrib.push_back("significantly resist cold");
            attrib.push_back("strongly resist negative energy");
            attrib.push_back("have an extremely slow metabolism");
            attrib.push_back("heal slowly.");
            break;
        case HS_VERY_HUNGRY:
        case HS_HUNGRY:
            attrib.push_back("resist poison");
            attrib.push_back("resist cold");
            attrib.push_back("significantly resist negative energy");
            if (you.hunger_state == HS_HUNGRY)
                attrib.push_back("have a slow metabolism");
            else
                attrib.push_back("have a very slow metabolism");
            attrib.push_back("heal slowly.");
            break;
        case HS_SATIATED:
            attrib.push_back("resist negative energy.");
            break;
        case HS_FULL:
            attrib.push_back("have a fast metabolism");
            attrib.push_back("heal quickly.");
            break;
        case HS_VERY_FULL:
            attrib.push_back("have a very fast metabolism");
            attrib.push_back("heal quickly.");
            break;
        case HS_ENGORGED:
            attrib.push_back("have an extremely fast metabolism");
            attrib.push_back("heal extremely quickly.");
            break;
    }

    if (!attrib.empty())
    {
        msg += comma_separated_line(attrib.begin(), attrib.end());
        mpr(msg.c_str());
    }
}

static void _display_movement_speed()
{
    const int move_cost = (player_speed() * player_movement_speed()) / 10;

    const bool water  = you.in_liquid();
    const bool swim   = you.swimming();

    const bool fly    = you.flight_mode();
    const bool swift  = (you.duration[DUR_SWIFTNESS] > 0
                         && you.attribute[ATTR_SWIFTNESS] >= 0);
    const bool antiswift = (you.duration[DUR_SWIFTNESS] > 0
                            && you.attribute[ATTR_SWIFTNESS] < 0);

    mprf("Your %s speed is %s%s%s.",
          // order is important for these:
          (swim)    ? "swimming" :
          (water)   ? "wading" :
          (fly)     ? "flying"
                    : "movement",

          (water && !swim)  ? "uncertain and " :
          (!water && swift) ? "aided by the wind" :
          (!water && antiswift) ? "hindered by the wind" : "",

          (!water && swift) ? ((move_cost >= 10) ? ", but still "
                                                 : " and ") :
          (!water && antiswift) ? ((move_cost <= 10) ? ", but still "
                                                     : " and ")
                            : "",

          (move_cost <   8) ? "very quick" :
          (move_cost <  10) ? "quick" :
          (move_cost == 10) ? "average" :
          (move_cost <  13) ? "slow"
                            : "very slow");
}

static void _display_tohit()
{
#ifdef DEBUG_DIAGNOSTICS
    melee_attack attk(&you, NULL);

    const int to_hit = attk.calc_to_hit(false);

    dprf("To-hit: %d", to_hit);
#endif
/*
    // Messages based largely on percentage chance of missing the
    // average EV 10 humanoid, and very agile EV 30 (pretty much
    // max EV for monsters currently).
    //
    // "awkward"    - need lucky hit (less than EV)
    // "difficult"  - worse than 2 in 3
    // "hard"       - worse than fair chance
    mprf("%s given your current equipment.",
         (to_hit <   1) ? "You are completely incapable of fighting" :
         (to_hit <   5) ? "Hitting even clumsy monsters is extremely awkward" :
         (to_hit <  10) ? "Hitting average monsters is awkward" :
         (to_hit <  15) ? "Hitting average monsters is difficult" :
         (to_hit <  20) ? "Hitting average monsters is hard" :
         (to_hit <  30) ? "Very agile monsters are a bit awkward to hit" :
         (to_hit <  45) ? "Very agile monsters are a bit difficult to hit" :
         (to_hit <  60) ? "Very agile monsters are a bit hard to hit" :
         (to_hit < 100) ? "You feel comfortable with your ability to fight"
                        : "You feel confident with your ability to fight");
*/
}

static const char* _attack_delay_desc(int attack_delay)
{
    return (attack_delay >= 200) ? "extremely slow" :
           (attack_delay >= 155) ? "very slow" :
           (attack_delay >= 125) ? "quite slow" :
           (attack_delay >= 105) ? "below average" :
           (attack_delay >=  95) ? "average" :
           (attack_delay >=  75) ? "above average" :
           (attack_delay >=  55) ? "quite fast" :
           (attack_delay >=  45) ? "very fast" :
           (attack_delay >=  35) ? "extremely fast" :
                                   "blindingly fast";
}

static void _display_attack_delay()
{
    const int delay = you.attack_delay(you.weapon(), NULL, false, false);

    // Scale to fit the displayed weapon base delay, i.e.,
    // normal speed is 100 (as in 100%).
    int avg;
    // FIXME for new ranged combat
/*    const item_def* weapon = you.weapon();
    if (weapon && is_range_weapon(*weapon))
        avg = launcher_final_speed(*weapon, you.shield(), false);
    else */
        avg = 10 * delay;

    // Haste shouldn't be counted, but let's show finesse.
    if (you.duration[DUR_FINESSE])
        avg = max(20, avg / 2);

    if (you.wizard)
        mprf("Your attack speed is %s (%d).", _attack_delay_desc(avg), avg);
    else
        mprf("Your attack speed is %s.", _attack_delay_desc(avg));
}

// forward declaration
static string _constriction_description();

void display_char_status()
{
    if (you.is_undead == US_SEMI_UNDEAD && you.hunger_state == HS_ENGORGED)
        mpr("You feel almost alive.");
    else if (you.is_undead)
        mpr("You are undead.");
    else if (you.duration[DUR_DEATHS_DOOR])
    {
        _output_expiring_message(DUR_DEATHS_DOOR,
                                 "You are standing in death's doorway.");
    }
    else
        mpr("You are alive.");

    const int halo_size = you.halo_radius2();
    if (halo_size >= 0)
    {
        if (halo_size > 37)
            mpr("You are illuminated by a large divine halo.");
        else if (halo_size > 10)
            mpr("You are illuminated by a divine halo.");
        else
            mpr("You are illuminated by a small divine halo.");
    }
    else if (you.haloed())
        mpr("An external divine halo illuminates you.");

    if (you.species == SP_VAMPIRE)
        _display_vampire_status();

    // A hard-coded duration/status list used to be used here. This list is no
    // longer hard-coded. May 2014. -reaverb
    status_info inf;
    for (unsigned i = 0; i <= STATUS_LAST_STATUS; ++i)
    {
        if (fill_status_info(i, &inf) && !inf.long_text.empty())
            mprf("%s", inf.long_text.c_str());
    }
    string cinfo = _constriction_description();
    if (!cinfo.empty())
        mpr(cinfo.c_str());

    _display_movement_speed();
    _display_tohit();
    _display_attack_delay();

    // magic resistance
    mprf("You are %s to hostile enchantments.",
         magic_res_adjective(player_res_magic(false)).c_str());
    dprf("MR: %d", you.res_magic());

    // character evaluates their ability to sneak around:
    mprf("You feel %s.", stealth_desc(check_stealth()).c_str());
    dprf("Stealth: %d", check_stealth());
}

bool player::clarity(bool calc_unid, bool items) const
{
    if (player_mutation_level(MUT_CLARITY))
        return true;

    if (religion == GOD_ASHENZARI && piety >= piety_breakpoint(2)
        && !player_under_penance())
    {
        return true;
    }

    return actor::clarity(calc_unid, items);
}

bool player::gourmand(bool calc_unid, bool items) const
{
    if (player_mutation_level(MUT_GOURMAND) > 0)
        return true;

    return actor::gourmand(calc_unid, items);
}

bool player::stasis(bool calc_unid, bool items) const
{
    if (species == SP_FORMICID)
        return true;

    return actor::stasis(calc_unid, items);
}

unsigned int exp_needed(int lev, int exp_apt)
{
    unsigned int level = 0;

    // Basic plan:
    // Section 1: levels  1- 5, second derivative goes 10-10-20-30.
    // Section 2: levels  6-13, second derivative is exponential/doubling.
    // Section 3: levels 14-27, second derivative is constant at 8470.

    // Here's a table:
    //
    // level      xp      delta   delta2
    // =====   =======    =====   ======
    //   1           0        0       0
    //   2          10       10      10
    //   3          30       20      10
    //   4          70       40      20
    //   5         140       70      30
    //   6         270      130      60
    //   7         520      250     120
    //   8        1010      490     240
    //   9        1980      970     480
    //  10        3910     1930     960
    //  11        7760     3850    1920
    //  12       15450     7690    3840
    //  13       26895    11445    3755
    //  14       45585    18690    7245
    //  15       72745    27160    8470
    //  16      108375    35630    8470
    //  17      152475    44100    8470
    //  18      205045    52570    8470
    //  19      266085    61040    8470
    //  20      335595    69510    8470
    //  21      413575    77980    8470
    //  22      500025    86450    8470
    //  23      594945    94920    8470
    //  24      698335    103390   8470
    //  25      810195    111860   8470
    //  26      930525    120330   8470
    //  27     1059325    128800   8470

    switch (lev)
    {
    case 1:
        level = 1;
        break;
    case 2:
        level = 10;
        break;
    case 3:
        level = 30;
        break;
    case 4:
        level = 70;
        break;

    default:
        if (lev < 13)
        {
            lev -= 4;
            level = 10 + 10 * lev + (60 << lev);
        }
        else
        {
            lev -= 12;
            level = 16675 + 5985 * lev + 4235 * lev * lev;
        }
        break;
    }

    if (exp_apt == -99)
        exp_apt = species_exp_modifier(you.species);

    return (unsigned int) ((level - 1) * exp(-log(2.0) * (exp_apt - 1) / 4));
}

// returns bonuses from rings of slaying, etc.
int slaying_bonus(weapon_property_type which_affected, bool ranged)
{
    int ret = 0;

    if (which_affected == PWPN_HIT)
    {
        ret += you.wearing(EQ_RINGS_PLUS, RING_SLAYING);
        ret += you.scan_artefacts(ARTP_ACCURACY);
        if (you.wearing_ego(EQ_GLOVES, SPARM_ARCHERY) && ranged)
            ret += 5;
    }
    else if (which_affected == PWPN_DAMAGE)
    {
        ret += you.wearing(EQ_RINGS_PLUS2, RING_SLAYING);
        ret += you.scan_artefacts(ARTP_DAMAGE);
        if (you.wearing_ego(EQ_GLOVES, SPARM_ARCHERY) && ranged)
            ret += 3;
    }

    ret += 4 * augmentation_amount();

    if (you.duration[DUR_SONG_OF_SLAYING])
        ret += you.props["song_of_slaying_bonus"].get_int();

    return ret;
}

// Checks each equip slot for a randart, and adds up all of those with
// a given property. Slow if any randarts are worn, so avoid where
// possible.
int player::scan_artefacts(artefact_prop_type which_property,
                           bool calc_unid) const
{
    int retval = 0;

    for (int i = EQ_WEAPON; i < NUM_EQUIP; ++i)
    {
        if (melded[i] || equip[i] == -1)
            continue;

        const int eq = equip[i];

        // Only weapons give their effects when in our hands.
        if (i == EQ_WEAPON && inv[ eq ].base_type != OBJ_WEAPONS)
            continue;

        if (!is_artefact(inv[ eq ]))
            continue;

        bool known;
        int val = artefact_wpn_property(inv[eq], which_property, known);
        if (calc_unid || known)
            retval += val;
    }

    return retval;
}

void calc_hp()
{
    int oldhp = you.hp, oldmax = you.hp_max;
    you.hp_max = get_real_hp(true, false);
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
        you.hp_max += get_real_mp(true);
#endif
    deflate_hp(you.hp_max, false);
    if (oldhp != you.hp || oldmax != you.hp_max)
        dprf("HP changed: %d/%d -> %d/%d", oldhp, oldmax, you.hp, you.hp_max);
    you.redraw_hit_points = true;
}

void dec_hp(int hp_loss, bool fatal, const char *aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!fatal && you.hp < 1)
        you.hp = 1;

    if (!fatal && hp_loss >= you.hp)
        hp_loss = you.hp - 1;

    if (hp_loss < 1)
        return;

    // If it's not fatal, use ouch() so that notes can be taken. If it IS
    // fatal, somebody else is doing the bookkeeping, and we don't want to mess
    // with that.
    if (!fatal && aux)
        ouch(hp_loss, NON_MONSTER, KILLED_BY_SOMETHING, aux);
    else
        you.hp -= hp_loss;

    you.redraw_hit_points = true;
}

void calc_mp()
{
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
    {
        you.magic_points = you.max_magic_points = 0;
        return calc_hp();
    }
#endif

    you.max_magic_points = get_real_mp(true);
    you.magic_points = min(you.magic_points, you.max_magic_points);
    you.redraw_magic_points = true;
}

void flush_mp()
{
    if (Options.magic_point_warning
        && you.magic_points < you.max_magic_points
                              * Options.magic_point_warning / 100)
    {
        mprf(MSGCH_DANGER, "* * * LOW MAGIC WARNING * * *");
    }

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));
    you.redraw_magic_points = true;
}

void dec_mp(int mp_loss, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (mp_loss < 1)
        return;

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
        return dec_hp(mp_loss * DJ_MP_RATE, false);
#endif

    you.magic_points -= mp_loss;

    you.magic_points = max(0, you.magic_points);
    if (!silent)
        flush_mp();
}

void drain_mp(int loss)
{
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
    {

        if (loss <= 0)
            return;

        you.duration[DUR_ANTIMAGIC] = min(you.duration[DUR_ANTIMAGIC] + loss * 3,
                                           1000); // so it goes away after one '5'
    }
    else
#endif
    return dec_mp(loss);
}

bool enough_hp(int minimum, bool suppress_msg, bool abort_macros)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.duration[DUR_DEATHS_DOOR])
    {
        if (!suppress_msg)
            mpr("You cannot pay life while functionally dead.");

        if (abort_macros)
        {
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
        }
        return false;
    }

    // We want to at least keep 1 HP. -- bwr
    if (you.hp < minimum + 1)
    {
        if (!suppress_msg)
            mpr("You don't have enough health at the moment.");

        if (abort_macros)
        {
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
        }
        return false;
    }

    return true;
}

bool enough_mp(int minimum, bool suppress_msg, bool abort_macros)
{
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
        return enough_hp(minimum * DJ_MP_RATE, suppress_msg);
#endif

    ASSERT(!crawl_state.game_is_arena());

    if (you.magic_points < minimum)
    {
        if (!suppress_msg)
        {
            if (get_real_mp(true) < minimum)
                mpr("You don't have enough magic capacity.");
            else
                mpr("You don't have enough magic at the moment.");
        }
        if (abort_macros)
        {
            crawl_state.cancel_cmd_again();
            crawl_state.cancel_cmd_repeat();
        }
        return false;
    }

    return true;
}

bool enough_zp(int minimum, bool suppress_msg)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.zot_points < minimum)
    {
        if (!suppress_msg)
            mpr("You don't have enough Zot Points.");

        crawl_state.cancel_cmd_again();
        crawl_state.cancel_cmd_repeat();
        return false;
    }
    return true;
}

void inc_mp(int mp_gain, bool silent)
{
    ASSERT(!crawl_state.game_is_arena());

    if (mp_gain < 1)
        return;

#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_DJINNI)
        return inc_hp(mp_gain * DJ_MP_RATE);
#endif

    bool wasnt_max = (you.magic_points < you.max_magic_points);

    you.magic_points += mp_gain;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    if (!silent)
    {
        if (wasnt_max && you.magic_points == you.max_magic_points)
            interrupt_activity(AI_FULL_MP);
        you.redraw_magic_points = true;
    }
}

// Note that "max_too" refers to the base potential, the actual
// resulting max value is subject to penalties, bonuses, and scalings.
// To avoid message spam, don't take notes when HP increases.
void inc_hp(int hp_gain)
{
    ASSERT(!crawl_state.game_is_arena());

    if (hp_gain < 1)
        return;

    bool wasnt_max = (you.hp < you.hp_max);

    you.hp += hp_gain;

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    if (wasnt_max && you.hp == you.hp_max)
        interrupt_activity(AI_FULL_HP);

    you.redraw_hit_points = true;
}

void rot_hp(int hp_loss)
{
    you.hp_max_temp -= hp_loss;
    calc_hp();

    // Kill the player if they reached 0 maxhp.
    ouch(0, NON_MONSTER, KILLED_BY_ROTTING);

    if (you.species != SP_GHOUL)
        xom_is_stimulated(hp_loss * 25);

    you.redraw_hit_points = true;
}

void unrot_hp(int hp_recovered)
{
    you.hp_max_temp += hp_recovered;
    if (you.hp_max_temp > 0)
        you.hp_max_temp = 0;

    calc_hp();

    you.redraw_hit_points = true;
}

int player_rotted()
{
    return -you.hp_max_temp;
}

void rot_mp(int mp_loss)
{
    you.mp_max_temp -= mp_loss;
    calc_mp();

    you.redraw_magic_points = true;
}

void inc_max_hp(int hp_gain)
{
    you.hp += hp_gain;
    you.hp_max_perm += hp_gain;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = true;
}

void dec_max_hp(int hp_loss)
{
    you.hp_max_perm -= hp_loss;
    calc_hp();

    take_note(Note(NOTE_MAXHP_CHANGE, you.hp_max));
    you.redraw_hit_points = true;
}

// Use of floor: false = hp max, true = hp min. {dlb}
void deflate_hp(int new_level, bool floor)
{
    ASSERT(!crawl_state.game_is_arena());

    if (floor && you.hp < new_level)
        you.hp = new_level;
    else if (!floor && you.hp > new_level)
        you.hp = new_level;

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_hit_points = true;
}

void set_hp(int new_amount)
{
    ASSERT(!crawl_state.game_is_arena());

    you.hp = new_amount;

    if (you.hp > you.hp_max)
        you.hp = you.hp_max;

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_hit_points = true;
}

void set_mp(int new_amount)
{
    ASSERT(!crawl_state.game_is_arena());

    you.magic_points = new_amount;

    if (you.magic_points > you.max_magic_points)
        you.magic_points = you.max_magic_points;

    take_note(Note(NOTE_MP_CHANGE, you.magic_points, you.max_magic_points));

    // Must remain outside conditional, given code usage. {dlb}
    you.redraw_magic_points = true;
}

// If trans is true, being berserk and/or transformed is taken into account
// here. Else, the base hp is calculated. If rotted is true, calculate the
// real max hp you'd have if the rotting was cured.
int get_real_hp(bool trans, bool rotted)
{
    int hitp;

    hitp  = you.experience_level * 11 / 2 + 8;
    hitp += you.hp_max_perm;
    // Important: we shouldn't add Heroism boosts here.
    hitp += you.experience_level * you.skill(SK_FIGHTING, 5, true) / 70
          + (you.skill(SK_FIGHTING, 3, true) + 1) / 2;

    // Racial modifier.
    hitp *= 10 + species_hp_modifier(you.species);
    hitp /= 10;

    // Mutations that increase HP by a percentage
    hitp *= 100 + (player_mutation_level(MUT_ROBUST) * 10)
                + (you.attribute[ATTR_DIVINE_VIGOUR] * 5)
                + (player_mutation_level(MUT_RUGGED_BROWN_SCALES) ?
                   player_mutation_level(MUT_RUGGED_BROWN_SCALES) * 2 + 1 : 0)
                - (player_mutation_level(MUT_FRAIL) * 10);

    hitp /= 100;

    if (!rotted)
        hitp += you.hp_max_temp;

    if (trans)
        hitp += you.scan_artefacts(ARTP_HP);

    // Being berserk makes you resistant to damage. I don't know why.
    if (trans && you.berserk())
        hitp = hitp * 3 / 2;

    if (trans) // Some transformations give you extra hp.
        hitp = hitp * form_hp_mod() / 10;

    return hitp;
}

int get_real_mp(bool include_items)
{
    int enp = you.experience_level + you.mp_max_perm;
    enp += (you.experience_level * species_mp_modifier(you.species) + 1) / 3;

    int spell_extra = you.skill(SK_SPELLCASTING, you.experience_level * 3, true) / 14
                    + you.skill(SK_SPELLCASTING, 1, true);
    int invoc_extra = you.skill(SK_INVOCATIONS, you.experience_level * 2, true) / 13
                    + you.skill(SK_INVOCATIONS, 1, true) / 3;
    int evoc_extra = you.skill(SK_EVOCATIONS, you.experience_level, true) / 6;

    enp += max(spell_extra, max(invoc_extra, evoc_extra));
    enp = stepdown_value(enp, 9, 18, 45, 100);

    // This is our "rotted" base (applied after scaling):
    enp += you.mp_max_temp;

    // Yes, we really do want this duplication... this is so the stepdown
    // doesn't truncate before we apply the rotted base.  We're doing this
    // the nice way. -- bwr
    enp = min(enp, 50);

    // Analogous to ROBUST/FRAIL
    enp *= 100 + (player_mutation_level(MUT_HIGH_MAGIC) * 10)
               + (you.attribute[ATTR_DIVINE_VIGOUR] * 5)
               - (player_mutation_level(MUT_LOW_MAGIC) * 10);
    enp /= 100;

    // Now applied after scaling so that power items are more useful -- bwr
    if (include_items)
    {
        enp +=  9 * you.wearing(EQ_RINGS, RING_MAGICAL_POWER);
        enp +=      you.scan_artefacts(ARTP_MAGICAL_POWER);

        if (you.wearing(EQ_STAFF, STAFF_POWER))
            enp += 5 + enp * 2 / 5;
    }

    if (enp > 50)
        enp = 50 + ((enp - 50) / 2);

    if (include_items && you.wearing_ego(EQ_WEAPON, SPWPN_ANTIMAGIC))
        enp /= 3;

    enp = max(enp, 0);

    return enp;
}

int get_contamination_level()
{
    const int glow = you.magic_contamination;

    if (glow > 60000)
        return glow / 20000 + 3;
    if (glow > 40000)
        return 5;
    if (glow > 25000)
        return 4;
    if (glow > 15000)
        return 3;
    if (glow > 5000)
        return 2;
    if (glow > 0)
        return 1;

    return 0;
}

string describe_contamination(int cont)
{
    if (cont > 5)
        return "You are engulfed in a nimbus of crackling magics!";
    else if (cont == 5)
        return "Your entire body has taken on an eerie glow!";
    else if (cont > 1)
    {
        return make_stringf("You are %s with residual magics%s",
                  (cont == 4) ? "practically glowing" :
                  (cont == 3) ? "heavily infused" :
                  (cont == 2) ? "contaminated"
                                   : "lightly contaminated",
                  (cont == 4) ? "!" : ".");
    }
    else if (cont == 1)
        return "You are very lightly contaminated with residual magic.";
    else
        return "";
}

// controlled is true if the player actively did something to cause
// contamination (such as drink a known potion of resistance),
// status_only is true only for the status output
void contaminate_player(int change, bool controlled, bool msg)
{
    ASSERT(!crawl_state.game_is_arena());

    int old_amount = you.magic_contamination;
    int old_level  = get_contamination_level();
    int new_level  = 0;

    you.magic_contamination = max(0, min(250000, you.magic_contamination + change));

    new_level = get_contamination_level();

    if (you.magic_contamination != old_amount)
        dprf("change: %d  radiation: %d", change, you.magic_contamination);

    if (msg && new_level >= 1 && old_level <= 1 && new_level != old_level)
        mprf("%s", describe_contamination(new_level).c_str());
    else if (msg && new_level != old_level)
    {
        if (old_level == 1 && new_level == 0)
            mpr("Your magical contamination has completely faded away.");
        else
        {
            mprf((change > 0) ? MSGCH_WARN : MSGCH_RECOVERY,
                 "You feel %s contaminated with magical energies.",
                 (change > 0) ? "more" : "less");
        }

        if (change > 0)
            xom_is_stimulated(new_level * 25);

        if (old_level > 1 && new_level <= 1 && you.invisible())
        {
            mpr("You fade completely from view now that you are no longer "
                "glowing from magical contamination.");
        }
    }

    if (you.magic_contamination > 0)
        learned_something_new(HINT_GLOWING);

    // Zin doesn't like mutations or mutagenic radiation.
    if (you_worship(GOD_ZIN))
    {
        // Whenever the glow status is first reached, give a warning message.
        if (old_level < 2 && new_level >= 2)
            did_god_conduct(DID_CAUSE_GLOWING, 0, false);
        // If the player actively did something to increase glowing,
        // Zin is displeased.
        else if (controlled && change > 0 && old_level > 1)
            did_god_conduct(DID_CAUSE_GLOWING, 1 + new_level, true);
    }
}

bool confuse_player(int amount, bool quiet)
{
    ASSERT(!crawl_state.game_is_arena());

    if (amount <= 0)
        return false;

    if (you.clarity())
    {
        if (!quiet)
            mpr("You feel momentarily confused.");
        return false;
    }

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        if (!quiet)
            mpr("Your divine stamina protects you from confusion!");
        return false;
    }

    const int old_value = you.duration[DUR_CONF];
    you.increase_duration(DUR_CONF, amount, 40);

    if (you.duration[DUR_CONF] > old_value)
    {
        you.check_awaken(500);

        if (!quiet)
        {
            mprf(MSGCH_WARN, "You are %sconfused.",
                 old_value > 0 ? "more " : "");
        }

        learned_something_new(HINT_YOU_ENCHANTED);

        xom_is_stimulated((you.duration[DUR_CONF] - old_value)
                           / BASELINE_DELAY);
    }

    return true;
}

bool curare_hits_player(int death_source, int levels, string name,
                        string source_name)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_poison() >= 3
        || player_res_poison() > 0 && !one_chance_in(5))
    {
        return false;
    }

    poison_player(roll_dice(levels, 12) + 1, source_name, name);

    int hurted = 0;

    if (you.res_asphyx() <= 0)
    {
        hurted = roll_dice(levels, 6);

        if (hurted)
        {
            you.increase_duration(DUR_BREATH_WEAPON, hurted,
                                  10*levels + random2(10*levels));
            mpr("You have difficulty breathing.");
            ouch(hurted, death_source, KILLED_BY_CURARE,
                 "curare-induced apnoea");
        }
    }

    potion_effect(POT_SLOWING, levels + random2(3*levels));

    return hurted > 0;
}

void paralyse_player(string source, int amount)
{
    if (!amount)
        amount = 2 + random2(6 + you.duration[DUR_PARALYSIS] / BASELINE_DELAY);

    you.paralyse(NULL, amount, source);
}

bool poison_player(int amount, string source, string source_aux, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from poison!");
        return false;
    }

    if (player_res_poison() >= 3)
    {
        dprf("Cannot poison, you are immune!");
        return false;
    }
    else if (!force && player_res_poison() > 0 && !one_chance_in(10))
        return false;

    const int old_value = you.duration[DUR_POISONING];
    const bool was_fatal = poison_is_lethal();

    if (player_res_poison() < 0)
        amount *= 2;

    you.duration[DUR_POISONING] += amount * 1000;

    if (you.duration[DUR_POISONING] > old_value)
    {
        if (poison_is_lethal() && !was_fatal)
            mprf(MSGCH_DANGER, "You are lethally poisoned!");
        else
        {
            mprf(MSGCH_WARN, "You are %spoisoned.",
                old_value > 0 ? "more " : "");
        }

        learned_something_new(HINT_YOU_POISON);
    }

    you.props["poisoner"] = source;
    you.props["poison_aux"] = source_aux;

    // Display the poisoned segment of our health, in case we take no damage
    you.redraw_hit_points = true;

    return amount;
}

int get_player_poisoning()
{
    if (player_res_poison() < 3)
    {
        // Approximate the effect of damage shaving by giving the first
        // 25 points of poison damage for 'free'
        if (you.species == SP_DEEP_DWARF)
            return max(0, (you.duration[DUR_POISONING] / 1000) - 25);
        else
            return you.duration[DUR_POISONING] / 1000;
    }
    else
        return 0;
}

// The amount of aut needed for poison to end if
// you.duration[DUR_POISONING] == dur, assuming no Chei/DD shenanigans.
// This function gives the following behavior:
// * 1/15 of current poison is removed every 10 aut normally
// * but speed of poison is capped between 0.025 and 1.000 HP/aut
static double _poison_dur_to_aut(double dur)
{
    // Poison already at minimum speed.
    if (dur < 15.0 * 250.0)
        return dur / 25.0;
    // Poison is not at maximum speed.
    if (dur < 15.0 * 10000.0)
        return 150.0 + 10.0 * log(dur / (15.0 * 250.0)) / log(15.0 / 14.0);
    return 150.0 + (dur - 15.0 * 10000.0) / 1000.0
                 + 10.0 * log(10000.0 / 250.0) / log(15.0 / 14.0);
}

// The inverse of the above function, i.e. the amount of poison needed
// to last for aut time.
static double _poison_aut_to_dur(double aut)
{
    // Amount of time that poison lasts at minimum speed.
    if (aut < 150.0)
        return aut * 25.0;
    // Amount of time that poison exactly at the maximum speed lasts.
    const double aut_from_max_speed = 150.0 + 10.0 * log(40.0) / log(15.0 / 14.0);
    if (aut < aut_from_max_speed)
        return 15.0 * 250.0 * exp(log(15.0 / 14.0) / 10.0 * (aut - 150.0));
    return 15.0 * 10000.0 + 1000.0 * (aut - aut_from_max_speed);
}

void handle_player_poison(int delay)
{
    const double cur_dur = you.duration[DUR_POISONING];
    const double cur_aut = _poison_dur_to_aut(cur_dur);

    // If Cheibriados has slowed your life processes, poison affects you less
    // quickly (you take the same total damage, but spread out over a longer
    // period of time).
    const double delay_scaling = (GOD_CHEIBRIADOS == you.religion && you.piety >= piety_breakpoint(0)) ? 2.0 / 3.0 : 1.0;

    const double new_aut = cur_aut - ((double) delay) * delay_scaling;
    const double new_dur = _poison_aut_to_dur(new_aut);

    const int decrease = you.duration[DUR_POISONING] - (int) new_dur;

    // Transforming into a form with no metabolism merely suspends the poison
    // but doesn't let your body get rid of it.
    // Hungry vampires are less affected by poison (not at all when bloodless).
    if (you.is_artificial() || you.is_undead
        && (you.is_undead != US_SEMI_UNDEAD || x_chance_in_y(4 - you.hunger_state, 4)))
    {
        return;
    }

    // Other sources of immunity (Zin, staff of Olgreb) let poison dissipate.
    bool do_dmg = (player_res_poison() >= 3 ? false : true);

    int dmg = (you.duration[DUR_POISONING] / 1000)
               - ((you.duration[DUR_POISONING] - decrease) / 1000);

    // Approximate old damage shaving by giving immunity to small amounts
    // of poison. Stronger poison will do the same damage as for non-DD
    // until it goes below the threshold, which is a bit weird, but
    // so is damage shaving.
    if (you.species == SP_DEEP_DWARF && you.duration[DUR_POISONING] - decrease < 25000)
    {
       dmg = (you.duration[DUR_POISONING] / 1000)
              - (25000 / 1000);
       if (dmg < 0)
           dmg = 0;
    }

    msg_channel_type channel = MSGCH_PLAIN;
    const char *adj = "";

    if (dmg > 6)
    {
        channel = MSGCH_DANGER;
        adj = "extremely ";
    }
    else if (dmg > 2)
    {
        channel = MSGCH_WARN;
        adj = "very ";
    }

    if (do_dmg && dmg > 0)
    {
        int oldhp = you.hp;
        ouch(dmg, NON_MONSTER, KILLED_BY_POISON);
        if (you.hp < oldhp)
            mprf(channel, "You feel %ssick.", adj);
    }

    // Now decrease the poison in our system
    reduce_player_poison(decrease);
}

void reduce_player_poison(int amount)
{
    if (amount <= 0)
        return;

    you.duration[DUR_POISONING] -= amount;

    // Less than 1 point of damage remaining, so just end the poison
    if (you.duration[DUR_POISONING] < 1000)
        you.duration[DUR_POISONING] = 0;

    if (you.duration[DUR_POISONING] <= 0)
    {
        you.duration[DUR_POISONING] = 0;
        you.props.erase("poisoner");
        you.props.erase("poison_aux");
        mprf(MSGCH_RECOVERY, "You are no longer poisoned.");
    }

    you.redraw_hit_points = true;
}

// Takes *current* regeneration rate into account. Might sometimes be
// incorrect, but hopefully if so then the player is surviving with 1 HP.
bool poison_is_lethal()
{
    if (you.hp <= 0)
        return get_player_poisoning();
    if (get_player_poisoning() < you.hp)
        return false;
    return poison_survival() <= 0;
}

// Try to predict the minimum value of the player's health in the coming
// turns given the current poison amount and regen rate.
int poison_survival()
{
    if (!get_player_poisoning())
        return you.hp;
    const int rr = player_regen();
    const bool chei = (you.religion == GOD_CHEIBRIADOS && you.piety >= piety_breakpoint(0));
    const bool dd = (you.species == SP_DEEP_DWARF);
    const int amount = you.duration[DUR_POISONING];
    const double full_aut = _poison_dur_to_aut(amount);
    // Calculate the poison amount at which regen starts to beat poison.
    double min_poison_rate = 0.25;
    if (dd)
        min_poison_rate = 25.0/15.0;
    if (chei)
        min_poison_rate /= 1.5;
    int regen_beats_poison;
    if (rr <= (int) (100.0 * min_poison_rate))
        regen_beats_poison = dd ? 25000 : 0;
    else
    {
        regen_beats_poison = 150 * rr;
        if (chei)
            regen_beats_poison = 3 * regen_beats_poison / 2;
    }

    if (rr == 0)
        return min(you.hp, you.hp - amount / 1000 + regen_beats_poison / 1000);

    // Calculate the amount of time until regen starts to beat poison.
    double poison_duration = full_aut - _poison_dur_to_aut(regen_beats_poison);

    if (poison_duration < 0)
        poison_duration = 0;
    if (chei)
        poison_duration *= 1.5;

    // Worst case scenario is right before natural regen gives you a point of
    // HP, so consider the nearest two such points.
    const int predicted_regen = (int) ((((double) you.hit_points_regeneration) + rr * poison_duration / 10.0) / 100.0);
    double test_aut1 = (100.0 * predicted_regen - 1.0 - ((double) you.hit_points_regeneration)) / (rr / 10.0);
    double test_aut2 = (100.0 * predicted_regen + 99.0 - ((double) you.hit_points_regeneration)) / (rr / 10.0);

    if (chei)
    {
        test_aut1 /= 1.5;
        test_aut2 /= 1.5;
    }

    const int test_amount1 = _poison_aut_to_dur(full_aut - test_aut1);
    const int test_amount2 = _poison_aut_to_dur(full_aut - test_aut2);

    int prediction1 = you.hp;
    int prediction2 = you.hp;

    // Don't look backwards in time.
    if (test_aut1 > 0)
        prediction1 -= (amount / 1000 - test_amount1 / 1000 - (predicted_regen - 1));
    prediction2 -= (amount / 1000 - test_amount2 / 1000 - predicted_regen);

    return min(prediction1, prediction2);
}

bool miasma_player(string source, string source_aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (you.res_rotting() || you.duration[DUR_DEATHS_DOOR])
        return false;

    if (you.duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from the miasma!");
        return false;
    }

    bool success = poison_player(5 + roll_dice(3, 12), source, source_aux);

    if (you.hp_max > 4 && coinflip())
    {
        rot_hp(1);
        success = true;
    }

    if (one_chance_in(3))
    {
        potion_effect(POT_SLOWING, 5);
        success = true;
    }

    return success;
}

bool napalm_player(int amount, string source, string source_aux)
{
    ASSERT(!crawl_state.game_is_arena());

    if (player_res_sticky_flame() || amount <= 0 || you.duration[DUR_WATER_HOLD])
        return false;

    const int old_value = you.duration[DUR_LIQUID_FLAMES];
    you.increase_duration(DUR_LIQUID_FLAMES, amount, 100);

    if (you.duration[DUR_LIQUID_FLAMES] > old_value)
        mprf(MSGCH_WARN, "You are covered in liquid flames!");

    you.props["napalmer"] = source;
    you.props["napalm_aux"] = source_aux;

    return true;
}

void dec_napalm_player(int delay)
{
    delay = min(delay, you.duration[DUR_LIQUID_FLAMES]);

    if (feat_is_watery(grd(you.pos())))
    {
        if (you.ground_level())
            mprf(MSGCH_WARN, "The flames go out!");
        else
            mprf(MSGCH_WARN, "You dip into the water, and the flames go out!");
        you.duration[DUR_LIQUID_FLAMES] = 0;
        you.props.erase("napalmer");
        you.props.erase("napalm_aux");
        return;
    }

    mprf(MSGCH_WARN, "You are covered in liquid flames!");

    expose_player_to_element(BEAM_NAPALM,
                             div_rand_round(delay * 4, BASELINE_DELAY));

    const int hurted = resist_adjust_damage(&you, BEAM_FIRE, player_res_fire(),
                                            random2avg(9, 2) + 1);

    ouch(hurted * delay / BASELINE_DELAY, NON_MONSTER, KILLED_BY_BURNING);

    you.duration[DUR_LIQUID_FLAMES] -= delay;
    if (you.duration[DUR_LIQUID_FLAMES] <= 0)
    {
        you.props.erase("napalmer");
        you.props.erase("napalm_aux");
    }
}

bool slow_player(int turns)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0)
        return false;

    if (stasis_blocks_effect(true, "%s rumbles.", 20, "%s rumbles."))
        return false;

    // Doubling these values because moving while slowed takes twice the
    // usual delay.
    turns = haste_mul(turns);
    int threshold = haste_mul(100);

    if (you.duration[DUR_SLOW] >= threshold * BASELINE_DELAY)
        mpr("You already are as slow as you could be.");
    else
    {
        if (you.duration[DUR_SLOW] == 0)
            mpr("You feel yourself slow down.");
        else
            mpr("You feel as though you will be slow longer.");

        you.increase_duration(DUR_SLOW, turns, threshold);
        learned_something_new(HINT_YOU_ENCHANTED);
    }

    return true;
}

void dec_slow_player(int delay)
{
    if (!you.duration[DUR_SLOW])
        return;

    if (you.duration[DUR_SLOW] > BASELINE_DELAY)
    {
        // Make slowing and hasting effects last as long.
        you.duration[DUR_SLOW] -= you.duration[DUR_HASTE]
            ? haste_mul(delay) : delay;
    }
    if (you.duration[DUR_SLOW] <= BASELINE_DELAY)
    {
        mprf(MSGCH_DURATION, "You feel yourself speed up.");
        you.duration[DUR_SLOW] = 0;
    }
}

// Exhaustion should last as long as slowing.
void dec_exhaust_player(int delay)
{
    if (!you.duration[DUR_EXHAUSTED])
        return;

    if (you.duration[DUR_EXHAUSTED] > BASELINE_DELAY)
    {
        you.duration[DUR_EXHAUSTED] -= you.duration[DUR_HASTE]
                                       ? haste_mul(delay) : delay;
    }
    if (you.duration[DUR_EXHAUSTED] <= BASELINE_DELAY)
    {
        mprf(MSGCH_DURATION, "You feel less exhausted.");
        you.duration[DUR_EXHAUSTED] = 0;
    }
}

bool haste_player(int turns, bool rageext)
{
    ASSERT(!crawl_state.game_is_arena());

    if (turns <= 0)
        return false;

    if (stasis_blocks_effect(true, "%s emits a piercing whistle.", 20,
                             "%s makes your neck tingle."))
    {
        return false;
    }

    // Cutting the nominal turns in half since hasted actions take half the
    // usual delay.
    turns = haste_div(turns);
    const int threshold = 40;

    if (!you.duration[DUR_HASTE])
        mpr("You feel yourself speed up.");
    else if (you.duration[DUR_HASTE] > threshold * BASELINE_DELAY)
        mpr("You already have as much speed as you can handle.");
    else if (!rageext)
    {
        mpr("You feel as though your hastened speed will last longer.");
        contaminate_player(1000, true); // always deliberate
    }

    you.increase_duration(DUR_HASTE, turns, threshold);

    return true;
}

void dec_haste_player(int delay)
{
    if (!you.duration[DUR_HASTE])
        return;

    if (you.duration[DUR_HASTE] > BASELINE_DELAY)
    {
        int old_dur = you.duration[DUR_HASTE];

        you.duration[DUR_HASTE] -= delay;

        int threshold = 6 * BASELINE_DELAY;
        // message if we cross the threshold
        if (old_dur > threshold && you.duration[DUR_HASTE] <= threshold)
        {
            mprf(MSGCH_DURATION, "Your extra speed is starting to run out.");
            if (coinflip())
                you.duration[DUR_HASTE] -= BASELINE_DELAY;
        }
    }
    else if (you.duration[DUR_HASTE] <= BASELINE_DELAY)
    {
        if (!you.duration[DUR_BERSERK])
            mprf(MSGCH_DURATION, "You feel yourself slow down.");
        you.duration[DUR_HASTE] = 0;
    }
}

void dec_disease_player(int delay)
{
    if (you.disease)
    {
        int rr = 50;

        // Extra regeneration means faster recovery from disease.
        // But not if not actually regenerating!
        if (player_mutation_level(MUT_SLOW_HEALING) < 3
            && !(you.species == SP_VAMPIRE && you.hunger_state == HS_STARVING))
        {
            rr += _player_bonus_regen();
        }

        // Trog's Hand.
        if (you.duration[DUR_TROGS_HAND])
            rr += 100;

        rr = div_rand_round(rr * delay, 50);

        you.disease -= rr;
        if (you.disease < 0)
            you.disease = 0;

        if (you.disease == 0)
            mprf(MSGCH_RECOVERY, "You feel your health improve.");
    }
}

static void _dec_elixir_hp(int delay)
{
    you.duration[DUR_ELIXIR_HEALTH] -= delay;
    if (you.duration[DUR_ELIXIR_HEALTH] < 0)
        you.duration[DUR_ELIXIR_HEALTH] = 0;

    int heal = (delay * you.hp_max / 10) / BASELINE_DELAY;
    if (!you.duration[DUR_DEATHS_DOOR])
        inc_hp(heal);
}

static void _dec_elixir_mp(int delay)
{
    you.duration[DUR_ELIXIR_MAGIC] -= delay;
    if (you.duration[DUR_ELIXIR_MAGIC] < 0)
        you.duration[DUR_ELIXIR_MAGIC] = 0;

    int heal = (delay * you.max_magic_points / 10) / BASELINE_DELAY;
    inc_mp(heal);
}

void dec_elixir_player(int delay)
{
    if (you.duration[DUR_ELIXIR_HEALTH])
        _dec_elixir_hp(delay);
    if (you.duration[DUR_ELIXIR_MAGIC])
        _dec_elixir_mp(delay);
}

bool flight_allowed(bool quiet)
{
    if (you.form == TRAN_TREE)
    {
        if (!quiet)
            mpr("Your roots keep you in place.");
        return false;
    }

    if (you.liquefied_ground())
    {
        if (!quiet)
            mpr("You can't fly while stuck in liquid ground.");
        return false;
    }

    if (you.duration[DUR_GRASPING_ROOTS])
    {
        if (!quiet)
            mpr("The grasping roots prevent you from becoming airborne.");
        return false;
    }

    return true;
}

void float_player()
{
    if (you.fishtail)
    {
        mpr("Your tail turns into legs as you fly out of the water.");
        merfolk_stop_swimming();
    }
    else if (you.tengu_flight())
        mpr("You swoop lightly up into the air.");
    else
        mpr("You fly up into the air.");

    if (you.species == SP_TENGU)
        you.redraw_evasion = true;
}

void fly_player(int pow, bool already_flying)
{
    if (!flight_allowed())
        return;

    bool standing = !you.airborne() && !already_flying;
    if (!already_flying)
        mprf(MSGCH_DURATION, "You feel %s buoyant.", standing ? "very" : "more");

    you.increase_duration(DUR_FLIGHT, 25 + random2(pow), 100);

    if (standing)
        float_player();
}

bool land_player(bool quiet)
{
    // there was another source keeping you aloft
    if (you.airborne())
        return false;

    if (!quiet)
        mpr("You float gracefully downwards.");
    if (you.species == SP_TENGU)
        you.redraw_evasion = true;
    you.attribute[ATTR_FLIGHT_UNCANCELLABLE] = 0;
    // Re-enter the terrain.
    move_player_to_grid(you.pos(), false);
    return true;
}

static void _end_water_hold()
{
    you.duration[DUR_WATER_HOLD] = 0;
    you.duration[DUR_WATER_HOLD_IMMUNITY] = 1;
    you.props.erase("water_holder");
}

void handle_player_drowning(int delay)
{
    if (you.duration[DUR_WATER_HOLD] == 1)
    {
        if (!you.res_water_drowning())
            mpr("You gasp with relief as air once again reaches your lungs.");
        _end_water_hold();
    }
    else
    {
        monster* mons = monster_by_mid(you.props["water_holder"].get_int());
        if (!mons || mons && !adjacent(mons->pos(), you.pos()))
        {
            if (you.res_water_drowning())
                mpr("The water engulfing you falls away.");
            else
                mpr("You gasp with relief as air once again reaches your lungs.");

            _end_water_hold();

        }
        else if (you.res_water_drowning())
        {
            // Reset so damage doesn't ramp up while able to breathe
            you.duration[DUR_WATER_HOLD] = 10;
        }
        else if (!you.res_water_drowning())
        {
            zin_recite_interrupt();

            you.duration[DUR_WATER_HOLD] += delay;
            int dam =
                div_rand_round((28 + stepdown((float)you.duration[DUR_WATER_HOLD], 28.0))
                                * delay,
                                BASELINE_DELAY * 10);
            ouch(dam, mons->mindex(), KILLED_BY_WATER);
            mprf(MSGCH_WARN, "Your lungs strain for air!");
        }
    }
}

int count_worn_ego(int which_ego)
{
    int result = 0;
    for (int slot = EQ_MIN_ARMOUR; slot <= EQ_MAX_ARMOUR; ++slot)
    {
        if (you.equip[slot] != -1 && !you.melded[slot]
            && get_armour_ego_type(you.inv[you.equip[slot]]) == which_ego)
        {
            result++;
        }
    }

    return result;
}

player::player()
    : kills(0), m_quiver(0)
{
    init();
}

player::player(const player &other)
    : kills(0), m_quiver(0)
{
    init();
    copy_from(other);
}

// Not called operator= because it is implemented in terms of the
// default operator=
void player::copy_from(const player &other)
{
    if (this == &other)
        return;

    KillMaster *saved_kills = kills;
    player_quiver* saved_quiver = m_quiver;

    // Rather than trying (and failing) to include explicit assignments
    // for every member at this point, we use the default operator=.
    *this = other;

    kills  = saved_kills;
    *kills = *(other.kills);
    m_quiver = saved_quiver;
    *m_quiver = *(other.m_quiver);
}

// player struct initialization
void player::init()
{
    // Permanent data:
    your_name.clear();
    species          = SP_UNKNOWN;
    species_name.clear();
    char_class       = JOB_UNKNOWN;
    class_name.clear();
    type             = MONS_PLAYER;
    mid              = MID_PLAYER;
    position.reset();

#ifdef WIZARD
    wizard = Options.wiz_mode == WIZ_YES;
#else
    wizard = false;
#endif
    birth_time       = time(0);

    // Long-term state:
    elapsed_time     = 0;
    elapsed_time_at_last_input = 0;

    hp               = 0;
    hp_max           = 0;
    hp_max_temp      = 0;
    hp_max_perm      = 0;

    magic_points     = 0;
    max_magic_points = 0;
    mp_max_temp      = 0;
    mp_max_perm      = 0;

    stat_loss.init(0);
    base_stats.init(0);
    stat_zero.init(0);

    hunger          = HUNGER_DEFAULT;
    hunger_state    = HS_SATIATED;
    disease         = 0;
    max_level       = 1;
    hit_points_regeneration   = 0;
    magic_points_regeneration = 0;
    experience       = 0;
    total_experience = 0;
    experience_level = 1;
    gold             = 0;
    zigs_completed   = 0;
    zig_max          = 0;

    equip.init(-1);
    melded.reset();
    unrand_reacts.reset();

    symbol          = MONS_PLAYER;
    form            = TRAN_NONE;

    for (int i = 0; i < ENDOFPACK; i++)
        inv[i].clear();
    runes.reset();
    obtainable_runes = 15;

    spells.init(SPELL_NO_SPELL);
    old_vehumet_gifts.clear();
    spell_no        = 0;
    vehumet_gifts.clear();
    char_direction  = GDT_DESCENDING;
    opened_zot      = false;
    royal_jelly_dead = false;
    transform_uncancellable = false;
    fishtail = false;

    pet_target      = MHITNOT;

    duration.init(0);
    rotting         = 0;
    berserk_penalty = 0;
    attribute.init(0);
    quiver.init(ENDOFPACK);

    last_timer_effect.init(0);
    next_timer_effect.init(20 * BASELINE_DELAY);

    is_undead       = US_ALIVE;

    friendly_pickup = 0;
    dead = false;
    lives = 0;
    deaths = 0;

#if TAG_MAJOR_VERSION == 34
    temperature = 1; // 1 is min; 15 is max.
    temperature_last = 1;
#endif

    xray_vision = false;

    init_skills();

    skill_menu_do = SKM_NONE;
    skill_menu_view = SKM_NONE;

    transfer_from_skill = SK_NONE;
    transfer_to_skill = SK_NONE;
    transfer_skill_points = 0;
    transfer_total_skill_points = 0;

    skill_cost_level = 1;
    exp_available = 0;
    zot_points = 0;

    item_description.init(255);
    unique_items.init(UNIQ_NOT_EXISTS);
    unique_creatures.reset();
    force_autopickup.init(0);

    if (kills)
        delete kills;
    kills = new KillMaster();

    where_are_you    = BRANCH_DUNGEON;
    depth            = 1;

    branch_stairs.init(0);

    religion         = GOD_NO_GOD;
    jiyva_second_name.clear();
    god_name.clear();
    piety            = 0;
    piety_hysteresis = 0;
    gift_timeout     = 0;
    penance.init(0);
    worshipped.init(0);
    num_current_gifts.init(0);
    num_total_gifts.init(0);
    one_time_ability_used.reset();
    piety_max.init(0);
    exp_docked       = 0;
    exp_docked_total = 0;

    mutation.init(0);
    innate_mutation.init(0);
    temp_mutation.init(0);
    demonic_traits.clear();

    magic_contamination = 0;

    had_book.reset();
    seen_spell.reset();
    seen_weapon.init(0);
    seen_armour.init(0);
    seen_misc.reset();

    octopus_king_rings = 0;

    normal_vision    = LOS_RADIUS;
    current_vision   = LOS_RADIUS;

    real_time        = 0;
    num_turns        = 0;
    exploration      = 0;

    last_view_update = 0;

    spell_letter_table.init(-1);
    ability_letter_table.init(ABIL_NON_ABILITY);

    uniq_map_tags.clear();
    uniq_map_names.clear();
    vault_list.clear();

    global_info = PlaceInfo();
    global_info.assert_validity();

    if (m_quiver)
        delete m_quiver;
    m_quiver = new player_quiver;

    props.clear();

    beholders.clear();
    fearmongers.clear();
    dactions.clear();
    level_stack.clear();
    type_ids.init(ID_UNKNOWN_TYPE);
    type_id_props.clear();

    zotdef_wave_name.clear();
    last_mid = 0;
    last_cast_spell = SPELL_NO_SPELL;

    // Non-saved UI state:
    prev_targ        = MHITNOT;
    prev_grd_targ.reset();
    prev_move.reset();

    travel_x         = 0;
    travel_y         = 0;
    travel_z         = level_id();

    running.clear();
    travel_ally_pace = false;
    received_weapon_warning = false;
    received_noskill_warning = false;
    wizmode_teleported_into_rock = false;
    ash_init_bondage(this);
    digging = false;

    delay_queue.clear();

    last_keypress_time = time(0);

    action_count.clear();

    branches_left.reset();

    // Volatile (same-turn) state:
    turn_is_over     = false;
    banished         = false;
    banished_by.clear();

    wield_change     = false;
    redraw_quiver    = false;
    redraw_status_flags = 0;
    redraw_hit_points   = false;
    redraw_magic_points = false;
#if TAG_MAJOR_VERSION == 34
    redraw_temperature  = false;
#endif
    redraw_stats.init(false);
    redraw_experience   = false;
    redraw_armour_class = false;
    redraw_evasion      = false;
    redraw_title        = false;

    flash_colour        = BLACK;
    flash_where         = nullptr;

    time_taken          = 0;
    shield_blocks       = 0;

    abyss_speed         = 0;
    for (int i = 0; i < NUM_SEEDS; i++)
        game_seeds[i] = random_int();

    old_hunger          = hunger;
    transit_stair       = DNGN_UNSEEN;
    entering_level      = false;

    reset_escaped_death();
    on_current_level    = true;
    walking             = 0;
    seen_portals        = 0;
    seen_invis          = false;
    frame_no            = 0;

    save                = 0;
    prev_save_version.clear();

    clear_constricted();
    constricting = 0;

    // Protected fields:
    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        branch_info[i].branch = (branch_type)i;
        branch_info[i].assert_validity();
    }
}

void player::init_skills()
{
    auto_training = !(Options.default_manual_training);
    skills.init(0);
    train.init(false);
    train_alt.init(false);
    training.init(0);
    can_train.reset();
    skill_points.init(0);
    ct_skill_points.init(0);
    skill_order.init(MAX_SKILL_ORDER);
    exercises.clear();
    exercises_all.clear();
}

player_save_info& player_save_info::operator=(const player& rhs)
{
    name             = rhs.your_name;
    experience       = rhs.experience;
    experience_level = rhs.experience_level;
    wizard           = rhs.wizard;
    species          = rhs.species;
    species_name     = rhs.species_name;
    class_name       = rhs.class_name;
    religion         = rhs.religion;
    god_name         = rhs.god_name;
    jiyva_second_name= rhs.jiyva_second_name;

    // [ds] Perhaps we should move game type to player?
    saved_game_type  = crawl_state.type;

    return *this;
}

bool player_save_info::operator<(const player_save_info& rhs) const
{
    return experience < rhs.experience
           || (experience == rhs.experience && name < rhs.name);
}

string player_save_info::short_desc() const
{
    ostringstream desc;

    const string qualifier = game_state::game_type_name_for(saved_game_type);
    if (!qualifier.empty())
        desc << "[" << qualifier << "] ";

    desc << name << ", a level " << experience_level << ' '
         << species_name << ' ' << class_name;

    if (religion == GOD_JIYVA)
        desc << " of " << god_name << " " << jiyva_second_name;
    else if (religion != GOD_NO_GOD)
        desc << " of " << god_name;

#ifdef WIZARD
    if (wizard)
        desc << " (WIZ)";
#endif

    return desc.str();
}

player::~player()
{
    delete kills;
    delete m_quiver;
    if (CrawlIsCrashing && save)
    {
        save->abort();
        delete save;
        save = 0;
    }
    ASSERT(!save); // the save file should be closed or deleted
}

flight_type player::flight_mode() const
{
    // Might otherwise be airborne, but currently stuck to the ground
    if (you.duration[DUR_GRASPING_ROOTS] || you.form == TRAN_TREE)
        return FL_NONE;

    if (duration[DUR_FLIGHT]
#if TAG_MAJOR_VERSION == 34
        || you.species == SP_DJINNI
#endif
        || attribute[ATTR_PERM_FLIGHT]
        || form == TRAN_WISP
        || form == TRAN_DRAGON
        || form == TRAN_BAT)
    {
        return FL_LEVITATE;
    }

    return FL_NONE;
}

bool player::is_banished() const
{
    return banished;
}

bool player::in_water() const
{
    return ground_level() && !beogh_water_walk()
           && feat_is_water(grd(pos()));
}

bool player::in_lava() const
{
    return ground_level() && feat_is_lava(grd(pos()));
}

bool player::in_liquid() const
{
    return in_water() || in_lava() || liquefied_ground();
}

bool player::can_swim(bool permanently) const
{
    // Transforming could be fatal if it would cause unequipment of
    // stat-boosting boots or heavy armour.
    return (species == SP_MERFOLK || species == SP_OCTOPODE
            || body_size(PSIZE_BODY) >= SIZE_GIANT
            || !permanently)
                && form_can_swim();
}

int player::visible_igrd(const coord_def &where) const
{
    // shop hack, etc.
    if (where.x == 0)
        return NON_ITEM;

    if (grd(where) == DNGN_LAVA
        || (grd(where) == DNGN_DEEP_WATER
            && !species_likes_water(species)))
    {
        return NON_ITEM;
    }

    return igrd(where);
}

bool player::has_spell(spell_type spell) const
{
    for (int i = 0; i < MAX_KNOWN_SPELLS; i++)
    {
        if (spells[i] == spell)
            return true;
    }

    return false;
}

bool player::cannot_speak() const
{
    if (silenced(pos()))
        return true;

    if (cannot_move()) // we allow talking during sleep ;)
        return true;

    // No transform that prevents the player from speaking yet.
    // ... yet setting this would prevent saccing junk and similar activities
    // for no good reason.
    return false;
}

string player::shout_verb() const
{
    switch (form)
    {
    case TRAN_DRAGON:
        return "roar";
    case TRAN_SPIDER:
        return "hiss";
    case TRAN_BAT:
    case TRAN_PORCUPINE:
        return "squeak";
    case TRAN_PIG:
        return coinflip() ? "squeal" : "oink";
    case TRAN_FUNGUS:
        return "sporulate";
    case TRAN_TREE:
        return "creak";
    case TRAN_WISP:
        return "whoosh"; // any wonder why?

    default:
        if (species == SP_FELID)
            return coinflip() ? "meow" : "yowl";
        // depends on SCREAM mutation
        int level = player_mutation_level(MUT_SCREAM);
        if (level <= 1)
            return "shout";
        else if (level == 2)
            return "yell";
        else // level == 3
            return "scream";
    }
}

int player::shout_volume() const
{
    int noise = 12;

    switch (form)
    {
    case TRAN_DRAGON:
        noise = 18;
        break;
    case TRAN_SPIDER:
        noise = 8;
        break;
    case TRAN_BAT:
    case TRAN_PORCUPINE:
    case TRAN_FUNGUS:
    case TRAN_WISP:
        noise = 4;
        break;

    default:
        break;
    }

    if (player_mutation_level(MUT_SCREAM))
        noise += 2 * (player_mutation_level(MUT_SCREAM) - 1);

    return noise;
}

void player::god_conduct(conduct_type thing_done, int level)
{
    ::did_god_conduct(thing_done, level);
}

void player::banish(actor *agent, const string &who)
{
    ASSERT(!crawl_state.game_is_arena());
    if (brdepth[BRANCH_ABYSS] == -1)
        return;

    if (elapsed_time <= attribute[ATTR_BANISHMENT_IMMUNITY])
    {
        mpr("You resist the pull of the Abyss.");
        return;
    }

    banished    = true;
    banished_by = who;
}

// For semi-undead species (Vampire!) reduce food cost for spells and abilities
// to 50% (hungry, very hungry) or zero (near starving, starving).
int calc_hunger(int food_cost)
{
    if (you.is_undead == US_SEMI_UNDEAD && you.hunger_state < HS_SATIATED)
    {
        if (you.hunger_state <= HS_NEAR_STARVING)
            return 0;

        return food_cost/2;
    }
    return food_cost;
}

bool player::paralysed() const
{
    return duration[DUR_PARALYSIS];
}

bool player::cannot_move() const
{
    return paralysed() || petrified();
}

bool player::confused() const
{
    return duration[DUR_CONF];
}

bool player::caught() const
{
    return attribute[ATTR_HELD];
}

bool player::petrifying() const
{
    return duration[DUR_PETRIFYING];
}

bool player::petrified() const
{
    return duration[DUR_PETRIFIED];
}

bool player::liquefied_ground() const
{
    return liquefied(pos())
           && ground_level() && !is_insubstantial();
}

int player::shield_block_penalty() const
{
    return 5 * shield_blocks * shield_blocks;
}

int player::shield_bonus() const
{
    const int shield_class = player_shield_class();
    if (shield_class <= 0)
        return -100;

    return random2avg(shield_class * 2, 2) / 3 - 1;
}

int player::shield_bypass_ability(int tohit) const
{
    return 15 + tohit / 2;
}

void player::shield_block_succeeded(actor *foe)
{
    actor::shield_block_succeeded(foe);

    shield_blocks++;
    practise(EX_SHIELD_BLOCK);
}

int player::missile_deflection() const
{
    if (attribute[ATTR_DEFLECT_MISSILES]
/*        || you_worship(GOD_QAZLAL)
           && !player_under_penance(GOD_QAZLAL)
           && you.piety >= piety_breakpoint(4)*/)
    {
        return 2;
    }
    if (attribute[ATTR_REPEL_MISSILES]
        || player_mutation_level(MUT_DISTORTION_FIELD) == 3
        || scan_artefacts(ARTP_RMSL, true)
        || you_worship(GOD_QAZLAL)
           && !player_under_penance(GOD_QAZLAL)
           && you.piety >= piety_breakpoint(3))
    {
        return 1;
    }
    return 0;
}

void player::ablate_deflection()
{
    int power;
    if (attribute[ATTR_DEFLECT_MISSILES])
    {
        power = calc_spell_power(SPELL_DEFLECT_MISSILES, true);
        if (one_chance_in(2 + power / 8))
        {
            attribute[ATTR_DEFLECT_MISSILES] = 0;
            mprf(MSGCH_DURATION, "You feel less protected from missiles.");
        }
    }
    else if (attribute[ATTR_REPEL_MISSILES])
    {
        power = calc_spell_power(SPELL_REPEL_MISSILES, true);
        if (one_chance_in(2 + power / 8))
        {
            attribute[ATTR_REPEL_MISSILES] = 0;
            mprf(MSGCH_DURATION, "You feel less protected from missiles.");
        }
    }
}

int player::unadjusted_body_armour_penalty() const
{
    const item_def *body_armour = slot_item(EQ_BODY_ARMOUR, false);
    if (!body_armour)
        return 0;

    const int base_ev_penalty = -property(*body_armour, PARM_EVASION);
    return base_ev_penalty;
}

// The EV penalty to the player for their worn body armour.
int player::adjusted_body_armour_penalty(int scale, bool use_size) const
{
    const int base_ev_penalty = unadjusted_body_armour_penalty();
    if (!base_ev_penalty)
        return 0;

    if (use_size)
    {
        const int size = body_size(PSIZE_BODY);

        const int size_bonus_factor = (size - SIZE_MEDIUM) * scale / 4;

        return max(0, scale * base_ev_penalty
                      - size_bonus_factor * base_ev_penalty);
    }

    // New formula for effect of str on aevp: (2/5) * evp^2 / (str+3)
    return 2 * base_ev_penalty * base_ev_penalty
           * (450 - skill(SK_ARMOUR, 10))
           * scale
           / (5 * (strength() + 3))
           / 450;
}

// The EV penalty to the player for wearing their current shield.
int player::adjusted_shield_penalty(int scale) const
{
    const item_def *shield_l = slot_item(EQ_SHIELD, false);
    if (!shield_l)
        return 0;

    const int base_shield_penalty = -property(*shield_l, PARM_EVASION);
    return max(0, (base_shield_penalty * scale - skill(SK_SHIELDS, scale)
                  / _player_shield_racial_factor()));
}

int player::armour_tohit_penalty(bool random_factor, int scale) const
{
    return maybe_roll_dice(1, adjusted_body_armour_penalty(scale), random_factor);
}

int player::shield_tohit_penalty(bool random_factor, int scale) const
{
    return maybe_roll_dice(1, adjusted_shield_penalty(scale), random_factor);
}

int player::skill(skill_type sk, int scale, bool real, bool drained) const
{
    // wizard racechange, or upgraded old save
    if (is_useless_skill(sk))
        return 0;

    // skills[sk] might not be updated yet if this is in the middle of
    // skill training, so make sure to use the correct value.
    // This duplicates code in check_skill_level_change(), unfortunately.
    int actual_skill = skills[sk];
    while (1)
    {
        if (actual_skill < 27
            && skill_points[sk] >= skill_exp_needed(actual_skill + 1, sk))
        {
            ++actual_skill;
        }
        else if (skill_points[sk] < skill_exp_needed(actual_skill, sk))
        {
            actual_skill--;
            ASSERT(actual_skill >= 0);
        }
        else
            break;
    }

    int level = actual_skill * scale + get_skill_progress(sk, actual_skill, skill_points[sk], scale);
    if (real)
        return level;
    if (drained && you.attribute[ATTR_XP_DRAIN])
    {
        int drain_scale = max(0, (30 * 100 - you.attribute[ATTR_XP_DRAIN]) * scale);
        level = skill(sk, drain_scale, real, false);
        return max(0, (level - 30 * scale * you.attribute[ATTR_XP_DRAIN]) / (30 * 100));
    }
    if (duration[DUR_HEROISM] && sk <= SK_LAST_MUNDANE)
        level = min(level + 5 * scale, 27 * scale);
    if (penance[GOD_ASHENZARI])
        level = max(level - 4 * scale, level / 2);
    else if (religion == GOD_ASHENZARI && piety_rank() > 2)
    {
        if (skill_boost.count(sk)
            && skill_boost.find(sk)->second)
        {
            level = ash_skill_boost(sk, scale);
        }
    }

    return level;
}

int player_icemail_armour_class()
{
    if (!you.mutation[MUT_ICEMAIL])
        return 0;

    return you.duration[DUR_ICEMAIL_DEPLETED] ? 0 : ICEMAIL_MAX;
}

bool player_stoneskin()
{
#if TAG_MAJOR_VERSION == 34
    // Lava orcs ignore DUR_STONESKIN
    if (you.species == SP_LAVA_ORC)
    {
        // Most transformations conflict with stone skin.
        if (form_changed_physiology() && you.form != TRAN_STATUE)
            return false;

        return temperature_effect(LORC_STONESKIN);
    }
    else
#endif
    return you.duration[DUR_STONESKIN];
}

static int _stoneskin_bonus()
{
    if (!player_stoneskin())
        return 0;

    // Max +7.4 base
    int boost = 200;
#if TAG_MAJOR_VERSION == 34
    if (you.species == SP_LAVA_ORC)
        boost += 20 * you.experience_level;
    else
#endif
    boost += you.skill(SK_EARTH_MAGIC, 20);

    // Max additional +7.75 from statue form
    if (you.form == TRAN_STATUE)
    {
        boost += 100;
#if TAG_MAJOR_VERSION == 34
        if (you.species == SP_LAVA_ORC)
            boost += 25 * you.experience_level;
        else
#endif
        boost += you.skill(SK_EARTH_MAGIC, 25);
    }

    return boost;
}

int player::armour_class() const
{
    int AC = 0;

    for (int eq = EQ_MIN_ARMOUR; eq <= EQ_MAX_ARMOUR; ++eq)
    {
        if (eq == EQ_SHIELD)
            continue;

        if (!player_wearing_slot(eq))
            continue;

        const item_def& item   = inv[equip[eq]];
        const int ac_value     = property(item, PARM_AC) * 100;
        const int beogh_bonus  = _player_armour_beogh_bonus(item);

        // [ds] effectively: ac_value * (22 + Arm) / 22, where Arm =
        // Armour Skill + beogh_bonus / 2.
        AC += ac_value * (440 + skill(SK_ARMOUR, 20) + beogh_bonus * 10) / 440;
        AC += item.plus * 100;

        // The deformed don't fit into body armour very well.
        // (This includes nagas and centaurs.)
        if (eq == EQ_BODY_ARMOUR && (player_mutation_level(MUT_DEFORMED)
                                     || player_mutation_level(MUT_PSEUDOPODS)))
        {
            AC -= ac_value / 2;
        }
    }

    AC += wearing(EQ_RINGS_PLUS, RING_PROTECTION) * 100;

    if (wearing_ego(EQ_WEAPON, SPWPN_PROTECTION))
        AC += 500;

    if (wearing_ego(EQ_SHIELD, SPARM_PROTECTION))
        AC += 300;

    AC += scan_artefacts(ARTP_AC) * 100;

    if (duration[DUR_ICY_ARMOUR])
        AC += 400 + skill(SK_ICE_MAGIC, 100) / 3;    // max 13

    AC += _stoneskin_bonus();

    if (mutation[MUT_ICEMAIL])
        AC += 100 * player_icemail_armour_class();

    if (duration[DUR_QAZLAL_AC])
        AC += 300;

    if (you.attribute[ATTR_DIVINE_AC] && !player_under_penance(GOD_QAZLAL))
        AC += 300;

    if (you.duration[DUR_CORROSION])
        AC -= 500 * you.props["corrosion_amount"].get_int();

    if (!player_is_shapechanged()
        || (form == TRAN_DRAGON && player_genus(GENPC_DRACONIAN))
        || (form == TRAN_STATUE && species == SP_GARGOYLE))
    {
        // Being a lich doesn't preclude the benefits of hide/scales -- bwr
        //
        // Note: Even though necromutation is a high level spell, it does
        // allow the character full armour (so the bonus is low). -- bwr
        if (form == TRAN_LICH)
            AC += 600;

        if (player_genus(GENPC_DRACONIAN))
        {
            AC += 400 + 100 * (experience_level / 3);  // max 13
            if (species == SP_GREY_DRACONIAN) // no breath
                AC += 500;
            if (form == TRAN_DRAGON)
                AC += 1000;
        }
        else
        {
            switch (species)
            {
            case SP_NAGA:
                AC += 100 * experience_level / 3;              // max 9
                break;

            case SP_GARGOYLE:
                AC += 200 + 100 * experience_level * 2 / 5     // max 20
                          + 100 * (max(0, experience_level - 7) * 2 / 5);
                if (form == TRAN_STATUE)
                    AC += 1300 + skill(SK_EARTH_MAGIC, 50);
                break;

            default:
                break;
            }
        }
    }
    else
    {
        // transformations:
        switch (form)
        {
        case TRAN_NONE:
        case TRAN_APPENDAGE:
        case TRAN_BLADE_HANDS:
        case TRAN_LICH:  // can wear normal body armour (no bonus)
            break;

#if TAG_MAJOR_VERSION == 34
        case TRAN_JELLY:
#endif
        case TRAN_BAT:
        case TRAN_PIG:
        case TRAN_PORCUPINE:
        case TRAN_SHADOW: // no bonus
            break;

        case TRAN_SPIDER: // low level (small bonus), also gets EV
            AC += 200;
            break;

        case TRAN_ICE_BEAST:
            AC += 500 + skill(SK_ICE_MAGIC, 25) + 25;    // max 12

            if (duration[DUR_ICY_ARMOUR])
                AC += 100 + skill(SK_ICE_MAGIC, 25);     // max +7
            break;

        case TRAN_WISP:
            AC += 500 + 50 * experience_level;
            break;
        case TRAN_FUNGUS:
            AC += 1200;
            break;
        case TRAN_DRAGON: // Draconians handled above
            AC += 1600;
            break;

        case TRAN_STATUE: // main ability is armour (high bonus)
            AC += 1700 + skill(SK_EARTH_MAGIC, 50);// max 30
            // Stoneskin bonus already accounted for.
            break;

        case TRAN_TREE: // extreme bonus, no EV
            AC += 2000 + 50 * experience_level;
            break;
        }
    }

    // Scale mutations, etc.  Statues don't get an AC benefit from scales,
    // since the scales are made of the same stone as everything else.
    AC += player_mutation_level(MUT_TOUGH_SKIN)
          ? player_mutation_level(MUT_TOUGH_SKIN) * 100 : 0;                   // +1, +2, +3
    AC += player_mutation_level(MUT_SHAGGY_FUR)
          ? player_mutation_level(MUT_SHAGGY_FUR) * 100 : 0;                   // +1, +2, +3
    AC += player_mutation_level(MUT_GELATINOUS_BODY)
          ? (player_mutation_level(MUT_GELATINOUS_BODY) == 3 ? 200 : 100) : 0; // +1, +1, +2
    AC += _mut_level(MUT_IRIDESCENT_SCALES, MUTACT_FULL)
          ? 200 + _mut_level(MUT_IRIDESCENT_SCALES, MUTACT_FULL) * 200 : 0;    // +4, +6, +8
    AC += _mut_level(MUT_LARGE_BONE_PLATES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_LARGE_BONE_PLATES, MUTACT_FULL) * 100 : 0;    // +2, +3, +4
    AC += _mut_level(MUT_ROUGH_BLACK_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_ROUGH_BLACK_SCALES, MUTACT_FULL) * 300 : 0;   // +4, +7, +10
    AC += _mut_level(MUT_RUGGED_BROWN_SCALES, MUTACT_FULL) * 100;              // +1, +2, +3
    AC += _mut_level(MUT_ICY_BLUE_SCALES, MUTACT_FULL) * 100 +
          (_mut_level(MUT_ICY_BLUE_SCALES, MUTACT_FULL) > 1 ? 100 : 0);        // +1, +3, +4
    AC += _mut_level(MUT_MOLTEN_SCALES, MUTACT_FULL) * 100 +
          (_mut_level(MUT_MOLTEN_SCALES, MUTACT_FULL) > 1 ? 100 : 0);          // +1, +3, +4
    AC += _mut_level(MUT_SLIMY_GREEN_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_SLIMY_GREEN_SCALES, MUTACT_FULL) * 100 : 0;   // +2, +3, +4
    AC += _mut_level(MUT_THIN_METALLIC_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_THIN_METALLIC_SCALES, MUTACT_FULL) * 100 : 0; // +2, +3, +4
    AC += _mut_level(MUT_YELLOW_SCALES, MUTACT_FULL)
          ? 100 + _mut_level(MUT_YELLOW_SCALES, MUTACT_FULL) * 100 : 0;        // +2, +3, +4

    return AC / 100;
}
 /**
  * Guaranteed damage reduction.
  *
  * The percentage of the damage received that is guaranteed to be reduced
  * by the armour. As the AC roll is done before GDR is applied, GDR is only
  * useful when the AC roll is inferior to it. Therefore a higher GDR means
  * more damage reduced, but also more often.
  *
  * \f[ GDR = 14 \times (base\_AC - 2)^\frac{1}{2} \f]
  *
  * \return GDR as a percentage.
  **/
int player::gdr_perc() const
{
    switch (form)
    {
    case TRAN_DRAGON:
        return 34; // base AC 8
    case TRAN_STATUE:
        return species == SP_GARGOYLE ? 50
                                      : 39; // like plate (AC 10)
    case TRAN_TREE:
        return 48;
    default:
        break;
    }

    const item_def *body_armour = slot_item(EQ_BODY_ARMOUR, false);

    int body_base_AC = (species == SP_GARGOYLE ? 5 : 0);
    if (body_armour)
        body_base_AC += property(*body_armour, PARM_AC);

    // We take a sqrt here because damage prevented by GDR is
    // actually proportional to the square of the GDR percentage
    // (assuming you have enough AC).
    int gdr = 14 * sqrt(max(body_base_AC - 2, 0));

    return gdr;
}

int player::melee_evasion(const actor *act, ev_ignore_type evit) const
{
    return player_evasion(evit)
           - (is_constricted() ? 3 : 0)
           - ((!act || act->visible_to(this)
               || (evit & EV_IGNORE_HELPLESS)) ? 0 : 10)
           - (you_are_delayed()
              && !(evit & EV_IGNORE_HELPLESS)
              && !delay_is_run(current_delay_action())? 5 : 0);
}

bool player::heal(int amount, bool max_too)
{
    ASSERT(!max_too);
    ::inc_hp(amount);
    return true; /* TODO Check whether the player was healed. */
}

mon_holy_type player::holiness() const
{
    if (species == SP_GARGOYLE || form == TRAN_STATUE || form == TRAN_WISP
        || petrified())
    {
        return MH_NONLIVING;
    }

    if (is_undead)
        return MH_UNDEAD;

    return MH_NATURAL;
}

bool player::undead_or_demonic() const
{
    // This is only for TSO-related stuff, so demonspawn are included.
    return is_undead || species == SP_DEMONSPAWN;
}

bool player::is_holy(bool check_spells) const
{
    if (is_good_god(religion) && check_spells)
        return true;

    return false;
}

bool player::is_unholy(bool check_spells) const
{
    return species == SP_DEMONSPAWN;
}

bool player::is_evil(bool check_spells) const
{
    if (holiness() == MH_UNDEAD)
        return true;

    if (is_evil_god(religion) && check_spells)
        return true;

    return false;
}

// This is a stub. Check is used only for silver damage. Worship of chaotic
// gods should probably be checked in the non-existing player::is_unclean,
// which could be used for something Zin-related (such as a priestly monster).
bool player::is_chaotic() const
{
    return false;
}

bool player::is_artificial() const
{
    return species == SP_GARGOYLE || form == TRAN_STATUE || petrified();
}

bool player::is_unbreathing() const
{
    switch (form)
    {
    case TRAN_LICH:
    case TRAN_STATUE:
    case TRAN_FUNGUS:
    case TRAN_TREE:
    case TRAN_WISP:
        return true;
    default:
        break;
    }

    if (petrified())
        return true;

    return player_mutation_level(MUT_UNBREATHING);
}

bool player::is_insubstantial() const
{
    return form == TRAN_WISP;
}

int player::res_acid(bool calc_unid) const
{
    return player_res_acid(calc_unid);
}

int player::res_fire() const
{
    return player_res_fire();
}

int player::res_holy_fire() const
{
#if TAG_MAJOR_VERSION == 34
    if (species == SP_DJINNI)
        return 3;
#endif
    return actor::res_holy_fire();
}

int player::res_steam() const
{
    return player_res_steam();
}

int player::res_cold() const
{
    return player_res_cold();
}

int player::res_elec() const
{
    return player_res_electricity() * 2;
}

int player::res_water_drowning() const
{
    int rw = 0;

    if (is_unbreathing()
        || species == SP_MERFOLK && !form_changed_physiology()
        || species == SP_OCTOPODE && !form_changed_physiology()
        || form == TRAN_ICE_BEAST)
    {
        rw++;
    }

#if TAG_MAJOR_VERSION == 34
    // A fiery lich/hot statue suffers from quenching but not drowning, so
    // neutral resistance sounds ok.
    if (species == SP_DJINNI)
        rw--;
#endif

    return rw;
}

int player::res_asphyx() const
{
    // The unbreathing are immune to asphyxiation.
    if (is_unbreathing())
        return 1;

    return 0;
}

int player::res_poison(bool temp) const
{
    return player_res_poison(true, temp);
}

int player::res_rotting(bool temp) const
{
    if (temp
        && (petrified() || form == TRAN_STATUE || form == TRAN_WISP
            || form == TRAN_SHADOW))
    {
        return 3;
    }

    if (player_mutation_level(MUT_ROT_IMMUNITY))
        return 3;

    switch (is_undead)
    {
    default:
    case US_ALIVE:
        return 0;

    case US_HUNGRY_DEAD:
        return 1; // rottable by Zin, not by necromancy

    case US_SEMI_UNDEAD:
        if (temp && hunger_state < HS_SATIATED)
            return 1;
        return 0; // no permanent resistance

    case US_UNDEAD:
        if (!temp && form == TRAN_LICH)
            return 0;
        return 3; // full immunity
    }
}

int player::res_sticky_flame() const
{
    return player_res_sticky_flame();
}

int player::res_holy_energy(const actor *attacker) const
{
    if (undead_or_demonic())
        return -2;

    if (is_evil())
        return -1;

    if (is_holy())
        return 1;

    return 0;
}

int player::res_negative_energy(bool intrinsic_only) const
{
    return player_prot_life(!intrinsic_only, true, !intrinsic_only);
}

int player::res_torment() const
{
    return player_res_torment();
}

int player::res_wind() const
{
    // Full control of the winds around you can negate a hostile tornado.
    return duration[DUR_TORNADO] ? 1 : 0;
}

int player::res_petrify(bool temp) const
{
    if (player_mutation_level(MUT_PETRIFICATION_RESISTANCE))
        return 1;

    if (temp && (form == TRAN_STATUE || form == TRAN_WISP))
        return 1;
    return 0;
}

int player::res_constrict() const
{
    if (is_insubstantial())
        return 3;
    if (form == TRAN_PORCUPINE
        || player_mutation_level(MUT_SPINY))
    {
        return 3;
    }
    return 0;
}

int player::res_magic() const
{
    return player_res_magic();
}

int player_res_magic(bool calc_unid, bool temp)
{
    int rm = 0;

    if (temp && you.form == TRAN_SHADOW)
        return MAG_IMMUNE;

    switch (you.species)
    {
    default:
        rm = you.experience_level * 3;
        break;
    case SP_HIGH_ELF:
    case SP_SLUDGE_ELF:
    case SP_DEEP_ELF:
    case SP_VAMPIRE:
    case SP_DEMIGOD:
    case SP_OGRE:
    case SP_FORMICID:
        rm = you.experience_level * 4;
        break;
    case SP_NAGA:
    case SP_MUMMY:
    case SP_VINE_STALKER:
        rm = you.experience_level * 5;
        break;
    case SP_PURPLE_DRACONIAN:
    case SP_DEEP_DWARF:
    case SP_FELID:
        rm = you.experience_level * 6;
        break;
    case SP_SPRIGGAN:
        rm = you.experience_level * 7;
        break;
    }

    // randarts
    rm += 40 * you.scan_artefacts(ARTP_MAGIC, calc_unid);

    // armour
    rm += 40 * you.wearing_ego(EQ_ALL_ARMOUR, SPARM_MAGIC_RESISTANCE, calc_unid);

    // rings of magic resistance
    rm += 40 * you.wearing(EQ_RINGS, RING_PROTECTION_FROM_MAGIC, calc_unid);

    // Mutations
    rm += 40 * player_mutation_level(MUT_MAGIC_RESISTANCE);

    // transformations
    if (you.form == TRAN_LICH && temp)
        rm += 40;

    // Trog's Hand
    if (you.duration[DUR_TROGS_HAND] && temp)
        rm += 80;

    // Enchantment effect
    if (you.duration[DUR_LOWERED_MR] && temp)
        rm /= 2;

    if (rm < 0)
        rm = 0;

    return rm;
}

bool player::no_tele(bool calc_unid, bool permit_id, bool blinking) const
{
    if (duration[DUR_DIMENSION_ANCHOR])
        return true;

    if (crawl_state.game_is_sprint() && !blinking)
        return true;

    if (form == TRAN_TREE)
        return true;

    return has_notele_item(calc_unid)
           || stasis_blocks_effect(calc_unid, NULL)
           || crawl_state.game_is_zotdef() && orb_haloed(pos());
}

bool player::fights_well_unarmed(int heavy_armour_penalty)
{
    return x_chance_in_y(skill(SK_UNARMED_COMBAT, 10), 200)
        && x_chance_in_y(2, 1 + heavy_armour_penalty);
}

bool player::cancellable_flight() const
{
    return duration[DUR_FLIGHT] && !permanent_flight()
           && !attribute[ATTR_FLIGHT_UNCANCELLABLE];
}

bool player::permanent_flight() const
{
    return attribute[ATTR_PERM_FLIGHT]
#if TAG_MAJOR_VERSION == 34
        || species == SP_DJINNI
#endif
        ;
}

bool player::racial_permanent_flight() const
{
    return species == SP_TENGU && experience_level >= 14
#if TAG_MAJOR_VERSION == 34
        || species == SP_DJINNI
#endif
        || species == SP_BLACK_DRACONIAN && experience_level >= 14
        || species == SP_GARGOYLE && experience_level >= 14;
}

bool player::tengu_flight() const
{
    // Only Tengu get perks for flying.
    return species == SP_TENGU && flight_mode();
}

/**
 * Returns the HP cost (per MP) of casting a spell.
 *
 * Checks to see if the player is wielding the Majin-Bo.
 *
 * @return        The HP cost (per MP) of casting a spell.
 **/
int player::spell_hp_cost() const
{
    int cost = 0;

    if (player_equip_unrand(UNRAND_MAJIN))
        cost += 1;

    return cost;
}

/**
 * Returns true if player spellcasting is considered unholy.
 *
 * Checks to see if the player is wielding the Majin-Bo.
 *
 * @return          Whether player spellcasting is an unholy act.
 */
bool player::spellcasting_unholy() const
{
    return player_equip_unrand(UNRAND_MAJIN);
}

bool player::nightvision() const
{
    return is_undead
           || (religion == GOD_DITHMENOS && piety >= piety_breakpoint(0))
           || (religion == GOD_YREDELEMNUL && piety >= piety_breakpoint(2));
}

reach_type player::reach_range() const
{
    const item_def *wpn = weapon();
    if (wpn)
        return weapon_reach(*wpn);
    return REACH_NONE;
}

monster_type player::mons_species(bool zombie_base) const
{
    return player_species_to_mons_species(species);
}

bool player::poison(actor *agent, int amount, bool force)
{
    return ::poison_player(amount, agent? agent->name(DESC_A, true) : "", "",
                           force);
}

void player::expose_to_element(beam_type element, int _strength,
                               bool slow_cold_blood)
{
    ::expose_player_to_element(element, _strength, slow_cold_blood);
}

void player::blink(bool allow_partial_control)
{
    random_blink(allow_partial_control);
}

void player::teleport(bool now, bool wizard_tele)
{
    ASSERT(!crawl_state.game_is_arena());

    if (now)
        you_teleport_now(true, wizard_tele);
    else
        you_teleport();
}

int player::hurt(const actor *agent, int amount, beam_type flavour,
                 bool cleanup_dead, bool attacker_effects)
{
    // We ignore cleanup_dead here.
    if (!agent)
    {
        // FIXME: This can happen if a deferred_damage_fineff does damage
        // to a player from a dead monster.  We should probably not do that,
        // but it could be tricky to fix, so for now let's at least avoid
        // a crash even if it does mean funny death messages.
        ouch(amount, NON_MONSTER, KILLED_BY_MONSTER, "",
             false, "posthumous revenge", attacker_effects);
    }
    else if (agent->is_monster())
    {
        const monster* mon = agent->as_monster();
        ouch(amount, mon->mindex(),
             flavour == BEAM_WATER ? KILLED_BY_WATER : KILLED_BY_MONSTER,
             "", mon->visible_to(this), NULL, attacker_effects);
    }
    else
    {
        // Should never happen!
        die("player::hurt() called for self-damage");
    }

    if ((flavour == BEAM_NUKE || flavour == BEAM_DISINTEGRATION) && can_bleed())
        blood_spray(pos(), type, amount / 5);

    return amount;
}

void player::drain_stat(stat_type s, int amount, actor *attacker)
{
    if (attacker == NULL)
        lose_stat(s, amount, false, "");
    else if (attacker->is_monster())
        lose_stat(s, amount, attacker->as_monster(), false);
    else if (attacker->is_player())
        lose_stat(s, amount, false, "suicide");
    else
        lose_stat(s, amount, false, "");
}

bool player::rot(actor *who, int amount, int immediate, bool quiet)
{
    ASSERT(!crawl_state.game_is_arena());

    if (amount <= 0 && immediate <= 0)
        return false;

    if (res_rotting() || duration[DUR_DEATHS_DOOR])
    {
        mpr("You feel terrible.");
        return false;
    }

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from decay!");
        return false;
    }

    if (immediate > 0)
        rot_hp(immediate);

    // Either this, or the actual rotting message should probably
    // be changed so that they're easier to tell apart. -- bwr
    if (!quiet)
    {
        mprf(MSGCH_WARN, "You feel your flesh %s away!",
             (rotting > 0 || immediate) ? "rotting" : "start to rot");
    }

    rotting += amount;

    learned_something_new(HINT_YOU_ROTTING);

    if (one_chance_in(4))
        sicken(50 + random2(100));

    return true;
}

bool player::drain_exp(actor *who, bool quiet, int pow)
{
    return ::drain_exp(!quiet, pow);
}

void player::confuse(actor *who, int str)
{
    confuse_player(str);
}

/**
 * Paralyse the player for str turns.
 *
 *  Duration is capped at 13.
 *
 * @param who Pointer to the actor who paralysed the player.
 * @param str The number of turns the paralysis will last.
 * @param source Description of the source of the paralysis.
 */
void player::paralyse(actor *who, int str, string source)
{
    ASSERT(!crawl_state.game_is_arena());

    // The shock is too mild to do damage.
    if (stasis_blocks_effect(true, "%s gives you a mild electric shock."))
        return;

    // The who check has an effect in a few cases, most notably making
    // Death's Door + Borg's paralysis unblockable.
    if (who && (duration[DUR_PARALYSIS] || duration[DUR_PARALYSIS_IMMUNITY]))
    {
        mpr("You shrug off the repeated paralysis!");
        return;
    }

    int &paralysis(duration[DUR_PARALYSIS]);

    if (source.empty() && who)
        source = who->name(DESC_A);

    if (!paralysis && !source.empty())
    {
        take_note(Note(NOTE_PARALYSIS, str, 0, source.c_str()));
        props["paralysed_by"] = source;
    }

    mprf("You %s the ability to move!",
         paralysis ? "still don't have" : "suddenly lose");

    str *= BASELINE_DELAY;
    if (str > paralysis && (paralysis < 3 || one_chance_in(paralysis)))
        paralysis = str;

    if (paralysis > 13 * BASELINE_DELAY)
        paralysis = 13 * BASELINE_DELAY;

    stop_constricting_all();
    end_searing_ray();
}

void player::petrify(actor *who, bool force)
{
    ASSERT(!crawl_state.game_is_arena());

    if (res_petrify() && !force)
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from petrification!");
        return;
    }

    if (petrifying())
    {
        mpr("Your limbs have turned to stone.");
        duration[DUR_PETRIFYING] = 1;
        return;
    }

    if (petrified())
        return;

    duration[DUR_PETRIFYING] = 3 * BASELINE_DELAY;

    redraw_evasion = true;
    mprf(MSGCH_WARN, "You are slowing down.");
}

bool player::fully_petrify(actor *foe, bool quiet)
{
    duration[DUR_PETRIFIED] = 6 * BASELINE_DELAY
                        + random2(4 * BASELINE_DELAY);
    redraw_evasion = true;
    mpr("You have turned to stone.");

    end_searing_ray();

    return true;
}

void player::slow_down(actor *foe, int str)
{
    ::slow_player(str);
}


int player::has_claws(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring claws with them
        if (form == TRAN_DRAGON)
            return 3;

        // blade hands override claws
        if (form == TRAN_BLADE_HANDS)
            return 0;

        // Most forms suppress natural claws.
        if (!form_keeps_mutations())
            return 0;
    }

    if (const int c = species_has_claws(species))
        return c;

    return player_mutation_level(MUT_CLAWS, allow_tran);
}

bool player::has_usable_claws(bool allow_tran) const
{
    return !player_wearing_slot(EQ_GLOVES) && has_claws(allow_tran);
}

int player::has_talons(bool allow_tran) const
{
    // XXX: Do merfolk in water belong under allow_tran?
    if (fishtail)
        return 0;

    return player_mutation_level(MUT_TALONS, allow_tran);
}

bool player::has_usable_talons(bool allow_tran) const
{
    return !player_wearing_slot(EQ_BOOTS) && has_talons(allow_tran);
}

int player::has_fangs(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring fangs with them
        if (form == TRAN_DRAGON)
            return 3;
    }

    return player_mutation_level(MUT_FANGS, allow_tran);
}

int player::has_usable_fangs(bool allow_tran) const
{
    return has_fangs(allow_tran);
}

int player::has_tail(bool allow_tran) const
{
    if (allow_tran)
    {
        // these transformations bring a tail with them
        if (form == TRAN_DRAGON)
            return 1;

        // Most transformations suppress a tail.
        if (!form_keeps_mutations())
            return 0;
    }

    // XXX: Do merfolk in water belong under allow_tran?
    if (player_genus(GENPC_DRACONIAN)
        || fishtail
        || player_mutation_level(MUT_STINGER, allow_tran))
    {
        return 1;
    }

    return 0;
}

int player::has_usable_tail(bool allow_tran) const
{
    // TSO worshippers don't use their stinger in order
    // to avoid poisoning.
    if (religion == GOD_SHINING_ONE
        && player_mutation_level(MUT_STINGER, allow_tran) > 0)
    {
        return 0;
    }

    return has_tail(allow_tran);
}

// Whether the player has a usable offhand for the
// purpose of punching.
bool player::has_usable_offhand() const
{
    if (player_wearing_slot(EQ_SHIELD))
        return false;

    const item_def* wp = slot_item(EQ_WEAPON);
    return !wp || hands_reqd(*wp) != HANDS_TWO;
}

bool player::has_usable_tentacle() const
{
    return usable_tentacles();
}

int player::usable_tentacles() const
{
    int numtentacle = has_usable_tentacles();

    if (numtentacle == 0)
        return false;

    int free_tentacles = numtentacle - num_constricting();

    if (shield())
        free_tentacles -= 2;

    const item_def* wp = slot_item(EQ_WEAPON);
    if (wp)
    {
        hands_reqd_type hands_req = hands_reqd(*wp);
        free_tentacles -= 2 * hands_req + 2;
    }

    return free_tentacles;
}

int player::has_pseudopods(bool allow_tran) const
{
    return player_mutation_level(MUT_PSEUDOPODS, allow_tran);
}

int player::has_usable_pseudopods(bool allow_tran) const
{
    return has_pseudopods(allow_tran);
}

int player::has_tentacles(bool allow_tran) const
{
    if (allow_tran)
    {
        // Most transformations suppress tentacles.
        if (!form_keeps_mutations())
            return 0;
    }

    if (species == SP_OCTOPODE)
        return 8;

    return 0;
}

int player::has_usable_tentacles(bool allow_tran) const
{
    return has_tentacles(allow_tran);
}

bool player::sicken(int amount, bool allow_hint, bool quiet)
{
    ASSERT(!crawl_state.game_is_arena());

    if (res_rotting() || amount <= 0)
        return false;

    if (duration[DUR_DIVINE_STAMINA] > 0)
    {
        mpr("Your divine stamina protects you from disease!");
        return false;
    }

    if (!quiet)
        mpr("You feel ill.");

    disease += amount * BASELINE_DELAY;
    if (disease > 210 * BASELINE_DELAY)
        disease = 210 * BASELINE_DELAY;

    if (allow_hint)
        learned_something_new(HINT_YOU_SICK);
    return true;
}

bool player::can_see_invisible(bool calc_unid, bool items) const
{
    if (crawl_state.game_is_arena())
        return true;

    if (items)
    {
        if (wearing(EQ_RINGS, RING_SEE_INVISIBLE, calc_unid)
            // armour: (checks head armour only)
            || wearing_ego(EQ_HELMET, SPARM_SEE_INVISIBLE)
            // randart gear
            || scan_artefacts(ARTP_EYESIGHT, calc_unid) > 0)
        {
            return true;
        }
    }

    // Possible to have both with a temp mutation.
    if (player_mutation_level(MUT_ACUTE_VISION)
        && !player_mutation_level(MUT_BLURRY_VISION))
    {
        return true;
    }

    // antennae give sInvis at 3
    if (player_mutation_level(MUT_ANTENNAE) == 3)
        return true;

    if (player_mutation_level(MUT_EYEBALLS) == 3)
        return true;

    if (religion == GOD_ASHENZARI && piety >= piety_breakpoint(2)
        && !player_under_penance())
    {
        return true;
    }

    return false;
}

bool player::can_see_invisible() const
{
    return can_see_invisible(true, true);
}

bool player::invisible() const
{
    return (duration[DUR_INVIS] || form == TRAN_SHADOW)
           && !backlit();
}

bool player::visible_to(const actor *looker) const
{
    if (crawl_state.game_is_arena())
        return false;

    if (this == looker)
        return can_see_invisible() || !invisible();

    const monster* mon = looker->as_monster();
    return mon->friendly()
        || (mons_sense_invis(mon) && distance2(pos(), mon->pos()) <= dist_range(4))
        || (!mon->has_ench(ENCH_BLIND)
            && (!invisible() || mon->can_see_invisible()));
}

bool player::backlit(bool check_haloed, bool self_halo) const
{
    if (get_contamination_level() > 1 || duration[DUR_CORONA]
        || duration[DUR_LIQUID_FLAMES] || duration[DUR_QUAD_DAMAGE])
    {
        return true;
    }
    if (check_haloed)
    {
        return !umbraed() && haloed()
               && (self_halo || halo_radius2() == -1);
    }
    return false;
}

bool player::umbra(bool check_haloed, bool self_halo) const
{
    if (backlit())
        return false;

    if (check_haloed)
    {
        return umbraed() && !haloed()
               && (self_halo || umbra_radius2() == -1);
    }
    return false;
}

bool player::glows_naturally() const
{
    return false;
}

// This is the imperative version.
void player::backlight()
{
    if (!duration[DUR_INVIS] && form != TRAN_SHADOW)
    {
        if (duration[DUR_CORONA] || glows_naturally())
            mpr("You glow brighter.");
        else
            mpr("You are outlined in light.");

        increase_duration(DUR_CORONA, random_range(15, 35), 250);
    }
    else
    {
        mpr("You feel strangely conspicuous.");

        increase_duration(DUR_CORONA, random_range(3, 5), 250);
    }
}

bool player::has_lifeforce() const
{
    const mon_holy_type holi = holiness();

    return holi == MH_NATURAL || holi == MH_PLANT;
}

bool player::can_mutate() const
{
    return true;
}

bool player::can_safely_mutate() const
{
    if (!can_mutate())
        return false;

    return !is_undead
           || is_undead == US_SEMI_UNDEAD;
}

// Is the player too undead to bleed, rage, and polymorph?
bool player::is_lifeless_undead() const
{
    if (is_undead == US_SEMI_UNDEAD)
        return hunger_state <= HS_SATIATED;
    else
        return is_undead != US_ALIVE;
}

bool player::can_polymorph() const
{
    return !(transform_uncancellable || is_lifeless_undead());
}

bool player::can_bleed(bool allow_tran) const
{
    if (allow_tran)
    {
        // These transformations don't bleed. Lichform is handled as undead.
        if (form == TRAN_STATUE || form == TRAN_ICE_BEAST
            || form == TRAN_SPIDER || form == TRAN_TREE
            || form == TRAN_FUNGUS || form == TRAN_PORCUPINE
            || form == TRAN_SHADOW)
        {
            return false;
        }
    }

    if (is_lifeless_undead()
#if TAG_MAJOR_VERSION == 34
        || species == SP_DJINNI
#endif
        || holiness() == MH_NONLIVING)
    {   // demonspawn and demigods have a mere drop of taint
        return false;
    }

    return true;
}

bool player::is_stationary() const
{
    return form == TRAN_TREE;
}

bool player::malmutate(const string &reason)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_mutate())
        return false;

    const mutation_type mut_quality = one_chance_in(5) ? RANDOM_MUTATION
                                                       : RANDOM_BAD_MUTATION;
    if (mutate(mut_quality, reason))
    {
        learned_something_new(HINT_YOU_MUTATED);
        return true;
    }
    return false;
}

bool player::polymorph(int pow)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_polymorph())
        return false;

    transformation_type f = TRAN_NONE;

    // Be unreliable over lava.  This is not that important as usually when
    // it matters you'll have temp flight and thus that pig will fly (and
    // when flight times out, we'll have roasted bacon).
    for (int tries = 0; tries < 3; tries++)
    {
        // Whole-body transformations only; mere appendage doesn't seem fitting.
        f = random_choose_weighted(
            100, TRAN_BAT,
            100, TRAN_FUNGUS,
            100, TRAN_PIG,
            100, TRAN_TREE,
            100, TRAN_PORCUPINE,
            100, TRAN_WISP,
             20, TRAN_SPIDER,
             20, TRAN_ICE_BEAST,
              5, TRAN_STATUE,
              1, TRAN_DRAGON,
              0);
        // need to do a dry run first, as Zin's protection has a random factor
        if (transform(pow, f, false, true))
            break;
        f = TRAN_NONE;
    }

    if (f && transform(pow, f))
    {
        transform_uncancellable = true;
        return true;
    }
    return false;
}

bool player::is_icy() const
{
    return form == TRAN_ICE_BEAST;
}

bool player::is_fiery() const
{
    return false;
}

bool player::is_skeletal() const
{
    return false;
}

void player::shiftto(const coord_def &c)
{
    crawl_view.shift_player_to(c);
    set_position(c);
    clear_far_constrictions();
}

void player::reset_prev_move()
{
    prev_move.reset();
}

bool player::asleep() const
{
    return duration[DUR_SLEEP];
}

bool player::cannot_act() const
{
    return asleep() || cannot_move();
}

bool player::can_throw_large_rocks() const
{
    return species_can_throw_large_rocks(species);
}

bool player::can_smell() const
{
    return species != SP_MUMMY;
}

void player::hibernate(int)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_hibernate() || duration[DUR_SLEEP_IMMUNITY])
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    stop_constricting_all();
    end_searing_ray();
    mpr("You fall asleep.");

    stop_delay();
    flash_view(DARKGREY);

    // Do this *after* redrawing the view, or viewwindow() will no-op.
    set_duration(DUR_SLEEP, 3 + random2avg(5, 2));
}

void player::put_to_sleep(actor*, int power)
{
    ASSERT(!crawl_state.game_is_arena());

    if (!can_sleep() || duration[DUR_SLEEP_IMMUNITY])
    {
        canned_msg(MSG_YOU_UNAFFECTED);
        return;
    }

    mpr("You fall asleep.");

    stop_constricting_all();
    end_searing_ray();
    stop_delay();
    flash_view(DARKGREY);

    // As above, do this after redraw.
    set_duration(DUR_SLEEP, 5 + random2avg(power/10, 5));
}

void player::awake()
{
    ASSERT(!crawl_state.game_is_arena());

    duration[DUR_SLEEP] = 0;
    duration[DUR_SLEEP_IMMUNITY] = 1;
    mpr("You wake up.");
    flash_view(BLACK);
}

void player::check_awaken(int disturbance)
{
    if (asleep() && x_chance_in_y(disturbance + 1, 50))
        awake();
}

int player::beam_resists(bolt &beam, int hurted, bool doEffects, string source)
{
    return check_your_resists(hurted, beam.flavour, source, &beam, doEffects);
}

void player::set_place_info(PlaceInfo place_info)
{
    place_info.assert_validity();

    if (place_info.is_global())
        global_info = place_info;
    else
        branch_info[place_info.branch] = place_info;
}

vector<PlaceInfo> player::get_all_place_info(bool visited_only,
                                             bool dungeon_only) const
{
    vector<PlaceInfo> list;

    for (int i = 0; i < NUM_BRANCHES; i++)
    {
        if (visited_only && branch_info[i].num_visits == 0
            || dungeon_only && !is_connected_branch((branch_type)i))
        {
            continue;
        }
        list.push_back(branch_info[i]);
    }

    return list;
}

// Used for falling into traps and other bad effects, but is a slightly
// different effect from the player invokable ability.
bool player::do_shaft()
{
    if (!is_valid_shaft_level())
        return false;

    // Handle instances of do_shaft() being invoked magically when
    // the player isn't standing over a shaft.
    if (get_trap_type(pos()) != TRAP_SHAFT)
    {
        switch (grd(pos()))
        {
        case DNGN_FLOOR:
        case DNGN_OPEN_DOOR:
        // what's the point of this list?
        case DNGN_TRAP_MECHANICAL:
        case DNGN_TRAP_TELEPORT:
        case DNGN_TRAP_ALARM:
        case DNGN_TRAP_ZOT:
        case DNGN_TRAP_SHAFT:
        case DNGN_UNDISCOVERED_TRAP:
        case DNGN_ENTER_SHOP:
            if (!ground_level() || body_weight() == 0)
                return true;
            break;

        default:
            return false;
        }

    }

    down_stairs(DNGN_TRAP_SHAFT);

    return true;
}

bool player::can_do_shaft_ability(bool quiet) const
{
    if (attribute[ATTR_HELD])
    {
        if (!quiet)
            mprf("You can't shaft yourself while %s.", held_status());
        return false;
    }

    switch (grd(pos()))
    {
    case DNGN_FLOOR:
    case DNGN_OPEN_DOOR:
        if (!is_valid_shaft_level())
        {
            if (!quiet)
                mpr("You can't shaft yourself on this level.");
            return false;
        }
        break;

    default:
        if (!quiet)
            mpr("You can't shaft yourself on this terrain.");
        return false;
    }

    return true;
}

// Like do_shaft, but forced by the player.
// It has a slightly different set of rules.
bool player::do_shaft_ability()
{
    if (can_do_shaft_ability(true))
    {
        mpr("A shaft appears beneath you!");
        down_stairs(DNGN_TRAP_SHAFT, true);
        return true;
    }
    else
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        redraw_screen();
        return false;
    }
}

bool player::did_escape_death() const
{
    return escaped_death_cause != NUM_KILLBY;
}

void player::reset_escaped_death()
{
    escaped_death_cause = NUM_KILLBY;
    escaped_death_aux   = "";
}

void player::add_gold(int delta)
{
    set_gold(gold + delta);
}

void player::del_gold(int delta)
{
    set_gold(gold - delta);
}

void player::set_gold(int amount)
{
    ASSERT(amount >= 0);

    if (amount != gold)
    {
        const int old_gold = gold;
        gold = amount;
        shopping_list.gold_changed(old_gold, gold);
    }
}

void player::increase_duration(duration_type dur, int turns, int cap,
                               const char* msg)
{
    if (msg)
        mpr(msg);
    cap *= BASELINE_DELAY;

    duration[dur] += turns * BASELINE_DELAY;
    if (cap && duration[dur] > cap)
        duration[dur] = cap;
}

void player::set_duration(duration_type dur, int turns,
                          int cap, const char * msg)
{
    duration[dur] = 0;
    increase_duration(dur, turns, cap, msg);
}

void player::goto_place(const level_id &lid)
{
    where_are_you = static_cast<branch_type>(lid.branch);
    depth = lid.depth;
    ASSERT_RANGE(depth, 1, brdepth[where_are_you] + 1);
}

bool player::attempt_escape(int attempts)
{
    monster *themonst;

    if (!is_constricted())
        return true;

    themonst = monster_by_mid(constricted_by);
    ASSERT(themonst);
    escape_attempts += attempts;

    // player breaks free if (4+n)d(8+str/4) >= 5d(8+HD/4)
    if (roll_dice(4 + escape_attempts, 8 + div_rand_round(strength(), 4))
        >= roll_dice(5, 8 + div_rand_round(themonst->hit_dice, 4)))
    {
        mprf("You escape %s's grasp.", themonst->name(DESC_THE, true).c_str());

        // Stun the monster to prevent it from constricting again right away.
        themonst->speed_increment -= 5;

        stop_being_constricted(true);

        return true;
    }
    else
    {
        mprf("Your attempt to break free from %s fails, but you feel that "
             "another attempt might succeed.",
             themonst->name(DESC_THE, true).c_str());
        turn_is_over = true;
        return false;
    }
}

void player::sentinel_mark(bool trap)
{
    if (duration[DUR_SENTINEL_MARK])
    {
        mpr("The mark upon you grows brighter.");
        increase_duration(DUR_SENTINEL_MARK, random_range(20, 40), 180);
    }
    else
    {
        mprf(MSGCH_WARN, "A sentinel's mark forms upon you.");
        increase_duration(DUR_SENTINEL_MARK, trap ? random_range(25, 40)
                                                  : random_range(35, 60),
                          250);
    }
}

bool player::made_nervous_by(const coord_def &p)
{
    if (form != TRAN_FUNGUS)
        return false;
    monster* mons = monster_at(p);
    if (mons && !mons_is_firewood(mons))
        return false;
    for (monster_near_iterator mi(&you); mi; ++mi)
    {
        if (!mons_is_wandering(*mi)
            && !mi->asleep()
            && !mi->confused()
            && !mi->cannot_act()
            && !mons_is_firewood(*mi)
            && !mi->wont_attack()
            && !mi->neutral())
        {
            return true;
        }
    }
    return false;
}

void player::weaken(actor *attacker, int pow)
{
    if (!duration[DUR_WEAK])
        mprf(MSGCH_WARN, "You feel your attacks grow feeble.");
    else
        mprf(MSGCH_WARN, "You feel as though you will be weak longer.");

    increase_duration(DUR_WEAK, pow + random2(pow + 3), 50);
}

/**
 * Check if the player is about to die from flight/form expiration.
 *
 * Check whether the player is on a cell which would be deadly if not for some
 * temporary condition, and if such condition is expiring. In that case, we
 * give a strong warning to the player. The actual message printing is done
 * by the caller.
 *
 * @param dur the duration to check for dangerous expiration.
 * @param p the coordinates of the cell to check. Defaults to player position.
 * @return whether the player is in immediate danger.
 */
bool need_expiration_warning(duration_type dur, dungeon_feature_type feat)
{
    if (!is_feat_dangerous(feat, true) || !dur_expiring(dur))
        return false;

    if (dur == DUR_FLIGHT)
        return true;
    else if (dur == DUR_TRANSFORMATION
             && (!you.airborne() || form_can_fly()))
    {
        return true;
    }
    return false;
}

bool need_expiration_warning(duration_type dur, coord_def p)
{
    return need_expiration_warning(dur, env.grid(p));
}

bool need_expiration_warning(dungeon_feature_type feat)
{
    return need_expiration_warning(DUR_FLIGHT, feat)
           || need_expiration_warning(DUR_TRANSFORMATION, feat);
}

bool need_expiration_warning(coord_def p)
{
    return need_expiration_warning(env.grid(p));
}

static string _constriction_description()
{
    string cinfo = "";
    vector<string> c_name;

    const int num_free_tentacles = you.usable_tentacles();
    if (num_free_tentacles)
    {
        cinfo += make_stringf("You have %d tentacle%s available for constriction.",
                              num_free_tentacles,
                              num_free_tentacles > 1 ? "s" : "");
    }
    // name of what this monster is constricted by, if any
    if (you.is_constricted())
    {
        if (!cinfo.empty())
            cinfo += "\n";

        cinfo += make_stringf("You are being %s by %s.",
                      you.held == HELD_MONSTER ? "held" : "constricted",
                      monster_by_mid(you.constricted_by)->name(DESC_A).c_str());
    }

    if (you.constricting && !you.constricting->empty())
    {
        actor::constricting_t::const_iterator i;
        for (i = you.constricting->begin(); i != you.constricting->end(); ++i)
        {
            monster *whom = monster_by_mid(i->first);
            ASSERT(whom);
            c_name.push_back(whom->name(DESC_A));
        }

        if (!cinfo.empty())
            cinfo += "\n";

        cinfo += "You are constricting ";
        cinfo += comma_separated_line(c_name.begin(), c_name.end());
        cinfo += ".";
    }

    return cinfo;
}

void count_action(caction_type type, int subtype)
{
    pair<caction_type, int> pair(type, subtype);
    if (!you.action_count.count(pair))
        you.action_count[pair].init(0);
    you.action_count[pair][you.experience_level - 1]++;
}

/**
 *   The player's radius of monster detection.
 *   @returns  the radius in which a player can detect monsters.
**/
int player_monster_detect_radius()
{
    int radius = player_mutation_level(MUT_ANTENNAE) * 2;

    if (player_equip_unrand(UNRAND_BOOTS_ASSASSIN))
        radius = max(radius, 4);
    if (you_worship(GOD_ASHENZARI) && !player_under_penance())
        radius = max(radius, you.piety / 20);
    return min(radius, LOS_RADIUS);
}

/**
 * Return true if the player has the Orb of Zot.
 * @returns True if the player has the Orb, false otherwise.
 */
bool player_has_orb()
{
    return you.char_direction == GDT_ASCENDING;
}

bool player::form_uses_xl() const
{
    // No body parts that translate in any way to something fisticuffs could
    // matter to, the attack mode is different.  Plus, it's weird to have
    // users of one particular [non-]weapon be effective for this
    // unintentional form while others can just run or die.  I believe this
    // should apply to more forms, too.  [1KB]
    return form == TRAN_WISP || form == TRAN_FUNGUS;
}

bool player::can_device_heal()
{
    return mutation[MUT_NO_DEVICE_HEAL] < 3;
}

#if TAG_MAJOR_VERSION == 34
// Lava orcs!
int temperature()
{
    return (int) you.temperature;
}

int temperature_last()
{
    return (int) you.temperature_last;
}

void temperature_check()
{
    // Whether to ignore caps on incrementing temperature
    bool ignore_cap = you.duration[DUR_BERSERK];

    // These numbers seem to work pretty well, but they're definitely experimental:
    int tension = get_tension(GOD_NO_GOD); // Raw tension

    // It would generally be better to handle this at the tension level and have temperature much more closely tied to tension.

    // For testing, but super handy for that!
    // mprf("Tension value: %d", tension);

    // Increment temp to full if you're in lava.
    if (feat_is_lava(env.grid(you.pos())) && you.ground_level())
    {
        // If you're already very hot, no message,
        // but otherwise it lets you know you're being
        // brought up to max temp.
        if (temperature() <= TEMP_FIRE)
            mpr("The lava instantly superheats you.");
        you.temperature = TEMP_MAX;
        ignore_cap = true;
        // Otherwise, your temperature naturally decays.
    }
    else
        temperature_decay();

    // Follow this up with 1 additional decrement each turn until
    // you're not hot enough to boil water.
    if (feat_is_water(env.grid(you.pos())) && you.ground_level()
        && temperature_effect(LORC_PASSIVE_HEAT))
    {
        temperature_decrement(1);

        for (adjacent_iterator ai(you.pos()); ai; ++ai)
        {
            const coord_def p(*ai);
            if (in_bounds(p)
                && env.cgrid(p) == EMPTY_CLOUD
                && !cell_is_solid(p)
                && one_chance_in(5))
            {
                place_cloud(CLOUD_STEAM, *ai, 2 + random2(5), &you);
            }
        }
    }

    // Next, add temperature from tension. Can override temperature loss from water!
    temperature_increment(tension);

    // Cap net temperature change to 1 per turn if no exceptions.
    float tempchange = you.temperature - you.temperature_last;
    if (!ignore_cap && tempchange > 1)
        you.temperature = you.temperature_last + 1;
    else if (tempchange < -1)
        you.temperature = you.temperature_last - 1;

    // Handle any effects that change with temperature.
    temperature_changed(tempchange);

    // Save your new temp as your new 'old' temperature.
    you.temperature_last = you.temperature;
}

void temperature_increment(float degree)
{
    // No warming up while you're exhausted!
    if (you.duration[DUR_EXHAUSTED])
        return;

    you.temperature += sqrt(degree);
    if (temperature() >= TEMP_MAX)
        you.temperature = TEMP_MAX;
}

void temperature_decrement(float degree)
{
    // No cooling off while you're angry!
    if (you.duration[DUR_BERSERK])
        return;

    you.temperature -= degree;
    if (temperature() <= TEMP_MIN)
        you.temperature = TEMP_MIN;
}

void temperature_changed(float change)
{
    // Arbitrary - how big does a swing in a turn have to be?
    float pos_threshold = .25;
    float neg_threshold = -1 * pos_threshold;

    // For INCREMENTS:

    // Check these no-nos every turn.
    if (you.temperature >= TEMP_WARM)
    {
        // Handles condensation shield, ozo's armour, icemail.
        expose_player_to_element(BEAM_FIRE, 0);

        // Handled separately because normally heat doesn't affect this.
        if (you.form == TRAN_ICE_BEAST || you.form == TRAN_STATUE)
            untransform(true, false);
    }

    // Just reached the temp that kills off stoneskin.
    if (change > pos_threshold && temperature_tier(TEMP_WARM))
    {
        mprf(MSGCH_DURATION, "Your stony skin melts.");
        you.redraw_armour_class = true;
    }

    // Passive heat stuff.
    if (change > pos_threshold && temperature_tier(TEMP_FIRE))
        mprf(MSGCH_DURATION, "You're getting fired up.");

    // Heat aura stuff.
    if (change > pos_threshold && temperature_tier(TEMP_MAX))
    {
        mprf(MSGCH_DURATION, "You blaze with the fury of an erupting volcano!");
        invalidate_agrid(true);
    }

    // For DECREMENTS (reverse order):
    if (change < neg_threshold && temperature_tier(TEMP_MAX))
        mprf(MSGCH_DURATION, "The intensity of your heat diminishes.");

    if (change < neg_threshold && temperature_tier(TEMP_FIRE))
        mprf(MSGCH_DURATION, "You're cooling off.");

    // Cooled down enough for stoneskin to kick in again.
    if (change < neg_threshold && temperature_tier(TEMP_WARM))
    {
        mprf(MSGCH_DURATION, "Your skin cools and hardens.");
        you.redraw_armour_class = true;
    }

    // If we're in this function, temperature changed, anyways.
    you.redraw_temperature = true;

#ifdef USE_TILE
    init_player_doll();
#endif

    // Just do this every turn to be safe. Can be fixed later if there
    // any performance issues.
    invalidate_agrid(true);
}

void temperature_decay()
{
    temperature_decrement(you.temperature / 10);
}

// Just a helper function to save space. Returns true if a
// threshold was crossed.
bool temperature_tier (int which)
{
    if (temperature() > which && temperature_last() <= which)
        return true;
    else if (temperature() < which && temperature_last() >= which)
        return true;
    else
        return false;
}

bool temperature_effect(int which)
{
    switch (which)
    {
        case LORC_FIRE_RES_I:
            return true; // 1-15
        case LORC_STONESKIN:
            return temperature() < TEMP_WARM; // 1-8
//      case nothing, right now:
//            return (you.temperature >= TEMP_COOL && you.temperature < TEMP_WARM); // 5-8
        case LORC_LAVA_BOOST:
            return temperature() >= TEMP_WARM && temperature() < TEMP_HOT; // 9-10
        case LORC_FIRE_RES_II:
            return temperature() >= TEMP_WARM; // 9-15
        case LORC_FIRE_RES_III:
        case LORC_FIRE_BOOST:
        case LORC_COLD_VULN:
            return temperature() >= TEMP_HOT; // 11-15
        case LORC_PASSIVE_HEAT:
            return temperature() >= TEMP_FIRE; // 13-15
        case LORC_HEAT_AURA:
            if (you_worship(GOD_BEOGH))
                return false;
            // Deliberate fall-through.
        case LORC_NO_SCROLLS:
            return temperature() >= TEMP_MAX; // 15

        default:
            return false;
    }
}

int temperature_colour(int temp)
{
    return (temp > TEMP_FIRE) ? LIGHTRED  :
           (temp > TEMP_HOT)  ? RED       :
           (temp > TEMP_WARM) ? YELLOW    :
           (temp > TEMP_ROOM) ? WHITE     :
           (temp > TEMP_COOL) ? LIGHTCYAN :
           (temp > TEMP_COLD) ? LIGHTBLUE : BLUE;
}

string temperature_string(int temp)
{
    return (temp > TEMP_FIRE) ? "lightred"  :
           (temp > TEMP_HOT)  ? "red"       :
           (temp > TEMP_WARM) ? "yellow"    :
           (temp > TEMP_ROOM) ? "white"     :
           (temp > TEMP_COOL) ? "lightcyan" :
           (temp > TEMP_COLD) ? "lightblue" : "blue";
}

string temperature_text(int temp)
{
    switch (temp)
    {
        case TEMP_MIN:
            return "rF+";
        case TEMP_COOL:
            return "";
        case TEMP_WARM:
            return "rF++; lava magic boost; Stoneskin melts";
        case TEMP_HOT:
            return "rF+++; rC-; fire magic boost";
        case TEMP_FIRE:
            return "Burn attackers";
        case TEMP_MAX:
            return "Burn surroundings; cannot read scrolls";
        default:
            return "";
    }
}
#endif
