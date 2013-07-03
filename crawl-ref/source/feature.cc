#include "AppHdr.h"

#include "feature.h"

#include "colour.h"
#include "options.h"
#include "show.h"

typedef map<show_type, feature_def> feat_map;
static feat_map Features;

const feature_def &get_feature_def(show_type object)
{
    // If this is a monster that is hidden explicitly, show items if
    // any instead, or the base feature if there are no items.
    if (object.cls == SH_MONSTER)
        object.cls = (object.item != SHOW_ITEM_NONE)? SH_ITEM : SH_FEATURE;
    return Features[object];
}

const feature_def &get_feature_def(dungeon_feature_type feat)
{
    ASSERT(feat < NUM_FEATURES);
    show_type object;
    object.cls = SH_FEATURE;
    object.feat = feat;
    return Features[object];
}

static void _apply_feature_overrides()
{
    for (int i = 0, size = Options.feature_overrides.size(); i < size; ++i)
    {
        const feature_override      &fov    = Options.feature_overrides[i];
        const feature_def           &ofeat  = fov.override;
        feature_def                 &feat   = Features[fov.object];
        ucs_t c;

        if (ofeat.symbol && (c = get_glyph_override(ofeat.symbol)))
            feat.symbol = c;
        if (ofeat.magic_symbol && (c = get_glyph_override(ofeat.magic_symbol)))
            feat.magic_symbol = c;
        if (ofeat.colour)
            feat.colour = ofeat.colour;
        if (ofeat.map_colour)
            feat.map_colour = ofeat.map_colour;
        if (ofeat.seen_colour)
            feat.seen_colour = ofeat.seen_colour;
        if (ofeat.seen_em_colour)
            feat.seen_em_colour = ofeat.seen_em_colour;
        if (ofeat.em_colour)
            feat.em_colour = ofeat.em_colour;
    }
}

