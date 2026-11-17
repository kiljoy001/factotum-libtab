#include "std.h"
#include "dat.h"
#include <bio.h>

int
memrandom(void *p, int n)
{
	uchar *cp;

	for(cp = (uchar*)p; n > 0; n--)
		*cp++ = fastrand();
	return 0;
}

Attr*
addcap(Attr *a, char *from, Ticket *t)
{
	return addattr(a, "cuid=%q suid=%q cap=''", t->cuid, t->suid);
}

/*
 * The auth server for authdom: the one -a names, when it is given;
 * else the one ndb names for the domain (authdial).  9front's factotum
 * dials its -a addresses only while it cannot reach cs, at bootstrap,
 * and otherwise asks ndb; here -a wins whenever it is given, since a
 * plan9port host seldom has an ndb entry for the realm it is told to
 * use, as a 9lx machine's.
 */
int
_authdial(char *net, char *authdom)
{
	if(authaddr != nil)
		return dial(netmkaddr(authaddr, "tcp", "ticket"), 0, 0, 0);
	return authdial(net, authdom);
}

Key*
plan9authkey(Attr *a)
{
	char *dom;
	Key *k;

	/*
	 * The only important part of a is dom.
	 * We don't care, for example, about user name.
	 */
	dom = strfindattr(a, "dom");
	if(dom)
		k = keylookup("proto=p9sk1 role=server user? dom=%q", dom);
	else
		k = keylookup("proto=p9sk1 role=server user? dom?");
	if(k == nil)
		werrstr("could not find plan 9 auth key dom %q", dom);
	return k;
}
