/*
 * UI Mnemonic Language Selector
 * Reusable component for choosing English or Chinese (Pinyin) word entry
 */

#include "language_selector.h"
#include <stdlib.h>

typedef struct {
  ui_menu_t *menu;
  mnemonic_lang_callback_t on_select;
  ui_menu_callback_t on_back;
} selector_state_t;

static selector_state_t *active_selector = NULL;

static void destroy_selector(void) {
  if (!active_selector)
    return;

  if (active_selector->menu)
    ui_menu_destroy(active_selector->menu);

  free(active_selector);
  active_selector = NULL;
}

static void lang_en_cb(void) {
  if (active_selector && active_selector->on_select) {
    mnemonic_lang_callback_t cb = active_selector->on_select;
    destroy_selector();
    cb(BIP39_LANG_EN);
  }
}

static void lang_zh_cb(void) {
  if (active_selector && active_selector->on_select) {
    mnemonic_lang_callback_t cb = active_selector->on_select;
    destroy_selector();
    cb(BIP39_LANG_ZH);
  }
}

static void back_wrapper_cb(void) {
  if (active_selector && active_selector->on_back) {
    ui_menu_callback_t cb = active_selector->on_back;
    destroy_selector();
    cb();
  }
}

void ui_mnemonic_lang_selector_create(lv_obj_t *parent, ui_menu_callback_t back_cb,
                                      mnemonic_lang_callback_t on_select) {
  if (!parent || !on_select)
    return;

  selector_state_t *selector = malloc(sizeof(selector_state_t));
  if (!selector)
    return;

  selector->on_select = on_select;
  selector->on_back = back_cb;
  selector->menu = ui_menu_create(parent, "Wordlist Language",
                                  back_cb ? back_wrapper_cb : NULL);

  if (!selector->menu) {
    free(selector);
    return;
  }

  ui_menu_add_entry(selector->menu, "English", lang_en_cb);
  ui_menu_add_entry(selector->menu, "Chinese (Pinyin)", lang_zh_cb);

  active_selector = selector;
  ui_menu_show(selector->menu);
}
