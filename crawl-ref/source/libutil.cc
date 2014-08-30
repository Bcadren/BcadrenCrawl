/**
 * @file
 * @brief Functions that may be missing from some systems
**/

#include "AppHdr.h"

#include "colour.h"
#include "defines.h"
#include "itemname.h" // is_vowel()
#include "libutil.h"
#include "externs.h"
#include "files.h"
#include "message.h"
#include "state.h"
#include "stringutil.h"
#include "unicode.h"
#include "version.h"
#include "viewgeom.h"

#include <sstream>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#ifdef TARGET_OS_WINDOWS
    #undef ARRAYSZ
    #include <windows.h>
    #undef max

    #ifdef WINMM_PLAY_SOUNDS
        #include <mmsystem.h>
    #endif
#endif

#ifdef UNIX
    #include <signal.h>
#endif

#ifdef DGL_ENABLE_CORE_DUMP
    #include <sys/time.h>
    #include <sys/resource.h>
#endif

#ifdef __ANDROID__
    #include <SDL_mixer.h>
    Mix_Chunk* android_sound_to_play = NULL;
#endif

unsigned int isqrt(unsigned int a)
{
    unsigned int rem = 0, root = 0;
    for (int i = 0; i < 16; i++)
    {
        root <<= 1;
        rem = (rem << 2) + (a >> 30);
        a <<= 2;
        if (++root <= rem)
            rem -= root++;
        else
            root--;
    }
    return root >> 1;
}

int isqrt_ceil(int x)
{
    if (x <= 0)
        return 0;
    return isqrt(x - 1) + 1;
}

description_level_type description_type_by_name(const char *desc)
{
    if (!desc)
        return DESC_PLAIN;

    if (!strcmp("The", desc) || !strcmp("the", desc))
        return DESC_THE;
    else if (!strcmp("A", desc) || !strcmp("a", desc))
        return DESC_A;
    else if (!strcmp("Your", desc) || !strcmp("your", desc))
        return DESC_YOUR;
    else if (!strcmp("its", desc))
        return DESC_ITS;
    else if (!strcmp("worn", desc))
        return DESC_INVENTORY_EQUIP;
    else if (!strcmp("inv", desc))
        return DESC_INVENTORY;
    else if (!strcmp("none", desc))
        return DESC_NONE;
    else if (!strcmp("base", desc))
        return DESC_BASENAME;
    else if (!strcmp("qual", desc))
        return DESC_QUALNAME;

    return DESC_PLAIN;
}

static string _number_to_string(unsigned number, bool in_words)
{
    return in_words? number_in_words(number) : make_stringf("%u", number);
}

string apply_description(description_level_type desc, const string &name,
                         int quantity, bool in_words)
{
    switch (desc)
    {
    case DESC_THE:
        return "the " + name;
    case DESC_A:
        return quantity > 1 ? _number_to_string(quantity, in_words) + name
                            : article_a(name, true);
    case DESC_YOUR:
        return "your " + name;
    case DESC_PLAIN:
    default:
        return name;
    }
}

// Should return true if the filename contains nothing that
// the shell can do damage with.
bool shell_safe(const char *file)
{
    int match = strcspn(file, "\\`$*?|><&\n!;");
    return match < 0 || !file[match];
}

void play_sound(const char *file)
{
#if defined(WINMM_PLAY_SOUNDS)
    // Check whether file exists, is readable, etc.?
    if (file && *file)
        sndPlaySoundW(OUTW(file), SND_ASYNC | SND_NODEFAULT);

#elif defined(SOUND_PLAY_COMMAND)
    char command[255];
    command[0] = 0;
    if (file && *file && (strlen(file) + strlen(SOUND_PLAY_COMMAND) < 255)
        && shell_safe(file))
    {
        snprintf(command, sizeof command, SOUND_PLAY_COMMAND, file);
        system(OUTS(command));
    }
#elif defined(__ANDROID__)
    if (Mix_Playing(0))
        Mix_HaltChannel(0);
    if (android_sound_to_play != NULL)
        Mix_FreeChunk(android_sound_to_play);
    android_sound_to_play = Mix_LoadWAV(OUTS(file));
    Mix_PlayChannel(0, android_sound_to_play, 0);
#endif
}

