-- mod-forever-hunter: bind the physical ability script to every rank of the physical hunter
-- pet abilities that scale with attack power.
--
-- These are all the focus-using, Physical-school, direct-damage spells in the 3.3.5 Spell.dbc
-- with an ap_bonus in spell_bonus_data (0.07 each, Rake 0.0175). None had a script before. The
-- script only changes damage when a hunter pet casts them, so NPCs that use the same spells keep
-- their damage. Idempotent: safe to run again.

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_hun_pet_sod_ability_ap';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
-- Bite
(17253, 'spell_hun_pet_sod_ability_ap'), (17255, 'spell_hun_pet_sod_ability_ap'), (17256, 'spell_hun_pet_sod_ability_ap'),
(17257, 'spell_hun_pet_sod_ability_ap'), (17258, 'spell_hun_pet_sod_ability_ap'), (17259, 'spell_hun_pet_sod_ability_ap'),
(17260, 'spell_hun_pet_sod_ability_ap'), (17261, 'spell_hun_pet_sod_ability_ap'), (27050, 'spell_hun_pet_sod_ability_ap'),
(52473, 'spell_hun_pet_sod_ability_ap'), (52474, 'spell_hun_pet_sod_ability_ap'),
-- Claw
(16827, 'spell_hun_pet_sod_ability_ap'), (16828, 'spell_hun_pet_sod_ability_ap'), (16829, 'spell_hun_pet_sod_ability_ap'),
(16830, 'spell_hun_pet_sod_ability_ap'), (16831, 'spell_hun_pet_sod_ability_ap'), (16832, 'spell_hun_pet_sod_ability_ap'),
(3010, 'spell_hun_pet_sod_ability_ap'), (3009, 'spell_hun_pet_sod_ability_ap'), (27049, 'spell_hun_pet_sod_ability_ap'),
(52471, 'spell_hun_pet_sod_ability_ap'), (52472, 'spell_hun_pet_sod_ability_ap'),
-- Smack
(49966, 'spell_hun_pet_sod_ability_ap'), (49967, 'spell_hun_pet_sod_ability_ap'), (49968, 'spell_hun_pet_sod_ability_ap'),
(49969, 'spell_hun_pet_sod_ability_ap'), (49970, 'spell_hun_pet_sod_ability_ap'), (49971, 'spell_hun_pet_sod_ability_ap'),
(49972, 'spell_hun_pet_sod_ability_ap'), (49973, 'spell_hun_pet_sod_ability_ap'), (49974, 'spell_hun_pet_sod_ability_ap'),
(52475, 'spell_hun_pet_sod_ability_ap'), (52476, 'spell_hun_pet_sod_ability_ap'),
-- Demoralizing Screech
(24423, 'spell_hun_pet_sod_ability_ap'), (24577, 'spell_hun_pet_sod_ability_ap'), (24578, 'spell_hun_pet_sod_ability_ap'),
(24579, 'spell_hun_pet_sod_ability_ap'), (27051, 'spell_hun_pet_sod_ability_ap'), (55487, 'spell_hun_pet_sod_ability_ap'),
-- Gore
(35290, 'spell_hun_pet_sod_ability_ap'), (35291, 'spell_hun_pet_sod_ability_ap'), (35292, 'spell_hun_pet_sod_ability_ap'),
(35293, 'spell_hun_pet_sod_ability_ap'), (35294, 'spell_hun_pet_sod_ability_ap'), (35295, 'spell_hun_pet_sod_ability_ap'),
-- Monstrous Bite
(54680, 'spell_hun_pet_sod_ability_ap'), (55495, 'spell_hun_pet_sod_ability_ap'), (55496, 'spell_hun_pet_sod_ability_ap'),
(55497, 'spell_hun_pet_sod_ability_ap'), (55498, 'spell_hun_pet_sod_ability_ap'), (55499, 'spell_hun_pet_sod_ability_ap'),
-- Rake (the initial hit; its bleed isn't changed)
(59881, 'spell_hun_pet_sod_ability_ap'), (59882, 'spell_hun_pet_sod_ability_ap'), (59883, 'spell_hun_pet_sod_ability_ap'),
(59884, 'spell_hun_pet_sod_ability_ap'), (59885, 'spell_hun_pet_sod_ability_ap'), (59886, 'spell_hun_pet_sod_ability_ap'),
-- Ravage
(50518, 'spell_hun_pet_sod_ability_ap'), (53558, 'spell_hun_pet_sod_ability_ap'), (53559, 'spell_hun_pet_sod_ability_ap'),
(53560, 'spell_hun_pet_sod_ability_ap'), (53561, 'spell_hun_pet_sod_ability_ap'), (53562, 'spell_hun_pet_sod_ability_ap'),
-- Savage Rend
(50498, 'spell_hun_pet_sod_ability_ap'), (53578, 'spell_hun_pet_sod_ability_ap'), (53579, 'spell_hun_pet_sod_ability_ap'),
(53580, 'spell_hun_pet_sod_ability_ap'), (53581, 'spell_hun_pet_sod_ability_ap'), (53582, 'spell_hun_pet_sod_ability_ap'),
-- Snatch
(50541, 'spell_hun_pet_sod_ability_ap'), (53537, 'spell_hun_pet_sod_ability_ap'), (53538, 'spell_hun_pet_sod_ability_ap'),
(53540, 'spell_hun_pet_sod_ability_ap'), (53542, 'spell_hun_pet_sod_ability_ap'), (53543, 'spell_hun_pet_sod_ability_ap'),
-- Swipe
(50256, 'spell_hun_pet_sod_ability_ap'), (53526, 'spell_hun_pet_sod_ability_ap'), (53528, 'spell_hun_pet_sod_ability_ap'),
(53529, 'spell_hun_pet_sod_ability_ap'), (53532, 'spell_hun_pet_sod_ability_ap'), (53533, 'spell_hun_pet_sod_ability_ap');
