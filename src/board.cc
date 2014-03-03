#include <iostream>
#include <cstring>
#include "board.h"
#include "rc4.h"

using namespace std;

Board::Board(string fen)
    : hash_side(rc4_uint64[H * W * 16])
    , history_positions(8)
{
    set(fen);
}

void Board::set(string fen)
{
    for (int i = 0; i < 32; ++i)
    {
        pieces[i].position = 0;
        pieces[i].piece = 0;
    }

    int start_position[16] =
        {0, 0, 1, 3, 5, 7, 9, 11,
         16, 16, 17, 19, 21, 23, 25, 27};

    hash = 0;
    current_static_value = 0;

    int i = 0, j = 0;
    for (size_t k = 0; k < fen.length(); ++k)
    {
        if (fen[k] == '/')
        {
            ++i;
            j = 0;
        }
        else if (fen[k] >= '0' && fen[k] <= '9')
        {
            int count = fen[k] - '0';
            while (count > 0)
            {
                board[i][j].index = 0;
                board[i][j].piece = 0;
                ++j;
                --count;
            }
        }
        else
        {
            PIECE piece = make_piece(fen[k]);
            int index = start_position[piece];
            ++start_position[piece];

            board[i][j].index = index;
            board[i][j].piece = piece;
            pieces[index].position = make_position(i, j);
            pieces[index].piece = piece;

            hash ^= get_hash(i, j, piece);
            current_static_value += static_values[piece][i][j];

            ++j;
        }
    }

    history.clear();
    history.reserve(128);
    history_positions.clear();
}

void Board::print()
{
    cout << "   a b c d e f g h i" << endl << endl;
    for (int i = 0; i < H; ++i)
    {
        cout << H - 1 - i << "  ";
        for (int j = 0; j < W; ++j)
        {
            if (board[i][j].piece == 0)
                cout << '.';
            else
                cout << piece_letter(board[i][j].piece);
            cout << " ";
        }
        cout << " " << H - 1 - i << endl;
    }
    cout << endl << "   a b c d e f g h i" << endl;
    cout << "Hash code: " << hash_code(0) << ", " << hash_code(1) <<  endl;
    cout << "Static value: " << static_value(1) << endl;
}

uint64_t Board::get_hash(int rank, int col, PIECE piece)
{
    return rc4_uint64[rank * W * 16 + col * 16 + piece];
}

uint64_t Board::hash_code(int side)
{
    if (side == 0)
        return hash;
    else
        return hash ^ hash_side;
}

bool Board::checked_move(int side, MOVE move, MoveType *mt)
{
    int src_i = position_rank(move_src(move)),
        src_j = position_file(move_src(move)),
        dst_i = position_rank(move_dst(move)),
        dst_j = position_file(move_dst(move));
    if (!(is_on_board(src_i, src_j) && is_on_board(dst_i, dst_j)))
        return false;
    if (!(is_on_board(src_i, src_j) && is_on_board(dst_i, dst_j)))
    if (board[src_i][src_j].piece == 0)
        return false;
    if (src_i == dst_i && src_j == dst_j)
        return false;

    PIECE src = board[src_i][src_j].piece;
    if (piece_side(src) != side)
        return false;
    int index = board[src_i][src_j].index;

    MOVE moves[120];
    int scores[120], count = 0;
    switch (piece_type(src))
    {
        case PIECE_K:
            generate_king_moves(index, moves, scores, &count);
            break;

        case PIECE_A:
            generate_advisor_moves(index, moves, scores, &count);
            break;

        case PIECE_E:
            generate_elephant_moves(index, moves, scores, &count);
            break;

        case PIECE_H:
            generate_horse_moves(index, moves, scores, &count);
            break;

        case PIECE_R:
            generate_rook_moves(index, moves, scores, &count);
            break;

        case PIECE_C:
            generate_cannon_moves(index, moves, scores, &count);
            break;

        case PIECE_P:
            generate_pawn_moves(index, moves, scores, &count);
            break;

        default:
            return false;
    }

    bool hit = false;
    for (int i = 0; !hit && i < count; ++i)
        if (moves[i] == move)
            hit = true;
    if (!hit)
        return false;

    if (!this->move(move, mt, true))
        return false;

    if (in_check(side))
    {
        unmove();
        return false;
    }

    return true;
}