bool key_is_escape(int key)
{
    switch (key)
    {
    CASE_ESCAPE
        return true;
    default:
        return false;
    }
}

// Returns true if s contains tag 'tag', and strips out tag from s.
bool strip_tag(string &s, const string &tag, bool skip_padding)
{
    if (s == tag)
    {
        s.clear();
        return true;
    }

    string::size_type pos;

    if (skip_padding)
    {
        if ((pos = s.find(tag)) != string::npos)
        {
            s.erase(pos, tag.length());
            trim_string(s);
            return true;
        }
        return false;
    }

    if ((pos = s.find(" " + tag + " ")) != string::npos)
    {
        // Leave one space intact.
        s.erase(pos, tag.length() + 1);
        trim_string(s);
        return true;
    }

    if ((pos = s.find(tag + " ")) == 0
        || ((pos = s.find(" " + tag)) != string::npos
            && pos + tag.length() + 1 == s.length()))
    {
        s.erase(pos, tag.length() + 1);
        trim_string(s);
        return true;
    }

    return false;
}

vector<string> strip_multiple_tag_prefix(string &s, const string &tagprefix)
{
    vector<string> results;

    while (true)
    {
        string this_result = strip_tag_prefix(s, tagprefix);
        if (this_result.empty())
            break;

        results.push_back(this_result);
    }

    return results;
}

string strip_tag_prefix(string &s, const string &tagprefix)
{
    string::size_type pos = s.find(tagprefix);

    while (pos && pos != string::npos && !isspace(s[pos - 1]))
        pos = s.find(tagprefix, pos + 1);

    if (pos == string::npos)
        return "";

    string::size_type ns = s.find(" ", pos);
    if (ns == string::npos)
        ns = s.length();

    const string argument = s.substr(pos + tagprefix.length(),
                                     ns - pos - tagprefix.length());

    s.erase(pos, ns - pos + 1);
    trim_string(s);

    return argument;
}

int strip_number_tag(string &s, const string &tagprefix)
{
    const string num = strip_tag_prefix(s, tagprefix);
    int x;
    if (num.empty() || !parse_int(num.c_str(), x))
        return TAG_UNFOUND;
    return x;
}

bool parse_int(const char *s, int &i)
{
    if (!s || !*s)
        return false;
    char *err;
    long x = strtol(s, &err, 10);
    if (*err || x < INT_MIN || x > INT_MAX)
        return false;
    i = x;
    return true;
}

// Naively prefix A/an to a noun.
string article_a(const string &name, bool lowercase)
{
    if (!name.length())
        return name;

    const char *a  = lowercase? "a "  : "A ";
    const char *an = lowercase? "an " : "An ";
    switch (name[0])
    {
        case 'a': case 'e': case 'i': case 'o': case 'u':
        case 'A': case 'E': case 'I': case 'O': case 'U':
            // XXX: Hack
            if (starts_with(name, "one-"))
                return a + name;
            return an + name;
        default:
            return a + name;
    }
}

const char *standard_plural_qualifiers[] =
{
    " of ", " labeled ", NULL
};

