/**
 * @file
 * @brief Functions for making use of inventory items.
**/

#include "AppHdr.h"

#include "item_use.h"

#include "ability.h"
#include "act-iter.h"
#include "areas.h"
#include "art-enum.h"
#include "artefact.h"
#include "cloud.h"
#include "colour.h"
#include "coordit.h"
#include "decks.h"
#include "delay.h"
#include "describe.h"
#include "effects.h"
#include "env.h"
#include "exercise.h"
#include "food.h"
#include "godabil.h"
#include "godconduct.h"
#include "goditem.h"
#include "hints.h"
#include "invent.h"
#include "evoke.h"
#include "itemprop.h"
#include "items.h"
#include "libutil.h"
#include "macro.h"
#include "makeitem.h"
#include "message.h"
#include "mgen_data.h"
#include "misc.h"
#include "mon-behv.h"
#include "mon-place.h"
#include "mutation.h"
#include "options.h"
#include "player-equip.h"
#include "player-stats.h"
#include "potion.h"
#include "random.h"
#include "religion.h"
#include "shout.h"
#include "skills.h"
#include "skills2.h"
#include "spl-book.h"
#include "spl-clouds.h"
#include "spl-damage.h"
#include "spl-goditem.h"
#include "spl-miscast.h"
#include "spl-selfench.h"
#include "spl-summoning.h"
#include "spl-transloc.h"
#include "state.h"
#include "stuff.h"
#include "target.h"
#include "terrain.h"
#include "throw.h"
#include "transform.h"
#include "uncancel.h"
#include "unwind.h"
#include "view.h"
#include "xom.h"

static bool _safe_to_remove_or_wear(const item_def &item, bool remove,
                                    bool quiet = false);

// Rather messy - we've gathered all the can't-wield logic from wield_weapon()
// here.
bool can_wield(item_def *weapon, bool say_reason,
               bool ignore_temporary_disability, bool unwield, bool only_known)
{
#define SAY(x) {if (say_reason) { x; }}

    if (!ignore_temporary_disability && you.berserk())
    {
        SAY(canned_msg(MSG_TOO_BERSERK));
        return false;
    }

    if (you.melded[EQ_WEAPON] && unwield)
    {
        SAY(mpr("Your weapon is melded into your body!"));
        return false;
    }

    if (!ignore_temporary_disability && !form_can_wield(you.form))
    {
        SAY(mpr("You can't wield anything in your present form."));
        return false;
    }

    if (!ignore_temporary_disability
        && you.weapon()
        && is_weapon(*you.weapon())
        && you.weapon()->cursed())
    {
        SAY(mprf("You can't unwield your weapon%s!",
                 !unwield ? " to draw a new one" : ""));
        return false;
    }

    // If we don't have an actual weapon to check, return now.
    if (!weapon)
        return true;

    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; i++)
    {
        if (you.equip[i] != -1 && &you.inv[you.equip[i]] == weapon)
        {
            SAY(mpr("You are wearing that object!"));
            return false;
        }
    }

    if (you.species == SP_FELID && is_weapon(*weapon))
    {
        SAY(mpr("You can't use weapons."));
        return false;
    }

    if (weapon->base_type == OBJ_ARMOUR)
    {
        SAY(mpr("You can't wield armour."));
        return false;
    }

    if (weapon->base_type == OBJ_JEWELLERY)
    {
        SAY(mpr("You can't wield jewellery."));
        return false;
    }

    // Only ogres and trolls can wield giant clubs and large rocks (for
    // sandblast).
    if (you.body_size(PSIZE_TORSO, true) < SIZE_LARGE
        && ((weapon->base_type == OBJ_WEAPONS
             && (is_giant_club_type(weapon->sub_type)))
             || (weapon->base_type == OBJ_MISSILES &&
                 weapon->sub_type == MI_LARGE_ROCK)))
    {
        SAY(mpr("That's too large and heavy for you to wield."));
        return false;
    }

    // All non-weapons only need a shield check.
    if (weapon->base_type != OBJ_WEAPONS)
    {
        if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
        {
            SAY(mpr("You can't wield that with a shield."));
            return false;
        }
        else
            return true;
    }

    // Small species wielding large weapons...
    if (you.body_size(PSIZE_BODY) < SIZE_MEDIUM
        && !check_weapon_wieldable_size(*weapon, you.body_size(PSIZE_BODY)))
    {
        SAY(mpr("That's too large for you to wield."));
        return false;
    }

    bool id_brand = false;

    if (you.undead_or_demonic() && is_holy_item(*weapon)
        && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("This weapon is holy and will not allow you to wield it.");
            id_brand = true;
        }
    }
    else if (!ignore_temporary_disability
             && you.hunger_state < HS_FULL
             && get_weapon_brand(*weapon) == SPWPN_VAMPIRICISM
             && !crawl_state.game_is_zotdef()
             && !you.is_undead
             && !you_foodless()
             && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("This weapon is vampiric, and you must be Full or above to equip it.");
            id_brand = true;
        }
    }
#if TAG_MAJOR_VERSION == 34
    else if (you.species == SP_DJINNI
             && get_weapon_brand(*weapon) == SPWPN_ANTIMAGIC
             && (item_type_known(*weapon) || !only_known))
    {
        if (say_reason)
        {
            mpr("As you grasp it, you feel your magic disrupted. Quickly, you stop.");
            id_brand = true;
        }
    }
#endif

    if (id_brand)
    {
        if (!is_artefact(*weapon) && !is_blessed(*weapon)
            && !item_type_known(*weapon))
        {
            set_ident_flags(*weapon, ISFLAG_KNOW_TYPE);
            if (in_inventory(*weapon))
                mprf_nocap("%s", weapon->name(DESC_INVENTORY_EQUIP).c_str());
        }
        else if (is_artefact(*weapon) && !item_type_known(*weapon))
            artefact_wpn_learn_prop(*weapon, ARTP_BRAND);
        return false;
    }

    if (!ignore_temporary_disability && is_shield_incompatible(*weapon))
    {
        SAY(mpr("You can't wield that with a shield."));
        return false;
    }

    // We can wield this weapon. Phew!
    return true;

#undef SAY
}

static bool _valid_weapon_swap(const item_def &item)
{
    if (is_weapon(item))
        return you.species != SP_FELID;

    // Some misc. items need to be wielded to be evoked.
    if (is_deck(item) || item.base_type == OBJ_MISCELLANY
                         && item.sub_type == MISC_LANTERN_OF_SHADOWS)
    {
        return true;
    }

    if (item.base_type == OBJ_MISSILES
        && (item.sub_type == MI_STONE || item.sub_type == MI_LARGE_ROCK))
    {
        return you.has_spell(SPELL_SANDBLAST);
    }

    // Snakable missiles; weapons were already handled above.
    if (item_is_snakable(item) && you.has_spell(SPELL_STICKS_TO_SNAKES))
        return true;

    // What follows pertains only to Sublimation of Blood and/or Simulacrum.
    if (!you.has_spell(SPELL_SUBLIMATION_OF_BLOOD)
        && !you.has_spell(SPELL_SIMULACRUM))
    {
        return false;
    }

    if (item.base_type == OBJ_FOOD && food_is_meaty(item))
        return item.sub_type == FOOD_CHUNK || you.has_spell(SPELL_SIMULACRUM);

    if (item.base_type == OBJ_POTIONS && item_type_known(item)
        && you.has_spell(SPELL_SUBLIMATION_OF_BLOOD))
    {
        return is_blood_potion(item);
    }

    return false;
}

/**
 * @param force If true, don't check weapon inscriptions.
 * (Assuming the player was already prompted for that.)
 */
bool wield_weapon(bool auto_wield, int slot, bool show_weff_messages,
                  bool force, bool show_unwield_msg, bool show_wield_msg)
{
    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    // Look for conditions like berserking that could prevent wielding
    // weapons.
    if (!can_wield(NULL, true, false, slot == SLOT_BARE_HANDS))
        return false;

    int item_slot = 0;          // default is 'a'

    if (auto_wield)
    {
        if (item_slot == you.equip[EQ_WEAPON]
            || you.equip[EQ_WEAPON] == -1
               && !_valid_weapon_swap(you.inv[item_slot]))
        {
            item_slot = 1;      // backup is 'b'
        }

        if (slot != -1)         // allow external override
            item_slot = slot;
    }

    // If the swap slot has a bad (but valid) item in it,
    // the swap will be to bare hands.
    const bool good_swap = (item_slot == SLOT_BARE_HANDS
                            || _valid_weapon_swap(you.inv[item_slot]));

    // Prompt if not using the auto swap command, or if the swap slot
    // is empty.
    if (item_slot != SLOT_BARE_HANDS
        && (!auto_wield || !you.inv[item_slot].defined() || !good_swap))
    {
        if (!auto_wield)
        {
            item_slot = prompt_invent_item(
                            "Wield which item (- for none, * to show all)?",
                            MT_INVLIST, OSEL_WIELD,
                            true, true, true, '-', -1, NULL, OPER_WIELD);
        }
        else
            item_slot = SLOT_BARE_HANDS;
    }

    if (prompt_failed(item_slot))
        return false;
    else if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are already wielding that!");
        return true;
    }

    // Now we really change weapons! (Most likely, at least...)
    if (you.duration[DUR_SURE_BLADE])
    {
        mpr("The bond with your blade fades away.");
        you.duration[DUR_SURE_BLADE] = 0;
    }
    // Reset the warning counter.
    you.received_weapon_warning = false;

    if (item_slot == SLOT_BARE_HANDS)
    {
        if (const item_def* wpn = you.weapon())
        {
            // Can we safely unwield this item?
            if (needs_handle_warning(*wpn, OPER_WIELD))
            {
                const string prompt =
                    "Really unwield " + wpn->name(DESC_INVENTORY) + "?";
                if (!yesno(prompt.c_str(), false, 'n'))
                {
                    canned_msg(MSG_OK);
                    return false;
                }
            }

            if (!unwield_item(show_weff_messages))
                return false;

            if (show_unwield_msg)
                canned_msg(MSG_EMPTY_HANDED_NOW);

            // Switching to bare hands is extra fast.
            you.turn_is_over = true;
            you.time_taken *= 3;
            you.time_taken /= 10;
        }
        else
            canned_msg(MSG_EMPTY_HANDED_ALREADY);

        return true;
    }

    item_def& new_wpn(you.inv[item_slot]);

    // Non-auto_wield cases are checked below.
    if (auto_wield && !force
        && !check_warning_inscriptions(new_wpn, OPER_WIELD))
    {
        return false;
    }

    // Ensure wieldable, stat loss non-fatal
    if (!can_wield(&new_wpn, true) || !_safe_to_remove_or_wear(new_wpn, false))
        return false;

    // Really ensure wieldable, even unknown brand
    if (!can_wield(&new_wpn, true, false, false, false))
        return false;

    // Unwield any old weapon.
    if (you.weapon())
    {
        if (unwield_item(show_weff_messages))
        {
            // Enable skills so they can be re-disabled later
            update_can_train();
        }
        else
            return false;
    }

    const unsigned int old_talents = your_talents(false).size();

    // Go ahead and wield the weapon.
    equip_item(EQ_WEAPON, item_slot, show_weff_messages);

    if (show_wield_msg)
        mprf_nocap("%s", new_wpn.name(DESC_INVENTORY_EQUIP).c_str());

    check_item_hint(new_wpn, old_talents);

    // Time calculations.
    you.time_taken /= 2;

    you.wield_change  = true;
    you.m_quiver->on_weapon_changed();
    you.turn_is_over  = true;

    return true;
}

