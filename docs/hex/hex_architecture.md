# Hexagonal Matrix Architecture & Visual Design

This document details the hardware geometry, coordinate mathematics, compile-time graphics engine, and visual design principles for the 271-pixel hexagonal LED matrix in NightDriver Afterglow.

---

## 1. Physical Hardware & Geometry

The hexagonal display is a custom PCB carrying **271 WS2812B LEDs** arranged in a flat-top hexagonal lattice.

* **Concentric Rings:** 10 rings (Ring 0 at center, Rings 1–9 expanding outward).
* **Outer Edge:** 10 LEDs along each of the 6 outer edges ($N = 10$).
* **Total LED Count:**
  $$\text{Total} = 1 + 3 \times N \times (N - 1) = 1 + 3 \times 10 \times 9 = 271 \text{ pixels}$$
* **Row Configuration:** 19 horizontal rows scanned in a serpentine layout. Rows taper symmetrically:
  $$\text{Row Lengths} = \{10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10\}$$
* **Serpentine Wiring:** Even rows scan Left-to-Right; odd rows scan Right-to-Left.

---

## 2. Why Cartesian (X, Y) Breaks Down

Traditional 2D graphics libraries assume an orthogonal, square Cartesian grid:
* **Row Staggering:** Every adjacent horizontal row is shifted by half a pixel width ($0.5 \times \text{pitch}$), meaning standard integer grid math shears circles into skewed ovals.
* **Anisotropic Distance:** On a square grid, diagonal neighbors are $\sqrt{2} \approx 1.414\times$ farther away than orthogonal neighbors. On a hex grid, all 6 neighbors are strictly **equidistant** ($1.000\times$).
* **Rotational Glitches:** 90° rotations on Cartesian grids are trivial integer swaps; 60° rotations on Cartesian grids require floating-point trigonometry, rounding errors, and ugly aliasing artifacts.

---

## 3. The Mathematical Solution: Cube / Axial (QRS) Coordinates

Following the elegant geometry formulated by Amit Patel (RedBlobGames), the display is modeled using a 3D cube coordinate system constrained to the 2D plane $q + r + s = 0$:

```
              +s  / -r
                 /
                /
  -q __________/__________ +q
              /
             /
            /
          +r  / -s
```

* **Coordinates:** Stored as `HexCoord(int q, int r)` where $s = -q - r$.
* **Hexagonal Distance Metric:**
  $$\text{Distance}(a, b) = \max(|a.q - b.q|,\; |a.r - b.r|,\; |a.s - b.s|) = \frac{|a.q - b.q| + |a.r - b.r| + |a.s - b.s|}{2}$$
  Distance from center $(0,0,0)$ simplifies to:
  $$d = \max(|q|, |r|, |s|)$$
* **Equidistant Neighbors:** Every cell has exactly 6 neighbors at distance 1:
  $$\text{HEX\_DIRECTIONS} = \{(1, 0), (1, -1), (0, -1), (-1, 0), (-1, 1), (0, 1)\}$$
* **Trig-Free 60° Rotations:** Rotating a coordinate 60° clockwise around the origin requires **zero trigonometry**—it is a pure cyclic coordinate negation and swap:
  $$(q, r, s) \longrightarrow (-r, -s, -q)$$
  A full 360° rotation consists of 6 discrete permutations, executing in single-digit CPU clock cycles without float rounding error.

---

## 4. Compile-Time Mastery (`constexpr` & `consteval`)

Rather than recalculating geometric projections and prefix sums dynamically in the rendering loop, `HexagonGFX` (`include/gfxhex.h` / `src/gfxhex.cpp`) resolves the entire grid geometry at compile time:

1. **Row Lengths & Prefix Sums:**
   * `generateRowLengths()`: Computes the 19 row capacities at compile time.
   * `generateCumulativeSums()`: Precomputes the 20-element cumulative prefix sum array. Linear index lookup from offset row/col is $O(1)$ arithmetic without loops.
2. **Precomputed Rings Table in Flash:**
   * `generatePrecomputedRings()`: Uses `consteval` to generate a static flattened array of all 271 `HexCoord` positions ordered ring-by-ring, complete with offset and size lookup arrays.
   * `getHexRing(radius)` returns a lightweight `HexCoordView` pointing directly to read-only `.rodata` in Flash memory—**zero heap allocation**, zero RAM usage.
3. **Bi-directional Index Mapping:**
   * `generateIndexToHex()`: Statically bakes the 1D serpentine LED index $\leftrightarrow$ 2D `HexCoord` axial coordinate mapping into Flash.

---

## 5. Visual Design Grammar & The "Fence vs. Field" Reality

### The "Fence to Field" Ratio
With 271 pixels and a maximum radius of 9 LEDs from the center to any vertex:
* **The Problem:** Classic algorithms designed for open 2D spaces struggle. For example:
  * **Boids:** With only 9 pixels of runway, flocking agents spend 90% of their time colliding with the perimeter fence rather than interacting as an emergent flock—looking more like gold dust swirling in a prospector's pan.
  * **Cyclic Cellular Automata (CCA):** Spiral rotor waves require spatial volume to nucleate. Absorbing boundary conditions at radius 9 immediately extinguish rotor curves, collapsing the system into monotonic diagonal barbershop stripes within seconds.
