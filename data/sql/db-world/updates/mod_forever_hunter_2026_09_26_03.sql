-- mod-forever-hunter: Summon Hawk. The hawk creature, and the script on the button.
--
-- 9500300 "Hawk" is new (the user's module entries live in the 9500000 range). It looks and moves
-- like the Fjord Hawk (24747): same model (22633), speed and flight. Its level, faction, health,
-- damage and attack speed are set when it's summoned, from the hunter; the numbers here are
-- placeholders. ScriptName must match npc_forever_hunter_hawk in src/ForeverHunter.cpp.
--
-- The button is Swoop (51919), a spell the client has that nothing uses. The script turns it into
-- Summon Hawk; the spell id must match src/ForeverHunter.cpp. Idempotent: safe to run again.

DELETE FROM `creature_template` WHERE `entry` = 9500300;
INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `speed_walk`, `speed_run`, `speed_flight`,
     `BaseAttackTime`, `RangeAttackTime`, `unit_class`, `unit_flags2`, `type`, `HealthModifier`, `DamageModifier`,
     `RegenHealth`, `ScriptName`)
VALUES
    (9500300, 'Hawk', '', 1, 80, 35, 1, 2.57143, 2.57143,
     2000, 2000, 1, 2048, 1, 1, 1,
     1, 'npc_forever_hunter_hawk');

DELETE FROM `creature_template_model` WHERE `CreatureID` = 9500300;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES (9500300, 0, 22633, 1, 1);

DELETE FROM `creature_template_movement` WHERE `CreatureId` = 9500300;
INSERT INTO `creature_template_movement` (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`)
VALUES (9500300, 1, 1, 1, 0, 0, 0);

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_hun_summon_hawk';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (51919, 'spell_hun_summon_hawk');