bool item_is_worn(int inv_slot)
{
    for (int i = EQ_MIN_ARMOUR; i <= EQ_MAX_WORN; ++i)
        if (inv_slot == you.equip[i])
            return true;

    return false;
}

//---------------------------------------------------------------
//
// armour_prompt
//
// Prompt the user for some armour. Returns true if the user picked
// something legit.
//
//---------------------------------------------------------------
bool armour_prompt(const string & mesg, int *index, operation_types oper)
{
    ASSERT(index != NULL);

    if (inv_count() < 1)
        canned_msg(MSG_NOTHING_CARRIED);
    else if (you.berserk())
        canned_msg(MSG_TOO_BERSERK);
    else
    {
        int selector = OBJ_ARMOUR;
        if (oper == OPER_TAKEOFF && !Options.equip_unequip)
            selector = OSEL_WORN_ARMOUR;
        int slot = prompt_invent_item(mesg.c_str(), MT_INVLIST, selector,
                                      true, true, true, 0, -1, NULL,
                                      oper);

        if (!prompt_failed(slot))
        {
            *index = slot;
            return true;
        }
    }

    return false;
}


//---------------------------------------------------------------
//
// wear_armour
//
//---------------------------------------------------------------
void wear_armour(int slot) // slot is for tiles
{
    if (you.species == SP_FELID)
    {
        mpr("You can't wear anything.");
        return;
    }

    if (!form_can_wear())
    {
        mpr("You can't wear anything in your present form.");
        return;
    }

    int armour_wear_2 = 0;

    if (slot != -1)
        armour_wear_2 = slot;
    else if (!armour_prompt("Wear which item?", &armour_wear_2, OPER_WEAR))
        return;

    do_wear_armour(armour_wear_2, false);
}

static int armour_equip_delay(const item_def &item)
{
    int delay = property(item, PARM_AC);

    // Shields are comparatively easy to wear.
    if (is_shield(item))
        delay = delay / 2 + 1;

    if (delay < 1)
        delay = 1;

    return delay;
}

bool can_wear_armour(const item_def &item, bool verbose, bool ignore_temporary)
{
    const object_class_type base_type = item.base_type;
    if (base_type != OBJ_ARMOUR || you.species == SP_FELID)
    {
        if (verbose)
            mpr("You can't wear that.");

        return false;
    }

    const int sub_type = item.sub_type;
    const equipment_type slot = get_armour_slot(item);

    if (you.species == SP_OCTOPODE && slot != EQ_HELMET && slot != EQ_SHIELD)
    {
        if (verbose)
            mpr("You can't wear that!");
        return false;
    }

    if (player_genus(GENPC_DRACONIAN) && slot == EQ_BODY_ARMOUR)
    {
        if (verbose)
        {
            mprf("Your wings%s won't fit in that.", you.mutation[MUT_BIG_WINGS]
                 ? "" : ", even vestigial as they are,");
        }
        return false;
    }

    if (sub_type == ARM_NAGA_BARDING || sub_type == ARM_CENTAUR_BARDING)
    {
        if (you.species == SP_NAGA && sub_type == ARM_NAGA_BARDING
            || you.species == SP_CENTAUR && sub_type == ARM_CENTAUR_BARDING)
        {
            if (ignore_temporary || !player_is_shapechanged())
                return true;
            else if (verbose)
                mpr("You can wear that only in your normal form.");
        }
        else if (verbose)
            mpr("You can't wear that!");
        return false;
    }

    // Lear's hauberk covers also head, hands and legs.
    if (is_unrandom_artefact(item) && item.special == UNRAND_LEAR)
    {
        if (!player_has_feet(!ignore_temporary))
        {
            if (verbose)
                mpr("You have no feet.");
            return false;
        }

        if (ignore_temporary)
        {
            // Hooves and talons were already checked by player_has_feet.

            if (species_has_claws(you.species) >= 3
                || player_mutation_level(MUT_CLAWS, false) >= 3)
            {
                if (verbose)
                    mpr("The hauberk won't fit your hands.");
                return false;
            }

            if (player_mutation_level(MUT_HORNS, false) >= 3
                || player_mutation_level(MUT_ANTENNAE, false) >= 3)
            {
                if (verbose)
                    mpr("The hauberk won't fit your head.");
                return false;
            }
        }
        else
        {
            for (int s = EQ_HELMET; s <= EQ_BOOTS; s++)
            {
                // No strange race can wear this.
                const char* parts[] = { "head", "hands", "feet" };
                // Auto-disrobing would be nice.
                if (you.equip[s] != -1)
                {
                    if (verbose)
                        mprf("You'd need your %s free.", parts[s - EQ_HELMET]);
                    return false;
                }

                if (!you_tran_can_wear(s, true))
                {
                    if (verbose)
                    {
                        mprf(you_tran_can_wear(s) ? "The hauberk won't fit your %s."
                                                  : "You have no %s!",
                             parts[s - EQ_HELMET]);
                    }
                    return false;
                }
            }
        }
    }
    else if (slot >= EQ_HELMET && slot <= EQ_BOOTS
             && !ignore_temporary
             && player_equip_unrand(UNRAND_LEAR))
    {
        // The explanation is iffy for loose headgear, especially crowns:
        // kings loved hooded hauberks, according to portraits.
        if (verbose)
            mpr("You can't wear this over your hauberk.");
        return false;
    }

    size_type player_size = you.body_size(PSIZE_TORSO, ignore_temporary);
    int bad_size = fit_armour_size(item, player_size);

    if (bad_size)
    {
        if (verbose)
        {
            mprf("This armour is too %s for you!",
                 (bad_size > 0) ? "big" : "small");
        }

        return false;
    }

    if (you.form == TRAN_APPENDAGE
        && ignore_temporary
        && slot == beastly_slot(you.attribute[ATTR_APPENDAGE])
        && you.mutation[you.attribute[ATTR_APPENDAGE]])
    {
        unwind_var<uint8_t> mutv(you.mutation[you.attribute[ATTR_APPENDAGE]], 0);
        // disable the mutation then check again
        return can_wear_armour(item, verbose, ignore_temporary);
    }

    if (sub_type == ARM_GLOVES)
    {
        if (you.has_claws(false) == 3)
        {
            if (verbose)
                mpr("You can't wear gloves with your huge claws!");
            return false;
        }
    }

    if (sub_type == ARM_BOOTS)
    {
        if (player_mutation_level(MUT_HOOVES) == 3)
        {
            if (verbose)
                mpr("You can't wear boots with hooves!");
            return false;
        }

        if (you.has_talons(false) == 3)
        {
            if (verbose)
                mpr("Boots don't fit your talons!");
            return false;
        }

        if (you.species == SP_NAGA
#if TAG_MAJOR_VERSION == 34
            || you.species == SP_DJINNI
#endif
           )
        {
            if (verbose)
                mpr("You have no legs!");
            return false;
        }

        if (!ignore_temporary && you.fishtail)
        {
            if (verbose)
                mpr("You don't currently have feet!");
            return false;
        }
    }

    if (slot == EQ_HELMET)
    {
        // Horns 3 & Antennae 3 mutations disallow all headgear
        if (player_mutation_level(MUT_HORNS) == 3)
        {
            if (verbose)
                mpr("You can't wear any headgear with your large horns!");
            return false;
        }

        if (player_mutation_level(MUT_ANTENNAE) == 3)
        {
            if (verbose)
                mpr("You can't wear any headgear with your large antennae!");
            return false;
        }

        // Soft helmets (caps and wizard hats) always fit, otherwise.
        if (is_hard_helmet(item))
        {
            if (player_mutation_level(MUT_HORNS))
            {
                if (verbose)
                    mpr("You can't wear that with your horns!");
                return false;
            }

            if (player_mutation_level(MUT_BEAK))
            {
                if (verbose)
                    mpr("You can't wear that with your beak!");
                return false;
            }

            if (player_mutation_level(MUT_ANTENNAE))
            {
                if (verbose)
                    mpr("You can't wear that with your antennae!");
                return false;
            }

            if (player_genus(GENPC_DRACONIAN))
            {
                if (verbose)
                    mpr("You can't wear that with your reptilian head.");
                return false;
            }

            if (you.species == SP_OCTOPODE)
            {
                if (verbose)
                    mpr("You can't wear that!");
                return false;
            }
        }
    }

    if (!ignore_temporary && !form_can_wear_item(item, you.form))
    {
        if (verbose)
            mpr("You can't wear that in your present form.");
        return false;
    }

    return true;
}

bool do_wear_armour(int item, bool quiet)
{
    const item_def &invitem = you.inv[item];
    if (!invitem.defined())
    {
        if (!quiet)
            mpr("You don't have any such object.");
        return false;
    }

    if (!can_wear_armour(invitem, !quiet, false))
        return false;

    const equipment_type slot = get_armour_slot(invitem);

    if (item == you.equip[EQ_WEAPON])
    {
        if (!quiet)
            mpr("You are wielding that object!");
        return false;
    }

    if (item_is_worn(item))
    {
        if (Options.equip_unequip)
            return !takeoff_armour(item);
        else
        {
            mpr("You're already wearing that object!");
            return false;
        }
    }

    // if you're wielding something,
    if (you.weapon()
        // attempting to wear a shield,
        && is_shield(invitem)
        && is_shield_incompatible(*you.weapon(), &invitem))
    {
        if (!quiet)
        {
            if (you.species == SP_OCTOPODE)
                mpr("You need the rest of your tentacles for walking.");
            else
                mprf("You'd need three %s to do that!", you.hand_name(true).c_str());
        }
        return false;
    }

    if ((slot == EQ_CLOAK
           || slot == EQ_HELMET
           || slot == EQ_GLOVES
           || slot == EQ_BOOTS
           || slot == EQ_SHIELD
           || slot == EQ_BODY_ARMOUR)
        && you.equip[slot] != -1)
    {
        if (!takeoff_armour(you.equip[slot]))
            return false;
    }

    you.turn_is_over = true;

    if (!_safe_to_remove_or_wear(invitem, false))
        return false;

    const int delay = armour_equip_delay(invitem);
    if (delay)
        start_delay(DELAY_ARMOUR_ON, delay, item);

    return true;
}

