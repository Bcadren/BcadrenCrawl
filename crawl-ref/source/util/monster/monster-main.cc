/*
 * ===========================================================================
 * Copyright (C) 2007 Marc H. Thoben
 * Copyright (C) 2008 Darshan Shaligram
 * Copyright (C) 2010 Jude Brown
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 * ===========================================================================
 * All commits by Adam Borowski and Darshan Shaligram are instead under plain
 * GPL 2+, like Crawl itself.
 * All commits by Stefan O'Rear are under the 2-clause BSD.
 * ===========================================================================
 */

#include "AppHdr.h"
#include "externs.h"
#include "directn.h"
#include "unwind.h"
#include "env.h"
#include "colour.h"
#include "dungeon.h"
#include "los.h"
#include "message.h"
#include "mon-abil.h"
#include "mon-book.h"
#include "mon-cast.h"
#include "mon-util.h"
#include "version.h"
#include "view.h"
#include "los.h"
#include "maps.h"
#include "initfile.h"
#include "libutil.h"
#include "itemname.h"
#include "itemprop.h"
#include "act-iter.h"
#include "mon-death.h"
#include "random.h"
#include "spl-util.h"
#include "state.h"
#include "stepdown.h"
#include "stringutil.h"
#include "artefact.h"
#include "vault_monsters.h"
#include <sstream>
#include <set>
#include <unistd.h>

extern const spell_type serpent_of_hell_breaths[4][3];

const coord_def MONSTER_PLACE(20, 20);

const std::string CANG = "cang";

const int PLAYER_MAXHP = 500;
const int PLAYER_MAXMP = 50;

// Clockwise, around the compass from north (same order as enum RUN_DIR)
const struct coord_def Compass[9] =
{
    coord_def(0, -1), coord_def(1, -1), coord_def(1, 0), coord_def(1, 1),
    coord_def(0, 1), coord_def(-1, 1), coord_def(-1, 0), coord_def(-1, -1),
    coord_def(0, 0),
};

bool is_element_colour(int col)
{
    col = col & 0x007f;
    ASSERT(col < NUM_COLOURS);
    return (col >= ETC_FIRE);
}


static std::string colour_codes[] = {
    "",
    "02",
    "03",
    "10",
    "05",
    "06",
    "07",
    "15",
    "14",
    "12",
    "09",
    "11",
    "04",
    "13",
    "08",
    "16"
};

