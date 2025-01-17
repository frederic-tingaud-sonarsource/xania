/*************************************************************************/
/*  Xania (M)ulti(U)ser(D)ungeon server source code                      */
/*  (C) 1995-2000 Xania Development Team                                    */
/*  See the header to file: merc.h for original code copyrights          */
/*************************************************************************/

/***************************************************************************
 *  The MOBprograms have been contributed by N'Atas-ha.  Any support for   *
 *  these routines should not be expected from Merc Industries.  However,  *
 *  under no circumstances should the blame for bugs, etc be placed on     *
 *  Merc Industries.  They are not guaranteed to work on all systems due   *
 *  to their frequent use of strxxx functions.  They are also not the most *
 *  efficient way to perform their tasks, but hopefully should be in the   *
 *  easiest possible way to install and begin using. Documentation for     *
 *  such installation can be found in INSTALL.  Enjoy...         N'Atas-Ha *
 ***************************************************************************/

#include "mob_prog.hpp"
#include "AffectFlag.hpp"
#include "ArgParser.hpp"
#include "Char.hpp"
#include "CharActFlag.hpp"
#include "Logging.hpp"
#include "Object.hpp"
#include "ObjectIndex.hpp"
#include "Room.hpp"
#include "common/BitOps.hpp"
#include "db.h"
#include "handler.hpp"
#include "interp.h"
#include "string_utils.hpp"

#include <fmt/format.h>

#include <string>

using namespace std::literals;

/*
 * Local function prototypes
 */

char *mprog_next_command(char *clist);
bool mprog_seval(std::string_view lhs, std::string_view opr, std::string_view rhs);
bool mprog_veval(int lhs, std::string_view opr, int rhs);
bool mprog_do_ifchck(char *ifchck, Char *mob, const Char *actor, const Object *obj, const void *vo, Char *rndm);
char *mprog_process_if(char *ifchck, char *com_list, Char *mob, const Char *actor, const Object *obj, const void *vo,
                       Char *rndm);
void mprog_translate(char ch, char *t, Char *mob, const Char *actor, const Object *obj, const void *vo, Char *rndm);
void mprog_process_cmnd(char *cmnd, Char *mob, const Char *actor, const Object *obj, const void *vo, Char *rndm);
void mprog_driver(std::string_view com_list, Char *mob, const Char *actor, const Object *obj, const void *vo);

/***************************************************************************
 * Local function code and brief comments.
 */

/* if you dont have these functions, you damn well should... */

#ifdef DUNNO_STRSTR
char *strstr(s1, s2) const char *s1;
const char *s2;
{
    char *cp;
    int i, j = strlen(s1) - strlen(s2), k = strlen(s2);
    if (j < 0)
        return nullptr;
    for (i = 0; i <= j && strncmp(s1++, s2, k) != 0; i++)
        ;
    return (i > j) ? nullptr : (s1 - 1);
}
#endif

/* Used to get sequential lines of a multi line string (separated by "\n\r")
 * Thus its like one_argument(), but a trifle different. It is destructive
 * to the multi line string argument, and thus clist must not be shared.
 */
char *mprog_next_command(char *clist) {

    char *pointer = clist;

    while (*pointer != '\n' && *pointer != '\0')
        pointer++;
    if (*pointer == '\n')
        *pointer++ = '\0';
    if (*pointer == '\r')
        *pointer++ = '\0';

    return (pointer);
}

/* These two functions do the basic evaluation of ifcheck operators.
 *  It is important to note that the string operations are not what
 *  you probably expect.  Equality is exact and division is substring.
 *  remember that lhs has been stripped of leading space, but can
 *  still have trailing spaces so be careful when editing since:
 *  "guard" and "guard " are not equal.
 */
bool mprog_seval(std::string_view lhs, std::string_view opr, std::string_view rhs) {
    if (opr == "=="sv)
        return matches(lhs, rhs);
    if (opr == "!="sv)
        return !matches(lhs, rhs);
    if (opr == "/"sv)
        return matches_inside(rhs, lhs);
    if (opr == "!/"sv)
        return !matches_inside(rhs, lhs);

    bug("Improper MOBprog operator '{}'", opr);
    return false;
}

bool mprog_veval(int lhs, std::string_view opr, int rhs) {
    if (opr == "=="sv)
        return lhs == rhs;
    if (opr == "!="sv)
        return lhs != rhs;
    if (opr == ">"sv)
        return lhs > rhs;
    if (opr == "<"sv)
        return lhs < rhs;
    if (opr == "<="sv)
        return lhs <= rhs;
    if (opr == ">="sv)
        return lhs >= rhs;
    if (opr == "&"sv)
        return lhs & rhs;
    if (opr == "|"sv)
        return lhs | rhs;

    bug("Improper MOBprog operator '{}'", opr);
    return false;
}