bool takeoff_armour(int item)
{
    const item_def& invitem = you.inv[item];

    if (invitem.base_type != OBJ_ARMOUR)
    {
        mpr("You aren't wearing that!");
        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    const equipment_type slot = get_armour_slot(invitem);
    if (item == you.equip[slot] && you.melded[slot])
    {
        mprf("%s is melded into your body!",
             invitem.name(DESC_YOUR).c_str());
        return false;
    }

    if (!item_is_worn(item))
    {
        if (Options.equip_unequip)
            return do_wear_armour(item, true);
        else
        {
            mpr("You aren't wearing that object!");
            return false;
        }
    }

    // If we get here, we're wearing the item.
    if (invitem.cursed())
    {
        mprf("%s is stuck to your body!", invitem.name(DESC_YOUR).c_str());
        return false;
    }

    if (!_safe_to_remove_or_wear(invitem, true))
        return false;


    switch (slot)
    {
    case EQ_BODY_ARMOUR:
    case EQ_SHIELD:
    case EQ_CLOAK:
    case EQ_HELMET:
    case EQ_GLOVES:
    case EQ_BOOTS:
        if (item != you.equip[slot])
        {
            mpr("You aren't wearing that!");
            return false;
        }
        break;

    default:
        break;
    }

    you.turn_is_over = true;

    const int delay = armour_equip_delay(invitem);
    start_delay(DELAY_ARMOUR_OFF, delay, item);

    return true;
}

// Returns a list of possible ring slots.
static vector<equipment_type> _current_ring_types()
{
    vector<equipment_type> ret;
    if (you.species == SP_OCTOPODE)
    {
        const int num_rings = (form_keeps_mutations() || you.form == TRAN_SPIDER
                               ? 8 : 2);
        for (int i = 0; i != num_rings; ++i)
            ret.push_back((equipment_type)(EQ_RING_ONE + i));
    }
    else
    {
        ret.push_back(EQ_LEFT_RING);
        ret.push_back(EQ_RIGHT_RING);
    }
    if (player_equip_unrand(UNRAND_FINGER_AMULET))
        ret.push_back(EQ_RING_AMULET);
    return ret;
}

static vector<equipment_type> _current_jewellery_types()
{
    vector<equipment_type> ret = _current_ring_types();
    ret.push_back(EQ_AMULET);
    return ret;
}

static int _prompt_ring_to_remove(int new_ring)
{
    const vector<equipment_type> ring_types = _current_ring_types();
    vector<char> slot_chars;
    vector<item_def*> rings;
    for (vector<equipment_type>::const_iterator eq_it = ring_types.begin();
         eq_it != ring_types.end();
         ++eq_it)
    {
        rings.push_back(you.slot_item(*eq_it, true));
        ASSERT(rings.back());
        slot_chars.push_back(index_to_letter(rings.back()->link));
    }

    mesclr();

    mprf(MSGCH_PROMPT,
         "You're wearing all the rings you can. Remove which one?");
    mprf(MSGCH_PROMPT, "(<w>?</w> for menu, <w>Esc</w> to cancel)");

    // FIXME: Needs TOUCH_UI version

    for (size_t i = 0; i < rings.size(); i++)
    {
        string m;
        if (ring_types[i] == EQ_LEFT_RING)
            m += "<< or ";
        if (ring_types[i] == EQ_RIGHT_RING)
            m += "> or ";
        if (ring_types[i] == EQ_RING_AMULET)
            m += "^ or ";
        m += rings[i]->name(DESC_INVENTORY);
        mprf_nocap("%s", m.c_str());
    }
    flush_prev_message();

    // Deactivate choice from tile inventory.
    // FIXME: We need to be able to get the choice (item letter)n
    //        *without* the choice taking action by itself!
    int eqslot = EQ_NONE;

    mouse_control mc(MOUSE_MODE_PROMPT);
    int c;
    do
    {
        c = getchm();
        for (size_t i = 0; i < slot_chars.size(); i++)
        {
            if (c == slot_chars[i]
                || (ring_types[i] == EQ_LEFT_RING   && c == '<')
                || (ring_types[i] == EQ_RIGHT_RING  && c == '>')
                || (ring_types[i] == EQ_RING_AMULET && c == '^'))
            {
                eqslot = ring_types[i];
                c = ' ';
                break;
            }
        }
    } while (!key_is_escape(c) && c != ' ' && c != '?');

    mesclr();

    if (c == '?')
        return EQ_NONE;
    else if (key_is_escape(c) || eqslot == EQ_NONE)
        return -2;

    return you.equip[eqslot];
}

// Checks whether a to-be-worn or to-be-removed item affects
// character stats and whether wearing/removing it could be fatal.
// If so, warns the player, or just returns false if quiet is true.
static bool _safe_to_remove_or_wear(const item_def &item, bool remove, bool quiet)
{
    if (remove && !safe_to_remove(item, quiet))
        return false;

    int prop_str = 0;
    int prop_dex = 0;
    int prop_int = 0;
    if (item.base_type == OBJ_JEWELLERY
        && item_ident(item, ISFLAG_KNOW_PLUSES))
    {
        switch (item.sub_type)
        {
        case RING_STRENGTH:
            if (item.plus != 0)
                prop_str = item.plus;
            break;
        case RING_DEXTERITY:
            if (item.plus != 0)
                prop_dex = item.plus;
            break;
        case RING_INTELLIGENCE:
            if (item.plus != 0)
                prop_int = item.plus;
            break;
        default:
            break;
        }
    }
    else if (item.base_type == OBJ_ARMOUR && item_type_known(item))
    {
        switch (item.special)
        {
        case SPARM_STRENGTH:
            prop_str = 3;
            break;
        case SPARM_INTELLIGENCE:
            prop_int = 3;
            break;
        case SPARM_DEXTERITY:
            prop_dex = 3;
            break;
        default:
            break;
        }
    }

    if (is_artefact(item))
    {
        prop_str += artefact_known_wpn_property(item, ARTP_STRENGTH);
        prop_int += artefact_known_wpn_property(item, ARTP_INTELLIGENCE);
        prop_dex += artefact_known_wpn_property(item, ARTP_DEXTERITY);
    }

    if (!remove)
    {
        prop_str *= -1;
        prop_int *= -1;
        prop_dex *= -1;
    }
    stat_type red_stat = NUM_STATS;
    if (prop_str >= you.strength() && you.strength() > 0)
        red_stat = STAT_STR;
    else if (prop_int >= you.intel() && you.intel() > 0)
        red_stat = STAT_INT;
    else if (prop_dex >= you.dex() && you.dex() > 0)
        red_stat = STAT_DEX;

    if (red_stat == NUM_STATS)
        return true;

    if (quiet)
        return false;

    string verb = "";
    if (remove)
    {
        if (item.base_type == OBJ_WEAPONS)
            verb = "Unwield";
        else
            verb = "Remov"; // -ing, not a typo
    }
    else
    {
        if (item.base_type == OBJ_WEAPONS)
            verb = "Wield";
        else
            verb = "Wear";
    }

    string prompt = make_stringf("%sing this item will reduce your %s to zero "
                                 "or below. Continue?", verb.c_str(),
                                 stat_desc(red_stat, SD_NAME));
    if (!yesno(prompt.c_str(), true, 'n', true, false))
    {
        canned_msg(MSG_OK);
        return false;
    }
    return true;
}

// Checks whether removing an item would cause flight to end and the
// player to fall to their death.
bool safe_to_remove(const item_def &item, bool quiet)
{
    item_info inf = get_item_info(item);

    const bool grants_flight =
         inf.base_type == OBJ_JEWELLERY && inf.sub_type == RING_FLIGHT
         || inf.base_type == OBJ_ARMOUR && inf.special == SPARM_FLYING
         || is_artefact(inf)
            && artefact_known_wpn_property(inf, ARTP_FLY);

    // assumes item can't grant flight twice
    const bool removing_ends_flight = you.flight_mode()
          && !you.racial_permanent_flight()
          && !you.attribute[ATTR_FLIGHT_UNCANCELLABLE]
          && (you.evokable_flight() == 1);

    const dungeon_feature_type feat = grd(you.pos());

    if (grants_flight && removing_ends_flight
        && is_feat_dangerous(feat, false, true))
    {
        if (!quiet)
            mpr("Losing flight right now would be fatal!");
        return false;
    }

    return true;
}

// Assumptions:
// you.inv[ring_slot] is a valid ring.
// EQ_LEFT_RING and EQ_RIGHT_RING are both occupied, and ring_slot is not
// one of the worn rings.
//
// Does not do amulets.
static bool _swap_rings(int ring_slot)
{
    vector<equipment_type> ring_types = _current_ring_types();
    const int num_rings = ring_types.size();
    int unwanted = 0;
    int last_inscribed = 0;
    int cursed = 0;
    int inscribed = 0;
    int melded = 0; // Both melded rings and unavailable slots.
    int available = 0;
    bool all_same = true;
    item_def* first_ring = NULL;
    for (vector<equipment_type>::iterator eq_it = ring_types.begin();
         eq_it != ring_types.end();
         ++eq_it)
    {
        item_def* ring = you.slot_item(*eq_it, true);
        if (!you_tran_can_wear(*eq_it) || you.melded[*eq_it])
            melded++;
        else if (ring != NULL)
        {
            if (first_ring == NULL)
                first_ring = ring;
            else if (all_same)
            {
                if (ring->sub_type != first_ring->sub_type
                    || ring->plus  != first_ring->plus
                    || ring->plus2 != first_ring->plus2
                    || is_artefact(*ring) || is_artefact(*first_ring))
                {
                    all_same = false;
                }
            }

            if (ring->cursed())
                cursed++;
            else if (strstr(ring->inscription.c_str(), "=R"))
            {
                inscribed++;
                last_inscribed = you.equip[*eq_it];
            }
            else
            {
                available++;
                unwanted = you.equip[*eq_it];
            }
        }
    }

    // If the only swappable rings are inscribed =R, go ahead and use them.
    if (available == 0 && inscribed > 0)
    {
        available += inscribed;
        unwanted = last_inscribed;
    }

    // We can't put a ring on, because we're wearing all cursed ones.
    if (melded == num_rings)
    {
        // Shouldn't happen, because hogs and bats can't put on jewellery at
        // all and thus won't get this far.
        mpr("You can't wear that in your present form.");
        return false;
    }
    else if (available == 0)
    {
        mprf("You're already wearing %s cursed rings!%s",
             number_in_words(cursed).c_str(),
             (cursed > 2 ? " Isn't that enough for you?" : ""));
        return false;
    }
    // The simple case - only one available ring.
    else if (available == 1)
    {
        if (!remove_ring(unwanted, false))
            return false;
    }
    // We can't put a ring on without swapping - because we found
    // multiple available rings.
    else if (available > 1)
    {
        // Don't prompt if all the rings are the same
        if (!all_same)
            unwanted = _prompt_ring_to_remove(ring_slot);

        // Cancelled:
        if (unwanted < -1)
        {
            canned_msg(MSG_OK);
            return false;
        }

        if (!remove_ring(unwanted, false))
            return false;
    }

    // Put on the new ring.
    start_delay(DELAY_JEWELLERY_ON, 1, ring_slot);

    return true;
}

static bool _puton_item(int item_slot)
{
    item_def& item = you.inv[item_slot];

    for (int eq = EQ_LEFT_RING; eq < NUM_EQUIP; eq++)
        if (item_slot == you.equip[eq])
        {
            // "Putting on" an equipped item means taking it off.
            if (Options.equip_unequip)
                return !remove_ring(item_slot);
            else
            {
                mpr("You're already wearing that object!");
                return false;
            }
        }

    if (item_slot == you.equip[EQ_WEAPON])
    {
        mpr("You are wielding that object.");
        return false;
    }

    if (item.base_type != OBJ_JEWELLERY)
    {
        mpr("You can only put on jewellery.");
        return false;
    }

    const vector<equipment_type> ring_types = _current_ring_types();
    const bool is_amulet = jewellery_is_amulet(item);

    if (!is_amulet)     // i.e. it's a ring
    {
        if (!you_tran_can_wear(item))
        {
            mpr("You can't wear that in your present form.");
            return false;
        }

        bool need_swap = true;
        for (vector<equipment_type>::const_iterator eq_it = ring_types.begin();
             eq_it != ring_types.end();
             ++eq_it)
            if (!you.slot_item(*eq_it, true))
            {
                need_swap = false;
                break;
            }

        if (need_swap)
            return _swap_rings(item_slot);
    }
    else if (you.slot_item(EQ_AMULET, true))
    {
        // Remove the previous one.
        if (!remove_ring(you.equip[EQ_AMULET], true))
            return false;

        // Check for stat loss.
        if (!_safe_to_remove_or_wear(item, false))
            return false;

        // Put on the new amulet.
        start_delay(DELAY_JEWELLERY_ON, 1, item_slot);

        // Assume it's going to succeed.
        return true;
    }

    // Check for stat loss.
    if (!_safe_to_remove_or_wear(item, false))
        return false;

    equipment_type hand_used = EQ_NONE;

    if (is_amulet)
        hand_used = EQ_AMULET;
    else
    {
        for (vector<equipment_type>::const_iterator eq_it = ring_types.begin();
             eq_it != ring_types.end();
             ++eq_it)
        {
            if (!you.slot_item(*eq_it, true))
            {
                hand_used = *eq_it;
                break;
            }
        }
    }

    const unsigned int old_talents = your_talents(false).size();

    // Actually equip the item.
    equip_item(hand_used, item_slot);

    check_item_hint(you.inv[item_slot], old_talents);
#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
    }
#endif

    // Putting on jewellery is as fast as wielding weapons.
    you.time_taken /= 2;
    you.turn_is_over = true;

    return true;
}