// Pluralises a monster or item name.  This'll need to be updated for
// correctness whenever new monsters/items are added.
string pluralise(const string &name, const char *qualifiers[],
                 const char *no_qualifier[])
{
    string::size_type pos;

    if (qualifiers)
    {
        for (int i = 0; qualifiers[i]; ++i)
            if ((pos = name.find(qualifiers[i])) != string::npos
                && !ends_with(name, no_qualifier))
            {
                return pluralise(name.substr(0, pos)) + name.substr(pos);
            }
    }

    if (!name.empty() && name[name.length() - 1] == ')'
        && (pos = name.rfind(" (")) != string::npos)
    {
        return pluralise(name.substr(0, pos)) + name.substr(pos);
    }

    if (!name.empty() && name[name.length() - 1] == ']'
        && (pos = name.rfind(" [")) != string::npos)
    {
        return pluralise(name.substr(0, pos)) + name.substr(pos);
    }

    if (ends_with(name, "us"))
    {
        if (ends_with(name, "lotus"))
            return name + "es";
        else
            // Fungus, ufetubus, for instance.
            return name.substr(0, name.length() - 2) + "i";
    }
    else if (ends_with(name, "larva") || ends_with(name, "antenna"))
        return name + "e";
    else if (ends_with(name, "ex"))
    {
        // Vortex; vortexes is legal, but the classic plural is cooler.
        return name.substr(0, name.length() - 2) + "ices";
    }
    else if (ends_with(name, "mosquito") || ends_with(name, "ss"))
        return name + "es";
    else if (ends_with(name, "cyclops"))
        return name.substr(0, name.length() - 1) + "es";
    else if (name == "catoblepas")
        return "catoblepae";
    else if (ends_with(name, "s"))
        return name;
    else if (ends_with(name, "y"))
    {
        if (name == "y")
            return "ys";
        // day -> days, boy -> boys, etc
        else if (is_vowel(name[name.length() - 2]))
            return name + "s";
        // jelly -> jellies
        else
            return name.substr(0, name.length() - 1) + "ies";
    }
    else if (ends_with(name, "fe"))
    {
        // knife -> knives
        return name.substr(0, name.length() - 2) + "ves";
    }
    else if (ends_with(name, "staff"))
    {
        // staff -> staves
        return name.substr(0, name.length() - 2) + "ves";
    }
    else if (ends_with(name, "f") && !ends_with(name, "ff"))
    {
        // elf -> elves, but not hippogriff -> hippogrives.
        // TODO: if someone defines a "goblin chief", this should be revisited.
        return name.substr(0, name.length() - 1) + "ves";
    }
    else if (ends_with(name, "mage"))
    {
        // mage -> magi
        return name.substr(0, name.length() - 1) + "i";
    }
    else if (name == "gold"                 || ends_with(name, "fish")
             || ends_with(name, "folk")     || ends_with(name, "spawn")
             || ends_with(name, "tengu")    || ends_with(name, "sheep")
             || ends_with(name, "swine")    || ends_with(name, "efreet")
             || ends_with(name, "jiangshi") || ends_with(name, "unborn")
             || ends_with(name, "raiju")    )
    {
        return name;
    }
    else if (ends_with(name, "ch") || ends_with(name, "sh")
             || ends_with(name, "x"))
    {
        // To handle cockroaches, sphinxes, and bushes.
        return name + "es";
    }
    else if (ends_with(name, "simulacrum") || ends_with(name, "eidolon"))
    {
        // simulacrum -> simulacra (correct Latin pluralisation)
        // also eidolon -> eidola (correct Greek pluralisation)
        return name.substr(0, name.length() - 2) + "a";
    }
    else if (ends_with(name, "djinni"))
    {
        // djinni -> djinn.
        return name.substr(0, name.length() - 1);
    }
    else if (name == "foot")
        return "feet";
    else if (name == "ophan" || name == "cherub" || name == "seraph")
    {
        // Unlike "angel" which is fully assimilated, and "cherub" and "seraph"
        // which may be pluralised both ways, "ophan" always uses Hebrew
        // pluralisation.
        return name + "im";
    }

    return name + "s";
}

string apostrophise(const string &name)
{
    if (name.empty())
        return name;

    if (name == "you" || name == "You")
        return name + "r";

    if (name == "it" || name == "It")
        return name + "s";

    if (name == "itself")
        return "its own";

    if (name == "himself")
        return "his own";

    if (name == "herself")
        return "her own";

    if (name == "themselves")
        return "their own";

    if (name == "yourself")
        return "your own";

    const char lastc = name[name.length() - 1];
    return name + (lastc == 's' ? "'" : "'s");
}

string apostrophise_fixup(const string &msg)
{
    if (msg.empty())
        return msg;

    // XXX: This is rather hackish.
    return replace_all(msg, "s's", "s'");
}

static string pow_in_words(int pow)
{
    switch (pow)
    {
    case 0:
        return "";
    case 3:
        return " thousand";
    case 6:
        return " million";
    case 9:
        return " billion";
    case 12:
    default:
        return " trillion";
    }
}

