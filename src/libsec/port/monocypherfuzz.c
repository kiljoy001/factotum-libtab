#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <monocypher.h>

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	uint8_t seed[32], sk[64], pk[32], sig[64], *msg;
	size_t msglen;

	if(size < sizeof seed)
		return 0;
	msglen = size - sizeof seed;
	if(msglen > 8192)
		msglen = 8192;
	memmove(seed, data, sizeof seed);
	msg = malloc(msglen == 0 ? 1 : msglen);
	if(msg == NULL)
		abort();
	memmove(msg, data + sizeof seed, msglen);

	crypto_eddsa_key_pair(sk, pk, seed);
	crypto_eddsa_sign(sig, sk, msg, msglen);
	if(crypto_eddsa_check(sig, pk, msg, msglen) != 0)
		abort();

	sig[0] ^= 1;
	if(crypto_eddsa_check(sig, pk, msg, msglen) == 0)
		abort();

	if(size >= 128)
		(void)crypto_eddsa_check(data + 32, data + 96, msg, msglen);

	free(msg);
	return 0;
}
