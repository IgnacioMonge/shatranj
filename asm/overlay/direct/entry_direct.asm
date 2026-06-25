SECTION code_user

EXTERN _direct_listen_ovl
EXTERN _direct_connect_ovl
EXTERN _direct_wait_pc_connect_ovl
EXTERN _direct_read_payload_ovl
EXTERN _direct_send_text_ovl

    DW 5
    DW _direct_listen_ovl
    DW _direct_connect_ovl
    DW _direct_wait_pc_connect_ovl
    DW _direct_read_payload_ovl_entry
    DW _direct_send_text_ovl_entry

_direct_read_payload_ovl_entry:
    ex de, hl
    jp _direct_read_payload_ovl

_direct_send_text_ovl_entry:
    ex de, hl
    jp _direct_send_text_ovl
