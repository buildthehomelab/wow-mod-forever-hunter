/*
 * mod-forever-hunter
 *
 * Hunter changes: Season of Discovery-style pet scaling, WoW Forever's Lone Wolf and Summon
 * Hawk, and pets that don't start a second fight in a dungeon.
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
 * hidden aura gives the bonus to every school of damage. Like the talent it stands in for, it's
 * only for Marksmanship: by default that tree must also have the most points.
 *
 * Summon Hawk. WoW Forever's Beast Mastery talent: a hawk attacks your target for 18 seconds, two
 * at most, on a 6 second cooldown shared with Arcane Shot. Every Beast Mastery hunter (15+ points,
 * the main tree) learns "Swoop" (51919), a hawk-icon spell the client has that nothing uses, as the
 * button. On the server Swoop no longer charges: it flies to the target and a hawk guardian
 * (creature 9500300, the Fjord Hawk's model) appears above it and attacks. The hawk scales with
 * the hunter's ranged attack power and gets Unleashed Fury and Ferocity, as in WoW Forever.
 *
 * Pet hold. The core sends a pet at whatever the client names in its pet command, which isn't
 * always the mob the hunter meant: a target still selected, or a mob behind the one under the
 * cursor. In a dungeon that is often the next pack. So in a dungeon or raid, while the hunter,
 * the pet or anyone of the group in that instance is in combat, a pet command aimed at a mob
 * that isn't in combat is dropped before the core sees it.
 *
 * Shadowstrike and Thunderstrike. The Molten Core polearm pair becomes a hunter weapon: Agility
 * on both forms (see the SQL), and the procs come from ranged hits instead of melee swings.
 * Thunderstrike's ranged hits can cast its Chain Lightning; Shadowstrike's make the hunter's pet
 * drain life from the target, which heals the pet. A hidden server-only aura (90360) on every
 * character does the proccing; it does nothing unless one of the two is in the main hand.
 *
 * Released under the MIT License.
 */

#include "CharmInfo.h"
#include "Chat.h"
#include "Config.h"
#include "CreatureAI.h"
#include "DataMap.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Group.h"
#include "Item.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Opcodes.h"
#include "Pet.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SpellAuraEffects.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "TemporarySummon.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <list>
#include <vector>

namespace
{
    // Must match the SQL.
    constexpr uint32 SPELL_PET_SCALING_MASTER_07 = 67562; // Focus, pet haste
    constexpr uint32 SPELL_PET_SCALING_MASTER_08 = 67563; // Pet crit

    constexpr uint32 SPELL_LONE_WOLF             = 67552; // Pet Scaling - Master Spell 01, renamed
    constexpr uint32 SPELL_LONE_WOLF_BUFF        = 37023; // Frenzy, shown to the player

    constexpr uint32 SPELL_SUMMON_HAWK           = 51919; // Swoop, unused: the Summon Hawk button
    constexpr uint32 SPELL_ARCANE_SHOT           = 3044;  // Rank 1; shares its cooldown with Summon Hawk
    constexpr uint32 SPELL_HUNTER_PET_SCALING_04 = 61017; // The core's hit and expertise scaling
    constexpr uint32 NPC_HAWK                    = 9500300;
    constexpr uint32 SUMMON_PROPERTIES_GUARDIAN  = 61;    // Ally guardian, no pet bar

    constexpr uint32 SPELL_SHADOWSTRIKE_RANGED   = 90360; // Hidden ranged proc aura, server-only
    constexpr uint32 SPELL_DRAIN_LIFE            = 21170; // Shadowstrike's stock proc, 100-180
    constexpr uint32 SPELL_CHAIN_LIGHTNING       = 21179; // Thunderstrike's stock proc, 150-250
    constexpr uint32 ITEM_SHADOWSTRIKE           = 17074;
    constexpr uint32 ITEM_THUNDERSTRIKE          = 17223;
    constexpr uint32 SPELL_RANGE_100_YARDS       = 6;     // SpellRange.dbc

