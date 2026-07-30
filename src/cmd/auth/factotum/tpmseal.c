#include "std.h"
#include "dat.h"

static int
xtpmseal(Conv *c)
{
	char *blob, *in, *policy;
	int nin, ret;

	ret = -1;
	blob = nil;
	in = nil;
	if(vaultrequire("tpm seal") < 0)
		return -1;
	policy = strfindattr(c->attr, "policy");
	if(policy == nil)
		policy = "";
	c->state = "read TPM seal input";
	if((nin = convreadm(c, &in)) < 0)
		goto out;
	if(tpmsealblob(policy, (uchar*)in, nin, &blob) < 0)
		goto out;
	c->state = "write TPM seal blob";
	ret = convprint(c, "name=%q policy=%q blob=%s",
	    strfindattr(c->attr, "name") == nil ? "" : strfindattr(c->attr, "name"),
	    policy, blob);

out:
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
	char *blob;
	uchar *out;
	int nout, ret;

	ret = -1;
	out = nil;
	if(vaultrequire("tpm unseal") < 0)
		return -1;
	blob = strfindattr(c->attr, "blob");
	if(blob == nil){
		werrstr("missing blob");
		return -1;
	}
	if(tpmunsealblob(blob, &out, &nout) < 0)
		return -1;
	c->state = "write TPM unsealed bytes";
	ret = convwrite(c, out, nout);
	memset(out, 0, nout);
	free(out);
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
