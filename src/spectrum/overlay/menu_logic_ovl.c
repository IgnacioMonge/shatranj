/* Menu logic overlay entries live in asm/overlay/menu_logic/entry_menu_logic.asm. */
#include <stdint.h>

extern uint8_t setup_choice[6];
extern uint8_t setup_focus_choice[6];
extern uint8_t setup_focus_board_theme;
extern uint8_t setup_cursor;
extern uint8_t setup_edit_row;
extern char setup_port_text[6];

typedef char menu_logic_ovl_entries_are_asm;
