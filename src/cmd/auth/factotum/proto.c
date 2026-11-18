#include "std.h"
#include "dat.h"

extern Proto	apop;		/* apop.c */
extern Proto	chap;		/* chap.c */
extern Proto	cram;		/* apop.c */
extern Proto	dsa;			/* dsa.c */
extern Proto	httpdigest;	/* httpdigest.c */
extern Proto	mschap;		/* chap.c */
extern Proto	ed25519;	/* monocypherproto.c */
extern Proto	monocypher;	/* monocypherproto.c */
extern Proto	oauth;		/* oauth.c */
extern Proto	oauth2;		/* oauth.c */
extern Proto	p9any;		/* p9any.c */
extern Proto	dp9ik;		/* p9sk1.c */
extern Proto	p9sk1;		/* p9sk1.c */
extern Proto	p9sk2;		/* p9sk2.c */
extern Proto	p9cr;
extern Proto	pass;			/* pass.c */
extern Proto	rsa;			/* rsa.c */
extern Proto	tpm;			/* tpm.c */
extern Proto	tpm9p;		/* tpm9p.c */
extern Proto	tpmkey;		/* tpmkey.c */
extern Proto	tpmseal;		/* tpmseal.c */
extern Proto	vnc;			/* p9cr.c */

Proto *prototab[] = {
	&apop,
	&chap,
	&cram,
	&dsa,
	&httpdigest,
	&mschap,
	&monocypher,
	&ed25519,
	&oauth,
	&oauth2,
	&p9any,
	&dp9ik,
	&p9cr,
	&p9sk1,
	&p9sk2,
	&pass,
	&rsa,
	&tpm,
	&tpm9p,
	&tpmkey,
	&tpmseal,
	&vnc,
	nil
};

Proto*
protolookup(char *name)
{
	int i;

	for(i=0; prototab[i]; i++)
		if(strcmp(prototab[i]->name, name) == 0)
			return prototab[i];
	return nil;
}
