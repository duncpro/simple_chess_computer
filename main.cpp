#include <iostream>
#include <cstdint>
#include <array>
#include <stack>

enum piece_color: bool { white = true, black = false };
constexpr piece_color operator!(piece_color original) { return static_cast<piece_color>(!static_cast<bool>(original)); }

/**
 * A bitboard is a low resolution chess board. That is, a bitboard has the structure of a chess board (8x8 squares)
 * but does not posses the capability of storing exact piece type and color. Instead, a square on a bitboard is
 * considered either "marked" or "unmarked", based on the value of the bit (0 or 1) corresponding to the square.
 *
 * <pre>\n
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
 * </pre>\n
 *
 * The square labeled with <i>n</i> is marked by setting the <nobr>(<i>n</i> + 1)th</nobr> least significant bit in
 * the bitboard. \n\n
 *
 * The clearest way to mark a square in a bitboard is using bitwise OR in conjunction with the <code>singleton</code>
 * function.
 */
using bitboard = std::uint64_t;

/**
 * A bitlane represents 8 consecutive bits of a bitboard.\n\n
 * On a <b>standard bitboard</b>, a bitlane typically describes the occupancy of a rank. The <nobr>(<i>n</i> + 1)th</nobr>
 * least significant bit corresponds to the <nobr>(<i>n</i> + 1)th</nobr> queenside-most square of a rank. \n\n
 * On a <b>rotated bitboard</b>, a bitlane typically describes the occupancy of a file.
 */
using bitlane = std::uint8_t;

/**
 * Creates a singleton bitlane. That is, a bitlane where only a single square is marked.
 * In the context of <b>standard bitboards</b> this returns a bitlane with the <nobr>(<i>n</i> + 1)th</nobr>
 * queenside-most square is marked.
 */
[[nodiscard]] constexpr bitlane sbitlane(const std::uint8_t n) { return ((bitlane) 1) << n; }

/**
 * Converts a rank-file coordinate the index of the bit corresponding to that coordinate within a bitboard.  \n\n
 * Ranks are indexed [0, 7] beginning with the white edge of the board. \n\n
 * Files are indexed [0, 7] beginning with the queenside edge of the board.\n\n
 */
[[nodiscard]] constexpr uint8_t coords_to_sindex(const std::uint8_t rank, const std::uint8_t file) { return rank * 8 + file; }

/** Converts a standard sindex to a rotated sindex and vice versa.  */
[[nodiscard]] constexpr std::uint8_t rotate_sindex(std::uint8_t original) {
    const uint8_t a = original >> 3;
    const uint8_t b = original & 0b111;
    return (b * 8) + a;
}

[[nodiscard]] consteval bitlane rank_literal(bool f0, bool f1, bool f2, bool f3, bool f4, bool f5, bool f6, bool f7) {
    bitlane rank = 0;
    if (f0) rank |= sbitlane(0);
    if (f1) rank |= sbitlane(1);
    if (f2) rank |= sbitlane(2);
    if (f3) rank |= sbitlane(3);
    if (f4) rank |= sbitlane(4);
    if (f5) rank |= sbitlane(5);
    if (f6) rank |= sbitlane(6);
    if (f7) rank |= sbitlane(7);
    return rank;
}

/** Creates a singleton bitboard. That is, a bitboard where only a single square is marked. */
[[nodiscard]] constexpr bitboard sbitboard(const std::uint8_t square_index) { return static_cast<bitboard>(1) << square_index; }

void print_bitboard(const bitboard board) {
    for (std::uint8_t rank = 8; rank > 0; --rank) {
        const uint8_t rank_begin_sindex = (rank - 1) * 8;
        const uint8_t rank_end_sindex = rank_begin_sindex + 8;
        for (std::uint8_t sindex = rank_begin_sindex; sindex < rank_end_sindex; ++sindex) {
            std::cout << ((board & sbitboard(sindex)) ? '1' : '0');
            std::cout << "  ";
        }
        std::cout << std::endl;
    }
}

void print_rank(const bitlane lane) {
    for (std::uint8_t file = 0; file < 8; file++) {
        std::cout << ((sbitlane(file) & lane) ? "1" : "0");
    }
    std::cout << "\n";
}

