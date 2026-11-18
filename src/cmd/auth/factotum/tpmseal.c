#include "std.h"
#include "dat.h"

static char*
tpmsealauth(Conv *c)
{
	Attr *a;
	char *auth, *name;

	name = "!auth";
	a = findattr(c->attr, "!auth");
	if(a == nil){
		name = "auth";
		a = findattr(c->attr, "auth");
	}
	if(a == nil)
		return estrdup("");
	auth = estrdup(a->val);
	memset(a->val, 0, strlen(a->val));
	c->attr = delattr(c->attr, name);
	return auth;
}

static int
xtpmseal(Conv *c)
{
	char *auth, *blob, *in, *policy;
	int nin, ret;

	ret = -1;
	auth = nil;
	blob = nil;
	in = nil;
	if(vaultrequire("tpm seal") < 0)
		return -1;
	auth = tpmsealauth(c);
	policy = strfindattr(c->attr, "policy");
	if(policy == nil)
		policy = "";
	c->state = "read TPM seal input";
	if((nin = convreadm(c, &in)) < 0)
		goto out;
	if(tpmsealblob(policy, auth, (uchar*)in, nin, &blob) < 0)
		goto out;
	c->state = "write TPM seal blob";
	ret = convprint(c, "name=%q policy=%q blob=%s",
	    strfindattr(c->attr, "name") == nil ? "" : strfindattr(c->attr, "name"),
	    policy, blob);

out:
	if(auth != nil){
		memset(auth, 0, strlen(auth));
		free(auth);
	}
	if(in != nil){
		memset(in, 0, nin);
		free(in);
	}
	free(blob);
	return ret;
}

static int
xtpmunseal(Conv *c)
{
	char *auth, *blob;
	uchar *out;
	int nout, ret;

	ret = -1;
	auth = nil;
	out = nil;
	if(vaultrequire("tpm unseal") < 0)
		return -1;
	auth = tpmsealauth(c);
	blob = strfindattr(c->attr, "blob");
	if(blob == nil){
		werrstr("missing blob");
		goto out;
	}
	if(tpmunsealblob(blob, auth, &out, &nout) < 0)
		goto out;
	c->state = "write TPM unsealed bytes";
	ret = convwrite(c, out, nout);

out:
	if(auth != nil){
		memset(auth, 0, strlen(auth));
		free(auth);
	}
	if(out != nil){
		memset(out, 0, nout);
		free(out);
	}
	return ret;
}

static Role tpmsealroles[] =
{
	{"seal",	xtpmseal},
	{"unseal",	xtpmunseal},
	{0, 0}
};

Proto tpmseal =
{
	"tpmseal",
	tpmsealroles,
	nil,
	nil,
	nil
};