bool Board::move(MOVE move, MoveType *mt, bool detect_repetition)
{
    int src_i = position_rank(move_src(move)),
        src_j = position_file(move_src(move)),
        dst_i = position_rank(move_dst(move)),
        dst_j = position_file(move_dst(move));
    BoardEntry src = board[src_i][src_j],
               dst = board[dst_i][dst_j];

    if (mt)
        *mt = REGULAR;

    if (dst.piece != 0)
    {
        if (mt)
        {
            if (piece_type(dst.piece) == PIECE_K)
                *mt = KING_CAPTURE;
            else
                *mt = CAPTURE;
        }

        pieces[dst.index].piece = 0;
        hash ^= get_hash(dst_i, dst_j, dst.piece);
        current_static_value -= static_values[dst.piece][dst_i][dst_j];
    }

    board[dst_i][dst_j] = src;
    pieces[src.index].position = make_position(dst_i, dst_j);
    hash ^= get_hash(dst_i, dst_j, src.piece);
    current_static_value += static_values[src.piece][dst_i][dst_j];

    board[src_i][src_j].piece = 0;
    hash ^= get_hash(src_i, src_j, src.piece);
    current_static_value -= static_values[src.piece][src_i][src_j];

    HistoryEntry history_entry;
    history_entry.move = move;
    history_entry.capture = dst;
    history_entry.perp_side = NON_PERPETUAL;
    history.push_back(history_entry);

    int my_side = piece_side(src.piece);
    int rep_count = history_positions.increment(hash_code(my_side));
    if (king_face_to_face())
    {
        unmove();
        return false;
    }

    if (rep_count > 1 && detect_repetition)
    {
        uint8_t perp_side = NON_PERPETUAL;
        if (dst.piece == 0)
        {
            if (history.size() >= 2 && history[history.size() - 2].perp_side != NON_PERPETUAL)
            {
                uint8_t his_perp = history[history.size() - 2].perp_side;
                if (his_perp == my_side)
                {
                    if (in_check(1 - my_side))
                        perp_side = my_side;
                    else
                        perp_side = NON_PERPETUAL;
                }
                else
                    perp_side = his_perp;
            }
            else
                perp_side = test_for_perpetual(my_side) ? my_side : NON_PERPETUAL;
        }

        if (mt)
        {
            if (perp_side == my_side)
                *mt = PERPETUAL_CHECK_OR_CHASE;
            else
                *mt = REPETITION;
        }
    }

    return true;
}

void Board::unmove()
{
    HistoryEntry history_entry = history.back();
    history.pop_back();


    int src_i = position_rank(move_src(history_entry.move)),
        src_j = position_file(move_src(history_entry.move)),
        dst_i = position_rank(move_dst(history_entry.move)),
        dst_j = position_file(move_dst(history_entry.move));

    BoardEntry src = board[dst_i][dst_j], dst = history_entry.capture;

    int my_side = piece_side(src.piece);
    history_positions.decrement(hash_code(my_side));

    board[src_i][src_j] = src;
    pieces[src.index].position = make_position(src_i, src_j);
    hash ^= get_hash(src_i, src_j, src.piece);
    current_static_value += static_values[src.piece][src_i][src_j];

    hash ^= get_hash(dst_i, dst_j, src.piece);
    current_static_value -= static_values[src.piece][dst_i][dst_j];
    board[dst_i][dst_j] = dst;
    if (dst.piece != 0)
    {
        pieces[dst.index].piece = dst.piece;
        pieces[dst.index].position = make_position(dst_i, dst_j);
        hash ^= get_hash(dst_i, dst_j, dst.piece);
        current_static_value += static_values[dst.piece][dst_i][dst_j];
    }
}

bool Board::checked_unmove()
{
    if (history.size() == 0)
        return false;
    unmove();
    return true;
}

bool Board::test_for_perpetual(int my_side)
{
    vector<HistoryEntry> saved_moves;
    int side = my_side;
    uint64_t rep_hash = hash_code(my_side);
    bool success = false, other_perp = true;
    while (!success && history.size() > 0)
    {
        HistoryEntry he = history.back();
        POSITION pos = move_dst(he.move);

        if (piece_side(board[position_rank(pos)][position_file(pos)].piece) != side)
            break;
        if (he.capture.piece != 0)
            break;

        if (!in_check(1 - side))
        {
            if (side == my_side)
                break;
            else
                other_perp = false;
        }

        saved_moves.push_back(he);
        unmove();
        side = 1 - side;

        if (hash_code(side) == rep_hash)
            success = true;
    }

    while (saved_moves.size() > 0)
    {
        HistoryEntry he = saved_moves.back();
        saved_moves.pop_back();
        move(he.move, NULL, false);
        history[history.size() - 1].perp_side = he.perp_side;
    }

    return success && !other_perp;
}