enum piece_type: uint8_t { rook = 0, knight = 1, bishop = 2, queen = 3, king = 4, pawn = 5, none = 6 };

/**
 * <h2>Space-Efficient, Forward, Chess Move Representation</h2>
 * <h3>Origin & Destination Squares</h3>
 * <p>There are 64 squares on a chess board, which means for every move there are 64 possible
 * origin squares, and 63 possible destination squares. log2(64) = 6, therefore 6 bits are sufficient for
 * describing the origin square. Similarly, ceil(log2(63)) = 6, so six bits must be reserved for describing
 * the destination square.</p>
 *
 * <h3>Promotion</h3>
 * <p>It is possible for a piece to transform in type after it has been moved.
 * Specifically, when a pawn which reaches the opposite end of the board it becomes a major/minor piece
 * of the player's choosing. A pawn may be promoted to a [Rook, Knight, Bishop, Queen].
 * Therefore, 3 bits are necessary to describe the desired promotion.</p>
 *
 * <h3>Memory Layout</h3>
 * <p>The following table illustrates the layout of the move data over <code>std::uint16_t</code>.
 * <pre>\n
 * n  | 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 \n
 * v  | () (promot) (destination) (origin    )
 * \n\n</pre>
 * Where <i>v</i> describes the value occupying the <i>n</i>th least significant bit.</p>\n\n
 */
class bitmove {
    private:
        std::uint16_t data;
    public:
        bitmove(const uint8_t origin, const uint8_t destination, const piece_type promote_to) {
            assert(origin < 64 && destination < 64 && origin != destination);

            data = (static_cast<uint16_t>(promote_to) << 12) |
                   (static_cast<uint16_t>(destination) << 6) |
                   static_cast<uint16_t>(origin);
        }
        [[nodiscard]] constexpr uint8_t unpack_origin() const { return data & 0b111111; }
        [[nodiscard]] constexpr uint8_t unpack_destination() const { return (data >> 6) & 0b111111; }
        [[nodiscard]] constexpr piece_type unpack_promotion() const { return static_cast<piece_type>((data >> 12) & 0b111); }

        [[nodiscard]] constexpr std::tuple<uint8_t, uint8_t, piece_type> unpack_all() const {
            return std::make_tuple(unpack_origin(), unpack_destination(), unpack_promotion());
        }
};

struct reversible_move {
    uint8_t origin;
    uint8_t destination;
    uint8_t target;
    piece_type captured_piece_type;
    bool is_promotion;
};

consteval auto generate_target_lookup_table() {
    std::array<uint8_t, 8 /* files */ * 2 /* colors */ + 64 /* identity targets */> table {};
    for (uint8_t file = 0; file < 8; file++) {
        table[file << piece_color::white] = coords_to_sindex(4, file);
        table[file << piece_color::black] = coords_to_sindex(3, file);
    }
    for (uint8_t i = 0; i < 64; i++) table[16 + i] = i;
    return table;
}
constinit auto target_lookup_table = generate_target_lookup_table();

/**
 * <h2>Target Square Calculation Function</h2>
 * <p>This function calculates the <b>target square</b> of the given move and returns its index.</p>
 * <p>For <b>capturing moves</b>, the <b>target square</b> is the square containing the piece which is captured once the
 * move is made.</p>
 * <p>For <b>non-capture moves</b>, the <b>target square</b> is equivalent to the <b>destination square</b>.</p>
 * <p>In traditional chess, the only <b>capturing move</b> where the <b>target square</b> differs from the <b>destination
 * square</b> is enpassant pawn capture.</p>
 * <h3>Assumptions</h3>
 * <p>This function assumes that the given input move is legal. Specifically, it assumes that if a pawn is switching
 * files, and there is no piece on the destination square, then the move is valid enpassant capture, regardless
 * of the origin and destination ranks. If a non-legal move is given as input, the return value is undefined.</p>
 */
