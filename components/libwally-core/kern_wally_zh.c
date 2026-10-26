/* Exposes the BIP39 Chinese (simplified) wordlist, which upstream libwally
 * only compiles in when BUILD_MINIMAL is unset (see bip39.c). Kern builds
 * with BUILD_MINIMAL=1 to avoid linking the unused es/fr/it/jp wordlists,
 * so instead of dropping that flag we compile just the Chinese simplified
 * table (data/wordlists/chinese_simplified.c is a self-contained,
 * generated data file -- see its header comment) into this always-built
 * translation unit and hand out a pointer to it directly, bypassing
 * bip39_get_wordlist()'s language-name lookup table (which is also gated
 * by BUILD_MINIMAL and would otherwise silently fall back to English).
 */
#include "data/wordlists/chinese_simplified.c"

const struct words *kern_bip39_zh_wordlist(void) { return &zhs_words; }