/* This function performs the evaluation of the if checks.  It is
 * here that you can add any ifchecks which you so desire. Hopefully
 * it is clear from what follows how one would go about adding your
 * own. The syntax for an if check is: ifchck ( arg ) [opr val]
 * where the parenthesis are required and the opr and val fields are
 * optional but if one is there then both must be. The spaces are all
 * optional. The evaluation of the opr expressions is farmed out
 * to reduce the redundancy of the mammoth if statement list.
 * If there are errors, then return -1 otherwise return boolean 1,0
 */
bool mprog_do_ifchck(char *ifchck, Char *mob, const Char *actor, const Object *obj, const void *vo, Char *rndm) {

    char buf[MAX_INPUT_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    char opr[MAX_INPUT_LENGTH];
    char val[MAX_INPUT_LENGTH];
    auto *vict = (const Char *)vo;
    auto *v_obj = (const Object *)vo;
    char *bufpt = buf;
    char *argpt = arg;
    char *oprpt = opr;
    char *valpt = val;
    char *point = ifchck;
    int lhsvl;
    int rhsvl;

    if (*point == '\0') {
        bug("Mob: {} null ifchck", mob->mobIndex->vnum);
        return -1;
    }
    /* skip leading spaces */
    while (std::isspace(*point))
        point++;

    /* get whatever comes before the left paren.. ignore spaces */
    while (*point != '(')
        if (*point == '\0') {
            bug("Mob: {} ifchck syntax error", mob->mobIndex->vnum);
            return -1;
        } else if (std::isspace(*point))
            point++;
        else
            *bufpt++ = *point++;

    *bufpt = '\0';
    point++;

    /* get whatever is in between the parens.. ignore spaces */
    while (*point != ')')
        if (*point == '\0') {
            bug("Mob: {} ifchck syntax error", mob->mobIndex->vnum);
            return -1;
        } else if (std::isspace(*point))
            point++;
        else
            *argpt++ = *point++;

    *argpt = '\0';
    point++;

    /* check to see if there is an operator */
    while (std::isspace(*point))
        point++;
    if (*point == '\0') {
        *opr = '\0';
        *val = '\0';
    } else /* there should be an operator and value, so get them */
    {
        while (!std::isspace(*point) && !isalnum(*point))
            if (*point == '\0') {
                bug("Mob: {} ifchck operator without value", mob->mobIndex->vnum);
                return -1;
            } else
                *oprpt++ = *point++;

        *oprpt = '\0';

        /* finished with operator, skip spaces and then get the value */
        while (std::isspace(*point))
            point++;
        while (!std::isspace(*point) && *point != '\0')
            *valpt++ = *point++;
        *valpt = '\0';
    }
    bufpt = buf;
    argpt = arg;
    oprpt = opr;
    valpt = val;

    /* Ok... now buf contains the ifchck, arg contains the inside of the
     *  parentheses, opr contains an operator if one is present, and val
     *  has the value if an operator was present.
     *  So.. basically use if statements and run over all known ifchecks
     *  Once inside, use the argument and expand the lhs. Then if need be
     *  send the lhs,opr,rhs off to be evaluated.
     */

    if (!str_cmp(buf, "rand")) {
        return (number_percent() <= atoi(arg));
    }

    if (!str_cmp(buf, "ispc")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return 0;
        case 'n':
            if (actor)
                return (actor->is_pc());
            else
                return -1;
        case 't':
            if (vict)
                return (vict->is_pc());
            else
                return -1;
        case 'r':
            if (rndm)
                return (rndm->is_pc());
            else
                return -1;
        default: bug("Mob: {} bad argument to 'ispc'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "isnpc")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return 1;
        case 'n':
            if (actor)
                return actor->is_npc();
            else
                return -1;
        case 't':
            if (vict)
                return vict->is_npc();
            else
                return -1;
        case 'r':
            if (rndm)
                return rndm->is_npc();
            else
                return -1;
        default: bug("Mob: {} bad argument to 'isnpc'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "isgood")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return mob->is_good();
        case 'n':
            if (actor)
                return actor->is_good();
            else
                return -1;
        case 't':
            if (vict)
                return vict->is_good();
            else
                return -1;
        case 'r':
            if (rndm)
                return rndm->is_good();
            else
                return -1;
        default: bug("Mob: {} bad argument to 'isgood'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "isfight")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return (mob->fighting) ? 1 : 0;
        case 'n':
            if (actor)
                return (actor->fighting) ? 1 : 0;
            else
                return -1;
        case 't':
            if (vict)
                return (vict->fighting) ? 1 : 0;
            else
                return -1;
        case 'r':
            if (rndm)
                return (rndm->fighting) ? 1 : 0;
            else
                return -1;
        default: bug("Mob: {} bad argument to 'isfight'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "isimmort")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return (mob->get_trust() > LEVEL_IMMORTAL);
        case 'n':
            if (actor)
                return (actor->get_trust() > LEVEL_IMMORTAL);
            else
                return -1;
        case 't':
            if (vict)
                return (vict->get_trust() > LEVEL_IMMORTAL);
            else
                return -1;
        case 'r':
            if (rndm)
                return (rndm->get_trust() > LEVEL_IMMORTAL);
            else
                return -1;
        default: bug("Mob: {} bad argument to 'isimmort'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "ischarmed")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
            // TheMoog changed here to get rid of warning
        case 'i': return mob->is_aff_charm() ? true : false;
        case 'n':
            if (actor)
                return actor->is_aff_charm() ? true : false;
            else
                return -1;
        case 't':
            if (vict)
                return vict->is_aff_charm() ? true : false;
            else
                return -1;
        case 'r':
            if (rndm)
                return rndm->is_aff_charm() ? true : false;
            else
                return -1;
        default: bug("Mob: {} bad argument to 'ischarmed'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "isfollow")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return (mob->master != nullptr && mob->master->in_room == mob->in_room);
        case 'n':
            if (actor)
                return (actor->master != nullptr && actor->master->in_room == actor->in_room);
            else
                return -1;
        case 't':
            if (vict)
                return (vict->master != nullptr && vict->master->in_room == vict->in_room);
            else
                return -1;
        case 'r':
            if (rndm)
                return (rndm->master != nullptr && rndm->master->in_room == rndm->in_room);
            else
                return -1;
        default: bug("Mob: {} bad argument to 'isfollow'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "isaffected")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return (mob->affected_by & atoi(arg));
        case 'n':
            if (actor)
                return (actor->affected_by & atoi(arg));
            else
                return -1;
        case 't':
            if (vict)
                return (vict->affected_by & atoi(arg));
            else
                return -1;
        case 'r':
            if (rndm)
                return (rndm->affected_by & atoi(arg));
            else
                return -1;
        default: bug("Mob: {} bad argument to 'isaffected'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "hitprcnt")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->hit / mob->max_hit;
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->hit / actor->max_hit;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->hit / vict->max_hit;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->hit / rndm->max_hit;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'hitprcnt'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "inroom")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->in_room->vnum;
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->in_room->vnum;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->in_room->vnum;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->in_room->vnum;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'inroom'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "sex")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->sex.integer();
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->sex.integer();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->sex.integer();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->sex.integer();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'sex'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "position")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->position.integer();
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->position.integer();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->position.integer();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->position.integer();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'position'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "level")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->get_trust();
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->get_trust();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->get_trust();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->get_trust();
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'level'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "class")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->class_num;
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->class_num;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->class_num;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->class_num;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'class'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "goldamt")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->gold;
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                lhsvl = actor->gold;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 't':
            if (vict) {
                lhsvl = vict->gold;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'r':
            if (rndm) {
                lhsvl = rndm->gold;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'goldamt'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "objtype")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'o':
            if (obj) {
                lhsvl = magic_enum::enum_integer<ObjectType>(obj->type);
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'p':
            if (v_obj) {
                lhsvl = magic_enum::enum_integer<ObjectType>(v_obj->type);
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'objtype'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "objval0")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'o':
            if (obj) {
                lhsvl = obj->value[0];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'p':
            if (v_obj) {
                lhsvl = v_obj->value[0];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'objval0'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "objval1")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'o':
            if (obj) {
                lhsvl = obj->value[1];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'p':
            if (v_obj) {
                lhsvl = v_obj->value[1];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'objval1'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "objval2")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'o':
            if (obj) {
                lhsvl = obj->value[2];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'p':
            if (v_obj) {
                lhsvl = v_obj->value[2];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'objval2'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "objval3")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'o':
            if (obj) {
                lhsvl = obj->value[3];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        case 'p':
            if (v_obj) {
                lhsvl = v_obj->value[3];
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
        default: bug("Mob: {} bad argument to 'objval3'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "number")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i':
            lhsvl = mob->gold;
            rhsvl = atoi(val);
            return mprog_veval(lhsvl, opr, rhsvl);
        case 'n':
            if (actor) {
                if (actor->is_npc()) {
                    lhsvl = actor->mobIndex->vnum;
                    rhsvl = atoi(val);
                    return mprog_veval(lhsvl, opr, rhsvl);
                }
            } else
                return -1;
            break; // TODO this fell through (as did all) but not clear that's what was wanted
        case 't':
            if (vict) {
                if (actor->is_npc()) {
                    lhsvl = vict->mobIndex->vnum;
                    rhsvl = atoi(val);
                    return mprog_veval(lhsvl, opr, rhsvl);
                }
            } else
                return -1;
            break;
        case 'r':
            if (rndm) {
                if (actor->is_npc()) {
                    lhsvl = rndm->mobIndex->vnum;
                    rhsvl = atoi(val);
                    return mprog_veval(lhsvl, opr, rhsvl);
                }
            } else
                return -1;
            break;
        case 'o':
            if (obj) {
                lhsvl = obj->objIndex->vnum;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
            break;
        case 'p':
            if (v_obj) {
                lhsvl = v_obj->objIndex->vnum;
                rhsvl = atoi(val);
                return mprog_veval(lhsvl, opr, rhsvl);
            } else
                return -1;
            break;
        default: bug("Mob: {} bad argument to 'number'", mob->mobIndex->vnum); return -1;
        }
    }

    if (!str_cmp(buf, "name")) {
        switch (arg[1]) /* arg should be "$*" so just get the letter */
        {
        case 'i': return mprog_seval(mob->name, opr, val);
        case 'n':
            if (actor)
                return mprog_seval(actor->name, opr, val);
            else
                return -1;
        case 't':
            if (vict)
                return mprog_seval(vict->name, opr, val);
            else
                return -1;
        case 'r':
            if (rndm)
                return mprog_seval(rndm->name, opr, val);
            else
                return -1;
        case 'o':
            if (obj)
                return mprog_seval(obj->name, opr, val);
            else
                return -1;
        case 'p':
            if (v_obj)
                return mprog_seval(v_obj->name, opr, val);
            else
                return -1;
        default: bug("Mob: {} bad argument to 'name'", mob->mobIndex->vnum); return -1;
        }
    }

    /* Ok... all the ifchcks are done, so if we didnt find ours then something
     * odd happened.  So report the bug and abort the MOBprogram (return error)
     */
    bug("Mob: {} unknown ifchck", mob->mobIndex->vnum);
    return -1;
}
/* Quite a long and arduous function, this guy handles the control
 * flow part of MOBprograms.  Basicially once the driver sees an
 * 'if' attention shifts to here.  While many syntax errors are
 * caught, some will still get through due to the handling of break
 * and errors in the same fashion.  The desire to break out of the
 * recursion without catastrophe in the event of a mis-parse was
 * believed to be high. Thus, if an error is found, it is bugged and
 * the parser acts as though a break were issued and just bails out
 * at that point. I havent tested all the possibilites, so I'm speaking
 * in theory, but it is 'guaranteed' to work on syntactically correct
 * MOBprograms, so if the mud crashes here, check the mob carefully!
 */
char *mprog_process_if(char *ifchck, char *com_list, Char *mob, const Char *actor, const Object *obj, const void *vo,
                       Char *rndm) {
    char buf[MAX_INPUT_LENGTH];
    char *morebuf = nullptr;
    char *cmnd = nullptr;
    bool loopdone = false;
    bool flag = false;
    int legal;

    /* check for trueness of the ifcheck */
    if ((legal = mprog_do_ifchck(ifchck, mob, actor, obj, vo, rndm))) {
        if (legal == 1)
            flag = true;
        else
            return nullptr;
    }

    while (loopdone == false) { /*scan over any existing or statements */
        cmnd = com_list;
        com_list = mprog_next_command(com_list);
        while (std::isspace(*cmnd))
            cmnd++;
        if (*cmnd == '\0') {
            bug("Mob: {} no commands after IF/OR", mob->mobIndex->vnum);
            return nullptr;
        }
        morebuf = one_argument(cmnd, buf);
        if (!str_cmp(buf, "or")) {
            if ((legal = mprog_do_ifchck(morebuf, mob, actor, obj, vo, rndm))) {
                if (legal == 1)
                    flag = true;
                else
                    return nullptr;
            }
        } else
            loopdone = true;
    }

    if (flag)
        for (;;) /*ifcheck was true, do commands but ignore else to endif*/
        {
            if (!str_cmp(buf, "if")) {
                com_list = mprog_process_if(morebuf, com_list, mob, actor, obj, vo, rndm);
                while (std::isspace(*cmnd))
                    cmnd++;
                if (*com_list == '\0')
                    return nullptr;
                cmnd = com_list;
                com_list = mprog_next_command(com_list);
                morebuf = one_argument(cmnd, buf);
                continue;
            }
            if (!str_cmp(buf, "break"))
                return nullptr;
            if (!str_cmp(buf, "endif"))
                return com_list;
            if (!str_cmp(buf, "else")) {
                while (str_cmp(buf, "endif")) {
                    cmnd = com_list;
                    com_list = mprog_next_command(com_list);
                    while (std::isspace(*cmnd))
                        cmnd++;
                    if (*cmnd == '\0') {
                        bug("Mob: {} missing endif after else", mob->mobIndex->vnum);
                        return nullptr;
                    }
                    morebuf = one_argument(cmnd, buf);
                }
                return com_list;
            }
            mprog_process_cmnd(cmnd, mob, actor, obj, vo, rndm);
            cmnd = com_list;
            com_list = mprog_next_command(com_list);
            while (std::isspace(*cmnd))
                cmnd++;
            if (*cmnd == '\0') {
                bug("Mob: {} missing else or endif", mob->mobIndex->vnum);
                return nullptr;
            }
            morebuf = one_argument(cmnd, buf);
        }
    else /*false ifcheck, find else and do existing commands or quit at endif*/
    {
        while ((str_cmp(buf, "else")) && (str_cmp(buf, "endif"))) {
            cmnd = com_list;
            com_list = mprog_next_command(com_list);
            while (std::isspace(*cmnd))
                cmnd++;
            if (*cmnd == '\0') {
                bug("Mob: {} missing an else or endif", mob->mobIndex->vnum);
                return nullptr;
            }
            morebuf = one_argument(cmnd, buf);
        }

        /* found either an else or an endif.. act accordingly */
        if (!str_cmp(buf, "endif"))
            return com_list;
        cmnd = com_list;
        com_list = mprog_next_command(com_list);
        while (std::isspace(*cmnd))
            cmnd++;
        if (*cmnd == '\0') {
            bug("Mob: {} missing endif", mob->mobIndex->vnum);
            return nullptr;
        }
        morebuf = one_argument(cmnd, buf);

        for (;;) /*process the post-else commands until an endif is found.*/
        {
            if (!str_cmp(buf, "if")) {
                com_list = mprog_process_if(morebuf, com_list, mob, actor, obj, vo, rndm);
                while (std::isspace(*cmnd))
                    cmnd++;
                if (*com_list == '\0')
                    return nullptr;
                cmnd = com_list;
                com_list = mprog_next_command(com_list);
                morebuf = one_argument(cmnd, buf);
                continue;
            }
            if (!str_cmp(buf, "else")) {
                bug("Mob: {} found else in an else section", mob->mobIndex->vnum);
                return nullptr;
            }
            if (!str_cmp(buf, "break"))
                return nullptr;
            if (!str_cmp(buf, "endif"))
                return com_list;
            mprog_process_cmnd(cmnd, mob, actor, obj, vo, rndm);
            cmnd = com_list;
            com_list = mprog_next_command(com_list);
            while (std::isspace(*cmnd))
                cmnd++;
            if (*cmnd == '\0') {
                bug("Mob:{} missing endif in else section", mob->mobIndex->vnum);
                return nullptr;
            }
            morebuf = one_argument(cmnd, buf);
        }
    }
}

/* This routine handles the variables for command expansion.
 * If you want to add any go right ahead, it should be fairly
 * clear how it is done and they are quite easy to do, so you
 * can be as creative as you want. The only catch is to check
 * that your variables exist before you use them. At the moment,
 * using $t when the secondary target refers to an object
 * i.e. >prog_act drops~<nl>if ispc($t)<nl>sigh<nl>endif<nl>~<nl>
 * probably makes the mud crash (vice versa as well) The cure
 * would be to change act() so that vo becomes vict & v_obj.
 * but this would require a lot of small changes all over the code.
 */
void mprog_translate(char ch, char *t, Char *mob, const Char *actor, const Object *obj, const void *vo, Char *rndm) {
    // TODO: when `t` is not just a string view, we can more easily use the `Pronouns` stuff.
    static const char *he_she[] = {"it", "he", "she"};
    static const char *him_her[] = {"it", "him", "her"};
    static const char *his_her[] = {"its", "his", "her"};
    auto *vict = (const Char *)vo;
    auto *v_obj = (const Object *)vo;

    *t = '\0';
    switch (ch) {
    case 'i': one_argument(mob->name.c_str(), t); break;

    case 'I': strcpy(t, mob->short_descr.c_str()); break;

    case 'n':
        if (actor)
            if (can_see(mob, actor))
                one_argument(actor->name.c_str(), t);
        if (actor->is_pc())
            *t = toupper(*t);
        break;

    case 'N':
        if (actor) {
            if (can_see(mob, actor)) {
                if (actor->is_npc()) {
                    strcpy(t, actor->short_descr.c_str());
                } else {
                    strcpy(t, actor->name.c_str());
                    strcat(t, " ");
                    strcat(t, actor->pcdata->title.c_str());
                }
            } else
                strcpy(t, "someone");
        }
        break;

    case 't':
        if (vict) {
            if (can_see(mob, vict))
                one_argument(vict->name.c_str(), t);
        }
        if (vict->is_pc())
            *t = toupper(*t);
        break;

    case 'T':
        if (vict) {
            if (can_see(mob, vict)) {
                if (vict->is_npc()) {
                    strcpy(t, vict->short_descr.c_str());
                } else {
                    strcpy(t, vict->name.c_str());
                    strcat(t, " ");
                    strcat(t, vict->pcdata->title.c_str());
                }
            } else {
                strcpy(t, "someone");
            }
        }
        break;

    case 'r':
        if (rndm) {
            if (can_see(mob, rndm))
                one_argument(rndm->name.c_str(), t);
        }
        if (rndm->is_pc())
            *t = toupper(*t);
        break;

    case 'R':
        if (rndm) {
            if (can_see(mob, rndm)) {
                if (rndm->is_npc()) {
                    strcpy(t, rndm->short_descr.c_str());
                } else {
                    strcpy(t, rndm->name.c_str());
                    strcat(t, " ");
                    strcat(t, rndm->pcdata->title.c_str());
                }
            } else
                strcpy(t, "someone");
        }
        break;

    case 'e':
        if (actor)
            can_see(mob, actor) ? strcpy(t, he_she[actor->sex.integer()])
                                : strcpy(t, "someone"); // TODO use Sex::Type and Pronouns stuff...
        break;

    case 'm':
        if (actor)
            can_see(mob, actor) ? strcpy(t, him_her[actor->sex.integer()]) : strcpy(t, "someone");
        break;

    case 's':
        if (actor)
            can_see(mob, actor) ? strcpy(t, his_her[actor->sex.integer()]) : strcpy(t, "someone's");
        break;

    case 'E':
        if (vict)
            can_see(mob, vict) ? strcpy(t, he_she[vict->sex.integer()]) : strcpy(t, "someone");
        break;

    case 'M':
        if (vict)
            can_see(mob, vict) ? strcpy(t, him_her[vict->sex.integer()]) : strcpy(t, "someone");
        break;

    case 'S':
        if (vict)
            can_see(mob, vict) ? strcpy(t, his_her[vict->sex.integer()]) : strcpy(t, "someone's");
        break;

    case 'j': strcpy(t, he_she[mob->sex.integer()]); break;

    case 'k': strcpy(t, him_her[mob->sex.integer()]); break;

    case 'l': strcpy(t, his_her[mob->sex.integer()]); break;

    case 'J':
        if (rndm)
            can_see(mob, rndm) ? strcpy(t, he_she[rndm->sex.integer()]) : strcpy(t, "someone");
        break;

    case 'K':
        if (rndm)
            can_see(mob, rndm) ? strcpy(t, him_her[rndm->sex.integer()]) : strcpy(t, "someone");
        break;

    case 'L':
        if (rndm)
            can_see(mob, rndm) ? strcpy(t, his_her[rndm->sex.integer()]) : strcpy(t, "someone's");
        break;

    case 'o':
        if (obj)
            can_see_obj(mob, obj) ? one_argument(obj->name.c_str(), t) : strcpy(t, "something");
        break;

    case 'O':
        if (obj)
            can_see_obj(mob, obj) ? strcpy(t, obj->short_descr.c_str()) : strcpy(t, "something");
        break;

    case 'p':
        if (v_obj)
            can_see_obj(mob, v_obj) ? one_argument(v_obj->name.c_str(), t) : strcpy(t, "something");
        break;

    case 'P':
        if (v_obj)
            can_see_obj(mob, v_obj) ? strcpy(t, v_obj->short_descr.c_str()) : strcpy(t, "something");
        break;

    case 'a':
        if (obj)
            switch (obj->name.front()) {
            case 'a':
            case 'e':
            case 'i':
            case 'o':
            case 'u': strcpy(t, "an"); break;
            default: strcpy(t, "a");
            }
        break;

    case 'A':
        if (v_obj)
            switch (v_obj->name.front()) {
            case 'a':
            case 'e':
            case 'i':
            case 'o':
            case 'u': strcpy(t, "an"); break;
            default: strcpy(t, "a");
            }
        break;

    case '$': strcpy(t, "$"); break;

    default: bug("Mob: {} bad $var", mob->mobIndex->vnum); break;
    }
}

/* This procedure simply copies the cmnd to a buffer while expanding
 * any variables by calling the translate procedure.  The observant
 * code scrutinizer will notice that this is taken from act()
 */
void mprog_process_cmnd(char *cmnd, Char *mob, const Char *actor, const Object *obj, const void *vo, Char *rndm) {
    char buf[MAX_INPUT_LENGTH];
    char tmp[MAX_INPUT_LENGTH];
    char *str;
    char *i;
    char *point;

    point = buf;
    str = cmnd;

    while (*str != '\0') {
        if (*str != '$') {
            *point++ = *str++;
            continue;
        }
        str++;
        mprog_translate(*str, tmp, mob, actor, obj, vo, rndm);
        i = tmp;
        ++str;
        while ((*point = *i) != '\0')
            ++point, ++i;
    }
    *point = '\0';
    interpret(mob, buf);
}

/* The main focus of the MOBprograms.  This routine is called
 *  whenever a trigger is successful.  It is responsible for parsing
 *  the command list and figuring out what to do. However, like all
 *  complex procedures, everything is farmed out to the other guys.
 */
void mprog_driver(std::string_view com_list, Char *mob, const Char *actor, const Object *obj, const void *vo) {

    char tmpcmndlst[MAX_STRING_LENGTH];
    char buf[MAX_INPUT_LENGTH];
    char *morebuf;
    char *command_list;
    char *cmnd;
    Char *rndm = nullptr;
    int count = 0;

    if (mob->is_aff_charm())
        return;

    /* get a random visible mortal player who is in the room with the mob */
    for (auto *vch : mob->in_room->people) {
        if (vch->is_pc() && vch->level < LEVEL_IMMORTAL && can_see(mob, vch)) {
            if (number_range(0, count) == 0)
                rndm = vch;
            count++;
        }
    }
    // Take a copy of the original program because currently the mprog parsing routines
    // mutate the input.
    // TODO: this needs to be rewritten to be more string friendly and use things like line_iter().
    auto com_str = std::string(com_list);
    strncpy(tmpcmndlst, com_str.c_str(), MAX_STRING_LENGTH - 1);
    command_list = tmpcmndlst;
    cmnd = command_list;
    command_list = mprog_next_command(command_list);
    while (*cmnd != '\0') {
        morebuf = one_argument(cmnd, buf);
        if (!str_cmp(buf, "if"))
            command_list = mprog_process_if(morebuf, command_list, mob, actor, obj, vo, rndm);
        else
            mprog_process_cmnd(cmnd, mob, actor, obj, vo, rndm);
        cmnd = command_list;
        command_list = mprog_next_command(command_list);
    }
}

/***************************************************************************
 * Global function code and brief comments.
 */

/* The next two routines are the basic trigger types. Either trigger
 *  on a certain percent, or trigger on a keyword or word phrase.
 *  To see how this works, look at the various trigger routines..
 */
void mprog_wordlist_check(std::string_view arg, Char *mob, const Char *actor, const Object *obj, const void *vo,
                          const MobProgTypeFlag type) {
    for (const auto &mprg : mob->mobIndex->mobprogs) {
        if (mprg.type == type) {
            // Player message matches the phrase in the program.
            if ((mprg.arglist[0] == 'p') && (std::isspace(mprg.arglist[1]))) {
                auto prog_phrase = mprg.arglist.substr(2, mprg.arglist.length());
                if (matches_inside(arg, prog_phrase)) {
                    mprog_driver(mprg.comlist, mob, actor, obj, vo);
                    break;
                }
            } else {
                // Is any keyword in the program trigger present anywhere in the character's acted message?
                auto prog_keywords = ArgParser(mprg.arglist);
                for (const auto &prog_keyword : prog_keywords) {
                    if (matches_inside(prog_keyword, arg)) {
                        mprog_driver(mprg.comlist, mob, actor, obj, vo);
                        break;
                    }
                }
            }
        }
    }
}

void mprog_percent_check(Char *mob, Char *actor, Object *obj, void *vo, const MobProgTypeFlag type) {
    for (const auto &mprg : mob->mobIndex->mobprogs) {
        if (mprg.type == type) {
            if (!is_number(mprg.arglist)) {
                bug("mprog_percent_check in mob #{} expected number argument: {}", mob->mobIndex->vnum, mprg.arglist);
                continue;
            }
            if (number_percent() < parse_number(mprg.arglist)) {
                mprog_driver(mprg.comlist, mob, actor, obj, vo);
                if (type != MobProgTypeFlag::Greet && type != MobProgTypeFlag::AllGreet)
                    break;
            }
        }
    }
}
/* The triggers.. These are really basic, and since most appear only
 * once in the code (hmm. i think they all do) it would be more efficient
 * to substitute the code in and make the mprog_xxx_check routines global.
 * However, they are all here in one nice place at the moment to make it
 * easier to see what they look like. If you do substitute them back in,
 * make sure you remember to modify the variable names to the ones in the
 * trigger calls.
 */
void mprog_act_trigger(std::string_view buf, Char *mob, const Char *ch, const Object *obj, const void *vo) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Act)) {
        mob->mpact.emplace_back(std::string(buf), ch, obj, vo);
    }
}

void mprog_bribe_trigger(Char *mob, Char *ch, int amount) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Bribe)) {
        for (const auto &mprg : mob->mobIndex->mobprogs)
            if (mprg.type == MobProgTypeFlag::Bribe) {
                if (!is_number(mprg.arglist)) {
                    bug("mprog_bribe_trigger in mob #{} expected number argument: {}", mob->mobIndex->vnum,
                        mprg.arglist);
                    continue;
                }
                if (amount >= parse_number(mprg.arglist)) {
                    /* this function previously created a gold object and gave it to ch
                       but there is zero point - the gold transfer is handled in do_give now */
                    mprog_driver(mprg.comlist, mob, ch, nullptr, nullptr);
                    break;
                }
            }
    }
}

void mprog_death_trigger(Char *mob) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Death)) {
        mprog_percent_check(mob, nullptr, nullptr, nullptr, MobProgTypeFlag::Death);
    }
}

void mprog_entry_trigger(Char *mob) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Entry))
        mprog_percent_check(mob, nullptr, nullptr, nullptr, MobProgTypeFlag::Entry);
}