static int bgr[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

#ifdef CONTROL
#undef CONTROL
#endif
#define CONTROL(x) char(x - 'A' + 1)

static std::string colour(int colour, std::string text, bool bg = false)
{
    if (is_element_colour(colour))
        colour = element_colour(colour, true);

    if (isatty(1))
    {
        if (!colour)
            return text;
        return make_stringf("\e[0;%d%d%sm%s\e[0m", bg ? 4 : 3, bgr[colour & 7],
                            (!bg && (colour & 8)) ? ";1" : "", text.c_str());
    }

    const std::string code(colour_codes[colour]);

    if (code.empty())
        return text;

    return (std::string() + CONTROL('C') + code + (bg ? ",01" : "")
            + text + CONTROL('O'));
}

std::string uppercase_first(std::string s);

template <class T> inline std::string to_string (const T& t);

static void record_resvul(int color, const char *name, const char *caption,
                          std::string &str, int rval)
{
  if (str.empty())
    str = " | " + std::string(caption) + ": ";
  else
    str += ", ";

  if (color && (rval == 3 || rval == 1 && color == BROWN
                || std::string(caption) == "Vul")
            && (int) color <= 7)
    color += 8;

  std::string token(name);
  if (rval > 1 && rval <= 3) {
    while (rval-- > 0)
      token += "+";
  }

  str += colour(color, token);
}

static void record_resist(int colour, std::string name,
                          std::string &res, std::string &vul,
                          int rval)
{
  if (rval > 0)
    record_resvul(colour, name.c_str(), "Res", res, rval);
  else if (rval < 0)
    record_resvul(colour, name.c_str(), "Vul", vul, -rval);
}

static void monster_action_cost(std::string &qual, int cost, const char *desc) {
  char buf[80];
  if (cost != 10) {
    snprintf(buf, sizeof buf, "%s: %d%%", desc, cost * 10);
    if (!qual.empty())
      qual += "; ";
    qual += buf;
  }
}

static std::string monster_int(const monster &mon)
{
  std::string intel = "???";
  switch (mons_intel(&mon))
  {
  case I_BRAINLESS:
    intel = "brainless";
    break;
  case I_ANIMAL:
    intel = "animal";
    break;
  case I_HUMAN:
    intel = "human";
    break;
  // Let the compiler issue warnings for missing entries.
  }

  return intel;
}

static std::string monster_size(const monster &mon)
{
  switch (mon.body_size())
  {
  case SIZE_TINY:
    return "tiny";
  case SIZE_LITTLE:
    return "little";
  case SIZE_SMALL:
    return "small";
  case SIZE_MEDIUM:
    return "Medium";
  case SIZE_LARGE:
    return "Large";
  case SIZE_BIG:
    return "Big";
  case SIZE_GIANT:
    return "Giant";
  default:
    return "???";
  }
}

static std::string monster_speed(const monster &mon,
                                 const monsterentry *me,
                                 int speed_min,
                                 int speed_max)
{
  std::string speed;

  char buf[50];
  if (speed_max != speed_min)
    snprintf(buf, sizeof buf, "%i-%i", speed_min, speed_max);
  else if (speed_max == 0)
    snprintf(buf, sizeof buf, "%s", colour(BROWN, "0").c_str());
  else
    snprintf(buf, sizeof buf, "%i", speed_max);

  speed += buf;

  const mon_energy_usage &cost = mons_energy(&mon);
  std::string qualifiers;

  bool skip_action = false;
  if (cost.attack != 10
      && cost.attack == cost.missile && cost.attack == cost.spell
      && cost.attack == cost.special && cost.attack == cost.item)
  {
    monster_action_cost(qualifiers, cost.attack, "act");
    skip_action = true;
  }

  monster_action_cost(qualifiers, cost.move, "move");
  if (cost.swim != cost.move)
    monster_action_cost(qualifiers, cost.swim, "swim");
  if (!skip_action)
  {
    monster_action_cost(qualifiers, cost.attack, "atk");
    monster_action_cost(qualifiers, cost.missile, "msl");
    monster_action_cost(qualifiers, cost.spell, "spell");
    monster_action_cost(qualifiers, cost.special, "special");
    monster_action_cost(qualifiers, cost.item, "item");
  }
  if (speed_max > 0 && mons_class_flag(mon.type, M_STATIONARY))
  {
    if (!qualifiers.empty())
      qualifiers += "; ";
    qualifiers += colour(BROWN, "stationary");
  }

  if (!qualifiers.empty())
    speed += " (" + qualifiers + ")";

  return speed;
}

static void mons_flag(std::string &flag, const std::string &newflag) {
  if (flag.empty())
    flag = " | ";
  else
    flag += ", ";
  flag += newflag;
}

static void mons_check_flag(bool set, std::string &flag,
                            const std::string &newflag)
{
  if (set)
    mons_flag(flag, newflag);
}

static void initialize_crawl() {
  init_monsters();
  init_properties();
  init_item_name_cache();

  init_spell_descs();
  init_monster_symbols();
  init_mon_name_cache();
  init_spell_name_cache();
  init_mons_spells();
  init_element_colours();
  init_show_table(); // Initializes indices for get_feature_def.

  dgn_reset_level();
  for (int y = 0; y < GYM; ++y)
    for (int x = 0; x < GXM; ++x)
      grd[x][y] = DNGN_FLOOR;

  los_changed();
  you.hp = you.hp_max = PLAYER_MAXHP;
  you.magic_points = you.max_magic_points = PLAYER_MAXMP;
  you.species = SP_HUMAN;
}

static std::string dice_def_string(dice_def dice) {
  return (dice.num == 1? make_stringf("d%d", dice.size)
          : make_stringf("%dd%d", dice.num, dice.size));
}

static dice_def mi_calc_iood_damage(monster *mons) {
  const int power = stepdown_value(6 * mons->get_experience_level(),
                                   30, 30, 200, -1);
  return dice_def(9, power / 4);
}

static std::string mi_calc_smiting_damage(monster *mons) {
  return "7-17";
}

static std::string mi_calc_airstrike_damage(monster *mons) {
  return make_stringf("0-%d", 10 + 2 * mons->get_experience_level());
}

static std::string mi_calc_glaciate_damage(monster *mons) {
  int pow = 12 * mons->get_experience_level();
  // Minimum of the number of dice, or the max damage at max range
  int min = std::min(10, (54 + 3 * pow / 2) / 6);
  // Maximum damage at minimum range.
  int max = (54 + 3 * pow / 2) / 3;

  return make_stringf("%d-%d", min, max);
}

static std::string mi_calc_chain_lightning_damage(monster* mons) {
  int pow = 4 * mons->get_experience_level();

  // Damage is 5d(9.2 + pow / 30), but if lots of targets are around
  // it can hit the player precisely once at very low (e.g. 1) power
  // and deal 5 damage.
  int min = 5;

  // Max damage per bounce is 46 + pow / 6; in the worst case every other
  // bounce hits the player, losing 8 pow on the bounce away and 8 on the
  // bounce back for a total of 16; thus, for n bounces, it's:
  // (46 + pow/6) * n less 16/6 times the (n - 1)th triangular number.
  int n = (pow + 15) / 16;
  int max = (46 + (pow / 6)) * n - 4 * n * (n - 1) / 3;

  return make_stringf("%d-%d", min, max);
}

static std::string mons_human_readable_spell_damage_string(
    monster *monster,
    spell_type sp)
{
  bolt spell_beam =
    mons_spell_beam(monster, sp, mons_power_for_hd(sp, monster->spell_hd(sp),
                                                   false),
                    true);
  // Fake damage beam
  if (sp == SPELL_PORTAL_PROJECTILE || sp == SPELL_LRD)
    return "";
  if (sp == SPELL_SMITING)
    return mi_calc_smiting_damage(monster);
  if (sp == SPELL_AIRSTRIKE)
    return mi_calc_airstrike_damage(monster);
  if (sp == SPELL_GLACIATE)
    return mi_calc_glaciate_damage(monster);
  if (sp == SPELL_IOOD || spell_beam.origin_spell == SPELL_IOOD)
    spell_beam.damage = mi_calc_iood_damage(monster);
  if (sp == SPELL_CHAIN_LIGHTNING)
    return mi_calc_chain_lightning_damage(monster);
  if (spell_beam.damage.size && spell_beam.damage.num)
    return dice_def_string(spell_beam.damage);
  return ("");
}

static std::string shorten_spell_name(std::string name) {
  lowercase(name);
  std::string::size_type pos = name.find('\'');
  if (pos != std::string::npos ) {
    pos = name.find(' ', pos);
    if (pos != std::string::npos)
    {
      // strip wizard names
      if (starts_with(name, "iskenderun's")
          || starts_with(name, "olgreb's")
          || starts_with(name, "lee's")
          || starts_with(name, "leda's")
          || starts_with(name, "lehudib's")
          || starts_with(name, "borgnjor's")
          || starts_with(name, "ozocubu's")
          || starts_with(name, "tukima's")
          || starts_with(name, "alistair's"))
      {
        name = name.substr(pos + 1);
      }
    }
  }
  if ((pos = name.find(" of ")) != std::string::npos)
    name = name.substr(0, 1) + "." + name.substr(pos+4);
  if (starts_with(name, "summon "))
    name = "sum." + name.substr(7);
  if (ends_with(name, " bolt"))
    name = "b." + name.substr(0, name.length() - 5);
  return (name);
}

std::string spell_flag_string(const mon_spell_slot &slot)
{
  std::string flags;

  if (!(slot.flags & MON_SPELL_ANTIMAGIC_MASK))
    flags += colour(LIGHTCYAN, "!AM");
  if (!(slot.flags & MON_SPELL_SILENCE_MASK))
  {
    if (!flags.empty())
      flags += ", ";
    flags += colour(MAGENTA, "!sil");
  }
  if (slot.flags & MON_SPELL_BREATH)
  {
    if (!flags.empty())
      flags += ", ";
    flags += colour(YELLOW, "breath");
  }
  if (slot.flags & MON_SPELL_EMERGENCY)
  {
    if (!flags.empty())
      flags += ", ";
    flags += colour(LIGHTRED, "emergency");
  }

  if (!flags.empty())
    flags = " [" + flags + "]";
  return flags;
}

// ::first is spell name, ::second is possible damages
typedef std::multimap<std::string, std::string> spell_damage_map;
static spell_damage_map record_spell_set(monster *mp, std::string& ret)
{
  spell_damage_map damages;
  for (std::size_t i = 0; i < mp->spells.size(); ++i) {
    spell_type sp = mp->spells[i].spell;
    if (!ret.empty())
      ret += ", ";
    if (sp == SPELL_SERPENT_OF_HELL_BREATH) {
      const int idx =
            mp->type == MONS_SERPENT_OF_HELL          ? 0
          : mp->type == MONS_SERPENT_OF_HELL_COCYTUS  ? 1
          : mp->type == MONS_SERPENT_OF_HELL_DIS      ? 2
          : mp->type == MONS_SERPENT_OF_HELL_TARTARUS ? 3
          :                                               -1;
      ASSERT(idx >= 0 && idx <= 3);
      ASSERT(mp->number == ARRAYSZ(serpent_of_hell_breaths[idx]));

      ret += "{";
      for (unsigned int k = 0; k < mp->number; ++k) {
        const spell_type breath = serpent_of_hell_breaths[idx][k];
        const std::string rawname = spell_title(breath);
        ret += k == 0 ? "" : ", ";
        ret += make_stringf("head %d: ", k + 1) + shorten_spell_name(rawname) + " (";
        ret += mons_human_readable_spell_damage_string(mp, breath) + ")";
      }
      ret += "}";

      ret += spell_flag_string(mp->spells[i]);
    }
    else {
      std::string spell_name = spell_title(sp);
      spell_name = shorten_spell_name(spell_name);
      ret += spell_name;
      ret += spell_flag_string(mp->spells[i]);

      for (int i = 0; i < 100; i++) {
        std::string damage = mons_human_readable_spell_damage_string(mp, sp);
        std::set<std::string> added_damages;
        if (!damage.empty() && !added_damages.count(damage))
        {
          damages.insert(std::pair<std::string, std::string>(spell_name, damage));
          added_damages.insert(damage);
        }
      }
    }
  }
  return damages;
}

static std::string construct_spells(std::set<std::string> spells,
                                    spell_damage_map damages)
{
  std::string ret;
  for (std::set<std::string>::const_iterator i = spells.begin();
       i != spells.end(); ++i)
  {
    if (i != spells.begin())
      ret += " / ";
    ret += *i;
  }
  std::map<std::string, std::string> merged_spell_dam;
  for (spell_damage_map::const_iterator i = damages.begin(); i != damages.end(); ++i)
  {
    std::string dam = merged_spell_dam[i->first];
    if (!dam.empty())
      dam += " / ";
    dam += i->second;
    merged_spell_dam[i->first] = dam;
  }

  for (std::map<std::string, std::string>::const_iterator i = merged_spell_dam.begin();
       i != merged_spell_dam.end(); ++i)
  {
    ret = replace_all(ret, i->first, make_stringf("%s (%s)", i->first.c_str(), i->second.c_str()));
  }

  return ret;
}

static inline void set_min_max(int num, int &min, int &max) {
  if (!min || num < min)
    min = num;
  if (!max || num > max)
    max = num;
}

static std::string monster_symbol(const monster &mon) {
  std::string symbol;
  const monsterentry *me = mon.find_monsterentry();
  if (me) {
    monster_info mi(&mon, MILEV_NAME);
    symbol += me->basechar;
    symbol = colour(mi.colour(), symbol);
  }
  return (symbol);
}

int mi_create_monster(mons_spec spec) {
  item_list items = spec.items;
  for (unsigned int i = 0; i < spec.items.size(); i++)
  {
    int ego = spec.items.get_item(i).ego;
    if (ego >= 0)
      continue;
    // XXX: this is an unholy hack; xref set_unique_item_status
    item_def tempitem;
    tempitem.flags |= ISFLAG_UNRANDART;
    tempitem.special = -ego;
    set_unique_item_status(tempitem, UNIQ_NOT_EXISTS);
  }
  monster *monster = 
    dgn_place_monster(spec, MONSTER_PLACE, true, false, false);
  if (monster) {
    monster->behaviour = BEH_SEEK;
    monster->foe = MHITYOU;
    no_messages mx;
    monster->del_ench(ENCH_SUBMERGED);
    return monster->mindex();
  }
  return NON_MONSTER;
}

static std::string damage_flavour(const std::string &name,
                                  const std::string &damage)
{
  return "(" + name + ":" + damage + ")";
}

static std::string damage_flavour(const std::string &name,
                                  int low, int high)
{
  return make_stringf("(%s:%d-%d)", name.c_str(), low, high);
}

static void rebind_mspec(std::string *requested_name,
                         const std::string &actual_name,
                         mons_spec *mspec)
{
  if (*requested_name != actual_name
      && (requested_name->find("draconian") == 0
          || requested_name->find("blood saint") == 0
          || requested_name->find("corrupter") == 0
          || requested_name->find("warmonger") == 0
          || requested_name->find("chaos champion") == 0
          || requested_name->find("black sun") == 0))
  {
    // If the user requested a drac, the game might generate a
    // coloured drac in response. Try to reuse that colour for further
    // tests.
    mons_list mons;
    const std::string err = mons.add_mons(actual_name, false);
    if (err.empty())
    {
      *mspec          = mons.get_monster(0);
      *requested_name = actual_name;
    }
  }
}

static std::string canned_reports[][2] = {
  { "cang",
    ("cang (" + colour(LIGHTRED, "Ω")
     + (") | Spd: c | HD: i | HP: 666 | AC/EV: e/π | Dam: 999"
         " | Res: sanity | XP: ∞ | Int: god | Sz: !!!")) },
};

int main(int argc, char *argv[])
{
  alarm(5);
  crawl_state.test = true;
  if (argc < 2)
  {
    printf("Usage: @? <monster name>\n");
    return 0;
  }

  if (!strcmp(argv[1], "-version") || !strcmp(argv[1], "--version"))
  {
    printf("Monster stats Crawl version: %s\n", Version::Long);
    return 0;
  }
  else if (!strcmp(argv[1], "-name") || !strcmp(argv[1], "--name"))
  {
    seed_rng();
    string name = make_name(random_int(), MNAME_DEFAULT);
    printf("%s\n", name.c_str());
    return 0;
  }

  initialize_crawl();

  mons_list mons;
  std::string target = argv[1];

  if (argc > 2)
    for (int x = 2; x < argc; x++)
    {
      target.append(" ");
      target.append(argv[x]);
    }

  trim_string(target);

  const bool want_vault_spec = target.find("spec:") == 0;
  if (want_vault_spec)
  {
    target.erase(0, 5);
    trim_string(target);
  }

  // [ds] Nobody mess with cang.
  for (unsigned i = 0; i < sizeof(canned_reports) / sizeof(*canned_reports);
       ++i)
  {
    if (canned_reports[i][0] == target)
    {
      printf("%s\n", canned_reports[i][1].c_str());
      return 0;
    }
  }

  std::string orig_target = std::string(target);

  std::string err = mons.add_mons(target, false);
  if (!err.empty()) {
    target = "the " + target;
    const std::string test = mons.add_mons(target, false);
    if (test.empty())
      err = test;
  }

  mons_spec spec = mons.get_monster(0);
  monster_type spec_type = static_cast<monster_type>(spec.type);
  bool vault_monster = false;
  string vault_spec;

  if ((spec_type < 0 || spec_type >= NUM_MONSTERS
       || spec_type == MONS_PLAYER_GHOST)
      || !err.empty())
  {
    spec = get_vault_monster(orig_target, &vault_spec);
    spec_type = static_cast<monster_type>(spec.type);
    if (spec_type < 0 || spec_type >= NUM_MONSTERS
        || spec_type == MONS_PLAYER_GHOST)
    {
      if (err.empty())
        printf("unknown monster: \"%s\"\n", target.c_str());
      else
        printf("%s\n", err.c_str());
      return 1;
    }

    // get_vault_monster created the monster; make uniques ungenerated again
    if (mons_is_unique(spec_type))
      you.unique_creatures.set(spec_type, false);

    vault_monster = true;
  }

  if (want_vault_spec)
  {
    if (!vault_monster)
    {
      printf("Not a vault monster: %s\n", orig_target.c_str());
      return 1;
    }
    else
    {
      printf("%s: %s\n", orig_target.c_str(), vault_spec.c_str());
      return 0;
    }
  }

  int index = mi_create_monster(spec);
  if (index < 0 || index >= MAX_MONSTERS) {
    printf("Failed to create test monster for %s\n", target.c_str());
    return 1;
  }

  const int ntrials = 100;


  long exper = 0L;
  int hp_min = 0;
  int hp_max = 0;
  int mac = 0;
  int mev = 0;
  int speed_min = 0, speed_max = 0;
  // Calculate averages.
  std::set<std::string> spells;
  spell_damage_map damages;
  for (int i = 0; i < ntrials; ++i) {
    monster *mp = &menv[index];
    const std::string mname = mp->name(DESC_PLAIN, true);
    exper += exper_value(mp);
    mac += mp->armour_class();
    mev += mp->evasion();
    set_min_max(mp->speed, speed_min, speed_max);
    set_min_max(mp->hit_points, hp_min, hp_max);

    std::string new_spells;
    const spell_damage_map new_damages = record_spell_set(mp, new_spells);
    for (spell_damage_map::const_iterator i = new_damages.begin(); i != new_damages.end(); ++i)
    {
      bool skip = false;
      std::pair<spell_damage_map::iterator, spell_damage_map::iterator> old_damages;
      old_damages = damages.equal_range(i->first);
      for (spell_damage_map::iterator j = old_damages.first; j != old_damages.second; ++j)
      {
        if (j->second == i->second)
        {
          skip = true;
          break;
        }
      }
      if (skip) continue;
      damages.insert(*i);
    }
    if (!new_spells.empty())
      spells.insert(new_spells);

    // Destroy the monster.
    mp->reset();
    you.unique_creatures.set(spec_type, false);

    rebind_mspec(&target, mname, &spec);

    index = mi_create_monster(spec);
    if (index == -1) {
      printf("Unexpected failure generating monster for %s\n",
             target.c_str());
      return 1;
    }
  }
  exper /= ntrials;
  mac /= ntrials;
  mev /= ntrials;

  monster &mon(menv[index]);

  const std::string symbol(monster_symbol(mon));

  const bool shapeshifter =
      mon.is_shapeshifter()
      || spec_type == MONS_SHAPESHIFTER
      || spec_type == MONS_GLOWING_SHAPESHIFTER;

  const bool nonbase =
      mons_species(mon.type) == MONS_DRACONIAN
      && mon.type != MONS_DRACONIAN
      || mons_species(mon.type) == MONS_DEMONSPAWN
         && mon.type != MONS_DEMONSPAWN;

  const monsterentry *me =
      shapeshifter ? get_monster_data(spec_type) : mon.find_monsterentry();

  const monsterentry *mbase =
      nonbase
      ? get_monster_data(draco_or_demonspawn_subspecies(&mon))
      : (monsterentry*) 0;

  if (me)
  {
    std::string monsterflags;
    std::string monsterresistances;
    std::string monstervulnerabilities;
    std::string monsterattacks;

    lowercase(target);

    const bool changing_name =
      mon.has_hydra_multi_attack() || mon.type == MONS_PANDEMONIUM_LORD
        || shapeshifter || mon.type == MONS_DANCING_WEAPON;

    printf("%s (%s)",
           changing_name ? me->name : mon.name(DESC_PLAIN, true).c_str(),
           symbol.c_str());

    if (mons_class_flag(mon.type, M_UNFINISHED))
        printf(" | %s", colour(LIGHTRED, "UNFINISHED").c_str());

    printf(" | Spd: %s",
           monster_speed(mon, me, speed_min, speed_max).c_str());

    const int hd = mon.get_experience_level();
    printf(" | HD: %d", hd);

    printf(" | HP: ");
    const int hplow = hp_min;
    const int hphigh = hp_max;
    if (hplow < hphigh)
        printf("%i-%i", hplow, hphigh);
    else
        printf("%i", hplow);

    printf(" | AC/EV: %i/%i", mac, mev);

    std::string defenses;
    if (mon.is_spiny() > 0)
        defenses += colour(YELLOW, "(spiny 5d4)");
    if (mons_species(mons_base_type(&mon)) == MONS_MINOTAUR)
        defenses += colour(LIGHTRED, "(headbutt: d20-1)");
    if (defenses != "")
        printf(" %s", defenses.c_str());

    mon.wield_melee_weapon();
    for (int x = 0; x < 4; x++)
    {
      mon_attack_def orig_attk(me->attack[x]);
      int attack_num = x;
      if (mon.has_hydra_multi_attack())
          attack_num = x == 0 ? x : x + mon.number - 1;
      mon_attack_def attk = mons_attack_spec(&mon, attack_num);
      if (attk.type)
      {
        if (monsterattacks.empty())
          monsterattacks = " | Dam: ";
        else
          monsterattacks += ", ";

        int frenzy_degree = -1;
        short int dam = attk.damage;
        if (mon.has_ench(ENCH_BERSERK) || mon.has_ench(ENCH_MIGHT))
          dam = dam * 3 / 2;
        else if (mon.has_ench(ENCH_BATTLE_FRENZY))
          frenzy_degree = mon.get_ench(ENCH_BATTLE_FRENZY).degree;
        else if (mon.has_ench(ENCH_ROUSED))
          frenzy_degree = mon.get_ench(ENCH_ROUSED).degree;

        if (frenzy_degree != -1)
          dam = dam * (115 + frenzy_degree * 15) / 100;

        if (mon.has_ench(ENCH_WEAK))
          dam = dam * 2 / 3;

        monsterattacks += to_string(dam);

        if (attk.type == AT_CONSTRICT)
            monsterattacks += colour(GREEN, "(constrict)");
        
        if (attk.type == AT_CLAW && mon.has_claws() >= 3)
            monsterattacks += colour(LIGHTGREEN, "(claw)");

        if (attk.type == AT_REACH_STING)
            monsterattacks += colour(RED, "(reach)");

        const attack_flavour flavour(
            orig_attk.flavour == AF_KLOWN || orig_attk.flavour == AF_DRAIN_STAT
                ? orig_attk.flavour : attk.flavour);

        switch (flavour)
        {
        case AF_REACH:
          monsterattacks += "(reach)";
          break;
        case AF_KITE:
          monsterattacks += "(kite)";
          break;
        case AF_SWOOP:
          monsterattacks += "(swoop)";
          break;
        case AF_ACID:
          monsterattacks += colour(YELLOW,
                                   damage_flavour("acid", "7d3"));
          break;
        case AF_BLINK:
          monsterattacks += colour(MAGENTA, "(blink self)");
          break;
        case AF_COLD:
          monsterattacks +=
            colour(LIGHTBLUE, damage_flavour("cold", hd, 3 * hd - 1));
          break;
        case AF_CONFUSE:
          monsterattacks += colour(LIGHTMAGENTA,"(confuse)");
          break;
        case AF_DRAIN_DEX:
          monsterattacks += colour(RED,"(drain dexterity)");
          break;
        case AF_DRAIN_STR:
          monsterattacks += colour(RED,"(drain strength)");
          break;
        case AF_DRAIN_XP:
          monsterattacks += colour(LIGHTMAGENTA, "(drain)");
          break;
        case AF_CHAOS:
          monsterattacks += colour(LIGHTGREEN, "(chaos)");
          break;
        case AF_ELEC:
          monsterattacks +=
            colour(LIGHTCYAN,
                   damage_flavour("elec", hd, hd + std::max(hd / 2 - 1, 0)));
          break;
        case AF_FIRE:
          monsterattacks +=
            colour(LIGHTRED, damage_flavour("fire", hd, hd * 2 - 1));
          break;
        case AF_PURE_FIRE:
          monsterattacks +=
            colour(LIGHTRED, damage_flavour("pure fire", hd*3/2, hd*5/2 - 1));
          break;
        case AF_STICKY_FLAME:
          monsterattacks += colour(LIGHTRED, "(napalm)");
          break;
        case AF_HUNGER:
          monsterattacks += colour(BLUE, "(hunger)");
          break;
        case AF_MUTATE:
          monsterattacks += colour(LIGHTGREEN, "(mutation)");
          break;
        case AF_PARALYSE:
          monsterattacks += colour(LIGHTRED, "(paralyse)");
          break;
        case AF_POISON:
          monsterattacks +=
            colour(YELLOW, damage_flavour("poison", hd*2, hd*4));
          break;
        case AF_POISON_STRONG:
          monsterattacks +=
            colour(LIGHTRED, damage_flavour("strong poison", hd*11/3, hd*13/2));
          break;
        case AF_ROT:
          monsterattacks += colour(LIGHTRED,"(rot)");
          break;
        case AF_VAMPIRIC:
          monsterattacks += colour(RED,"(vampiric)");
          break;
        case AF_KLOWN:
          monsterattacks += colour(LIGHTBLUE,"(klown)");
          break;
        case AF_SCARAB:
          monsterattacks += colour(LIGHTMAGENTA,"(scarab)");
          break;
        case AF_DISTORT:
          monsterattacks += colour(LIGHTBLUE,"(distort)");
          break;
        case AF_RAGE:
          monsterattacks += colour(RED,"(rage)");
          break;
        case AF_HOLY:
          monsterattacks += colour(YELLOW,"(holy)");
          break;
        case AF_PAIN:
          monsterattacks += colour(RED,"(pain)");
          break;
        case AF_ANTIMAGIC:
          monsterattacks += colour(LIGHTBLUE,"(antimagic)");
          break;
        case AF_DRAIN_INT:
          monsterattacks += colour(BLUE, "(drain int)");
          break;
        case AF_DRAIN_STAT:
          monsterattacks += colour(BLUE, "(drain stat)");
          break;
        case AF_STEAL:
          monsterattacks += colour(CYAN, "(steal)");
          break;
        case AF_ENSNARE:
          monsterattacks += colour(WHITE, "(ensnare)");
          break;
        case AF_DROWN:
          monsterattacks += colour(LIGHTBLUE, "(drown)");
          break;
        case AF_ENGULF:
          monsterattacks += colour(LIGHTBLUE, "(engulf)");
          break;
        case AF_DRAIN_SPEED:
          monsterattacks += colour(LIGHTMAGENTA, "(drain speed)");
          break;
        case AF_VULN:
          monsterattacks += colour(LIGHTBLUE, "(vuln)");
          break;
        case AF_WEAKNESS_POISON:
          monsterattacks += colour(LIGHTRED, "(poison, weakness)");
          break;
        case AF_SHADOWSTAB:
          monsterattacks += colour(MAGENTA, "(shadow stab)");
          break;
        case AF_CORRODE:
          monsterattacks += colour(BROWN, "(corrosion)");
          break;
        case AF_FIREBRAND:
          monsterattacks +=
            colour(RED, damage_flavour("firebrand", hd, hd * 2 - 1));
          break;
        case AF_TRAMPLE:
          monsterattacks += colour(BROWN, "(trample)");
          break;
        case AF_CRUSH:
        case AF_PLAIN:
          break;
#if TAG_MAJOR_VERSION == 34
        case AF_DISEASE:
        case AF_PLAGUE:
        case AF_STEAL_FOOD:
        case AF_POISON_MEDIUM:
        case AF_POISON_NASTY:
        case AF_POISON_STR:
        case AF_POISON_DEX:
        case AF_POISON_INT:
        case AF_POISON_STAT:
          monsterattacks += colour(LIGHTRED, "(?\?\?)");
          break;
#endif
// let the compiler issue warnings for us
//      default:
//        monsterattacks += "(???)";
//        break;
        }

        if (x == 0 && mon.has_hydra_multi_attack())
          monsterattacks += " per head";
      }
    }

    printf("%s", monsterattacks.c_str());

    switch (me->holiness)
    {
    case MH_HOLY:
      mons_flag(monsterflags, colour(YELLOW, "holy"));
      break;
    case MH_UNDEAD:
      mons_flag(monsterflags, colour(BROWN, "undead"));
      break;
    case MH_DEMONIC:
      mons_flag(monsterflags, colour(RED, "demonic"));
      break;
    case MH_NONLIVING:
      mons_flag(monsterflags, colour(LIGHTCYAN, "non-living"));
      break;
    case MH_PLANT:
      mons_flag(monsterflags, colour(GREEN, "plant"));
      break;
    case MH_NATURAL:
    default:
      break;
    }

    switch (me->gmon_use)
    {
      case MONUSE_WEAPONS_ARMOUR:
        mons_flag(monsterflags, colour(CYAN, "weapons"));
      // intentional fall-through
      case MONUSE_STARTING_EQUIPMENT:
        mons_flag(monsterflags, colour(CYAN, "items"));
      // intentional fall-through
      case MONUSE_OPEN_DOORS:
        mons_flag(monsterflags, colour(CYAN, "doors"));
      // intentional fall-through
      case MONUSE_NOTHING:
        break;

      case NUM_MONUSE:  // Can't happen
        mons_flag(monsterflags, colour(CYAN, "uses bugs"));
        break;
    }

    mons_check_flag(bool(me->bitfields & M_EAT_ITEMS), monsterflags, colour(LIGHTRED, "eats items"));
    mons_check_flag(bool(me->bitfields & M_CRASH_DOORS), monsterflags, colour(LIGHTRED, "breaks doors"));

    mons_check_flag(mons_wields_two_weapons(&mon), monsterflags, "two-weapon");
    mons_check_flag(mon.is_fighter(), monsterflags, "fighter");
    if (mon.is_archer())
    {
      if (me->bitfields & M_DONT_MELEE)
        mons_flag(monsterflags, "master archer");
      else
        mons_flag(monsterflags, "archer");
    }
    mons_check_flag(mon.is_priest(), monsterflags, "priest");

    mons_check_flag(me->habitat == HT_AMPHIBIOUS,
                    monsterflags, "amphibious");

    mons_check_flag(mon.is_evil(), monsterflags, "evil");
    mons_check_flag(mon.is_actual_spellcaster(),
                    monsterflags, "spellcaster");
    mons_check_flag(bool(me->bitfields & M_COLD_BLOOD), monsterflags, "cold-blooded");
    mons_check_flag(bool(me->bitfields & M_SEE_INVIS), monsterflags, "see invisible");
    mons_check_flag(bool(me->bitfields & M_FLIES), monsterflags, "fly");
    mons_check_flag(bool(me->bitfields & M_FAST_REGEN), monsterflags, "regen");
    mons_check_flag(bool(me->bitfields & M_WEB_SENSE), monsterflags, "web sense");
    mons_check_flag(mon.is_unbreathing(), monsterflags, "unbreathing");

    std::string spell_string = construct_spells(spells, damages);
    if (shapeshifter
        || mon.type == MONS_PANDEMONIUM_LORD
        || mon.type == MONS_LICH
        || mon.type == MONS_ANCIENT_LICH
        || mon.type == MONS_CHIMERA
           && (mon.base_monster == MONS_PANDEMONIUM_LORD
               || mon.base_monster == MONS_LICH
               || mon.base_monster == MONS_ANCIENT_LICH))
    {
      spell_string = "(random)";
    }

    mons_check_flag(vault_monster, monsterflags, colour(BROWN, "vault"));

    printf("%s", monsterflags.c_str());

    if (me->resist_magic == 5000)
    {
      if (monsterresistances.empty())
        monsterresistances = " | Res: ";
      else
        monsterresistances += ", ";
      monsterresistances += colour(LIGHTMAGENTA, "magic(immune)");
    }
    else if (me->resist_magic < 0)
    {
      const int res = (mbase) ? mbase->resist_magic : me->resist_magic;
      if (monsterresistances.empty())
        monsterresistances = " | Res: ";
      else
        monsterresistances += ", ";
      monsterresistances += colour(MAGENTA, std::string() + "magic("
                                   + to_string((short int) hd * res * 4 / 3 * -1)
                                   + ")");
    }
    else if (me->resist_magic > 0)
    {
      if (monsterresistances.empty())
        monsterresistances = " | Res: ";
      else
        monsterresistances += ", ";
      monsterresistances += colour(MAGENTA, std::string("magic(")
                                   + to_string((short int) me->resist_magic)
                                   + ")");
    }

    const resists_t res(
      shapeshifter? me->resists : get_mons_resists(&mon));
#define res(c,x)                                  \
    do                                            \
    {                                             \
      record_resist(c,lowercase_string(#x),       \
                    monsterresistances,           \
                    monstervulnerabilities,       \
                    get_resist(res, MR_RES_##x)); \
    } while (false)                               \

#define res2(c,x,y)                               \
    do                                            \
    {                                             \
      record_resist(c,#x,                         \
                    monsterresistances,           \
                    monstervulnerabilities,       \
                    y);                           \
    } while (false)                               \


    // Don't record regular rF as hellfire vulnerability.
    int rfire = get_resist(res, MR_RES_FIRE);
    bool rhellfire = rfire >= 4;
    if (rfire > 3)
      rfire = 3;
    res2(RED, hellfire, (int)rhellfire);
    res2(RED, fire, rfire);
    res(BLUE, COLD);
    res(CYAN, ELEC);
    res(GREEN, POISON);
    res(BROWN, ACID);
    res(0, STEAM);

    if (me->bitfields & M_UNBLINDABLE)
      res2(YELLOW, blind, 1);

    res2(LIGHTBLUE,    drown,  mon.res_water_drowning());
    res2(LIGHTRED,     rot,    mon.res_rotting());
    res2(LIGHTMAGENTA, neg,    mon.res_negative_energy(true));
    res2(YELLOW,       holy,   mon.res_holy_energy(&you));
    res2(LIGHTMAGENTA, torm,   mon.res_torment());
    res2(LIGHTBLUE,    wind,   mon.res_wind());
    res2(LIGHTRED,     napalm, mon.res_sticky_flame());
    res2(LIGHTCYAN,    silver, mon.how_chaotic() ? -1 : 0);

    printf("%s", monsterresistances.c_str());
    printf("%s", monstervulnerabilities.c_str());

    if (me->corpse_thingy != CE_NOCORPSE && me->corpse_thingy != CE_CLEAN)
    {
      printf(" | Chunks: ");
      switch (me->corpse_thingy)
      {
      case CE_NOXIOUS:
        printf("%s", colour(DARKGREY,"noxious").c_str());
        break;
      case CE_MUTAGEN:
        printf("%s", colour(MAGENTA, "mutagenic").c_str());
        break;
      // We should't get here; including these values so we can get compiler
      // warnings for unhandled enum values.
      case CE_NOCORPSE:
      case CE_CLEAN:
        printf("???");
      }
    }

    printf(" | XP: %ld", exper);

    if (!spell_string.empty())
      printf(" | Sp: %s", spell_string.c_str());

    printf(" | Sz: %s",
           monster_size(mon).c_str());

    printf(" | Int: %s",
           monster_int(mon).c_str());

    printf(".\n");

    return 0;
  }
  return 1;
}

template <class T> inline std::string to_string (const T& t)
{
  std::stringstream ss;
  ss << t;
  return ss.str();
}

//////////////////////////////////////////////////////////////////////////
// acr.cc stuff

CLua clua(true);
CLua dlua(false);      // Lua interpreter for the dungeon builder.
crawl_environment env; // Requires dlua.
player you;
game_state crawl_state;

FILE *yyin;
int yylineno;

std::string init_file_error;    // externed in newgame.cc

int stealth;                    // externed in view.cc
bool apply_berserk_penalty;     // externed in evoke.cc

void process_command(command_type) {
}

int yyparse() {
  return 0;
}

void world_reacts() {
}
