#include "std.h"
#include "dat.h"
#include "tpm_backend.h"

#ifdef HAVE_TSS2
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tss2/tss2_esys.h>
#include <tss2/tss2_mu.h>
#include <tss2/tss2_rc.h>
#include <tss2/tss2_tctildr.h>

enum
{
	MarshalMax = 8192,
	PcrSelectSha256Bytes = 3,
	TssTimeoutMs = 60000
};

typedef struct TssCtx TssCtx;
struct TssCtx
{
	ESYS_CONTEXT *esys;
	TSS2_TCTI_CONTEXT *tcti;
};

static char*
tssconf(void)
{
	char *s;

	s = getenv("FACTOTUM_TPM_TCTI");
	if(s == nil || s[0] == 0){
		free(s);
		return estrdup("swtpm:host=127.0.0.1,port=2321");
	}
	return s;
}

static int
tpmrcis(TSS2_RC rc, TPM2_RC want)
{
	return (rc & 0xffff) == want;
}

static int
tsserr(TSS2_RC rc, char *op)
{
	char *s;

	s = (char*)Tss2_RC_Decode(rc);
	werrstr("%s: tss2 rc 0x%ux (%s)", op, (uint)rc, s == nil ? "unknown" : s);
	return -1;
}

static void
tssclose(TssCtx *tc)
{
	if(tc->esys != nil)
		Esys_Finalize(&tc->esys);
	if(tc->tcti != nil)
		Tss2_TctiLdr_Finalize(&tc->tcti);
	memset(tc, 0, sizeof *tc);
}

static int
tssopen(TssCtx *tc)
{
	TSS2_RC rc;
	TPM2B_AUTH empty;
	char *conf;

	memset(tc, 0, sizeof *tc);
	memset(&empty, 0, sizeof empty);
	conf = tssconf();
	rc = Tss2_TctiLdr_Initialize(conf, &tc->tcti);
	if(rc != TSS2_RC_SUCCESS){
		werrstr("open TPM TCTI %q: tss2 rc 0x%ux (%s)",
		    conf, (uint)rc, Tss2_RC_Decode(rc));
		free(conf);
		return -1;
	}
	free(conf);
	rc = Esys_Initialize(&tc->esys, tc->tcti, nil);
	if(rc != TSS2_RC_SUCCESS){
		tssclose(tc);
		return tsserr(rc, "initialize TPM ESAPI");
	}
	rc = Esys_SetTimeout(tc->esys, TssTimeoutMs);
	if(rc != TSS2_RC_SUCCESS){
		tssclose(tc);
		return tsserr(rc, "set TPM timeout");
	}
	rc = Esys_TR_SetAuth(tc->esys, ESYS_TR_RH_OWNER, &empty);
	if(rc != TSS2_RC_SUCCESS){
		tssclose(tc);
		return tsserr(rc, "set owner auth");
	}
	rc = Esys_Startup(tc->esys, TPM2_SU_CLEAR);
	if(rc != TSS2_RC_SUCCESS && !tpmrcis(rc, TPM2_RC_INITIALIZE)){
		tssclose(tc);
		return tsserr(rc, "startup TPM");
	}
	return 0;
}

static int
parsehandle(char *s, TPM2_HANDLE *h)
{
	char *p;
	ulong v;

	if(s == nil || s[0] == 0){
		werrstr("missing TPM persistent handle");
		return -1;
	}
	errno = 0;
	v = strtoul(s, &p, 0);
	if(errno != 0 || *p != 0){
		werrstr("bad TPM persistent handle %q", s);
		return -1;
	}
	if(v < TPM2_HR_PERSISTENT || v > TPM2_HR_PERSISTENT + 0x00ffffff){
		werrstr("TPM handle %q is not in the persistent range", s);
		return -1;
	}
	*h = (TPM2_HANDLE)v;
	return 0;
}

static int
copyout(uchar *buf, size_t n, uchar **out, int *nout)
{
	if(n > 0x7fffffff){
		werrstr("TPM object is too large");
		return -1;
	}
	*out = emalloc(n == 0 ? 1 : n);
	memmove(*out, buf, n);
	*nout = (int)n;
	return 0;
}

static int
marshalpub(TPM2B_PUBLIC *src, uchar **out, int *nout)
{
	TSS2_RC rc;
	size_t off;
	uchar buf[MarshalMax];

	off = 0;
	rc = Tss2_MU_TPM2B_PUBLIC_Marshal(src, buf, sizeof buf, &off);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "marshal TPM public key");
	return copyout(buf, off, out, nout);
}

