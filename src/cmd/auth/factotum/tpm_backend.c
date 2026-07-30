#include "std.h"
#include "dat.h"
#include "tpm_backend.h"

static char b64e[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	"abcdefghijklmnopqrstuvwxyz"
	"0123456789-_";

static TpmBackend *backend;
static char backenderr[ERRMAX];

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

char*
tpmb64(uchar *in, int n)
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

int
tpmunb64(char *s, uchar **out, int *nout)
{
	uchar *buf;
	uint quad;
	int i, n, o, pad, v0, v1, v2, v3, want;

	if(s == nil){
		werrstr("missing base64 data");
		return -1;
	}
	n = strlen(s);
	if((n & 3) != 0){
		werrstr("base64 length is not a multiple of four");
		return -1;
	}
	pad = 0;
	if(n > 0 && s[n-1] == '=')
		pad++;
	if(n > 1 && s[n-2] == '=')
		pad++;
	want = (n/4)*3 - pad;
	if(want < 0){
		werrstr("bad base64 padding");
		return -1;
	}
	buf = emalloc(want == 0 ? 1 : want);
	o = 0;
	for(i = 0; i < n; i += 4){
		v0 = b64val((uchar)s[i]);
		v1 = b64val((uchar)s[i+1]);
		v2 = s[i+2] == '=' ? 0 : b64val((uchar)s[i+2]);
		v3 = s[i+3] == '=' ? 0 : b64val((uchar)s[i+3]);
		if(v0 < 0 || v1 < 0 || (s[i+2] != '=' && v2 < 0)
		|| (s[i+3] != '=' && v3 < 0)){
			free(buf);
			werrstr("bad base64 character");
			return -1;
		}
		if(s[i+2] == '=' && s[i+3] != '='){
			free(buf);
			werrstr("bad base64 padding");
			return -1;
		}
		if((s[i+2] == '=' || s[i+3] == '=') && i+4 != n){
			free(buf);
			werrstr("base64 padding before final quantum");
			return -1;
		}
		quad = (v0 << 18) | (v1 << 12) | (v2 << 6) | v3;
		if(o < want)
			buf[o++] = (quad >> 16) & 0xff;
		if(o < want)
			buf[o++] = (quad >> 8) & 0xff;
		if(o < want)
			buf[o++] = quad & 0xff;
	}
	*out = buf;
	*nout = want;
	return 0;
}

static TpmBackend*
tpmbackend(void)
{
	char *name;

	if(backend != nil)
		return backend;
	if(backenderr[0] != 0){
		werrstr("%s", backenderr);
		return nil;
	}
	name = getenv("FACTOTUM_TPM_BACKEND");
	if(name == nil || name[0] == '\0' || strcmp(name, "tpm2") == 0)
		backend = &tpmbackendtss;
	else if(strcmp(name, "mock") == 0)
		backend = &tpmbackendmock;
	else{
		snprint(backenderr, sizeof backenderr, "unknown TPM backend %s", name);
		werrstr("%s", backenderr);
		return nil;
	}
	return backend;
}

char*
tpmbackendname(void)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return "unknown";
	return b->name;
}

int
tpmprobe(char *buf, int nbuf)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->probe)(buf, nbuf);
}

int
tpmrandom(uchar *buf, int nbuf)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->random)(buf, nbuf);
}

int
tpmgenkey(TpmKeyInfo *k, uchar **pub, int *npub)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->genkey)(k, pub, npub);
}

int
tpmkeypublic(TpmKeyInfo *k, uchar **pub, int *npub)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->keypublic)(k, pub, npub);
}

int
tpmkeysign(TpmKeyInfo *k, uchar *digest, int ndigest, uchar **sig, int *nsig)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->keysign)(k, digest, ndigest, sig, nsig);
}

int
tpmdestroykey(TpmKeyInfo *k)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->destroykey)(k);
}

int
tpmsealblob(char *policy, uchar *in, int nin, char **blob)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->seal)(policy, in, nin, blob);
}

int
tpmunsealblob(char *blob, uchar **out, int *nout)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->unseal)(blob, out, nout);
}

int
tpmquote(char *pcrs, uchar *nonce, int nnonce, char **quote)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->quote)(pcrs, nonce, nnonce, quote);
}

int
tpmbackendinitvault(char *handle, char **blob)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->initvault)(handle, blob);
}

int
tpmbackendunlockvault(char *blob)
{
	TpmBackend *b;

	b = tpmbackend();
	if(b == nil)
		return -1;
	return (*b->unlockvault)(blob);
}
