No no particular order.

DONE Implement std::byteswap
DONE Use Saturation arithmetic 


Salvage doc, ideas code from ~/src/hex and revive hexagon effort.
  * Split gfx for hex QRS addressing into gfxhex.h
  * Distill HTML demos to C++.
  * Weirdly, I think my games matrix effects are here. :-/
  * Create a Q-Bert isometric cube effect for Hex grids.

Mine the AP work from /Users/robertlipe/src/tmp/src/wifi-apA
   On branch wifi-ap
 git remote -v:
  origin	https://github.com/robertlipe/NightDriverStrip.git (fetch)
  origin	https://github.com/robertlipe/NightDriverStrip.git (push)
  upstream	https://github.com/PlummersSoftwareLLC/NightDriverStrip.git (fetch)
  upstream	https://github.com/PlummersSoftwareLLC/NightDriverStrip.git (push)

Mind /Users/robertlipe/src/tmp/src/rpi
  40 branches. :-(

Mine /Users/robertlipe/tmp/src/nightdriverstrip - see if there's anything
  there remaining to be salvaged. Potentially delete.

Mine each of /Users/robertlipe/src/nightdriverstrip/.worktrees
  (Probably each one a project...) for ideas, code worth salvaging.

Mine /Users/robertlipe/src/tinyled -  harvest 'tinyled' work.
  ➜  tinyled git:(tinyled) ✗ git branch --list | wc -l
      41

Mine  /Users/robertlipe/tmp/ld-virgin/NightDriverStrip
  11 branches (looks like low value)

Mine /Users/robertlipe/tmp/src/ndstrip-test
  2 branches (looks like low value)

Finally: Run  locate gfxbase.cpp - find yet OTHER trees to salvage

Low priority
  /Users/robertlipe/src/tmp/src/build - see if anything is worth salvaging.
- [ ] FastLED Tech Debt: Refactor the 'bottom half' of FastLED to address reinterpret_cast warnings/errors.
- [x] sensors.cpp Surgery: Refactor the merge-collision issue in sensors.cpp.
- [ ] FFT Optimization: Evaluate replacing kosme/arduinoFFT with Espressif’s esp-dsp (LX7 PIE optimized).
- [ ] Matrix Effects Modernization (ATMega Legacy / "ESP32-edness"):
    - Review and optimize `*SM*` (SmartMatrix) patterns. Many were originally written for ATMega and lack ESP32 hardware advantages (e.g., single-point hardware floats).
    - Refactor heavy effects (`PatternLife`, `PatternSMStarDeep`, `PatternSMFire2021/2012`, `PatternSMNoise*`) where appropriate.
    - Evaluate `PatternSMStarDeep` for leveraging precomputed radial math from `gfxbase` (or `gfxmatrix`) to remove heavy per-frame trig calculations.
    - Standardize array flipping: use `std::swap` (or similar standard algorithms) instead of manual buffer copying in heavy effects like `PatternLife`.
    - Evaluate bounds checking, wrapping logic, and `uint8_t` iterators vs `int`. Safe pixel-setting methods are ideal, but if `g()->drawPixel` harms 60fps performance on hot paths, explore optimized manual bounds-checking and wrapping alternatives to mitigate the risks of unchecked array access (`g()->leds[XY(...)]`).
