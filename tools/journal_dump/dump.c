/* Read-only dump of /SYS/CONFIG/SHATJRNL.TMP. Does not write or unlink. */

#include <stdint.h>

#define PATH "/SYS/CONFIG/SHATJRNL.TMP"
#define DUMP_MAX 512u

extern uint8_t esx_handle;
extern uint16_t esx_buf;
extern uint16_t esx_count;
extern uint16_t esx_result;

void esx_fopen(const char *path) __z88dk_fastcall;
void esx_fread(void);
uint8_t esx_fclose(void);

static void putch(uint8_t c) __z88dk_fastcall
{
    c;
#asm
    ld a, l
    rst 16
#endasm
}

static void puts_zx(const char *s)
{
    while (*s != '\0') {
        putch((uint8_t)*s++);
    }
    putch(13u);
}

static void putu16_raw(uint16_t n)
{
    char buf[5];
    uint8_t i = 5u;

    do {
        buf[--i] = (char)('0' + (n % 10u));
        n = (uint16_t)(n / 10u);
    } while (n != 0u);
    while (i < 5u) {
        putch((uint8_t)buf[i++]);
    }
}

static void puthex4(uint8_t n) __z88dk_fastcall
{
    n = (uint8_t)(n & 15u);
    putch((uint8_t)(n < 10u ? ('0' + n) : ('A' + (n - 10u))));
}

static void puthex8(uint8_t n) __z88dk_fastcall
{
    puthex4((uint8_t)(n >> 4));
    puthex4(n);
}

static void putsq(uint8_t idx) __z88dk_fastcall
{
    putch((uint8_t)('a' + (idx & 7u)));
    putch((uint8_t)('8' - (idx >> 3)));
}

static void enable_im1(void)
{
#asm
    ld iy, 0x5c3a
    im 1
    ei
#endasm
}

int main(void)
{
    uint8_t buf[DUMP_MAX];
    uint16_t got;
    uint16_t i;
    uint16_t rec;
    uint16_t n;

    enable_im1();
    puts_zx("SHATJRNL.TMP");
    esx_fopen(PATH);
    if (esx_handle == 0u) {
        puts_zx("open fail");
        return 0;
    }
    esx_buf = (uint16_t)buf;
    esx_count = DUMP_MAX;
    esx_fread();
    got = esx_result;
    (void)esx_fclose();

    putch('b');
    putch(' ');
    putu16_raw(got);
    putch(13u);
    if ((got & 1u) != 0u) {
        puts_zx("odd size");
    }
    n = (uint16_t)(got >> 1);
    for (i = 0u; i < n; ++i) {
        rec = (uint16_t)buf[i * 2u] | ((uint16_t)buf[i * 2u + 1u] << 8);
        putu16_raw(i);
        putch(' ');
        puthex8((uint8_t)(rec >> 8));
        puthex8((uint8_t)rec);
        putch(' ');
        if (rec == 0x8000u) {
            puts_zx("UNDO");
        } else if ((rec & 0x8000u) != 0u) {
            puts_zx("BAD");
        } else {
            putsq((uint8_t)(rec & 63u));
            putsq((uint8_t)((rec >> 6) & 63u));
            putch(13u);
        }
    }
    if (got == DUMP_MAX) {
        puts_zx("trunc 512");
    }
    return 0;
}