    // Beast Mastery talents that WoW Forever lets hawks share: 3% damage and 2% crit per rank.
    constexpr std::array<uint32, 5> TALENT_UNLEASHED_FURY = { 19616, 19617, 19618, 19619, 19620 };
    constexpr std::array<uint32, 5> TALENT_FEROCITY       = { 19598, 19599, 19600, 19601, 19602 };

    // CMSG_PET_ACTION: uint64 pet GUID, uint32 action (its id, and its type in the top byte),
    // uint64 target GUID.
    constexpr std::size_t PET_ACTION_DATA_POS   = 8;
    constexpr std::size_t PET_ACTION_TARGET_POS = PET_ACTION_DATA_POS + 4;
    constexpr std::size_t PET_ACTION_SIZE       = PET_ACTION_TARGET_POS + 8;

    // A hunter hammering the pet attack key gets the "won't start a new fight" line this often.
    constexpr Milliseconds PET_HOLD_NOTICE_INTERVAL = 2s;

    // When a hunter was last told the pet held back, and about which mob. Kept on the player.
    struct PetHoldNotice : public DataMap::Base
    {
        ObjectGuid target;
        Milliseconds sent = 0ms;
    };

    constexpr int32 RECALCULATE_INTERVAL = 2 * IN_MILLISECONDS;
    constexpr int32 LONE_WOLF_CHECK_INTERVAL = 1 * IN_MILLISECONDS;

    // Hunter talent tabs, as TalentTab.dbc numbers them.
    constexpr uint8 TALENT_TAB_BEAST_MASTERY = 0;
    constexpr uint8 TALENT_TAB_MARKSMANSHIP  = 1;

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
        bool loneWolfRequireMainTree = true;

        bool hawkEnabled = true;
        uint32 hawkBeastMasteryPoints = 15;
        bool hawkRequireMainTree = true;
        uint32 hawkDuration = 18000;
        uint32 hawkCooldown = 6000;
        uint32 hawkMaxActive = 2;
        uint32 hawkManaCostPercent = 5;
        uint32 hawkAttackPowerPercent = 30;

        bool petHoldEnabled = true;

        bool shadowstrikeEnabled = true;
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

    // Stands in for a WoW Forever talent: enough points in the tree, and optionally the tree has to
    // be the one with the most points, so only that spec gets it.
    bool HasTalentLike(Player const* player, uint8 tabPage, uint32 points, bool requireMainTree)
    {
        if (player->getClass() != CLASS_HUNTER)
            return false;

        if (requireMainTree && player->GetMostPointsTalentTree() != tabPage)
            return false;

        return GetTalentPointsInTab(player, tabPage) >= points;
    }

    // Lone Wolf is on for a Marksmanship hunter with no living pet out.
    bool HasLoneWolf(Player const* player)
    {
        if (!config.loneWolfEnabled)
            return false;

        if (Pet* pet = player->GetPet(); pet && pet->IsAlive())
            return false;

        return HasTalentLike(player, TALENT_TAB_MARKSMANSHIP, config.loneWolfMarksmanshipPoints,
            config.loneWolfRequireMainTree);
    }

    bool ShouldKnowSummonHawk(Player const* player)
    {
        return config.hawkEnabled && HasTalentLike(player, TALENT_TAB_BEAST_MASTERY,
            config.hawkBeastMasteryPoints, config.hawkRequireMainTree);
    }

    // Teach or remove Summon Hawk so it matches the hunter's talents, spec and the config.
    void UpdateSummonHawkSpell(Player* player)
    {
        bool const shouldKnow = ShouldKnowSummonHawk(player);
        bool const knows = player->HasSpell(SPELL_SUMMON_HAWK);

        if (shouldKnow && !knows)
            player->learnSpell(SPELL_SUMMON_HAWK);
        else if (!shouldKnow && knows)
            player->removeSpell(SPELL_SUMMON_HAWK, SPEC_MASK_ALL, false);
    }

