#include "std.h"
#include "dat.h"
#include "monokey.h"

static char seedhex[] =
	"000102030405060708090a0b0c0d0e0f"
	"101112131415161718191a1b1c1d1e1f";
static char badpub[] =
	"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=";
static char msg[] = "factotum monocypher unit test";

int
vaultrequire(char *op)
{
	USED(op);

	return 0;
}

static void
setupfmt(void)
{
	static int once;

	if(once)
		return;
	quotefmtinstall();
	fmtinstall('A', attrfmt);
	fmtinstall('N', attrnamefmt);
	once = 1;
}

static void
freekey(Key *k)
{
	monoclosekey(k);
	freeattr(k->attr);
	freeattr(k->privattr);
	memset(k, 0, sizeof *k);
}

static int
mkkey(Key *k, char *pub, char *priv)
{
	memset(k, 0, sizeof *k);
	k->attr = parseattr(pub);
	if(priv != nil && priv[0] != 0)
		k->privattr = parseattr(priv);
	if(k->attr == nil || (priv != nil && priv[0] != 0 && k->privattr == nil)){
		fprint(2, "parseattr failed\n");
		freekey(k);
		return -1;
	}
	return 0;
}

static int
require(int ok, char *what)
{
	if(ok)
		return 0;
	fprint(2, "%s\n", what);
	return -1;
}

static int
test_seed_key(void)
{
	Key k;
	char *priv, *pub;
	uchar sig[MonoSigLen];
	int ret;

	ret = -1;
	priv = smprint("!seed=%s", seedhex);
	if(priv == nil)
		return -1;
	if(mkkey(&k, "proto=monocypher user=alice", priv) < 0)
		goto out0;
	if(monocheckkey(&k) < 0){
		fprint(2, "monocheckkey seed: %r\n");
		goto out1;
	}
	pub = strfindattr(k.attr, "pub");
	if(require(pub != nil, "monocheckkey did not publish pub") < 0)
		goto out1;
	if(monosign(&k, (uchar*)msg, strlen(msg), sig) < 0){
		fprint(2, "monosign: %r\n");
		goto out1;
	}
	if(require(monoverify(&k, (uchar*)msg, strlen(msg), sig) == 0,
	    "signature did not verify") < 0)
		goto out1;
	sig[0] ^= 1;
	if(require(monoverify(&k, (uchar*)msg, strlen(msg), sig) != 0,
	    "tampered signature verified") < 0)
		goto out1;
	ret = 0;
out1:
	memset(sig, 0, sizeof sig);
	freekey(&k);
out0:
	free(priv);
	return ret;
}

static int
test_signed_cell(void)
{
	Key k;
	char *cell, *priv;
	uchar sig[MonoSigLen];
	int colons, i, ret;

	ret = -1;
	cell = nil;
	priv = smprint("!seed=%s", seedhex);
	if(priv == nil)
		return -1;
	if(mkkey(&k, "proto=monocypher user=alice", priv) < 0)
		goto out0;
	if(monocheckkey(&k) < 0)
		goto out1;
	if(monosign(&k, (uchar*)msg, strlen(msg), sig) < 0)
		goto out1;
	cell = monosignedcell((uchar*)msg, strlen(msg), sig);
	if(cell == nil)
		goto out1;
	if(require(strncmp(cell, "signed:", 7) == 0,
	    "signed cell missing prefix") < 0)
		goto out1;
	colons = 0;
	for(i = 0; cell[i] != 0; i++)
		if(cell[i] == ':')
			colons++;
	if(require(colons == 2, "signed cell should contain two colons") < 0)
		goto out1;
	ret = 0;
out1:
	memset(sig, 0, sizeof sig);
	free(cell);
	freekey(&k);
out0:
	free(priv);
	return ret;
}

static int
test_public_only(void)
{
	Key signer, verifier;
	char *priv, *pubattr, *pub;
	uchar sig[MonoSigLen];
	int ret;

	ret = -1;
	priv = smprint("!seed=%s", seedhex);
	if(priv == nil)
		return -1;
	if(mkkey(&signer, "proto=monocypher user=alice", priv) < 0)
		goto out0;
	if(monocheckkey(&signer) < 0)
		goto out1;
	if(monosign(&signer, (uchar*)msg, strlen(msg), sig) < 0)
		goto out1;

	pub = strfindattr(signer.attr, "pub");
	pubattr = smprint("proto=monocypher user=alice pub=%q", pub);
	if(pubattr == nil)
		goto out1;
	if(mkkey(&verifier, pubattr, "") < 0){
		free(pubattr);
		goto out1;
	}
	free(pubattr);
	if(monocheckkey(&verifier) < 0)
		goto out2;
	if(require(monosign(&verifier, (uchar*)msg, strlen(msg), sig) < 0,
	    "public-only key signed") < 0)
		goto out2;
	if(require(monoverify(&verifier, (uchar*)msg, strlen(msg), sig) == 0,
	    "public-only key did not verify") < 0)
		goto out2;
	ret = 0;
out2:
	freekey(&verifier);
out1:
	memset(sig, 0, sizeof sig);
	freekey(&signer);
out0:
	free(priv);
	return ret;
}

static int
test_bad_public_mismatch(void)
{
	Key k;
	char *attr, *priv;
	int ret;

	ret = -1;
	attr = smprint("proto=monocypher user=alice pub=%s", badpub);
	priv = smprint("!seed=%s", seedhex);
	if(attr == nil || priv == nil)
		goto out;
	if(mkkey(&k, attr, priv) < 0)
		goto out;
	if(require(monocheckkey(&k) < 0,
	    "mismatched public key was accepted") < 0)
		goto out1;
	ret = 0;
out1:
	freekey(&k);
out:
	free(attr);
	free(priv);
	return ret;
}

void
threadmain(int, char**)
{
	setupfmt();
	if(test_seed_key() < 0
	|| test_signed_cell() < 0
	|| test_public_only() < 0
	|| test_bad_public_mismatch() < 0)
		threadexitsall("monokey");
	print("monokey unit tests ok\n");
	threadexitsall(nil);
}