void mprog_fight_trigger(Char *mob, Char *ch) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Fight))
        mprog_percent_check(mob, ch, nullptr, nullptr, MobProgTypeFlag::Fight);
}

void mprog_give_trigger(Char *mob, Char *ch, Object *obj) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Give)) {
        for (const auto &mprg : mob->mobIndex->mobprogs) {
            if (mprg.type == MobProgTypeFlag::Give) {
                auto prog_args = ArgParser(mprg.arglist);
                const auto first_prog_arg = prog_args.shift();
                if (matches(obj->name, mprg.arglist) || matches("all", first_prog_arg)) {
                    mprog_driver(mprg.comlist, mob, ch, obj, nullptr);
                    break;
                }
            }
        }
    }
}

void mprog_greet_trigger(Char *mob) {
    for (auto *vmob : mob->in_room->people)
        if (vmob->is_npc() && mob != vmob && can_see(vmob, mob) && (vmob->fighting == nullptr) && vmob->is_pos_awake()
            && check_enum_bit(vmob->mobIndex->progtypes, MobProgTypeFlag::Greet))
            mprog_percent_check(vmob, mob, nullptr, nullptr, MobProgTypeFlag::Greet);
        else if (vmob->is_npc() && (vmob->fighting == nullptr) && vmob->is_pos_awake()
                 && check_enum_bit(vmob->mobIndex->progtypes, MobProgTypeFlag::AllGreet))
            mprog_percent_check(vmob, mob, nullptr, nullptr, MobProgTypeFlag::AllGreet);
}

