/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

// @tswow-begin
#include "TSCreature.h"
// @tswow-end

#include "Unit.h"
#include "Creature.h"
#include "Item.h"
#include "Pet.h"
#include "Player.h"
#include "SharedDefines.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "World.h"
#include <numeric>

// @tswow-begin move dodge_cap to top of file and remove const
float dodge_cap[MAX_CLASSES] =
{
    88.129021f,     // Warrior
    88.129021f,     // Paladin
    145.560408f,    // Hunter
    145.560408f,    // Rogue
    150.375940f,    // Priest
    88.129021f,     // DK
    145.560408f,    // Shaman
    150.375940f,    // Mage
    150.375940f,    // Warlock
    0.0f,           // ??
    116.890707f,     // Druid

    // default values for custom classes
    100,100,100,100,100,100,100,
    100,100,100,100,100,100,100,
    100,100,100,100,100,100,100,
};
// @tswow-end

// @tswow-begin move miss_cap to top of file and remove const
float miss_cap[MAX_CLASSES] =
{
    16.00f,     // Warrior //correct
    16.00f,     // Paladin //correct
    16.00f,     // Hunter  //?
    16.00f,     // Rogue   //?
    16.00f,     // Priest  //?
    16.00f,     // DK      //correct
    16.00f,     // Shaman  //?
    16.00f,     // Mage    //?
    16.00f,     // Warlock //?
    0.0f,       // ??
    16.00f,      // Druid   //?

    // default values for custom classes
    16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,
    16,16,16,16,16,16,16,
};
// @tswow-end

// @tswow-begin move m_diminishing_k to top of file and remove const
float m_diminishing_k[MAX_CLASSES] =
{
    0.9560f,  // Warrior
    0.9560f,  // Paladin
    0.9880f,  // Hunter
    0.9880f,  // Rogue
    0.9830f,  // Priest
    0.9560f,  // DK
    0.9880f,  // Shaman
    0.9830f,  // Mage
    0.9830f,  // Warlock
    0.0f,     // ??
    0.9720f,   // Druid
    // default values for custom classes
    0.98f,0.98f,0.98f,0.98f,0.98f,0.98f,0.98f,
    0.98f,0.98f,0.98f,0.98f,0.98f,0.98f,0.98f,
    0.98f,0.98f,0.98f,0.98f,0.98f,0.98f,0.98f,
};
// @tswow-end

// @tswow-begin move parry_cap to top of file and remove const
float parry_cap[MAX_CLASSES] =
{
    47.003525f,     // Warrior
    47.003525f,     // Paladin
    145.560408f,    // Hunter
    145.560408f,    // Rogue
    0.0f,           // Priest
    47.003525f,     // DK
    145.560408f,    // Shaman
    0.0f,           // Mage
    0.0f,           // Warlock
    0.0f,           // ??
    0.0f,           // Druid

    // default values for custom classes
    0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,
};
// @tswow-end

inline bool _ModifyUInt32(bool apply, uint32& baseValue, int32& amount)
{
    // If amount is negative, change sign and value of apply.
    if (amount < 0)
    {
        apply = !apply;
        amount = -amount;
    }
    if (apply)
        baseValue += amount;
    else
    {
        // Make sure we do not get uint32 overflow.
        if (amount > int32(baseValue))
            amount = baseValue;
        baseValue -= amount;
    }
    return apply;
}

/*#######################################
########                         ########
########    UNIT STAT SYSTEM     ########
########                         ########
#######################################*/

void Unit::UpdateAllResistances()
{
    for (uint8 i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateResistances(i);
}

void Unit::UpdateDamagePhysical(WeaponAttackType attType)
{
    float totalMin = 0.f;
    float totalMax = 0.f;

    float tmpMin, tmpMax;
    for (uint8 i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
    {
        CalculateMinMaxDamage(attType, false, true, tmpMin, tmpMax, i);
        totalMin += tmpMin;
        totalMax += tmpMax;
    }

    // @tswow-begin
    if (Creature* c = this->ToCreature())
    {
        // @tswow-begin
        FIRE_ID(
              c->GetCreatureTemplate()->events.id
            , Creature,OnUpdateDamagePhysical
            , TSCreature(c)
            , TSMutableNumber<float>(&totalMin)
            , TSMutableNumber<float>(&totalMax)
            , false
            , uint8(attType)
        );
        // @tswow-end
    }
    // @tswow-end

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            SetStatFloatValue(UNIT_FIELD_MINDAMAGE, totalMin);
            SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, totalMax);
            break;
        case OFF_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINOFFHANDDAMAGE, totalMin);
            SetStatFloatValue(UNIT_FIELD_MAXOFFHANDDAMAGE, totalMax);
            break;
        case RANGED_ATTACK:
            SetStatFloatValue(UNIT_FIELD_MINRANGEDDAMAGE, totalMin);
            SetStatFloatValue(UNIT_FIELD_MAXRANGEDDAMAGE, totalMax);
            break;
    }
}

/*#######################################
########                         ########
########   PLAYERS STAT SYSTEM   ########
########                         ########
#######################################*/

bool Player::UpdateStats(Stats stat)
{
    if (stat > STAT_SPIRIT)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);

    SetStat(stat, int32(value));

    if (stat == STAT_STAMINA || stat == STAT_INTELLECT || stat == STAT_STRENGTH)
    {
        Pet* pet = GetPet();
        if (pet)
            pet->UpdateStats(stat);
    }

    switch (stat)
    {
        case STAT_STRENGTH:
            UpdateShieldBlockValue();
            break;
        case STAT_AGILITY:
            UpdateArmor();
            UpdateAllCritPercentages();
            UpdateDodgePercentage();
            break;
        case STAT_STAMINA:
            UpdateMaxHealth();
            break;
        case STAT_INTELLECT:
            UpdateMaxPower(POWER_MANA);
            UpdateAllSpellCritChances();
            UpdateArmor();                                  //SPELL_AURA_MOD_RESISTANCE_OF_INTELLECT_PERCENT, only armor currently
            break;
        case STAT_SPIRIT:
            break;
        default:
            break;
    }

    if (stat == STAT_STRENGTH)
    {
        UpdateAttackPowerAndDamage(false);
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(true);
    }
    else if (stat == STAT_AGILITY)
    {
        UpdateAttackPowerAndDamage(false);
        UpdateAttackPowerAndDamage(true);
    }
    else
    {
        // Need update (exist AP from stat auras)
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(false);
        if (HasAuraTypeWithMiscvalue(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT, stat))
            UpdateAttackPowerAndDamage(true);
    }

    UpdateSpellDamageAndHealingBonus();
    UpdatePowerRegen(POWER_MANA);

    // Update ratings in exist SPELL_AURA_MOD_RATING_FROM_STAT and only depends from stat
    uint32 mask = 0;
    AuraEffectList const& modRatingFromStat = GetAuraEffectsByType(SPELL_AURA_MOD_RATING_FROM_STAT);
    for (AuraEffectList::const_iterator i = modRatingFromStat.begin(); i != modRatingFromStat.end(); ++i)
        if (Stats((*i)->GetMiscValueB()) == stat)
            mask |= (*i)->GetMiscValue();
    if (mask)
    {
        for (uint32 rating = 0; rating < MAX_COMBAT_RATING; ++rating)
            if (mask & (1 << rating))
                ApplyRatingMod(CombatRating(rating), 0, true);
    }
    return true;
}

void Player::ApplySpellPowerBonus(int32 amount, bool apply)
{
    apply = _ModifyUInt32(apply, m_baseSpellPower, amount);

    // For speed just update for client
    ApplyModUInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, amount, apply);
    for (int i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
        ApplyModUInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, amount, apply);
}

void Player::UpdateSpellDamageAndHealingBonus()
{
    // Magic damage modifiers implemented in Unit::SpellDamageBonusDone
    // This information for client side use only
    // Get healing bonus for all schools
    SetStatInt32Value(PLAYER_FIELD_MOD_HEALING_DONE_POS, SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_ALL));
    // Get damage bonus for all schools
    Unit::AuraEffectList const& modDamageAuras = GetAuraEffectsByType(SPELL_AURA_MOD_DAMAGE_DONE);
    for (uint16 i = SPELL_SCHOOL_HOLY; i < MAX_SPELL_SCHOOL; ++i)
    {
        SetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i, std::accumulate(modDamageAuras.begin(), modDamageAuras.end(), 0, [i](int32 negativeMod, AuraEffect const* aurEff)
        {
            if (aurEff->GetAmount() < 0 && aurEff->GetMiscValue() & (1 << i))
                negativeMod += aurEff->GetAmount();
            return negativeMod;
        }));
        SetStatInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + i, SpellBaseDamageBonusDone(SpellSchoolMask(1 << i)) - GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + i));
    }
}

