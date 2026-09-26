/*
 * mod-forever-hunter
 *
 * Hunter changes. For now that's Season of Discovery-style pet scaling on top of AzerothCore's own. Stock AzerothCore already
 * gives hunter pets a share of the hunter's stamina, attack power, armor, resistances and hit.
 * This module adds what SoD's pet scaling has and 3.3.5 doesn't:
 *
 *   - Crit: the pet gets the hunter's ranged crit chance, for auto attacks and special abilities.
 *   - Haste: the pet attacks faster by the hunter's ranged haste.
 *   - Focus: the pet's maximum focus goes up by a flat amount (51 in SoD).
 *   - Physical abilities: Claw, Bite, Smack and the other physical pet abilities get more from the
 *     pet's attack power. In stock 3.3.5 they only get 7% of it, about 1.5% of the hunter's
 *     ranged attack power, while magic ones like Lightning Breath get about 4.3%.
 *
 * The bonuses ride on two server-side spells Blizzard left as empty stubs, "Pet Scaling - Master
 * Spell 07" (67562) and "08" (67563). The core never uses them and the client doesn't know them,
 * so no client patch is needed. The module's SQL gives them their effects, and the aura script
 * below works out the amounts from the hunter's stats, refreshing every 2 seconds like the core's
 * own pet scaling.
 *
 * The ability bonus is a spell script on the physical pet abilities. It raises the ability's base
 * damage before the core adds its bonuses, so the pet's damage modifiers and crits apply to the
 * extra damage as well.
 *
 * Released under the MIT License.
 */

#include "Config.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"

#include <cmath>

namespace
{
    // Must match the SQL.
    constexpr uint32 SPELL_PET_SCALING_MASTER_07 = 67562; // Focus, pet haste
    constexpr uint32 SPELL_PET_SCALING_MASTER_08 = 67563; // Pet crit

    constexpr int32 RECALCULATE_INTERVAL = 2 * IN_MILLISECONDS;

    enum class HasteSource : uint8
    {
        Rating = 0, // Haste rating only
        All    = 1  // Everything that speeds up the hunter's ranged attacks
    };

    struct Config
    {
        bool enabled = true;
        uint32 critPercent = 100;
        uint32 hastePercent = 100;
        HasteSource hasteSource = HasteSource::Rating;
        uint32 focusBonus = 51;
        float physicalAbilityAPMultiplier = 2.0f;
    };

    Config config;

    // The hunter's ranged haste as a percentage, from the chosen sources.
    float GetRangedHaste(Player const* hunter)
    {
        if (config.hasteSource == HasteSource::Rating)
            return hunter->GetRatingBonusValue(CR_HASTE_RANGED);

        // The core keeps all ranged haste as one attack time multiplier: 0.8 means 25% faster.
        float const attackTimeMultiplier = hunter->m_modAttackSpeedPct[RANGED_ATTACK];
        if (attackTimeMultiplier <= 0.0f)
            return 0.0f;

        return (1.0f / attackTimeMultiplier - 1.0f) * 100.0f;
    }

    int32 ShareOf(float value, uint32 percent)
    {
        return std::max<int32>(0, int32(std::lround(value * percent / 100.0f)));
    }
}

// 67562 - Pet Scaling - Master Spell 07
// 67563 - Pet Scaling - Master Spell 08
class spell_hun_pet_sod_scaling : public AuraScript
{
    PrepareAuraScript(spell_hun_pet_sod_scaling);

    Player* GetHunter() const
    {
        Unit* pet = GetUnitOwner();
        if (!pet || !pet->IsHunterPet())
            return nullptr;

        Unit* owner = pet->GetOwner();
        return owner ? owner->ToPlayer() : nullptr;
    }

    void CalculateCritAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount = 0;
        if (Player* hunter = GetHunter(); hunter && config.enabled)
            amount = ShareOf(hunter->GetFloatValue(PLAYER_RANGED_CRIT_PERCENTAGE), config.critPercent);
    }

    void CalculateHasteAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount = 0;
        if (Player* hunter = GetHunter(); hunter && config.enabled)
            amount = ShareOf(GetRangedHaste(hunter), config.hastePercent);
    }

    void CalculateFocusAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount = 0;
        if (GetHunter() && config.enabled)
            amount = int32(config.focusBonus);
    }

    void CalcPeriodic(AuraEffect const* /*aurEff*/, bool& isPeriodic, int32& amplitude)
    {
        isPeriodic = true;
        amplitude = RECALCULATE_INTERVAL;
    }

    // Follow the hunter's gear, buffs and the config. Changing the amount reapplies the effect,
    // which takes the old bonus off before adding the new one.
    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        GetEffect(aurEff->GetEffIndex())->RecalculateAmount();
    }

    void Register() override
    {
        if (m_scriptSpellId == SPELL_PET_SCALING_MASTER_07)
        {
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hun_pet_sod_scaling::CalculateFocusAmount, EFFECT_ALL, SPELL_AURA_MOD_INCREASE_ENERGY);
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hun_pet_sod_scaling::CalculateHasteAmount, EFFECT_ALL, SPELL_AURA_MOD_MELEE_HASTE);
        }
        else
            DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hun_pet_sod_scaling::CalculateCritAmount, EFFECT_ALL, SPELL_AURA_MOD_CRIT_PCT);

        DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(spell_hun_pet_sod_scaling::CalcPeriodic, EFFECT_ALL, SPELL_AURA_ANY);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hun_pet_sod_scaling::HandlePeriodic, EFFECT_ALL, SPELL_AURA_ANY);
    }
};

