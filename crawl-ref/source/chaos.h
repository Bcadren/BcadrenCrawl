#pragma once

#include "beam-type.h"

beam_type eldritch_damage_type();
beam_type chaos_damage_type(bool player = false);
void chaotic_status(actor * victim, int dur, actor * source);
void chaotic_buff(actor * act, int dur, actor * attacker);
void chaotic_debuff(actor * act, int dur, actor * attacker);