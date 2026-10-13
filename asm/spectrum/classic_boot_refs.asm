; Resident references to the one-shot Classic capsule loaded at 0x6800.

IFDEF NETCHESSZX_CLASSIC_BOOT

PUBLIC _spectrum_assets_load
PUBLIC _spectrum_assets_fatal
PUBLIC _spectrum_frame_clock_init
PUBLIC _spectrum_app_tick_init
PUBLIC classic_boot_frame_clock_isr

EXTERN frame_clock_isr

DEFC _spectrum_assets_load = 0x6800
DEFC _spectrum_assets_fatal = 0x6849
DEFC _spectrum_frame_clock_init = 0x6833
DEFC _spectrum_app_tick_init = 0x6842
DEFC classic_boot_frame_clock_isr = frame_clock_isr

ENDIF