bool Player::UpdateAllStats()
{
    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
    {
        float value = GetTotalStatValue(Stats(i));
        SetStat(Stats(i), int32(value));
    }

    UpdateArmor();
    UpdateAttackPowerAndDamage(false);
    UpdateAttackPowerAndDamage(true);
    UpdateMaxHealth();

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllRatings();
    UpdateAllCritPercentages();
    UpdateAllSpellCritChances();
    UpdateDefenseBonusesMod();
    UpdateShieldBlockValue();
    UpdateSpellDamageAndHealingBonus();
    UpdatePowerRegen(POWER_MANA);
    UpdatePowerRegen(POWER_RAGE);
    UpdatePowerRegen(POWER_ENERGY);
    UpdatePowerRegen(POWER_RUNIC_POWER);
    UpdateExpertise(BASE_ATTACK);
    UpdateExpertise(OFF_ATTACK);
    RecalculateRating(CR_ARMOR_PENETRATION);
    UpdateAllResistances();

    return true;
}

void Player::ApplySpellPenetrationBonus(int32 amount, bool apply)
{
    ApplyModInt32Value(PLAYER_FIELD_MOD_TARGET_RESISTANCE, -amount, apply);
    m_spellPenetrationItemMod += apply ? amount : -amount;
}

void Player::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));
        // @tswow-begin
        FIRE(
              Player,OnUpdateResistance
            , TSPlayer(this)
            , TSMutableNumber<float>(&value)
            , school
        );
        // @tswow-end

        Pet* pet = GetPet();
        if (pet)
            pet->UpdateResistances(school);
    }
    else
        UpdateArmor();
}

void Player::UpdateArmor()
{
    UnitMods unitMod = UNIT_MOD_ARMOR;

    float value = GetFlatModifierValue(unitMod, BASE_VALUE);    // base armor (from items)
    value *= GetPctModifierValue(unitMod, BASE_PCT);            // armor percent from items
    value += GetStat(STAT_AGILITY) * 2.0f;                      // armor bonus from stats
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE);

    //add dynamic flat mods
    AuraEffectList const& mResbyIntellect = GetAuraEffectsByType(SPELL_AURA_MOD_RESISTANCE_OF_STAT_PERCENT);
    for (AuraEffectList::const_iterator i = mResbyIntellect.begin(); i != mResbyIntellect.end(); ++i)
    {
        if ((*i)->GetMiscValue() & SPELL_SCHOOL_MASK_NORMAL)
            value += CalculatePct(GetStat(Stats((*i)->GetMiscValueB())), (*i)->GetAmount());
    }

    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    // @tswow-begin
    FIRE(
          Player,OnUpdateArmor
        , TSPlayer(this)
        , TSMutableNumber<float>(&value)
    );
    // @tswow-end
    SetArmor(int32(value));

    Pet* pet = GetPet();
    if (pet)
        pet->UpdateArmor();
}

float Player::GetHealthBonusFromStamina()
{
    float stamina = GetStat(STAT_STAMINA);
    float baseStam = std::min(20.0f, stamina);
    float moreStam = stamina - baseStam;
    // @tswow-begin
    float health = baseStam + (moreStam*10.0f);
    FIRE(Player,OnCalcStaminaHealthBonus
        , TSPlayer(this)
        , TSMutableNumber<float>(&health)
        , baseStam
        , moreStam
    );
    return health;
    // @tswow-end
}

float Player::GetManaBonusFromIntellect()
{
    float intellect = GetStat(STAT_INTELLECT);

    float baseInt = std::min(20.0f, intellect);
    float moreInt = intellect - baseInt;

    // @tswow-begin
    float mana = baseInt + (moreInt * 15.0f);
    FIRE(Player,OnCalcIntellectManaBonus
        ,TSPlayer(this)
        ,TSMutableNumber<float>(&mana)
        ,baseInt
        ,moreInt
    );
    // @tswow-end
    return mana;
}

void Player::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + GetHealthBonusFromStamina();
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);
    // @tswow-begin
    FIRE(Player,OnUpdateMaxHealth
        ,TSPlayer(this)
        ,TSMutableNumber<float>(&value)
    );
    // @tswow-end
    SetMaxHealth((uint32)value);
}

void Player::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + AsUnderlyingType(power));

    float bonusPower = (power == POWER_MANA && GetCreatePowerValue(power) > 0) ? GetManaBonusFromIntellect() : 0;

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreatePowerValue(power);
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) +  bonusPower;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);
    // @tswow-begin
    FIRE(Player,OnUpdateMaxPower
        , TSPlayer(this)
        , TSMutableNumber<float>(&value)
        , static_cast<int8>(power)
        , bonusPower
    );
    // @tswow-end
    SetMaxPower(power, uint32(std::lroundf(value)));
}

void Player::ApplyFeralAPBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseFeralAP, amount);
    UpdateAttackPowerAndDamage();
}

uint32 meleeAPFormulas[32];
uint32 rangedAPFormulas[32];

enum class ClassStatFormulaTypes: uint32 {
    MELEE =  1,
    RANGED = 2,
};

enum class ClassStatValueTypes : uint32 {
    DIMINISHING_K = 1,
    MISS_CAP      = 2,
    PARRY_CAP     = 3,
    DODGE_CAP     = 4,
    DODGE_BASE    = 5,
    CRIT_TO_DODGE = 6,
};

void LoadAPFormulas()
{
    for (int i = 0; i < 32; ++i)
    {
        meleeAPFormulas[i] = i;
        rangedAPFormulas[i] = i;
    }

    {
        QueryResult result = WorldDatabase.Query("SELECT * from class_stat_formulas;");
        if (result)
        {
            do
            {
                Field* field = result->Fetch();
                uint32 cls = field[0].GetUInt32();
                ClassStatFormulaTypes stat = ClassStatFormulaTypes(field[1].GetUInt32());
                uint32 clsOut = field[2].GetUInt32();

                if (cls >= MAX_CLASSES) {
                    continue;
                }

                switch (stat) {
                    case ClassStatFormulaTypes::MELEE:
                        meleeAPFormulas[cls] = clsOut;
                        break;
                    case ClassStatFormulaTypes::RANGED:
                        rangedAPFormulas[cls] = clsOut;
                        break;
                }
            } while(result->NextRow());
        }
    }

    {
        QueryResult result = WorldDatabase.Query("SELECT * from class_stat_values");
        if (result)
        {
            do {
                Field* field = result->Fetch();
                uint32 cls = field[0].GetUInt32()-1;
                ClassStatValueTypes stat = ClassStatValueTypes(field[1].GetUInt32());
                float value = field[2].GetFloat();

                if (cls >= MAX_CLASSES) {
                    continue;
                }

                switch (stat)
                {
                case ClassStatValueTypes::DIMINISHING_K:
                    m_diminishing_k[cls] = value;
                    break;
                case ClassStatValueTypes::DODGE_CAP:
                    dodge_cap[cls] = value;
                    break;
                case ClassStatValueTypes::MISS_CAP:
                    miss_cap[cls] = value;
                    break;
                case ClassStatValueTypes::PARRY_CAP:
                    parry_cap[cls] = value;
                    break;
                case ClassStatValueTypes::DODGE_BASE:
                    dodge_base[cls] = value;
                    break;
                case ClassStatValueTypes::CRIT_TO_DODGE:
                    crit_to_dodge[cls] = value;
                    break;
                }
            } while (result->NextRow());
        }
    }
}

