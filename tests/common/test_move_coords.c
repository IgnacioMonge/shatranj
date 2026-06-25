#include "common/chess/move_coords.h"

#include <stdio.h>
#include <stdlib.h>

static void check(int condition, const char *message)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        exit(1);
    }
}

static void expect_valid(const char *move,
                         uint8_t from_row,
                         uint8_t from_col,
                         uint8_t to_row,
                         uint8_t to_col)
{
    uint8_t fr = 0xffu;
    uint8_t fc = 0xffu;
    uint8_t tr = 0xffu;
    uint8_t tc = 0xffu;

    check(netchesszx_move_parse_coords(move, &fr, &fc, &tr, &tc),
          "valid move");
    check(fr == from_row && fc == from_col && tr == to_row && tc == to_col,
          "valid move coords");
}

static void expect_invalid(const char *move)
{
    uint8_t fr;
    uint8_t fc;
    uint8_t tr;
    uint8_t tc;

    check(!netchesszx_move_parse_coords(move, &fr, &fc, &tr, &tc),
          "invalid move");
}

int main(void)
{
    expect_valid("a8h1", 0u, 0u, 7u, 7u);
    expect_valid("e2e4", 6u, 4u, 4u, 4u);
    expect_valid("h1a8", 7u, 7u, 0u, 0u);

    expect_invalid("a1a1");
    expect_invalid("A1B2");
    expect_invalid("a9a8");
    expect_invalid("i1a1");
    expect_invalid("a0a1");

    puts("move coords tests ok");
    return 0;
}
