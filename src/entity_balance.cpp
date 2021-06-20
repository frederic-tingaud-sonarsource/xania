/*************************************************************************/
/*  Xania (M)ulti(U)ser(D)ungeon server source code                      */
/*  (C) 1995-2000 Xania Development Team                                 */
/*  See merc.h and README for original copyrights                        */
/*************************************************************************/

#include "AFFECT_DATA.hpp"
#include "Char.hpp"
#include "MobIndexData.hpp"
#include "merc.h"
#include "string_utils.hpp"

#include <fmt/format.h>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/view/transform.hpp>

namespace {

void objectbug(std::string_view str, OBJ_INDEX_DATA *obj) {
    log_string("obj> {} (#{}): {}", obj->short_descr, obj->vnum, str);
}

void mobbug(std::string_view str, MobIndexData *mob) {
    log_string("mob> {} (#{}): {}", mob->short_descr, mob->vnum, str);
}

/* report_object, takes an object_index_data obj and a param boot and returns the 'worth' of an
   object in points.  If boot is non-zero it will also 'BUG' these, along with any other things
   that could be wrong with the object */
int report_object(OBJ_DATA *object, int boot) {
    int averagedam, allowedaverage;
    OBJ_INDEX_DATA *obj = object->pIndexData;

    auto value =
        ranges::accumulate(obj->affected | ranges::views::transform(&AFFECT_DATA::worth), AFFECT_DATA::Value{});

    /* Weapons are allowed 1 hit and 1 dam for each point */

    auto worth =
        value.worth + obj->item_type == ITEM_WEAPON ? (value.hit + value.damage) / 2 : value.hit + value.damage;

    /* Object specific routines */

    switch (obj->item_type) {

    case ITEM_WEAPON:
        /* Calculate the damage allowed and actual */
        allowedaverage = (object->level / 2) + 4;
        if (IS_SET(obj->value[4], WEAPON_TWO_HANDS) && IS_SET(obj->wear_flags, ITEM_TWO_HANDS))
            allowedaverage += UMAX(1, (allowedaverage) / 20);
        averagedam = (obj->value[1] * obj->value[2] + obj->value[1]) / 2;
        if ((averagedam > allowedaverage) && boot) {
            objectbug("average damage too high", obj);
        }
        /* Add to worth for each weapon type */
        if (IS_SET(obj->value[4], WEAPON_FLAMING))
            worth++;
        if (IS_SET(obj->value[4], WEAPON_FROST))
            worth++;
        if (IS_SET(obj->value[4], WEAPON_VAMPIRIC))
            worth++;
        if (IS_SET(obj->value[4], WEAPON_SHARP))
            worth++;
        if (IS_SET(obj->value[4], WEAPON_VORPAL))
            worth++;
        break;

    case ITEM_POTION:
    case ITEM_PILL:
    case ITEM_SCROLL:
    case ITEM_BOMB:
    case ITEM_STAFF:
        if ((obj->value[4] > (object->level + (UMAX(5, obj->level / 10)))) && boot)
            objectbug("level of spell too high", obj);
        break;

    default: break;
    }

    if (boot && (worth > ((obj->level / 10) + 1))) {
        objectbug(fmt::format("points too high: has {} points (max should be {})", worth, ((obj->level / 10) + 1)),
                  obj);
    }
    return worth;
}

/* report_mobile - for checking those not-hard-enough mobs */

void report_mobile(MobIndexData *mob) {

    if ((mob->damage.bonus() + mob->hitroll + ((mob->damage.number() * mob->damage.type()) + mob->damage.number() / 2))
        < (mob->level * 3 / 2))
        mobbug("can't do enough damage", mob);

    if ((mob->hit.number() + mob->hit.bonus()) < (mob->level * 30))
        mobbug("has too few health points", mob);
}

}

// Report entities that have stats that are imbalanced.
void report_entity_imbalance() {
    bug("obj> **********************************************************************");
    bug("obj> **               Beginning sweep of all object in Xania             **");
    bug("obj> **********************************************************************");

    for (auto *object : object_list) {
        report_object(object, 1);
    }

    bug("obj> **********************************************************************");
    bug("obj> **                       Object sweep completed                     **");
    bug("obj> **********************************************************************");

    bug("mob> **********************************************************************");
    bug("mob> **                       Beginning mobile sweep                     **");
    bug("mob> **********************************************************************");

    for (auto *mobile : char_list) {
        report_mobile(mobile->pIndexData);
    }

    bug("mob> **********************************************************************");
    bug("mob> **                       Mobile sweep completed                     **");
    bug("mob> **********************************************************************");
}

void do_immworth(Char *ch, const char *argument) {
    OBJ_DATA *obj;
    int worth, shouldbe;

    if ((obj = get_obj_world(ch, argument)) == nullptr) {
        ch->send_line("Nothing like that in Xania.");
        return;
    }

    worth = report_object(obj, 0);
    shouldbe = ((obj->level / 10) + 1);
    if (worth == shouldbe) {
        ch->send_line("Object '{}' has {} point(s) - exactly right.", obj->pIndexData->short_descr, worth);
    } else if (worth > shouldbe) {
        ch->send_line("Object '{}' has {} point(s), {} points |Rtoo high|w.", obj->pIndexData->short_descr, worth,
                      worth - shouldbe);
    } else {
        ch->send_line("Object '{}' has {} point(s), within the {} point maximum.", obj->pIndexData->short_descr, worth,
                      shouldbe);
    }
}