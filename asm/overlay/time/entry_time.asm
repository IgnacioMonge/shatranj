SECTION code_user

EXTERN _spectranext_time_sync_ovl
EXTERN _spectranext_time_retry_start_ovl
EXTERN _spectranext_time_retry_poll_ovl
EXTERN _spectranext_time_retry_cancel_ovl

    DEFB 4
    DW _spectranext_time_sync_ovl
    DW _spectranext_time_retry_start_ovl
    DW _spectranext_time_retry_poll_ovl
    DW _spectranext_time_retry_cancel_ovl
