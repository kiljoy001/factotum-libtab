/* Test-only host I/O; the cryptographic modules have no libc dependency. */
#include <stdio.h>
#include "monocypher-ed25519.h"

int main(void)
{
    uint8_t input[32768], secret[64], public_key[32], signature[64];
    size_t n = fread(input, 1, sizeof input, stdin);
    if (n < 96 || ferror(stdin) || !feof(stdin)) return 1;
    crypto_ed25519_key_pair(secret, public_key, input);
    if (crypto_ed25519_check(input + 32, public_key, input + 96, n - 96)) return 2;
    crypto_ed25519_sign(signature, secret, input + 96, n - 96);
    crypto_wipe(secret, sizeof secret);
    if (fwrite(public_key, 1, 32, stdout) != 32 || fwrite(signature, 1, 64, stdout) != 64) return 3;
    return 0;
}
