#include <iostream>
#include <cstdint>
#include <array>
#include <stack>

/**
 * A bitboard is a low resolution chess board. That is, a bitboard has the structure of a chess board (8x8 squares)
 * but does not posses the capability of storing exact piece type and color. Instead, a square on a bitboard is
 * considered either "marked" or "unmarked", based on the value of the bit (0 or 1) corresponding to the square.
 *
 * <pre> \n
 * _         Black         \n
 * Queenside      Kingside \n
 * 56 57 58 59 60 61 62 63 \n
 * 48 49 50 51 52 53 54 55 \n
 * 40 41 42 45 44 45 46 47 \n
 * 32 33 34 35 36 37 38 39 \n
 * 24 25 26 27 28 29 30 31 \n
 * 16 17 18 19 20 21 22 23 \n
 * 8  9  10 11 12 13 14 15 \n
 * 0  1  2  3  4  5  6  7  \n
 * Queenside      Kingside \n
 * _         White         \n
 * </pre> \n
 *
 * The square labeled with <i>n</i> is marked by setting the <nobr>(<i>n</i> + 1)th</nobr> least significant bit in
 * the bitboard. \n\n
 *
 * The clearest way to mark a square in a bitboard is using bitwise OR in conjunction with the <code>sbitboard</code>
 * function.
 */
using bitboard = std::uint64_t;

using symmetric4_lookup_table = std::array<std::array<bitboard, 4096 /* 2^(12) */>, 16>;

/** Creates a singleton bitboard. That is, a bitboard where only a single square is marked. */
constexpr bitboard sbitboard(std::uint8_t square_index) { return static_cast<bitboard>(1) << square_index; }

/**
 * Converts a rank-file coordinate to a square index, which is the location of the bit corresponding to the coordinate
 * within a bitboard.  \n\n
 * Ranks are indexed [0, 7] beginning with the white edge of the board. \n\n
 * Files are indexed [0, 7] beginning with the queenside edge of the board.\n\n
 *
 * This function is intended for use during move pre-generation, where it is natural to think in terms of files and
 * ranks, and not necessarily square indices. Conversely, this function, and rank-file coordinates in general,
 * should not be used at runtime, in an effort to reduce unnecessary repetitive computation during game-tree construction.
 */
consteval uint8_t sindex_from_coords(std::uint8_t rank, std::uint8_t file) { return rank * 8 + file; }

void print_bitboard(const bitboard board) {
    for (std::uint8_t rank = 8; rank > 0; --rank) {
        const uint8_t rank_begin_sindex = rank * 8;
        const uint8_t rank_end_sindex = rank_begin_sindex + 8;
        for (std::uint8_t sindex = rank_begin_sindex; sindex < rank_end_sindex; ++sindex) {
            std::cout << (((board & sbitboard(sindex)) > 0) ? '1' : '0');
            std::cout << "  ";
        }
        std::cout << std::endl;
    }
}

consteval std::array<bitboard, 64> generate_knight_move_table() {
    std::array<bitboard, 64> table {};

    for (std::uint8_t knight_file = 0; knight_file < 8; ++knight_file) {
        for (std::uint8_t knight_rank = 0; knight_rank < 8; ++knight_rank) {
            bitboard moves = 0;

            // The following comments refer to knight moves using the format "towards x-y" where
            // x = the direction which the knight moves two spaces.
            // y = the direction which the knight moves one space.
            // x, y are both in [black, kingside, white, queenside], the set of chess board cardinal directions.
            // Also, recall that the rank approaches the black-side border as it increases.
            // And the file approaches the kingside of the board as it increases.
            // The origin is (rank = 0, file = 0) and it refers to the furthest white queenside space.

            // There exists a towards black-queenside knight move.
            if (knight_file > 0 && knight_rank < 6)
                moves |= sbitboard(sindex_from_coords(knight_rank + 2, knight_file - 1));

            // There exists a towards black-kingside knight move.
            if (knight_file < 7 && knight_rank < 6)
                moves |= sbitboard(sindex_from_coords(knight_rank + 2, knight_file + 1));

            // There exists a towards queenside-black knight move.
            if (knight_file < 6 && knight_rank < 7)
                moves |= sbitboard(sindex_from_coords(knight_rank + 1, knight_file + 2));

            // There exists a towards kingside-black knight move.
            if (knight_file > 1 && knight_rank < 7)
                moves |= sbitboard(sindex_from_coords(knight_rank + 1, knight_file - 2));

            // There exists a towards white-queenside knight move.
            if (knight_file > 0 && knight_rank > 1)
                moves |= sbitboard(sindex_from_coords(knight_rank - 2, knight_file - 1));

            // There exists a towards white-kingside knight move.
            if (knight_file < 7 && knight_rank > 1)
                moves |= sbitboard(sindex_from_coords(knight_rank - 2, knight_file + 1));

            // There exists a towards queenside-white knight move.
            if (knight_file > 1 && knight_rank > 0)
                moves |= sbitboard(sindex_from_coords(knight_rank - 1, knight_file - 2));

            // There exists a towards kingside-white knight move.
            if (knight_file < 6 && knight_rank > 0)
                moves |= sbitboard(sindex_from_coords(knight_rank - 1, knight_file + 2));

            table[sindex_from_coords(knight_rank, knight_file)] = moves;
        }
    }
    return table;
}

