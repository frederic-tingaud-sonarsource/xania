/*************************************************************************/
/*  Xania (M)ulti(U)ser(D)ungeon server source code                      */
/*  (C) 2021 Xania Development Team                                      */
/*  See merc.h and README for original copyrights                        */
/*************************************************************************/
#include "Races.hpp"
#include "BitsCharAct.hpp"
#include "merc.h"

/* race table */
/* when adding a new PC race ensure that it appears towards the top
   of this list! */
const struct race_type race_table[] = {
    /*
        {
       name,    pc_race?,
       act bits,   aff_by bits,   off bits,
       imm,     res,     vuln,
       form,    parts
        },
    */
    {"unique", false, 0, 0, 0, 0, 0, 0, G, A | D | F},

    {"human", true, 0, 0, 0, 0, 0, 0, A | H | M | V, A | B | C | D | E | F | G | H | I | J | K},

    {"hobbit", true, 0, 0, 0, 0, 0, 0, A | H | M | V, A | B | C | D | E | F | G | H | I | J | K},

    {"minotaur", true, 0, 0, 0, 0, 0, 0, A | H | M | V, A | B | C | D | E | F | G | H | I | J | K | W},

    {"elf", true, 0, AFF_INFRARED, 0, 0, RES_CHARM, VULN_IRON, A | H | M | V,
     A | B | C | D | E | F | G | H | I | J | K},

    {"half-elf", true, 0, AFF_INFRARED, 0, 0, 0, VULN_IRON, A | H | M | V, A | B | C | D | E | F | G | H | I | J | K},

    {"dragon", true, 0, 0, 0, 0, 0, 0, A | H | M | V, A | C | D | E | F | H | J | K | P | U | V | X},

    {"eagle", true, 0, AFF_FLYING, 0, 0, 0, 0, A | H | M | V, A | C | D | E | F | H | J | P | K | U},

    {"orc", true, 0, AFF_INFRARED, 0, 0, 0, 0, A | H | M | V, A | B | C | D | E | F | G | H | I | J | K},

    {"dwarf", true, 0, AFF_INFRARED, 0, 0, RES_POISON | RES_DISEASE, VULN_DROWNING, A | H | M | V,
     A | B | C | D | E | F | G | H | I | J | K},

    {"wolf", true, 0, 0, 0, 0, 0, 0, A | H | M | V, A | C | D | E | F | H | J | K | Q | U | V},
    /* commented out for the time being --Fara
       {
       "wraith",               true,
       0,              AFF_INFRARED,              0,
       IMM_COLD,              0,              VULN_FIRE,
       A|H|M|V|cc,        A|C|D|E|F|G|H|I|J|K|L|U
    }
    ,*/

    {"giant", false, 0, 0, 0, 0, RES_FIRE | RES_COLD, VULN_MENTAL | VULN_LIGHTNING, A | H | M | V,
     A | B | C | D | E | F | G | H | J | K},

    {"bat", false, 0, AFF_FLYING | AFF_DARK_VISION, OFF_DODGE | OFF_FAST, 0, 0, VULN_LIGHT, A | G | W,
     A | C | D | E | F | H | J | K | P},

    {"bear", false, 0, 0, OFF_CRUSH | OFF_DISARM | OFF_BERSERK, 0, RES_BASH | RES_COLD, 0, A | G | V,
     A | B | C | D | E | F | H | J | K | U | V},

    {"cat", false, 0, AFF_DARK_VISION, OFF_FAST | OFF_DODGE, 0, 0, 0, A | G | V,
     A | C | D | E | F | H | J | K | Q | U | V},

    {"centipede", false, 0, AFF_DARK_VISION, 0, 0, RES_PIERCE | RES_COLD, VULN_BASH, O | Y /* insect, snake  */,
     A | F | Q | X},

    {"dog", false, 0, 0, OFF_FAST, 0, 0, 0, A | G | V, A | C | D | E | F | H | J | K | U | V},

    {"doll", false, 0, 0, 0, IMM_MAGIC, RES_BASH | RES_LIGHT,
     VULN_SLASH | VULN_FIRE | VULN_ACID | VULN_LIGHTNING | VULN_ENERGY, E | J | M | cc, A | B | C | G | H | K},

    {"fido", false, 0, 0, OFF_DODGE | ASSIST_RACE, 0, 0, VULN_MAGIC, B | G | V, A | C | D | E | F | H | J | K | Q | V},

    {"fox", false, 0, AFF_DARK_VISION, OFF_FAST | OFF_DODGE, 0, 0, 0, A | G | V,
     A | C | D | E | F | H | J | K | Q | U | V},

    {"goblin", false, 0, AFF_INFRARED, 0, 0, RES_DISEASE, VULN_MAGIC, A | H | M | V,
     A | B | C | D | E | F | G | H | I | J | K},

    {
        "hobgoblin", false, 0, AFF_INFRARED, 0, 0, RES_DISEASE | RES_POISON, 0, A | H | M | V,
        A | B | C | D | E | F | G | H | I | J | K | Q /* includes a tail */

    },

    {"kobold", false, 0, AFF_INFRARED, 0, 0, RES_POISON, VULN_MAGIC, A | B | H | M | V,
     A | B | C | D | E | F | G | H | I | J | K | Q},

    {"lizard", false, 0, 0, 0, 0, RES_POISON, VULN_COLD, A | G | X | cc, A | C | D | E | F | H | K | Q | V},

    {"modron", false, 0, AFF_INFRARED, ASSIST_RACE | ASSIST_ALIGN,
     IMM_CHARM | IMM_DISEASE | IMM_MENTAL | IMM_HOLY | IMM_NEGATIVE, RES_FIRE | RES_COLD | RES_ACID, 0, H,
     A | B | C | G | H | J | K},

    {"pig", false, 0, 0, 0, 0, 0, 0, A | G | V, A | C | D | E | F | H | J | K},

    {"rabbit", false, 0, 0, OFF_DODGE | OFF_FAST, 0, 0, 0, A | G | V, A | C | D | E | F | H | J | K},

    {"school monster", false, ACT_NOALIGN, 0, 0, IMM_CHARM | IMM_SUMMON, 0, VULN_MAGIC, A | M | V,
     A | B | C | D | E | F | H | J | K | Q | U},

    {"snake", false, 0, 0, 0, 0, RES_POISON, VULN_COLD, A | G | R | X | Y | cc, A | D | E | F | K | L | Q | V | X},

    {"song bird", false, 0, AFF_FLYING, OFF_FAST | OFF_DODGE, 0, 0, 0, A | G | W, A | C | D | E | F | H | K | P},

    {"troll", false, 0, AFF_REGENERATION | AFF_INFRARED | AFF_DETECT_HIDDEN, OFF_BERSERK, 0, RES_CHARM | RES_BASH,
     VULN_FIRE | VULN_ACID, B | M | V, A | B | C | D | E | F | G | H | I | J | K | U | V},

    {"water fowl", false, 0, AFF_SWIM | AFF_FLYING, 0, 0, RES_DROWNING, 0, A | G | W,
     A | C | D | E | F | H | K | P | Q},

    {"wyvern", false, 0, AFF_FLYING | AFF_DETECT_INVIS | AFF_DETECT_HIDDEN, OFF_BASH | OFF_FAST | OFF_DODGE, IMM_POISON,
     0, VULN_LIGHT, B | Z | cc, A | C | D | E | F | H | J | K | P | Q | V | X},

    {nullptr, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

const struct pc_race_type pc_race_table[] = {
    {"null race", "", 0, {100, 100, 100, 100}, {""}, {13, 13, 13, 13, 13}, {18, 18, 18, 18, 18}, 0},

    /*
        {
       "race name",   short name,    points,  { class multipliers },
       { bonus skills },
       { base stats },      { max stats },    size
        },
    */

    {"human", "Human", 0, {100, 100, 100, 100}, {""}, {14, 14, 14, 14, 14}, {18, 18, 18, 18, 18}, SIZE_MEDIUM},

    {"hobbit",
     "Hobbt",
     20,
     {100, 120, 100, 120},
     {"backstab", "pick lock"},
     {13, 15, 16, 16, 14},
     {17, 19, 21, 20, 17},
     SIZE_SMALL},

    {"minotaur",
     "Mintr",
     15,
     {140, 100, 150, 100},
     {"headbutt"},
     {16, 14, 15, 11, 16},
     {21, 18, 19, 15, 20},
     SIZE_LARGE},

    {"elf",
     " Elf ",
     5,
     {100, 125, 100, 150},
     {"sneak", "hide"},
     {11, 16, 14, 16, 11},
     {16, 21, 18, 21, 15},
     SIZE_MEDIUM},

    {"half-elf", "H/Elf", 15, {125, 100, 100, 125}, {"dodge"}, {14, 15, 14, 15, 14}, {18, 20, 18, 19, 18}, SIZE_MEDIUM},

    {"dragon",
     "Dragn",
     40,
     {100, 100, 100, 100},
     {"draconian", "transportation", "bash"},
     {16, 16, 16, 16, 16},
     {21, 20, 20, 13, 21},
     SIZE_LARGE},

    {"eagle",
     "Eagle",
     10,
     {110, 100, 100, 150},
     {"fly", "lore"},
     {13, 15, 15, 13, 14},
     {17, 20, 20, 17, 18},
     SIZE_SMALL},

    {"orc",
     " Orc ",
     10,
     {100, 100, 120, 100},
     {"fast healing", "berserk"},
     {16, 12, 16, 12, 16},
     {20, 16, 20, 16, 20},
     SIZE_LARGE},

    {"dwarf",
     "Dwarf",
     5,
     {150, 100, 120, 100},
     {"haggle", "peek"},
     {16, 12, 16, 10, 16},
     {20, 16, 20, 14, 21},
     SIZE_MEDIUM},

    {"wolf",
     "Wolf ",
     20,
     {150, 150, 120, 100},
     {"berserk", "second attack"},
     {16, 11, 15, 16, 15},
     {21, 14, 19, 21, 19},
     SIZE_MEDIUM},
    /* commented out for the time being --Fara
    {
       "wraith",          "Wrait",       15,       {
          120, 100, 100, 100       }
       ,
       {
          "meditation"       }
       ,
       {
          13, 16, 16, 14, 14       }
       , {
          17, 20, 20, 16, 18       }
       , SIZE_MEDIUM
       }*/

};