static int
marshalpriv(TPM2B_PRIVATE *src, uchar **out, int *nout)
{
	TSS2_RC rc;
	size_t off;
	uchar buf[MarshalMax];

	off = 0;
	rc = Tss2_MU_TPM2B_PRIVATE_Marshal(src, buf, sizeof buf, &off);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "marshal TPM private blob");
	return copyout(buf, off, out, nout);
}

static int
marshalsig(TPMT_SIGNATURE *src, uchar **out, int *nout)
{
	TSS2_RC rc;
	size_t off;
	uchar buf[MarshalMax];

	off = 0;
	rc = Tss2_MU_TPMT_SIGNATURE_Marshal(src, buf, sizeof buf, &off);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "marshal TPM signature");
	return copyout(buf, off, out, nout);
}

static int
marshalattest(TPM2B_ATTEST *src, uchar **out, int *nout)
{
	TSS2_RC rc;
	size_t off;
	uchar buf[MarshalMax];

	off = 0;
	rc = Tss2_MU_TPM2B_ATTEST_Marshal(src, buf, sizeof buf, &off);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "marshal TPM attest");
	return copyout(buf, off, out, nout);
}

static int
unmarshalpub(uchar *buf, int nbuf, TPM2B_PUBLIC *pub)
{
	TSS2_RC rc;
	size_t off;

	memset(pub, 0, sizeof *pub);
	off = 0;
	rc = Tss2_MU_TPM2B_PUBLIC_Unmarshal(buf, nbuf, &off, pub);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "unmarshal TPM public blob");
	if(off != (size_t)nbuf){
		werrstr("trailing data in TPM public blob");
		return -1;
	}
	return 0;
}

static int
unmarshalpriv(uchar *buf, int nbuf, TPM2B_PRIVATE *priv)
{
	TSS2_RC rc;
	size_t off;

	memset(priv, 0, sizeof *priv);
	off = 0;
	rc = Tss2_MU_TPM2B_PRIVATE_Unmarshal(buf, nbuf, &off, priv);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "unmarshal TPM private blob");
	if(off != (size_t)nbuf){
		werrstr("trailing data in TPM private blob");
		return -1;
	}
	return 0;
}

static void
freeesys(void *p)
{
	if(p != nil)
		Esys_Free(p);
}

static void
flushtr(TssCtx *tc, ESYS_TR *h)
{
	if(*h == ESYS_TR_NONE)
		return;
	Esys_FlushContext(tc->esys, *h);
	*h = ESYS_TR_NONE;
}

static void
closetr(TssCtx *tc, ESYS_TR *h)
{
	if(*h == ESYS_TR_NONE)
		return;
	Esys_TR_Close(tc->esys, h);
	*h = ESYS_TR_NONE;
}

static void
emptydata(TPM2B_DATA *d)
{
	memset(d, 0, sizeof *d);
}

static void
emptypcr(TPML_PCR_SELECTION *p)
{
	memset(p, 0, sizeof *p);
}

static void
emptysensitive(TPM2B_SENSITIVE_CREATE *s)
{
	memset(s, 0, sizeof *s);
	s->sensitive.userAuth.size = 0;
	s->sensitive.data.size = 0;
}

static void
primarytemplate(TPM2B_PUBLIC *pub)
{
	memset(pub, 0, sizeof *pub);
	pub->publicArea.type = TPM2_ALG_RSA;
	pub->publicArea.nameAlg = TPM2_ALG_SHA256;
	pub->publicArea.objectAttributes =
	    TPMA_OBJECT_FIXEDTPM |
	    TPMA_OBJECT_FIXEDPARENT |
	    TPMA_OBJECT_SENSITIVEDATAORIGIN |
	    TPMA_OBJECT_USERWITHAUTH |
	    TPMA_OBJECT_NODA |
	    TPMA_OBJECT_RESTRICTED |
	    TPMA_OBJECT_DECRYPT;
	pub->publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_AES;
	pub->publicArea.parameters.rsaDetail.symmetric.keyBits.aes = 128;
	pub->publicArea.parameters.rsaDetail.symmetric.mode.aes = TPM2_ALG_CFB;
	pub->publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_NULL;
	pub->publicArea.parameters.rsaDetail.keyBits = 2048;
	pub->publicArea.parameters.rsaDetail.exponent = 0;
	pub->publicArea.unique.rsa.size = 0;
}