bool puton_ring(int slot)
{
    int item_slot;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Put on which piece of jewellery?",
                                        MT_INVLIST, OBJ_JEWELLERY, true, true,
                                        true, 0, -1, NULL, OPER_PUTON);
    }

    if (prompt_failed(item_slot))
        return false;

    return _puton_item(item_slot);
}

bool remove_ring(int slot, bool announce)
{
    equipment_type hand_used = EQ_NONE;
    int ring_wear_2;
    bool has_jewellery = false;
    bool has_melded = false;
    const vector<equipment_type> ring_types = _current_ring_types();
    const vector<equipment_type> jewellery_slots = _current_jewellery_types();

    for (vector<equipment_type>::const_iterator eq_it = jewellery_slots.begin();
         eq_it != jewellery_slots.end();
         ++eq_it)
    {
        if (player_wearing_slot(*eq_it))
        {
            if (has_jewellery)
            {
                // At least one other piece, which means we'll have to ask
                hand_used = EQ_NONE;
            }
            else
                hand_used = *eq_it;

            has_jewellery = true;
        }
        else if (you.melded[*eq_it])
            has_melded = true;
    }

    if (!has_jewellery)
    {
        if (has_melded)
            mpr("You aren't wearing any unmelded rings or amulets.");
        else
            mpr("You aren't wearing any rings or amulets.");

        return false;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return false;
    }

    if (hand_used == EQ_NONE)
    {
        const int equipn =
            (slot == -1)? prompt_invent_item("Remove which piece of jewellery?",
                                             MT_INVLIST,
                                             OBJ_JEWELLERY, true, true, true,
                                             0, -1, NULL, OPER_REMOVE,
                                             false, false)
                        : slot;

        if (prompt_failed(equipn))
            return false;

        hand_used = item_equip_slot(you.inv[equipn]);
        if (hand_used == EQ_NONE)
        {
            mpr("You aren't wearing that.");
            return false;
        }
        else if (you.inv[equipn].base_type != OBJ_JEWELLERY)
        {
            mpr("That isn't a piece of jewellery.");
            return false;
        }
    }

    if (you.equip[hand_used] == -1)
    {
        mpr("I don't think you really meant that.");
        return false;
    }
    else if (you.melded[hand_used])
    {
        mpr("You can't take that off while it's melded.");
        return false;
    }
    else if (hand_used == EQ_AMULET
        && you.equip[EQ_RING_AMULET] != -1
        && !remove_ring(you.equip[EQ_RING_AMULET], announce))
    {
        // This can be removed in the future if more ring amulets are added.
        ASSERT(player_equip_unrand(UNRAND_FINGER_AMULET));

        mpr("The amulet cannot be taken off without first removing the ring!");
        return false;
    }

    if (!check_warning_inscriptions(you.inv[you.equip[hand_used]],
                                    OPER_REMOVE))
    {
        canned_msg(MSG_OK);
        return false;
    }

    if (you.inv[you.equip[hand_used]].cursed())
    {
        if (announce)
        {
            mprf("%s is stuck to you!",
                 you.inv[you.equip[hand_used]].name(DESC_YOUR).c_str());
        }
        else
            mpr("It's stuck to you!");

        set_ident_flags(you.inv[you.equip[hand_used]], ISFLAG_KNOW_CURSE);
        return false;
    }

    ring_wear_2 = you.equip[hand_used];

    // Remove the ring.
    if (!_safe_to_remove_or_wear(you.inv[ring_wear_2], true))
        return false;

    mprf("You remove %s.", you.inv[ring_wear_2].name(DESC_YOUR).c_str());
#ifdef USE_TILE_LOCAL
    const unsigned int old_talents = your_talents(false).size();
#endif
    unequip_item(hand_used);
#ifdef USE_TILE_LOCAL
    if (your_talents(false).size() != old_talents)
    {
        tiles.layout_statcol();
        redraw_screen();
    }
#endif

    you.time_taken /= 2;
    you.turn_is_over = true;

    return true;
}

static int _wand_range(zap_type ztype)
{
    // FIXME: Eventually we should have sensible values here.
    return LOS_RADIUS;
}

static int _max_wand_range()
{
    return LOS_RADIUS;
}

static bool _dont_use_invis()
{
    if (!you.backlit())
        return false;

    if (you.haloed() || you.glows_naturally())
    {
        mpr("You can't turn invisible.");
        return true;
    }
    else if (get_contamination_level() > 1
             && !yesno("Invisibility will do you no good right now; "
                       "use anyway?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return true;
    }

    return false;
}

static targetter *_wand_targetter(const item_def *wand)
{
    int range = _wand_range(wand->zap());
    const int power = 15 + you.skill(SK_EVOCATIONS, 5) / 2;

    switch (wand->sub_type)
    {
    case WAND_FIREBALL:
        return new targetter_beam(&you, range, ZAP_FIREBALL, power, 1, 1);
    case WAND_LIGHTNING:
        return new targetter_beam(&you, range, ZAP_LIGHTNING_BOLT, power, 0, 0);
    case WAND_FLAME:
        return new targetter_beam(&you, range, ZAP_THROW_FLAME, power, 0, 0);
    case WAND_FIRE:
        return new targetter_beam(&you, range, ZAP_BOLT_OF_FIRE, power, 0, 0);
    case WAND_FROST:
        return new targetter_beam(&you, range, ZAP_THROW_FROST, power, 0, 0);
    case WAND_COLD:
        return new targetter_beam(&you, range, ZAP_BOLT_OF_COLD, power, 0, 0);
    case WAND_DIGGING:
        return new targetter_beam(&you, range, ZAP_DIG, power, 0, 0);
    default:
        return 0;
    }
}

void zap_wand(int slot)
{
    if (!form_can_use_wand())
    {
        mpr("You have no means to grasp a wand firmly enough.");
        return;
    }

    bolt beam;
    dist zap_wand;
    int item_slot;

    // Unless the character knows the type of the wand, the targeting
    // system will default to enemies. -- [ds]
    targ_mode_type targ_mode = TARG_HOSTILE;

    beam.beam_source = MHITYOU;

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (slot != -1)
        item_slot = slot;
    else
    {
        item_slot = prompt_invent_item("Zap which item?",
                                       MT_INVLIST,
                                       OBJ_WANDS,
                                       true, true, true, 0, -1, NULL,
                                       OPER_ZAP);
    }

    if (prompt_failed(item_slot))
        return;

    item_def& wand = you.inv[item_slot];
    if (wand.base_type != OBJ_WANDS)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        return;
    }

    // If you happen to be wielding the wand, its display might change.
    if (you.equip[EQ_WEAPON] == item_slot)
        you.wield_change = true;

    const zap_type type_zapped = wand.zap();

    bool has_charges = true;
    if (wand.plus < 1)
    {
        if (wand.plus2 == ZAPCOUNT_EMPTY)
        {
            mpr("This wand has no charges.");
            return;
        }
        has_charges = false;
    }

    const bool alreadyknown = item_type_known(wand);
          bool invis_enemy  = false;
    const bool dangerous    = player_in_a_dangerous_place(&invis_enemy);
    targetter *hitfunc      = 0;

    if (!alreadyknown)
    {
        beam.effect_known = false;
        beam.effect_wanton = false;
    }
    else
    {
        switch (wand.sub_type)
        {
        case WAND_DIGGING:
        case WAND_TELEPORTATION:
            targ_mode = TARG_ANY;
            break;

        case WAND_HEAL_WOUNDS:
            if (you_worship(GOD_ELYVILON))
            {
                targ_mode = TARG_ANY;
                break;
            }
            // else intentional fall-through
        case WAND_HASTING:
        case WAND_INVISIBILITY:
            targ_mode = TARG_FRIEND;
            break;

        default:
            targ_mode = TARG_HOSTILE;
            break;
        }

        hitfunc = _wand_targetter(&wand);
    }

    const int tracer_range =
        (alreadyknown && wand.sub_type != WAND_RANDOM_EFFECTS) ?
        _wand_range(type_zapped) : _max_wand_range();
    const string zap_title =
        "Zapping: " + get_menu_colour_prefix_tags(wand, DESC_INVENTORY);
    direction_chooser_args args;
    args.mode = targ_mode;
    args.range = tracer_range;
    args.top_prompt = zap_title;
    args.hitfunc = hitfunc;
    direction(zap_wand, args);

    if (hitfunc)
        delete hitfunc;

    if (!zap_wand.isValid)
    {
        if (zap_wand.isCancel)
            canned_msg(MSG_OK);
        return;
    }

    if (alreadyknown && zap_wand.target == you.pos())
    {
        if (wand.sub_type == WAND_TELEPORTATION && you.no_tele(false, false))
        {
            if (you.species == SP_FORMICID)
                mpr("You cannot teleport.");
            else
                mpr("You cannot teleport right now.");
            return;
        }
        else if (wand.sub_type == WAND_HASTING && you.stasis(false))
        {
            if (you.species == SP_FORMICID)
                mpr("You cannot haste.");
            else
                mpr("You cannot haste right now.");
            return;
        }
        else if (wand.sub_type == WAND_INVISIBILITY && _dont_use_invis())
            return;
    }

    if (!has_charges)
    {
        canned_msg(MSG_NOTHING_HAPPENS);
        // It's an empty wand; inscribe it that way.
        wand.plus2 = ZAPCOUNT_EMPTY;
        you.turn_is_over = true;
        return;
    }

    if (you.confused())
        zap_wand.confusion_fuzz();

    if (wand.sub_type == WAND_RANDOM_EFFECTS)
    {
        beam.effect_known = false;
        beam.effect_wanton = alreadyknown;
    }

    beam.source   = you.pos();
    beam.attitude = ATT_FRIENDLY;
    beam.set_target(zap_wand);

    const bool aimed_at_self = (beam.target == you.pos());
    const int power = 15 + you.skill(SK_EVOCATIONS, 5) / 2;

    // Check whether we may hit friends, use "safe" values for random effects
    // and unknown wands (highest possible range, and unresistable beam
    // flavour). Don't use the tracer if firing at self.
    if (!aimed_at_self)
    {
        beam.range = tracer_range;
        if (!player_tracer(beam.effect_known ? type_zapped
                                             : ZAP_DEBUGGING_RAY,
                           power, beam, beam.effect_known ? 0 : 17))
        {
            return;
        }
    }

    // Zapping the wand isn't risky if you aim it away from all monsters
    // and yourself, unless there's a nearby invisible enemy and you're
    // trying to hit it at random.
    const bool risky = dangerous && (beam.friend_info.count
                                     || beam.foe_info.count
                                     || invis_enemy
                                     || aimed_at_self);

    if (risky && alreadyknown && wand.sub_type == WAND_RANDOM_EFFECTS)
    {
        // Xom loves it when you use a Wand of Random Effects and
        // there is a dangerous monster nearby...
        xom_is_stimulated(200);
    }

    // Reset range.
    beam.range = _wand_range(type_zapped);

#ifdef WIZARD
    if (you.wizard)
    {
        string str = wand.inscription;
        int wiz_range = strip_number_tag(str, "range:");
        if (wiz_range != TAG_UNFOUND)
            beam.range = wiz_range;
    }
#endif

    // zapping() updates beam.
    zapping(type_zapped, power, beam);

    // Take off a charge.
    wand.plus--;

    // Zap counts count from the last recharge.
    if (wand.plus2 == ZAPCOUNT_RECHARGED)
        wand.plus2 = 0;
    // Increment zap count.
    if (wand.plus2 >= 0)
        wand.plus2++;

    // Identify if unknown.
    if (!alreadyknown)
    {
        set_ident_type(wand, ID_KNOWN_TYPE);
        if (wand.sub_type == WAND_RANDOM_EFFECTS)
            mpr("You feel that this wand is rather unreliable.");

        mprf_nocap("%s", wand.name(DESC_INVENTORY_EQUIP).c_str());
    }

    if (item_type_known(wand)
        && (item_ident(wand, ISFLAG_KNOW_PLUSES)
            || you.skill(SK_EVOCATIONS, 10) > 50 + random2(141)))
    {
        if (!item_ident(wand, ISFLAG_KNOW_PLUSES))
        {
            mpr("Your skill with magical items lets you calculate "
                "the power of this device...");
        }

        mprf("This wand has %d charge%s left.",
             wand.plus, wand.plus == 1 ? "" : "s");

        set_ident_flags(wand, ISFLAG_KNOW_PLUSES);
    }
    // Mark as empty if necessary.
    if (wand.plus == 0 && wand.flags & ISFLAG_KNOW_PLUSES)
        wand.plus2 = ZAPCOUNT_EMPTY;

    practise(EX_DID_ZAP_WAND);
    count_action(CACT_EVOKE, EVOC_WAND);
    alert_nearby_monsters();

    if (!alreadyknown && risky)
    {
        // Xom loves it when you use an unknown wand and there is a
        // dangerous monster nearby...
        xom_is_stimulated(200);
    }

    you.turn_is_over = true;
}

