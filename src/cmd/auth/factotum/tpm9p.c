#include "std.h"
#include "dat.h"
#include "monokey.h"

static char proofversion[] = "tpm9p-user-auth-v1";

static int
xtpm9pproof(Conv *c)
{
	char *authdom, *cell, *challenge, *user;
	uchar *body, *p, sig[MonoSigLen];
	int nauthdom, nbody, nchallenge, nversion, nuser, ret;
	Key *k;

	ret = -1;
	body = nil;
	cell = nil;
	challenge = nil;
	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;

	authdom = strfindattr(k->attr, "authdom");
	if(authdom == nil)
		authdom = strfindattr(k->attr, "dom");
	user = strfindattr(k->attr, "user");
	if(authdom == nil || user == nil){
		werrstr("tpm9p key needs authdom and user");
		goto out;
	}

	c->state = "read challenge";
	if((nchallenge = convreadm(c, &challenge)) < 0)
		goto out;
	if(nchallenge < 16){
		werrstr("challenge too short");
		goto out;
	}

	nversion = strlen(proofversion);
	nauthdom = strlen(authdom);
	nuser = strlen(user);
	nbody = nversion + 1 + nauthdom + 1 + nuser + 1 + nchallenge;
	body = emalloc(nbody);
	p = body;
	memmove(p, proofversion, nversion);
	p += nversion;
	*p++ = 0;
	memmove(p, authdom, nauthdom);
	p += nauthdom;
	*p++ = 0;
	memmove(p, user, nuser);
	p += nuser;
	*p++ = 0;
	memmove(p, challenge, nchallenge);

	if(monosign(k, body, nbody, sig) < 0)
		goto out;
	cell = monosignedcell(body, nbody, sig);
	if(cell == nil)
		goto out;

	c->state = "write proof";
	ret = convwrite(c, cell, strlen(cell));

out:
	memset(sig, 0, sizeof sig);
	if(body != nil){
		memset(body, 0, nbody);
		free(body);
	}
	free(cell);
	free(challenge);
	keyclose(k);
	return ret;
}

static int
xtpm9ppub(Conv *c)
{
	MonoKey *mk;
	Key *k;
	int ret;

	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;
	mk = k->priv;
	c->state = "write public key";
	ret = convwrite(c, mk->pk, MonoPubLen);
	keyclose(k);
	return ret;
}

static Role tpm9proles[] =
{
	{"client",	xtpm9pproof},
	{"proof",	xtpm9pproof},
	{"public",	xtpm9ppub},
	{0, 0}
};

Proto tpm9p =
{
	"tpm9p",
	tpm9proles,
	nil,
	monocheckkey,
	monoclosekey
};
