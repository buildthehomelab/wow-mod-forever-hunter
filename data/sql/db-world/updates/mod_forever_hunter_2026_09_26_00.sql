-- mod-forever-hunter: give two unused server-side pet scaling spells their effects and bind
-- the script that works out the amounts.
--
-- 67562 "Pet Scaling - Master Spell 07 - Mana Regeneration, Spell Haste, Melee Haste" and
-- 67563 "Pet Scaling - Master Spell 08 - Spell Crit, Melee Crit, Armor Penetration" are empty
-- stubs in AzerothCore's spell_dbc, and nothing in the core uses them. The client doesn't have
-- them either, so this needs no client patch. The columns set here match the stubs' filled-in
-- siblings, 67557 and 67561: passive, hidden, permanent, on the pet itself.
--
--   67562 effect 1: SPELL_AURA_MOD_INCREASE_ENERGY (35), focus (2): extra max focus
--   67562 effect 3: SPELL_AURA_MOD_MELEE_HASTE (138): faster pet attacks
--   67563 effect 2: SPELL_AURA_MOD_CRIT_PCT (290): pet crit, for auto attacks and abilities
--
-- The script sets every amount; the base points just copy the siblings'. The spell ids must match
-- src/ForeverHunter.PetScaling.cpp. Idempotent: safe to run again.

UPDATE `spell_dbc` SET
    `Attributes` = 448, `AttributesEx4` = 34603008, `ProcChance` = 101,
    `Effect_1` = 6, `EffectBasePoints_1` = -1, `ImplicitTargetA_1` = 1, `EffectMultipleValue_1` = 1,
    `EffectAura_1` = 35, `EffectMiscValue_1` = 2,
    `Effect_2` = 0, `EffectBasePoints_2` = 0, `ImplicitTargetA_2` = 0, `EffectMultipleValue_2` = 0,
    `EffectAura_2` = 0, `EffectMiscValue_2` = 0,
    `Effect_3` = 6, `EffectBasePoints_3` = -1, `ImplicitTargetA_3` = 1, `EffectMultipleValue_3` = 1,
    `EffectAura_3` = 138, `EffectMiscValue_3` = 0
WHERE `ID` = 67562;

UPDATE `spell_dbc` SET
    `Attributes` = 448, `AttributesEx4` = 34603008, `ProcChance` = 101,
    `Effect_1` = 0, `EffectBasePoints_1` = 0, `ImplicitTargetA_1` = 0, `EffectMultipleValue_1` = 0,
    `EffectAura_1` = 0, `EffectMiscValue_1` = 0,
    `Effect_2` = 6, `EffectBasePoints_2` = -1, `ImplicitTargetA_2` = 1, `EffectMultipleValue_2` = 1,
    `EffectAura_2` = 290, `EffectMiscValue_2` = 0,
    `Effect_3` = 0, `EffectBasePoints_3` = 0, `ImplicitTargetA_3` = 0, `EffectMultipleValue_3` = 0,
    `EffectAura_3` = 0, `EffectMiscValue_3` = 0
WHERE `ID` = 67563;

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_hun_pet_sod_scaling';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(67562, 'spell_hun_pet_sod_scaling'),
(67563, 'spell_hun_pet_sod_scaling');