void Player::UpdateAttackPowerAndDamage(bool ranged)
{
    float val2 = 0.0f;
    float level = float(GetLevel());

    AttackPowerModIndex unitMod = ranged ? RANGED_AP_MODS : MELEE_AP_MODS;

    if (ranged)
    {
        // @tswow-begin
        switch (rangedAPFormulas[GetClass()])
        // @tswow-end
        {
            case CLASS_HUNTER:
                val2 = level * 2.0f + GetStat(STAT_AGILITY) - 10.0f;
                break;
            case CLASS_ROGUE:
                val2 = level + GetStat(STAT_AGILITY) - 10.0f;
                break;
            case CLASS_WARRIOR:
                val2 = level + GetStat(STAT_AGILITY) - 10.0f;
                break;
            case CLASS_DRUID:
                switch (GetShapeshiftForm())
                {
                    case FORM_CAT:
                    case FORM_BEAR:
                    case FORM_DIREBEAR:
                        val2 = 0.0f; break;
                    default:
                        val2 = GetStat(STAT_AGILITY) - 10.0f; break;
                }
                break;
            default: val2 = GetStat(STAT_AGILITY) - 10.0f; break;
        }
        FIRE(Player,OnUpdateRangedAttackPower
            , TSPlayer(this)
            , TSMutableNumber<float>(&val2)
        );
    }
    else
    {
        // @tswow-begin
        switch (meleeAPFormulas[GetClass()])
        // @tswow-end
        {
            case CLASS_WARRIOR:
                val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            case CLASS_PALADIN:
                val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            case CLASS_DEATH_KNIGHT:
                val2 = level * 3.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            case CLASS_ROGUE:
                val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY) - 20.0f;
                break;
            case CLASS_HUNTER:
                val2 = level * 2.0f + GetStat(STAT_STRENGTH) + GetStat(STAT_AGILITY) - 20.0f;
                break;
            case CLASS_SHAMAN:
                val2 = level * 2.0f + GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                break;
            case CLASS_DRUID:
            {
                // Check if Predatory Strikes is skilled
                float levelBonus = 0.0f;
                float weaponBonus = 0.0f;
                if (IsInFeralForm())
                {
                    if (AuraEffect const* levelMod = GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_DRUID, 1563, EFFECT_0))
                        levelBonus = CalculatePct(1.0f, levelMod->GetAmount());

                    // = 0 if removing the weapon, do not calculate bonus (uses template)
                    if (m_baseFeralAP)
                    {
                        if (Item const* weapon = m_items[EQUIPMENT_SLOT_MAINHAND])
                        {
                            if (AuraEffect const* weaponMod = GetAuraEffect(SPELL_AURA_DUMMY, SPELLFAMILY_DRUID, 1563, EFFECT_1))
                            {
                                ItemTemplate const* itemTemplate = weapon->GetTemplate();
                                int32 bonusAP = itemTemplate->GetTotalAPBonus() + m_baseFeralAP;
                                weaponBonus = CalculatePct(static_cast<float>(bonusAP), weaponMod->GetAmount());
                            }
                        }
                    }
                }

                switch (GetShapeshiftForm())
                {
                    case FORM_CAT:
                        val2 = GetLevel() * levelBonus + GetStat(STAT_STRENGTH) * 2.0f + GetStat(STAT_AGILITY) - 20.0f + weaponBonus + m_baseFeralAP;
                        break;
                    case FORM_BEAR:
                    case FORM_DIREBEAR:
                        val2 = GetLevel() * levelBonus + GetStat(STAT_STRENGTH) * 2.0f - 20.0f + weaponBonus + m_baseFeralAP;
                        break;
                    case FORM_MOONKIN:
                        val2 = GetStat(STAT_STRENGTH) * 2.0f - 20.0f + m_baseFeralAP;
                        break;
                    default:
                        val2 = GetStat(STAT_STRENGTH) * 2.0f - 20.0f;
                        break;
                }
                break;
            }
            case CLASS_MAGE:
                val2 = GetStat(STAT_STRENGTH) - 10.0f;
                break;
            case CLASS_PRIEST:
                val2 = GetStat(STAT_STRENGTH) - 10.0f;
                break;
            case CLASS_WARLOCK:
                val2 = GetStat(STAT_STRENGTH) - 10.0f;
                break;
        }
        FIRE(Player,OnUpdateAttackPower
            , TSPlayer(this)
            , TSMutableNumber<float>(&val2)
        );
    }


    float baseAttackPower  = val2;
    // attack power mods are split into positive and negative field
    float attackPowerModPositive = GetAttackPowerModifierValue(unitMod, AP_MOD_POSITIVE_FLAT);
    float attackPowerModNegative = GetAttackPowerModifierValue(unitMod, AP_MOD_NEGATIVE_FLAT);

    float attackPowerMultiplier = GetAttackPowerModifierValue(unitMod, AP_MOD_PCT) - 1.0f;

    // Until we rip the bandaid and properly implement these auras, we just modify positive and negative mods accordingly
    float temp_val = 0.0f;
    //add dynamic flat mods
    if (ranged)
    {
        if ((GetClassMask() & CLASSMASK_WAND_USERS) == 0)
        {
            AuraEffectList const& mRAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_RANGED_ATTACK_POWER_OF_STAT_PERCENT);
            for (AuraEffect const* aurEff : mRAPbyStat)
            {
                temp_val = CalculatePct(GetStat(Stats(aurEff->GetMiscValue())), aurEff->GetAmount());
                if (temp_val > 0.0f)
                    attackPowerModPositive += temp_val;
                else
                    attackPowerModNegative += temp_val;
            }
        }
    }
    else
    {
        AuraEffectList const& mAPbyStat = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACK_POWER_OF_STAT_PERCENT);
        for (AuraEffect const* aurEff : mAPbyStat)
        {
            temp_val = CalculatePct(GetStat(Stats(aurEff->GetMiscValue())), aurEff->GetAmount());
            if (temp_val > 0.0f)
                attackPowerModPositive += temp_val;
            else
                attackPowerModNegative += temp_val;
        }
    }

    // applies to both, amount updated in PeriodicTick each 30 seconds
    temp_val =  GetTotalAuraModifier(SPELL_AURA_MOD_ATTACK_POWER_OF_ARMOR);
    if (temp_val > 0.0f)
        attackPowerModPositive += temp_val;
    else
        attackPowerModNegative += temp_val;

    if (ranged)
    {
        SetRangedAttackPower(int32(baseAttackPower));
        SetRangedAttackPowerModPos(int32(attackPowerModPositive));
        SetRangedAttackPowerModNeg(int32(attackPowerModNegative));
        SetRangedAttackPowerMultiplier(attackPowerMultiplier);
    }
    else
    {
        SetAttackPower(int32(baseAttackPower));
        SetAttackPowerModPos(int32(attackPowerModPositive));
        SetAttackPowerModNeg(int32(attackPowerModNegative));
        SetAttackPowerMultiplier(attackPowerMultiplier);
    }

    Pet* pet = GetPet();                                //update pet's AP
    Guardian* guardian = GetGuardianPet();
    //automatically update weapon damage after attack power modification
    if (ranged)
    {
        UpdateDamagePhysical(RANGED_ATTACK);
        if (pet && pet->IsHunterPet()) // At ranged attack change for hunter pet
            pet->UpdateAttackPowerAndDamage();
    }
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        if (CanDualWield() && haveOffhandWeapon())           //allow update offhand damage only if player knows DualWield Spec and has equipped offhand weapon
            UpdateDamagePhysical(OFF_ATTACK);
        if (GetClass() == CLASS_SHAMAN || GetClass() == CLASS_PALADIN)                      // mental quickness
            UpdateSpellDamageAndHealingBonus();

        if (pet && (pet->IsPetGhoul() || pet->IsRisenAlly())) // At melee attack power change for DK pet
            pet->UpdateAttackPowerAndDamage();

        if (guardian && guardian->IsSpiritWolf()) // At melee attack power change for Shaman feral spirit
            guardian->UpdateAttackPowerAndDamage();
    }
}

void Player::UpdateShieldBlockValue()
{
    // @tswow-begin move block and fire event
    uint32 block = GetShieldBlockValue();
    FIRE(
          Player,OnUpdateShieldBlock
        , TSPlayer(this)
        , TSMutableNumber<uint32>(&block)
    );
    SetUInt32Value(PLAYER_SHIELD_BLOCK, block);
    // @tswow-end
}

void Player::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage, uint8 damageIndex) const
{
    // Only proto damage, not affected by any mods
    if (damageIndex != 0)
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;

        if (!IsInFeralForm() && CanUseAttackType(attType))
        {
            minDamage = GetWeaponDamageRange(attType, MINDAMAGE, damageIndex);
            maxDamage = GetWeaponDamageRange(attType, MAXDAMAGE, damageIndex);
        }
        return;
    }

    UnitMods unitMod;

    switch (attType)
    {
        case BASE_ATTACK:
        default:
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    float const attackPowerMod = std::max(GetAPMultiplier(attType, normalized), 0.25f);

    float baseValue  = GetFlatModifierValue(unitMod, BASE_VALUE);
    baseValue += GetTotalAttackPowerValue(attType) / 14.0f * attackPowerMod;

    float basePct    = GetPctModifierValue(unitMod, BASE_PCT);
    float totalValue = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float totalPct   = addTotalPct ? GetPctModifierValue(unitMod, TOTAL_PCT) : 1.0f;

    float weaponMinDamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMaxDamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    // check if player is druid and in cat or bear forms
    if (IsInFeralForm())
    {
        uint8 lvl = GetLevel();
        if (lvl > 60)
            lvl = 60;

        weaponMinDamage = lvl * 0.85f * attackPowerMod;
        weaponMaxDamage = lvl * 1.25f * attackPowerMod;
    }
    else if (!CanUseAttackType(attType)) // check if player not in form but still can't use (disarm case)
    {
        // cannot use ranged/off attack, set values to 0
        if (attType != BASE_ATTACK)
        {
            minDamage = 0.f;
            maxDamage = 0.f;
            return;
        }

        weaponMinDamage = BASE_MINDAMAGE;
        weaponMaxDamage = BASE_MAXDAMAGE;
    }
    else if (attType == RANGED_ATTACK) // add ammo DPS to ranged primary damage
    {
        weaponMinDamage += GetAmmoDPS() * attackPowerMod;
        weaponMaxDamage += GetAmmoDPS() * attackPowerMod;
    }

    minDamage = ((weaponMinDamage + baseValue) * basePct + totalValue) * totalPct;
    maxDamage = ((weaponMaxDamage + baseValue) * basePct + totalValue) * totalPct;
}

