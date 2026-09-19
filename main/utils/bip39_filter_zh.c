// BIP39 Chinese (simplified) wordlist filtering utilities for pinyin input

#include "bip39_filter_zh.h"
#include "kern_wally.h"
#include <string.h>
#include <wally_bip39.h>
#include <wally_core.h>

static const struct words *wordlist = NULL;

// Cache for valid last words to avoid recalculating on every keystroke.
// Unlike the English filter (which caches word pointers), we cache word
// indices: the pinyin reading and the UTF-8 word text are both derived
// from the index via bip39_zh_pinyin[] / bip39_get_word_by_index(). Sized
// to hold the full unfiltered candidate set (see
// BIP39_ZH_MAX_VALID_LAST_WORDS) -- pinyin-prefix narrowing happens when
// reading back out of this cache, not while filling it.
#define MAX_VALID_LAST_WORDS_ZH BIP39_ZH_MAX_VALID_LAST_WORDS
static int valid_last_words_cache[MAX_VALID_LAST_WORDS_ZH];
static int valid_last_words_count = 0;

bool bip39_filter_zh_init(void) {
  if (wordlist)
    return true;
  wordlist = kern_bip39_zh_wordlist();
  return wordlist != NULL;
}

uint32_t bip39_filter_zh_get_valid_letters(const char *prefix, int prefix_len) {
  uint32_t mask = 0;
  if (!wordlist)
    return 0xFFFFFFFF;

  for (int letter = 0; letter < 26; letter++) {
    char test_prefix[BIP39_ZH_MAX_PINYIN_LEN + 2];
    int test_len = prefix_len + 1;

    if (prefix && prefix_len > 0) {
      int copy_len = prefix_len < BIP39_ZH_MAX_PINYIN_LEN
                        ? prefix_len
                        : BIP39_ZH_MAX_PINYIN_LEN;
      memcpy(test_prefix, prefix, copy_len);
      test_prefix[copy_len] = 'a' + letter;
      test_prefix[copy_len + 1] = '\0';
    } else {
      test_prefix[0] = 'a' + letter;
      test_prefix[1] = '\0';
      test_len = 1;
    }

    for (size_t i = 0; i < BIP39_ZH_WORDLIST_SIZE; i++) {
      if (strncmp(bip39_zh_pinyin[i], test_prefix, test_len) == 0) {
        mask |= (1u << letter);
        break;
      }
    }
  }
  return mask;
}

int bip39_filter_zh_by_prefix(const char *prefix, int prefix_len,
                              const char **out_words, int max_words) {
  int count = 0;
  if (!wordlist || !out_words || max_words <= 0)
    return 0;
  if (!prefix || prefix_len <= 0)
    return 0;

  for (size_t i = 0; i < BIP39_ZH_WORDLIST_SIZE && count < max_words; i++) {
    if (strncmp(bip39_zh_pinyin[i], prefix, prefix_len) == 0) {
      const char *word = bip39_get_word_by_index(wordlist, i);
      if (word)
        out_words[count++] = word;
    }
  }
  return count;
}

int bip39_filter_zh_count_matches(const char *prefix, int prefix_len) {
  int count = 0;
  if (!wordlist)
    return 0;
  if (!prefix || prefix_len <= 0)
    return BIP39_ZH_WORDLIST_SIZE;

  for (size_t i = 0; i < BIP39_ZH_WORDLIST_SIZE; i++) {
    if (strncmp(bip39_zh_pinyin[i], prefix, prefix_len) == 0)
      count++;
  }
  return count;
}

int bip39_filter_zh_get_word_index(const char *word) {
  if (!wordlist || !word)
    return -1;

  for (size_t i = 0; i < BIP39_ZH_WORDLIST_SIZE; i++) {
    const char *w = bip39_get_word_by_index(wordlist, i);
    if (w && strcmp(w, word) == 0)
      return (int)i;
  }
  return -1;
}

void bip39_filter_zh_clear_last_word_cache(void) { valid_last_words_count = 0; }

/* Warms valid_last_words_cache and returns how many entries it holds. */
static int ensure_last_word_cache(const char entered_words[24][16],
                                  int word_count) {
  if (valid_last_words_count == 0) {
    const char *temp[MAX_VALID_LAST_WORDS_ZH];
    return bip39_filter_zh_get_valid_last_words(entered_words, word_count,
                                                temp, MAX_VALID_LAST_WORDS_ZH);
  }
  return valid_last_words_count;
}