static int
createprimary(TssCtx *tc, ESYS_TR *primary)
{
	TSS2_RC rc;
	TPM2B_SENSITIVE_CREATE sensitive;
	TPM2B_PUBLIC inpublic;
	TPM2B_PUBLIC *outpublic;
	TPM2B_CREATION_DATA *creation;
	TPM2B_DIGEST *creationhash;
	TPMT_TK_CREATION *ticket;
	TPM2B_DATA outside;
	TPML_PCR_SELECTION pcr;

	outpublic = nil;
	creation = nil;
	creationhash = nil;
	ticket = nil;
	emptysensitive(&sensitive);
	primarytemplate(&inpublic);
	emptydata(&outside);
	emptypcr(&pcr);
	rc = Esys_CreatePrimary(tc->esys, ESYS_TR_RH_OWNER,
	    ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE,
	    &sensitive, &inpublic, &outside, &pcr, primary,
	    &outpublic, &creation, &creationhash, &ticket);
	freeesys(outpublic);
	freeesys(creation);
	freeesys(creationhash);
	freeesys(ticket);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "create TPM primary object");
	return 0;
}

static int
algisecc(char *alg)
{
	return alg != nil && strcmp(alg, "tpm2-ecdsa-p256-sha256") == 0;
}

static int
algisrsa(char *alg)
{
	return alg != nil && strcmp(alg, "tpm2-rsa2048-sha256") == 0;
}

static int
sigscheme(char *alg, TPMT_SIG_SCHEME *scheme)
{
	memset(scheme, 0, sizeof *scheme);
	if(algisecc(alg)){
		scheme->scheme = TPM2_ALG_ECDSA;
		scheme->details.ecdsa.hashAlg = TPM2_ALG_SHA256;
		return 0;
	}
	if(algisrsa(alg)){
		scheme->scheme = TPM2_ALG_RSASSA;
		scheme->details.rsassa.hashAlg = TPM2_ALG_SHA256;
		return 0;
	}
	werrstr("unsupported native TPM signing alg %s", alg == nil ? "" : alg);
	return -1;
}

static int
sigschemefrompub(TPM2B_PUBLIC *pub, TPMT_SIG_SCHEME *scheme)
{
	memset(scheme, 0, sizeof *scheme);
	switch(pub->publicArea.type){
	case TPM2_ALG_ECC:
		scheme->scheme = TPM2_ALG_ECDSA;
		scheme->details.ecdsa.hashAlg = TPM2_ALG_SHA256;
		return 0;
	case TPM2_ALG_RSA:
		scheme->scheme = TPM2_ALG_RSASSA;
		scheme->details.rsassa.hashAlg = TPM2_ALG_SHA256;
		return 0;
	default:
		werrstr("TPM quote key is not RSA or ECC");
		return -1;
	}
}

static int
signtemplate(char *alg, TPM2B_PUBLIC *pub)
{
	memset(pub, 0, sizeof *pub);
	pub->publicArea.nameAlg = TPM2_ALG_SHA256;
	pub->publicArea.objectAttributes =
	    TPMA_OBJECT_FIXEDTPM |
	    TPMA_OBJECT_FIXEDPARENT |
	    TPMA_OBJECT_SENSITIVEDATAORIGIN |
	    TPMA_OBJECT_USERWITHAUTH |
	    TPMA_OBJECT_NODA |
	    TPMA_OBJECT_SIGN_ENCRYPT;
	if(algisecc(alg)){
		pub->publicArea.type = TPM2_ALG_ECC;
		pub->publicArea.parameters.eccDetail.symmetric.algorithm = TPM2_ALG_NULL;
		pub->publicArea.parameters.eccDetail.scheme.scheme = TPM2_ALG_ECDSA;
		pub->publicArea.parameters.eccDetail.scheme.details.ecdsa.hashAlg =
		    TPM2_ALG_SHA256;
		pub->publicArea.parameters.eccDetail.curveID = TPM2_ECC_NIST_P256;
		pub->publicArea.parameters.eccDetail.kdf.scheme = TPM2_ALG_NULL;
		pub->publicArea.unique.ecc.x.size = 0;
		pub->publicArea.unique.ecc.y.size = 0;
		return 0;
	}
	if(algisrsa(alg)){
		pub->publicArea.type = TPM2_ALG_RSA;
		pub->publicArea.parameters.rsaDetail.symmetric.algorithm = TPM2_ALG_NULL;
		pub->publicArea.parameters.rsaDetail.scheme.scheme = TPM2_ALG_RSASSA;
		pub->publicArea.parameters.rsaDetail.scheme.details.rsassa.hashAlg =
		    TPM2_ALG_SHA256;
		pub->publicArea.parameters.rsaDetail.keyBits = 2048;
		pub->publicArea.parameters.rsaDetail.exponent = 0;
		pub->publicArea.unique.rsa.size = 0;
		return 0;
	}
	werrstr("unsupported native TPM key alg %s", alg == nil ? "" : alg);
	return -1;
}

