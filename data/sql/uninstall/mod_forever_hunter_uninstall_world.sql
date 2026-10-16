-- mod-forever-hunter: undo the module's world database changes. Run it by hand on the world
-- database after removing the module, together with mod_forever_hunter_uninstall_characters.sql.
-- They aren't applied automatically: AzerothCore only runs the module's db-world folder.
--
-- Turning the module off doesn't need this: the Enable settings are enough. This is for taking
-- the module out of the server for good, so the core doesn't log missing scripts.
--
-- Puts 67552, 67562 and 67563 back to the empty stubs AzerothCore ships, removes the script
-- bindings and the Summon Hawk creature, and puts Shadowstrike and Thunderstrike back to stock.
-- Idempotent: safe to run again.

UPDATE `spell_dbc` SET
    `Attributes` = 384, `AttributesEx4` = 1048576, `ProcChance` = 0,
    `Effect_1` = 0, `EffectBasePoints_1` = 0, `ImplicitTargetA_1` = 0, `EffectMultipleValue_1` = 0,
    `EffectAura_1` = 0, `EffectMiscValue_1` = 0,
    `Effect_2` = 0, `EffectBasePoints_2` = 0, `ImplicitTargetA_2` = 0, `EffectMultipleValue_2` = 0,
    `EffectAura_2` = 0, `EffectMiscValue_2` = 0,
    `Effect_3` = 0, `EffectBasePoints_3` = 0, `ImplicitTargetA_3` = 0, `EffectMultipleValue_3` = 0,
    `EffectAura_3` = 0, `EffectMiscValue_3` = 0
WHERE `ID` IN (67562, 67563);

UPDATE `spell_dbc` SET
    `Attributes` = 384,
    `Effect_1` = 0, `EffectBasePoints_1` = 0, `ImplicitTargetA_1` = 0, `EffectMultipleValue_1` = 0,
    `EffectAura_1` = 0, `EffectMiscValue_1` = 0,
    `Name_Lang_enUS` = 'Pet Scaling - Master Spell 01 - AP, SP, Armor'
WHERE `ID` = 67552;

DELETE FROM `spell_script_names` WHERE `ScriptName` IN ('spell_hun_pet_sod_scaling', 'spell_hun_pet_sod_ability_ap', 'spell_hun_lone_wolf', 'spell_hun_summon_hawk', 'spell_hun_shadowstrike_ranged');

-- Shadowstrike and Thunderstrike: back to stock (no stats, melee procs, 60 second swap), and the
-- ranged proc aura.
UPDATE `item_template` SET
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0,
    `spellid_1` = 21170, `spelltrigger_1` = 2, `spellppmRate_1` = 2,
    `spellcategorycooldown_2` = 60000, `description` = ''
WHERE `entry` = 17074;
UPDATE `item_template` SET
    `stat_type1` = 0, `stat_value1` = 0, `stat_type2` = 0, `stat_value2` = 0,
    `spellid_1` = 21179, `spelltrigger_1` = 2, `spellppmRate_1` = 2,
    `spellcategorycooldown_2` = 60000, `description` = ''
WHERE `entry` = 17223;
DELETE FROM `spell_dbc` WHERE `ID` = 90360;
DELETE FROM `spell_proc` WHERE `SpellId` = 90360;

-- The Summon Hawk creature.
DELETE FROM `creature_template` WHERE `entry` = 9500300;
DELETE FROM `creature_template_model` WHERE `CreatureID` = 9500300;
DELETE FROM `creature_template_movement` WHERE `CreatureId` = 9500300;