void Player::UpdateDefenseBonusesMod()
{
    UpdateBlockPercentage();
    UpdateParryPercentage();
    UpdateDodgePercentage();
}

void Player::UpdateBlockPercentage()
{
    // No block
    float value = 0.0f;
    if (CanBlock())
    {
        // Base value
        value = 5.0f;
        // Increase from SPELL_AURA_MOD_BLOCK_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_BLOCK_PERCENT);
        // Increase from rating
        value += GetRatingBonusValue(CR_BLOCK);
        // Set UI display value: modify value from defense skill against same level target
        value += (int32(GetDefenseSkillValue()) - int32(GetMaxSkillValueForLevel())) * 0.04f;
    }

    // @tswow-begin
    FIRE(
          Player,OnUpdateBlockPercentage
        , TSPlayer(this)
        , TSMutableNumber<float>(&value)
    );
    // @tswow-end

    SetStatFloatValue(PLAYER_BLOCK_PERCENTAGE, std::max(0.0f, std::min(value, 100.0f)));
}

void Player::UpdateCritPercentage(WeaponAttackType attType)
{
    BaseModGroup modGroup;
    uint16 index;
    CombatRating cr;

    switch (attType)
    {
        case OFF_ATTACK:
            modGroup = OFFHAND_CRIT_PERCENTAGE;
            index = PLAYER_OFFHAND_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
        case RANGED_ATTACK:
            modGroup = RANGED_CRIT_PERCENTAGE;
            index = PLAYER_RANGED_CRIT_PERCENTAGE;
            cr = CR_CRIT_RANGED;
            break;
        case BASE_ATTACK:
        default:
            modGroup = CRIT_PERCENTAGE;
            index = PLAYER_CRIT_PERCENTAGE;
            cr = CR_CRIT_MELEE;
            break;
    }

    float value = GetBaseModValue(modGroup, FLAT_MOD) + GetBaseModValue(modGroup, PCT_MOD); // + GetRatingBonusValue(cr);

    // Modify crit from weapon skill and maximized defense skill of same level victim difference
    value += (int32(GetWeaponSkillValue(attType)) - int32(GetMaxSkillValueForLevel())) * 0.04f;

    value = std::max(0.0f, value);

    // @tswow-begin
    FIRE(
        Player,OnUpdateCrit
        , TSPlayer(this)
        , TSMutableNumber<float>(&value)
        , uint32(attType)
    );
    // @tswow-end

    SetStatFloatValue(index, std::max(0.0f, std::min(value, 100.0f)));
}

void Player::UpdateAllCritPercentages()
{
    float value = GetMeleeCritFromAgility();

    SetBaseModPctValue(CRIT_PERCENTAGE, value);
    SetBaseModPctValue(OFFHAND_CRIT_PERCENTAGE, value);
    SetBaseModPctValue(RANGED_CRIT_PERCENTAGE, value);

    UpdateCritPercentage(BASE_ATTACK);
    UpdateCritPercentage(OFF_ATTACK);
    UpdateCritPercentage(RANGED_ATTACK);
}

// @tswow-begin move m_diminishing_k to top of file
// @tswow-end

// helper function
float CalculateDiminishingReturns(float const (&capArray)[MAX_CLASSES], uint8 playerClass, float nonDiminishValue, float diminishValue)
{
    //  1     1     k              cx
    // --- = --- + --- <=> x' = --------
    //  x'    c     x            x + ck

    // where:
    // k  is m_diminishing_k for that class
    // c  is capArray for that class
    // x  is chance before DR (diminishValue)
    // x' is chance after DR (our result)

    uint32 const classIdx = playerClass - 1;

    float const k = m_diminishing_k[classIdx];
    float const c = capArray[classIdx];

    float result = c * diminishValue / (diminishValue + c * k);
    result += nonDiminishValue;
    return result;
}

// @tswow-begin move miss_cap to top of file
// @tswow-end

float Player::GetMissPercentageFromDefense() const
{
    return (int32(GetSkillValue(SKILL_DEFENSE)) - int32(GetMaxSkillValueForLevel())) * 0.04f;
}

// @tswow-begin move parry-cap to top of file
// @tswow-end

void Player::UpdateParryPercentage()
{
    // No parry
    float value = 0.0f;
    if (CanParry())
    {
        // Base parry
        value  = 5.0f;
        // Parry from SPELL_AURA_MOD_PARRY_PERCENT aura
        value += GetTotalAuraModifier(SPELL_AURA_MOD_PARRY_PERCENT);
        // Parry from rating
        value += GetRatingBonusValue(CR_PARRY);
        // Set UI display value: modify value from defense skill against same level target
        value += (int32(GetDefenseSkillValue()) - int32(GetMaxSkillValueForLevel())) * 0.04f;
    }

    // @tswow-begin
    FIRE(
          Player,OnUpdateParryPercentage
        , TSPlayer(this)
        , TSMutableNumber<float>(&value)
    );

    // @tswow-end
    SetStatFloatValue(PLAYER_PARRY_PERCENTAGE, value);
}

// @tswow-begin move dodge_cap to top of file
// @tswow-end

// Base static dodge values in percentages (%)
static const float PLAYER_BASE_DODGE[MAX_CLASSES] =
{
    0.0000f, // [0]  <Unused>
    0.7500f, // [1]  Warrior
    0.6520f, // [2]  Paladin
    -5.4500f, // [3]  Hunter
    -0.5900f, // [4]  Rogue
    3.1830f, // [5]  Priest
    1.1400f, // [6]  DK
    1.6700f, // [7]  Shaman
    3.4575f, // [8]  Mage
    2.0110f, // [9]  Warlock
    0.0000f, // [10] <Unused>
    -1.8700f, // [11] Druid
};

void Player::UpdateDodgePercentage()
{
    // Base dodge
    float value = (GetClass() < MAX_CLASSES) ? PLAYER_BASE_DODGE[GetClass()] : 0.0f;
    // Dodge from agility
    value += GetDodgeFromAgility(GetStat(STAT_AGILITY));
    // Dodge from SPELL_AURA_MOD_DODGE_PERCENT aura
    value += GetTotalAuraModifier(SPELL_AURA_MOD_DODGE_PERCENT);
    // Set UI display value: modify value from defense skill against same level target
    value += (int32(GetDefenseSkillValue()) - int32(GetMaxSkillValueForLevel())) * 0.04f;

    value = value < 0.0f ? 0.0f : value;

    // @tswow-begin
    FIRE(
          Player,OnUpdateDodgePercentage
        , TSPlayer(this)
        , TSMutableNumber<float>(&value)
    );
    // @tswow-end

    SetStatFloatValue(PLAYER_DODGE_PERCENTAGE, std::max(0.0f, std::min(value, 100.0f)));
}

void Player::UpdateSpellCritChance(uint32 school)
{
    // For normal school set zero crit chance
    if (school == SPELL_SCHOOL_NORMAL)
    {
        SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1, 0.0f);
        return;
    }
    // For others recalculate it from:
    float crit = 0.0f;
    // Crit from Intellect
    crit += GetSpellCritFromIntellect();
    // Increase crit from SPELL_AURA_MOD_SPELL_CRIT_CHANCE
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_CRIT_CHANCE);
    // Increase crit from SPELL_AURA_MOD_CRIT_PCT
    crit += GetTotalAuraModifier(SPELL_AURA_MOD_CRIT_PCT);
    // Increase crit by school from SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL
    crit += GetTotalAuraModifierByMiscMask(SPELL_AURA_MOD_SPELL_CRIT_CHANCE_SCHOOL, 1<<school);
    // Increase crit from spell crit ratings
    crit += GetRatingBonusValue(CR_CRIT_SPELL);

    // @tswow-begin
    FIRE(
          Player,OnUpdateSpellCrit
        , TSPlayer(this)
        , TSMutableNumber<float>(&crit)
        , school
    );
    // @tswow-end
    // Store crit value
    SetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + school, std::max(0.0f, std::min(crit, 100.0f)));
}

void Player::UpdateArmorPenetration(int32 amount)
{
    // Store Rating Value
    // @tswow-begin
    FIRE(
          Player,OnUpdateArmorPenetration
        , TSPlayer(this)
        , TSMutableNumber<int32>(&amount)
    );
    // @tswow-end
    SetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + AsUnderlyingType(CR_ARMOR_PENETRATION), amount);
}

