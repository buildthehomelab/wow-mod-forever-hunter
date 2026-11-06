-- mod-forever-hunter: Call Stabled Pet (62757) from the hunter trainer at level 60 instead of 80.
--
-- Stock 3.3.5 teaches it at 80 (trainer 7, the shared hunter trainer list, 30 gold). The spell
-- itself has no level check when cast, so only the trainer row changes. The cost stays the same.
-- Idempotent: safe to run again. The uninstall file puts it back to 80.

UPDATE `trainer_spell` SET `ReqLevel` = 60 WHERE `TrainerId` = 7 AND `SpellId` = 62757;
