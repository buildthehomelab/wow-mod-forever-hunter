-- mod-forever-hunter: Lone Wolf. Turn one more unused server-side stub into the hidden aura that
-- carries the bonus, and bind its script.
--
-- 67552 "Pet Scaling - Master Spell 01 - AP, SP, Armor" is another empty stub in AzerothCore's
-- spell_dbc that nothing uses and the client doesn't have. It gets the same passive, hidden,
-- permanent setup as its filled-in siblings, with one effect:
--
--   effect 1: SPELL_AURA_MOD_DAMAGE_PERCENT_DONE (79), all schools (127)
--
-- It's renamed so GM aura lists say what it is. The script sets the amount: the configured
-- percentage while Lone Wolf is on, otherwise 0. The spell id must match src/ForeverHunter.cpp.
-- Idempotent: safe to run again.
--
-- The buff players see is Frenzy (37023), which the client already has; it needs no database
-- change.

UPDATE `spell_dbc` SET
    `Attributes` = 448,
    `Effect_1` = 6, `EffectBasePoints_1` = -1, `ImplicitTargetA_1` = 1, `EffectMultipleValue_1` = 1,
    `EffectAura_1` = 79, `EffectMiscValue_1` = 127,
    `Name_Lang_enUS` = 'Lone Wolf (mod-forever-hunter)'
WHERE `ID` = 67552;

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_hun_lone_wolf';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (67552, 'spell_hun_lone_wolf');
