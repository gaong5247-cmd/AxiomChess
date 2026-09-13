#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace axiom {
enum Piece { Empty=0, Pawn=1, Knight=2, Bishop=3, Rook=4, Queen=5, King=6 };
constexpr int White=1, Black=-1;
constexpr int WK=1, WQ=2, BK=4, BQ=8;
inline bool valid(int s) { return s >= 0 && s < 128 && !(s & 0x88); }
inline int color(int p) { return (p>0)-(p<0); }
struct Move {
    int from=-1, to=-1, promotion=0;
    bool operator==(const Move&) const = default;
    explicit operator bool() const { return valid(from) && valid(to); }
    std::string uci() const;
};
std::string square_name(int s);
int parse_square(const std::string& s);
struct Undo {
    std::array<int,128> squares;
    int side, castle, ep, halfmove, fullmove;
    std::size_t history_size;
};
class Board {
public:
    std::array<int,128> squares{};
    int side=White, castle=15, ep=-1, halfmove=0, fullmove=1;
    std::vector<std::uint64_t> history;
    std::vector<std::string> identities;
    // Execution policy, not chess state. No cached facts survive a call/mutation.
    bool legal_fast_path=false;
    Board();
    explicit Board(const std::string& fen);
    void set_fen(const std::string& fen);
    std::string fen() const;
    std::uint64_t hash() const;
    std::string proof_key() const;
    bool attacked(int sq, int by) const;
    bool piece_attacks(int from,int to) const;
    int king_square(int who) const;
    bool in_check() const { return attacked(king_square(side), -side); }
    std::vector<Move> legal_moves();
    std::vector<Move> legal_moves_reference();
    std::vector<Move> legal_captures_to(int target);
    bool capture(Move m) const;
    bool gives_check(Move m);
    Undo push(Move m,bool record_history=true);
    Undo push_null();
    void pop(const Undo& u);
    std::optional<Move> parse_move(const std::string& uci);
    int repetitions() const;
    bool insufficient() const;
    bool automatic_draw() const;
    bool claim_now() const;
    bool can_claim_draw(bool legal_known=false);
    int piece_count() const;
    static constexpr const char* StartFen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
private:
    std::vector<Move> pseudo_moves() const;
    bool legal_ep_exists() const;
    std::string repetition_identity() const;
};
std::uint64_t perft(Board& board, int depth);
}
