#ifndef SPECTRUM_SETUP_FIXTURE_H
#define SPECTRUM_SETUP_FIXTURE_H
/* Host binding to the same phase-shared workspace defined by app.c. */
const volatile spectrum_setup_view_t *netchesszx_host_setup_view(void)
{
    return &app_workspace.setup;
}
#define setup_test_state app_workspace.setup
void spectrum_setup_set_visible(uint16_t mask) { setup_test_state.visible_mask = mask; }
void spectrum_setup_accept_set(void)
{
    setup_test_state.choice[SETUP_CHOICE_SET] = setup_test_state.focus_choice[SETUP_CHOICE_SET];
    setup_test_state.defined_mask |= SETUP_MASK_SET;
    setup_test_state.action_focus = 0u;
    setup_test_state.cursor = SETUP_ROW_HINTS;
}
#ifndef SPECTRUM_SETUP_CUSTOM_OVERLAYS
uint8_t netchesszx_setup_dispatch_overlay(uint8_t key) { return netchesszx_setup_step_overlay(key); }
void netchesszx_setup_time_ui(void) {}
uint8_t netchesszx_setup_time_init(uint8_t flags) { (void)flags; return 1u; }
uint8_t netchesszx_setup_time_commit(void) { return 1u; }
#endif
#endif
