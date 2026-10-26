// Load Storage Page — list stored mnemonics from flash or SD card

#include "load_storage.h"
#include "../../core/kef.h"
#include "../../core/storage.h"
#include "../../ui/dialog.h"
#include "../../utils/bip39_filter_zh.h"
#include "../../utils/session_cleanup.h"
#include "../shared/kef_decrypt_page.h"
#include "../shared/key_confirmation.h"
#include "../shared/storage_browser.h"
#include "kern_wally.h"
#include <lvgl.h>
#include <stdlib.h>
#include <string.h>
#include <wally_bip39.h>
#include <wally_core.h>

static void (*success_callback)(void) = NULL;
// Set by load_selected() just before decrypting, consumed by
// success_from_kef_decrypt() once the KEF envelope's plaintext (raw
// entropy) comes back, to know which wordlist to re-render it in.
static bool pending_is_chinese = false;

/* ---------- Key confirmation callbacks ---------- */

static void return_from_key_confirmation(void) {
  key_confirmation_page_destroy();
  storage_browser_show();
}

static void success_from_key_confirmation(void) {
  key_confirmation_page_destroy();
  if (success_callback)
    success_callback();
}

/* ---------- Decrypt callbacks ---------- */

static void return_from_kef_decrypt(void) {
  kef_decrypt_page_destroy();
  storage_browser_show();
}

static void success_from_kef_decrypt(const uint8_t *data, size_t len) {
  // The KEF envelope's plaintext is always raw entropy (see
  // store_mnemonic.c), which is language-neutral -- but for a mnemonic
  // saved from Chinese input, re-rendering it back into words must use
  // the Chinese wordlist to reproduce the Chinese text the user
  // originally entered, instead of key_confirmation_page_create's normal
  // (English) SeedQR-family decoding.
  if (pending_is_chinese) {
    char *mnemonic = NULL;
    if (bip39_filter_zh_init() &&
        kern_bip39_mnemonic_from_bytes(kern_bip39_zh_wordlist(), data, len,
                                       &mnemonic) == WALLY_OK &&
        mnemonic) {
      key_confirmation_page_create(lv_screen_active(),
                                   return_from_key_confirmation,
                                   success_from_key_confirmation, mnemonic,
                                   strlen(mnemonic), true);
      key_confirmation_page_show();
      wally_free_string(mnemonic);
      kef_decrypt_page_destroy();
      return;
    }
    // Fall through to the generic path below rather than silently
    // failing if reconstruction didn't work for some reason.
  }

  key_confirmation_page_create(lv_screen_active(), return_from_key_confirmation,
                               success_from_key_confirmation,
                               (const char *)data, len, true);
  key_confirmation_page_show();
  kef_decrypt_page_destroy();
}

/* ---------- Load selected entry ---------- */

static void load_selected(int idx, const char *filename) {
  (void)idx;

  uint8_t *envelope = NULL;
  size_t envelope_len = 0;

  esp_err_t ret = storage_load_mnemonic(storage_browser_get_location(),
                                        filename, &envelope, &envelope_len);
  if (ret != ESP_OK) {
    dialog_show_error_timeout("Failed to load file", NULL, 0);
    return;
  }

  pending_is_chinese =
      storage_mnemonic_is_chinese(storage_browser_get_location(), filename);

  if (!kef_is_envelope(envelope, envelope_len)) {
    free(envelope);
    dialog_show_error_timeout("Invalid encrypted data", NULL, 0);
    return;
  }

  storage_browser_hide();
  kef_decrypt_page_create(lv_screen_active(), return_from_kef_decrypt,
                          success_from_kef_decrypt, envelope, envelope_len);
  kef_decrypt_page_show();
  free(envelope); /* kef_decrypt_page copies it */
}

/* ---------- Display name ---------- */

static char *get_display_name(storage_location_t loc, const char *filename) {
  uint8_t *envelope = NULL;
  size_t envelope_len = 0;

  if (storage_load_mnemonic(loc, filename, &envelope, &envelope_len) != ESP_OK)
    return strdup(filename);

  char *name = storage_get_kef_display_name(envelope, envelope_len);
  free(envelope);
  return name ? name : strdup(filename);
}

/* ---------- Page lifecycle ---------- */

void load_storage_page_create(lv_obj_t *parent, void (*return_cb)(void),
                              void (*success_cb)(void),
                              storage_location_t location) {
  session_cleanup_register(load_storage_page_destroy);
  if (!parent)
    return;

  success_callback = success_cb;

  storage_browser_config_t config = {
      .item_type_name = "mnemonic",
      .location = location,
      .list_files = storage_list_mnemonics,
      .delete_file = storage_delete_mnemonic,
      .get_display_name = get_display_name,
      .load_selected = load_selected,
      .return_cb = return_cb,
  };

  storage_browser_create(parent, &config);
}

void load_storage_page_show(void) { storage_browser_show(); }

void load_storage_page_hide(void) { storage_browser_hide(); }

void load_storage_page_destroy(void) {
  session_cleanup_unregister(load_storage_page_destroy);
  storage_browser_destroy();
  success_callback = NULL;
  pending_is_chinese = false;
}
