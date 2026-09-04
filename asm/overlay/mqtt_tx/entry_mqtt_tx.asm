SECTION code_user

EXTERN _mqtt_tx_send_text_ovl
EXTERN _mqtt_tx_publish_setup_ovl
EXTERN _mqtt_tx_sync_time_ovl
EXTERN _mqtt_tx_publish_presence_ovl
EXTERN _mqtt_tx_clock_retry_start_ovl
EXTERN _mqtt_tx_clock_retry_poll_ovl

    DEFB 6
    DW _mqtt_tx_send_text_ovl_entry
    DW _mqtt_tx_publish_setup_ovl_entry
    DW _mqtt_tx_sync_time_ovl
    DW _mqtt_tx_publish_presence_ovl
    DW _mqtt_tx_clock_retry_start_ovl
    DW _mqtt_tx_clock_retry_poll_ovl

DEFC _mqtt_tx_send_text_ovl_entry = _mqtt_tx_send_text_ovl

DEFC _mqtt_tx_publish_setup_ovl_entry = _mqtt_tx_publish_setup_ovl
