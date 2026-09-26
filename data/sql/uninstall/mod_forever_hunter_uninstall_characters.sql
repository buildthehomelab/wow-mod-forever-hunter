-- mod-forever-hunter: take Summon Hawk (51919, Swoop) away from every character. Run it by hand
-- on the characters database, with the worldserver stopped: a running server saves online
-- characters over these tables. AzerothCore doesn't run it automatically.
--
-- While the module is installed, ForeverHunter.SummonHawk.Enable = 0 removes it from each hunter
-- within a second of logging in. Once the module is gone nothing removes it, and hunters would
-- keep an unscripted Swoop that charges them at the target. Run this after removing the module
-- (with mod_forever_hunter_uninstall_world.sql).
--
-- 51919 is a spell nothing else gives players, so every character that has it got it from this
-- module (or a GM). Also clears it from action bars and saved cooldowns. The module's auras are
-- never saved, so they need nothing. Idempotent: safe to run again.

DELETE FROM `character_spell` WHERE `spell` = 51919;
DELETE FROM `character_action` WHERE `action` = 51919 AND `type` = 0; -- 0 = spell button
DELETE FROM `character_spell_cooldown` WHERE `spell` = 51919;
