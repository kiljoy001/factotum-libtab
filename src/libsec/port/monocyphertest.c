#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <monocypher.h>

static int
unhex(char c)
{
	if('0' <= c && c <= '9')
		return c - '0';
	if('a' <= c && c <= 'f')
		return c - 'a' + 10;
	if('A' <= c && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int
fromhex(uint8_t *out, int nout, const char *hex)
{
	int i, hi, lo;

	if((int)strlen(hex) != nout*2)
		return -1;
	for(i = 0; i < nout; i++){
		hi = unhex(hex[i*2]);
		lo = unhex(hex[i*2+1]);
		if(hi < 0 || lo < 0)
			return -1;
		out[i] = (hi << 4) | lo;
	}
	return 0;
}

static int
same(const char *name, const uint8_t *got, const uint8_t *want, int n)
{
	if(memcmp(got, want, n) == 0)
		return 0;
	fprintf(stderr, "%s mismatch\n", name);
	return -1;
}

int
main(void)
{
	static const char pkhex[] =
		"f65333fa6303b6a23defd7de2af8aa461cb047ccbf12d4edd29ef3b1eba6706b";
	static const char sighex[] =
		"33f67d45a12f7d6a8ec0d8f1c153ed95b9e010db8dd3a372d55d2e2e6fef9383"
		"25553110dd3447fe02e2007c990a9fea59e0719abf83666f19210e51234e0a02";
	static const uint8_t msg[] =
		"tpm9p-user-auth-v1\0rentonsoftworks.coin\0alice\0abcdefghijklmnop";
	uint8_t seed[32], sk[64], pk[32], sig[64], wantpk[32], wantsig[64];
	int i;

	if(fromhex(wantpk, sizeof wantpk, pkhex) < 0
	|| fromhex(wantsig, sizeof wantsig, sighex) < 0){
		fprintf(stderr, "bad test vector\n");
		return 1;
	}

	for(i = 0; i < 32; i++)
		seed[i] = i;
	crypto_eddsa_key_pair(sk, pk, seed);
	crypto_eddsa_sign(sig, sk, msg, sizeof(msg)-1);

	if(same("public key", pk, wantpk, sizeof pk) < 0
	|| same("signature", sig, wantsig, sizeof sig) < 0)
		return 1;
	if(crypto_eddsa_check(sig, pk, msg, sizeof(msg)-1) != 0){
		fprintf(stderr, "signature did not verify\n");
		return 1;
	}
	sig[0] ^= 1;
	if(crypto_eddsa_check(sig, pk, msg, sizeof(msg)-1) == 0){
		fprintf(stderr, "tampered signature verified\n");
		return 1;
	}

	printf("monocypher eddsa vectors ok\n");
	return 0;
}