bool Board::is_capture(MOVE move, int *value)
{
    int src_i = position_rank(move_src(move)),
        src_j = position_file(move_src(move)),
        dst_i = position_rank(move_dst(move)),
        dst_j = position_file(move_dst(move));
    BoardEntry src = board[src_i][src_j],
               dst = board[dst_i][dst_j];
    if (dst.piece != 0)
    {
        if (value)
            *value = capture_values[piece_type(dst.piece)] -
                capture_values[piece_type(src.piece)];
        return true;
    }
    return false;
}

POSITION Board::king_position(int side)
{
    int index = 0;
    if (side == 1)
        index += 16;
    return pieces[index].position;
}

const int Board::capture_values[8] = {0, 5, 2, 2, 3, 4, 3, 1};
const int Board::static_values[16][H][W] =
{{{0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}},
{{0, 0, 0, 11, 15, 11, 0, 0, 0},
    {0, 0, 0, 2, 2, 2, 0, 0, 0},
    {0, 0, 0, 1, 1, 1, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}},
{{0, 0, 0, 20, 0, 20, 0, 0, 0},
    {0, 0, 0, 0, 23, 0, 0, 0, 0},
    {0, 0, 0, 20, 0, 20, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}},
{{0, 0, 20, 0, 0, 0, 20, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {18, 0, 0, 0, 23, 0, 0, 0, 18},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 20, 0, 0, 0, 20, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}},
{{88, 85, 90, 88, 90, 88, 90, 85, 88},
    {85, 90, 92, 93, 78, 93, 92, 90, 85},
    {93, 92, 94, 95, 92, 95, 94, 92, 93},
    {92, 94, 98, 95, 98, 95, 98, 94, 92},
    {90, 98, 101, 102, 103, 102, 101, 98, 90},
    {90, 100, 99, 103, 104, 103, 99, 100, 90},
    {93, 108, 100, 107, 100, 107, 100, 108, 93},
    {92, 98, 99, 103, 99, 103, 99, 98, 92},
    {90, 96, 103, 97, 94, 97, 103, 96, 90},
    {90, 90, 90, 96, 90, 96, 90, 90, 90}},
{{194, 206, 204, 212, 200, 212, 204, 206, 194},
    {200, 208, 206, 212, 200, 212, 206, 208, 200},
    {198, 208, 204, 212, 212, 212, 204, 208, 198},
    {204, 209, 204, 212, 214, 212, 204, 209, 204},
    {208, 212, 212, 214, 215, 214, 212, 212, 208},
    {208, 211, 211, 214, 215, 214, 211, 211, 208},
    {206, 213, 213, 216, 216, 216, 213, 213, 206},
    {206, 208, 207, 214, 216, 214, 207, 208, 206},
    {206, 212, 209, 216, 233, 216, 209, 212, 206},
    {206, 208, 207, 213, 214, 213, 207, 208, 206}},
{{96, 96, 97, 99, 99, 99, 97, 96, 96},
    {96, 97, 98, 98, 98, 98, 98, 97, 96},
    {97, 96, 100, 99, 101, 99, 100, 96, 97},
    {96, 96, 96, 96, 96, 96, 96, 96, 96},
    {95, 96, 99, 96, 100, 96, 99, 96, 95},
    {96, 96, 96, 96, 100, 96, 96, 96, 96},
    {96, 99, 99, 98, 100, 98, 99, 99, 96},
    {97, 97, 96, 91, 92, 91, 96, 97, 97},
    {98, 98, 96, 92, 89, 92, 96, 98, 98},
    {100, 100, 96, 91, 90, 91, 96, 100, 100}},
{{0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {7, 0, 7, 0, 15, 0, 7, 0, 7},
    {7, 0, 13, 0, 16, 0, 13, 0, 7},
    {14, 18, 20, 27, 29, 27, 20, 18, 14},
    {19, 23, 27, 29, 30, 29, 27, 23, 19},
    {19, 24, 32, 37, 37, 37, 32, 24, 19},
    {19, 24, 34, 42, 44, 42, 34, 24, 19},
    {9, 9, 9, 11, 13, 11, 9, 9, 9}},
{{0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}},
{{0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, -1, -1, -1, 0, 0, 0},
    {0, 0, 0, -2, -2, -2, 0, 0, 0},
    {0, 0, 0, -11, -15, -11, 0, 0, 0}},
{{0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, -20, 0, -20, 0, 0, 0},
    {0, 0, 0, 0, -23, 0, 0, 0, 0},
    {0, 0, 0, -20, 0, -20, 0, 0, 0}},
{{0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, -20, 0, 0, 0, -20, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {-18, 0, 0, 0, -23, 0, 0, 0, -18},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, -20, 0, 0, 0, -20, 0, 0}},
{{-90, -90, -90, -96, -90, -96, -90, -90, -90},
    {-90, -96, -103, -97, -94, -97, -103, -96, -90},
    {-92, -98, -99, -103, -99, -103, -99, -98, -92},
    {-93, -108, -100, -107, -100, -107, -100, -108, -93},
    {-90, -100, -99, -103, -104, -103, -99, -100, -90},
    {-90, -98, -101, -102, -103, -102, -101, -98, -90},
    {-92, -94, -98, -95, -98, -95, -98, -94, -92},
    {-93, -92, -94, -95, -92, -95, -94, -92, -93},
    {-85, -90, -92, -93, -78, -93, -92, -90, -85},
    {-88, -85, -90, -88, -90, -88, -90, -85, -88}},
{{-206, -208, -207, -213, -214, -213, -207, -208, -206},
    {-206, -212, -209, -216, -233, -216, -209, -212, -206},
    {-206, -208, -207, -214, -216, -214, -207, -208, -206},
    {-206, -213, -213, -216, -216, -216, -213, -213, -206},
    {-208, -211, -211, -214, -215, -214, -211, -211, -208},
    {-208, -212, -212, -214, -215, -214, -212, -212, -208},
    {-204, -209, -204, -212, -214, -212, -204, -209, -204},
    {-198, -208, -204, -212, -212, -212, -204, -208, -198},
    {-200, -208, -206, -212, -200, -212, -206, -208, -200},
    {-194, -206, -204, -212, -200, -212, -204, -206, -194}},
{{-100, -100, -96, -91, -90, -91, -96, -100, -100},
    {-98, -98, -96, -92, -89, -92, -96, -98, -98},
    {-97, -97, -96, -91, -92, -91, -96, -97, -97},
    {-96, -99, -99, -98, -100, -98, -99, -99, -96},
    {-96, -96, -96, -96, -100, -96, -96, -96, -96},
    {-95, -96, -99, -96, -100, -96, -99, -96, -95},
    {-96, -96, -96, -96, -96, -96, -96, -96, -96},
    {-97, -96, -100, -99, -101, -99, -100, -96, -97},
    {-96, -97, -98, -98, -98, -98, -98, -97, -96},
    {-96, -96, -97, -99, -99, -99, -97, -96, -96}},
{{-9, -9, -9, -11, -13, -11, -9, -9, -9},
    {-19, -24, -34, -42, -44, -42, -34, -24, -19},
    {-19, -24, -32, -37, -37, -37, -32, -24, -19},
    {-19, -23, -27, -29, -30, -29, -27, -23, -19},
    {-14, -18, -20, -27, -29, -27, -20, -18, -14},
    {-7, 0, -13, 0, -16, 0, -13, 0, -7},
    {-7, 0, -7, 0, -15, 0, -7, 0, -7},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0}}};

