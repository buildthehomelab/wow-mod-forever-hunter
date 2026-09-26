# Forever Hunter

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module for hunter changes:
Season of Discovery-style pet scaling, so hunter pets get more from the hunter's gear, and WoW
Forever's Lone Wolf for Marksmanship hunters who play without a pet. No client patch needed.

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

## Lone Wolf

In WoW Forever, Lone Wolf is a Marksmanship talent: 20% more damage with all attacks while you
don't have a pet out. It needs 10 points in Marksmanship.

A 3.3.5 client can't show a new talent without a client patch, so here it isn't a talent you
pick. **Every hunter with at least 10 points in Marksmanship** gets it whenever they have no
living pet out: dismissed, stabled, dead or never summoned. Summon a pet and it's gone.

- It adds 20% to all of the hunter's damage: shots, auto shot, melee, stings, traps.
- The hunter sees it as a **Frenzy** buff with a red unholy-frenzy icon. Frenzy is a buff the
  client already has; in the game Blizzard only gave it to an NPC (Moroes in Karazhan). Its tooltip
  says "Physical damage dealt is increased by 20%", but the bonus covers every school, so Arcane
  Shot, Serpent Sting and Explosive Shot get it too.
- The buff can't be dispelled or spellstolen. Cancelling it by right-click does nothing: it comes
  back within a second, and the bonus never went away.

The bonus itself is on a hidden server-side aura every hunter carries: "Pet Scaling - Master Spell
01" (67552), another empty stub, renamed "Lone Wolf" for GM aura lists. Once a second it checks
the hunter's pet and talents, sets the bonus, and shows or hides Frenzy.

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
| `ForeverHunter.LoneWolf.Enable` | `1` | Lone Wolf on or off. |
| `ForeverHunter.LoneWolf.DamagePercent` | `20` | Extra damage in percent. The buff's tooltip says 20% whatever this is. |
| `ForeverHunter.LoneWolf.MarksmanshipPoints` | `10` | Marksmanship points needed. `0` gives it to every hunter. |

## Turning it off

Change the config and reload it (`.reload config`) or restart:

- **Pet scaling:** `ForeverHunter.PetScaling.Enable = 0`. Within 2 seconds every pet loses the
  crit, haste, focus and ability bonuses; no resummon needed. For one part only, set
  `CritPercent = 0`, `HastePercent = 0`, `FocusBonus = 0` or `PhysicalAbilityAPMultiplier = 1.0`.
- **Lone Wolf:** `ForeverHunter.LoneWolf.Enable = 0`. Within a second hunters lose the bonus and
  the Frenzy buff.

Nothing is saved on characters or pets, so turning it off leaves nothing behind. To remove the
module for good, delete it, rebuild, and run `data/sql/uninstall/mod_forever_hunter_uninstall.sql`
on the world database. That puts the three server-side spells back to the empty stubs AzerothCore
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
- Lone Wolf isn't a real talent: nobody can skip it, and it doesn't cost a talent point. A hunter
  with 10+ Marksmanship points who dismisses their pet always gets it, in PvP too.
- The WoW Forever beta reports the talent also costs a few percent of damage while a pet is out.
  That isn't copied.