static string tens_in_words(unsigned num)
{
    static const char *numbers[] =
    {
        "", "one", "two", "three", "four", "five", "six", "seven",
        "eight", "nine", "ten", "eleven", "twelve", "thirteen", "fourteen",
        "fifteen", "sixteen", "seventeen", "eighteen", "nineteen"
    };
    static const char *tens[] =
    {
        "", "", "twenty", "thirty", "forty", "fifty", "sixty", "seventy",
        "eighty", "ninety"
    };

    if (num < 20)
        return numbers[num];

    int ten = num / 10, digit = num % 10;
    return string(tens[ten]) + (digit ? string("-") + numbers[digit] : "");
}

static string join_strings(const string &a, const string &b)
{
    if (!a.empty() && !b.empty())
        return a + " " + b;

    return a.empty() ? b : a;
}

static string hundreds_in_words(unsigned num)
{
    unsigned dreds = num / 100, tens = num % 100;
    string sdreds = dreds? tens_in_words(dreds) + " hundred" : "";
    string stens  = tens? tens_in_words(tens) : "";
    return join_strings(sdreds, stens);
}

string number_in_words(unsigned num, int pow)
{
    if (pow == 12)
        return number_in_words(num, 0) + pow_in_words(pow);

    unsigned thousands = num % 1000, rest = num / 1000;
    if (!rest && !thousands)
        return "zero";

    return join_strings((rest? number_in_words(rest, pow + 3) : ""),
                        (thousands? hundreds_in_words(thousands)
                                    + pow_in_words(pow)
                                  : ""));
}

/**
 * Compare two strings, sorting integer numeric parts according to their value.
 *
 * "foo123bar" > "foo99bar"
 * "0.10" > "0.9" (version sort)
 *
 * @param a String one.
 * @param b String two.
 * @param limit If passed, comparison ends after X numeric parts.
 * @return As in strcmp().
**/
int numcmp(const char *a, const char *b, int limit)
{
    int res;

not_numeric:
    while (*a && *a == *b && !isadigit(*a))
    {
        a++;
        b++;
    }
    if (!a && !b)
        return 0;
    if (!isadigit(*a) || !isadigit(*b))
        return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
    while (*a == '0')
        a++;
    while (*b == '0')
        b++;
    res = 0;
    while (isadigit(*a))
    {
        if (!isadigit(*b))
            return 1;
        if (*a != *b && !res)
            res = (*a < *b) ? -1 : 1;
        a++;
        b++;
    }
    if (isadigit(*b))
        return -1;
    if (res)
        return res;

    if (--limit)
        goto not_numeric;
    return 0;
}

// make STL sort happy
bool numcmpstr(const string a, const string b)
{
    return numcmp(a.c_str(), b.c_str()) == -1;
}

bool version_is_stable(const char *v)
{
    // vulnerable to changes in the versioning scheme
    for (;; v++)
    {
        if (*v == '.' || isadigit(*v))
            continue;
        if (*v == '-')
            return isadigit(v[1]);
        return true;
    }
}

static void inline _untag(string &s, const string pre,
                          const string post, bool onoff)
{
    size_t p = 0;
    while ((p = s.find(pre, p)) != string::npos)
    {
        size_t q = s.find(post, p);
        if (q == string::npos)
            q = s.length();
        if (onoff)
        {
            s.erase(q, post.length());
            s.erase(p, pre.length());
        }
        else
            s.erase(p, q - p + post.length());
    }
}

string untag_tiles_console(string s)
{
    _untag(s, "<tiles>", "</tiles>", is_tiles());
    _untag(s, "<console>", "</console>", !is_tiles());
#ifdef USE_TILE_WEB
    _untag(s, "<webtiles>", "</webtiles>", true);
#else
    _untag(s, "<webtiles>", "</webtiles>", false);
#endif
#ifdef USE_TILE_LOCAL
    _untag(s, "<localtiles>", "</localtiles>", true);
    _untag(s, "<nomouse>", "</nomouse>", false);
#else
    _untag(s, "<localtiles>", "</localtiles>", false);
    _untag(s, "<nomouse>", "</nomouse>", true);
#endif
    return s;
}

