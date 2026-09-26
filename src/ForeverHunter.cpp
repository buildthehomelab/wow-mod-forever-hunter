/*
 * mod-forever-hunter
 *
 * Hunter changes: Season of Discovery-style pet scaling, and WoW Forever's Lone Wolf.
 *
 * Pet scaling. Stock AzerothCore already gives hunter pets a share of the hunter's stamina, attack
 * power, armor, resistances and hit. This module adds what SoD's pet scaling has and 3.3.5
 * doesn't:
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
 * Lone Wolf. In WoW Forever it's a Marksmanship talent: 20% more damage while you don't have a
 * pet out. The 3.3.5 talent tree can't get a new talent without a client patch, so here every
 * hunter with at least 10 points in Marksmanship (what the talent needs) gets it. The bonus rides
 * on a third hidden stub, "Pet Scaling - Master Spell 01" (67552), which every hunter carries and
 * which checks every second whether it should be on. So that players can see it, it also shows
 * them "Frenzy" (37023), an NPC-only buff the client already has, whose tooltip reads "Physical
 * damage dealt is increased by 20%". That buff is for show: its own effect is zeroed, and the
 * hidden aura gives the bonus to every school of damage.
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

    constexpr uint32 SPELL_LONE_WOLF             = 67552; // Pet Scaling - Master Spell 01, renamed
    constexpr uint32 SPELL_LONE_WOLF_BUFF        = 37023; // Frenzy, shown to the player

    constexpr int32 RECALCULATE_INTERVAL = 2 * IN_MILLISECONDS;
    constexpr int32 LONE_WOLF_CHECK_INTERVAL = 1 * IN_MILLISECONDS;

    constexpr uint8 TALENT_TAB_MARKSMANSHIP = 1; // Hunter tabs: 0 Beast Mastery, 1 Marksmanship, 2 Survival

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

        bool loneWolfEnabled = true;
        uint32 loneWolfDamagePercent = 20;
        uint32 loneWolfMarksmanshipPoints = 10;
    };

    Config config;

    // Talent points the player has spent in one tree, in their active spec.
    uint32 GetTalentPointsInTab(Player const* player, uint8 tabPage)
    {
        uint32 points = 0;
        for (auto const& [spellId, talent] : player->GetTalentMap())
        {
            if (talent->State == PLAYERSPELL_REMOVED || !talent->IsInSpec(player->GetActiveSpec()))
                continue;

            TalentEntry const* talentInfo = sTalentStore.LookupEntry(talent->talentID);
            if (!talentInfo)
                continue;

            TalentTabEntry const* tab = sTalentTabStore.LookupEntry(talentInfo->TalentTab);
            if (!tab || tab->tabpage != tabPage)
                continue;

            for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
                if (talentInfo->RankID[rank] == spellId)
                {
                    points += rank + 1;
                    break;
                }
        }
        return points;
    }

    // Lone Wolf is on for a hunter with enough points in Marksmanship and no living pet out.
    bool HasLoneWolf(Player const* player)
    {
        if (!config.loneWolfEnabled || player->getClass() != CLASS_HUNTER)
            return false;

        if (Pet* pet = player->GetPet(); pet && pet->IsAlive())
            return false;

        return GetTalentPointsInTab(player, TALENT_TAB_MARKSMANSHIP) >= config.loneWolfMarksmanshipPoints;
    }

    // Show or hide the Frenzy buff. It never expires, and its own damage effect is set to 0: the
    // hidden Lone Wolf aura gives the real bonus.
    void UpdateLoneWolfBuff(Player* player, bool show)
    {
        bool const shown = player->HasAura(SPELL_LONE_WOLF_BUFF, player->GetGUID());
        if (!show)
        {
            if (shown)
                player->RemoveAurasDueToSpell(SPELL_LONE_WOLF_BUFF, player->GetGUID());
            return;
        }

        if (shown)
            return;

        Aura* buff = player->AddAura(SPELL_LONE_WOLF_BUFF, player);
        if (!buff)
            return;

        buff->SetMaxDuration(-1);
        buff->SetDuration(-1);
        if (AuraEffect* effect = buff->GetEffect(EFFECT_0))
        {
            effect->ChangeAmount(0);
            effect->SetCanBeRecalculated(false);
        }
        buff->SetNeedClientUpdateForTargets();
    }

    // The Frenzy buff mustn't be saved with the character: it would come back at login without the
    // module keeping it right, or after the module is removed. Runs once the spells are loaded.
    void MarkLoneWolfBuffUnsaved()
    {
        if (SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_LONE_WOLF_BUFF)))
            spellInfo->AttributesCu |= SPELL_ATTR0_CU_AURA_CANNOT_BE_SAVED;
    }

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

// 67552 - Pet Scaling - Master Spell 01, renamed Lone Wolf. Every hunter carries it, hidden; its
// damage bonus is 0 unless Lone Wolf is on.
class spell_hun_lone_wolf : public AuraScript
{
    PrepareAuraScript(spell_hun_lone_wolf);

    Player* GetHunter() const
    {
        Unit* owner = GetUnitOwner();
        return owner ? owner->ToPlayer() : nullptr;
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& /*canBeRecalculated*/)
    {
        amount = 0;
        if (Player* hunter = GetHunter(); hunter && HasLoneWolf(hunter))
            amount = int32(config.loneWolfDamagePercent);
    }

    void CalcPeriodic(AuraEffect const* /*aurEff*/, bool& isPeriodic, int32& amplitude)
    {
        isPeriodic = true;
        amplitude = LONE_WOLF_CHECK_INTERVAL;
    }

    // Catch pets being summoned, dismissed or killed, talent changes, spec swaps and config
    // reloads, and keep the Frenzy buff in step with the bonus.
    void HandlePeriodic(AuraEffect const* aurEff)
    {
        PreventDefaultAction();
        AuraEffect* effect = GetEffect(aurEff->GetEffIndex());
        effect->RecalculateAmount();

        // Buffs go at death; the next check after resurrecting puts Frenzy back.
        if (Player* hunter = GetHunter())
            UpdateLoneWolfBuff(hunter, effect->GetAmount() > 0 && hunter->IsAlive());
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* hunter = GetHunter())
            UpdateLoneWolfBuff(hunter, false);
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_hun_lone_wolf::CalculateAmount, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(spell_hun_lone_wolf::CalcPeriodic, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_hun_lone_wolf::HandlePeriodic, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE);
        AfterEffectRemove += AuraEffectRemoveFn(spell_hun_lone_wolf::HandleRemove, EFFECT_0, SPELL_AURA_MOD_DAMAGE_PERCENT_DONE, AURA_EFFECT_HANDLE_REAL);
    }
};

