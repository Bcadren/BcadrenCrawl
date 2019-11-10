/*
--- Colour related functions

module "colour"
*/

#include "AppHdr.h"

#include "l-libs.h"

#include "cluautil.h"
#include "colour.h"
#include "dlua.h"
#include "mpr.h"

typedef int (*lua_element_colour_calculator)(int, const coord_def&, lua_datum);

static int _lua_element_colour(int rand, const coord_def& loc,
                               lua_datum function);

struct lua_element_colour_calc : public element_colour_calc
{
    lua_element_colour_calc(element_type _type, string _name,
                            lua_datum _function)
        : element_colour_calc(_type, _name, (element_colour_calculator)_lua_element_colour),
          function(_function)
        {};

    virtual int get(const coord_def& loc = coord_def(),
                    bool non_random = false) override;

protected:
    lua_datum function;
};

int lua_element_colour_calc::get(const coord_def& loc, bool non_random)
{
    // casting function pointers from other function pointers is guaranteed
    // to be safe, but calling them on pointers not of their type isn't, so
    // assert here to be safe - add to this assert if something different is
    // needed
    ASSERT((lua_element_colour_calculator)calc == _lua_element_colour);
    lua_element_colour_calculator real_calc =
        (lua_element_colour_calculator)calc;
    return (*real_calc)(rand(non_random), loc, function);
}

static int _lua_element_colour(int rand, const coord_def& loc,
                               lua_datum function)
{
    lua_State *ls = dlua.state();

    function.push();
    lua_pushinteger(ls, rand);
    lua_pushinteger(ls, loc.x);
    lua_pushinteger(ls, loc.y);
    if (!dlua.callfn(nullptr, 3, 1))
    {
        mprf(MSGCH_WARN, "%s", dlua.error.c_str());
        return BLACK;
    }

    string colour = luaL_checkstring(ls, -1);
    lua_pop(ls, 1);

    return str_to_colour(colour);
}

/*
--- Define a new elemental colour.
-- See <code>COLOUR:</code> in <tt>docs/develop/levels/syntax.txt</tt> for details.
function add_colour(name, fun)
*/
LUAFN(l_add_colour)
{
    const string &name = luaL_checkstring(ls, 1);
    if (lua_gettop(ls) != 2 || !lua_isfunction(ls, 2))
        luaL_error(ls, "Expected colour generation function.");

    int col = str_to_colour(name, -1, false);
    if (col == -1)
        luaL_error(ls, ("Unknown colour: " + name).c_str());

    CLua& vm(CLua::get_vm(ls));
    lua_datum function(vm, 2);

    add_element_colour(
        new lua_element_colour_calc((element_type)col, name, function)
    );

    return 0;
}

static const struct luaL_reg colour_lib[] =
{
    { "add_colour", l_add_colour },

    { nullptr, nullptr }
};

void dluaopen_colour(lua_State *ls)
{
    luaL_openlib(ls, "colour", colour_lib, 0);
}