void prompt_inscribe_item()
{
    if (inv_count() < 1)
    {
        mpr("You don't have anything to inscribe.");
        return;
    }

    int item_slot = prompt_invent_item("Inscribe which item?",
                                       MT_INVLIST, OSEL_ANY);

    if (prompt_failed(item_slot))
        return;

    inscribe_item(you.inv[item_slot], true);
}

static void _vampire_corpse_help()
{
    if (you.species != SP_VAMPIRE)
        return;

    if (check_blood_corpses_on_ground() || count_corpses_in_pack(true) > 0)
        mpr("Use <w>e</w> to drain blood from corpses.");
}

void drink(int slot)
{
    if (you_foodless(true))
    {
        mpr("You can't drink.");
        return;
    }

    if (inv_count() == 0)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        _vampire_corpse_help();
        return;
    }

    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.form == TRAN_BAT)
    {
        canned_msg(MSG_PRESENT_FORM);
        _vampire_corpse_help();
        return;
    }

    if (you.duration[DUR_NO_POTIONS])
    {
        mpr("You cannot drink potions in your current state!");
        return;
    }

    if (slot == -1)
    {
        slot = prompt_invent_item("Drink which item?",
                                  MT_INVLIST, OBJ_POTIONS,
                                  true, true, true, 0, -1, NULL,
                                  OPER_QUAFF);
        if (prompt_failed(slot))
        {
            _vampire_corpse_help();
            return;
        }
    }

    item_def& potion = you.inv[slot];

    if (potion.base_type != OBJ_POTIONS)
    {
        if (you.species == SP_VAMPIRE && potion.base_type == OBJ_CORPSES)
            eat_food(slot);
        else
            mpr("You can't drink that!");
        return;
    }

    const bool alreadyknown = item_type_known(potion);

    if (alreadyknown && you.hunger_state == HS_ENGORGED
        && (is_blood_potion(potion) || potion.sub_type == POT_PORRIDGE))
    {
        mpr("You are much too full right now.");
        return;
    }

    if (alreadyknown && potion.sub_type == POT_INVISIBILITY
        && _dont_use_invis())
    {
        return;
    }

    if (alreadyknown && potion.sub_type == POT_BERSERK_RAGE
        && (!berserk_check_wielded_weapon()
            || !you.can_go_berserk(true, true)))
    {
        return;
    }

    if (alreadyknown && is_blood_potion(potion)
        && is_good_god(you.religion)
        && !yesno("Really drink that potion of blood?", false, 'n'))
    {
        canned_msg(MSG_OK);
        return;
    }

    zin_recite_interrupt();

    // The "> 1" part is to reduce the amount of times that Xom is
    // stimulated when you are a low-level 1 trying your first unknown
    // potions on monsters.
    const bool dangerous = (player_in_a_dangerous_place()
                            && you.experience_level > 1);
    potion_type pot_type = (potion_type)potion.sub_type;

    if (!potion_effect(static_cast<potion_type>(potion.sub_type),
                       40, &potion, alreadyknown))
    {
        return;
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you drink an unknown potion and there is
        // a dangerous monster nearby...
        xom_is_stimulated(200);
    }

    if (is_blood_potion(potion))
    {
        // Always drink oldest potion.
        remove_oldest_blood_potion(potion);
    }

    dec_inv_item_quantity(slot, 1);
    count_action(CACT_USE, OBJ_POTIONS);
    you.turn_is_over = true;

    // This got deferred from the it_use2 switch to prevent SIGHUP abuse.
    if (pot_type == POT_EXPERIENCE)
        level_change();
}

// XXX: Only checks brands that can be rebranded to,
// there's probably a nicer way of doing this.
static bool _god_hates_brand(const int brand)
{
    if (is_good_god(you.religion)
        && (brand == SPWPN_DRAINING
            || brand == SPWPN_VAMPIRICISM
            || brand == SPWPN_CHAOS))
    {
        return true;
    }

    if (you_worship(GOD_DITHMENOS)
        && (brand == SPWPN_FLAMING
            || brand == SPWPN_FLAME
            || brand == SPWPN_CHAOS))
    {
        return true;
    }

    if (you_worship(GOD_SHINING_ONE) && brand == SPWPN_VENOM)
        return true;

    if (you_worship(GOD_CHEIBRIADOS) && brand == SPWPN_CHAOS)
        return true;

    return false;
}

static void _rebrand_weapon(item_def& wpn)
{
    const int old_brand = get_weapon_brand(wpn);
    int new_brand = old_brand;
    const string itname = wpn.name(DESC_YOUR);

    // now try and find an appropriate brand
    while (old_brand == new_brand || _god_hates_brand(new_brand))
    {
        if (is_range_weapon(wpn))
        {
            new_brand = random_choose_weighted(
                                    30, SPWPN_FLAME,
                                    30, SPWPN_FROST,
                                    20, SPWPN_VENOM,
                                    20, SPWPN_VORPAL,
                                    12, SPWPN_EVASION,
                                    5, SPWPN_ELECTROCUTION,
                                    3, SPWPN_CHAOS,
                                    0);
        }
        else
        {
            new_brand = random_choose_weighted(
                                    30, SPWPN_FLAMING,
                                    30, SPWPN_FREEZING,
                                    20, SPWPN_VENOM,
                                    15, SPWPN_DRAINING,
                                    15, SPWPN_VORPAL,
                                    15, SPWPN_ELECTROCUTION,
                                    12, SPWPN_PROTECTION,
                                    8, SPWPN_VAMPIRICISM,
                                    3, SPWPN_CHAOS,
                                    0);
        }
    }

    set_item_ego_type(wpn, OBJ_WEAPONS, new_brand);
    convert2bad(wpn);
}

static void _brand_weapon(item_def &wpn)
{
    you.wield_change = true;

    _rebrand_weapon(wpn);

    const string itname = wpn.name(DESC_YOUR);
    bool success = true;
    colour_t flash_colour = BLACK;

    switch (get_weapon_brand(wpn))
    {
    case SPWPN_VORPAL:
    case SPWPN_PROTECTION:
    case SPWPN_EVASION:
        flash_colour = YELLOW;
        mprf("%s emits a brilliant flash of light!",itname.c_str());
        break;

    case SPWPN_FLAME:
    case SPWPN_FLAMING:
        flash_colour = RED;
        mprf("%s is engulfed in flames!", itname.c_str());
        break;

    case SPWPN_FROST:
    case SPWPN_FREEZING:
        flash_colour = LIGHTCYAN;
        mprf("%s is covered with a thin layer of ice!", itname.c_str());
        break;

    case SPWPN_DRAINING:
    case SPWPN_VAMPIRICISM:
        flash_colour = DARKGREY;
        mprf("%s thirsts for the lives of mortals!", itname.c_str());
        break;

    case SPWPN_VENOM:
        flash_colour = GREEN;
        mprf("%s drips with poison.", itname.c_str());
        break;

    case SPWPN_ELECTROCUTION:
        flash_colour = LIGHTCYAN;
        mprf("%s crackles with electricity.", itname.c_str());
        break;

    case SPWPN_CHAOS:
        flash_colour = random_colour();
        mprf("%s erupts in a glittering mayhem of colour.", itname.c_str());
        break;

    default:
        success = false;
        break;
    }

    if (success)
    {
        item_set_appearance(wpn);
        // Message would spoil this even if we didn't identify.
        set_ident_flags(wpn, ISFLAG_KNOW_TYPE);
        // Might be rebranding to/from protection or evasion.
        you.redraw_armour_class = true;
        you.redraw_evasion = true;
        // Might be removing antimagic.
        calc_mp();
        flash_view_delay(flash_colour, 300);
    }
    return;
}

