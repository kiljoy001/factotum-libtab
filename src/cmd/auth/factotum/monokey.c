#include "std.h"
#include "dat.h"
#include "monokey.h"
#include <monocypher.h>
#include <monocypher-ed25519.h>

static const char b64e[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-_";

static int
hexval(int c)
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
allhex(char *s)
{
	for(; *s; s++)
		if(hexval((uchar)*s) < 0)
			return 0;
	return 1;
}

static int
b64val(int c)
{
	if('A' <= c && c <= 'Z')
		return c - 'A';
	if('a' <= c && c <= 'z')
		return c - 'a' + 26;
	if('0' <= c && c <= '9')
		return c - '0' + 52;
	if(c == '-' || c == '+')
		return 62;
	if(c == '_' || c == '/')
		return 63;
	return -1;
}

static int
decodeb64(char *s, uchar *out, int want)
{
	int i, n, o, pad, v0, v1, v2, v3;
	uint quad;

	n = strlen(s);
	if((n & 3) != 0)
		return -1;
	pad = 0;
	if(n > 0 && s[n-1] == '=')
		pad++;
	if(n > 1 && s[n-2] == '=')
		pad++;
	if((n/4)*3 - pad != want)
		return -1;

	o = 0;
	for(i = 0; i < n; i += 4){
		v0 = b64val((uchar)s[i]);
		v1 = b64val((uchar)s[i+1]);
		v2 = s[i+2] == '=' ? 0 : b64val((uchar)s[i+2]);
		v3 = s[i+3] == '=' ? 0 : b64val((uchar)s[i+3]);
		if(v0 < 0 || v1 < 0 || (s[i+2] != '=' && v2 < 0)
		|| (s[i+3] != '=' && v3 < 0))
			return -1;
		quad = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
		if(o < want)
			out[o++] = (quad >> 16) & 0xff;
		if(o < want)
			out[o++] = (quad >> 8) & 0xff;
		if(o < want)
			out[o++] = quad & 0xff;
	}
	return o == want ? 0 : -1;
}

static int
decodebytes(char *s, uchar *out, int want, char *name)
{
	int i;

	if(s == nil){
		werrstr("missing %s", name);
		return -1;
	}
	if((int)strlen(s) == want*2 && allhex(s)){
		for(i = 0; i < want; i++)
			out[i] = (hexval((uchar)s[i*2]) << 4) | hexval((uchar)s[i*2+1]);
		return 0;
	}
	if(decodeb64(s, out, want) == 0)
		return 0;
	werrstr("%s must be %d bytes encoded as hex or base64url", name, want);
	return -1;
}

static char*
findpriv(Key *k, char *a, char *b, char *c)
{
	char *s;

	if((s = strfindattr(k->privattr, a)) != nil)
		return s;
	if(b != nil && (s = strfindattr(k->privattr, b)) != nil)
		return s;
	if(c != nil && (s = strfindattr(k->privattr, c)) != nil)
		return s;
	return nil;
}

static char*
findpub(Key *k)
{
	char *s;

	if((s = strfindattr(k->attr, "pub")) != nil)
		return s;
	if((s = strfindattr(k->attr, "public_key")) != nil)
		return s;
	return strfindattr(k->attr, "public");
}

int
monoalg(char *alg)
{
	if(alg == nil || strcmp(alg, MonoAlg) == 0)
		return 1;
	if(strcmp(alg, MonoAlgShort) == 0)
		return 1;
	if(strcmp(alg, "monocypher-eddsa-blake2b-v1") == 0)
		return 1;
	return 0;
}

char*
monob64(uchar *in, int n)
{
	char *out;
	uint triple;
	int i, o, olen;

	if(n < 0)
		return nil;
	olen = 4*((n + 2)/3);
	out = emalloc(olen + 1);
	o = 0;
	for(i = 0; i + 2 < n; i += 3){
		triple = (in[i] << 16) | (in[i+1] << 8) | in[i+2];
		out[o++] = b64e[(triple >> 18) & 0x3f];
		out[o++] = b64e[(triple >> 12) & 0x3f];
		out[o++] = b64e[(triple >> 6) & 0x3f];
		out[o++] = b64e[triple & 0x3f];
	}
	if(i < n){
		triple = in[i] << 16;
		if(i + 1 < n)
			triple |= in[i+1] << 8;
		out[o++] = b64e[(triple >> 18) & 0x3f];
		out[o++] = b64e[(triple >> 12) & 0x3f];
		out[o++] = i + 1 < n ? b64e[(triple >> 6) & 0x3f] : '=';
		out[o++] = '=';
	}
	out[o] = 0;
	return out;
}

static void
derive(uchar sk[MonoKeyLen], uchar pk[MonoPubLen], uchar seed[MonoSeedLen], int ed25519)
{
	uchar s[MonoSeedLen];

	memmove(s, seed, sizeof s);
	if(ed25519)
		crypto_ed25519_key_pair(sk, pk, s);
	else
		crypto_eddsa_key_pair(sk, pk, s);
	crypto_wipe(s, sizeof s);
}

static int
checkkey(Key *k, int ed25519)
{
	char *alg, *p;
	uchar seed[MonoSeedLen], pub[MonoPubLen], checksk[MonoKeyLen], checkpk[MonoPubLen];
	MonoKey *mk;

	alg = strfindattr(k->attr, "alg");
	if(ed25519 ? (alg != nil && strcmp(alg, "ed25519") != 0) : !monoalg(alg)){
		werrstr("unsupported signing alg %s", alg);
		return -1;
	}
	if(alg == nil)
		k->attr = addattr(k->attr, "alg=%q", ed25519 ? "ed25519" : MonoAlg);

	mk = mallocz(sizeof *mk, 1);
	if(mk == nil){
		werrstr("out of memory");
		return -1;
	}

	mk->ed25519 = ed25519;
	if((p = findpriv(k, "!seed", nil, nil)) != nil){
		if(decodebytes(p, seed, sizeof seed, "!seed") < 0)
			goto Error;
		derive(mk->sk, mk->pk, seed, ed25519);
		crypto_wipe(seed, sizeof seed);
		mk->havesk = 1;
	}else if((p = findpriv(k, "!sk", "!secret", "!private")) != nil){
		if(decodebytes(p, mk->sk, sizeof mk->sk, "!sk") < 0)
			goto Error;
		memmove(mk->pk, mk->sk + MonoSeedLen, sizeof mk->pk);
		memmove(seed, mk->sk, sizeof seed);
		derive(checksk, checkpk, seed, ed25519);
		crypto_wipe(seed, sizeof seed);
		crypto_wipe(checksk, sizeof checksk);
		if(tsmemcmp(checkpk, mk->pk, sizeof mk->pk) != 0){
			werrstr("!sk public half does not match seed half");
			crypto_wipe(checkpk, sizeof checkpk);
			goto Error;
		}
		crypto_wipe(checkpk, sizeof checkpk);
		mk->havesk = 1;
	}

	if((p = findpub(k)) != nil){
		if(decodebytes(p, pub, sizeof pub, "pub") < 0)
			goto Error;
		if(mk->havesk && tsmemcmp(pub, mk->pk, sizeof pub) != 0){
			werrstr("public key does not match private key");
			goto Error;
		}
		memmove(mk->pk, pub, sizeof mk->pk);
	}else if(mk->havesk){
		p = monob64(mk->pk, sizeof mk->pk);
		k->attr = addattr(k->attr, "pub=%q", p);
		free(p);
	}else{
		werrstr("missing public key or private key");
		goto Error;
	}

	k->priv = mk;
	return 0;

Error:
	crypto_wipe(seed, sizeof seed);
	crypto_wipe(pub, sizeof pub);
	crypto_wipe(mk, sizeof *mk);
	free(mk);
	return -1;
}

int
monocheckkey(Key *k)
{
	return checkkey(k, 0);
}

int
ed25519checkkey(Key *k)
{
	return checkkey(k, 1);
}

void
monoclosekey(Key *k)
{
	if(k->priv != nil){
		crypto_wipe(k->priv, sizeof(MonoKey));
		free(k->priv);
		k->priv = nil;
	}
}

int
monosign(Key *k, uchar *msg, int n, uchar sig[MonoSigLen])
{
	MonoKey *mk;

	if(vaultrequire("monocypher sign") < 0)
		return -1;
	mk = k->priv;
	if(mk == nil || !mk->havesk){
		werrstr("missing private key -- cannot sign");
		return -1;
	}
	if(n < 0){
		werrstr("invalid message length");
		return -1;
	}
	if(mk->ed25519)
		crypto_ed25519_sign(sig, mk->sk, msg, n);
	else
		crypto_eddsa_sign(sig, mk->sk, msg, n);
	return 0;
}

int
monoverify(Key *k, uchar *msg, int n, uchar sig[MonoSigLen])
{
	MonoKey *mk;

	mk = k->priv;
	if(mk == nil){
		werrstr("missing public key");
		return -1;
	}
	if(n < 0)
		return -1;
	if(mk->ed25519)
		return crypto_ed25519_check(sig, mk->pk, msg, n);
	return crypto_eddsa_check(sig, mk->pk, msg, n);
}

char*
monosignedcell(uchar *body, int n, uchar sig[MonoSigLen])
{
	char *bb, *sb, *cell;

	bb = monob64(body, n);
	sb = monob64(sig, MonoSigLen);
	cell = smprint("signed:%s:%s", bb, sb);
	free(bb);
	free(sb);
	if(cell == nil)
		werrstr("out of memory");
	return cell;
}
