#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define NETCHESSZX_HOST_TEST 1
#define __z88dk_fastcall

char *spectrum_append_text(char *dst, const char *src)
{
    return strcpy(dst, src) + strlen(src);
}

#include "../../src/spectrum/overlay/fileui_ovl.c"

int main(void)
{
    char names[FILEUI_CANDIDATE_BYTES] = {0};
    static const unsigned char stamp[FILEUI_STAMP_SIZE] = {0u};
    char *slot1 = fileui_candidate(names, 1u);
    char *slot2 = fileui_candidate(names, 2u);

    fileui_stamp = stamp;

    if (!fileui_keep_newest(slot1, "0168JF26") ||
        fileui_keep_newest(slot1, "0167JF26") ||
        strcmp(slot1, "0168JF26") != 0 ||
        !fileui_keep_newest(slot1, "0169JF26") ||
        strcmp(slot1, "0169JF26") != 0 ||
        !fileui_keep_newest(slot2, "02000000") ||
        strcmp(slot2, "02000000") != 0) {
        fputs("FAIL: FILEUI duplicate policy\n", stderr);
        return 1;
    }
    puts("FILEUI duplicate policy ok");
    return 0;
}
