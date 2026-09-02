#pragma once

enum recite_type
{
    RECITE_HERETIC,
    RECITE_CHAOTIC,
    RECITE_IMPURE,
    RECITE_UNHOLY,
    RECITE_BREATH,      // Empowered Silver Breath.
    NUM_RECITE_TYPES
};

enum class zin_eff
{
    nothing,
    daze,
    confuse,
    cause_fear,
    smite,
    blind,
    silver_candle,
    antimagic,
    mute,
    mad,
    dumb,
    ignite_chaos,
    saltify,
    rot,
    holy_word,
};