static void _init_feat(feature_def &f, dungeon_feature_type feat)
{
    switch (feat)
    {
        case DNGN_UNSEEN:
        case DNGN_EXPLORE_HORIZON:
        default:
            break;

        case DNGN_ROCK_WALL:
        case DNGN_PERMAROCK_WALL:
            f.dchar        = DCHAR_WALL;
            f.colour       = ETC_ROCK;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.minimap      = MF_WALL;
            break;

        case DNGN_STONE_WALL:
            f.dchar        = DCHAR_WALL;
            f.colour       = LIGHTGRAY;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.minimap      = MF_WALL;
            break;

        case DNGN_SLIMY_WALL:
            f.dchar        = DCHAR_WALL;
            f.colour       = LIGHTGREEN;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.minimap      = MF_WALL;
            break;

        case DNGN_CLEAR_ROCK_WALL:
        case DNGN_CLEAR_STONE_WALL:
        case DNGN_CLEAR_PERMAROCK_WALL:
            f.dchar        = DCHAR_WALL;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.colour       = LIGHTCYAN;
            f.minimap      = MF_WALL;
            break;

        case DNGN_GRATE:
            f.dchar        = DCHAR_GRATE;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.colour       = LIGHTBLUE;
            f.minimap      = MF_WALL;
            break;

        case DNGN_TREE:
            f.dchar        = DCHAR_TREE;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.colour       = ETC_TREE;
            f.minimap      = MF_WALL;
            break;

        case DNGN_MANGROVE:
            f.dchar        = DCHAR_TREE;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.colour       = ETC_MANGROVE;
            f.minimap      = MF_WALL;
            break;

        case DNGN_OPEN_SEA:
            f.dchar        = DCHAR_WALL;
            f.colour       = BLUE;
            f.minimap      = MF_WATER;
            break;

        case DNGN_LAVA_SEA:
            f.dchar        = DCHAR_WAVY;
            f.colour       = RED;
            f.minimap      = MF_LAVA;
            break;

        case DNGN_OPEN_DOOR:
            f.dchar   = DCHAR_DOOR_OPEN;
            f.colour  = LIGHTGREY;
            f.minimap = MF_DOOR;
            break;

        case DNGN_CLOSED_DOOR:
            f.dchar   = DCHAR_DOOR_CLOSED;
            f.colour  = LIGHTGREY;
            f.minimap = MF_DOOR;
            break;

        case DNGN_RUNED_DOOR:
            f.dchar   = DCHAR_DOOR_CLOSED;
            f.colour  = LIGHTBLUE;
            f.minimap = MF_DOOR;
            f.map_colour = LIGHTBLUE;
            break;

        case DNGN_SEALED_DOOR:
            f.dchar   = DCHAR_DOOR_CLOSED;
            f.colour  = LIGHTGREEN;
            f.minimap = MF_DOOR;
            f.map_colour = LIGHTGREEN;
            break;

        case DNGN_METAL_WALL:
            f.dchar        = DCHAR_WALL;
            f.colour       = CYAN;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.minimap      = MF_WALL;
            break;

        case DNGN_GREEN_CRYSTAL_WALL:
            f.dchar        = DCHAR_WALL;
            f.colour       = GREEN;
            f.magic_symbol = Options.char_table[ DCHAR_WALL_MAGIC ];
            f.minimap      = MF_WALL;
            break;

        case DNGN_ORCISH_IDOL:
            f.dchar   = DCHAR_STATUE;
            f.colour  = BROWN; // same as clay golem, I hope that's okay
            f.minimap = MF_WALL;
            break;

        case DNGN_GRANITE_STATUE:
            f.dchar   = DCHAR_STATUE;
            f.colour  = DARKGREY;
            f.minimap = MF_WALL;
            break;

        case DNGN_LAVA:
            f.dchar   = DCHAR_WAVY;
            f.colour  = RED;
            f.minimap = MF_LAVA;
            break;

        case DNGN_DEEP_WATER:
            f.dchar   = DCHAR_WAVY;
            f.colour  = BLUE;
            f.minimap = MF_WATER;
            break;

        case DNGN_SHALLOW_WATER:
            f.dchar   = DCHAR_WAVY;
            f.colour  = CYAN;
            f.minimap = MF_WATER;
            break;

        case DNGN_FLOOR:
            f.dchar        = DCHAR_FLOOR;
            f.colour       = ETC_FLOOR;
            f.magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            f.minimap      = MF_FLOOR;
            break;

        case DNGN_EXIT_HELL:
            f.dchar       = DCHAR_ARCH;
            f.colour      = LIGHTRED;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = LIGHTRED;
            f.minimap     = MF_STAIR_UP;
            break;

        case DNGN_ENTER_HELL:
            f.dchar       = DCHAR_ARCH;
            f.colour      = RED;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = RED;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_TELEPORTER:
            f.dchar       = DCHAR_TELEPORTER;
            f.colour      = YELLOW;
            f.map_colour  = YELLOW;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_TRAP_MECHANICAL:
            f.colour     = LIGHTCYAN;
            f.dchar      = DCHAR_TRAP;
            f.map_colour = LIGHTCYAN;
            f.minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_MAGICAL:
            f.colour     = MAGENTA;
            f.dchar      = DCHAR_TRAP;
            f.map_colour = MAGENTA;
            f.minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_NATURAL:
            f.colour     = BROWN;
            f.dchar      = DCHAR_TRAP;
            f.map_colour = BROWN;
            f.minimap    = MF_TRAP;
            break;

        case DNGN_TRAP_WEB:
            f.colour     = LIGHTGREY;
            f.dchar      = DCHAR_TRAP;
            f.map_colour = LIGHTGREY;
            f.minimap    = MF_TRAP;
            break;

        case DNGN_UNDISCOVERED_TRAP:
            f.dchar        = DCHAR_FLOOR;
            f.colour       = ETC_FLOOR;
            f.magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            f.minimap      = MF_FLOOR;
            break;

        case DNGN_ENTER_SHOP:
            f.dchar       = DCHAR_ARCH;
            f.colour      = YELLOW;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = YELLOW;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ABANDONED_SHOP:
            f.colour     = LIGHTGREY;
            f.dchar      = DCHAR_ARCH;
            f.map_colour = LIGHTGREY;
            f.minimap    = MF_FLOOR;
            break;

        case DNGN_ENTER_LABYRINTH:
            f.dchar       = DCHAR_ARCH;
            f.colour      = CYAN;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = CYAN;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_PORTAL_VAULT:
            f.flags |= FFT_NOTABLE;
            // fall through

        case DNGN_EXIT_PORTAL_VAULT:
            f.dchar       = DCHAR_ARCH;
            f.colour      = ETC_SHIMMER_BLUE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = ETC_SHIMMER_BLUE;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_MALIGN_GATEWAY:
            f.dchar       = DCHAR_ARCH;
            f.colour      = ETC_SHIMMER_BLUE;
            f.map_colour  = LIGHTGREY;
            f.colour      = ETC_SHIMMER_BLUE;
            f.minimap     = MF_STAIR_UP;
            break;

        case DNGN_EXPIRED_PORTAL:
            f.dchar        = DCHAR_FLOOR;
            f.colour       = BROWN;
            f.magic_symbol = Options.char_table[ DCHAR_FLOOR_MAGIC ];
            f.minimap      = MF_FLOOR;
            break;


        case DNGN_ESCAPE_HATCH_DOWN:
            f.dchar      = DCHAR_STAIRS_DOWN;
            f.colour     = BROWN;
            f.map_colour = BROWN;
            f.minimap    = MF_STAIR_DOWN;
            break;

        case DNGN_STONE_STAIRS_DOWN_I:
        case DNGN_STONE_STAIRS_DOWN_II:
        case DNGN_STONE_STAIRS_DOWN_III:
            f.dchar          = DCHAR_STAIRS_DOWN;
            f.colour         = RED;
            f.em_colour      = WHITE;
            f.map_colour     = RED;
            f.seen_em_colour = WHITE;
            f.minimap        = MF_STAIR_DOWN;
            break;

        case DNGN_ESCAPE_HATCH_UP:
            f.dchar      = DCHAR_STAIRS_UP;
            f.colour     = BROWN;
            f.map_colour = BROWN;
            f.minimap    = MF_STAIR_UP;
            break;

        case DNGN_STONE_STAIRS_UP_I:
        case DNGN_STONE_STAIRS_UP_II:
        case DNGN_STONE_STAIRS_UP_III:
            f.dchar          = DCHAR_STAIRS_UP;
            f.colour         = GREEN;
            f.map_colour     = GREEN;
            f.em_colour      = WHITE;
            f.seen_em_colour = WHITE;
            f.minimap        = MF_STAIR_UP;
            break;

        case DNGN_ENTER_DIS:
            f.colour      = CYAN;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = CYAN;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_GEHENNA:
            f.colour      = RED;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = RED;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_COCYTUS:
            f.colour      = LIGHTCYAN;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = LIGHTCYAN;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_TARTARUS:
            f.colour      = DARKGREY;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = DARKGREY;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ABYSS:
        case DNGN_EXIT_THROUGH_ABYSS:
            f.colour      = ETC_RANDOM;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = ETC_RANDOM;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_ABYSS:
            f.colour     = ETC_RANDOM;
            f.dchar      = DCHAR_ARCH;
            f.map_colour = ETC_RANDOM;
            f.minimap    = MF_STAIR_BRANCH;
            break;

        case DNGN_ABYSSAL_STAIR:
            f.colour     = LIGHTCYAN;
            f.dchar      = DCHAR_STAIRS_DOWN;
            f.map_colour = LIGHTCYAN;
            f.minimap    = MF_STAIR_BRANCH;
            break;

        case DNGN_STONE_ARCH:
            f.colour     = LIGHTGREY;
            f.dchar      = DCHAR_ARCH;
            f.map_colour = LIGHTGREY;
            f.minimap    = MF_FLOOR;
            break;

        case DNGN_ENTER_PANDEMONIUM:
            f.colour      = LIGHTBLUE;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = LIGHTBLUE;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_PANDEMONIUM:
            f.colour      = LIGHTBLUE;
            f.dchar       = DCHAR_ARCH;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = LIGHTBLUE;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_TRANSIT_PANDEMONIUM:
            f.colour      = LIGHTGREEN;
            f.dchar       = DCHAR_ARCH;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = LIGHTGREEN;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_DWARVEN_HALL:
        case DNGN_ENTER_ORCISH_MINES:
        case DNGN_ENTER_LAIR:
        case DNGN_ENTER_SLIME_PITS:
        case DNGN_ENTER_VAULTS:
        case DNGN_ENTER_CRYPT:
        case DNGN_ENTER_HALL_OF_BLADES:
        case DNGN_ENTER_TEMPLE:
        case DNGN_ENTER_SNAKE_PIT:
        case DNGN_ENTER_ELVEN_HALLS:
        case DNGN_ENTER_TOMB:
        case DNGN_ENTER_SWAMP:
        case DNGN_ENTER_SHOALS:
        case DNGN_ENTER_SPIDER_NEST:
        case DNGN_ENTER_FOREST:
            f.colour      = YELLOW;
            f.dchar       = DCHAR_STAIRS_DOWN;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = RED;
            f.seen_colour = YELLOW;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ENTER_ZOT:
            f.colour      = MAGENTA;
            f.dchar       = DCHAR_ARCH;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = MAGENTA;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_EXIT_DUNGEON:
            f.colour      = LIGHTBLUE;
            f.dchar       = DCHAR_STAIRS_UP;
            f.map_colour  = GREEN;
            f.seen_colour = LIGHTBLUE;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_RETURN_FROM_DWARVEN_HALL:
        case DNGN_RETURN_FROM_ORCISH_MINES:
        case DNGN_RETURN_FROM_LAIR:
        case DNGN_RETURN_FROM_SLIME_PITS:
        case DNGN_RETURN_FROM_VAULTS:
        case DNGN_RETURN_FROM_CRYPT:
        case DNGN_RETURN_FROM_HALL_OF_BLADES:
        case DNGN_RETURN_FROM_TEMPLE:
        case DNGN_RETURN_FROM_SNAKE_PIT:
        case DNGN_RETURN_FROM_ELVEN_HALLS:
        case DNGN_RETURN_FROM_TOMB:
        case DNGN_RETURN_FROM_SWAMP:
        case DNGN_RETURN_FROM_SHOALS:
        case DNGN_RETURN_FROM_SPIDER_NEST:
        case DNGN_RETURN_FROM_FOREST:
            f.colour      = YELLOW;
            f.dchar       = DCHAR_STAIRS_UP;
            f.map_colour  = GREEN;
            f.seen_colour = YELLOW;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_RETURN_FROM_ZOT:
            f.colour      = MAGENTA;
            f.dchar       = DCHAR_ARCH;
            f.map_colour  = LIGHTGREY;
            f.seen_colour = MAGENTA;
            f.minimap     = MF_STAIR_BRANCH;
            break;

        case DNGN_ALTAR_ZIN:
            f.colour      = LIGHTGREY;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = LIGHTGREY;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_SHINING_ONE:
            f.colour      = YELLOW;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = YELLOW;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_KIKUBAAQUDGHA:
            f.colour      = DARKGREY;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = DARKGREY;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_YREDELEMNUL:
            f.colour      = ETC_UNHOLY;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = ETC_UNHOLY;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_XOM:
            f.colour      = ETC_RANDOM;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = ETC_RANDOM;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_VEHUMET:
            f.colour      = ETC_VEHUMET;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = ETC_VEHUMET;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_OKAWARU:
            f.colour      = CYAN;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = CYAN;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_MAKHLEB:
            f.colour      = ETC_FIRE;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = ETC_FIRE;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_SIF_MUNA:
            f.colour      = BLUE;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = BLUE;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_TROG:
            f.colour      = RED;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = RED;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_NEMELEX_XOBEH:
            f.colour      = LIGHTMAGENTA;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = LIGHTMAGENTA;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_ELYVILON:
            f.colour      = WHITE;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = WHITE;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_LUGONU:
            f.colour      = MAGENTA;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = MAGENTA;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_BEOGH:
            f.colour      = ETC_BEOGH;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = ETC_BEOGH;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_JIYVA:
            f.colour      = ETC_SLIME;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = ETC_SLIME;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_FEDHAS:
            f.colour      = GREEN;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = GREEN;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_CHEIBRIADOS:
            f.colour      = LIGHTCYAN;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = LIGHTCYAN;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_ALTAR_ASHENZARI:
            f.colour      = LIGHTRED;
            f.dchar       = DCHAR_ALTAR;
            f.flags      |= FFT_NOTABLE;
            f.map_colour  = DARKGREY;
            f.seen_colour = LIGHTRED;
            f.minimap     = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_BLUE:
            f.colour  = BLUE;
            f.dchar   = DCHAR_FOUNTAIN;
            f.minimap = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_SPARKLING:
            f.colour  = LIGHTBLUE;
            f.dchar   = DCHAR_FOUNTAIN;
            f.minimap = MF_FEATURE;
            break;

        case DNGN_FOUNTAIN_BLOOD:
            f.colour  = RED;
            f.dchar   = DCHAR_FOUNTAIN;
            f.minimap = MF_FEATURE;
            break;

        case DNGN_DRY_FOUNTAIN_BLUE:
        case DNGN_DRY_FOUNTAIN_SPARKLING:
        case DNGN_DRY_FOUNTAIN_BLOOD:
        case DNGN_PERMADRY_FOUNTAIN:
            f.colour  = LIGHTGREY;
            f.dchar   = DCHAR_FOUNTAIN;
            f.minimap = MF_FEATURE;
            break;

        case DNGN_UNKNOWN_ALTAR:
            f.colour  = LIGHTGREY;
            f.dchar   = DCHAR_ALTAR;
            f.minimap = MF_FEATURE;
            break;

        case DNGN_UNKNOWN_PORTAL:
            f.colour  = LIGHTGREY;
            f.dchar   = DCHAR_ARCH;
            f.minimap = MF_FEATURE;
            break;
    }

    if (feat == DNGN_ENTER_ORCISH_MINES || feat == DNGN_ENTER_SLIME_PITS
        || feat == DNGN_ENTER_LABYRINTH)
    {
        f.flags |= FFT_EXAMINE_HINT;
    }
}

