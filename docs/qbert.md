Feasability discussion with Gemini to figure out if Q-bert would
be viable on our tinyled-hexagon configuration. TL'DR: Yes!

  We absolutely have enough pixels! In fact, 271 pixels is not just enough—the math works out almost poetically.

  ### The Math: 28 Blocks is the Magic Number

  Consider the original 1982 Gottlieb arcade machine:

  • Row 1: 1 cube
  • Row 2: 2 cubes
  • Row 3: 3 cubes
  • Row 4: 4 cubes
  • Row 5: 5 cubes
  • Row 6: 6 cubes
  • Row 7: 7 cubes
  • Total: Exactly 28 cubes!

  On your 271-pixel hexagon (10 rings, 19 rows tall), a 7-row (28 hexes) or 8-row (36 hexes) or 9-row (45 hexes)
  triangular pyramid fits comfortably in the upper/center portion of the grid.

  Having the pyramid take up ~28 to 45 pixels, surrounded by the remaining ~220+ dark pixels, is actually ideal:

  1. The Void: Q*bert was famous for floating in a pitch-black cosmic void. The unlit hexes give him room to hop off the
  edge and plummet into the abyss!
  2. Escape Discs: The unused hexes just off the sides of rows 5 and 7 can serve as the spinning rainbow escape discs
  that whisk Q*bert back to the apex.
  3. Legibility: If every single hex on the board were a block, it would feel like a flat honeycomb. But carving a
  lighted pyramid out of the black field instantly reads as an isometric 3D pyramid to the human eye.
  ──────
  ### How Q-Hex / "Q*bert" Would Work on Flat-Top Hexes

  On your flat-topped hex grid, movement is staggered by 60°. In classic Q*bert, there are only 4 movement directions
  (the isometric diagonals: ↗, ↘, ↙, ↖).
  In hex coordinates (q, r):

  • Down-Right: HexCoord(0, 1)
  • Down-Left: HexCoord(-1, 1)
  • Up-Right: HexCoord(1, -1)
  • Up-Left: HexCoord(0, -1)

  (Leaving pure Left and pure Right out of Q*bert's repertoire maintains the iconic diagonal hop!)

  ### The Cast of Single-Pixel Characters:

  • Q*bert (Bright Orange/Amber):
      • Hops from hex to hex on a timer.
      • Hopping animation: Quick brightness swell and dimming trail so you see the "arc" of the jump.
      • When he lands: The hex toggles state (e.g., Deep Navy Blue → Glowing Canary Yellow).
  • Coily (Purple):
      • Starts as a bouncing purple ball dropping down row by row.
      • Reaches the bottom, pauses, and "hatches" into a relentless purple hunter that follows Q*bert up the pyramid.
  • Red Bouncing Balls:
      • Drop from the top, bouncing downward toward the bottom edge.
  • Slick & Sam (Lime Green):
      • Little green jerks that bounce down and revert yellow pads back to blue.
  • Round Clear / Strobe:
      • Once all 28 pads turn yellow, the whole pyramid flashes in victory cycling colors, and a new round begins!
  • The @!#?@! Death Stutter:
      • When caught or jumping into the void, a rapid red stutter flash and a chirp/tone if audio is enabled.


  Because you already have the coordinate transforms, neighbor checks, and bounded grid math in gfxhex.h (and
  AI/wandering logic in PatternHexSnake and PatternHexAnt), an autonomous Q*bert simulation is completely feasible.

  It would be one of the coolest retro tributes possible on that hardware!******

%s/ *$//

