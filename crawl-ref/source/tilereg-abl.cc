#include "AppHdr.h"

#ifdef USE_TILE_LOCAL

#include "tilereg-abl.h"

#include "ability.h"
#include "cio.h"
#include "libutil.h"
#include "macro.h"
#include "message.h"
#include "output.h"
#include "process_desc.h"
#include "tiledef-dngn.h"
#include "tiledef-icons.h"
#include "tilepick.h"
#include "viewgeom.h"

AbilityRegion::AbilityRegion(const TileRegionInit &init) : GridRegion(init)
{
}

void AbilityRegion::activate()
{
    if (your_talents(false, true).size() == 0)
    {
        no_ability_msg();
        flush_prev_message();
    }
}

void AbilityRegion::draw_tag()
{
    if (m_cursor == NO_CURSOR)
        return;

    int curs_index = cursor_index();
    if (curs_index >= (int)m_items.size())
        return;
    int idx = m_items[curs_index].idx;
    if (idx == -1)
        return;

    const ability_type ability = (ability_type) idx;
    const string failure = failure_rate_to_string(get_talent(ability,
                                                             false).fail);
    string desc = make_stringf("%s    (%s)",
                               ability_name(ability), failure.c_str());
    draw_desc(desc.c_str());
}

int AbilityRegion::handle_mouse(MouseEvent &event)
{
    unsigned int item_idx;
    if (!place_cursor(event, item_idx))
        return 0;

    const ability_type ability = (ability_type) m_items[item_idx].idx;
    if (event.button == MouseEvent::LEFT)
    {
        // close tab again if using small layout
        if (tiles.is_using_small_layout())
            tiles.deactivate_tab();

        m_last_clicked_item = item_idx;
        tiles.set_need_redraw();
        talent tal = get_talent(ability, true);
        if (tal.which == ABIL_NON_ABILITY || !activate_talent(tal))
            flush_input_buffer(FLUSH_ON_FAILURE);
        return CK_MOUSE_CMD;
    }
    else if (ability != NUM_ABILITIES && event.button == MouseEvent::RIGHT)
    {
        describe_talent(get_talent(ability, false));
        redraw_screen();
        return CK_MOUSE_CMD;
    }
    return 0;
}

bool AbilityRegion::update_tab_tip_text(string &tip, bool active)
{
    const char *prefix1 = active ? "" : "[L-Click] ";
    const char *prefix2 = active ? "" : "          ";

    tip = make_stringf("%s%s\n%s%s",
                       prefix1, "Display abilities",
                       prefix2, "Use abilities");

    return true;
}

bool AbilityRegion::update_tip_text(string& tip)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    int flag = m_items[item_idx].flag;
    vector<command_type> cmd;
    if (flag & TILEI_FLAG_INVALID)
        tip = "You cannot use this ability right now.";
    else
    {
        tip = "[L-Click] Use (%)";
        cmd.push_back(CMD_USE_ABILITY);
    }

    // TODO: command to display abilities outside of use
#if 0
    tip += "\n[R-Click] Describe (%)";
    cmd.push_back(CMD_DISPLAY_SPELLS);
    insert_commands(tip, cmd);
#endif

    return true;
}

bool AbilityRegion::update_alt_text(string &alt)
{
    if (m_cursor == NO_CURSOR)
        return false;

    unsigned int item_idx = cursor_index();
    if (item_idx >= m_items.size() || m_items[item_idx].empty())
        return false;

    if (m_last_clicked_item >= 0
        && item_idx == (unsigned int) m_last_clicked_item)
    {
        return false;
    }

    int idx = m_items[item_idx].idx;

    const ability_type ability = (ability_type) idx;

    describe_info inf;
    inf.body << get_ability_desc(ability);

    alt_desc_proc proc(crawl_view.msgsz.x, crawl_view.msgsz.y);
    process_description<alt_desc_proc>(proc, inf);

    proc.get_string(alt);

    return true;
}

int AbilityRegion::get_max_slots()
{
    return ABIL_MAX_INTRINSIC + (ABIL_MAX_EVOKE - ABIL_MIN_EVOKE) + 1
           + MAX_GOD_ABILITIES + 1
           + (ABIL_MAX_ZOTDEF - ABIL_MIN_ZOTDEF) + 1;
}

void AbilityRegion::pack_buffers()
{
    if (m_items.size() == 0)
        return;

    const int max_spells = get_max_slots();

    // Pack base separately, as it comes from a different texture...
    int i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= max_spells)
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i++ >= max_spells)
                break;

            m_buf.add_dngn_tile(TILE_ITEM_SLOT, x, y);
        }
    }

    i = 0;
    for (int y = 0; y < my; y++)
    {
        if (i >= (int)m_items.size())
            break;

        for (int x = 0; x < mx; x++)
        {
            if (i >= (int)m_items.size())
                break;

            InventoryTile &item = m_items[i++];
            if (item.flag & TILEI_FLAG_INVALID)
                m_buf.add_icons_tile(TILEI_MESH, x, y);

            if (item.flag & TILEI_FLAG_CURSOR)
                m_buf.add_icons_tile(TILEI_CURSOR, x, y);

            if (item.quantity > 0) // mp cost
                draw_number(x, y, item.quantity);

            if (item.tile)
                m_buf.add_spell_tile(item.tile, x, y);
        }
    }
}

static InventoryTile _tile_for_ability(ability_type ability)
{
    InventoryTile desc;
    desc.tile     = tileidx_ability(ability);
    desc.idx      = (int) ability;
    desc.quantity = get_ability_def(ability).mp_cost;

    if (!check_ability_possible(ability, true, true))
        desc.flag |= TILEI_FLAG_INVALID;

    return desc;
}

void AbilityRegion::update()
{
    m_items.clear();
    m_dirty = true;

    if (mx * my == 0)
        return;

    const unsigned int max_abilities = min(get_max_slots(), mx*my);

    vector<talent> talents = your_talents(false, true);
    if (talents.empty())
        return;

    vector<InventoryTile> m_zotdef;
    vector<InventoryTile> m_invoc;

    for (const auto &talent : talents)
    {
        if (talent.is_invocation)
            m_invoc.push_back(_tile_for_ability(talent.which));
        else if (talent.is_zotdef)
            m_zotdef.push_back(_tile_for_ability(talent.which));
        else
            m_items.push_back(_tile_for_ability(talent.which));

        if (m_items.size() >= max_abilities)
            return;
    }

    for (const auto &tile : m_zotdef)
    {
        m_items.push_back(tile);
        if (m_items.size() >= max_abilities)
            return;
    }

    for (const auto &tile : m_invoc)
    {
        m_items.push_back(tile);
        if (m_items.size() >= max_abilities)
            return;
    }
}

#endif
