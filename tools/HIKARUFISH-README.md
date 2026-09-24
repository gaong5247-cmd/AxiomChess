# HikaruFish 2792 prototype

This is an experimental Stockfish-derived build aimed at **human-like elite play** rather than maximum engine Elo.

## Version 1 behavior

- Defaults to `UCI_LimitStrength=true`
- Defaults to `UCI_Elo=2792`
- Uses up to 8 root candidates while strength limiting
- Keeps candidates within roughly 45 centipawns of the best searched move
- Adds only small root-selection preferences for:
  - checking moves
  - active captures
  - centralization
  - forward pawn space
- Adds tiny bounded randomness among close candidates
- Forced mates are protected from style noise

The Stockfish search and NNUE remain intact. The style layer acts only at root move selection.

## Important

"2792" is a target/calibration setting, **not proof of real-world FIDE 2792 playing strength**. Engine Elo depends on time control, hardware, opponent pool, and testing protocol.

## License

Derived from Stockfish and distributed under GPLv3. See `COPYING-STOCKFISH-GPLv3.txt`.

Build trigger: 2026-09-25