static void
sealtemplate(TPM2B_PUBLIC *pub)
{
	memset(pub, 0, sizeof *pub);
	pub->publicArea.type = TPM2_ALG_KEYEDHASH;
	pub->publicArea.nameAlg = TPM2_ALG_SHA256;
	pub->publicArea.objectAttributes =
	    TPMA_OBJECT_FIXEDTPM |
	    TPMA_OBJECT_FIXEDPARENT |
	    TPMA_OBJECT_USERWITHAUTH |
	    TPMA_OBJECT_NODA;
	pub->publicArea.parameters.keyedHashDetail.scheme.scheme = TPM2_ALG_NULL;
	pub->publicArea.unique.keyedHash.size = 0;
}

static int
loadpersistent(TssCtx *tc, TPM2_HANDLE handle, ESYS_TR *object)
{
	TSS2_RC rc;
	TPM2B_AUTH empty;

	memset(&empty, 0, sizeof empty);
	rc = Esys_TR_FromTPMPublic(tc->esys, handle,
	    ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE, object);
	if(rc != TSS2_RC_SUCCESS)
		return tsserr(rc, "load TPM persistent handle");
	rc = Esys_TR_SetAuth(tc->esys, *object, &empty);
	if(rc != TSS2_RC_SUCCESS){
		closetr(tc, object);
		return tsserr(rc, "set TPM object auth");
	}
	return 0;
}

static int
tssprobe(char *buf, int nbuf)
{
	TssCtx tc;
	TPM2B_DIGEST *rnd;
	TSS2_RC rc;
	char err[ERRMAX], *conf;
	int ret;

	rnd = nil;
	ret = -1;
	conf = tssconf();
	if(tssopen(&tc) < 0){
		rerrstr(err, sizeof err);
		snprint(buf, nbuf,
		    "backend=tpm2 status=unavailable tcti=%q err=%q\n",
		    conf, err);
		goto out;
	}
	rc = Esys_GetRandom(tc.esys, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
	    1, &rnd);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "read TPM random byte");
		rerrstr(err, sizeof err);
		snprint(buf, nbuf,
		    "backend=tpm2 status=unavailable tcti=%q err=%q\n",
		    conf, err);
		goto outclose;
	}
	snprint(buf, nbuf,
	    "backend=tpm2 status=ok tcti=%q maxseal=%d\n"
	    "alg=tpm2-ecdsa-p256-sha256 usage=sign\n"
	    "alg=tpm2-rsa2048-sha256 usage=sign\n"
	    "seal=unbound policy=none\n"
	    "quote=requires FACTOTUM_TPM_QUOTE_HANDLE\n",
	    conf, TPM2_MAX_SYM_DATA);
	ret = 0;

outclose:
	freeesys(rnd);
	tssclose(&tc);
out:
	free(conf);
	return ret;
}

static int
tssrandom(uchar *buf, int nbuf)
{
	TssCtx tc;
	TPM2B_DIGEST *rnd;
	TSS2_RC rc;
	int n, off, ret;

	if(nbuf < 0){
		werrstr("negative random byte count");
		return -1;
	}
	if(tssopen(&tc) < 0)
		return -1;
	ret = -1;
	off = 0;
	while(off < nbuf){
		n = nbuf - off;
		if(n > (int)sizeof rnd->buffer)
			n = sizeof rnd->buffer;
		rnd = nil;
		rc = Esys_GetRandom(tc.esys, ESYS_TR_NONE, ESYS_TR_NONE,
		    ESYS_TR_NONE, n, &rnd);
		if(rc != TSS2_RC_SUCCESS){
			tsserr(rc, "read TPM random bytes");
			goto out;
		}
		if(rnd->size == 0){
			freeesys(rnd);
			werrstr("TPM returned zero random bytes");
			goto out;
		}
		memmove(buf + off, rnd->buffer, rnd->size);
		off += rnd->size;
		freeesys(rnd);
	}
	ret = 0;

out:
	tssclose(&tc);
	return ret;
}