int Board::static_value(int side)
{
    if (side == 0)
        return current_static_value;
    else
        return -current_static_value;
}

bool Board::is_valid_position(PIECE piece, int i, int j)
{
    return (static_values[piece][i][j] != 0);
}

bool Board::is_on_board(int i, int j)
{
    return (i >= 0 && i < H && j >= 0 && j < W);
}

bool Board::is_in_palace(int side, int i, int j)
{
    if (side == 0)
        return (i >= 0 && i <= 2 && j >= 3 && j <= 5);
    else
        return (i >= 7 && i <= 9 && j >= 3 && j <= 5);
}

bool Board::is_in_half(int side, int i, int j)
{
    if (!(i >= 0 && i < H && j >= 0 && j < W))
        return false;
    if ((side == 0 && i <= 4) || (side == 1 && j >= 5))
        return true;
    else
        return false;
}

bool Board::check_position(int side, int i, int j, int *target_capture_score)
{
    if (board[i][j].piece == 0)
    {
        *target_capture_score = NON_CAPTURE;
        return true;
    }
    else if (piece_side(board[i][j].piece) != side)
    {
        *target_capture_score = capture_values[piece_type(board[i][j].piece)];
        return true;
    }
    else
        return false;
}

int Board::c4di[4] = {0, 1, 0, -1}, Board::c4dj[4] = {1, 0, -1, 0};
int Board::king_moves[256][4][2], Board::king_moves_count[256] = {0};
int Board::horse_d[8][6] = {
    {-2, 1, -1, 0, -1, 1},
    {-1, 2, 0, 1, -1, 1},
    {1, 2, 0, 1, 1, 1},
    {2, 1, 1, 0, 1, 1},
    {2, -1, 1, 0, 1, -1},
    {1, -2, 0, -1, 1, -1},
    {-1, -2, 0, -1, -1, -1},
    {-2, -1, -1, 0, -1, -1}
};
int Board::horse_moves[256][8][6], Board::horse_moves_count[256];
int Board::s4di[4] = {1, 1, -1, -1}, Board::s4dj[4] = {-1, 1, 1, -1};
int Board::elephant_positions[7][2] = {{0, 2}, {0, 6}, {2, 0}, {2, 4}, {2, 8}, {4, 2}, {4, 6}};
int Board::elephant_moves[256][4][4], Board::elephant_moves_count[256];
int Board::advisor_moves[256][4][2], Board::advisor_moves_count[256];
int Board::pawn_moves[2][256][3][2], Board::pawn_moves_count[2][256];