static void _init_item(feature_def &f, show_item_type item)
{
    f.minimap = MF_ITEM;
    switch (item)
    {
        case SHOW_ITEM_DETECTED:
            f.dchar   = DCHAR_ITEM_DETECTED;
            break;

        case SHOW_ITEM_ORB:
            f.dchar   = DCHAR_ITEM_ORB;
            break;

        case SHOW_ITEM_RUNE:
            f.dchar   = DCHAR_ITEM_RUNE;
            break;

        case SHOW_ITEM_WEAPON:
            f.dchar   = DCHAR_ITEM_WEAPON;
            break;

        case SHOW_ITEM_ARMOUR:
            f.dchar   = DCHAR_ITEM_ARMOUR;
            break;

        case SHOW_ITEM_WAND:
            f.dchar   = DCHAR_ITEM_WAND;
            break;

        case SHOW_ITEM_FOOD:
            f.dchar   = DCHAR_ITEM_FOOD;
            break;

        case SHOW_ITEM_SCROLL:
            f.dchar   = DCHAR_ITEM_SCROLL;
            break;

        case SHOW_ITEM_RING:
            f.dchar   = DCHAR_ITEM_RING;
            break;

        case SHOW_ITEM_POTION:
            f.dchar   = DCHAR_ITEM_POTION;
            break;

        case SHOW_ITEM_MISSILE:
            f.dchar   = DCHAR_ITEM_MISSILE;
            break;

        case SHOW_ITEM_BOOK:
            f.dchar   = DCHAR_ITEM_BOOK;
            break;

        case SHOW_ITEM_STAVE:
            f.dchar   = DCHAR_ITEM_STAVE;
            break;

        case SHOW_ITEM_MISCELLANY:
            f.dchar   = DCHAR_ITEM_MISCELLANY;
            break;

        case SHOW_ITEM_CORPSE:
            f.dchar   = DCHAR_ITEM_CORPSE;
            break;

        case SHOW_ITEM_GOLD:
            f.dchar   = DCHAR_ITEM_GOLD;
            break;

        case SHOW_ITEM_AMULET:
            f.dchar   = DCHAR_ITEM_AMULET;
            break;

        default:
            break;
    }
}