static int
tssgenkey(TpmKeyInfo *k, uchar **pub, int *npub)
{
	TssCtx tc;
	TPM2_HANDLE handle;
	ESYS_TR primary, child, persistent;
	TSS2_RC rc;
	TPM2B_SENSITIVE_CREATE sensitive;
	TPM2B_PUBLIC inpublic;
	TPM2B_PRIVATE *outprivate;
	TPM2B_PUBLIC *outpublic;
	TPM2B_CREATION_DATA *creation;
	TPM2B_DIGEST *creationhash;
	TPMT_TK_CREATION *ticket;
	TPM2B_DATA outside;
	TPML_PCR_SELECTION pcr;
	int ret;

	primary = ESYS_TR_NONE;
	child = ESYS_TR_NONE;
	persistent = ESYS_TR_NONE;
	outprivate = nil;
	outpublic = nil;
	creation = nil;
	creationhash = nil;
	ticket = nil;
	if(parsehandle(k->handle, &handle) < 0)
		return -1;
	if(signtemplate(k->alg, &inpublic) < 0)
		return -1;
	if(tssopen(&tc) < 0)
		return -1;
	ret = -1;
	if(createprimary(&tc, &primary) < 0)
		goto out;
	emptysensitive(&sensitive);
	emptydata(&outside);
	emptypcr(&pcr);
	rc = Esys_Create(tc.esys, primary, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, &sensitive, &inpublic,
	    &outside, &pcr, &outprivate, &outpublic, &creation,
	    &creationhash, &ticket);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "create TPM signing key");
		goto out;
	}
	rc = Esys_Load(tc.esys, primary, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, outprivate, outpublic, &child);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "load TPM signing key");
		goto out;
	}
	rc = Esys_EvictControl(tc.esys, ESYS_TR_RH_OWNER, child,
	    ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, handle, &persistent);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "make TPM signing key persistent");
		goto out;
	}
	if(marshalpub(outpublic, pub, npub) < 0)
		goto out;
	ret = 0;

out:
	closetr(&tc, &persistent);
	flushtr(&tc, &child);
	flushtr(&tc, &primary);
	freeesys(outprivate);
	freeesys(outpublic);
	freeesys(creation);
	freeesys(creationhash);
	freeesys(ticket);
	tssclose(&tc);
	return ret;
}

static int
tsspub(TpmKeyInfo *k, uchar **pub, int *npub)
{
	TssCtx tc;
	TPM2_HANDLE handle;
	ESYS_TR object;
	TSS2_RC rc;
	TPM2B_PUBLIC *outpublic;
	TPM2B_NAME *name, *qname;
	int ret;

	object = ESYS_TR_NONE;
	outpublic = nil;
	name = nil;
	qname = nil;
	if(parsehandle(k->handle, &handle) < 0)
		return -1;
	if(tssopen(&tc) < 0)
		return -1;
	ret = -1;
	if(loadpersistent(&tc, handle, &object) < 0)
		goto out;
	rc = Esys_ReadPublic(tc.esys, object, ESYS_TR_NONE,
	    ESYS_TR_NONE, ESYS_TR_NONE, &outpublic, &name, &qname);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "read TPM public key");
		goto out;
	}
	ret = marshalpub(outpublic, pub, npub);

out:
	closetr(&tc, &object);
	freeesys(outpublic);
	freeesys(name);
	freeesys(qname);
	tssclose(&tc);
	return ret;
}

static int
tsssign(TpmKeyInfo *k, uchar *digest, int ndigest, uchar **sig, int *nsig)
{
	TssCtx tc;
	TPM2_HANDLE handle;
	ESYS_TR object;
	TSS2_RC rc;
	TPM2B_DIGEST d;
	TPMT_SIG_SCHEME scheme;
	TPMT_TK_HASHCHECK validation;
	TPMT_SIGNATURE *outsig;
	int ret;

	object = ESYS_TR_NONE;
	outsig = nil;
	if(ndigest != SHA2_256dlen){
		werrstr("TPM signing currently requires a SHA-256 digest");
		return -1;
	}
	if(parsehandle(k->handle, &handle) < 0)
		return -1;
	if(sigscheme(k->alg, &scheme) < 0)
		return -1;
	if(tssopen(&tc) < 0)
		return -1;
	ret = -1;
	if(loadpersistent(&tc, handle, &object) < 0)
		goto out;
	memset(&d, 0, sizeof d);
	d.size = ndigest;
	memmove(d.buffer, digest, ndigest);
	memset(&validation, 0, sizeof validation);
	validation.tag = TPM2_ST_HASHCHECK;
	validation.hierarchy = TPM2_RH_NULL;
	validation.digest.size = 0;
	rc = Esys_Sign(tc.esys, object, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, &d, &scheme, &validation, &outsig);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "sign with TPM key");
		goto out;
	}
	ret = marshalsig(outsig, sig, nsig);

out:
	closetr(&tc, &object);
	freeesys(outsig);
	tssclose(&tc);
	return ret;
}

static int
tssdestroy(TpmKeyInfo *k)
{
	TssCtx tc;
	TPM2_HANDLE handle;
	ESYS_TR object, out;
	TSS2_RC rc;
	int ret;

	object = ESYS_TR_NONE;
	out = ESYS_TR_NONE;
	if(parsehandle(k->handle, &handle) < 0)
		return -1;
	if(tssopen(&tc) < 0)
		return -1;
	ret = -1;
	if(loadpersistent(&tc, handle, &object) < 0)
		goto outclose;
	rc = Esys_EvictControl(tc.esys, ESYS_TR_RH_OWNER, object,
	    ESYS_TR_PASSWORD, ESYS_TR_NONE, ESYS_TR_NONE, handle, &out);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "destroy TPM persistent key");
		goto outclose;
	}
	ret = 0;

