#include "std.h"
#include "dat.h"
#include "tpm_backend.h"

static DigestState*
hashstr(DigestState *s, char *p)
{
	if(p == nil)
		p = "";
	return sha2_256((uchar*)p, strlen(p), nil, s);
}

static int
hashkey(uchar out[SHA2_256dlen], char *tag, TpmKeyInfo *k, uchar *data, int ndata)
{
	DigestState *s;
	uchar sep;

	sep = 0;
	s = sha2_256((uchar*)tag, strlen(tag), nil, nil);
	if(s == nil){
		werrstr("out of memory");
		return -1;
	}
	s = sha2_256(&sep, 1, nil, s);
	s = hashstr(s, k->name);
	s = sha2_256(&sep, 1, nil, s);
	s = hashstr(s, k->handle);
	s = sha2_256(&sep, 1, nil, s);
	s = hashstr(s, k->alg);
	s = sha2_256(&sep, 1, nil, s);
	s = hashstr(s, k->usage);
	s = sha2_256(&sep, 1, nil, s);
	if(data == nil)
		data = (uchar*)"";
	sha2_256(data, ndata, out, s);
	return 0;
}

static int
hashquote(uchar out[SHA2_256dlen], char *pcrs, uchar *nonce, int nnonce)
{
	DigestState *s;
	uchar sep;

	sep = 0;
	if(pcrs == nil)
		pcrs = "";
	s = sha2_256((uchar*)"mock quote", 10, nil, nil);
	if(s == nil){
		werrstr("out of memory");
		return -1;
	}
	s = sha2_256(&sep, 1, nil, s);
	s = sha2_256((uchar*)pcrs, strlen(pcrs), nil, s);
	s = sha2_256(&sep, 1, nil, s);
	if(nonce == nil)
		nonce = (uchar*)"";
	sha2_256(nonce, nnonce, out, s);
	return 0;
}

static int
mockprobe(char *buf, int nbuf)
{
	snprint(buf, nbuf,
	    "backend=mock version=tpm2 tcti=mock\n"
	    "alg=tpm2-ecdsa-p256-sha256 usage=sign\n"
	    "alg=tpm2-rsa2048-sha256 usage=sign,seal\n"
	    "vault=runtime-only persistent-handles=mock\n");
	return 0;
}

static int
mockrandom(uchar *buf, int nbuf)
{
	return memrandom(buf, nbuf);
}

static int
mockpub(TpmKeyInfo *k, uchar **pub, int *npub)
{
	uchar h[SHA2_256dlen];

	if(hashkey(h, "mock public", k, nil, 0) < 0)
		return -1;
	*pub = emalloc(sizeof h);
	memmove(*pub, h, sizeof h);
	*npub = sizeof h;
	return 0;
}

static int
mockgenkey(TpmKeyInfo *k, uchar **pub, int *npub)
{
	return mockpub(k, pub, npub);
}

static int
mocksign(TpmKeyInfo *k, uchar *digest, int ndigest, uchar **sig, int *nsig)
{
	uchar h[SHA2_256dlen];

	*sig = emalloc(2*SHA2_256dlen);
	if(hashkey(h, "mock sig 0", k, digest, ndigest) < 0){
		free(*sig);
		*sig = nil;
		return -1;
	}
	memmove(*sig, h, sizeof h);
	if(hashkey(h, "mock sig 1", k, digest, ndigest) < 0){
		free(*sig);
		*sig = nil;
		return -1;
	}
	memmove(*sig + SHA2_256dlen, h, sizeof h);
	*nsig = 2*SHA2_256dlen;
	return 0;
}

static int
mockdestroy(TpmKeyInfo *k)
{
	USED(k);

	return 0;
}

static int
mockseal(char *policy, uchar *in, int nin, char **blob)
{
	char *b64;

	USED(policy);

	b64 = tpmb64(in, nin);
	*blob = smprint("mock-v1:%s", b64);
	free(b64);
	if(*blob == nil){
		werrstr("out of memory");
		return -1;
	}
	return 0;
}

static int
mockunseal(char *blob, uchar **out, int *nout)
{
	char *p;

	p = "mock-v1:";
	if(blob == nil || strncmp(blob, p, strlen(p)) != 0){
		werrstr("bad mock TPM seal blob");
		return -1;
	}
	return tpmunb64(blob + strlen(p), out, nout);
}

static int
mockquote(char *pcrs, uchar *nonce, int nnonce, char **quote)
{
	char *b64;
	uchar h[SHA2_256dlen];

	if(hashquote(h, pcrs, nonce, nnonce) < 0)
		return -1;
	b64 = tpmb64(h, sizeof h);
	*quote = smprint("backend=mock pcrs=%q quote=%s", pcrs == nil ? "" : pcrs, b64);
	free(b64);
	if(*quote == nil){
		werrstr("out of memory");
		return -1;
	}
	return 0;
}

static int
mockinitvault(char *handle, char **blob)
{
	char *b64;
	uchar key[32];

	if(handle == nil)
		handle = "";
	if(memrandom(key, sizeof key) < 0)
		return -1;
	b64 = tpmb64(key, sizeof key);
	*blob = smprint("mock-vault-v1:%s:%s", handle, b64);
	memset(key, 0, sizeof key);
	free(b64);
	if(*blob == nil){
		werrstr("out of memory");
		return -1;
	}
	return 0;
}

static int
mockunlockvault(char *blob)
{
	char *p;

	p = "mock-vault-v1:";
	if(blob == nil || strncmp(blob, p, strlen(p)) != 0){
		werrstr("bad mock vault blob");
		return -1;
	}
	return 0;
}

TpmBackend tpmbackendmock =
{
	"mock",
	mockprobe,
	mockrandom,
	mockgenkey,
	mockpub,
	mocksign,
	mockdestroy,
	mockseal,
	mockunseal,
	mockquote,
	mockinitvault,
	mockunlockvault
};