void Player::UpdateMeleeHitChances()
{
    m_modMeleeHitChance = GetRatingBonusValue(CR_HIT_MELEE);
    // @tswow-begin
    FIRE(
          Player,OnUpdateMeleeHitChances
        , TSPlayer(this)
        , TSMutableNumber<float>(&m_modMeleeHitChance)
    );
    // @tswow-end
}

void Player::UpdateRangedHitChances()
{
    m_modRangedHitChance = GetRatingBonusValue(CR_HIT_RANGED);
    // @tswow-begin
    FIRE(
        Player,OnUpdateRangedHitChances
        , TSPlayer(this)
        , TSMutableNumber<float>(&m_modRangedHitChance)
    );
    // @tswow-end
}

void Player::UpdateSpellHitChances()
{
    m_modSpellHitChance = (float)GetTotalAuraModifier(SPELL_AURA_MOD_SPELL_HIT_CHANCE);
    m_modSpellHitChance += GetRatingBonusValue(CR_HIT_SPELL);
    // @tswow-begin
    FIRE(
        Player,OnUpdateSpellHitChances
        , TSPlayer(this)
        , TSMutableNumber<float>(&m_modSpellHitChance)
    );
    // @tswow-end
}

void Player::UpdateAllSpellCritChances()
{
    for (int i = SPELL_SCHOOL_NORMAL; i < MAX_SPELL_SCHOOL; ++i)
        UpdateSpellCritChance(i);
}

void Player::UpdateExpertise(WeaponAttackType attack)
{
    if (attack == RANGED_ATTACK)
        return;

    int32 expertise = int32(GetRatingBonusValue(CR_EXPERTISE));

    Item const* weapon = GetWeaponForAttack(attack, true);
    expertise += GetTotalAuraModifier(SPELL_AURA_MOD_EXPERTISE, [weapon](AuraEffect const* aurEff) -> bool
    {
        return aurEff->GetSpellInfo()->IsItemFitToSpellRequirements(weapon);
    });

    if (expertise < 0)
        expertise = 0;

    // @tswow-begin
    FIRE(
          Player,OnUpdateExpertise
        , TSPlayer(this)
        , TSMutableNumber<int32>(&expertise)
        , uint32(attack)
        , TSItem(const_cast<Item*>(weapon))
    );
    // @tswow-end

    switch (attack)
    {
        case BASE_ATTACK:
            SetUInt32Value(PLAYER_EXPERTISE, expertise);
            break;
        case OFF_ATTACK:
            SetUInt32Value(PLAYER_OFFHAND_EXPERTISE, expertise);
            break;
        default:
            break;
    }
}

void Player::ApplyManaRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseManaRegen, amount);
    UpdatePowerRegen(POWER_MANA);
}

void Player::ApplyHealthRegenBonus(int32 amount, bool apply)
{
    _ModifyUInt32(apply, m_baseHealthRegen, amount);
}

static std::pair<float, Optional<Rates>> const powerRegenInfo[MAX_POWERS] =
{
    { 0.f,      RATE_POWER_MANA             }, // POWER_MANA
    { -12.5f,   RATE_POWER_RAGE_LOSS        }, // POWER_RAGE,           -1.25 rage per second
    { 0.f,      std::nullopt                }, // POWER_FOCUS
    { 10.f,     RATE_POWER_ENERGY           }, // POWER_ENERGY,         +10 energy per second
    { 0.f,      std::nullopt                }, // POWER_HAPPINESS
    { 0.f,      std::nullopt                }, // POWER_RUNE
    { -12.5f,   RATE_POWER_RUNICPOWER_LOSS  }  // POWER_RUNIC_POWER,    -1.25 runic power per second
};

void Player::UpdatePowerRegen(Powers power)
{
    if (power == POWER_HEALTH || power >= MAX_POWERS)
        return;

    float result_regen              = 0.f; // Out-of-combat / without last mana use effect
    float result_regen_interrupted  = 0.f; // In combat / with last mana use effect
    float modifier                  = 1.f; // Config rate or any other modifiers

    /// @todo possible use of miscvalueb instead of amount
    if (HasAuraTypeWithValue(SPELL_AURA_PREVENT_REGENERATE_POWER, power))
    {
        SetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER + AsUnderlyingType(power), power == POWER_ENERGY ? -10.f : 0.f);
        SetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER + AsUnderlyingType(power), power == POWER_ENERGY ? -10.f : 0.f);
        return;
    }

    switch (power)
    {
        case POWER_MANA:
        {
            float power_regen = OCTRegenMPPerSpirit();

            // Apply PCT bonus from SPELL_AURA_MOD_POWER_REGEN_PERCENT aura on spirit base regen
            power_regen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, POWER_MANA);

            // Mana regen from SPELL_AURA_MOD_POWER_REGEN aura
            float power_regen_mp5 = (GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, POWER_MANA)) / 5.0f;

            // Get bonus from SPELL_AURA_MOD_MANA_REGEN_FROM_STAT aura
            AuraEffectList const& regenAura = GetAuraEffectsByType(SPELL_AURA_MOD_MANA_REGEN_FROM_STAT);
            for (AuraEffectList::const_iterator i = regenAura.begin(); i != regenAura.end(); ++i)
                power_regen_mp5 += GetStat(Stats((*i)->GetMiscValue())) * (*i)->GetAmount() / 500.0f;

            // Set regen rate in cast state apply only on spirit based regen
            int32 modManaRegenInterrupt = GetTotalAuraModifier(SPELL_AURA_MOD_MANA_REGEN_INTERRUPT);
            if (modManaRegenInterrupt > 100)
                modManaRegenInterrupt = 100;

            // @tswow-begin
            FIRE(Player,OnUpdateManaRegen
                , TSPlayer(this)
                , TSMutableNumber<float>(&power_regen)
                , TSMutableNumber<float>(&power_regen_mp5)
                , TSMutableNumber<int32>(&modManaRegenInterrupt)
            );
            // @tswow-end

            result_regen                = power_regen_mp5 + power_regen;
            result_regen_interrupted    = power_regen_mp5 + CalculatePct(power_regen, modManaRegenInterrupt);

            break;
        }
        case POWER_RAGE:
        case POWER_ENERGY:
        case POWER_RUNIC_POWER:
        {
            result_regen                = powerRegenInfo[AsUnderlyingType(power)].first;
            result_regen_interrupted    = 0.f;

            result_regen *= GetTotalAuraMultiplierByMiscValue(SPELL_AURA_MOD_POWER_REGEN_PERCENT, AsUnderlyingType(power));
            result_regen_interrupted += static_cast<float>(GetTotalAuraModifierByMiscValue(SPELL_AURA_MOD_POWER_REGEN, AsUnderlyingType(power))) / 5.f;

            if (power != POWER_RUNIC_POWER) // Butchery requires combat
                result_regen += result_regen_interrupted;
            break;
        }
        default:
            break;
    }

    if (powerRegenInfo[AsUnderlyingType(power)].second.has_value())
        modifier *= sWorld->getRate(powerRegenInfo[AsUnderlyingType(power)].second.value()); // Config rate

    result_regen                *= modifier;
    result_regen_interrupted    *= modifier;

    // Unit fields contain an offset relative to the base power regeneration.
    if (power != POWER_MANA)
        result_regen -= powerRegenInfo[AsUnderlyingType(power)].first;

    if (power == POWER_ENERGY)
        result_regen_interrupted = result_regen;

    SetFloatValue(UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER + AsUnderlyingType(power), result_regen);
    SetFloatValue(UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER + AsUnderlyingType(power), result_regen_interrupted);
}

float Player::GetPowerRegen(Powers power) const
{
    if (power == POWER_HEALTH || power >= MAX_POWERS)
        return 0.f;

    bool interrupted =  HasAuraType(SPELL_AURA_INTERRUPT_REGEN) ||
                        (power == POWER_MANA && IsUnderLastManaUseEffect()) ||
                        (power != POWER_MANA && IsInCombat());

    float regen = GetFloatValue((interrupted ? UNIT_FIELD_POWER_REGEN_INTERRUPTED_FLAT_MODIFIER : UNIT_FIELD_POWER_REGEN_FLAT_MODIFIER) + AsUnderlyingType(power));
    if (power != POWER_MANA)
        regen += (power == POWER_ENERGY || !interrupted) ? powerRegenInfo[AsUnderlyingType(power)].first : 0.f;

    return regen;
}

void Player::UpdateRuneRegen(RuneType rune)
{
    if (rune >= NUM_RUNE_TYPES)
        return;

    uint32 cooldown = 0;

    for (uint32 i = 0; i < MAX_RUNES; ++i)
        if (GetBaseRune(i) == rune)
        {
            cooldown = GetRuneBaseCooldown(i);
            break;
        }

    if (cooldown <= 0)
        return;

    float regen = float(1 * IN_MILLISECONDS) / float(cooldown);
    // @tswow-begin
    FIRE(
          Player,OnUpdateRuneRegen
        , TSPlayer(this)
        , TSMutableNumber<float>(&regen)
        , uint32(rune)
    );
    // @tswow-end
    SetFloatValue(PLAYER_RUNE_REGEN_1 + uint8(rune), regen);
}

