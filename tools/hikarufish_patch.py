#!/usr/bin/env python3
from pathlib import Path

def replace_one(path, old, new):
    p = Path(path)
    s = p.read_text(encoding="utf-8")
    if old not in s:
        raise SystemExit(f"patch anchor not found: {path}: {old[:80]!r}")
    s2 = s.replace(old, new, 1)
    p.write_text(s2, encoding="utf-8")

# Brand the engine while preserving Stockfish attribution.
replace_one("src/misc.cpp",
'''    ss << "Stockfish " << version << std::setfill('0');''',
'''    ss << "HikaruFish 2792 (Stockfish-based) " << version << std::setfill('0');''')

replace_one("src/misc.cpp",
'''         + "the Stockfish developers (see AUTHORS file)";''',
'''         + "HikaruFish prototype; based on Stockfish by the Stockfish developers";''')

# Default to the requested 2792 strength.
replace_one("src/engine.cpp",
'''    options.add("UCI_LimitStrength", Option(false));''',
'''    options.add("UCI_LimitStrength", Option(true));''')

replace_one("src/engine.cpp",
'''    options.add("UCI_Elo",
                Option(Stockfish::Search::Skill::LowestElo, Stockfish::Search::Skill::LowestElo,
                       Stockfish::Search::Skill::HighestElo));''',
'''    options.add("UCI_Elo",
                Option(2792, Stockfish::Search::Skill::LowestElo,
                       Stockfish::Search::Skill::HighestElo));''')

# Pass the root position into the selector so style scoring can inspect moves.
replace_one("src/search.h",
'''    Move pick_best(const RootMoves&, usize multiPV);''',
'''    Move pick_best(const RootMoves&, usize multiPV, const Position&);''')

# Search a wider human candidate set than vanilla limited-strength Stockfish.
replace_one("src/search.cpp",
'''    if (skill.enabled())
        multiPV = std::max(multiPV, usize(4));''',
'''    if (skill.enabled())
        multiPV = std::max(multiPV, usize(8));''')

replace_one("src/search.cpp",
'''            skill.pick_best(rootMoves, multiPV);''',
'''            skill.pick_best(rootMoves, multiPV, rootPos);''')

replace_one("src/search.cpp",
'''                             skill.best ? skill.best : skill.pick_best(rootMoves, multiPV)));''',
'''                             skill.best ? skill.best : skill.pick_best(rootMoves, multiPV, rootPos)));''')

old = '''Move Skill::pick_best(const RootMoves& rootMoves, usize multiPV) {
    static PRNG rng(now());  // PRNG sequence should be non-deterministic

    // With tablebases at the root, rootMoves are ordered by tbRank rather
    // than by score, so compute the score range explicitly to keep 'delta'
    // non-negative.
    Value topScore = rootMoves[0].score;
    Value minScore = rootMoves[0].score;
    for (usize i = 1; i < multiPV; ++i)
    {
        topScore = std::max(topScore, rootMoves[i].score);
        minScore = std::min(minScore, rootMoves[i].score);
    }
    int    delta    = std::min(topScore - minScore, int(PawnValue));
    int    maxScore = -VALUE_INFINITE;
    double weakness = 120 - 2 * level;

    // Choose best move. For each move score we add two terms dependent on
    // weakness. One is deterministic and bigger for weaker levels, and one
    // is random. Then we choose the move with the resulting highest score.
    for (usize i = 0; i < multiPV; ++i)
    {
        // This is our magic formula
        int push = int(weakness * int(topScore - rootMoves[i].score)
                       + delta * (rng.rand<unsigned>() % int(weakness)))
                 / 128;

        if (rootMoves[i].score + push >= maxScore)
        {
            maxScore = rootMoves[i].score + push;
            best     = rootMoves[i].pv[0];
        }
    }

    return best;
}'''

new = '''Move Skill::pick_best(const RootMoves& rootMoves, usize multiPV, const Position& pos) {
    static PRNG rng(now());

    // HikaruFish philosophy:
    //   * Stockfish still does the calculation.
    //   * We only choose among already-strong root candidates.
    //   * Never manufacture large tactical blunders just to look "human".
    //   * Prefer active, forcing and central moves when the objective scores are close.
    Value topScore = rootMoves[0].score;
    for (usize i = 1; i < multiPV; ++i)
        topScore = std::max(topScore, rootMoves[i].score);

    // If Stockfish has a forced mate, don't style-noise it away.
    if (is_mate_or_mated(topScore))
    {
        best = rootMoves[0].pv[0];
        return best;
    }

    // Roughly 45 centipawns expressed in Stockfish internal value units.
    // This is intentionally small: 2792-level humanization should mostly be
    // second/third-choice inaccuracies, not hanging pieces.
    const int lossLimit = int(PawnValue) * 45 / 100;

    int bestHumanScore = -VALUE_INFINITE;

    for (usize i = 0; i < multiPV; ++i)
    {
        const RootMove& rm = rootMoves[i];
        if (rm.pv.empty())
            continue;

        const int loss = int(topScore - rm.score);
        if (loss > lossLimit)
            continue;

        const Move m = rm.pv[0];
        int style = 0;

        // Forcing play and initiative.
        if (pos.gives_check(m))
            style += int(PawnValue) * 6 / 100;
        if (pos.capture_stage(m))
            style += int(PawnValue) * 2 / 100;

        // Centralization/activity. This is deliberately a small tie-breaker.
        const int f = int(file_of(m.to_sq()));
        const int r = int(rank_of(m.to_sq()));
        const int centerDistance = std::abs(2 * f - 7) + std::abs(2 * r - 7);
        style += std::max(0, 8 - centerDistance) * int(PawnValue) / 220;

        // Encourage active pawn space gains a little, without overriding eval.
        const PieceType pt = type_of(pos.moved_piece(m));
        if (pt == PAWN)
        {
            const int fromRank = int(relative_rank(pos.side_to_move(), rank_of(m.from_sq())));
            const int toRank   = int(relative_rank(pos.side_to_move(), rank_of(m.to_sq())));
            style += std::max(0, toRank - fromRank) * int(PawnValue) / 100;
        }

        // Small, bounded non-determinism is what makes repeated equal positions
        // less robotic. It is tiny compared with the 45cp admissibility gate.
        const int jitter = int(rng.rand<unsigned>() % 9) - 4;

        // Penalize objective loss strongly; style can only resolve close calls.
        const int humanScore = int(rm.score) - loss * 2 + style + jitter;

        if (humanScore >= bestHumanScore)
        {
            bestHumanScore = humanScore;
            best = m;
        }
    }

    // Defensive fallback should the candidate list be incomplete.
    if (!best)
        best = rootMoves[0].pv[0];

    return best;
}'''

replace_one("src/search.cpp", old, new)

print("HikaruFish patch applied successfully")