    uint32 GetTalentRank(Player const* player, std::array<uint32, 5> const& ranks)
    {
        for (uint8 rank = ranks.size(); rank > 0; --rank)
            if (player->HasTalent(ranks[rank - 1], player->GetActiveSpec()))
                return rank;
        return 0;
    }

    uint32 GetHawkManaCost(Player const* player)
    {
        return CalculatePct(player->GetCreateMana(), config.hawkManaCostPercent);
    }

    // Put Summon Hawk, and with arcaneShot every Arcane Shot rank the hunter knows, on the shared
    // cooldown, and tell the client, which has no idea the two are linked.
    void StartSharedCooldown(Player* player, bool arcaneShot)
    {
        if (!config.hawkCooldown)
            return;

        std::vector<uint32> spells;
        if (player->HasSpell(SPELL_SUMMON_HAWK))
            spells.push_back(SPELL_SUMMON_HAWK);
        if (arcaneShot)
            for (SpellInfo const* rank = sSpellMgr->GetSpellInfo(SPELL_ARCANE_SHOT); rank; rank = rank->GetNextRankSpell())
                if (player->HasSpell(rank->Id))
                    spells.push_back(rank->Id);

        if (spells.empty())
            return;

        WorldPacket data(SMSG_SPELL_COOLDOWN, 8 + 1 + spells.size() * 8);
        data << player->GetGUID();
        data << uint8(SPELL_COOLDOWN_FLAG_NONE);
        for (uint32 spellId : spells)
        {
            player->AddSpellCooldown(spellId, 0, config.hawkCooldown);
            data << uint32(spellId);
            data << uint32(config.hawkCooldown);
        }
        player->SendDirectMessage(&data);
    }

