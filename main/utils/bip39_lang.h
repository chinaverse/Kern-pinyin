// BIP39 mnemonic language detection and English translation.
//
// Kern's key-derivation, checksum-validation and storage-entropy code all
// operate on the *English* BIP39 wordlist only. To let a wallet be created,
// restored and displayed entirely in Chinese while reusing that code
// unmodified, every Chinese mnemonic is translated word-for-word (by the
// shared BIP39 word index, which is spec-defined to carry the same
// entropy/checksum bits in every official wordlist) into the equivalent
// English mnemonic immediately before it reaches key derivation.
//
// Important: this means a Chinese mnemonic entered here only reproduces
// the same wallet on firmware/tooling that applies this same
// translate-to-English-then-derive rule. It is NOT the same wallet a
// spec-compliant "Chinese BIP39" wallet would derive from the same
// characters (those hash the Chinese sentence directly, with Unicode NFKD
// normalization Kern does not implement). This is an intentional,
// documented trade-off, not a bug.

#ifndef BIP39_LANG_H
#define BIP39_LANG_H

#include "attributes.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  BIP39_LANG_EN,
  BIP39_LANG_ZH,
} bip39_lang_t;

// Large enough for 24 English BIP39 words (longest word "aerobics" is 8
// chars) plus separating spaces and a NUL terminator; matches the
// MAX_MNEMONIC_LEN convention already used by manual_input.c /
// mnemonic_editor.c.
#define BIP39_LANG_MAX_MNEMONIC_LEN 256

/**
 * Detect which BIP39 wordlist a mnemonic string is written in.
 *
 * English BIP39 words are pure lowercase ASCII. Chinese (simplified) BIP39
 * words are single Han characters, always encoded in UTF-8 as bytes with
 * the high bit set. The two encodings can never produce the same byte
 * sequence, so the presence of any byte >= 0x80 is a fully reliable,
 * O(n) signal -- no wordlist lookup needed.
 */
KERN_WARN_UNUSED_RESULT bip39_lang_t bip39_lang_detect(const char *mnemonic);

/**
 * Validate a mnemonic's checksum against whichever wordlist matches its
 * detected language.
 * @return WALLY_OK on success, or a WALLY_E* error code.
 */
KERN_WARN_UNUSED_RESULT int bip39_lang_validate(const char *mnemonic);

/**
 * Translate a space-separated Chinese mnemonic into the equivalent
 * English mnemonic, word-for-word by shared BIP39 index.
 * @param mnemonic Chinese mnemonic (UTF-8, space-separated)
 * @param out Destination buffer for the English mnemonic
 * @param out_len Size of out
 * @return true on success (out is NUL-terminated), false if any word
 *         wasn't found in the Chinese wordlist, the output didn't fit, or
 *         the wordlists failed to load.
 */
KERN_WARN_UNUSED_RESULT bool
bip39_lang_translate_zh_to_en(const char *mnemonic, char *out, size_t out_len);

/**
 * If `mnemonic` is Chinese, translate it to the equivalent English
 * mnemonic (see bip39_lang_translate_zh_to_en) into a newly
 * kern_secret_alloc'd buffer suitable for feeding into seed derivation.
 * If `mnemonic` is already English, returns a secret-allocated copy
 * unchanged.
 *
 * Caller must free the result with SECURE_FREE_STRING.
 * @return NULL on failure (invalid word, OOM, translation error).
 */
KERN_WARN_UNUSED_RESULT char *
bip39_lang_to_english_secret(const char *mnemonic);

/**
 * Translate a space-separated English mnemonic into the equivalent
 * Chinese mnemonic, word-for-word by shared BIP39 index. Mirrors
 * bip39_lang_translate_zh_to_en in the other direction.
 */
KERN_WARN_UNUSED_RESULT bool
bip39_lang_translate_en_to_zh(const char *mnemonic, char *out, size_t out_len);

/**
 * Translate `mnemonic` (in whichever language it's already in, auto-
 * detected) into `dst_lang` for display purposes -- e.g. showing an
 * English-created wallet's backup in Chinese, or vice versa. This never
 * changes which wallet/keys the mnemonic represents (translation is by
 * shared BIP39 index either way); it only changes how it reads. A
 * mnemonic already in dst_lang is copied through unchanged.
 * @return true on success (out is NUL-terminated), false if any word
 *         wasn't found, the output didn't fit, or a wordlist failed to
 *         load.
 */
KERN_WARN_UNUSED_RESULT bool
bip39_lang_translate_to(const char *mnemonic, bip39_lang_t dst_lang,
                        char *out, size_t out_len);

#endif // BIP39_LANG_H