string colour_string(string in, int col)
{
    if (in.empty())
        return in;
    const string cols = colour_to_str(col);
    return "<" + cols + ">" + in + "</" + cols + ">";
}



#ifndef USE_TILE_LOCAL
static coord_def _cgettopleft(GotoRegion region)
{
    switch (region)
    {
    case GOTO_MLIST:
        return crawl_view.mlistp;
    case GOTO_STAT:
        return crawl_view.hudp;
    case GOTO_MSG:
        return crawl_view.msgp;
    case GOTO_DNGN:
        return crawl_view.viewp;
    case GOTO_CRT:
    default:
        return crawl_view.termp;
    }
}

coord_def cgetpos(GotoRegion region)
{
    const coord_def where = coord_def(wherex(), wherey());
    return where - _cgettopleft(region) + coord_def(1, 1);
}

static GotoRegion _current_region = GOTO_CRT;

void cgotoxy(int x, int y, GotoRegion region)
{
    _current_region = region;
    const coord_def tl = _cgettopleft(region);
    const coord_def sz = cgetsize(region);

#ifdef ASSERTS
    if (x < 1 || y < 1 || x > sz.x || y > sz.y)
    {
        save_game(false); // should be safe
        die("screen write out of bounds: (%d,%d) into (%d,%d)", x, y,
            sz.x, sz.y);
    }
#endif

    gotoxy_sys(tl.x + x - 1, tl.y + y - 1);

#ifdef USE_TILE_WEB
    tiles.cgotoxy(x, y, region);
#endif
}

GotoRegion get_cursor_region()
{
    return _current_region;
}
#endif // USE_TILE_LOCAL

coord_def cgetsize(GotoRegion region)
{
    switch (region)
    {
    case GOTO_MLIST:
        return crawl_view.mlistsz;
    case GOTO_STAT:
        return crawl_view.hudsz;
    case GOTO_MSG:
        return crawl_view.msgsz;
    case GOTO_DNGN:
        return crawl_view.viewsz;
    case GOTO_CRT:
    default:
        return crawl_view.termsz;
    }
}

void cscroll(int n, GotoRegion region)
{
    // only implemented for the message window right now
    if (region == GOTO_MSG)
        scroll_message_window(n);
}

mouse_mode mouse_control::ms_current_mode = MOUSE_MODE_NORMAL;

string unwrap_desc(string desc)
{
    // Don't append a newline to an empty description.
    if (desc == "")
        return "";

    trim_string_right(desc);

    // Lines beginning with a colon are tags with a full entry scope
    // Only nowrap is supported for now
    while (desc[0] == ':')
    {
        int pos = desc.find("\n");
        string tag = desc.substr(1, pos - 1);
        desc.erase(0, pos + 1);
        if (tag == "nowrap")
            return desc;
        else if (desc == "")
            return "";
    }

    // An empty line separates paragraphs.
    desc = replace_all(desc, "\n\n", "\\n\\n");
    // Indented lines are pre-formatted.
    desc = replace_all(desc, "\n ", "\\n ");

    // Don't add whitespaces between tags
    desc = replace_all(desc, ">\n<", "><");
    // Newlines are still whitespace.
    desc = replace_all(desc, "\n", " ");
    // Can force a newline with a literal "\n".
    desc = replace_all(desc, "\\n", "\n");

    return desc + "\n";
}

#ifdef TARGET_OS_WINDOWS
// FIXME: This function should detect if aero is running, but the DwmIsCompositionEnabled
// function isn't included in msys, so I don't know how to do that. Instead, I just check
// if we are running vista or higher. -rla
static bool _is_aero()
{
    OSVERSIONINFOEX osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (GetVersionEx((OSVERSIONINFO *) &osvi))
        return osvi.dwMajorVersion >= 6;
    else
        return false;
}

