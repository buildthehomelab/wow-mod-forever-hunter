-- mod-forever-hunter: Shadowstrike and Thunderstrike as hunter weapons.
--
-- Stock, the Molten Core polearm pair (17074, 17223) has no stats at all: just a 2-per-minute
-- melee proc and a 60 second swap between the two forms. Here both forms are Agility weapons for
-- hunters, and the procs come from ranged hits instead of melee swings:
--
--   Shadowstrike  +30 Agility, +20 Stamina. Ranged hits can make your pet Drain Life (21170,
--                 100-180) from the target, healing the pet, if the pet is fighting that
--                 target. Otherwise you drain it.
--   Thunderstrike +30 Agility, +10 hit rating. Ranged hits can cast Chain Lightning (21179,
--                 150-250 Nature, up to 3 targets).
--   Swapping forms has a 30 second cooldown instead of 60.
--
-- The items lose their melee procs. The ranged procs come from a hidden server-only aura, 90360,
-- which the module puts on every character at login and when either form is equipped; its script
-- checks the main hand for one of the two forms. The client never sees 90360, so the tooltip line is the item description.
-- spell_proc sets the rate: 1 proc per minute per ranged hit, so with Auto Shot and the shots in
-- between, about 2 a minute like the stock item, and at most one every 3 seconds, so a Multi-Shot
-- can't proc three times. The spell id must match src/ForeverHunter.cpp. Idempotent: safe to
-- run again.

UPDATE `item_template` SET
    `stat_type1` = 3, `stat_value1` = 30,  -- Agility
    `stat_type2` = 7, `stat_value2` = 20,  -- Stamina
    `spellid_1` = 0, `spelltrigger_1` = 0, `spellppmRate_1` = 0,
    `spellcategorycooldown_2` = 30000,
    `description` = 'Chance on ranged hit: your pet drains 100 to 180 life from the target, more with its spell power. If your pet isn''t fighting it, you drain it.'
WHERE `entry` = 17074;

UPDATE `item_template` SET
    `stat_type1` = 3,  `stat_value1` = 30,  -- Agility
    `stat_type2` = 31, `stat_value2` = 10,  -- Hit rating
    `spellid_1` = 0, `spelltrigger_1` = 0, `spellppmRate_1` = 0,
    `spellcategorycooldown_2` = 30000,
    `description` = 'Chance on ranged hit: Chain Lightning blasts up to 3 targets for 150 to 250 Nature damage.'
WHERE `entry` = 17223;

-- A passive, hidden dummy aura on the character; EquippedItemClass -1 so the core doesn't look
-- for a required weapon itself.
DELETE FROM `spell_dbc` WHERE `ID` = 90360;
INSERT INTO `spell_dbc`
    (`ID`, `Attributes`, `AttributesEx`, `CastingTimeIndex`, `ProcTypeMask`, `ProcChance`, `DurationIndex`,
     `RangeIndex`, `EquippedItemClass`, `Effect_1`, `ImplicitTargetA_1`, `EffectAura_1`,
     `EffectChainAmplitude_1`, `EffectBonusMultiplier_1`, `Name_Lang_enUS`, `Name_Lang_Mask`)
VALUES
    (90360, 192, 268435456, 1, 320, 101, 21,
     1, -1, 6, 1, 4,
     1.0, 1.0, 'Shadowstrike and Thunderstrike (mod-forever-hunter)', 16712190);

-- ProcFlags 320 = ranged auto attack (64) + ranged spells (256); SpellTypeMask 1 = damage;
-- SpellPhaseMask 2 = on hit; HitMask 1027 = normal, critical and absorbed hits (the core's
-- default for done procs, so shots into a shield still proc).
DELETE FROM `spell_proc` WHERE `SpellId` = 90360;
INSERT INTO `spell_proc`
    (`SpellId`, `ProcFlags`, `SpellTypeMask`, `SpellPhaseMask`, `HitMask`, `ProcsPerMinute`, `Cooldown`)
VALUES
    (90360, 320, 1, 2, 1027, 1, 3000);

DELETE FROM `spell_script_names` WHERE `ScriptName` = 'spell_hun_shadowstrike_ranged';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES (90360, 'spell_hun_shadowstrike_ranged');