outclose:
	closetr(&tc, &out);
	closetr(&tc, &object);
	tssclose(&tc);
	return ret;
}

static int
tssseal(char *policy, uchar *in, int nin, char **blob)
{
	TssCtx tc;
	ESYS_TR primary;
	TSS2_RC rc;
	TPM2B_SENSITIVE_CREATE sensitive;
	TPM2B_PUBLIC inpublic;
	TPM2B_PRIVATE *outprivate;
	TPM2B_PUBLIC *outpublic;
	TPM2B_CREATION_DATA *creation;
	TPM2B_DIGEST *creationhash;
	TPMT_TK_CREATION *ticket;
	TPM2B_DATA outside;
	TPML_PCR_SELECTION pcr;
	uchar *priv, *pub;
	char *priv64, *pub64;
	int npriv, npub, ret;

	primary = ESYS_TR_NONE;
	outprivate = nil;
	outpublic = nil;
	creation = nil;
	creationhash = nil;
	ticket = nil;
	priv = nil;
	pub = nil;
	priv64 = nil;
	pub64 = nil;
	if(policy != nil && policy[0] != 0 && strcmp(policy, "none") != 0){
		werrstr("native TPM seal does not yet implement PCR policy; refusing to seal");
		return -1;
	}
	if(nin < 0 || nin > TPM2_MAX_SYM_DATA){
		werrstr("native TPM seal input must be between 0 and %d bytes", TPM2_MAX_SYM_DATA);
		return -1;
	}
	if(tssopen(&tc) < 0)
		return -1;
	ret = -1;
	if(createprimary(&tc, &primary) < 0)
		goto out;
	emptysensitive(&sensitive);
	sensitive.sensitive.data.size = nin;
	memmove(sensitive.sensitive.data.buffer, in, nin);
	sealtemplate(&inpublic);
	emptydata(&outside);
	emptypcr(&pcr);
	rc = Esys_Create(tc.esys, primary, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, &sensitive, &inpublic,
	    &outside, &pcr, &outprivate, &outpublic, &creation,
	    &creationhash, &ticket);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "create TPM sealed object");
		goto out;
	}
	if(marshalpriv(outprivate, &priv, &npriv) < 0)
		goto out;
	if(marshalpub(outpublic, &pub, &npub) < 0)
		goto out;
	priv64 = tpmb64(priv, npriv);
	pub64 = tpmb64(pub, npub);
	*blob = smprint("tss2seal-v1:%s:%s", priv64, pub64);
	if(*blob == nil){
		werrstr("out of memory");
		goto out;
	}
	ret = 0;

out:
	flushtr(&tc, &primary);
	if(priv != nil){
		memset(priv, 0, npriv);
		free(priv);
	}
	free(pub);
	free(priv64);
	free(pub64);
	freeesys(outprivate);
	freeesys(outpublic);
	freeesys(creation);
	freeesys(creationhash);
	freeesys(ticket);
	tssclose(&tc);
	return ret;
}

static int
splitsealblob(char *blob, char **copy, char **priv64, char **pub64)
{
	char *p, *q, *prefix;

	prefix = "tss2seal-v1:";
	if(blob == nil || strncmp(blob, prefix, strlen(prefix)) != 0){
		werrstr("bad native TPM seal blob");
		return -1;
	}
	*copy = estrdup(blob + strlen(prefix));
	p = strchr(*copy, ':');
	if(p == nil){
		free(*copy);
		*copy = nil;
		werrstr("truncated native TPM seal blob");
		return -1;
	}
	*p++ = 0;
	q = strchr(p, ':');
	if(q != nil){
		free(*copy);
		*copy = nil;
		werrstr("bad native TPM seal blob");
		return -1;
	}
	*priv64 = *copy;
	*pub64 = p;
	return 0;
}

