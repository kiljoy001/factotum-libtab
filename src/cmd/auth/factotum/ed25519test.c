#include "std.h"
#include "dat.h"
#include "monokey.h"

static int locked;
int vaultrequire(char *op) { USED(op); return locked ? -1 : 0; }

static void
need(int ok, char *what)
{
	if(!ok)
		sysfatal("ed25519: %s: %r", what);
}

static void
release(Key *k)
{
	monoclosekey(k);
	freeattr(k->attr);
	freeattr(k->privattr);
	memset(k, 0, sizeof *k);
}

static int
load(Key *k, char *attrs, char *private)
{
	memset(k, 0, sizeof *k);
	k->attr = parseattr(attrs);
	k->privattr = parseattr(private);
	return ed25519checkkey(k);
}

static void
vector(char *seed, char *pub, char *message, char *signature)
{
	Key k, v, imported;
	char *attrs, *private;
	uchar msg[2], expected[64], sig[64], pk[32];
	int n;

	n = strlen(message)/2;
	if(n) need(dec16(msg, sizeof msg, message, strlen(message)) == n, "decode message");
	need(dec16(pk, sizeof pk, pub, strlen(pub)) == 32, "decode public key");
	need(dec16(expected, sizeof expected, signature, strlen(signature)) == 64, "decode signature");
	private = smprint("!seed=%s", seed);
	need(load(&k, "proto=ed25519 user=alice", private) == 0, "load seed");
	free(private);
	need(memcmp(((MonoKey*)k.priv)->pk, pk, 32) == 0, "RFC public key");
	need(monosign(&k, msg, n, sig) == 0, "sign");
	if(memcmp(sig, expected, 64) != 0){
 char hex[129]; enc16(hex, sizeof hex, sig, 64); fprint(2, "seed %s got %s expected %s\n", seed, hex, signature);
}
	need(memcmp(sig, expected, 64) == 0, "RFC signature");
	attrs = smprint("proto=ed25519 user=alice pub=%s", pub);
	need(load(&v, attrs, "") == 0, "load public-only key");
	need(monoverify(&v, msg, n, sig) == 0, "verify RFC signature");
	need(monosign(&v, msg, n, sig) < 0, "public-only key cannot sign");
	sig[0] ^= 1;
	need(monoverify(&v, msg, n, sig) < 0, "reject altered signature");
	need(monoverify(&v, (uchar*)"wrong", 5, expected) < 0, "reject altered message");
	memset(sig, 255, sizeof sig);
	need(monoverify(&v, msg, n, sig) < 0, "reject noncanonical signature");
	private = smprint("!sk=%s%s", seed, pub);
	need(load(&imported, attrs, private) == 0, "import Go seed/public key format");
	need(monosign(&imported, msg, n, sig) == 0, "sign imported key");
	need(memcmp(sig, expected, 64) == 0, "imported key signature");
	locked = 1;
	need(monosign(&imported, msg, n, sig) < 0, "vault denies signing");
	need(monoverify(&v, msg, n, expected) == 0, "locked vault permits verification");
	locked = 0;
	release(&imported); release(&v); release(&k);
	free(private); free(attrs);
}

void
threadmain(int, char**)
{
	Key k;
	quotefmtinstall(); fmtinstall('A', attrfmt); fmtinstall('N', attrnamefmt);
	/* RFC 8032 section 7.1, tests 1, 2 and 3. */
	vector("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
	 "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
	 "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555"
	 "fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
	vector("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
	 "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
	 "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
	 "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00");
	vector("c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
	 "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
	 "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
	 "18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a");
	need(load(&k, "proto=ed25519 alg=libtab-eddsa-blake2b-v1", "") < 0, "reject other algorithm"); release(&k);
	need(load(&k, "proto=ed25519", "!seed=abcd") < 0, "reject short seed"); release(&k);
	need(load(&k, "proto=ed25519", "") < 0, "reject missing key"); release(&k);
	need(load(&k, "proto=ed25519 pub=0000000000000000000000000000000000000000000000000000000000000000",
	 "!seed=9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60") < 0, "reject public mismatch"); release(&k);
	need(load(&k, "proto=ed25519",
	 "!sk=9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f600000000000000000000000000000000000000000000000000000000000000000") < 0, "reject corrupt Go key"); release(&k);
	print("ed25519 RFC vectors and key policy tests ok\n");
	threadexitsall(nil);
}