constinit std::array<bitboard, 64> knight_move_table = generate_knight_move_table();

using symmetric4_slider_lookup_table = std::array<std::array<bitboard, 4096 /* 2^(12) */>, 16>;

enum class capture_type: uint8_t { NONE = 0, ROOK = 1, KNIGHT = 2, BISHOP = 3, QUEEN = 4, PAWN = 5 };

class move {
    private:
        /**
         * <h2>Complete & Space-Efficient Move Representation</h2>
         * <h3>Origin & Destination Squares</h3>
         * <p>There are 64 squares on a chess board, which means, for every move, there are 64 possible
         * origin squares, and 63 possible destination squares. log2(64) = 6, therefore 6 bits are sufficient for
         * describing the origin square. Similarly, ceil(log2(63)) = 6, so six bits must be reserved for describing
         * the destination square.</p>
         * <h3>Capture</h3>
         * <p>A move can potentially result in a capture. In order to fully describe the move such that
         * it can be undone in the future, the type of piece which was captured must also be stored.
         * Captureable Pieces (C) = [Rook, Knight, Bishop, Queen, Pawn, None]. |C| = 6. Since ceil(log2(6)) = 3,
         * three bits must be reserved for this purposes.</p>
         *
         * <h3>Memory Layout</h3>
         * The following table illustrates the layout of the move data over <code>uint16_t</code>.
         * <pre>\n
         * n  | 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 \n
         * v  | () (capt..) (to          ) (from     )
         * \n\n</pre>
         * Where <i>v</i> describes the value occupying the <i>n</i>th least significant bit.
         */
        std::uint16_t data;
    public:
        move(const uint8_t origin, const uint8_t destination, const capture_type capture) {
            data = static_cast<uint16_t>(capture) << 12 | static_cast<uint16_t>(destination) << 6 | origin;
        }
        [[nodiscard]] uint8_t origin() const { return data & 0b111111; }
        [[nodiscard]] uint8_t destination() const { return (data >> 6) & 0b111111; }
        [[nodiscard]] capture_type capture() const { return static_cast<capture_type>((data >> 12) & 0b111); }
};

struct game_state {
    uint64_t white;
    uint64_t black;

    uint64_t rook;
    uint64_t bishop;
    uint64_t knight;
    uint64_t pawn;
    uint64_t king;
    uint64_t queen;

    std::stack<move> move_log;
};

enum class piece_color: bool { white = true, black = false };

void compute_valid_moves(const game_state& state) {
    bitboard self_occupancy, opponent_occupancy;

    const piece_color self = state.move_log.size() % 2 == 0 ? piece_color::white : piece_color::black;
    switch (self) {
        case piece_color::white:
            self_occupancy = state.white;
            opponent_occupancy = state.black;
            break;
        case piece_color::black:
            self_occupancy = state.black;
            opponent_occupancy = state.white;
            break;
    }

    // All the pieces which may be legally moved. In general this is the set of all self pieces less
    // those which are pinned to the king. Moving an absolutely pinned piece is illegal since it violates
    // the rule which prohibits moving oneself into check.
    bitboard movable;

    bitboard valid_destinations = ~self_occupancy;
}




int main() {
    print_bitboard(knight_move_table[sindex_from_coords(3, 3)]);
    return 0;
}
