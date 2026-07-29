#include "std.h"
#include "dat.h"
#include "monokey.h"

static int
xmonosign(Conv *c)
{
	char *msg;
	uchar sig[MonoSigLen];
	int n, ret;
	Key *k;

	ret = -1;
	msg = nil;
	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;

	c->state = "read message";
	if((n = convreadm(c, &msg)) < 0)
		goto out;
	if(monosign(k, (uchar*)msg, n, sig) < 0)
		goto out;

	c->state = "write signature";
	ret = convwrite(c, sig, sizeof sig);

out:
	memset(sig, 0, sizeof sig);
	free(msg);
	keyclose(k);
	return ret;
}

static int
xmonosigcell(Conv *c)
{
	char *cell, *msg;
	uchar sig[MonoSigLen];
	int n, ret;
	Key *k;

	ret = -1;
	cell = nil;
	msg = nil;
	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;

	c->state = "read message";
	if((n = convreadm(c, &msg)) < 0)
		goto out;
	if(monosign(k, (uchar*)msg, n, sig) < 0)
		goto out;
	cell = monosignedcell((uchar*)msg, n, sig);
	if(cell == nil)
		goto out;

	c->state = "write signed cell";
	ret = convwrite(c, cell, strlen(cell));

out:
	memset(sig, 0, sizeof sig);
	free(cell);
	free(msg);
	keyclose(k);
	return ret;
}

static int
xmonoverify(Conv *c)
{
	char *msg;
	uchar sig[MonoSigLen];
	int n, ret;
	Key *k;

	ret = -1;
	msg = nil;
	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;

	c->state = "read message";
	if((n = convreadm(c, &msg)) < 0)
		goto out;
	c->state = "read signature";
	if(convread(c, sig, sizeof sig) < 0)
		goto out;

	c->state = "verify";
	if(monoverify(k, (uchar*)msg, n, sig) == 0)
		ret = convprint(c, "ok");
	else
		ret = convprint(c, "signature does not verify");

out:
	memset(sig, 0, sizeof sig);
	free(msg);
	keyclose(k);
	return ret;
}

static int
xmonopub(Conv *c)
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

static Role monoroles[] =
{
	{"sign",	xmonosign},
	{"sigcell",	xmonosigcell},
	{"verify",	xmonoverify},
	{"public",	xmonopub},
	{0, 0}
};

Proto monocypher =
{
	"monocypher",
	monoroles,
	nil,
	monocheckkey,
	monoclosekey
};
