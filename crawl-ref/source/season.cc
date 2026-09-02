#include "AppHdr.h"

#include "season.h"

#include "options.h"

/**
* Halloween or Hallowe'en (/ˌhæləˈwiːn, -oʊˈiːn, ˌhɑːl-/; a contraction of
* "All Hallows' Evening"),[6] also known as Allhalloween,[7] All Hallows' Eve,
* [8] or All Saints' Eve,[9] is a yearly celebration observed in a number of
* countries on 31 October, the eve of the Western Christian feast of All
* Hallows' Day... Within Allhallowtide, the traditional focus of All Hallows'
* Eve revolves around the theme of using "humor and ridicule to confront the
* power of death."[12]
*
* Typical festive Halloween activities include trick-or-treating (or the
* related "guising"), attending costume parties, decorating, carving pumpkins
* into jack-o'-lanterns, lighting bonfires, apple bobbing, visiting haunted
* house attractions, playing pranks, telling scary stories, and watching
* horror films.
*
* @return  Whether to use the Halloween in game effects.
*/
bool is_halloween()
{
    holiday_state holi = Options.holiday;

    if (holi == holiday_state::halloween)
        return true;

    const time_t curr_time = time(nullptr);
    const struct tm *date = TIME_FN(&curr_time);
    // tm_mon is zero-based in case you are wondering
    if (date->tm_mon == 9)
    {
        switch (holi)
        {
        default:
            return false;
        case holiday_state::month:
            return true;
        case holiday_state::day:
            return date->tm_mday == 31;
        case holiday_state::week:
            return date->tm_mday >= 25;
        }
    }

    return false;
}

/**
* @return  Whether to use the Christmas in game effects.
*/
bool is_christmas()
{
    holiday_state holi = Options.holiday;

    if (holi == holiday_state::christmas)
        return true;

    const time_t curr_time = time(nullptr);
    const struct tm *date = TIME_FN(&curr_time);
    // tm_mon is zero-based in case you are wondering
    if (date->tm_mon == 11)
    {
        switch (holi)
        {
        default:
            return false;
        case holiday_state::month:
            return true;
        case holiday_state::day:
            return date->tm_mday == 25;
        case holiday_state::week:
            return date->tm_mday >= 18 && date->tm_mday <= 25;
        }
    }

    return false;
}

/**
* @return  Whether to use the April Fools in game effects.
*/
bool is_april_fools()
{
    holiday_state holi = Options.holiday;

    if (holi == holiday_state::aprilfools)
        return true;

    const time_t curr_time = time(nullptr);
    const struct tm *date = TIME_FN(&curr_time);
    // tm_mon is zero-based in case you are wondering
    if (date->tm_mon == 4)
    {
        switch (holi)
        {
        default:
            return false;
        case holiday_state::month:
            return true;
        case holiday_state::day:
            return date->tm_mday == 1;
        case holiday_state::week:
            return date->tm_mday <= 7;
        }
    }

    return false;
}
