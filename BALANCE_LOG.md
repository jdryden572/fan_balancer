# Balance log

Measurements are from `ble_monitor.py` (`ble.log`). Amplitudes are in g at the rotation frequency.
All runs use fan speed setting "medium" (about 1430–1455 rpm; the 3-step speed control can't hit an exact rpm).

## Round 2: initial balance of the broken half (squirrel-cage rotor)

**Setup:** the rotor has two blade sets separated by a dividing ring, and one blade is missing on one
half (the "broken half"). The motor sits on a foam pad and was re-seated before this round to reduce
motion. Axis used for the calculation: **X** (largest axis at baseline). Positions are measured in the
same direction as the earlier A/B/C numbering.

**Baseline (no weights)**

| Run | Lines | RPM | X | Y | Z | Combined |
|---|---|---|---|---|---|---|
| A | 25 | 1448 | 0.157 | 0.063 | 0.123 | 0.209 |
| B (rotor removed and re-attached) | 11 | 1455 | 0.154 | 0.061 | 0.122 | 0.206 |

Re-mounting the rotor changes the reading by under 3% and leaves the phase unchanged, so removing
and re-attaching the rotor between runs is acceptable.

**Weight on the broken half at about 330°, same radius, screw in a hole at that location**

| Weight | RPM | X | Y | Z | Combined | X change |
|---|---|---|---|---|---|---|
| none | 1450 | 0.157 | 0.062 | 0.123 | 0.209 | - |
| 1.25 g | 1446 | 0.043 | 0.017 | 0.039 | 0.060 | -73% |
| 1.44 g | 1428 | 0.070 | 0.029 | 0.057 | 0.095 | -55% |
| **1.00 g** | 1433 | **0.014** | 0.003 | 0.021 | **0.025** | **-91%** |

A parabola fitted to X² against mass using the 0, 1.25 and 1.44 g runs predicted a minimum at about
1.0 g with X of about 0.015 g. The measured 1.0 g run gave X = 0.014 g, which confirms that 330° is the
right angle for a single weight in the broken half's plane.

## Result: initial balancing complete

- **Correction:** 1.0 g screw at about 330° from reference position A, on the broken half.
- **Vibration:** combined amplitude 0.209 g -> 0.025 g (-88%).
- **Remaining:** Z (0.021 g) is now the largest axis, then X (0.014 g). This is probably the leftover
  couple from the second plane (the intact half), plus normal mechanical noise.
- **Caution:** speed was 1.2% below baseline on the final run, so the real X value at 1450 rpm is
  about 3% higher (about 0.0145 g). Re-check after a few hours of running.

## Round 2b: second plane (intact half), four-run method

The 1.0 g screw stays on the broken half. The intact half was trialled with tape, rotor left on.
Axis: combined amplitude (X and Z gave the same answer). Trial weight 0.75 g, closer to the axis than
the broken-half screw. Positions A, B, C are 120° apart in the direction A -> B -> C.

| Run | Intact-half weight | RPM | X | Z | Combined |
|---|---|---|---|---|---|
| 0 | none (new baseline) | 1372 | 0.0235 | 0.0303 | 0.041 |
| 1 | 0.75 g at A | 1410 | 0.104 | 0.084 | 0.139 |
| 2 | 0.75 g at B | 1417 | 0.087 | 0.071 | 0.121 |
| 3 | 0.75 g at C | 1421 | 0.126 | 0.113 | 0.179 |

The baseline in this part (0.041 g) is higher than the end of the broken-half work (0.025 g, 1433 rpm).
Something physical changed between sessions (rotor re-attached, different speed setting or motor
seating), so only comparisons within this part are valid.

Four-run result (X, Z and combined, with and without speed correction, all agree):
- Correction angle: **75-85° from A** (towards B).
- Correction mass: **0.17-0.28 g** at the trial weight's radius (about 0.2 g).
- The computed heavy-spot size (0.022-0.037 g) matched the measured baseline, which is the sanity check.

| Intact-half weight | RPM | X | Y | Z | Combined |
|---|---|---|---|---|---|
| none | 1372 | 0.0235 | 0.0139 | 0.0303 | 0.041 |
| 0.3 g at about 80°, taped (first try) | 1425 | 0.0127 | 0.0093 | 0.0026 | 0.016 (drifted up to 0.020 as the tape crept) |
| 0.66 g at a smaller radius | 1429 | 0.0368 | 0.0194 | 0.0245 | 0.048 (too much; radius not small enough) |
| 0.3 g at about 80°, taped again | 1428 | 0.0130 | 0.0084 | 0.0031 | **0.016** (stable, repeatable) |

The weight x radius product is what matters: 0.3 g at the trial radius was close to optimal; the 0.66 g
weight needed roughly 0.3x the trial radius and was estimated to be sitting at about 0.67x.

## Final result

| Stage | Combined | X | Y | Z |
|---|---|---|---|---|
| Original, no weights (round 2) | 0.209 g | 0.157 | 0.062 | 0.123 |
| + 1.0 g screw at 330°, broken half | 0.025 g | 0.014 | 0.003 | 0.021 |
| Re-baseline next session (see note above) | 0.041 g | 0.0235 | 0.0139 | 0.0303 |
| + 0.3 g at 80°, intact half | **0.016 g** | 0.013 | 0.008 | 0.003 |

Final configuration:
- **Broken half:** 1.0 g screw at about 330° from A, fixed.
- **Intact half:** 0.3 g at about 80° from A, at the radius of the 0.75 g trial weight, fixed with epoxy.
- **Net change from original:** combined vibration 0.209 g -> 0.016 g (**-92%**), measured at about 1430 rpm,
  where the unbalanced rotor read 0.209 g at about 1450 rpm. Z (axial/couple) is down to near the sensor
  noise floor (about 0.002-0.003 g).

**Not yet verified:** the 0.016 g figure is from the 0.3 g weight held by tape, just before it was epoxied.
`ble.log` has no steady reading after the epoxy was applied (the last logged data at 21:53-21:55 is the
fan spinning down). Run once more at medium speed after the epoxy has fully cured, wait two minutes, and
confirm the combined amplitude is still about 0.016 g (within roughly 10%). Re-check after a few hours of
running; if it rises, the weight has moved.

## Method notes

- Remove and re-attach the rotor changes the reading by under 3%, with no change in phase.
- The fan's 3-speed control can't hit a fixed rpm; normalise amplitudes with (rpm_ref / rpm)^2 and keep runs
  within about 1-4% of each other. Speed corrections moved the answers by under 20% in mass and under 10 deg in angle.
- Settle for 1-2 minutes before recording; readings drifted for the first minute or two after each change.
- Tape weights can creep under centrifugal force: watch for amplitudes that climb steadily.
- Place a weight in the same axial plane as the missing blade first. A weight on the other half only cancels the force,
  not the rocking couple, and leaves a residual.
- A parabola of amplitude squared against mass (at a fixed angle) predicted the best screw mass (1.0 g) from three points.