Board::BoardStaticFieldsInitializer Board::board_initializer;

Board::BoardStaticFieldsInitializer::BoardStaticFieldsInitializer()
{
    // King
    for (int side = 0; side <= 1; ++side)
    {
        for (int ii = 0; ii <= 2; ++ii)
        {
            int i = ii;
            if (side == 1)
                i = H - 1 - ii;
            for (int j = 3; j <= 5; ++j)
            {
                POSITION p = make_position(i, j);
                king_moves_count[p] = 0;
                for (int r = 0; r < 4; ++r)
                {
                    int oi = i + c4di[r], oj = j + c4dj[r];
                    if (is_in_palace(side, oi, oj))
                    {
                        int index = king_moves_count[p];
                        ++king_moves_count[p];

                        king_moves[p][index][0] = oi;
                        king_moves[p][index][1] = oj;
                    }
                }
            }
        }
    }

    // Advisor
    for (int side = 0; side <= 1; ++side)
    {
        for (int ii = 0; ii <= 2; ++ii)
        {
            int i = ii;
            if (side == 1)
                i = H - 1 - ii;
            for (int j = 3; j <= 5; ++j)
            {
                POSITION p = make_position(i, j);
                advisor_moves_count[p] = 0;
                for (int r = 0; r < 4; ++r)
                {
                    int oi = i + s4di[r], oj = j + s4dj[r];
                    if (is_in_palace(side, oi, oj))
                    {
                        int index = advisor_moves_count[p];
                        ++advisor_moves_count[p];

                        advisor_moves[p][index][0] = oi;
                        advisor_moves[p][index][1] = oj;
                    }
                }
            }
        }
    }

    // Horse
    for (int i = 0; i < H; ++i)
        for (int j = 0; j < W; ++j)
        {
            POSITION p = make_position(i, j);
            horse_moves_count[p] = 0;
            for (int r = 0; r < 8; ++r)
            {
                int oi = i + horse_d[r][0], oj = j + horse_d[r][1];
                if (is_on_board(oi, oj))
                {
                    int index = horse_moves_count[p];
                    ++horse_moves_count[p];

                    horse_moves[p][index][0] = oi;
                    horse_moves[p][index][1] = oj;
                    horse_moves[p][index][2] = i + horse_d[r][2];
                    horse_moves[p][index][3] = j + horse_d[r][3];
                    horse_moves[p][index][4] = i + horse_d[r][4];
                    horse_moves[p][index][5] = j + horse_d[r][5];
                }
            }
        }

    // Elephant
    for (int k = 0; k < 7; ++k)
    {
        int i = elephant_positions[k][0], j = elephant_positions[k][1];
        POSITION p = make_position(i, j);
        elephant_moves_count[p] = 0;
        for (int r = 0; r < 4; ++r)
        {
            int oi = i + s4di[r] * 2, oj = j + s4dj[r] * 2;
            if (is_in_half(0, oi, oj))
            {
                int index = elephant_moves_count[p];
                ++elephant_moves_count[p];

                elephant_moves[p][index][0] = oi;
                elephant_moves[p][index][1] = oj;
                elephant_moves[p][index][2] = i + s4di[r];
                elephant_moves[p][index][3] = j + s4dj[r];
            }
        }

        int pi = H - 1 - i, pj = j;
        POSITION pp = make_position(pi, pj);
        elephant_moves_count[pp] = elephant_moves_count[p];
        for (int kk = 0; kk < elephant_moves_count[p]; ++kk)
        {
            elephant_moves[pp][kk][0] = H - 1 - elephant_moves[p][kk][0];
            elephant_moves[pp][kk][1] = elephant_moves[p][kk][1];
            elephant_moves[pp][kk][2] = H - 1 - elephant_moves[p][kk][2];
            elephant_moves[pp][kk][3] = elephant_moves[p][kk][3];
        }
    }

    // Pawn
    for (int i = 3; i <= 4; ++i)
        for (int j = 0; j < W; j += 2)
        {
            POSITION p = make_position(i, j);
            pawn_moves_count[0][p] = 1;
            pawn_moves[0][p][0][0] = i + 1;
            pawn_moves[0][p][0][1] = j;
        }
    for (int i = 5; i < H; ++i)
        for (int j = 0; j < W; ++j)
        {
            POSITION p = make_position(i, j);
            pawn_moves_count[0][p] = 0;
            for (int r = 0; r < 4; ++r)
            {
                int oi = i + c4di[r], oj = j + c4dj[r];
                if (c4di[r] >= 0 && is_on_board(oi, oj))
                {
                    int index = pawn_moves_count[0][p];
                    ++pawn_moves_count[0][p];

                    pawn_moves[0][p][index][0] = oi;
                    pawn_moves[0][p][index][1] = oj;
                }
            }
        }
    for (int i = 0; i < H; ++i)
        for (int j = 0; j < W; ++j)
        {
            int pi = H - 1 - i, pj = j;
            if (pawn_moves_count[0][make_position(pi, pj)] > 0)
            {
                pawn_moves_count[1][make_position(i, j)] = pawn_moves_count[0][make_position(pi, pj)];
                for (int r = 0; r < pawn_moves_count[1][make_position(i, j)]; ++r)
                {
                    pawn_moves[1][make_position(i, j)][r][0] = H - 1 - pawn_moves[0][make_position(pi, pj)][r][0];
                    pawn_moves[1][make_position(i, j)][r][1] = pawn_moves[0][make_position(pi, pj)][r][1];
                }
            }
        }
}