uint8_t lookup_target(std::uint8_t origin, std::uint8_t destination, piece_type moved_piece_type,
                      piece_type destination_occupant_type, piece_color aggressor_color) {
    const bool is_enpassant = (moved_piece_type == piece_type::pawn) | ((origin & 0b111) != (destination & 0b111))
                              | (destination_occupant_type == piece_type::none);
    const uint8_t file = destination & 0b111;
    const uint8_t key = ((((file + 1) << aggressor_color) - 1) * is_enpassant) + (!is_enpassant * (16 + destination));
    return target_lookup_table[key];
}

struct chess_position {
    std::array<bitboard, 2> color_bitboard;

    std::array<bitboard, 2> color_bitboard_rotated;

    /**
     * <p>An array of bitboards indexed by <code>piece_type</code>. Each bitboard contains a mapping
     * of all the pieces of the associated type which exist on the board currently.</p>
     * <p>This array intentionally includes a bitboard associated with <code>piece_type::none</code>. It too
     * must be kept consistent by <code>make_move</code> and <code>unmake_move</code>.</p>
     */
    std::array<bitboard, 7> type_specific_bitboard;

    std::array<piece_type, 64> occupier_type_lookup_table;

    std::stack<reversible_move> move_log;

    /**
     * The player whose turn it is to move this turn (either white or black). A read from this field is functionally
     * equivalent to the following computation...
     * \n<code><pre>
     *      state.move_log.size() % 2 == 0 ? piece_color::white : piece_color::black;
     * </pre></code>\n
     */
    piece_color whos_turn;
};

void make_move(bitmove move, chess_position& position) {
    const auto [origin, destination, promote_to] = move.unpack_all();
    const bool is_promotion = promote_to != piece_type::none;
    const piece_type moved_piece_type = position.occupier_type_lookup_table[origin];
    const piece_type destination_occupant_type = position.occupier_type_lookup_table[destination];
    const piece_color opponent_color = !position.whos_turn;
    const uint8_t origin_rotated = rotate_sindex(origin);

    // The target and destination values are equivalent in all cases except en-passant.
    const uint8_t target = lookup_target(origin, destination, moved_piece_type, destination_occupant_type,
                                         position.whos_turn);

    position.move_log.push(reversible_move {
        .origin = origin,
        .destination = destination,
        .target = target,
        .captured_piece_type = position.occupier_type_lookup_table[target],
        .is_promotion = is_promotion,
    });

    // Clear the now vacated origin square.
    position.occupier_type_lookup_table[origin] = piece_type::none;
    position.color_bitboard[position.whos_turn] &= ~sbitboard(origin);
    position.type_specific_bitboard[moved_piece_type] &= ~sbitboard(origin);
    position.color_bitboard_rotated[position.whos_turn] &= ~sbitboard(origin_rotated);

    // Clear the target square since the piece which resides on it has been captured.
    const piece_type target_piece_type = position.occupier_type_lookup_table[target];
    position.type_specific_bitboard[target_piece_type] &= ~sbitboard(target);
    position.occupier_type_lookup_table[target] = piece_type::none;
    position.color_bitboard[opponent_color] &= ~sbitboard(target);
    position.color_bitboard_rotated[opponent_color] &= ~sbitboard(origin_rotated);

    // Fill the destination square with the moved piece.
    position.occupier_type_lookup_table[destination] = (is_promotion ? moved_piece_type : promote_to);
    position.color_bitboard[position.whos_turn] |= sbitboard(destination);
    position.type_specific_bitboard[moved_piece_type] |= sbitboard(destination);
    position.color_bitboard_rotated[position.whos_turn] |= sbitboard(rotate_sindex(destination));

    position.whos_turn = opponent_color;
}