taskbar_pos get_taskbar_pos()
{
    RECT rect;
    HWND taskbar = FindWindow("Shell_traywnd", NULL);
    if (taskbar && GetWindowRect(taskbar, &rect))
    {
        if (rect.right - rect.left > rect.bottom - rect.top)
        {
            if (rect.top > 0)
                return TASKBAR_BOTTOM;
            else
                return TASKBAR_TOP;
        }
        else
        {
            if (rect.left > 0)
                return TASKBAR_RIGHT;
            else
                return TASKBAR_LEFT;
        }
    }
    return TASKBAR_NO;
}

int get_taskbar_size()
{
    RECT rect;
    int size;
    taskbar_pos tpos = get_taskbar_pos();
    HWND taskbar = FindWindow("Shell_traywnd", NULL);

    if (taskbar && GetWindowRect(taskbar, &rect))
    {
        if (tpos & TASKBAR_H)
                size = rect.bottom - rect.top;
        else if (tpos & TASKBAR_V)
                size = rect.right - rect.left;
        else
            return 0;

        if (_is_aero())
            size += 3; // Taskbar behave strangely when aero is active.

        return size;
    }
    return 0;
}

static BOOL WINAPI console_handler(DWORD sig)
{
    switch (sig)
    {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        return true; // block the signal
    default:
        return false;
    case CTRL_CLOSE_EVENT:
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
        if (crawl_state.seen_hups++)
            return true; // abort immediately

#ifndef USE_TILE_LOCAL
        // Should never happen in tiles -- if it does (Cygwin?), this will
        // kill the game without saving.
        w32_insert_escape();

        Sleep(15000); // allow 15 seconds for shutdown, then kill -9
#endif
        return true;
    }
}

void init_signals()
{
    // If there's no console, this will return an error, which we ignore.
    // For GUI programs there's no controlling terminal, but there might be
    // one on Cygwin.
    SetConsoleCtrlHandler(console_handler, true);
}

void release_cli_signals()
{
    SetConsoleCtrlHandler(nullptr, false);
}

void text_popup(const string& text, const wchar_t *caption)
{
    MessageBoxW(0, OUTW(text), caption, MB_OK);
}
#else
# ifdef USE_CURSES

/* [ds] This SIGHUP handling is primitive and far from safe, but it
 * should be better than nothing. Feel free to get rigorous on this.
 */
static void handle_hangup(int)
{
    if (crawl_state.seen_hups++)
        return;

    // When using Curses, closing stdin will cause any Curses call blocking
    // on key-presses to immediately return, including any call that was
    // still blocking in the main thread when the HUP signal was caught.
    // This should guarantee that the main thread will un-stall and
    // will eventually return to _input() in main.cc, which will then
    // gracefully save the game.

    // SAVE CORRUPTING BUG!!!  We're in a signal handler, calling free()
    // when closing the FILE object is likely to lead to lock-ups, and even
    // if it were a plain kernel-side descriptor, calling functions such
    // as select() or read() is undefined behaviour.
    fclose(stdin);
}
# endif

void init_signals()
{
#ifdef DGAMELAUNCH
    // Force timezone to UTC.
    setenv("TZ", "", 1);
    tzset();
#endif

#ifdef USE_UNIX_SIGNALS
#ifdef SIGQUIT
    signal(SIGQUIT, SIG_IGN);
#endif

#ifdef SIGINT
    signal(SIGINT, SIG_IGN);
#endif

# ifdef USE_TILE_LOCAL
    // Losing the controlling terminal doesn't matter, we continue and will
    // shut down only when the actual window is closed.
    signal(SIGHUP, SIG_IGN);
# else
    signal(SIGHUP, handle_hangup);
# endif
#endif

#ifdef DGL_ENABLE_CORE_DUMP
    rlimit lim;
    if (!getrlimit(RLIMIT_CORE, &lim))
    {
        lim.rlim_cur = RLIM_INFINITY;
        setrlimit(RLIMIT_CORE, &lim);
    }
#endif
}

void release_cli_signals()
{
#ifdef USE_UNIX_SIGNALS
    signal(SIGQUIT, SIG_DFL);
    signal(SIGINT, SIG_DFL);
#endif
}

#endif