* **The Solution:** Effects that embrace the structure of the geometry thrive. See the 25 production implementations in [include/effects/hex/](../../include/effects/hex/).

### What Works Exceptionally Well:

1. **6-Fold Rotational Symmetry (C6 Kaleidoscopes):**
   * Generate a procedural field (or simple spark/line) in a single 60° sector.
   * Replicate it across all 6 sectors via `hexRotate()`.
   * Introduce subtle phase drift across sectors (symmetry breaking) to cycle between perfect order and organic chaos.
   * **Implementations:**
     * [`PatternHexKaleidoscope.h`](../../include/effects/hex/PatternHexKaleidoscope.h): Classic dynamic mandala reflecting sector math across all 6 axes.
     * [`PatternHexGeometry.h`](../../include/effects/hex/PatternHexGeometry.h): Shifting polygons, cones, and rotating geometric frames.
     * [`PatternHexSnowflake.h`](../../include/effects/hex/PatternHexSnowflake.h): Algorithmic crystal formation utilizing C6 radial mirroring.

2. **Radial Distance & Wave Interference:**
   * Concentric shockwaves and standing waves emit from the center or vertices.
   * Concentric hex rings expand organically without square-matrix aliasing steps.
   * **Implementations:**
     * [`PatternHexRipple.h`](../../include/effects/hex/PatternHexRipple.h): Pebble drops causing harmonic interference ripples across the lattice.
     * [`PatternHexPulse.h`](../../include/effects/hex/PatternHexPulse.h): Breathing concentric rings expanding and fading smoothly.
     * [`PatternHexFireworks.h`](../../include/effects/hex/PatternHexFireworks.h): Center-launched shells exploding into expanding radial rings.
     * [`PatternHexBlackHole.h`](../../include/effects/hex/PatternHexBlackHole.h): Gravitational radial lensing drawing energy inward.

3. **Angular Phase & Mechanical Iris:**
   * Rotating sectors and radar sweeps line up seamlessly with the physical edges of the PCB.
   * Phase-shifted rings: Each concentric ring drifts with its own rotational velocity, creating the illusion of an opening and closing camera iris.
   * **Implementations:**
     * [`PatternHexRadar.h`](../../include/effects/hex/PatternHexRadar.h): Sweeping wedge with anti-aliased leading edge and phosphorescent phosphor decay tail.
     * [`PatternHexSpiral.h`](../../include/effects/hex/PatternHexSpiral.h): Hyperspace wormhole rotating along the 6 natural axes.
     * [`PatternHexClock.h`](../../include/effects/hex/PatternHexClock.h): Hex-native timekeeper using quantized angular hour and minute hands.

4. **Organic Procedural Flow & Noise:**
   * Smooth noise mapping that hugs the physical curvature rather than clashing with borders.
   * **Implementations:**
     * [`PatternHexPlasma.h`](../../include/effects/hex/PatternHexPlasma.h): FastLED multi-variable plasma mapped across axial distance.
     * [`PatternHexLiquid.h`](../../include/effects/hex/PatternHexLiquid.h): Fluid turbulence modeling swirling color masses.
     * [`PatternHexFlowField.h`](../../include/effects/hex/PatternHexFlowField.h): Vector field particle flow guided by underlying noise gradients.

5. **Equidistant Discrete Automata:**
   * Cellular automata algorithms where all 6 neighbor directions have identical, isotropic distances ($1.000\times$).
   * **Implementations:**
     * [`PatternHexLife.h`](../../include/effects/hex/PatternHexLife.h): Conway's Game of Life adapted to 6-neighbor hex neighborhoods.
     * [`PatternHexAnt.h`](../../include/effects/hex/PatternHexAnt.h): Langton's Ant navigating hexagonal state transitions.
     * [`PatternHexMaze.h`](../../include/effects/hex/PatternHexMaze.h): Dynamic maze generator carving paths through hex corridors.

6. **Audio Reactivity:**
   * **Axial Energy Flow:** Bass transients and high-hat hits shoot outward along the 6 primary hexagonal axes at 60° increments.
   * **Radial Frequency Rings:** Low frequencies map to hot core rings, mid-tones pulse the mid-rings, and treble flickers the outer perimeter.
   * **Implementation:** [`PatternHexAudio.h`](../../include/effects/hex/PatternHexAudio.h) (pulsating hot core and outward expanding frequency rings).

7. **High-Contrast Palette Quantization:**
   * Low-brightness WS2812 PWM output can look muddy. High-contrast, sharp color ramps with clean blacks look vibrant and modern (demonstrated across [`PatternHexTesla.h`](../../include/effects/hex/PatternHexTesla.h) and [`PatternHexVoronoi.h`](../../include/effects/hex/PatternHexVoronoi.h)).

---

## 6. Prototyping & Development Workflow

To avoid the cycle of flashing firmware over serial just to tweak animation formulas:

1. **Browser Sandbox (`docs/hex/hexproto.html`):**
   * A 74-line HTML5/Canvas emulator rendering the exact 271-pixel flat-top geometry.
   * Allows rapid testing of wave interference, color math, and rotation speed in JavaScript with instant `Cmd+R` feedback.
2. **Headless Verification (`tools/nightdriver_client.py`):**
   * Connects to the device's color data socket, captures raw `CRGB` frames, and generates tiled 2D PNG contact sheets or GIFs for visual regression testing and frame-by-frame analysis.