void init_show_table(void)
{
    show_type obj;
    for (int i = 0; i < NUM_FEATURES; i++)
    {
        obj.cls = SH_FEATURE;
        obj.feat = static_cast<dungeon_feature_type>(i);

        _init_feat(Features[obj], obj.feat);
    }

    obj.cls = SH_INVIS_EXPOSED;
    Features[obj].dchar   = DCHAR_INVIS_EXPOSED;
    Features[obj].minimap = MF_MONS_HOSTILE;

    for (int i = 0; i < NUM_SHOW_ITEMS; i++)
    {
        obj.cls = SH_ITEM;
        obj.item = static_cast<show_item_type>(i);

        _init_item(Features[obj], obj.item);
    }

    obj.cls = SH_CLOUD;
    Features[obj].dchar   = DCHAR_CLOUD;
    Features[obj].minimap = MF_SKIP;

    for (feat_map::iterator i = Features.begin(); i != Features.end(); ++i)
    {
        feature_def &f = i->second;
        if (f.dchar != NUM_DCHAR_TYPES)
            f.symbol = Options.char_table[f.dchar];
    }

    _apply_feature_overrides();

    for (feat_map::iterator i = Features.begin(); i != Features.end(); ++i)
    {
        feature_def &f = i->second;
        if (!f.magic_symbol)
            f.magic_symbol = f.symbol;

        if (f.seen_colour == BLACK)
            f.seen_colour = f.map_colour;

        if (f.seen_em_colour == BLACK)
            f.seen_em_colour = f.seen_colour;

        if (f.em_colour == BLACK)
            f.em_colour = f.colour;
    }
}

dungeon_feature_type magic_map_base_feat(dungeon_feature_type feat)
{
    const feature_def& fdef = get_feature_def(feat);
    switch (fdef.dchar)
    {
    case DCHAR_STATUE:
        return DNGN_GRANITE_STATUE;
    case DCHAR_FLOOR:
    case DCHAR_TRAP:
        return DNGN_FLOOR;
    case DCHAR_WAVY:
        return DNGN_SHALLOW_WATER;
    case DCHAR_ARCH:
        return DNGN_UNKNOWN_PORTAL;
    case DCHAR_FOUNTAIN:
        return DNGN_FOUNTAIN_BLUE;
    case DCHAR_WALL:
        return DNGN_ROCK_WALL;
    case DCHAR_ALTAR:
        return DNGN_UNKNOWN_ALTAR;
    default:
        // We could do more, e.g. map the different upstairs together.
        return feat;
    }
}
