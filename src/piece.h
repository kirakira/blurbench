#pragma once

#include <stdint.h>

typedef uint8_t PIECE;

const int PIECE_K = 1;
const int PIECE_A = 2;
const int PIECE_E = 3;
const int PIECE_H = 4;
const int PIECE_R = 5;
const int PIECE_C = 6;
const int PIECE_P = 7;

inline PIECE make_piece(int side, int type)
{
    return (side << 3) | type;
}

inline PIECE make_piece(char letter)
{
    int side = 0;
    if (letter >= 'A' && letter <= 'Z')
    {
        side = 1;
        letter += ('a' - 'A');
    }

    int type;
    if (letter == 'k')
        type = PIECE_K;
    else if (letter == 'a')
        type = PIECE_A;
    else if (letter == 'e')
        type = PIECE_E;
    else if (letter == 'h' || letter == 'n')
        type = PIECE_H;
    else if (letter == 'r')
        type = PIECE_R;
    else if (letter == 'c')
        type = PIECE_C;
    else if (letter == 'p')
        type = PIECE_P;
    else
        return 0;

    return make_piece(side, type);
}

inline int piece_side(PIECE piece)
{
    return piece >> 3;
}

inline int piece_type(PIECE piece)
{
    return piece & 7;
}

inline char piece_letter(PIECE piece)
{
    const char array[17] = ".kaehrcp?KAEHRCP";
    return array[piece];
}
