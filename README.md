# Forever Hunter

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module for hunter changes. For now
that's Season of Discovery-style pet scaling: hunter pets get more from the hunter's gear. No
client patch needed.

## Pet scaling

Stock AzerothCore already gives hunter pets part of the hunter's stats: 45% of stamina, 22% of
ranged attack power as attack power, 12.87% of it as spell damage, 35% of armor, 40% of
resistances, and hit and expertise scaled to the caps. This module leaves all of that alone and
adds the parts SoD has that 3.3.5 doesn't:

- **Crit:** the pet gets the hunter's ranged crit chance, on top of its own 5%. It counts for auto
  attacks and for pet abilities, physical and magic.
- **Haste:** the pet attacks faster by the hunter's ranged haste. By default only haste rating
  counts; a setting adds quiver, Rapid Fire and other ranged haste.
- **Focus:** the pet's maximum focus goes up by 51, as in SoD, so from 100 to 151.
- **Physical abilities scale better with attack power.** SoD made pet abilities scale better with
  the hunter's attack power. In stock 3.3.5, Claw, Bite, Smack and the other physical pet
  abilities get only 7% of the pet's attack power (about 1.5% of the hunter's ranged AP), while
  magic ones like Lightning Breath get about 4.3%. By default the module doubles the physical
  share. The extra damage gets the pet's damage bonuses and can crit, like the rest of the hit.

The bonuses follow the hunter's gear and buffs, updating every 2 seconds.

## How it works

Blizzard left two server-side spells as empty stubs: "Pet Scaling - Master Spell 07" (67562, meant
for haste) and "Master Spell 08" (67563, meant for crit). Nothing in AzerothCore uses them, and the
client doesn't know them. The module's SQL gives them their effects in `spell_dbc`, the module puts
them on every hunter pet when it's tamed, summoned or levels up, and a spell script works out the
amounts from the hunter's stats.

## Install

Clone it into your AzerothCore `modules` folder **as `mod-forever-hunter`**, without the repo's
`wow-` prefix. AzerothCore finds the module's entry point from the folder name.

```bash
cd <azerothcore>/modules
git clone https://github.com/buildthehomelab/wow-mod-forever-hunter.git mod-forever-hunter
```

Rebuild the worldserver, then copy `conf/mod_forever_hunter.conf.dist` to your config folder
as `mod_forever_hunter.conf`. The SQL is applied to the world database on the next start.
Pets that are already out get the bonuses the next time they're summoned.

## Settings

| Setting | Default | What it does |
|---------|---------|--------------|
| `ForeverHunter.PetScaling.Enable` | `1` | Master switch. With `0`, pets lose the bonuses within 2 seconds. |
| `ForeverHunter.PetScaling.CritPercent` | `100` | Share of the hunter's ranged crit chance the pet gets, in percent. |
| `ForeverHunter.PetScaling.HastePercent` | `100` | Share of the hunter's ranged haste the pet gets, in percent. |
| `ForeverHunter.PetScaling.HasteSource` | `0` | `0`: haste rating only. `1`: all ranged haste (quiver, Rapid Fire, ...). |
| `ForeverHunter.PetScaling.FocusBonus` | `51` | Extra maximum focus. `0` for none. |
| `ForeverHunter.PetScaling.PhysicalAbilityAPMultiplier` | `2.0` | Multiplies the attack power share of physical pet abilities. `1.0` changes nothing; about `2.8` matches the magic abilities. |

## Turning it off

- **Everything:** set `ForeverHunter.PetScaling.Enable = 0` and reload the config (`.reload config`)
  or restart. Within 2 seconds every pet loses the crit, haste, focus and ability bonuses; no
  resummon needed.
- **One part:** `CritPercent = 0`, `HastePercent = 0`, `FocusBonus = 0` or
  `PhysicalAbilityAPMultiplier = 1.0` turns off just that part.

Nothing is saved on characters or pets, so turning it off leaves nothing behind. To remove the
module for good, delete it, rebuild, and run `data/sql/uninstall/mod_forever_hunter_uninstall.sql`
on the world database. That puts the two server-side spells back to the empty stubs AzerothCore
ships and removes the script bindings.

## Limits

- Crit and haste are whole percents: 23.6% crit gives the pet 24%.
- The pet's crit doesn't show anywhere in the client; the 3.3.5 pet frame has no crit stat. The
  extra focus and the faster attack speed do show.
- With `HasteSource = 1`, Serpent's Swiftness counts twice: it already speeds up the pet by itself.
- Rake's bleed isn't changed, only its first hit.
- Tendon Rip (spiders) is untouched and gets no attack power or spell power scaling at all, in
  stock AzerothCore too: it's Physical but only has a spell power coefficient, and pets only get
  spell power for magic schools.