void Player::_ApplyAllStatBonuses()
{
    SetCanModifyStats(false);

    _ApplyAllAuraStatMods();
    _ApplyAllItemMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

void Player::_RemoveAllStatBonuses()
{
    SetCanModifyStats(false);

    _RemoveAllItemMods();
    _RemoveAllAuraStatMods();

    SetCanModifyStats(true);

    UpdateAllStats();
}

/*#######################################
########                         ########
########    MOBS STAT SYSTEM     ########
########                         ########
#######################################*/

bool Creature::UpdateStats(Stats /*stat*/)
{
    return true;
}

bool Creature::UpdateAllStats()
{
    UpdateMaxHealth();
    UpdateAttackPowerAndDamage();
    UpdateAttackPowerAndDamage(true);

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();

    return true;
}

void Creature::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));
        SetResistance(SpellSchools(school), int32(value));
        // @tswow-begin
        FIRE_ID(
              GetCreatureTemplate()->events.id
            , Creature,OnUpdateResistance
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , false
            , school
        );
        // @tswow-end
    }
    else
        UpdateArmor();
}

void Creature::UpdateArmor()
{
    float value = GetTotalAuraModValue(UNIT_MOD_ARMOR);
    // @tswow-begin
    FIRE_ID(
        GetCreatureTemplate()->events.id
        , Creature,OnUpdateArmor
        , TSCreature(this)
        , TSMutableNumber<float>(&value)
        , false
    );
    // @tswow-end
    SetArmor(int32(value));
}

void Creature::UpdateMaxHealth()
{
    float value = GetTotalAuraModValue(UNIT_MOD_HEALTH);
    // @tswow-begin
    FIRE_ID(
          GetCreatureTemplate()->events.id
        , Creature,OnUpdateMaxHealth
        , TSCreature(this)
        , TSMutableNumber<float>(&value)
        , false
    );
    // @tswow-end
    SetMaxHealth(uint32(value));
}

void Creature::UpdateMaxPower(Powers power)
{
    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + AsUnderlyingType(power));

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreatePowerValue(power);
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE);
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);
    // @tswow-begin
    FIRE_ID(
        GetCreatureTemplate()->events.id
        , Creature,OnUpdateMaxPower
        , TSCreature(this)
        , TSMutableNumber<float>(&value)
        , false
        , uint8(power)
    );
    // @tswow-end
    SetMaxPower(power, uint32(std::lroundf(value)));
}

void Creature::UpdateAttackPowerAndDamage(bool ranged)
{
    AttackPowerModIndex unitMod = ranged ? RANGED_AP_MODS : MELEE_AP_MODS;

    float baseAttackPower = ranged ? m_BaseRangedAttackPower : m_BaseAttackPower;

    float attackPowerModPositive = GetAttackPowerModifierValue(unitMod, AP_MOD_POSITIVE_FLAT);
    float attackPowerModNegative = GetAttackPowerModifierValue(unitMod, AP_MOD_NEGATIVE_FLAT);
    float attackPowerMultiplier = GetAttackPowerModifierValue(unitMod, AP_MOD_PCT) - 1.0f;

    // Commenting out until hook is changed to accept both AP Mods
    // @tswow-begin
    // FIRE_ID(
    //     GetCreatureTemplate()->events.id
    //     , Creature,OnUpdateAttackPowerDamage
    //     , TSCreature(this)
    //     , TSMutableNumber<float>(&baseAttackPower)
    //     , TSMutableNumber<float>(&attackPowerMod)
    //     , TSMutableNumber<float>(&attackPowerMultiplier)
    //     , false
    //     , ranged
    // );
    // @tswow-end

    if (ranged)
    {
        SetRangedAttackPower(int32(baseAttackPower));
        SetRangedAttackPowerModPos(int32(attackPowerModPositive));
        SetRangedAttackPowerModNeg(int32(attackPowerModNegative));
        SetRangedAttackPowerMultiplier(attackPowerMultiplier);
    }
    else
    {
        SetAttackPower(int32(baseAttackPower));
        SetAttackPowerModPos(int32(attackPowerModPositive));
        SetAttackPowerModNeg(int32(attackPowerModNegative));
        SetAttackPowerMultiplier(attackPowerMultiplier);
    }

    // automatically update weapon damage after attack power modification
    if (ranged)
        UpdateDamagePhysical(RANGED_ATTACK);
    else
    {
        UpdateDamagePhysical(BASE_ATTACK);
        UpdateDamagePhysical(OFF_ATTACK);
    }
}

