/*
 * UI Mnemonic Language Selector
 * Reusable component for choosing English or Chinese (Pinyin) word entry
 */

#ifndef LANGUAGE_SELECTOR_H
#define LANGUAGE_SELECTOR_H

#include "../utils/bip39_lang.h"
#include "menu.h"
#include <lvgl.h>

// Callback type for mnemonic language selection
typedef void (*mnemonic_lang_callback_t)(bip39_lang_t lang);

/**
 * @brief Create and show a mnemonic language selector menu
 *
 * The selector is displayed immediately and auto-destroys after any action
 * (selection or back). The on_select callback receives BIP39_LANG_EN or
 * BIP39_LANG_ZH.
 *
 * @param parent Parent LVGL object
 * @param back_cb Callback for back button (NULL for no back button)
 * @param on_select Callback when a language is selected
 */
void ui_mnemonic_lang_selector_create(lv_obj_t *parent, ui_menu_callback_t back_cb,
                                      mnemonic_lang_callback_t on_select);

#endif
