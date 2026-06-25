SECTION code_user

EXTERN _mqtt_tx_send_text_ovl
EXTERN _mqtt_tx_publish_setup_ovl
EXTERN _mqtt_tx_publish_session_ovl
EXTERN _mqtt_tx_sync_time_ovl

    DW 4
    DW _mqtt_tx_send_text_ovl_entry
    DW _mqtt_tx_publish_setup_ovl_entry
    DW _mqtt_tx_publish_session_ovl
    DW _mqtt_tx_sync_time_ovl

_mqtt_tx_send_text_ovl_entry:
    ex de, hl
    jp _mqtt_tx_send_text_ovl

_mqtt_tx_publish_setup_ovl_entry:
    ex de, hl
    jp _mqtt_tx_publish_setup_ovl
