# Forever Hunter

An [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) module for hunter changes. No
client patch needed.

- **Pet scaling**, Season of Discovery-style: hunter pets get more from the hunter's gear.
- **Lone Wolf**, from WoW Forever: Marksmanship hunters deal 20% more damage without a pet out.
- **Summon Hawk**, from WoW Forever: Beast Mastery hunters send hawks at their target.

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
pick. **Every Marksmanship hunter** gets it whenever they have no living pet out: dismissed,
stabled, dead or never summoned. Summon a pet and it's gone. Marksmanship hunter means at least
10 points in Marksmanship, and more there than in either other tree (settings can drop the second
part).

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

## Summon Hawk

In WoW Forever, Summon Hawk is a Beast Mastery talent (15 points in the tree): a hawk dives at
your target and keeps attacking it for 18 seconds. Up to two hawks can be out, and it shares a
6 second cooldown with Arcane Shot, so every hawk costs a shot.

Here **every Beast Mastery hunter** (15+ points in Beast Mastery, more than in either other
tree) learns it; it shows up in the General tab of the spellbook. Drop below that and it's gone.

- Target an enemy and press it. The hawk flies in from above the target and attacks it; when the
  target dies it goes for your next target, and with nothing to fight it follows you. After 18
  seconds it leaves.
- Two hawks at most. A third replaces the one with the least time left.
- **Cooldown:** 6 seconds, shared with Arcane Shot both ways. Both buttons show it.
- **Cost:** 5% of base mana, like Arcane Shot.
- **Range:** Arcane Shot's, Hawk Eye included, and it needs line of sight.
- **Damage:** each hawk hits like a hunter pet of your level, with 30% of your ranged attack power
  as its attack power, fixed when it's summoned. It gets your hit chance, and Unleashed Fury
  (+3% damage per rank) and Ferocity (+2% crit per rank) work on it, as in WoW Forever. Bestial
  Wrath, Frenzy and the other pet talents don't.
- Hawks aren't pets: they don't turn off Lone Wolf, and they don't use the pet bar.

**The button is "Swoop"**, a spell with a hawk icon and a bird sound that the client already has
and nothing in the game uses. Its tooltip reads "Swoop down from a distant height to attack your
target, dealing 648 damage to it"; the 648 is the client's own text and means nothing here. On
the server it's rebuilt: it doesn't move you, it sends the hawk. Swoop has no cooldown or cost
in the client's data, so the button shows no mana cost, and it isn't on the global cooldown.

The hawk is a new creature (9500300) with the Fjord Hawk's model.

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
| `ForeverHunter.LoneWolf.MarksmanshipPoints` | `10` | Marksmanship points needed. |
| `ForeverHunter.LoneWolf.RequireMainTree` | `1` | `1`: Marksmanship must be the tree with the most points. `0`: the points are enough. |
| `ForeverHunter.SummonHawk.Enable` | `1` | Summon Hawk on or off. |
| `ForeverHunter.SummonHawk.BeastMasteryPoints` | `15` | Beast Mastery points needed. |
| `ForeverHunter.SummonHawk.RequireMainTree` | `1` | `1`: Beast Mastery must be the tree with the most points. `0`: the points are enough. |
| `ForeverHunter.SummonHawk.Duration` | `18000` | How long a hawk stays, in milliseconds. |
| `ForeverHunter.SummonHawk.Cooldown` | `6000` | Cooldown shared with Arcane Shot, in milliseconds. `0` for none. |
| `ForeverHunter.SummonHawk.MaxActive` | `2` | Hawks out at once. |
| `ForeverHunter.SummonHawk.ManaCostPercent` | `5` | Mana cost as a percentage of base mana. |
| `ForeverHunter.SummonHawk.AttackPowerPercent` | `30` | Share of the hunter's ranged AP each hawk gets. |

## Turning it off

Change the config and reload it (`.reload config`) or restart:

- **Pet scaling:** `ForeverHunter.PetScaling.Enable = 0`. Within 2 seconds every pet loses the
  crit, haste, focus and ability bonuses; no resummon needed. For one part only, set
  `CritPercent = 0`, `HastePercent = 0`, `FocusBonus = 0` or `PhysicalAbilityAPMultiplier = 1.0`.
- **Lone Wolf:** `ForeverHunter.LoneWolf.Enable = 0`. Within a second hunters lose the bonus and
  the Frenzy buff.
- **Summon Hawk:** `ForeverHunter.SummonHawk.Enable = 0`. Within a second of being online,
  hunters lose the spell; hawks already out stay until they expire.

To remove the module for good: set the `Enable` settings to `0` first if you can, so hunters lose
Summon Hawk as they log in. Then stop the worldserver, delete the module, rebuild, and run both
uninstall files:

- `data/sql/uninstall/mod_forever_hunter_uninstall_world.sql` on the world database puts the
  three server-side spells back to the empty stubs AzerothCore ships, and removes the script
  bindings and the hawk creature.
- `data/sql/uninstall/mod_forever_hunter_uninstall_characters.sql` on the characters database
  takes Summon Hawk (Swoop) off every character, their action bars and saved cooldowns. Without
  it, hunters keep an unscripted Swoop that charges them at the target.

Nothing else is saved on characters or pets.

## Limits

- Crit and haste are whole percents: 23.6% crit gives the pet 24%.
- The pet's crit doesn't show anywhere in the client; the 3.3.5 pet frame has no crit stat. The
  extra focus and the faster attack speed do show.
- With `HasteSource = 1`, Serpent's Swiftness counts twice: it already speeds up the pet by itself.
- Rake's bleed isn't changed, only its first hit.
- Tendon Rip (spiders) is untouched and gets no attack power or spell power scaling at all, in
  stock AzerothCore too: it's Physical but only has a spell power coefficient, and pets only get
  spell power for magic schools.
- Lone Wolf and Summon Hawk aren't real talents: nobody can skip them, and they don't cost a
  talent point. Every Marksmanship hunter who dismisses their pet gets Lone Wolf, in PvP too.
- Summon Hawk's tooltip is Swoop's, and the button shows no cost or cooldown until you use it.
- WoW Forever doesn't publish the hawk's damage; 30% of ranged AP is a guess to tune.
- The WoW Forever beta reports the talent also costs a few percent of damage while a pet is out.
  That isn't copied.