class ForeverHunterWorldScript : public WorldScript
{
public:
    ForeverHunterWorldScript() : WorldScript("ForeverHunterWorldScript") { }

    void OnBeforeWorldInitialized() override
    {
        MarkLoneWolfBuffUnsaved();
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.loneWolfEnabled            = sConfigMgr->GetOption<bool>("ForeverHunter.LoneWolf.Enable", true);
        config.loneWolfDamagePercent      = sConfigMgr->GetOption<uint32>("ForeverHunter.LoneWolf.DamagePercent", 20);
        config.loneWolfMarksmanshipPoints = sConfigMgr->GetOption<uint32>("ForeverHunter.LoneWolf.MarksmanshipPoints", 10);

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

class ForeverHunterPlayerScript : public PlayerScript
{
public:
    ForeverHunterPlayerScript() : PlayerScript("ForeverHunterPlayerScript", { PLAYERHOOK_ON_LOGIN }) { }

    // Give every hunter the hidden Lone Wolf aura. It's passive, so it stays through death and
    // isn't saved; each login adds it again. With Lone Wolf switched off it does nothing.
    void OnPlayerLogin(Player* player) override
    {
        if (player->getClass() == CLASS_HUNTER && !player->HasAura(SPELL_LONE_WOLF))
            player->AddAura(SPELL_LONE_WOLF, player);
    }
};

void AddForeverHunterScripts()
{
    new ForeverHunterWorldScript();
    new ForeverHunterPetScript();
    new ForeverHunterPlayerScript();
    RegisterSpellScript(spell_hun_pet_sod_scaling);
    RegisterSpellScript(spell_hun_pet_sod_ability_ap);
    RegisterSpellScript(spell_hun_lone_wolf);
}