void Board::add_move(MOVE *moves, int *capture_scores, int *moves_count, MOVE move_to_add, int capture_score)
{
    moves[*moves_count] = move_to_add;
    capture_scores[*moves_count] = capture_score;
    ++*moves_count;
}

void Board::generate_moves(int side, MOVE *moves, int *capture_scores, int *moves_count)
{
    int index;
    *moves_count = 0;

    // Rook
    index = 7;
    if (side != 0)
        index += 16;
    for (int i = 0; i < 2; ++i)
        if (pieces[index + i].piece != 0)
            generate_rook_moves(index + i, moves, capture_scores, moves_count);

    // Horse
    index = 5;
    if (side != 0)
        index += 16;
    for (int i = 0; i < 2; ++i)
        if (pieces[index + i].piece != 0)
            generate_horse_moves(index + i, moves, capture_scores, moves_count);

    // Cannon
    index = 9;
    if (side != 0)
        index += 16;
    for (int i = 0; i < 2; ++i)
        if (pieces[index + i].piece != 0)
            generate_cannon_moves(index + i, moves, capture_scores, moves_count);

    // Pawn
    index = 11;
    if (side != 0)
        index += 16;
    for (int i = 0; i < 5; ++i)
        if (pieces[index + i].piece != 0)
            generate_pawn_moves(index + i, moves, capture_scores, moves_count);

    // Advisor
    index = 1;
    if (side != 0)
        index += 16;
    for (int i = 0; i < 2; ++i)
        if (pieces[index + i].piece != 0)
            generate_advisor_moves(index + i, moves, capture_scores, moves_count);

    // Elephant
    index = 3;
    if (side != 0)
        index += 16;
    for (int i = 0; i < 2; ++i)
        if (pieces[index + i].piece != 0)
            generate_elephant_moves(index + i, moves, capture_scores, moves_count);

    // King
    index = 0;
    if (side != 0)
        index += 16;
    generate_king_moves(index, moves, capture_scores, moves_count);
}

void Board::generate_king_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);
    for (int i = 0; i < king_moves_count[pos]; ++i)
    {
        int oi = king_moves[pos][i][0], oj = king_moves[pos][i][1];
        int capture_value;
        if (check_position(side, oi, oj, &capture_value))
            add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                    capture_value * 8 - capture_values[PIECE_K]);
    }
}

