#include "std.h"
#include "dat.h"

int
tpmkeyinfoattrs(Attr *a, TpmKeyInfo *i)
{
	memset(i, 0, sizeof *i);
	i->name = strfindattr(a, "name");
	i->handle = strfindattr(a, "handle");
	i->alg = strfindattr(a, "alg");
	i->usage = strfindattr(a, "usage");
	i->pub = strfindattr(a, "pub");
	return 0;
}

int
tpmkeyalgok(char *alg)
{
	if(alg == nil)
		return 0;
	if(strcmp(alg, "tpm2-ecdsa-p256-sha256") == 0)
		return 1;
	if(strcmp(alg, "tpm2-rsa2048-sha256") == 0)
		return 1;
	return 0;
}

int
tpmkeycanuse(TpmKeyInfo *i, char *want)
{
	char *p;
	int n;

	if(i->usage == nil || want == nil)
		return 0;
	n = strlen(want);
	for(p = i->usage; *p != 0; ){
		while(*p == ',' || *p == ' ' || *p == '\t')
			p++;
		if(strncmp(p, want, n) == 0
		&& (p[n] == 0 || p[n] == ',' || p[n] == ' ' || p[n] == '\t'))
			return 1;
		while(*p != 0 && *p != ',')
			p++;
	}
	return 0;
}

static int
tpmkeycheck(Key *k)
{
	TpmKeyInfo i;
	uchar *pub;
	int npub;

	tpmkeyinfoattrs(k->attr, &i);
	if(i.name == nil || i.name[0] == 0){
		werrstr("missing name");
		return -1;
	}
	if(i.handle == nil || i.handle[0] == 0){
		werrstr("missing handle");
		return -1;
	}
	if(!tpmkeyalgok(i.alg)){
		werrstr("unsupported TPM key alg %s", i.alg == nil ? "" : i.alg);
		return -1;
	}
	if(i.usage == nil || i.usage[0] == 0){
		werrstr("missing usage");
		return -1;
	}
	if(!tpmkeycanuse(&i, "sign")){
		werrstr("TPM key usage must include sign");
		return -1;
	}
	if(i.pub != nil){
		if(tpmunb64(i.pub, &pub, &npub) < 0){
			werrstr("bad TPM public key: %r");
			return -1;
		}
		free(pub);
	}
	return 0;
}

static Key*
fetchtpmkey(Conv *c)
{
	char *handle, *name;

	name = strfindattr(c->attr, "name");
	handle = strfindattr(c->attr, "handle");
	if(name != nil && handle != nil)
		return keyfetch(c, "proto=tpmkey name=%q handle=%q", name, handle);
	if(name != nil)
		return keyfetch(c, "proto=tpmkey name=%q", name);
	if(handle != nil)
		return keyfetch(c, "proto=tpmkey handle=%q", handle);
	werrstr("missing name or handle");
	return nil;
}

static int
xtpmkeypublic(Conv *c)
{
	Key *k;
	TpmKeyInfo i;
	uchar *pub;
	char *pub64;
	int npub, ret;

	ret = -1;
	pub = nil;
	pub64 = nil;
	k = fetchtpmkey(c);
	if(k == nil)
		return -1;
	tpmkeyinfoattrs(k->attr, &i);
	if(i.pub != nil)
		pub64 = estrdup(i.pub);
	else{
		if(tpmkeypublic(&i, &pub, &npub) < 0)
			goto out;
		pub64 = tpmb64(pub, npub);
	}
	c->state = "write TPM public key";
	ret = convprint(c, "alg=%q pub=%s", i.alg, pub64);

out:
	free(pub);
	free(pub64);
	keyclose(k);
	return ret;
}

static int
xtpmkeysign(Conv *c)
{
	Key *k;
	TpmKeyInfo i;
	char *hash, *sig64;
	uchar digest[SHA2_256dlen], *sig;
	int nsig, ret;

	ret = -1;
	sig = nil;
	sig64 = nil;
	hash = strfindattr(c->attr, "hash");
	if(hash == nil)
		hash = "sha256";
	if(strcmp(hash, "sha256") != 0){
		werrstr("unsupported hash %s", hash);
		return -1;
	}
	if(vaultrequire("tpm sign") < 0)
		return -1;
	k = fetchtpmkey(c);
	if(k == nil)
		return -1;
	tpmkeyinfoattrs(k->attr, &i);
	if(!tpmkeycanuse(&i, "sign")){
		werrstr("TPM key is not allowed to sign");
		goto out;
	}
	c->state = "read SHA-256 digest";
	if(convread(c, digest, sizeof digest) < 0)
		goto out;
	if(tpmkeysign(&i, digest, sizeof digest, &sig, &nsig) < 0)
		goto out;
	sig64 = tpmb64(sig, nsig);
	c->state = "write TPM signature";
	ret = convprint(c, "alg=%q hash=sha256 sig=%s", i.alg, sig64);

out:
	memset(digest, 0, sizeof digest);
	if(sig != nil){
		memset(sig, 0, nsig);
		free(sig);
	}
	free(sig64);
	keyclose(k);
	return ret;
}

static Role tpmkeyroles[] =
{
	{"public",	xtpmkeypublic},
	{"sign",	xtpmkeysign},
	{0, 0}
};

Proto tpmkey =
{
	"tpmkey",
	tpmkeyroles,
	nil,
	tpmkeycheck,
	nil
};