void mprog_hitprcnt_trigger(Char *mob, Char *ch) {
    if (mob->is_npc() && check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::HitPercent)) {
        for (const auto &mprg : mob->mobIndex->mobprogs) {
            if (mprg.type == MobProgTypeFlag::HitPercent) {
                if (!is_number(mprg.arglist)) {
                    bug("mprog_hitprcnt_trigger in mob #{} expected number argument: {}", mob->mobIndex->vnum,
                        mprg.arglist);
                    continue;
                }
                if (100 * mob->hit / mob->max_hit < parse_number(mprg.arglist)) {
                    mprog_driver(mprg.comlist, mob, ch, nullptr, nullptr);
                    break;
                }
            }
        }
    }
}

void mprog_random_trigger(Char *mob) {
    if (check_enum_bit(mob->mobIndex->progtypes, MobProgTypeFlag::Random))
        mprog_percent_check(mob, nullptr, nullptr, nullptr, MobProgTypeFlag::Random);
}

void mprog_speech_trigger(std::string_view txt, const Char *mob) {
    for (auto *vmob : mob->in_room->people)
        if (vmob->is_npc() && (check_enum_bit(vmob->mobIndex->progtypes, MobProgTypeFlag::Speech)))
            mprog_wordlist_check(txt, vmob, mob, nullptr, nullptr, MobProgTypeFlag::Speech);
}
