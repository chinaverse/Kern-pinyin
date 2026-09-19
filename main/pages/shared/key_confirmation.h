#ifndef KEY_CONFIRMATION_H
#define KEY_CONFIRMATION_H

#include <lvgl.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Create the key confirmation page
 *
 * Validates and processes mnemonic content from QR codes.
 * Supports multiple formats (auto-detected):
 * - Plaintext: Space-separated BIP39 words
 * - Compact SeedQR: 16/32 bytes binary entropy
 * - SeedQR: 48/96 digit numeric string (4 digits per word index)
 *
 * @param parent Parent LVGL object
 * @param return_cb Callback when user wants to go back
 * @param success_cb Callback when mnemonic is successfully loaded
 * @param content QR content (plaintext, binary, or numeric)
 * @param content_len Length of content (required for binary detection)
 * @param offer_lang_choice If true, ask the user whether to display/load
 *        the resolved mnemonic in English or Chinese before proceeding
 *        (word-for-word translation by shared BIP39 index -- never
 *        changes which wallet it is). Intended for genuine "import from
 *        elsewhere" entry points (Flash/SD, QR/camera scan), where the
 *        content carries no language choice of its own (Compact SeedQR
 *        and SeedQR are raw entropy/indices) or was set by someone else.
 *        Pass false for flows where the caller already established the
 *        language upstream (manual pinyin/English entry, dice/camera
 *        generation review) -- asking again there would be redundant and
 *        could silently override a choice the user just made.
 */
void key_confirmation_page_create(lv_obj_t *parent, void (*return_cb)(void),
                                  void (*success_cb)(void), const char *content,
                                  size_t content_len, bool offer_lang_choice);
void key_confirmation_page_show(void);
void key_confirmation_page_hide(void);
void key_confirmation_page_destroy(void);

#endif // KEY_CONFIRMATION_H