static object_selector _enchant_selector(scroll_type scroll)
{
    if (scroll == SCR_BRAND_WEAPON)
        return OSEL_BRANDABLE_WEAPON;
    else if (scroll == SCR_ENCHANT_WEAPON_I)
        return OSEL_ENCHANTABLE_WEAPON_I;
    else if (scroll == SCR_ENCHANT_WEAPON_II)
        return OSEL_ENCHANTABLE_WEAPON_II;
    else if (scroll == SCR_ENCHANT_WEAPON_III)
        return OSEL_ENCHANTABLE_WEAPON_III;
    die("Invalid scroll type %d for _enchant_selector", (int)scroll);
}

// Returns NULL if no weapon was chosen.
static item_def* _scroll_choose_weapon(bool alreadyknown, string *pre_msg, scroll_type scroll)
{
    int item_slot;
    const bool branding = scroll == SCR_BRAND_WEAPON;
    const object_selector selector = _enchant_selector(scroll);

    while (true)
    {
        item_slot = prompt_invent_item(branding ? "Brand which weapon?"
                                                : "Enchant which weapon?",
                                       MT_INVLIST, selector,
                                       true, true, false);

        // The scroll is used up if we didn't know what it was originally.
        if (item_slot == PROMPT_NOTHING)
            return NULL;

        if (item_slot == PROMPT_ABORT)
        {
            if (alreadyknown
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return NULL;
            }
            else
                continue;
        }

        item_def* wpn = &you.inv[item_slot];

        if (!is_item_selected(*wpn, selector))
        {
            mpr("Choose a valid weapon, or Esc to abort.");
            more();

            continue;
        }

        // Now we're definitely using up the scroll.
        if (pre_msg && alreadyknown)
            mpr(pre_msg->c_str());

        return wpn;
    }
}

// Returns true if the scroll is used up.
static bool _handle_brand_weapon(bool alreadyknown, string *pre_msg)
{
    item_def* weapon = _scroll_choose_weapon(alreadyknown, pre_msg, SCR_BRAND_WEAPON);

    if (!weapon)
        return !alreadyknown;

    _brand_weapon(*weapon);
    return true;
}

bool enchant_weapon(item_def &wpn, int acc, int dam, const char *colour)
{
    bool success = false;

    // Get item name now before changing enchantment.
    string iname = wpn.name(DESC_YOUR);
    const char *s = wpn.quantity == 1 ? "s" : "";

    // Blowguns only have one stat.
    if (wpn.base_type == OBJ_WEAPONS && wpn.sub_type == WPN_BLOWGUN)
    {
        acc = acc + dam;
        dam = 0;
    }

    if (is_weapon(wpn))
    {
        if (!is_artefact(wpn) && wpn.base_type == OBJ_WEAPONS)
        {
            while (acc--)
            {
                if (wpn.plus < 4 || !x_chance_in_y(wpn.plus, MAX_WPN_ENCHANT))
                    wpn.plus++, success = true;
            }
            while (dam--)
            {
                if (wpn.plus2 < 4 || !x_chance_in_y(wpn.plus2, MAX_WPN_ENCHANT))
                    wpn.plus2++, success = true;
            }
            if (success && colour)
                mprf("%s glow%s %s for a moment.", iname.c_str(), s, colour);
        }
        if (wpn.cursed())
        {
            if (!success && colour)
            {
                if (const char *space = strchr(colour, ' '))
                    colour = space + 1;
                mprf("%s glow%s silvery %s for a moment.", iname.c_str(), s, colour);
            }
            success = true;
        }
        do_uncurse_item(wpn, true, true);
    }

    if (!success && colour)
        mprf("%s very briefly gain%s a %s sheen.", iname.c_str(), s, colour);

    if (success)
        you.wield_change = true;

    return success;
}

// Returns true if the scroll is used up.
static bool _identify(bool alreadyknown, string *pre_msg)
{
    int item_slot = -1;
    while (true)
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item(
                "Identify which item? (\\ to view known items)",
                MT_INVLIST, OSEL_UNIDENT, true, true, false, 0,
                -1, NULL, OPER_ANY, true);
        }

        if (item_slot == PROMPT_NOTHING)
            return !alreadyknown;

        if (item_slot == PROMPT_ABORT)
        {
            if (alreadyknown
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return !alreadyknown;
            }
            else
            {
                item_slot = -1;
                continue;
            }
        }

        item_def& item(you.inv[item_slot]);

        if (fully_identified(item)
            && (!is_deck(item) || top_card_is_known(item)))
        {
            mpr("Choose an unidentified item, or Esc to abort.");
            more();
            item_slot = -1;
            continue;
        }

        if (alreadyknown && pre_msg)
            mpr(pre_msg->c_str());

        set_ident_type(item, ID_KNOWN_TYPE);
        set_ident_flags(item, ISFLAG_IDENT_MASK);

        if (is_deck(item) && !top_card_is_known(item))
            deck_identify_first(item_slot);

        // Output identified item.
        mprf_nocap("%s", item.name(DESC_INVENTORY_EQUIP).c_str());
        if (item_slot == you.equip[EQ_WEAPON])
            you.wield_change = true;

        if (item.base_type == OBJ_JEWELLERY
            && item.sub_type == AMU_INACCURACY
            && item_slot == you.equip[EQ_AMULET]
            && !item_known_cursed(item))
        {
            learned_something_new(HINT_INACCURACY);
        }

        return true;
    }
}

static bool _handle_enchant_weapon(bool alreadyknown, string *pre_msg, scroll_type scr)
{
    item_def* weapon = _scroll_choose_weapon(alreadyknown, pre_msg, scr);
    if (!weapon)
        return !alreadyknown;

    int acc = (scr == SCR_ENCHANT_WEAPON_I ? 1 : scr == SCR_ENCHANT_WEAPON_II ? 0 : 1 + random2(2));
    int dam = (scr == SCR_ENCHANT_WEAPON_I ? 0 : scr == SCR_ENCHANT_WEAPON_II ? 1 : 1 + random2(2));
    enchant_weapon(*weapon, acc, dam, scr == SCR_ENCHANT_WEAPON_I ? "green" :
                                     scr == SCR_ENCHANT_WEAPON_II ? "red"  :
                                     "yellow");
    return true;
}

bool enchant_armour(int &ac_change, bool quiet, item_def &arm)
{
    ASSERT(arm.defined());
    ASSERT(arm.base_type == OBJ_ARMOUR);

    ac_change = 0;

    // Cannot be enchanted nor uncursed.
    if (!is_enchantable_armour(arm, true))
    {
        if (!quiet)
            canned_msg(MSG_NOTHING_HAPPENS);

        // That proved that it was uncursed.
        if (!you_worship(GOD_ASHENZARI))
            arm.flags |= ISFLAG_KNOW_CURSE;

        return false;
    }

    const bool is_cursed = arm.cursed();

    // Turn hides into mails where applicable.
    // NOTE: It is assumed that armour which changes in this way does
    // not change into a form of armour with a different evasion modifier.
    if (armour_is_hide(arm, false))
    {
        if (!quiet)
        {
            mprf("%s glows purple and changes!",
                 arm.name(DESC_YOUR).c_str());
        }

        ac_change = property(arm, PARM_AC);
        hide2armour(arm);
        ac_change = property(arm, PARM_AC) - ac_change;

        do_uncurse_item(arm, true, true);

        // No additional enchantment.
        return true;
    }

    // Even if not affected, it may be uncursed.
    if (!is_enchantable_armour(arm, false))
    {
        if (!quiet)
        {
            if (is_cursed)
            {
                mprf("%s glows silver for a moment.",
                     arm.name(DESC_YOUR).c_str());
            }
            else
                canned_msg(MSG_NOTHING_HAPPENS);
        }
        do_uncurse_item(arm, true, true);
        return is_cursed; // was_cursed, really
    }

    // Output message before changing enchantment and curse status.
    if (!quiet)
    {
        mprf("%s glows green for a moment.",
             arm.name(DESC_YOUR).c_str());
    }

    arm.plus++;
    ac_change++;
    do_uncurse_item(arm, true, true);

    return true;
}

static int _handle_enchant_armour(bool alreadyknown, string *pre_msg)
{
    int item_slot = -1;
    do
    {
        if (item_slot == -1)
        {
            item_slot = prompt_invent_item("Enchant which item?", MT_INVLIST,
                                           OSEL_ENCH_ARM, true, true, false);
        }

        if (item_slot == PROMPT_NOTHING)
            return alreadyknown ? -1 : 0;

        if (item_slot == PROMPT_ABORT)
        {
            if (alreadyknown
                || crawl_state.seen_hups
                || yesno("Really abort (and waste the scroll)?", false, 0))
            {
                canned_msg(MSG_OK);
                return alreadyknown ? -1 : 0;
            }
            else
            {
                item_slot = -1;
                continue;
            }
        }

        item_def& arm(you.inv[item_slot]);

        if (!is_enchantable_armour(arm, true, true))
        {
            mpr("Choose some type of armour to enchant, or Esc to abort.");
            more();

            item_slot = -1;
            continue;
        }

        // Okay, we may actually (attempt to) enchant something.
        if (pre_msg && alreadyknown)
            mpr(pre_msg->c_str());

        int ac_change;
        bool result = enchant_armour(ac_change, false, arm);

        if (ac_change)
            you.redraw_armour_class = true;

        return result ? 1 : 0;
    }
    while (true);

    return 0;
}

static void _handle_read_book(int item_slot)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.stat_zero[STAT_INT])
    {
        mpr("Reading books requires mental cohesion, which you lack.");
        return;
    }

    item_def& book(you.inv[item_slot]);
    ASSERT(book.sub_type != BOOK_MANUAL);

    if (book.sub_type == BOOK_DESTRUCTION)
    {
        if (silenced(you.pos()))
            mpr("This book does not work if you cannot read it aloud!");
        else
            tome_of_power(item_slot);
        return;
    }

    while (true)
    {
        // Spellbook
        const int ltr = read_book(book, RBOOK_READ_SPELL);

        if (ltr < 'a' || ltr > 'h')     //jmf: was 'g', but 8=h
        {
            mesclr();
            return;
        }

        const spell_type spell = which_spell_in_book(book,
                                                     letter_to_index(ltr));
        if (spell == SPELL_NO_SPELL)
        {
            mesclr();
            return;
        }

        describe_spell(spell, &book);

        // Player memorised spell which was being looked at.
        if (you.turn_is_over)
            return;
    }
}