static int
tssunseal(char *blob, uchar **out, int *nout)
{
	TssCtx tc;
	ESYS_TR primary, object;
	TSS2_RC rc;
	TPM2B_PRIVATE priv;
	TPM2B_PUBLIC pub;
	TPM2B_SENSITIVE_DATA *data;
	uchar *privb, *pubb;
	char *copy, *priv64, *pub64;
	int nprivb, npubb, ret;

	primary = ESYS_TR_NONE;
	object = ESYS_TR_NONE;
	data = nil;
	privb = nil;
	pubb = nil;
	copy = nil;
	if(splitsealblob(blob, &copy, &priv64, &pub64) < 0)
		return -1;
	ret = -1;
	if(tpmunb64(priv64, &privb, &nprivb) < 0)
		goto out;
	if(tpmunb64(pub64, &pubb, &npubb) < 0)
		goto out;
	if(unmarshalpriv(privb, nprivb, &priv) < 0)
		goto out;
	if(unmarshalpub(pubb, npubb, &pub) < 0)
		goto out;
	if(tssopen(&tc) < 0)
		goto out;
	if(createprimary(&tc, &primary) < 0)
		goto outclose;
	rc = Esys_Load(tc.esys, primary, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, &priv, &pub, &object);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "load TPM sealed object");
		goto outclose;
	}
	rc = Esys_Unseal(tc.esys, object, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, &data);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "unseal TPM object");
		goto outclose;
	}
	ret = copyout(data->buffer, data->size, out, nout);

outclose:
	flushtr(&tc, &object);
	flushtr(&tc, &primary);
	tssclose(&tc);
out:
	if(privb != nil){
		memset(privb, 0, nprivb);
		free(privb);
	}
	free(pubb);
	free(copy);
	freeesys(data);
	return ret;
}

static int
parsepcrs(char *pcrs, TPML_PCR_SELECTION *sel)
{
	char *p, *q;
	long n;

	memset(sel, 0, sizeof *sel);
	if(pcrs == nil || pcrs[0] == 0)
		return 0;
	sel->count = 1;
	sel->pcrSelections[0].hash = TPM2_ALG_SHA256;
	sel->pcrSelections[0].sizeofSelect = PcrSelectSha256Bytes;
	for(p = pcrs; *p != 0; ){
		while(*p == ',' || *p == ' ' || *p == '\t')
			p++;
		if(*p == 0)
			break;
		errno = 0;
		n = strtol(p, &q, 0);
		if(errno != 0 || q == p || n < 0 || n >= 8*PcrSelectSha256Bytes){
			werrstr("bad PCR selection %q", pcrs);
			return -1;
		}
		sel->pcrSelections[0].pcrSelect[n / 8] |= 1 << (n % 8);
		p = q;
		if(*p != 0 && *p != ',' && *p != ' ' && *p != '\t'){
			werrstr("bad PCR selection %q", pcrs);
			return -1;
		}
	}
	return 0;
}

static int
tssquote(char *pcrs, uchar *nonce, int nnonce, char **quote)
{
	TssCtx tc;
	TPM2_HANDLE handle;
	ESYS_TR object;
	TSS2_RC rc;
	TPM2B_DATA qdata;
	TPML_PCR_SELECTION sel;
	TPMT_SIG_SCHEME scheme;
	TPM2B_ATTEST *attest;
	TPMT_SIGNATURE *sig;
	TPM2B_PUBLIC *pub;
	TPM2B_NAME *name, *qname;
	uchar *attb, *sigb;
	char *att64, *sig64, *hstr;
	int nattb, nsigb, ret;

	object = ESYS_TR_NONE;
	attest = nil;
	sig = nil;
	pub = nil;
	name = nil;
	qname = nil;
	attb = nil;
	sigb = nil;
	att64 = nil;
	sig64 = nil;
	hstr = getenv("FACTOTUM_TPM_QUOTE_HANDLE");
	if(hstr == nil || hstr[0] == 0){
		free(hstr);
		werrstr("FACTOTUM_TPM_QUOTE_HANDLE is required for native TPM quote");
		return -1;
	}
	if(parsehandle(hstr, &handle) < 0)
		goto bad;
	if(nnonce < 0 || nnonce > (int)sizeof qdata.buffer){
		werrstr("TPM quote nonce must be between 0 and %d bytes",
		    (int)sizeof qdata.buffer);
		goto bad;
	}
	if(parsepcrs(pcrs, &sel) < 0)
		goto bad;
	if(tssopen(&tc) < 0)
		goto bad;
	ret = -1;
	if(loadpersistent(&tc, handle, &object) < 0)
		goto out;
	rc = Esys_ReadPublic(tc.esys, object, ESYS_TR_NONE,
	    ESYS_TR_NONE, ESYS_TR_NONE, &pub, &name, &qname);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "read TPM quote key public area");
		goto out;
	}
	if(sigschemefrompub(pub, &scheme) < 0)
		goto out;
	memset(&qdata, 0, sizeof qdata);
	qdata.size = nnonce;
	memmove(qdata.buffer, nonce, nnonce);
	rc = Esys_Quote(tc.esys, object, ESYS_TR_PASSWORD,
	    ESYS_TR_NONE, ESYS_TR_NONE, &qdata, &scheme, &sel, &attest, &sig);
	if(rc != TSS2_RC_SUCCESS){
		tsserr(rc, "quote TPM PCRs");
		goto out;
	}
	if(marshalattest(attest, &attb, &nattb) < 0)
		goto out;
	if(marshalsig(sig, &sigb, &nsigb) < 0)
		goto out;
	att64 = tpmb64(attb, nattb);
	sig64 = tpmb64(sigb, nsigb);
	*quote = smprint("backend=tpm2 pcrs=%q handle=%q attest=%s sig=%s",
	    pcrs == nil ? "" : pcrs, hstr, att64, sig64);
	if(*quote == nil){
		werrstr("out of memory");
		goto out;
	}
	ret = 0;

