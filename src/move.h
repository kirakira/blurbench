#pragma once

#include <string>
#include <stdint.h>

typedef uint8_t POSITION;
typedef uint16_t MOVE;

const POSITION INVALID_POSITION = 255;

inline POSITION make_position(int rank, int col)
{
    return (POSITION) ((rank << 4) | col);
}

inline POSITION make_position(std::string s)
{
    if (s.length() < 2)
        return 0;
    else
    {
        int col = s[0] - 'a', rank = 9 - (s[1] - '0');
        return make_position(rank, col);
    }
}

inline int position_rank(POSITION p)
{
    return p >> 4;
}

inline int position_file(POSITION p)
{
    return p & 0xf;
}

inline std::string position_string(POSITION p)
{
    std::string ret;
    int rank = position_rank(p), col = position_file(p);
    ret += (char) (col + 'a');
    ret += (char) (9 - rank + '0');
    return ret;
}


inline MOVE make_move(POSITION src, POSITION dst)
{
    return (MOVE) ((src << 8) | dst);
}

inline MOVE make_move(std::string s)
{
    POSITION src, dst;
    if (s.length() >= 4)
    {
        src = make_position(s.substr(0, 2));
        dst = make_position(s.substr(2, 2));
    }
    else
    {
        src = 0;
        dst = 0;
    }
    return make_move(src, dst);
}

inline POSITION move_src(MOVE move)
{
    return move >> 8;
}

inline POSITION move_dst(MOVE move)
{
    return move & 0xff;
}

inline std::string move_string(MOVE move)
{
    return position_string(move_src(move)) + position_string(move_dst(move));
}

inline bool are_inverse_moves(MOVE move1, MOVE move2)
{
    return move_src(move1) == move_dst(move2) && move_dst(move1) == move_src(move2);
}