static void _vulnerability_scroll()
{
    // First cast antimagic on yourself.
    antimagic();

    mon_enchant lowered_mr(ENCH_LOWERED_MR, 1, &you, 400);

    // Go over all creatures in LOS.
    for (radius_iterator ri(you.pos(), LOS_NO_TRANS); ri; ++ri)
    {
        if (monster* mon = monster_at(*ri))
        {
            debuff_monster(mon);

            // If relevant, monsters have their MR halved.
            if (!mons_immune_magic(mon))
                mon->add_ench(lowered_mr);

            // Annoying but not enough to turn friendlies against you.
            if (!mon->wont_attack())
                behaviour_event(mon, ME_ANNOY, &you);
        }
    }

    you.set_duration(DUR_LOWERED_MR, 40, 0,
                     "Magic dampens, then quickly surges around you.");
}

static bool _is_cancellable_scroll(scroll_type scroll)
{
    return scroll == SCR_IDENTIFY
           || scroll == SCR_BLINKING
           || scroll == SCR_RECHARGING
           || scroll == SCR_ENCHANT_ARMOUR
           || scroll == SCR_AMNESIA
           || scroll == SCR_REMOVE_CURSE
           || scroll == SCR_CURSE_ARMOUR
           || scroll == SCR_CURSE_JEWELLERY
           || scroll == SCR_BRAND_WEAPON
           || scroll == SCR_ENCHANT_WEAPON_I
           || scroll == SCR_ENCHANT_WEAPON_II
           || scroll == SCR_ENCHANT_WEAPON_III;
}

void read_scroll(int slot)
{
    if (you.berserk())
    {
        canned_msg(MSG_TOO_BERSERK);
        return;
    }

    if (you.confused())
    {
        canned_msg(MSG_TOO_CONFUSED);
        return;
    }

    if (you.duration[DUR_WATER_HOLD] && !you.res_water_drowning())
    {
        mpr("You cannot read scrolls while unable to breathe!");
        return;
    }

    if (inv_count() < 1)
    {
        canned_msg(MSG_NOTHING_CARRIED);
        return;
    }

    int item_slot = (slot != -1) ? slot
                                 : prompt_invent_item("Read which item?",
                                                       MT_INVLIST,
                                                       OBJ_SCROLLS,
                                                       true, true, true, 0, -1,
                                                       NULL, OPER_READ);

    if (prompt_failed(item_slot))
        return;

    item_def& scroll = you.inv[item_slot];

    if ((scroll.base_type != OBJ_BOOKS || scroll.sub_type == BOOK_MANUAL)
        && scroll.base_type != OBJ_SCROLLS)
    {
        mpr("You can't read that!");
        crawl_state.zero_turns_taken();
        return;
    }

    // Here we try to read a book {dlb}:
    if (scroll.base_type == OBJ_BOOKS)
    {
        _handle_read_book(item_slot);
        return;
    }

    if (silenced(you.pos()))
    {
        mpr("Magic scrolls do not work when you're silenced!");
        crawl_state.zero_turns_taken();
        return;
    }

#if TAG_MAJOR_VERSION == 34
    // Prevent hot lava orcs reading scrolls
    if (you.species == SP_LAVA_ORC && temperature_effect(LORC_NO_SCROLLS))
    {
        crawl_state.zero_turns_taken();
        return mpr("You'd burn any scroll you tried to read!");
    }
#endif

    const scroll_type which_scroll = static_cast<scroll_type>(scroll.sub_type);
    const bool alreadyknown = item_type_known(scroll);

    if (alreadyknown)
    {
        switch (which_scroll)
        {
        case SCR_BLINKING:
        case SCR_TELEPORTATION:
            if (you.no_tele(false, false, which_scroll == SCR_BLINKING))
            {
                if (you.species == SP_FORMICID)
                    mpr("You cannot teleport.");
                else
                    mpr("You cannot teleport right now.");
                return;
            }
            break;

        case SCR_ENCHANT_ARMOUR:
            if (!any_items_to_select(OSEL_ENCH_ARM, true))
                return;
            break;

        case SCR_ENCHANT_WEAPON_I:
        case SCR_ENCHANT_WEAPON_II:
        case SCR_ENCHANT_WEAPON_III:
            if (!any_items_to_select(_enchant_selector(which_scroll), true))
                return;
            break;

        case SCR_IDENTIFY:
            if (!any_items_to_select(OSEL_UNIDENT, true))
                return;
            break;

        case SCR_RECHARGING:
            if (!any_items_to_select(OSEL_RECHARGE, true))
                return;
            break;

        case SCR_AMNESIA:
            if (you.spell_no == 0)
            {
                canned_msg(MSG_NO_SPELLS);
                return;
            }
            break;

        case SCR_REMOVE_CURSE:
            if (!any_items_to_select(OSEL_CURSED_WORN, true))
                return;
            break;

        case SCR_CURSE_ARMOUR:
            if (!any_items_to_select(OSEL_UNCURSED_WORN_ARMOUR, true))
                return;
            break;

        case SCR_CURSE_JEWELLERY:
            if (!any_items_to_select(OSEL_UNCURSED_WORN_JEWELLERY, true))
                return;
            break;

        default:
            break;
        }
    }

    // Ok - now we FINALLY get to read a scroll !!! {dlb}
    you.turn_is_over = true;

    zin_recite_interrupt();

    // ... but some scrolls may still be cancelled afterwards.
    bool cancel_scroll = false;

    if (you.stat_zero[STAT_INT] && !one_chance_in(5))
    {
        // mpr("You stumble in your attempt to read the scroll. Nothing happens!");
        // mpr("Your reading takes too long for the scroll to take effect.");
        // mpr("Your low mental capacity makes reading really difficult. You give up!");
        mpr("You almost manage to decipher the scroll, but fail in this attempt.");
        return;
    }

    // Imperfect vision prevents players from reading actual content {dlb}:
    if (player_mutation_level(MUT_BLURRY_VISION)
        && x_chance_in_y(player_mutation_level(MUT_BLURRY_VISION), 5))
    {
        mpr("The writing blurs in front of your eyes.");
        return;
    }

    // For cancellable scrolls leave printing this message to their
    // respective functions.
    string pre_succ_msg =
            make_stringf("As you read the %s, it crumbles to dust.",
                          scroll.name(DESC_QUALNAME).c_str());
    if (!_is_cancellable_scroll(which_scroll))
    {
        mpr(pre_succ_msg.c_str());
        // Actual removal of scroll done afterwards. -- bwr
    }

    const bool dangerous = player_in_a_dangerous_place();

    bool bad_effect = false; // for Xom: result is bad (or at least dangerous)

    int prev_quantity = you.inv[item_slot].quantity;

    switch (which_scroll)
    {
    case SCR_RANDOM_USELESSNESS:
        random_uselessness(item_slot);
        break;

    case SCR_BLINKING:
        // XXX Because some checks in blink() are made before players get to
        // choose target location it is possible to "abuse" scrolls' free
        // cancelling to get some normally hidden information (i.e. presence
        // of (unidentified) -Tele gear).
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            blink(1000, false);
        }
        else
            cancel_scroll = (blink(1000, false, false, &pre_succ_msg) == -1);
        break;

    case SCR_TELEPORTATION:
        you_teleport();
        break;

    case SCR_REMOVE_CURSE:
        if (!alreadyknown)
        {
            mprf("%s", pre_succ_msg.c_str());
            remove_curse(false);
        }
        else
            cancel_scroll = !remove_curse(true, &pre_succ_msg);
        break;

    case SCR_ACQUIREMENT:
        mpr("This is a scroll of acquirement!");
        more();
        // Identify it early in case the player checks the '\' screen.
        set_ident_type(scroll, ID_KNOWN_TYPE);
        run_uncancel(UNC_ACQUIREMENT, AQ_SCROLL);
        break;

    case SCR_FEAR:
        mpr("You assume a fearsome visage.");
        mass_enchantment(ENCH_FEAR, 1000);
        break;

    case SCR_NOISE:
        noisy(25, you.pos(), "You hear a loud clanging noise!");
        break;

    case SCR_SUMMONING:
        cast_shadow_creatures(MON_SUMM_SCROLL);
        break;

    case SCR_FOG:
        mpr("The scroll dissolves into smoke.");
        big_cloud(random_smoke_type(), &you, you.pos(), 50, 8 + random2(8));
        break;

    case SCR_MAGIC_MAPPING:
        magic_mapping(500, 90 + random2(11), false);
        break;

    case SCR_TORMENT:
        torment(&you, TORMENT_SCROLL, you.pos());

        // This is only naughty if you know you're doing it.
        did_god_conduct(DID_NECROMANCY, 10, item_type_known(scroll));
        bad_effect = true;
        break;

    case SCR_IMMOLATION:
    {
        // Dithmenos hates trying to play with fire, even if it does nothing.
        did_god_conduct(DID_FIRE, 2 + random2(3), item_type_known(scroll));

        bool had_effect = false;
        for (monster_near_iterator mi(you.pos(), LOS_NO_TRANS); mi; ++mi)
        {
            if (mons_immune_magic(*mi) || mi->is_summoned())
                continue;

            if (mi->add_ench(mon_enchant(ENCH_INNER_FLAME, 0, &you)))
                had_effect = true;
        }

        if (had_effect)
            mpr("The creatures around you are filled with an inner flame!");
        else
            mpr("The air around you briefly surges with heat, but it dissipates.");

        bad_effect = true;
        break;
    }

    case SCR_CURSE_WEAPON:
    {
        // Not you.weapon() because we want to handle melded weapons too.
        item_def * const weapon = you.slot_item(EQ_WEAPON, true);
        if (!weapon || !is_weapon(*weapon) || weapon->cursed())
        {
            bool plural = false;
            const string weapon_name =
                weapon ? weapon->name(DESC_YOUR)
                       : "Your " + you.hand_name(true, &plural);
            mprf("%s very briefly gain%s a black sheen.",
                 weapon_name.c_str(), plural ? "" : "s");
        }
        else
        {
            // Also sets wield_change.
            do_curse_item(*weapon, false);
            learned_something_new(HINT_YOU_CURSED);
            bad_effect = true;
        }
        break;
    }

    case SCR_ENCHANT_WEAPON_I:
    case SCR_ENCHANT_WEAPON_II:
    case SCR_ENCHANT_WEAPON_III:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            mprf("It is a scroll of enchant weapon %s.",
                    which_scroll == SCR_ENCHANT_WEAPON_I ? "I" :
                    which_scroll == SCR_ENCHANT_WEAPON_II ? "II" :
                    "III");
            // Pause to display the message before jumping to the weapon list.
            more();
        }

        cancel_scroll = !_handle_enchant_weapon(alreadyknown, &pre_succ_msg, which_scroll);
        break;

    case SCR_BRAND_WEAPON:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            mpr("It is a scroll of brand weapon.");
            // Pause to display the message before jumping to the weapon list.
            more();
        }

        cancel_scroll = !_handle_brand_weapon(alreadyknown, &pre_succ_msg);
        break;

    case SCR_IDENTIFY:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            mpr("It is a scroll of identify.");
            more();
            // Do this here so it doesn't turn up in the ID menu.
            set_ident_type(scroll, ID_KNOWN_TYPE);
        }
        cancel_scroll = !_identify(alreadyknown, &pre_succ_msg);
        break;

    case SCR_RECHARGING:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            mpr("It is a scroll of recharging.");
            more();
        }
        cancel_scroll = (recharge_wand(alreadyknown, &pre_succ_msg) == -1);
        break;

    case SCR_ENCHANT_ARMOUR:
        if (!alreadyknown)
        {
            mpr(pre_succ_msg.c_str());
            mpr("It is a scroll of enchant armour.");
            more();
        }
        cancel_scroll =
            (_handle_enchant_armour(alreadyknown, &pre_succ_msg) == -1);
        break;

    // Should always be identified by Ashenzari.
    case SCR_CURSE_ARMOUR:
    case SCR_CURSE_JEWELLERY:
    {
        const bool armour = which_scroll == SCR_CURSE_ARMOUR;
        cancel_scroll = !curse_item(armour, &pre_succ_msg);
        break;
    }

    case SCR_HOLY_WORD:
    {
        holy_word(100, HOLY_WORD_SCROLL, you.pos(), false, &you);

        // This is always naughty, even if you didn't affect anyone.
        // Don't speak those foul holy words even in jest!
        did_god_conduct(DID_HOLY, 10, item_type_known(scroll));
        break;
    }

    case SCR_SILENCE:
        cast_silence(30);
        break;

    case SCR_VULNERABILITY:
        _vulnerability_scroll();
        break;

    case SCR_AMNESIA:
        if (!alreadyknown)
            mpr(pre_succ_msg.c_str());
        if (you.spell_no == 0)
            mpr("You feel forgetful for a moment.");
        else if (!alreadyknown)
            cast_selective_amnesia();
        else
            cancel_scroll = (cast_selective_amnesia(&pre_succ_msg) == -1);
        break;

    default:
        mpr("Read a buggy scroll, please report this.");
        break;
    }

    if (cancel_scroll)
        you.turn_is_over = false;

    set_ident_type(scroll, ID_KNOWN_TYPE);
    set_ident_flags(scroll, ISFLAG_KNOW_TYPE); // for notes

    string scroll_name = scroll.name(DESC_QUALNAME).c_str();

    if (!cancel_scroll)
    {
        dec_inv_item_quantity(item_slot, 1);
        count_action(CACT_USE, OBJ_SCROLLS);
    }

    if (!alreadyknown
        && which_scroll != SCR_ACQUIREMENT
        && which_scroll != SCR_BRAND_WEAPON
        && which_scroll != SCR_ENCHANT_WEAPON_I
        && which_scroll != SCR_ENCHANT_WEAPON_II
        && which_scroll != SCR_ENCHANT_WEAPON_III
        && which_scroll != SCR_IDENTIFY
        && which_scroll != SCR_ENCHANT_ARMOUR
        && which_scroll != SCR_RECHARGING)
    {
        mprf("It %s a %s.",
             you.inv[item_slot].quantity < prev_quantity ? "was" : "is",
             scroll_name.c_str());
    }

    if (!alreadyknown && dangerous)
    {
        // Xom loves it when you read an unknown scroll and there is a
        // dangerous monster nearby... (though not as much as potions
        // since there are no *really* bad scrolls, merely useless ones).
        xom_is_stimulated(bad_effect ? 100 : 50);
    }
}