void Creature::CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, bool addTotalPct, float& minDamage, float& maxDamage, uint8 damageIndex /*= 0*/) const
{
    // creatures only have one damage
    if (damageIndex != 0)
    {
        minDamage = 0.f;
        maxDamage = 0.f;
        return;
    }

    float variance = 1.0f;
    UnitMods unitMod;
    switch (attType)
    {
        case BASE_ATTACK:
        default:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_MAINHAND;
            break;
        case OFF_ATTACK:
            variance = GetCreatureTemplate()->BaseVariance;
            unitMod = UNIT_MOD_DAMAGE_OFFHAND;
            break;
        case RANGED_ATTACK:
            variance = GetCreatureTemplate()->RangeVariance;
            unitMod = UNIT_MOD_DAMAGE_RANGED;
            break;
    }

    if (attType == OFF_ATTACK && !haveOffhandWeapon())
    {
        minDamage = 0.0f;
        maxDamage = 0.0f;
        return;
    }

    float weaponMinDamage = GetWeaponDamageRange(attType, MINDAMAGE);
    float weaponMaxDamage = GetWeaponDamageRange(attType, MAXDAMAGE);

    if (!CanUseAttackType(attType)) // disarm case
    {
        weaponMinDamage = 0.0f;
        weaponMaxDamage = 0.0f;
    }

    float attackPower      = GetTotalAttackPowerValue(attType);
    float attackSpeedMulti = GetAPMultiplier(attType, normalized);
    float baseValue        = GetFlatModifierValue(unitMod, BASE_VALUE) + (attackPower / 14.0f) * variance;
    float basePct          = GetPctModifierValue(unitMod, BASE_PCT) * attackSpeedMulti;
    float totalValue       = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float totalPct         = addTotalPct ? GetPctModifierValue(unitMod, TOTAL_PCT) : 1.0f;
    float dmgMultiplier    = GetCreatureTemplate()->ModDamage; // = ModDamage * _GetDamageMod(rank);

    minDamage = ((weaponMinDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;
    maxDamage = ((weaponMaxDamage + baseValue) * dmgMultiplier * basePct + totalValue) * totalPct;
}

/*#######################################
########                         ########
########    PETS STAT SYSTEM     ########
########                         ########
#######################################*/

#define ENTRY_IMP               416
#define ENTRY_VOIDWALKER        1860
#define ENTRY_SUCCUBUS          1863
#define ENTRY_FELHUNTER         417
#define ENTRY_FELGUARD          17252
#define ENTRY_WATER_ELEMENTAL   510
#define ENTRY_TREANT            1964
#define ENTRY_FIRE_ELEMENTAL    15438
#define ENTRY_GHOUL             26125
#define ENTRY_BLOODWORM         28017

bool Guardian::UpdateStats(Stats stat)
{
    if (stat >= MAX_STATS)
        return false;

    // value = ((base_value * base_pct) + total_value) * total_pct
    float value  = GetTotalStatValue(stat);
    //ApplyStatBuffMod(stat, m_statFromOwner[stat], false);
    float ownersBonus = 0.0f;

    Unit* owner = GetOwner();
    // Handle Death Knight Glyphs and Talents
    float mod = 0.75f;
    if ((IsPetGhoul() || IsRisenAlly()) && (stat == STAT_STAMINA || stat == STAT_STRENGTH))
    {
        if (stat == STAT_STAMINA)
            mod = 0.3f; // Default Owner's Stamina scale
        else
            mod = 0.7f; // Default Owner's Strength scale

        // Check just if owner has Ravenous Dead since it's effect is not an aura
        AuraEffect const* aurEff = owner->GetAuraEffect(SPELL_AURA_MOD_TOTAL_STAT_PERCENTAGE, SPELLFAMILY_DEATHKNIGHT, 3010, 0);
        if (aurEff)
        {
            SpellInfo const* spellInfo = aurEff->GetSpellInfo();                                                // Then get the SpellProto and add the dummy effect value
            AddPct(mod, spellInfo->GetEffect(EFFECT_1).CalcValue());                                            // Ravenous Dead edits the original scale
        }
        // Glyph of the Ghoul
        aurEff = owner->GetAuraEffect(58686, 0);
        if (aurEff)
            mod += CalculatePct(1.0f, aurEff->GetAmount());                                                    // Glyph of the Ghoul adds a flat value to the scale mod
        ownersBonus = float(owner->GetStat(stat)) * mod;
        value += ownersBonus;
    }
    else if (stat == STAT_STAMINA)
    {
        if (owner->GetClass() == CLASS_WARLOCK && IsPet())
        {
            ownersBonus = CalculatePct(owner->GetStat(STAT_STAMINA), 30);
            value += ownersBonus;
        }
        else
        {
            mod = 0.45f;
            if (IsPet())
            {
                PetSpellMap::const_iterator itr = (ToPet()->m_spells.find(62758)); // Wild Hunt rank 1
                if (itr == ToPet()->m_spells.end())
                    itr = ToPet()->m_spells.find(62762);                            // Wild Hunt rank 2

                if (itr != ToPet()->m_spells.end())                                 // If pet has Wild Hunt
                {
                    SpellInfo const* spellInfo = sSpellMgr->AssertSpellInfo(itr->first); // Then get the SpellProto and add the dummy effect value
                    AddPct(mod, spellInfo->GetEffect(EFFECT_0).CalcValue());
                }
            }
            ownersBonus = float(owner->GetStat(stat)) * mod;
            value += ownersBonus;
        }
        // @epoch-start
        FIRE_ID(
            GetCreatureTemplate()->events.id
            , Creature, OnUpdateStamina
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , true
        );
        // @epoch-end
    }
                                                            //warlock's and mage's pets gain 30% of owner's intellect
    else if (stat == STAT_INTELLECT)
    {
        if (owner->GetClass() == CLASS_WARLOCK || owner->GetClass() == CLASS_MAGE)
        {
            ownersBonus = CalculatePct(owner->GetStat(stat), 30);
            value += ownersBonus;
        }
        // @epoch-start
        FIRE_ID(
            GetCreatureTemplate()->events.id
            , Creature, OnUpdateIntellect
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , true
        );
        // @epoch-end
    }

    // @epoch-start
    else if (stat == STAT_STRENGTH)
    {
        FIRE_ID(
            GetCreatureTemplate()->events.id
            , Creature, OnUpdateStrength
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , true
        );
        //if (IsPetGhoul())
        //    value += float(owner->GetStat(stat)) * 0.3f;
    }

    else if (stat == STAT_AGILITY)
    {
        FIRE_ID(
            GetCreatureTemplate()->events.id
            , Creature, OnUpdateAgility
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , true
        );
    }

    else if (stat == STAT_SPIRIT)
    {
        FIRE_ID(
            GetCreatureTemplate()->events.id
            , Creature, OnUpdateSpirit
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , true
        );
    }
    // @epoch-end

    SetStat(stat, int32(value));
    m_statFromOwner[stat] = ownersBonus;
    UpdateStatBuffMod(stat);

    switch (stat)
    {
        case STAT_STRENGTH:         UpdateAttackPowerAndDamage();        break;
        case STAT_AGILITY:          UpdateArmor();                       break;
        case STAT_STAMINA:          UpdateMaxHealth();                   break;
        case STAT_INTELLECT:        UpdateMaxPower(POWER_MANA);          break;
        case STAT_SPIRIT:
        default:
            break;
    }

    return true;
}

bool Guardian::UpdateAllStats()
{
    UpdateMaxHealth();

    for (uint8 i = STAT_STRENGTH; i < MAX_STATS; ++i)
        UpdateStats(Stats(i));

    for (uint8 i = POWER_MANA; i < MAX_POWERS; ++i)
        UpdateMaxPower(Powers(i));

    UpdateAllResistances();

    return true;
}

void Guardian::UpdateResistances(uint32 school)
{
    if (school > SPELL_SCHOOL_NORMAL)
    {
        float value  = GetTotalAuraModValue(UnitMods(UNIT_MOD_RESISTANCE_START + school));

        // hunter and warlock pets gain 40% of owner's resistance
        if (IsPet())
            value += float(CalculatePct(m_owner->GetResistance(SpellSchools(school)), 40));

        // @tswow-begin
        FIRE_ID(
            GetCreatureTemplate()->events.id
            , Creature,OnUpdateResistance
            , TSCreature(this)
            , TSMutableNumber<float>(&value)
            , true
            , school
        );
        // @tswow-end

        SetResistance(SpellSchools(school), int32(value));
    }
    else
        UpdateArmor();
}

void Guardian::UpdateArmor()
{
    float value = 0.0f;
    float bonus_armor = 0.0f;
    UnitMods unitMod = UNIT_MOD_ARMOR;

    // hunter and warlock pets gain 35% of owner's armor value
    if (IsPet())
        bonus_armor = float(CalculatePct(m_owner->GetArmor(), 35));

    value  = GetFlatModifierValue(unitMod, BASE_VALUE);
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetStat(STAT_AGILITY) * 2.0f;
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + bonus_armor;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    // @tswow-begin
    FIRE_ID(
        GetCreatureTemplate()->events.id
        , Creature,OnUpdateArmor
        , TSCreature(this)
        , TSMutableNumber<float>(&value)
        , true
    );
    // @tswow-end

    SetArmor(int32(value));
}

void Guardian::UpdateMaxHealth()
{
    UnitMods unitMod = UNIT_MOD_HEALTH;
    float stamina = GetStat(STAT_STAMINA) - GetCreateStat(STAT_STAMINA);

    float multiplicator;
    switch (GetEntry())
    {
        case ENTRY_IMP:         multiplicator = 5.9f;   break;
        case ENTRY_VOIDWALKER:  multiplicator = 5.1f;  break;
        case ENTRY_SUCCUBUS:    multiplicator = 4.2f;   break;
        case ENTRY_FELHUNTER:   multiplicator = 4.4f;   break;
        case ENTRY_FELGUARD:    multiplicator = 5.1f;  break;
        case ENTRY_BLOODWORM:   multiplicator = 1.0f;   break;
        default:                multiplicator = 10.0f;  break;
    }

    float value = GetFlatModifierValue(unitMod, BASE_VALUE) + GetCreateHealth();
    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + stamina * multiplicator;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    // @tswow-begin
    FIRE_ID(
        GetCreatureTemplate()->events.id
        , Creature,OnUpdateMaxHealth
        , TSCreature(this)
        , TSMutableNumber<float>(&value)
        , true
    );
    // @tswow-end

    SetMaxHealth((uint32)value);
}

void Guardian::UpdateMaxPower(Powers power)
{
    if (GetPowerType() != power)
        return;

    UnitMods unitMod = UnitMods(UNIT_MOD_POWER_START + AsUnderlyingType(power));

    float intellect = (power == POWER_MANA) ? GetStat(STAT_INTELLECT) - GetCreateStat(STAT_INTELLECT) : 0.0f;
    float multiplicator = 15.0f;

    switch (GetEntry())
    {
        case ENTRY_IMP:         multiplicator = 4.7f;  break;
        case ENTRY_VOIDWALKER:  multiplicator = 10.7f;  break;
        case ENTRY_SUCCUBUS:    multiplicator = 10.7f;  break;
        case ENTRY_FELHUNTER:   multiplicator = 10.8f;  break;
        case ENTRY_FELGUARD:    multiplicator = 10.7f;  break;
        default:                multiplicator = 15.0f;  break;
    }

    float value = GetFlatModifierValue(unitMod, BASE_VALUE);
    if (IsPet() || IsGuardian())
        value += GetCreatePowerValue(power);

    value *= GetPctModifierValue(unitMod, BASE_PCT);
    value += GetFlatModifierValue(unitMod, TOTAL_VALUE) + intellect * multiplicator;
    value *= GetPctModifierValue(unitMod, TOTAL_PCT);

    // @tswow-begin
    FIRE_ID(
          GetCreatureTemplate()->events.id
        , Creature,OnUpdateMaxPower
        , TSCreature(this)
        , TSMutableNumber<float>(&value)
        , true
        , int8(power)
    );
    // @tswow-end

    SetMaxPower(power, uint32(value));
}

void Guardian::UpdateAttackPowerAndDamage(bool ranged)
{
    if (ranged)
        return;

    float val = 0.0f;
    float bonusAP = 0.0f;
    AttackPowerModIndex unitMod = MELEE_AP_MODS;

    if (GetEntry() == ENTRY_IMP)                                   // imp's attack power
        val = GetStat(STAT_STRENGTH) - 10.0f;
    else
        val = 2 * GetStat(STAT_STRENGTH) - 20.0f;

    Unit* owner = GetOwner();
    if (owner && owner->GetTypeId() == TYPEID_PLAYER)
    {
        if (IsHunterPet())                      //hunter pets benefit from owner's attack power
        {
            float mod = 1.0f;                                                 //Hunter contribution modifier
            if (IsPet())
            {
                PetSpellMap::const_iterator itr = ToPet()->m_spells.find(62758);    //Wild Hunt rank 1
                if (itr == ToPet()->m_spells.end())
                    itr = ToPet()->m_spells.find(62762);                            //Wild Hunt rank 2

                if (itr != ToPet()->m_spells.end())                                 // If pet has Wild Hunt
                {
                    SpellInfo const* sProto = sSpellMgr->AssertSpellInfo(itr->first); // Then get the SpellProto and add the dummy effect value
                    mod += CalculatePct(1.0f, sProto->GetEffect(EFFECT_1).CalcValue());
                }
            }

            bonusAP = owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.22f * mod;
            /** @epoch-start */
            //if (AuraEffect* aurEff = owner->GetAuraEffectOfRankedSpell(34453, EFFECT_1, owner->GetGUID())) // Animal Handler
            //{
            //    AddPct(bonusAP, aurEff->GetAmount());
            //    AddPct(val, aurEff->GetAmount());
            //}
            /** @epoch-end */
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(RANGED_ATTACK) * 0.1287f * mod));
        }
        else if (IsPetGhoul() || IsRisenAlly()) //ghouls benefit from deathknight's attack power (may be summon pet or not)
        {
            bonusAP = owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.22f;
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(BASE_ATTACK) * 0.1287f));
        }
        else if (IsSpiritWolf()) //wolf benefit from shaman's attack power
        {
            float dmg_multiplier = 0.31f;
            if (m_owner->GetAuraEffect(63271, 0)) // Glyph of Feral Spirit
                dmg_multiplier = 0.61f;
            bonusAP = owner->GetTotalAttackPowerValue(BASE_ATTACK) * dmg_multiplier;
            SetBonusDamage(int32(owner->GetTotalAttackPowerValue(BASE_ATTACK) * dmg_multiplier));
        }
        //demons benefit from warlocks shadow or fire damage
        else if (IsPet())
        {
            int32 fire  = owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + AsUnderlyingType(SPELL_SCHOOL_FIRE)) - owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + AsUnderlyingType(SPELL_SCHOOL_FIRE));
            int32 shadow = owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + AsUnderlyingType(SPELL_SCHOOL_SHADOW)) - owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + AsUnderlyingType(SPELL_SCHOOL_SHADOW));
            int32 maximum  = (fire > shadow) ? fire : shadow;
            if (maximum < 0)
                maximum = 0;
            SetBonusDamage(int32(maximum * 0.15f));
            bonusAP = maximum * 0.57f;
        }
        //water elementals benefit from mage's frost damage
        else if (GetEntry() == ENTRY_WATER_ELEMENTAL)
        {
            int32 frost = owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + AsUnderlyingType(SPELL_SCHOOL_FROST)) - owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + AsUnderlyingType(SPELL_SCHOOL_FROST));
            if (frost < 0)
                frost = 0;
            SetBonusDamage(int32(frost * 0.4f));
        }
    }

    //in BASE_VALUE of UNIT_MOD_ATTACK_POWER for creatures we store data of meleeattackpower field in DB
    float baseAttackPower  = val + bonusAP;

    // attack power mods are split into positive and negative field
    float attackPowerModPositive = GetAttackPowerModifierValue(unitMod, AP_MOD_POSITIVE_FLAT);
    float attackPowerModNegative = GetAttackPowerModifierValue(unitMod, AP_MOD_NEGATIVE_FLAT);

    float attackPowerMultiplier = GetAttackPowerModifierValue(unitMod, AP_MOD_PCT) - 1.0f;

    // Commenting out until hook is changed to accept both AP Mods
    // @tswow-begin
    // FIRE_ID(
    //     GetCreatureTemplate()->events.id
    //     , Creature,OnUpdateAttackPowerDamage
    //     , TSCreature(this)
    //     , TSMutableNumber<float>(&baseAttackPower)
    //     , TSMutableNumber<float>(&attPowerMod)
    //     , TSMutableNumber<float>(&attackPowerMultiplier)
    //     , true
    //     , ranged
    // );
    // @tswow-end

    SetAttackPower(int32(baseAttackPower));
    SetAttackPowerModPos(int32(attackPowerModPositive));
    SetAttackPowerModNeg(int32(attackPowerModNegative));
    SetAttackPowerMultiplier(attackPowerMultiplier);

    //automatically update weapon damage after attack power modification
    UpdateDamagePhysical(BASE_ATTACK);
}