void unmake_move(chess_position& position) {
    const reversible_move last_move = position.move_log.top();
    const bool is_capture = last_move.captured_piece_type != piece_type::none;
    const piece_color last_player_to_move = !position.whos_turn;
    const piece_type post_move_piece_type = position.occupier_type_lookup_table[last_move.destination];
    const piece_type pre_move_piece_type = last_move.is_promotion ? piece_type::pawn : post_move_piece_type;
    const uint8_t destination_rotated = rotate_sindex(last_move.destination);
    const uint8_t target_rotated = rotate_sindex(last_move.target);
    const uint8_t origin_rotated = rotate_sindex(last_move.origin);

    // Remove the piece from its destination square.
    position.occupier_type_lookup_table[last_move.destination] = piece_type::none;
    position.color_bitboard[last_player_to_move] &= ~sbitboard(last_move.destination);
    position.color_bitboard_rotated[last_player_to_move] &= ~sbitboard(destination_rotated);
    position.type_specific_bitboard[post_move_piece_type] &= ~sbitboard(last_move.destination);

    // If a piece was captured as a result of this move, un-capture it.
    position.occupier_type_lookup_table[last_move.target] = last_move.captured_piece_type;
    position.color_bitboard[position.whos_turn] |= (sbitboard(last_move.target) * is_capture);
    position.color_bitboard_rotated[position.whos_turn] |= (sbitboard(target_rotated) * is_capture);
    position.type_specific_bitboard[last_move.captured_piece_type] |= sbitboard(last_move.target);

    // Put the piece back on its origin square.
    position.occupier_type_lookup_table[last_move.origin] = pre_move_piece_type;
    position.color_bitboard[last_player_to_move] |= sbitboard(last_move.origin);
    position.color_bitboard_rotated[last_player_to_move] |= sbitboard(origin_rotated);
    position.type_specific_bitboard[pre_move_piece_type] |= sbitboard(last_move.origin);

    position.move_log.pop();
    position.whos_turn = last_player_to_move;
}

// Knights

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
                moves |= sbitboard(coords_to_sindex(knight_rank + 2, knight_file - 1));

            // There exists a towards black-kingside knight move.
            if (knight_file < 7 && knight_rank < 6)
                moves |= sbitboard(coords_to_sindex(knight_rank + 2, knight_file + 1));

            // There exists a towards queenside-black knight move.
            if (knight_file < 6 && knight_rank < 7)
                moves |= sbitboard(coords_to_sindex(knight_rank + 1, knight_file + 2));

            // There exists a towards kingside-black knight move.
            if (knight_file > 1 && knight_rank < 7)
                moves |= sbitboard(coords_to_sindex(knight_rank + 1, knight_file - 2));

            // There exists a towards white-queenside knight move.
            if (knight_file > 0 && knight_rank > 1)
                moves |= sbitboard(coords_to_sindex(knight_rank - 2, knight_file - 1));

            // There exists a towards white-kingside knight move.
            if (knight_file < 7 && knight_rank > 1)
                moves |= sbitboard(coords_to_sindex(knight_rank - 2, knight_file + 1));

            // There exists a towards queenside-white knight move.
            if (knight_file > 1 && knight_rank > 0)
                moves |= sbitboard(coords_to_sindex(knight_rank - 1, knight_file - 2));

            // There exists a towards kingside-white knight move.
            if (knight_file < 6 && knight_rank > 0)
                moves |= sbitboard(coords_to_sindex(knight_rank - 1, knight_file + 2));

            table[coords_to_sindex(knight_rank, knight_file)] = moves;
        }
    }
    return table;
}

constinit std::array<bitboard, 64> knight_move_table = generate_knight_move_table();

// Rooks

[[nodiscard]] consteval std::array<std::array<bitlane, 256>, 8> generate_rooklike_move_table() {
    std::array<std::array<bitlane, 256>, 8> table {};

    for (uint8_t origin = 0; origin < 8; ++origin) {
        for (bitlane occupancy = 255; occupancy > 0; --occupancy) {
            bitlane destinations = 0;
            // Towards Queenside
            for (uint8_t towards_queenside = 1; towards_queenside <= origin; ++towards_queenside) {
                const bitlane mark = sbitlane(origin - towards_queenside);
                const bool is_square_occupied = mark & occupancy;
                destinations |= mark;
                if (is_square_occupied) break;
            }

            // Towards Kingside
            for (uint8_t kingside_square = origin + 1; kingside_square < 8; ++kingside_square) {
                const bitlane mark = sbitlane(kingside_square);
                const bool is_square_occupied = mark & occupancy;
                destinations |= mark;
                if (is_square_occupied) break;
            }
            table[origin][occupancy] = destinations;
        }
    }

    return table;
}

constinit std::array<std::array<bitlane, 256>, 8> rooklike_move_table = generate_rooklike_move_table();





int main() {
    return 0;
}