void Board::generate_advisor_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);
    for (int i = 0; i < advisor_moves_count[pos]; ++i)
    {
        int oi = advisor_moves[pos][i][0], oj = advisor_moves[pos][i][1];
        int capture_value;
        if (check_position(side, oi, oj, &capture_value))
            add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                    capture_value * 8 - capture_values[PIECE_A]);
    }
}

void Board::generate_rook_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);

    int i = position_rank(pos), j = position_file(pos);
    for (int r = 0; r < 4; ++r)
    {
        int oi = i, oj = j;
        while (true)
        {
            oi += c4di[r];
            oj += c4dj[r];

            if (!is_on_board(oi, oj))
                break;

            if (board[oi][oj].piece == 0)
                add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                        NON_CAPTURE);
            else
            {
                int capture_value;
                if (check_position(side, oi, oj, &capture_value))
                    add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                            capture_value * 8 - capture_values[PIECE_R]);
                break;
            }
        }
    }
}

void Board::generate_horse_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);

    for (int i = 0; i < horse_moves_count[pos]; ++i)
    {
        int oi = horse_moves[pos][i][0], oj = horse_moves[pos][i][1];
        int capture_value;
        if (check_position(side, oi, oj, &capture_value) &&
                board[horse_moves[pos][i][2]][horse_moves[pos][i][3]].piece == 0)
            add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                    capture_value * 8 - capture_values[PIECE_H]);
    }
}

void Board::generate_cannon_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);

    int i = position_rank(pos), j = position_file(pos);
    for (int r = 0; r < 4; ++r)
    {
        int oi = i, oj = j;
        int state = 0;
        while (true)
        {
            oi += c4di[r];
            oj += c4dj[r];

            if (!is_on_board(oi, oj))
                break;

            if (state == 0)
            {
                if (board[oi][oj].piece == 0)
                    add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)), NON_CAPTURE);
                else
                    state = 1;
            }
            else if (state == 1)
            {
                if (board[oi][oj].piece != 0)
                {
                    int capture_value;
                    if (check_position(side, oi, oj, &capture_value))
                        add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                                capture_value * 8 - capture_values[PIECE_C]);
                    break;
                }
            }
        }
    }
}

void Board::generate_elephant_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);

    for (int i = 0; i < elephant_moves_count[pos]; ++i)
    {
        int oi = elephant_moves[pos][i][0], oj = elephant_moves[pos][i][1];
        int capture_value;
        if (check_position(side, oi, oj, &capture_value) &&
                board[elephant_moves[pos][i][2]][elephant_moves[pos][i][3]].piece == 0)
            add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                    capture_value * 8 - capture_values[PIECE_E]);
    }
}

void Board::generate_pawn_moves(int index, MOVE *moves, int *capture_scores, int *moves_count)
{
    POSITION pos = pieces[index].position;
    int side = piece_side(pieces[index].piece);

    for (int i = 0; i < pawn_moves_count[side][pos]; ++i)
    {
        int oi = pawn_moves[side][pos][i][0], oj = pawn_moves[side][pos][i][1];
        int capture_value;
        if (check_position(side, oi, oj, &capture_value))
            add_move(moves, capture_scores, moves_count, make_move(pos, make_position(oi, oj)),
                    capture_value * 8 - capture_values[PIECE_P]);
    }
}

