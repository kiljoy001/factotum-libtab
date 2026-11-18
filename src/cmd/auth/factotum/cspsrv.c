/*
 * cspsrv.c - serve factotum on CSP channel pairs (9lx's /dev/csp).
 *
 * Built only when HAVE_CSP is defined (the 9lx build passes CSP_INC, the
 * directory holding 9lx's cspctl.h).  factotum -C n creates n pairs of
 * anonymous CSP channels at startup, writes their capabilities on
 * standard output, one pair per line as "<req-cap> <resp-cap>\n", and
 * serves 9P on each pair with its own srv loop.  The process that
 * started factotum reads the lines and attaches, itself or by handing
 * them to the one client it means to have them.  CSP channels on the
 * default band take one attacher each, so once attached a capability is
 * spent, and no name is registered anywhere for anyone else to find:
 * holding the channel is the authority.
 *
 * The extra srv loops change nothing in factotum's core: like the one
 * threadpostmountsrv starts, each answers attach, walk and stat itself
 * and forwards everything else to fsproc, where the request and
 * conversation threads run cooperatively as before.  fsstart, which
 * creates fsproc and its channels, therefore runs once, here, and is
 * taken out of the template every loop is copied from.
 */
#include "std.h"
#include "dat.h"
#include <sys/ioctl.h>
#include "cspctl.h"

enum {
	CspCapacity = 65536,	/* room for several msize-sized messages */
};

static int
cspcreate(struct csp_create_req *req)
{
	int fd;

	fd = open("/dev/csp", ORDWR);
	if(fd < 0)
		return -1;
	memset(req, 0, sizeof *req);
	req->capacity = CspCapacity;
	if(ioctl(fd, CSP_CREATE, req) < 0){
		close(fd);
		return -1;
	}
	return fd;
}

static void
cspconn(void *v)
{
	Srv *s;

	s = v;
	srv(s);
	close(s->infd);
	close(s->outfd);
}

void
cspserve(int n)
{
	struct csp_create_req rq, rs;
	int i, reqfd, respfd, null;
	Srv *s;

	fsstart(&fs);
	fs.start = nil;
	for(i = 0; i < n; i++){
		/* req: the client writes, factotum reads; resp: the reverse */
		if((reqfd = cspcreate(&rq)) < 0)
			sysfatal("csp: create request channel: %r");
		if((respfd = cspcreate(&rs)) < 0)
			sysfatal("csp: create response channel: %r");
		fprint(1, "%ud:%016llux%016llux %ud:%016llux%016llux\n",
			rq.id, (uvlong)rq.secret.hi, (uvlong)rq.secret.lo,
			rs.id, (uvlong)rs.secret.hi, (uvlong)rs.secret.lo);
		s = emalloc(sizeof *s);
		*s = fs;
		s->infd = reqfd;
		s->outfd = respfd;
		s->srvfd = -1;
		s->leavefdsopen = 1;
		proccreate(cspconn, s, STACK);
	}
	memset(&rq, 0, sizeof rq);
	memset(&rs, 0, sizeof rs);
	/* The reader stops at n lines; nothing else goes to it. */
	if((null = open("/dev/null", OWRITE)) >= 0){
		dup2(null, 1);
		close(null);
	}
}