    // Swoop (51919) was never given to anything: a charge plus fixed damage, with a hawk icon and a
    // bird sound. The client only needs it as a button. On the server it becomes a plain dummy at
    // the target, so the hunter doesn't charge, and the script sends the hawk instead.
    void ApplySummonHawkSpellChanges()
    {
        SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_SUMMON_HAWK));
        if (!spellInfo)
            return;

        spellInfo->Effects[EFFECT_0].Effect = SPELL_EFFECT_DUMMY;
        spellInfo->Effects[EFFECT_1].Effect = SpellEffects(0); // no effect
        spellInfo->AttributesEx7 &= ~SPELL_ATTR7_ATTACK_ON_CHARGE_TO_UNIT;
    }

    // The hunter, the pet or someone of the group in the same instance is in combat. A hunter
    // who hasn't hit anything yet isn't in combat while the rest of the group already is, and
    // that is when the pet gets sent.
    bool IsGroupFighting(Player* hunter, Pet const* pet)
    {
        if (hunter->IsInCombat() || pet->IsInCombat())
            return true;

        if (Group* group = hunter->GetGroup())
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                if (Player* member = itr->GetSource())
                    if (member != hunter && member->IsInMap(hunter) && member->IsInCombat())
                        return true;

        return false;
    }

    // A mob that would be a new fight: alive, attackable and fighting nobody. A critter starts
    // none. A totem or another summon is never in combat itself, so it's in the fight when its
    // master is.
    bool IsOutsideTheFight(Player const* hunter, Unit const* target)
    {
        Creature const* creature = target->ToCreature();
        if (!creature || !creature->IsAlive() || creature->IsInCombat() || creature->IsCritter()
            || !hunter->IsValidAttackTarget(creature))
            return false;

        Unit const* master = creature->GetCharmerOrOwner();
        if (!master)
            if (TempSummon const* summon = creature->ToTempSummon())
                master = summon->GetSummonerUnit();

        return !master || !master->IsInCombat();
    }

    // Tell the hunter why the pet didn't go, once per mob and then every couple of seconds.
    void SendPetHoldNotice(Player* hunter, Unit const* target)
    {
        PetHoldNotice* notice = hunter->CustomData.GetDefault<PetHoldNotice>("ForeverHunterPetHoldNotice");
        Milliseconds now = GameTime::GetGameTimeMS();
        if (notice->target == target->GetGUID() && now - notice->sent < PET_HOLD_NOTICE_INTERVAL)
            return;

        notice->target = target->GetGUID();
        notice->sent = now;
        ChatHandler(hunter->GetSession()).SendNotification("Your pet won't start a new fight: {} is not in combat.", target->GetName());
    }

    // Whether this pet command would start a second fight: an attack, or one of the pet's
    // abilities that it uses on an enemy, aimed at a mob that isn't in combat, in a dungeon or
    // raid, while the group is fighting. Tells the hunter why the pet didn't go.
    bool IsPetSentOutsideTheFight(WorldSession* session, WorldPacket const& packet)
    {
        if (packet.size() < PET_ACTION_SIZE)
            return false;

        Player* hunter = session->GetPlayer();
        if (!hunter || !hunter->GetMap()->IsDungeon())
            return false;

        // Follow, stay, the stances and abilities without a target name nobody.
        ObjectGuid targetGuid(packet.read<uint64>(PET_ACTION_TARGET_POS));
        if (!targetGuid)
            return false;

        uint32 data = packet.read<uint32>(PET_ACTION_DATA_POS);
        switch (UNIT_ACTION_BUTTON_TYPE(data))
        {
            case ACT_COMMAND:
                if (UNIT_ACTION_BUTTON_ACTION(data) != COMMAND_ATTACK)
                    return false;
                break;
            case ACT_PASSIVE:
            case ACT_DISABLED:
            case ACT_ENABLED:
            {
                // An ability can carry the hunter's target without being used on it (Dash,
                // Cower). Only one the pet uses on an enemy sends the pet at that target.
                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(UNIT_ACTION_BUTTON_ACTION(data));
                if (!spellInfo || spellInfo->IsPositive() || !spellInfo->NeedsExplicitUnitTarget())
                    return false;
                break;
            }
            default:
                return false;
        }

        Pet* pet = hunter->GetPet();
        if (!pet || !pet->IsHunterPet() || pet->GetGUID() != ObjectGuid(packet.read<uint64>(0)))
            return false;

        if (!IsGroupFighting(hunter, pet))
            return false;

        // A pet already on its way to this mob keeps going: the pull was started out of combat.
        Unit* target = ObjectAccessor::GetUnit(*hunter, targetGuid);
        if (!target || target == pet->GetVictim() || !IsOutsideTheFight(hunter, target))
            return false;

        SendPetHoldNotice(hunter, target);
        return true;
    }

    // Set up a new hawk's stats. Called from the guardian's stat setup, before the core adds it all
    // up. Like a hunter pet's: weapon damage from its level, plus a share of the hunter's ranged
    // attack power, the hunter's hit (the core's own pet hit scaling), and Unleashed Fury and
    // Ferocity as WoW Forever does.
    void SetUpHawk(Guardian* hawk, Player* hunter)
    {
        uint8 const level = hawk->GetLevel();
        hawk->SetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE, float(level - level / 4));
        hawk->SetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE, float(level + level / 4));

        float const attackPower = hunter->GetTotalAttackPowerValue(RANGED_ATTACK) * config.hawkAttackPowerPercent / 100.0f;
        hawk->HandleStatFlatModifier(UNIT_MOD_ATTACK_POWER, TOTAL_VALUE, attackPower, true);

        if (uint32 rank = GetTalentRank(hunter, TALENT_UNLEASHED_FURY))
            hawk->ApplyStatPctModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, float(rank * 3));

        hawk->AddAura(SPELL_HUNTER_PET_SCALING_04, hawk);

        // Creatures only get crit from auras; the pet crit stub carries it. Its script would reset
        // it to 0 for anything that isn't a hunter pet, so fix the amount.
        if (uint32 rank = GetTalentRank(hunter, TALENT_FEROCITY))
            if (Aura* crit = hawk->AddAura(SPELL_PET_SCALING_MASTER_08, hawk))
                if (AuraEffect* effect = crit->GetEffect(EFFECT_1))
                {
                    effect->ChangeAmount(int32(rank * 2));
                    effect->SetCanBeRecalculated(false);
                }
    }

    // Damage meters (Recount, Skada, Details) only credit a guardian's damage to its owner once the
    // combat log has shown a SPELL_SUMMON for it, which the client makes from a spell's summon
    // effect. Swoop's effect is a dummy here and the script summons the hawk itself, so send the
    // log entry a summon spell would: Swoop, one summon effect, one target, the hawk.
    void SendHawkSummonLog(Player* hunter, Creature* hawk)
    {
        WorldPacket data(SMSG_SPELLLOGEXECUTE, 8 + 4 + 4 + 4 + 4 + 8);
        data << hunter->GetPackGUID();
        data << uint32(SPELL_SUMMON_HAWK);
        data << uint32(1);                   // effects
        data << uint32(SPELL_EFFECT_SUMMON);
        data << uint32(1);                   // targets
        data << hawk->GetPackGUID();
        hunter->SendMessageToSet(&data, true);
    }

    // Send out a hawk at the target. A hunter can have a few out; a new one replaces the one with
    // the least time left.
    void SummonHawk(Player* hunter, Unit* target)
    {
        std::list<Creature*> hawks;
        hunter->GetAllMinionsByEntry(hawks, NPC_HAWK);
        while (!hawks.empty() && hawks.size() >= std::max<uint32>(1, config.hawkMaxActive))
        {
            auto oldest = std::min_element(hawks.begin(), hawks.end(), [](Creature* a, Creature* b)
            {
                return a->ToTempSummon()->GetTimer() < b->ToTempSummon()->GetTimer();
            });
            (*oldest)->ToTempSummon()->UnSummon();
            hawks.erase(oldest);
        }

        // Start it in the air near the target, so it dives in.
        float const angle = frand(0.0f, 2 * float(M_PI));
        Position pos(target->GetPositionX() + 4.0f * std::cos(angle), target->GetPositionY() + 4.0f * std::sin(angle),
            target->GetPositionZ() + 8.0f);
        pos.SetOrientation(pos.GetAbsoluteAngle(target->GetPosition()));

        SummonPropertiesEntry const* properties = sSummonPropertiesStore.LookupEntry(SUMMON_PROPERTIES_GUARDIAN);
        TempSummon* hawk = hunter->GetMap()->SummonCreature(NPC_HAWK, pos, properties, config.hawkDuration, hunter, SPELL_SUMMON_HAWK);
        if (!hawk)
            return;

        hawk->SetCanFly(true);
        hawk->SetDisableGravity(true);
        if (hawk->AI())
            hawk->AI()->AttackStart(target);

        SendHawkSummonLog(hunter, hawk);
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

    // Thunderstrike's Chain Lightning has 30 yards of range, less than a hunter shoots from, and
    // the core checks range on triggered spells too. Only the four Shadowstrike/Thunderstrike
    // items cast it; their melee procs don't mind the longer range.
    void ApplyChainLightningRange()
    {
        SpellInfo* spellInfo = const_cast<SpellInfo*>(sSpellMgr->GetSpellInfo(SPELL_CHAIN_LIGHTNING));
        if (!spellInfo)
            return;

        if (SpellRangeEntry const* range = sSpellRangeStore.LookupEntry(SPELL_RANGE_100_YARDS))
            spellInfo->RangeEntry = range;
    }

    // Which form is in the main hand, if either: its entry, or 0.
    uint32 GetEquippedStrikeForm(Player const* player)
    {
        Item const* weapon = player->GetUseableItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        if (!weapon)
            return 0;

        uint32 const entry = weapon->GetEntry();
        return entry == ITEM_SHADOWSTRIKE || entry == ITEM_THUNDERSTRIKE ? entry : 0;
    }

    // Shadowstrike: the pet drains the target and heals itself. Drain Life heals whoever casts
    // it, so the pet casts it. A pet that is dead, dismissed, out of range or out of sight of the
    // target can't; then the hunter drains it, as the stock item did.
    void CastShadowstrikeDrain(Player* hunter, Unit* target, AuraEffect const* aurEff)
    {
        if (Pet* pet = hunter->GetPet(); pet && pet->IsAlive() && pet->IsInMap(target)
            && pet->CastSpell(target, SPELL_DRAIN_LIFE, true) == SPELL_CAST_OK)
            return;

        hunter->CastSpell(target, SPELL_DRAIN_LIFE, true, nullptr, aurEff);
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

// The hawk: attacks the hunter's target until it dies, then the hunter's next target, and follows
// the hunter when there's nothing to fight. It never runs home to reset.
struct npc_forever_hunter_hawk : public CreatureAI
{
    explicit npc_forever_hunter_hawk(Creature* creature) : CreatureAI(creature) { }

    bool IsGoodTarget(Unit* target) const
    {
        return target && target->IsAlive() && me->IsValidAttackTarget(target);
    }

    Unit* FindTarget() const
    {
        Unit* hunter = me->GetOwner();
        if (!hunter)
            return nullptr;

        if (Unit* victim = hunter->GetVictim(); IsGoodTarget(victim))
            return victim;

        if (Unit* attacker = hunter->getAttackerForHelper(); IsGoodTarget(attacker))
            return attacker;

        return nullptr;
    }

    void AttackStart(Unit* target) override
    {
        if (IsGoodTarget(target) && me->Attack(target, true))
            me->GetMotionMaster()->MoveChase(target);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override { }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!IsGoodTarget(me->GetVictim()))
        {
            if (Unit* target = FindTarget())
                AttackStart(target);
            else
            {
                if (me->GetVictim())
                    me->AttackStop();

                Unit* hunter = me->GetOwner();
                if (hunter && me->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
                    me->GetMotionMaster()->MoveFollow(hunter, PET_FOLLOW_DIST, me->GetFollowAngle());
                return;
            }
        }

        DoMeleeAttackIfReady();
    }
};

// 51919 - Swoop, the Summon Hawk button. Costs a share of base mana like Arcane Shot and shares
// its cooldown. Only players are changed: nothing else casts it.
class spell_hun_summon_hawk : public SpellScript
{
    PrepareSpellScript(spell_hun_summon_hawk);

    Player* GetHunter()
    {
        Unit* caster = GetCaster();
        return caster ? caster->ToPlayer() : nullptr;
    }

    // The client thinks it's a free 100 yard spell, so check here what Arcane Shot would: range
    // (with Hawk Eye), line of sight and mana.
    SpellCastResult CheckCast()
    {
        Player* hunter = GetHunter();
        if (!hunter)
            return SPELL_CAST_OK;

        if (!ShouldKnowSummonHawk(hunter))
            return SPELL_FAILED_SPELL_UNAVAILABLE;

        Unit* target = GetExplTargetUnit();
        if (!target)
            return SPELL_FAILED_BAD_TARGETS;

        float maxRange = 35.0f;
        if (SpellInfo const* arcaneShot = sSpellMgr->GetSpellInfo(SPELL_ARCANE_SHOT))
            maxRange = arcaneShot->GetMaxRange(false, hunter);

        if (!hunter->IsWithinCombatRange(target, maxRange))
            return SPELL_FAILED_OUT_OF_RANGE;

        if (!hunter->IsWithinLOSInMap(target))
            return SPELL_FAILED_LINE_OF_SIGHT;

        if (hunter->GetPower(POWER_MANA) < GetHawkManaCost(hunter))
            return SPELL_FAILED_NO_POWER;

        return SPELL_CAST_OK;
    }

    void TakeManaAndStartCooldown()
    {
        if (Player* hunter = GetHunter())
        {
            hunter->ModifyPower(POWER_MANA, -int32(GetHawkManaCost(hunter)));
            StartSharedCooldown(hunter, true);
        }
    }

    // The spell flies to the target first; the hawk comes when it lands.
    void HandleHit(SpellEffIndex /*effIndex*/)
    {
        Player* hunter = GetHunter();
        Unit* target = GetHitUnit();
        if (hunter && target && target->IsAlive())
            SummonHawk(hunter, target);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_hun_summon_hawk::CheckCast);
        AfterCast += SpellCastFn(spell_hun_summon_hawk::TakeManaAndStartCooldown);
        // Any effect: the scripts are checked before the module turns effect 0 into a dummy.
        OnEffectHitTarget += SpellEffectFn(spell_hun_summon_hawk::HandleHit, EFFECT_0, SPELL_EFFECT_ANY);
    }
};

// 67552 - Pet Scaling - Master Spell 01, renamed Lone Wolf. Every hunter carries it, hidden; its
// damage bonus is 0 unless Lone Wolf is on. Its once-a-second check also keeps Summon Hawk in the
// spellbook of the hunters who should have it.
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
        {
            UpdateLoneWolfBuff(hunter, effect->GetAmount() > 0 && hunter->IsAlive());
            UpdateSummonHawkSpell(hunter);
        }
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

// 90360 - Shadowstrike and Thunderstrike (mod-forever-hunter). Hidden, on every character; procs
// on ranged hits (spell_proc sets the rate), but only with one of the two forms in the main hand.
class spell_hun_shadowstrike_ranged : public AuraScript
{
    PrepareAuraScript(spell_hun_shadowstrike_ranged);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!config.shadowstrikeEnabled)
            return false;

        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        return player && target && target != player && target->IsAlive() && GetEquippedStrikeForm(player);
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        Player* player = GetTarget()->ToPlayer();
        Unit* target = eventInfo.GetActionTarget();
        if (!player || !target)
            return;

        switch (GetEquippedStrikeForm(player))
        {
            case ITEM_SHADOWSTRIKE:
                CastShadowstrikeDrain(player, target, aurEff);
                break;
            case ITEM_THUNDERSTRIKE:
                player->CastSpell(target, SPELL_CHAIN_LIGHTNING, true, nullptr, aurEff);
                break;
            default:
                break;
        }
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_hun_shadowstrike_ranged::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_hun_shadowstrike_ranged::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};

