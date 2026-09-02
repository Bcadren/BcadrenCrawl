# Bcadren Crawl: Boulder Brew

Dungeon Crawl Stone Soup is a game of dungeon exploration, combat and magic, involving characters of diverse skills, worshipping deities of great power and caprice. To win, you'll need to be a master of tactics and strategy, and prevail against overwhelming odds.

## Contents

1. [Overview](#overview)
   1. [Core Changes](#core-changes)
   2. [Branch Changes](#branch-changes)
   3. [Gods](#gods)
   4. [New Races](#new-races)
   5. [Reworked Vanilla Races](#reworked-vanilla-races)
   6. [Hybrid Backgrounds](#hybrid-backgrounds)
   7. [Customizable Backgrounds](#customizable-backgrounds)
3. [How to Play](#how-to-play)
4. [Full Changelog](#full-changelog)
5. [Community](#community)
6. [How you can help](#how-you-can-help)
7. [License and history information](#license-and-history-information)

## Overview

This is just a rundown of some of the fun features and major differences from mainline crawl; may they serve to entice Crawl veterans and new players alike to the variant.

### Core Changes

#### Dual Wielding

![Sai](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/item/shields/sai2.png) 
Players can wield any combination of one handed weapons or shields, instead of being restricted to a strict main-hand and shield-hand. The weapon listed on top in the sidebar strikes first and the weapon listed second strikes second. Examining either weapon in the inventory while wielded will tell the skill necessary to reach mindelay with the combination. *Pro-Tip:* Dual wielding is great in mid to late game, but the delay penalties make it not worth it in early game. 

Certain items, such as the sai pictured are shield/weapon hybrids and provide some shield hand (SH) on top of a weapon attack. In the case of the Sai, both the attack and the SH are based on short blades skill.

#### Ranged Reform

![Mangonel](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/item/weapon/ranged/mangonel2.png)
Ranged combat was reworked to be most effective against enemies 3-5 spaces away from the player to encourage swapping to and from a melee weapon. Additionally, throwing was removed/merged into slings. Normal (human-sized) slings have a chance to ricochet, while the massive mangonel usable by ogre-sized and larger player characters hurls large rocks, making explosions of dust when they land. Additionally the triple crossbow actually fires three shots at once and all crossbows pierce.

#### Staff Reform

![Magical Staff](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/item/staff/staff03.png) In addition to their spellschool, staves now have actual brands; such as warped, which forces smite targetting on spells; scoped, which increases the range of spells; and wretched, which strips the associated resist on melee. Any spell changes from brands only affect spells of the staves' element. Additionally staff fixedArts are actually staves now and the +/- value on staves is now used for additive spellpower. Staves of transmutations added, they constrict enemies. Staves of Life (renamed summoning) heal and extend the duration of summons on melee.

#### Mounts

![Spider Mount](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/mount/spider_mount_front.png) Two invocations (one with Jiyva and one with Bahamut & Tiamat) as well as two summoning spells allow the player to summon a mount; which for most purposes functions as a monster on the same tile as the player. It tanks hits for you (randomly), your movement speed is set to match it and once per 1.0 auts it joins in melee combat with you. Some of them have additional abilities beyond this.

#### Chaos Magic

![Chaos](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/item/amulet/i-chaos.png) With a source of chaos (being a Scintillating Draconian, worshipping Xom, wielding a Chaos magical staff or wearing an amulet of chaos); most spells have a chance of going chaotic. For summons this means the monster may come as a glowing shapeshifter that starts in the form of the intended creature or it comes with a buff that gives it chaos-branded melee; for damaging spells this means the damage type is rolled randomly, any clouds it leaves are rolled randomly (with bias to seething chaos) and anything that survives the hit is randomly buffed or debuffed.

### Branch Changes

#### Dungeon
![Dungeon](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/gateways/exit_dungeon.png) The dungeon itself has been shortened to 8 floors, with slightly more monsters per floor and a much faster difficulty scaling past D:3.

#### Pits of Slime
![Slime Pits](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/gateways/enter_slime.png)
Slime Pits reworked to have actual caustic pools of ooze and unique mushroom trees in the slime with a much larger variety of slime monsters throughout. The climactic battle with the Royal Jelly also got an overhaul.

#### Sewer
![Sewer](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/gateways/sewer_portal.png)
Sewer has been merged into D:2 with a dungeon generation pattern unique to the floor and a mix of submerging watery threats meant to be the first early game challenge.

#### Ossuary
![Ossuary](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/gateways/ossuary_portal.png)
Ossuary portals removed and their vaults placed randomly (one small and one large one per game) somewhere between D:3 and Spider:3. Their monster sets were toughened and they blend into the area they spawn somewhat, being more ruined/flooded in swamp; having even tougher monster sets at deeper depths. Worst case scenario is a single early Greater Mummy and some death scarabs in Spider.

### Gods

#### Bahamut & Tiamat
![Bahamut & Tiamat](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/bahamut_tiamat.png) The twin dragon gods only accept the worship of those with draconic ancestry (draconians); they offer such powers as the ability to change your draconian colour, enhanced power of your draconian breath and summoning of drakes.

#### Dithmenos
![Dithmenos](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/dithmenos.png) Enemies now 'cast long shadows', vastly reducing your LoS while worshipping Dithmenos.

#### Hepliaklqana
![Hepliaklqana](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/hep0.png) Your ancestors now more directly match your species. In all cases this means that they match your racial mutations (such as your reflexive headbutt as a minotaur) as well as relative bulk, movement speed; ability to use weapons, etc. In some cases, this also means the spell choices of the battlemage and/or the weapon choices of the knight reflect your species in terms of aptitudes or the sets used by the denizens of the dungeon.

![Kitty Knight](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/mon/nonliving/felid_ancestor_knight.png) Some races also get unique tiles.

#### Jiyva
![Jiyva](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/jiyva06.png) Jiyva massively reworked. They now slowly give a set of extremely powerful slimy mutations on a gift timer, similar to a demonspawn set; though heavily slime themed and rolling out more slowly. Additionally, Slimify now works on everything; you can summon a slime mount, which moves slowly but dissolves walls and a little bit of the slime bits splashes onto the walls and floors when you use your dissolution ability.

#### Kikubaaqudgha
![Kikubaaqudgha](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/kikubaaqudgha.png) The first god to receive 'rituals', Kikubaaqudgha now has a group of spells that are very powerful for their spell level, but cost piety on success. Additionally, receive corpses was reworked into a toggle; which while on expends a small amount of piety for additional corpses on spell casts and Kikubaaqudgha allows the living to cast from HP, via expending 1 piety to cast Sublimation of Blood instantly when you have insufficient mana to otherwise finish your spell.

#### Trog
![Trog](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/trog.png) Trog was simultaneously buffed and nerfed by an update that made his abilities stronger and his ability to extend berserk longer a little better at the cost of none of his abilities other than "berserk" working unless you are berserk and berserkitis. As he's more strongly tied to berserking now; undead and formicids can no longer worship him.

#### Yredelemnul
![Yredelemnul](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/yredelemnul.png) Animate Remains was reworked into a reintroduction of the old "Twisted Reanimation" spell; that is you make undead abominations from large amounts of corpses now. Additionally abominations (including enemy ones) have been made far more interesting, with random powerful attacks instead of a single bland one. The maximum power and amount of abominations you can have scales with piety. Oh and enslaved souls respawn on a timer, similar to Hepliaklqana ancestors.

![Small Abomination](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/mon/aberrations/abomination_small1.png) Lovely aren't they?

### New Races
![Molten Gargoyle](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/molten_gargoyle_m.png) Molten gargoyles are frailer than their stone cousins, but nearly immune to fire and able to meld their molten forms to perfectly fit into any armour, suffering no spellcasting or evasion penalties.

![Oozomorph](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/oozomorph.png) Gelatinous oozomorphs can mold their slimy appendages to use any combination of armour pieces and can subsume a single magical item into their protoplasm to benefit from its magic properties.

![Fairy](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/fairy.png) Fairies start the game with only 3 HP, deal no damage in melee and move very slowly; but almost all hits only do 1 damage to them. Spells that normally cost 1 MP are free and all other spells cost 1 MP.

![Goblin](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/goblin.png) Goblins have very low stats, which never grow with level; but very high aptitudes and an innate **invocations enhancer** in their place. Intended as the opposite of Demigods, low stats, high apts, more help from gods.

![Lignifites](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/lignifite.png) Lignifites are slow-growing treefolk. Like all plant they are immune to torment and draining; they start the game as small fast-moving, but frail saplings; with level they grow larger, slower and bulkier and gain the ability to plant their roots to grant a temporary regeneration boost and stasis.

![Silent Spectre](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/silent_spectre_m.png) The oldest part of the fork; silent spectres have an innate aura of silence and are partially immune to it, having a poor invocations aptitude and anti-wizardry instead of being unable to cast or invoke godly power. They still; however, cannot use scrolls. Unlike most other undead they are allowed to use transmutations forms, as they are formless ghosts, which morph easily, rather than rotting bodies.

### Reworked Vanilla Races

![Octopode](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/octopode1.png) Octopodes are now allowed to use 2Hers one-handed, like Formicids; but they sacrifice how many arms they have left to move with; the heavier equipment you have wielded, the lower your evasion and the slower your move.

![Scintillating Draconian](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/drchead/drchead_scintillating.png) Draconians majorly reworked to add a massive variety of new colours. They now have poor apts, excluding those buffed by their ancestry to enforce the 'randomized character' intention of the design more heavily; but this ancestry will also buff two random weapon skills and a random defensive skill (dodging/stealth/shields).

### Hybrid Backgrounds

Many races were also turned into backgrounds to allow hybridizing the uniqueness of one race with another.

![Centaur](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/bottom/centaur_brown.png) **Centaur**: Top half (base race), bottom half horse. The centaur background grants centaur speed movement, bardings and increased ability with ranged weapons at the cost of poor defensive apts and slow leveling.

![Naga](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/bottom/naga_red_f.png) **Naga**: Top half (base race), bottom half giant snake. The Naga background grants consider bulk, bardings, aptitude boosts and constriction at the cost of slow movement.

![Merfolk](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/bottom/merfolk_water.png) **Merfolk**: Your legs turn into a tail when you touch water; additionally you have extremely good aptitudes with polearms, slings and transmutations and overall better aptitudes than most others of your race; however you are a bit frail.

![Demonspawn](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/demonspawn_pink.png) **Demonspawn**: This background cannot worship the 'good' gods and suffers some aptitude maluses in return for a set of Demonspawn mutations with level and an amazing bonus to Invocations.

![Demigod](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/demigod_m.png) **Demigod**: This background cannot worship any god and has poor aptitudes, but starts with higher than average stats and has far higher stat gain than normal as well. *Still a challenge, but now you can do it as any race.*

![Vine Stalker](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/vine_stalker_m.png) **Vine Stalker**: Being grafted with a plant symbiote has left you frailer than other members of your species; but with a natural spirit shield and the ability to leech mana on your bite.

![Mummy](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/player/base/mummy_m.png) **Mummy**: No longer *just* a challenge option; mummies have increased magic apts, as well as their undead resists and foodlessness to trade for lowered melee apts and lack of potions. 

### Customizable Backgrounds

Both customizable backgrounds have your choice of how to distribute the +12 stat points that are usually fixed based on your background choice.

![Gold](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/item/gold/25.png) **Nobles**: Start with 1000 gold in a specially made bazaar designed to allow the player to make whatever kind of starting item package they want.

![Altar](https://github.com/Bcadren/crawl/blob/bCrawl/crawl-ref/source/rltiles/dngn/altars/ecumenical.png) **Priests**: Start in a small temple, where they can pick any god (excluding the ones used by Zealot starts) to start their game with; however, they don't start with much else.

## How to Play

#### Internet Play

You can play BC:BB online, competing with other players or watching them.

**Playable Links:**

*Updates Daily:*
* https://crawl.kelbi.org/#play-dcss-bcadrencrawl (New York, United States)
* https://crawl.xtahua.com/#play-dcss-bcadren (France)
* https://crawl.project357.org/play/dcss-web-bcadrencrawl (Australia)

*Updates Occasionally (May Miss the Newest Updates):*
* http://webzook.net:8080/#play-dcss-web-bcadren (Korea)
* http://x-crawl.de/play/bcadrencrawl-web-trunk (Germany)

## Full Changelog

[The master document](https://docs.google.com/document/d/1gNrFq3TNuaUoSYYwBHJG4bprF4zrJwzvipujZh6DFHI/edit) has grown long and cumbersome to reference, but it is a master reference to everything that makes the variant different from mainline to date. Peruse if you need to know something specific.

#### Offline Play

Both classical ASCII and tiles (GUI) versions of BC:BB are available to [download for Windows](https://github.com/Bcadren/crawl/releases/).

## How you can help

If you like the game and you want to help make it better, there are a number
of ways to do so. 

### Financial Contributions
The fork has a [Patreon](https://www.patreon.com/Bcadren) for those so inclined. I won't active solicit donations too much; but more time consuming feature requests are more likely to taken seriously or given priority when they come from a patron.

### Reporting bugs

At any time, there will be bugs -- finding and reporting them is a great help.
Many of the online servers host the regularly updated development version. Bugs
should be reported to [Bcadren](bhylton7@gmail.com). Besides pointing out bugs, new ideas on how to improve interface or gameplay and implementation requests are welcome.

### Map making
Crawl creates levels by combining many hand-made (but often randomised) maps, known as *vaults*. Making them is fun and
easy. It's best to start with simple entry vaults: see [simple.des](crawl-ref/source/dat/des/arrival/simple.des) for examples. You can also read [the level-design manual](crawl-ref/docs/develop/levels/introduction.txt) for more help.

If you're ambitious, you can create new vaults for anywhere in the game. If you've
made some vaults, you can test them on your own system (no compiling needed) and
submit them to [Bcadren](bhylton7@gmail.com)

### Monster Speech & Descriptions
Monster speech provides a lot of flavour. Just like vaults, varied speech depends
upon a large set of entries. Speech syntax is effective but unusual, so you may want to read [the formatting guide](crawl-ref/docs/develop/monster_speech.txt).

Current item/monster/spell descriptions can be read in-game with `?/` or out-of-game
them in [dat/descript/](crawl-ref/source/dat/descript/). The following conventions should be more or less obeyed:
* Descriptions ought to contain flavour text, ideally pointing out major weaknesses/strengths.
* Citations are okay, but try to stay away from the most generic ones.

Most items unique to this fork lack any translation and mainline's translations may prove incorrect or outdated, especially for items that are changed heavily here so translation work in particular may be helpful.

### Tiles
We're always open to improvements to existing tiles or variants of often-used tiles (eg floor tiles). If you want to give this a shot, please [contact us](#community).

### Patches
For developers (both existing & aspiring!), you can download/fork the source code and write patches. Bug fixes as well as new features are very much welcome.

#### Community

Bcadren Crawl: Boulder Brew has an official [subreddit](https://www.reddit.com/r/bcadrencrawl) and [discord server](https://discord.gg/XE4hwVB7N4). Additionally the lead developer can be reached via bhylton7@gmail.com

## License and history information

Bcadren Crawl: Boulder Brew is descended from Dungeon Crawl: Stone Soup. Once a small project with only a few races different from mainline, it's not one of, if not the most ambitious of the 'forks'. Licensing information inherits from DCSS.

BC:BB is licensed as GPLv2+. See [licence.txt](crawl-ref/licence.txt) for the full text.

[CREDITS.txt](crawl-ref/CREDITS.txt) contains a list of mainline contributors. BcadrenCrawl Specific contributors can be found at the top of [the details document](https://docs.google.com/document/d/1gNrFq3TNuaUoSYYwBHJG4bprF4zrJwzvipujZh6DFHI/edit).

We gladly use the following open source packages; thanks to their developers:

* The Lua scripting language, for in-game functionality and user macros ([license](crawl-ref/docs/license/lualicense.txt)).
* The PCRE library, for regular expressions ([license](crawl-ref/docs/license/pcre_license.txt)).
* The SQLite library, as a database engine ([license](https://www.sqlite.org/copyright.html)).
* The SDL and SDL_image libraries, for tiles display ([license](crawl-ref/docs/license/lgpl.txt)).
* The libpng library, for tiles image loading ([license](crawl-ref/docs/license/libpng-LICENSE.txt)).

Thank you, and have fun crawling!