bool stasis_blocks_effect(bool calc_unid,
                          const char *msg, int noise,
                          const char *silenced_msg)
{
    if (you.stasis(calc_unid))
    {
        item_def *amulet = you.slot_item(EQ_AMULET, false);

        // For non-amulet sources of stasis.
        if (amulet && amulet->sub_type != AMU_STASIS)
            amulet = 0;

        if (msg)
        {
            // Override message for formicids
            if (you.species == SP_FORMICID)
                mpr("Your stasis keeps you stable.");
            else
            {
                const string name(amulet? amulet->name(DESC_YOUR) : "Something");
                const string message = make_stringf(msg, name.c_str());

                if (noise)
                {
                    if (!noisy(noise, you.pos(), message.c_str())
                        && silenced_msg)
                    {
                        mprf(silenced_msg, name.c_str());
                    }
                }
                else
                    mpr(message.c_str());
            }
        }
        return true;
    }
    return false;
}

#ifdef USE_TILE
// Interactive menu for item drop/use.

void tile_item_use_floor(int idx)
{
    if (mitm[idx].base_type == OBJ_CORPSES
        && mitm[idx].sub_type != CORPSE_SKELETON
        && !food_is_rotten(mitm[idx]))
    {
        butchery(idx);
    }
}

void tile_item_pickup(int idx, bool part)
{
    if (item_is_stationary(mitm[idx]))
    {
        mpr("You can't pick that up.");
        return;
    }

    if (part)
    {
        pickup_menu(idx);
        return;
    }
    pickup_single_item(idx, -1);
}

void tile_item_drop(int idx, bool partdrop)
{
    int quantity = you.inv[idx].quantity;
    if (partdrop && quantity > 1)
    {
        quantity = prompt_for_int("Drop how many? ", true);
        if (quantity < 1)
        {
            canned_msg(MSG_OK);
            return;
        }
        if (quantity > you.inv[idx].quantity)
            quantity = you.inv[idx].quantity;
    }
    drop_item(idx, quantity);
}

static bool _prompt_eat_bad_food(const item_def food)
{
    if (food.base_type != OBJ_CORPSES
        && (food.base_type != OBJ_FOOD || food.sub_type != FOOD_CHUNK))
    {
        return true;
    }

    if (!is_bad_food(food))
        return true;

    const string food_colour = item_prefix(food);
    string colour            = "";
    string colour_off        = "";

    const int col = menu_colour(food.name(DESC_A), food_colour, "pickup");
    if (col != -1)
        colour = colour_to_str(col);

    if (!colour.empty())
    {
        // Order is important here.
        colour_off  = "</" + colour + ">";
        colour      = "<" + colour + ">";
    }

    const string qualifier = colour
                             + (is_poisonous(food)      ? "poisonous" :
                                is_mutagenic(food)      ? "mutagenic" :
                                causes_rot(food)        ? "rot-inducing" :
                                is_forbidden_food(food) ? "forbidden" : "")
                             + colour_off;

    string prompt  = "Really ";
           prompt += (you.species == SP_VAMPIRE ? "drink from" : "eat");
           prompt += " this " + qualifier;
           prompt += (food.base_type == OBJ_CORPSES ? " corpse"
                                                    : " chunk of meat");
           prompt += "?";

    if (!yesno(prompt.c_str(), false, 'n'))
    {
        canned_msg(MSG_OK);
        return false;
    }
    return true;
}

void tile_item_eat_floor(int idx)
{
    if (mitm[idx].base_type == OBJ_CORPSES
            && you.species == SP_VAMPIRE
        || mitm[idx].base_type == OBJ_FOOD
            && you.is_undead != US_UNDEAD && you.species != SP_VAMPIRE)
    {
        if (can_ingest(mitm[idx], false)
            && _prompt_eat_bad_food(mitm[idx]))
        {
            eat_item(mitm[idx]);
        }
    }
}

void tile_item_use_secondary(int idx)
{
    const item_def item = you.inv[idx];

    if (item.base_type == OBJ_WEAPONS && is_throwable(&you, item))
    {
        if (check_warning_inscriptions(item, OPER_FIRE))
            fire_thing(idx); // fire weapons
    }
    else if (you.equip[EQ_WEAPON] == idx)
        wield_weapon(true, SLOT_BARE_HANDS);
    else if (_valid_weapon_swap(item))
    {
        // secondary wield for several spells and such
        wield_weapon(true, idx); // wield
    }
}

void tile_item_use(int idx)
{
    const item_def item = you.inv[idx];

    // Equipped?
    bool equipped = false;
    bool equipped_weapon = false;
    for (unsigned int i = 0; i < NUM_EQUIP; i++)
    {
        if (you.equip[i] == idx)
        {
            equipped = true;
            if (i == EQ_WEAPON)
                equipped_weapon = true;
            break;
        }
    }

    // Special case for folks who are wielding something
    // that they shouldn't be wielding.
    // Note that this is only a problem for equipables
    // (otherwise it would only waste a turn)
    if (you.equip[EQ_WEAPON] == idx
        && (item.base_type == OBJ_ARMOUR
            || item.base_type == OBJ_JEWELLERY))
    {
        wield_weapon(true, SLOT_BARE_HANDS);
        return;
    }

    const int type = item.base_type;

    // Use it
    switch (type)
    {
        case OBJ_WEAPONS:
        case OBJ_STAVES:
        case OBJ_RODS:
        case OBJ_MISCELLANY:
        case OBJ_WANDS:
            // Wield any unwielded item of these types.
            if (!equipped && item_is_wieldable(item))
            {
                wield_weapon(true, idx);
                return;
            }
            // Evoke misc. items, rods, or wands.
            if (item_is_evokable(item, false))
            {
                evoke_item(idx);
                return;
            }
            // Unwield wielded items.
            if (equipped)
                wield_weapon(true, SLOT_BARE_HANDS);
            return;

        case OBJ_MISSILES:
            if (check_warning_inscriptions(item, OPER_FIRE))
                fire_thing(idx);
            return;

        case OBJ_ARMOUR:
            if (!form_can_wear())
            {
                mpr("You can't wear or remove anything in your present form.");
                return;
            }
            if (equipped && !equipped_weapon)
            {
                if (check_warning_inscriptions(item, OPER_TAKEOFF))
                    takeoff_armour(idx);
            }
            else if (check_warning_inscriptions(item, OPER_WEAR))
                wear_armour(idx);
            return;

        case OBJ_CORPSES:
            if (you.species != SP_VAMPIRE
                || item.sub_type == CORPSE_SKELETON
                || food_is_rotten(item))
            {
                break;
            }
            // intentional fall-through for Vampires
        case OBJ_FOOD:
            if (check_warning_inscriptions(item, OPER_EAT)
                && _prompt_eat_bad_food(item))
            {
                eat_food(idx);
            }
            return;

        case OBJ_BOOKS:
            if (item.sub_type == BOOK_MANUAL)
                return;
            if (!item_is_spellbook(item) || !you.skill(SK_SPELLCASTING))
            {
                if (check_warning_inscriptions(item, OPER_READ))
                    _handle_read_book(idx);
            } // else it's a spellbook
            else if (check_warning_inscriptions(item, OPER_MEMORISE))
                learn_spell(); // offers all spells, might not be what we want
            return;

        case OBJ_SCROLLS:
            if (check_warning_inscriptions(item, OPER_READ))
                read_scroll(idx);
            return;

        case OBJ_JEWELLERY:
            if (equipped && !equipped_weapon)
                remove_ring(idx);
            else if (check_warning_inscriptions(item, OPER_PUTON))
                puton_ring(idx);
            return;

        case OBJ_POTIONS:
            if (check_warning_inscriptions(item, OPER_QUAFF))
                drink(idx);
            return;

        default:
            return;
    }
}
#endif