bool Board::is_attacked(POSITION pos, bool test_all_attacks, MOVE *best_attack)
{
    int src_i = position_rank(pos), src_j = position_file(pos);
    int side_to_attack = 1 - piece_side(board[src_i][src_j].piece);

    MOVE best_response = 0;
    int least_response_value = -1;

    // Detect attackings by rook, cannon, pawn, king
    for (int r = 0; r < 4; ++r)
    {
        int oi = src_i, oj = src_j;
        int len = 0;
        bool ob = false;
        while (true)
        {
            oi += c4di[r], oj += c4dj[r];
            if (!is_on_board(oi, oj))
                break;

            ++len;

            PIECE p = board[oi][oj].piece;
            if (p == 0)
                continue;

            if (len == 1)
            {
                if (p == make_piece(side_to_attack, PIECE_P))
                {
                    POSITION dst = make_position(oi, oj);
                    for (int j = 0; j < pawn_moves_count[side_to_attack][dst]; ++j)
                        if (pawn_moves[side_to_attack][dst][j][0] == src_i
                                && pawn_moves[side_to_attack][dst][j][1] == src_j)
                        {
                            if (best_attack)
                                *best_attack = make_move(dst, pos);
                            return true;
                        }
                }
                else if (test_all_attacks && p == make_piece(side_to_attack, PIECE_K))
                {
                    POSITION dst = make_position(oi, oj);
                    for (int j = 0; j < king_moves_count[dst]; ++j)
                        if (king_moves[dst][j][0] == src_i
                                && king_moves[dst][j][1] == src_j)
                        {
                            if (best_attack)
                            {
                                if (least_response_value == -1
                                        || capture_values[PIECE_K] < least_response_value)
                                {
                                    least_response_value = capture_values[PIECE_K];
                                    best_response = make_move(dst, pos);
                                }
                            }
                            else
                                return true;
                        }
                }
            }

            if (!ob)
            {
                if (p == make_piece(side_to_attack, PIECE_R))
                {
                    if (best_attack)
                    {
                        if (least_response_value == -1 ||
                                capture_values[PIECE_R] < least_response_value)
                        {
                            least_response_value = capture_values[PIECE_R];
                            best_response = make_move(make_position(oi, oj), pos);
                        }
                    }
                    else
                        return true;
                }
                ob = true;
            }
            else
            {
                if (p == make_piece(side_to_attack, PIECE_C))
                {
                    if (best_attack)
                    {
                        if (least_response_value == -1 ||
                                capture_values[PIECE_C] < least_response_value)
                        {
                            least_response_value = capture_values[PIECE_C];
                            best_response = make_move(make_position(oi, oj), pos);
                        }
                    }
                    else
                        return true;
                }
                break;
            }
        }
    }

    // Detect attackings by horse
    for (int i = 0; i < horse_moves_count[pos]; ++i)
    {
        int oi = horse_moves[pos][i][0], oj = horse_moves[pos][i][1];
        if (board[oi][oj].piece == make_piece(side_to_attack, PIECE_H)
                && board[horse_moves[pos][i][4]][horse_moves[pos][i][5]].piece == 0)
        {
            if (best_attack)
            {
                if (least_response_value == -1 ||
                        capture_values[PIECE_H] < least_response_value)
                {
                    least_response_value = capture_values[PIECE_H];
                    best_response = make_move(make_position(oi, oj), pos);
                }
            }
            else
                return true;
        }
    }

    if (test_all_attacks)
    {
        // Detect attackings by elephant
        for (int i = 0; i < elephant_moves_count[pos]; ++i)
        {
            int oi = elephant_moves[pos][i][0], oj = elephant_moves[pos][i][1];
            if (board[oi][oj].piece == make_piece(side_to_attack, PIECE_E)
                    && board[elephant_moves[pos][i][2]][elephant_moves[pos][i][3]].piece == 0)
            {
                if (best_attack)
                    *best_attack = make_move(make_position(oi, oj), pos);
                return true;
            }
        }

        // Detect attackings by advisor
        for (int i = 0; i < advisor_moves_count[pos]; ++i)
        {
            int oi = advisor_moves[pos][i][0], oj = advisor_moves[pos][i][1];
            if (board[oi][oj].piece == make_piece(side_to_attack, PIECE_A))
            {
                if (best_attack)
                    *best_attack = make_move(make_position(oi, oj), pos);
                return true;
            }
        }
    }

    if (least_response_value == -1)
        return false;

    if (best_attack)
        *best_attack = best_response;
    return true;
    return false;
}

bool Board::in_check(int side)
{
    int index = 0;
    if (side != 0)
        index += 16;
    POSITION pos = pieces[index].position;
    return is_attacked(pos, false);
}

bool Board::king_face_to_face()
{
    if (pieces[0].piece == 0 || pieces[16].piece == 0)
        return false;
    POSITION pos1 = pieces[0].position, pos2 = pieces[16].position;
    if (position_file(pos1) != position_file(pos2))
        return false;
    for (int i = position_rank(pos1) + 1; i <= position_rank(pos2) - 1; ++i)
        if (board[i][position_file(pos1)].piece != 0)
            return false;
    return true;
}

string Board::fen_string(int side)
{
    string s;
    for (int i = 0; i < H; ++i)
    {
        int c = 0;
        for (int j = 0; j < W; ++j)
        {
            if (board[i][j].piece == 0)
                ++c;
            else
            {
                if (c != 0)
                    s += ('0' + c);
                s += piece_letter(board[i][j].piece);
                c = 0;
            }
        }
        if (c != 0)
            s += ('0' + c);
        if (i != H - 1)
            s += "/";
    }
    if (side == 0)
        s += " b";
    else
        s += " w";
    return s;
}
