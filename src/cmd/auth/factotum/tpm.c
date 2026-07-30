#include "std.h"
#include "dat.h"

static int
parsecount(char *s, int min, int max, int *out)
{
	char *p;
	long n;

	if(s == nil || s[0] == 0){
		werrstr("missing count");
		return -1;
	}
	n = strtol(s, &p, 0);
	if(*p != 0 || n < min || n > max){
		werrstr("count must be between %d and %d", min, max);
		return -1;
	}
	*out = n;
	return 0;
}

static int
xtpmcaps(Conv *c)
{
	char caps[4096];

	if(tpmprobe(caps, sizeof caps) < 0)
		return -1;
	c->state = "write TPM capabilities";
	return convwrite(c, caps, strlen(caps));
}

static int
xtpmrandom(Conv *c)
{
	char *s;
	uchar *buf;
	int n, ret;

	ret = -1;
	s = strfindattr(c->attr, "n");
	if(parsecount(s, 1, 4096, &n) < 0)
		return -1;
	buf = emalloc(n);
	if(tpmrandom(buf, n) < 0)
		goto out;
	c->state = "write TPM random bytes";
	ret = convwrite(c, buf, n);

out:
	memset(buf, 0, n);
	free(buf);
	return ret;
}

static int
xtpmquote(Conv *c)
{
	char *nonce, *pcrs, *quote;
	int nnonce, ret;

	ret = -1;
	nonce = nil;
	quote = nil;
	if(vaultrequire("tpm quote") < 0)
		return -1;
	pcrs = strfindattr(c->attr, "pcrs");
	if(pcrs == nil)
		pcrs = strfindattr(c->attr, "pcr");
	c->state = "read TPM quote nonce";
	if((nnonce = convreadm(c, &nonce)) < 0)
		goto out;
	if(nnonce < 16){
		werrstr("quote nonce too short");
		goto out;
	}
	if(tpmquote(pcrs, (uchar*)nonce, nnonce, &quote) < 0)
		goto out;
	c->state = "write TPM quote";
	ret = convwrite(c, quote, strlen(quote));

out:
	free(nonce);
	free(quote);
	return ret;
}

static Role tpmroles[] =
{
	{"caps",	xtpmcaps},
	{"random",	xtpmrandom},
	{"quote",	xtpmquote},
	{0, 0}
};

Proto tpm =
{
	"tpm",
	tpmroles,
	nil,
	nil,
	nil
};
