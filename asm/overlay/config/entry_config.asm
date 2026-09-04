SECTION code_user

PUBLIC _config_load_ovl_entry
PUBLIC _config_save_ovl_entry
PUBLIC _config_defaults_ovl_entry

EXTERN _config_load_ovl
EXTERN _config_save_ovl
EXTERN _config_defaults_ovl

    DEFB 3
    DW _config_load_ovl_entry
    DW _config_save_ovl_entry
    DW _config_defaults_ovl_entry

DEFC _config_load_ovl_entry = _config_load_ovl
DEFC _config_save_ovl_entry = _config_save_ovl
DEFC _config_defaults_ovl_entry = _config_defaults_ovl