void Guardian::UpdateDamagePhysical(WeaponAttackType attType)
{
    if (attType > BASE_ATTACK)
        return;

    float bonusDamage = 0.0f;
    if (m_owner->GetTypeId() == TYPEID_PLAYER)
    {
        //force of nature
        if (GetEntry() == ENTRY_TREANT)
        {
            int32 spellDmg = m_owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + AsUnderlyingType(SPELL_SCHOOL_NATURE)) - m_owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + AsUnderlyingType(SPELL_SCHOOL_NATURE));
            if (spellDmg > 0)
                bonusDamage = spellDmg * 0.09f;
        }
        //greater fire elemental
        else if (GetEntry() == ENTRY_FIRE_ELEMENTAL)
        {
            int32 spellDmg = m_owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_POS + AsUnderlyingType(SPELL_SCHOOL_FIRE)) - m_owner->GetInt32Value(PLAYER_FIELD_MOD_DAMAGE_DONE_NEG + AsUnderlyingType(SPELL_SCHOOL_FIRE));
            if (spellDmg > 0)
                bonusDamage = spellDmg * 0.4f;
        }
    }

    UnitMods unitMod = UNIT_MOD_DAMAGE_MAINHAND;

    float att_speed = float(GetAttackTime(BASE_ATTACK))/1000.0f;

    float base_value  = GetFlatModifierValue(unitMod, BASE_VALUE) + GetTotalAttackPowerValue(attType) / 14.0f * att_speed + bonusDamage;
    float base_pct    = GetPctModifierValue(unitMod, BASE_PCT);
    float total_value = GetFlatModifierValue(unitMod, TOTAL_VALUE);
    float total_pct   = GetPctModifierValue(unitMod, TOTAL_PCT);

    float weapon_mindamage = GetWeaponDamageRange(BASE_ATTACK, MINDAMAGE);
    float weapon_maxdamage = GetWeaponDamageRange(BASE_ATTACK, MAXDAMAGE);

    float mindamage = ((base_value + weapon_mindamage) * base_pct + total_value) * total_pct;
    float maxdamage = ((base_value + weapon_maxdamage) * base_pct + total_value) * total_pct;

    //  Pet's base damage changes depending on happiness
    if (IsHunterPet())
    {
        switch (ToPet()->GetHappinessState())
        {
            case HAPPY:
                // 125% of normal damage
                mindamage = mindamage * 1.25f;
                maxdamage = maxdamage * 1.25f;
                break;
            case CONTENT:
                // 100% of normal damage, nothing to modify
                break;
            case UNHAPPY:
                // 75% of normal damage
                mindamage = mindamage * 0.75f;
                maxdamage = maxdamage * 0.75f;
                break;
        }
    }

    /// @todo: remove this
    Unit::AuraEffectList const& mDummy = GetAuraEffectsByType(SPELL_AURA_MOD_ATTACKSPEED);
    for (Unit::AuraEffectList::const_iterator itr = mDummy.begin(); itr != mDummy.end(); ++itr)
    {
        switch ((*itr)->GetSpellInfo()->Id)
        {
            case 61682:
            case 61683:
                AddPct(mindamage, -(*itr)->GetAmount());
                AddPct(maxdamage, -(*itr)->GetAmount());
                break;
            default:
                break;
        }
    }

    // @tswow-begin
    FIRE_ID(
          GetCreatureTemplate()->events.id
        , Creature,OnUpdateDamagePhysical
        , TSCreature(this)
        , TSMutableNumber<float>(&mindamage)
        , TSMutableNumber<float>(&maxdamage)
        , true
        , uint8(attType)
    );
    // @tswow-end

    SetStatFloatValue(UNIT_FIELD_MINDAMAGE, mindamage);
    SetStatFloatValue(UNIT_FIELD_MAXDAMAGE, maxdamage);
}

void Guardian::SetBonusDamage(int32 damage)
{
    // @epoch-start
    FIRE_ID(
        GetCreatureTemplate()->events.id
        , Creature, OnUpdateSpellPower
        , TSCreature(this)
        , TSMutableNumber<int32>(&damage)
        , true
    );
    // @epoch-end

    m_bonusSpellDamage = damage;
    if (GetOwner()->GetTypeId() == TYPEID_PLAYER)
        GetOwner()->SetUInt32Value(PLAYER_PET_SPELL_POWER, damage);
}
