// BIP39 mnemonic language detection and translation (English <-> Chinese).

#include "bip39_lang.h"
#include "bip39_filter_zh.h"
#include "kern_wally.h"
#include "secure_mem.h"
#include <stdlib.h>
#include <string.h>
#include <wally_bip39.h>
#include <wally_core.h>

// A Chinese BIP39 word is exactly one Han character: at most 3 UTF-8 bytes
// plus a NUL terminator.
#define ZH_WORD_BUF_LEN 8

bip39_lang_t bip39_lang_detect(const char *mnemonic) {
  if (mnemonic) {
    for (const unsigned char *p = (const unsigned char *)mnemonic; *p; p++) {
      if (*p >= 0x80)
        return BIP39_LANG_ZH;
    }
  }
  return BIP39_LANG_EN;
}

int bip39_lang_validate(const char *mnemonic) {
  if (!mnemonic)
    return WALLY_EINVAL;

  if (bip39_lang_detect(mnemonic) == BIP39_LANG_ZH) {
    if (!bip39_filter_zh_init())
      return WALLY_ERROR;
    return bip39_mnemonic_validate(kern_bip39_zh_wordlist(), mnemonic);
  }

  return bip39_mnemonic_validate(NULL, mnemonic);
}

bool bip39_lang_translate_zh_to_en(const char *mnemonic, char *out,
                                   size_t out_len) {
  if (!mnemonic || !out || out_len == 0)
    return false;

  if (!bip39_filter_zh_init())
    return false;

  struct words *en_words = NULL;
  if (bip39_get_wordlist(NULL, &en_words) != WALLY_OK || !en_words)
    return false;

  out[0] = '\0';
  size_t out_pos = 0;
  const char *p = mnemonic;
  bool wrote_any = false;

  while (*p) {
    while (*p == ' ')
      p++;
    if (!*p)
      break;

    const char *word_start = p;
    while (*p && *p != ' ')
      p++;
    size_t word_len = (size_t)(p - word_start);

    char word_buf[ZH_WORD_BUF_LEN];
    if (word_len == 0 || word_len >= sizeof(word_buf))
      return false;
    memcpy(word_buf, word_start, word_len);
    word_buf[word_len] = '\0';

    int idx = bip39_filter_zh_get_word_index(word_buf);
    secure_memzero(word_buf, sizeof(word_buf));
    if (idx < 0)
      return false;

    const char *en_word = bip39_get_word_by_index(en_words, (size_t)idx);
    if (!en_word)
      return false;

    size_t en_word_len = strlen(en_word);
    size_t needed = en_word_len + (wrote_any ? 1 : 0);
    if (out_pos + needed >= out_len)
      return false;

    if (wrote_any)
      out[out_pos++] = ' ';
    memcpy(out + out_pos, en_word, en_word_len);
    out_pos += en_word_len;
    out[out_pos] = '\0';
    wrote_any = true;
  }

  return wrote_any;
}

bool bip39_lang_translate_en_to_zh(const char *mnemonic, char *out,
                                   size_t out_len) {
  if (!mnemonic || !out || out_len == 0)
    return false;

  const struct words *zh_words = kern_bip39_zh_wordlist();
  struct words *en_words = NULL;
  if (!zh_words || bip39_get_wordlist(NULL, &en_words) != WALLY_OK ||
      !en_words)
    return false;

  out[0] = '\0';
  size_t out_pos = 0;
  const char *p = mnemonic;
  bool wrote_any = false;

  while (*p) {
    while (*p == ' ')
      p++;
    if (!*p)
      break;

    const char *word_start = p;
    while (*p && *p != ' ')
      p++;
    size_t word_len = (size_t)(p - word_start);

    /* Longest English BIP39 word ("abandon"/"aerobics"...) is 8 chars. */
    char word_buf[16];
    if (word_len == 0 || word_len >= sizeof(word_buf))
      return false;
    memcpy(word_buf, word_start, word_len);
    word_buf[word_len] = '\0';

    int idx = -1;
    for (size_t i = 0; i < BIP39_ZH_WORDLIST_SIZE; i++) {
      const char *w = bip39_get_word_by_index(en_words, i);
      if (w && strcmp(w, word_buf) == 0) {
        idx = (int)i;
        break;
      }
    }
    if (idx < 0)
      return false;

    const char *zh_word = bip39_get_word_by_index(zh_words, (size_t)idx);
    if (!zh_word)
      return false;

    size_t zh_word_len = strlen(zh_word);
    size_t needed = zh_word_len + (wrote_any ? 1 : 0);
    if (out_pos + needed >= out_len)
      return false;

    if (wrote_any)
      out[out_pos++] = ' ';
    memcpy(out + out_pos, zh_word, zh_word_len);
    out_pos += zh_word_len;
    out[out_pos] = '\0';
    wrote_any = true;
  }

  return wrote_any;
}

bool bip39_lang_translate_to(const char *mnemonic, bip39_lang_t dst_lang,
                             char *out, size_t out_len) {
  if (!mnemonic || !out || out_len == 0)
    return false;

  if (bip39_lang_detect(mnemonic) == dst_lang) {
    size_t len = strlen(mnemonic);
    if (len >= out_len)
      return false;
    memcpy(out, mnemonic, len + 1);
    return true;
  }

  return dst_lang == BIP39_LANG_EN
             ? bip39_lang_translate_zh_to_en(mnemonic, out, out_len)
             : bip39_lang_translate_en_to_zh(mnemonic, out, out_len);
}

char *bip39_lang_to_english_secret(const char *mnemonic) {
  if (!mnemonic)
    return NULL;

  if (bip39_lang_detect(mnemonic) != BIP39_LANG_ZH)
    return kern_secret_strdup(mnemonic);

  char *out = kern_secret_alloc(BIP39_LANG_MAX_MNEMONIC_LEN);
  if (!out)
    return NULL;

  if (!bip39_lang_translate_zh_to_en(mnemonic, out,
                                     BIP39_LANG_MAX_MNEMONIC_LEN)) {
    SECURE_FREE_BUFFER(out, BIP39_LANG_MAX_MNEMONIC_LEN);
    return NULL;
  }

  return out;
}