// Claw, Bite, Smack, Gore, Monstrous Bite, Swipe, Snatch, Savage Rend, Ravage, Rake and
// Demoralizing Screech, every rank (see the SQL).
class spell_hun_pet_sod_ability_ap : public SpellScript
{
    PrepareSpellScript(spell_hun_pet_sod_ability_ap);

    // The core adds ap_bonus x the pet's attack power from spell_bonus_data (0.07 for all of
    // these). Add the rest of the multiplier to the base damage. Only hunter pets: NPCs that cast
    // the same spells are left alone.
    void AddAttackPowerBonus(SpellEffIndex /*effIndex*/)
    {
        if (!config.enabled || config.physicalAbilityAPMultiplier == 1.0f)
            return;

        Unit* pet = GetCaster();
        Unit* target = GetHitUnit();
        if (!pet || !target || !pet->IsHunterPet())
            return;

        SpellBonusEntry const* bonus = sSpellMgr->GetSpellBonusData(GetSpellInfo()->Id);
        if (!bonus || bonus->ap_bonus <= 0.0f)
            return;

        // The same attack power the core uses, including bonuses the target gives its attackers.
        float const attackPower = pet->GetTotalAttackPowerValue(BASE_ATTACK)
            + target->GetTotalAuraModifier(SPELL_AURA_MELEE_ATTACK_POWER_ATTACKER_BONUS);
        float const extra = (config.physicalAbilityAPMultiplier - 1.0f) * bonus->ap_bonus * attackPower;

        SetEffectValue(std::max<int32>(0, GetEffectValue() + int32(extra)));
    }

    void Register() override
    {
        // The core works out school damage when the spell launches at each target, not on hit.
        OnEffectLaunchTarget += SpellEffectFn(spell_hun_pet_sod_ability_ap::AddAttackPowerBonus, EFFECT_ALL, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

class ForeverHunterWorldScript : public WorldScript
{
public:
    ForeverHunterWorldScript() : WorldScript("ForeverHunterWorldScript") { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.enabled      = sConfigMgr->GetOption<bool>("ForeverHunter.PetScaling.Enable", true);
        config.critPercent  = sConfigMgr->GetOption<uint32>("ForeverHunter.PetScaling.CritPercent", 100);
        config.hastePercent = sConfigMgr->GetOption<uint32>("ForeverHunter.PetScaling.HastePercent", 100);
        config.hasteSource  = sConfigMgr->GetOption<uint32>("ForeverHunter.PetScaling.HasteSource", 0) ? HasteSource::All : HasteSource::Rating;
        config.focusBonus   = sConfigMgr->GetOption<uint32>("ForeverHunter.PetScaling.FocusBonus", 51);
        config.physicalAbilityAPMultiplier = std::max(0.0f, sConfigMgr->GetOption<float>("ForeverHunter.PetScaling.PhysicalAbilityAPMultiplier", 2.0f));
    }
};

class ForeverHunterPetScript : public PetScript
{
public:
    ForeverHunterPetScript() : PetScript("ForeverHunterPetScript", { PETHOOK_ON_INIT_STATS_FOR_LEVEL }) { }

    // Runs whenever a pet's stats are set up: when it's tamed, summoned, loaded or levels up.
    // That's where the core adds its own pet scaling auras too. They're passive, so they stay
    // through death and are never saved; the next summon adds them again.
    void OnInitStatsForLevel(Guardian* guardian, uint8 /*petlevel*/) override
    {
        if (!config.enabled || !guardian->IsHunterPet())
            return;

        for (uint32 spellId : { SPELL_PET_SCALING_MASTER_07, SPELL_PET_SCALING_MASTER_08 })
            if (!guardian->HasAura(spellId))
                guardian->AddAura(spellId, guardian);
    }
};

void AddForeverHunterScripts()
{
    new ForeverHunterWorldScript();
    new ForeverHunterPetScript();
    RegisterSpellScript(spell_hun_pet_sod_scaling);
    RegisterSpellScript(spell_hun_pet_sod_ability_ap);
}
