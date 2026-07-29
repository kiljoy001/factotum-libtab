#include "std.h"
#include "dat.h"

static char*
findtoken(Key *k, char *name)
{
	char buf[64];
	char *s;

	s = strfindattr(k->privattr, name);
	if(s != nil)
		return s;
	snprint(buf, sizeof buf, "!%s", name);
	return strfindattr(k->privattr, buf);
}

static int
oauthcheck(Key *k)
{
	if(findtoken(k, "access_token") == nil){
		werrstr("missing !access_token");
		return -1;
	}
	return 0;
}

static int
xoauthaccess(Conv *c)
{
	char *tok;
	Key *k;
	int ret;

	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;
	tok = findtoken(k, "access_token");
	if(tok == nil){
		werrstr("missing !access_token");
		keyclose(k);
		return -1;
	}
	c->state = "write access token";
	ret = convwrite(c, tok, strlen(tok));
	keyclose(k);
	return ret;
}

static int
xoauthbearer(Conv *c)
{
	char *bearer, *tok;
	Key *k;
	int ret;

	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;
	tok = findtoken(k, "access_token");
	if(tok == nil){
		werrstr("missing !access_token");
		keyclose(k);
		return -1;
	}
	bearer = smprint("Bearer %s", tok);
	if(bearer == nil){
		werrstr("out of memory");
		keyclose(k);
		return -1;
	}
	c->state = "write bearer token";
	ret = convwrite(c, bearer, strlen(bearer));
	free(bearer);
	keyclose(k);
	return ret;
}

static int
xoauthrefresh(Conv *c)
{
	char *tok;
	Key *k;
	int ret;

	k = keyfetch(c, "%A", c->attr);
	if(k == nil)
		return -1;
	tok = findtoken(k, "refresh_token");
	if(tok == nil){
		werrstr("missing !refresh_token");
		keyclose(k);
		return -1;
	}
	c->state = "write refresh token";
	ret = convwrite(c, tok, strlen(tok));
	keyclose(k);
	return ret;
}

static Role oauthroles[] =
{
	{"client",	xoauthaccess},
	{"access",	xoauthaccess},
	{"bearer",	xoauthbearer},
	{"refresh",	xoauthrefresh},
	{0, 0}
};

Proto oauth =
{
	"oauth",
	oauthroles,
	nil,
	oauthcheck,
	nil
};

Proto oauth2 =
{
	"oauth2",
	oauthroles,
	nil,
	oauthcheck,
	nil
};
