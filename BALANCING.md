# Balancing a fan rotor with the fan balancer

The firmware has no tach, so it reports only the **vibration amplitude** (g) at the rotation
frequency, not the rotor phase. That is enough to balance the rotor with the
**four-run method**: measure the original vibration, then three runs with the same trial
weight stuck at three positions 120° apart. A little arithmetic then tells you where the
correction weight goes and how heavy it should be.

## What the output means

One line is printed about every 5 seconds, over USB serial and over BLE:

```
rpm, Ax_g, Ay_g, Az_g, Y-X_deg, Z-X_deg
```

- `rpm`: rotation speed, found from the strongest vibration peak between 5 and 100 Hz.
- `Ax_g`, `Ay_g`, `Az_g`: vibration amplitude at that frequency on each axis.
- `Y-X_deg`, `Z-X_deg`: phase of Y and Z relative to X. You don't need these for the method below.

Pressing the **BOOT** button restarts the firmware (BLE disconnects; reconnect afterwards).

## Setup

1. **Safety first.** Unplug the hood. Take off anything that could come loose and fly off. Wear eye
   protection when spinning a rotor that is out of balance.
2. **Mount the sensor rigidly** on the motor or bearing housing, as close to the rotor as possible.
   Use a bolt, hot glue or strong double-sided tape. A loose sensor reads its own wobble, not the
   fan's.
3. **Note the axis orientation.** One of the sensor's axes should point radially (across the shaft).
   Radial vibration is what imbalance causes. The axis along the shaft will read low.
4. Power the board and connect with a phone (Serial Bluetooth Terminal) or a USB serial monitor.
   Wait for lines to appear.
5. Mark the rotor with a reference position and divide it into three positions 120° apart.
   Use the blades or the rotor's mounting holes, for example:
   - **Position A** (call it 0°): any blade or mark you choose.
   - **Position B** (120°): one third of a turn around in the **same direction** from A.
   - **Position C** (240°): two thirds of a turn around in that same direction.

   Number the positions in one fixed direction (say, the direction the fan turns) and stick to it.
   If the rotor has a number of blades that is not a multiple of 3, put marks on the rim with a
   protractor or a printed template.
6. Get a **trial weight** you can put on and take off easily at those positions, such as a small
   blob of modelling clay, a few turns of lead tape, or a nut and tape. Weigh it. Its effect should
   be noticeable, roughly changing the vibration by 30–100%. Use the same weight every time, and
   put it at the same radius.

## Procedure

Use the same fan speed for every run. Run the fan at a speed you can hold steady (a fixed speed
setting on the hood works well). Check that `rpm` stays within about 1–2% between runs. If it
doesn't, the amplitudes can't be compared.

For each run:
1. Start the fan and let it settle for 20–30 seconds.
2. Record 4–6 lines of output and **average the amplitude** of the axis you are using.
3. Stop the fan and wait for the rotor to stop completely before touching it.

Pick **one axis** (the one with the largest amplitude, normally a radial one) and use that same
axis for every run.

| Run | What is on the rotor              | Record |
|-----|-----------------------------------|--------|
| 0   | Nothing (original)                | `a0`   |
| 1   | Trial weight at A (0°)            | `a1`   |
| 2   | Trial weight at B (120°)          | `a2`   |
| 3   | Trial weight at C (240°)          | `a3`   |

Remove the trial weight completely between runs.

## Calculating the correction

Let `m` be the mass of the trial weight. The trial weight's own effect is `t`:

```
t² = (a1² + a2² + a3²) / 3 − a0²
```

If `t²` comes out zero or negative, the trial weight is too small, or the readings were too noisy.
Use a bigger trial weight, or average more lines per run, and redo the runs.

Then find the position of the heavy spot:

```
p1 = (a1² − a0² − t²) / (2t)
p2 = (a2² − a0² − t²) / (2t)

x = p1
y = (p2 + 0.5·x) / 0.866

angle = atan2(y, x)           // degrees from position A, in the direction you numbered B and C
size  = sqrt(x² + y²)          // this is the original vibration a0 (a sanity check)
```

Correction:
- **Weight to add:** `m_corr = m · a0 / t`
- **Where to add it:** at `angle + 180°` from position A, in the same direction you numbered B and C.
  (This is directly opposite the heavy spot. Alternatively, take away `m_corr` of material at
  `angle`.)

### Quick Python version

```python
import math

def correction(a0, a1, a2, a3, m):
    t2 = (a1**2 + a2**2 + a3**2) / 3 - a0**2
    if t2 <= 0:
        raise ValueError("trial weight too small or readings too noisy")
    t = math.sqrt(t2)
    p1 = (a1**2 - a0**2 - t2) / (2 * t)
    p2 = (a2**2 - a0**2 - t2) / (2 * t)
    x = p1
    y = (p2 + 0.5 * x) / math.sin(math.radians(120))
    heavy = math.degrees(math.atan2(y, x)) % 360
    return (heavy + 180) % 360, m * a0 / t   # (add-weight angle, add-weight mass)

# example: amplitudes in g, trial weight 5 g
print(correction(0.30, 0.42, 0.18, 0.35, 5))
```

## Check and refine

1. Fix the correction weight at the calculated angle and radius.
2. Run the fan again at the same speed and record `a_new`. It should be much lower than `a0`.
3. If it is still too high, treat the new state as the starting point: use `a_new` as `a0`, keep the
   correction weight on, and repeat the four runs with your trial weight to find a smaller extra
   correction. One or two passes usually gets there.
4. When satisfied, fix the weight permanently (epoxy or a screw/rivet in place, or balancing clips),
   and re-check after it has cured.

## Tips and pitfalls

- **Same speed every run.** Vibration amplitude changes strongly with speed. This is the most
  common source of nonsense results.
- **Don't change anything else** between runs: the sensor position, the fan mounting, the
  settings.
- Run-to-run noise of 5–10% is normal. If your trial weight changes the amplitude by less than that,
  use a heavier one.
- If the reported `rpm` jumps around from window to window, the strongest peak isn't the rotor.
  Something else (motor hum, a resonance, the room) is dominating. Try a different speed, a stiffer
  sensor mount, or limit the search band with `MIN_HZ` and `MAX_HZ` in the sketch.
- The signal is only valid when the rotor is spinning at a steady speed. Ignore lines printed
  during spin-up or spin-down.
- A rotor with a bent shaft, worn bearings or an uneven blade can't be fully fixed by adding weight.
- With a tach added later, a single run per weight would give the heavy spot directly.