int bip39_filter_zh_get_valid_last_words(const char entered_words[24][16],
                                         int word_count, const char **out_words,
                                         int max_words) {
  if (!wordlist || !entered_words || !out_words || max_words <= 0)
    return 0;
  if (word_count != 12 && word_count != 24)
    return 0;

  // 12 words: 128 bits entropy + 4 bits checksum, last word has 7 entropy bits
  // 24 words: 256 bits entropy + 8 bits checksum, last word has 3 entropy bits
  size_t checksum_bits = word_count / 3;
  size_t entropy_bytes = ((word_count * 11) - checksum_bits) / 8;
  size_t last_word_entropy_bits = 11 - checksum_bits;
  int num_possibilities = 1 << last_word_entropy_bits;

  // Pack word indices (11 bits each) from first N-1 words
  uint8_t packed[32] = {0};
  int bit_pos = 0;

  for (int i = 0; i < word_count - 1; i++) {
    int idx = bip39_filter_zh_get_word_index(entered_words[i]);
    if (idx < 0)
      return 0;

    for (int b = 10; b >= 0; b--) {
      int byte_idx = bit_pos / 8;
      int bit_idx = 7 - (bit_pos % 8);
      if (idx & (1 << b))
        packed[byte_idx] |= (1 << bit_idx);
      bit_pos++;
    }
  }

  int count = 0;
  valid_last_words_count = 0;

  for (int entropy_val = 0;
       entropy_val < num_possibilities && count < max_words; entropy_val++) {
    uint8_t test_packed[32];
    memcpy(test_packed, packed, sizeof(test_packed));

    int test_bit_pos = bit_pos;
    for (int b = last_word_entropy_bits - 1; b >= 0; b--) {
      int byte_idx = test_bit_pos / 8;
      int bit_idx = 7 - (test_bit_pos % 8);
      if (entropy_val & (1 << b))
        test_packed[byte_idx] |= (1 << bit_idx);
      else
        test_packed[byte_idx] &= ~(1 << bit_idx);
      test_bit_pos++;
    }

    char *new_mnemonic = NULL;
    if (kern_bip39_mnemonic_from_bytes(wordlist, test_packed, entropy_bytes,
                                       &new_mnemonic) != WALLY_OK)
      continue;

    char *last_space = strrchr(new_mnemonic, ' ');
    const char *last_word_str = last_space ? last_space + 1 : new_mnemonic;
    int idx = bip39_filter_zh_get_word_index(last_word_str);
    if (idx >= 0) {
      const char *w = bip39_get_word_by_index(wordlist, idx);
      out_words[count++] = w;
      if (valid_last_words_count < MAX_VALID_LAST_WORDS_ZH)
        valid_last_words_cache[valid_last_words_count++] = idx;
    }

    wally_free_string(new_mnemonic);
  }

  return count;
}

uint32_t
bip39_filter_zh_get_valid_letters_for_last_word(const char entered_words[24][16],
                                                int word_count,
                                                const char *prefix,
                                                int prefix_len) {
  if (!wordlist)
    return 0xFFFFFFFF;

  if (ensure_last_word_cache(entered_words, word_count) == 0)
    return 0;

  uint32_t mask = 0;
  for (int letter = 0; letter < 26; letter++) {
    char test_prefix[BIP39_ZH_MAX_PINYIN_LEN + 2];
    int test_len;

    if (prefix && prefix_len > 0) {
      int copy_len = prefix_len < BIP39_ZH_MAX_PINYIN_LEN
                        ? prefix_len
                        : BIP39_ZH_MAX_PINYIN_LEN;
      memcpy(test_prefix, prefix, copy_len);
      test_prefix[copy_len] = 'a' + letter;
      test_prefix[copy_len + 1] = '\0';
      test_len = prefix_len + 1;
    } else {
      test_prefix[0] = 'a' + letter;
      test_prefix[1] = '\0';
      test_len = 1;
    }

    for (int i = 0; i < valid_last_words_count; i++) {
      const char *py = bip39_zh_pinyin[valid_last_words_cache[i]];
      if (strncmp(py, test_prefix, test_len) == 0) {
        mask |= (1u << letter);
        break;
      }
    }
  }

  return mask;
}

int bip39_filter_zh_last_word_by_prefix(const char entered_words[24][16],
                                        int word_count, const char *prefix,
                                        int prefix_len, const char **out_words,
                                        int max_words) {
  if (!wordlist || !out_words || max_words <= 0)
    return 0;

  if (ensure_last_word_cache(entered_words, word_count) == 0)
    return 0;

  if (!prefix || prefix_len <= 0) {
    int count =
        valid_last_words_count < max_words ? valid_last_words_count : max_words;
    for (int i = 0; i < count; i++)
      out_words[i] = bip39_get_word_by_index(wordlist, valid_last_words_cache[i]);
    return count;
  }

  int count = 0;
  for (int i = 0; i < valid_last_words_count && count < max_words; i++) {
    const char *py = bip39_zh_pinyin[valid_last_words_cache[i]];
    if (strncmp(py, prefix, prefix_len) == 0)
      out_words[count++] =
          bip39_get_word_by_index(wordlist, valid_last_words_cache[i]);
  }

  return count;
}
