-- mod-forever-hunter: undo the module's world database changes. Run it by hand after removing
-- the module. It isn't applied automatically: AzerothCore only runs the module's db-world folder.
--
-- Turning the module off doesn't need this: ForeverHunter.PetScaling.Enable = 0 is enough. This
-- is for taking the module out of the server for good, so the core doesn't log missing scripts.
--
-- Puts 67562 and 67563 back to the empty stubs AzerothCore ships, and removes the script
-- bindings. Idempotent: safe to run again.

UPDATE `spell_dbc` SET
    `Attributes` = 384, `AttributesEx4` = 1048576, `ProcChance` = 0,
    `Effect_1` = 0, `EffectBasePoints_1` = 0, `ImplicitTargetA_1` = 0, `EffectMultipleValue_1` = 0,
    `EffectAura_1` = 0, `EffectMiscValue_1` = 0,
    `Effect_2` = 0, `EffectBasePoints_2` = 0, `ImplicitTargetA_2` = 0, `EffectMultipleValue_2` = 0,
    `EffectAura_2` = 0, `EffectMiscValue_2` = 0,
    `Effect_3` = 0, `EffectBasePoints_3` = 0, `ImplicitTargetA_3` = 0, `EffectMultipleValue_3` = 0,
    `EffectAura_3` = 0, `EffectMiscValue_3` = 0
WHERE `ID` IN (67562, 67563);

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_hun_pet_sod_scaling', 'spell_hun_pet_sod_ability_ap');