out:
	closetr(&tc, &object);
	freeesys(attest);
	freeesys(sig);
	freeesys(pub);
	freeesys(name);
	freeesys(qname);
	free(attb);
	free(sigb);
	free(att64);
	free(sig64);
	tssclose(&tc);
	free(hstr);
	return ret;

bad:
	free(hstr);
	return -1;
}

static int
tssinitvault(char *handle, char **blob)
{
	uchar key[32];
	char *seal;
	int ret;

	ret = -1;
	seal = nil;
	if(handle == nil)
		handle = "";
	if(tssrandom(key, sizeof key) < 0)
		goto out;
	if(tssseal("", key, sizeof key, &seal) < 0)
		goto out;
	*blob = smprint("tss2vault-v1:%s:%s", handle, seal);
	if(*blob == nil){
		werrstr("out of memory");
		goto out;
	}
	ret = 0;

out:
	memset(key, 0, sizeof key);
	free(seal);
	return ret;
}

static int
tssunlockvault(char *blob)
{
	uchar *key;
	char *copy, *p, *prefix;
	int nkey, ret;

	key = nil;
	copy = nil;
	prefix = "tss2vault-v1:";
	if(blob == nil || strncmp(blob, prefix, strlen(prefix)) != 0){
		werrstr("bad native TPM vault blob");
		return -1;
	}
	copy = estrdup(blob + strlen(prefix));
	p = strchr(copy, ':');
	if(p == nil){
		werrstr("truncated native TPM vault blob");
		goto bad;
	}
	p++;
	ret = tssunseal(p, &key, &nkey);
	if(key != nil){
		memset(key, 0, nkey);
		free(key);
	}
	free(copy);
	return ret;

bad:
	free(copy);
	return -1;
}

#else

static int
tssmissing(void)
{
	werrstr("native TPM2 TSS backend is not built; install tss2-esys/tss2-tcti development headers and enable ESAPI support");
	return -1;
}

static int
tssprobe(char *buf, int nbuf)
{
	snprint(buf, nbuf,
	    "backend=tpm2 status=unavailable reason='native TPM2 TSS backend is not built'\n");
	return tssmissing();
}

static int
tssrandom(uchar *buf, int nbuf)
{
	USED(buf);
	USED(nbuf);

	return tssmissing();
}

static int
tssgenkey(TpmKeyInfo *k, uchar **pub, int *npub)
{
	USED(k);
	USED(pub);
	USED(npub);

	return tssmissing();
}

static int
tsspub(TpmKeyInfo *k, uchar **pub, int *npub)
{
	USED(k);
	USED(pub);
	USED(npub);

	return tssmissing();
}

static int
tsssign(TpmKeyInfo *k, uchar *digest, int ndigest, uchar **sig, int *nsig)
{
	USED(k);
	USED(digest);
	USED(ndigest);
	USED(sig);
	USED(nsig);

	return tssmissing();
}

static int
tssdestroy(TpmKeyInfo *k)
{
	USED(k);

	return tssmissing();
}

static int
tssseal(char *policy, uchar *in, int nin, char **blob)
{
	USED(policy);
	USED(in);
	USED(nin);
	USED(blob);

	return tssmissing();
}

static int
tssunseal(char *blob, uchar **out, int *nout)
{
	USED(blob);
	USED(out);
	USED(nout);

	return tssmissing();
}

static int
tssquote(char *pcrs, uchar *nonce, int nnonce, char **quote)
{
	USED(pcrs);
	USED(nonce);
	USED(nnonce);
	USED(quote);

	return tssmissing();
}

static int
tssinitvault(char *handle, char **blob)
{
	USED(handle);
	USED(blob);

	return tssmissing();
}

static int
tssunlockvault(char *blob)
{
	USED(blob);

	return tssmissing();
}

#endif

TpmBackend tpmbackendtss =
{
	"tpm2",
	tssprobe,
	tssrandom,
	tssgenkey,
	tsspub,
	tsssign,
	tssdestroy,
	tssseal,
	tssunseal,
	tssquote,
	tssinitvault,
	tssunlockvault
};
