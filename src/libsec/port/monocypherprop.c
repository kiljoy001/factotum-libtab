#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <monocypher.h>

static uint64_t rndstate = 0x9e3779b97f4a7c15ULL;

static uint64_t
rnd64(void)
{
	uint64_t x;

	x = rndstate;
	x ^= x >> 12;
	x ^= x << 25;
	x ^= x >> 27;
	rndstate = x;
	return x * 0x2545f4914f6cdd1dULL;
}

static void
fill(uint8_t *p, size_t n)
{
	size_t i;

	for(i = 0; i < n; i++)
		p[i] = rnd64() & 0xff;
}

static int
fail(int iter, const char *msg)
{
	fprintf(stderr, "property failed at iteration %d: %s\n", iter, msg);
	return -1;
}

static int
one(int iter)
{
	uint8_t seed[32], seed2[32], sk[64], sk2[64], pk[32], pk2[32];
	uint8_t sig[64], sig2[64], wrongsig[64], wrongpk[32], *msg, *tampered;
	size_t msglen;
	int ret;

	msg = NULL;
	tampered = NULL;
	ret = -1;
	msglen = rnd64() % 4097;
	msg = malloc(msglen == 0 ? 1 : msglen);
	tampered = malloc(msglen == 0 ? 1 : msglen);
	if(msg == NULL || tampered == NULL)
		goto out;
	fill(seed, sizeof seed);
	memmove(seed2, seed, sizeof seed);
	fill(msg, msglen);

	crypto_eddsa_key_pair(sk, pk, seed);
	crypto_eddsa_key_pair(sk2, pk2, seed2);
	if(memcmp(sk, sk2, sizeof sk) != 0 || memcmp(pk, pk2, sizeof pk) != 0)
		return fail(iter, "key derivation is not deterministic");

	crypto_eddsa_sign(sig, sk, msg, msglen);
	crypto_eddsa_sign(sig2, sk, msg, msglen);
	if(memcmp(sig, sig2, sizeof sig) != 0)
		return fail(iter, "signature is not deterministic");
	if(crypto_eddsa_check(sig, pk, msg, msglen) != 0)
		return fail(iter, "signature did not verify");

	memmove(wrongsig, sig, sizeof wrongsig);
	wrongsig[rnd64() % sizeof wrongsig] ^= 1 + (rnd64() % 255);
	if(crypto_eddsa_check(wrongsig, pk, msg, msglen) == 0)
		return fail(iter, "mutated signature verified");

	if(msglen > 0){
		memmove(tampered, msg, msglen);
		tampered[rnd64() % msglen] ^= 1 + (rnd64() % 255);
		if(crypto_eddsa_check(sig, pk, tampered, msglen) == 0)
			return fail(iter, "mutated message verified");
	}

	fill(seed2, sizeof seed2);
	crypto_eddsa_key_pair(sk2, wrongpk, seed2);
	if(memcmp(wrongpk, pk, sizeof pk) != 0
	&& crypto_eddsa_check(sig, wrongpk, msg, msglen) == 0)
		return fail(iter, "wrong public key verified");
	ret = 0;

out:
	free(msg);
	free(tampered);
	return ret;
}

int
main(int argc, char **argv)
{
	int i, n;

	n = 2000;
	if(argc > 1)
		n = atoi(argv[1]);
	if(n <= 0)
		n = 1;
	for(i = 0; i < n; i++)
		if(one(i) < 0)
			return 1;
	printf("monocypher property tests ok (%d iterations)\n", n);
	return 0;
}