class ForeverHunterWorldScript : public WorldScript
{
public:
    ForeverHunterWorldScript() : WorldScript("ForeverHunterWorldScript") { }

    void OnBeforeWorldInitialized() override
    {
        MarkLoneWolfBuffUnsaved();
        ApplySummonHawkSpellChanges();
        ApplyChainLightningRange();
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        config.loneWolfEnabled            = sConfigMgr->GetOption<bool>("ForeverHunter.LoneWolf.Enable", true);
        config.loneWolfDamagePercent      = sConfigMgr->GetOption<uint32>("ForeverHunter.LoneWolf.DamagePercent", 20);
        config.loneWolfMarksmanshipPoints = sConfigMgr->GetOption<uint32>("ForeverHunter.LoneWolf.MarksmanshipPoints", 10);
        config.loneWolfRequireMainTree    = sConfigMgr->GetOption<bool>("ForeverHunter.LoneWolf.RequireMainTree", true);

        config.hawkEnabled            = sConfigMgr->GetOption<bool>("ForeverHunter.SummonHawk.Enable", true);
        config.hawkBeastMasteryPoints = sConfigMgr->GetOption<uint32>("ForeverHunter.SummonHawk.BeastMasteryPoints", 15);
        config.hawkRequireMainTree    = sConfigMgr->GetOption<bool>("ForeverHunter.SummonHawk.RequireMainTree", true);
        config.hawkDuration           = sConfigMgr->GetOption<uint32>("ForeverHunter.SummonHawk.Duration", 18000);
        config.hawkCooldown           = sConfigMgr->GetOption<uint32>("ForeverHunter.SummonHawk.Cooldown", 6000);
        config.hawkMaxActive          = sConfigMgr->GetOption<uint32>("ForeverHunter.SummonHawk.MaxActive", 2);
        config.hawkManaCostPercent    = sConfigMgr->GetOption<uint32>("ForeverHunter.SummonHawk.ManaCostPercent", 5);
        config.hawkAttackPowerPercent = sConfigMgr->GetOption<uint32>("ForeverHunter.SummonHawk.AttackPowerPercent", 30);

        config.petHoldEnabled = sConfigMgr->GetOption<bool>("ForeverHunter.PetHold.Enable", true);

        config.shadowstrikeEnabled = sConfigMgr->GetOption<bool>("ForeverHunter.Shadowstrike.Enable", true);

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

    // Runs whenever a pet's or guardian's stats are set up: when it's tamed, summoned, loaded or
    // levels up. That's where the core adds its own pet scaling auras too. They're passive, so
    // they stay through death and are never saved; the next summon adds them again.
    void OnInitStatsForLevel(Guardian* guardian, uint8 /*petlevel*/) override
    {
        if (guardian->GetEntry() == NPC_HAWK)
        {
            if (Unit* owner = guardian->GetOwner())
                if (Player* hunter = owner->ToPlayer())
                    SetUpHawk(guardian, hunter);
            return;
        }

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

    // Give every hunter the hidden Lone Wolf aura, and everyone the hidden Shadowstrike aura: any
    // class that can wield the polearm and shoot gets its ranged procs. Both are passive, so they
    // stay through death and aren't saved; each login adds them again. Switched off in the
    // config, they do nothing.
    void OnPlayerLogin(Player* player) override
    {
        if (!player->HasAura(SPELL_SHADOWSTRIKE_RANGED))
            player->AddAura(SPELL_SHADOWSTRIKE_RANGED, player);

        if (player->getClass() != CLASS_HUNTER)
            return;

        if (!player->HasAura(SPELL_LONE_WOLF))
            player->AddAura(SPELL_LONE_WOLF, player);

        UpdateSummonHawkSpell(player);
    }
};

class ForeverHunterAllSpellScript : public AllSpellScript
{
public:
    ForeverHunterAllSpellScript() : AllSpellScript("ForeverHunterAllSpellScript", { ALLSPELLHOOK_ON_CAST }) { }

    // Arcane Shot puts Summon Hawk on cooldown too. The core has already started Arcane Shot's own.
    void OnSpellCast(Spell* /*spell*/, Unit* caster, SpellInfo const* spellInfo, bool /*skipCheck*/) override
    {
        if (!caster || !caster->IsPlayer() || spellInfo->GetFirstRankSpell()->Id != SPELL_ARCANE_SHOT)
            return;

        Player* hunter = caster->ToPlayer();
        if (hunter->HasSpell(SPELL_SUMMON_HAWK))
            StartSharedCooldown(hunter, false);
    }
};

class ForeverHunterServerScript : public ServerScript
{
public:
    ForeverHunterServerScript() : ServerScript("ForeverHunterServerScript", { SERVERHOOK_CAN_PACKET_RECEIVE }) { }

    // Runs for every packet from every client, so bail out on the opcode first. Pet commands are
    // handled on the map's own update, so the hunter, the pet and the target are safe to read.
    bool CanPacketReceive(WorldSession* session, WorldPacket const& packet) override
    {
        if (!config.petHoldEnabled || packet.GetOpcode() != CMSG_PET_ACTION || !session)
            return true;

        return !IsPetSentOutsideTheFight(session, packet);
    }
};

void AddForeverHunterScripts()
{
    new ForeverHunterWorldScript();
    new ForeverHunterPetScript();
    new ForeverHunterPlayerScript();
    new ForeverHunterAllSpellScript();
    new ForeverHunterServerScript();
    RegisterSpellScript(spell_hun_pet_sod_scaling);
    RegisterSpellScript(spell_hun_pet_sod_ability_ap);
    RegisterSpellScript(spell_hun_lone_wolf);
    RegisterSpellScript(spell_hun_summon_hawk);
    RegisterSpellScript(spell_hun_shadowstrike_ranged);
    RegisterCreatureAI(npc_forever_hunter_hawk);
}
