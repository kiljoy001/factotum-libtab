#include <u.h>
#include <libc.h>
#include <fcall.h>
#include <thread.h>
#include <9pclient.h>

static char seedhex[] =
	"000102030405060708090a0b0c0d0e0f"
	"101112131415161718191a1b1c1d1e1f";

static int
hasproto(char *list, char *proto)
{
	char *e, *p;
	int n;

	n = strlen(proto);
	for(p = list; *p != 0; p = e == nil ? p+strlen(p) : e+1){
		e = strchr(p, '\n');
		if(e == nil){
			if((int)strlen(p) == n && memcmp(p, proto, n) == 0)
				return 1;
			break;
		}
		if(e-p == n && memcmp(p, proto, n) == 0)
			return 1;
	}
	return 0;
}

static CFsys*
connectservice(char *service)
{
	CFsys *fs;
	int i;

	for(i = 0; i < 50; i++){
		if((fs = nsmount(service, nil)) != nil)
			return fs;
		sleep(100);
	}
	return nil;
}

static int
readfile(CFsys *fs, char *name, char *buf, int nbuf)
{
	CFid *fid;
	long n;
	int tot;

	if((fid = fsopen(fs, name, OREAD)) == nil){
		fprint(2, "open %s: %r\n", name);
		return -1;
	}
	tot = 0;
	while(tot < nbuf-1){
		n = fsread(fid, buf+tot, nbuf-1-tot);
		if(n < 0){
			fprint(2, "read %s: %r\n", name);
			fsclose(fid);
			return -1;
		}
		if(n == 0)
			break;
		tot += n;
	}
	buf[tot] = 0;
	fsclose(fid);
	return 0;
}

static int
writectl(CFsys *fs, char *line)
{
	CFid *fid;
	long n, got;

	if((fid = fsopen(fs, "ctl", OWRITE)) == nil){
		fprint(2, "open ctl: %r\n");
		return -1;
	}
	n = strlen(line);
	got = fswrite(fid, line, n);
	fsclose(fid);
	if(got != n){
		if(got < 0)
			fprint(2, "write ctl %q: %r\n", line);
		else
			fprint(2, "short write ctl %q: got %ld want %ld\n", line, got, n);
		return -1;
	}
	return 0;
}

static void
requestexit(CFsys *fs)
{
	CFid *fid;
	char line[] = "exit";

	if((fid = fsopen(fs, "ctl", OWRITE)) == nil)
		return;
	fswrite(fid, line, strlen(line));
	fsclose(fid);
}

static int
rpcdo(CFid *rpc, char *verb, void *data, int ndata, char *reply, int nreply)
{
	uchar buf[8192];
	long got, n;
	int nverb;

	nverb = strlen(verb);
	if(nverb + 1 + ndata > (int)sizeof buf){
		fprint(2, "rpc too large: %s\n", verb);
		return -1;
	}
	memmove(buf, verb, nverb);
	if(ndata > 0){
		buf[nverb] = ' ';
		memmove(buf+nverb+1, data, ndata);
		n = nverb + 1 + ndata;
	}else
		n = nverb;
	got = fswrite(rpc, buf, n);
	if(got != n){
		if(got < 0)
			fprint(2, "rpc write %s: %r\n", verb);
		else
			fprint(2, "rpc short write %s: got %ld want %ld\n", verb, got, n);
		return -1;
	}
	got = fsread(rpc, reply, nreply-1);
	if(got < 0){
		fprint(2, "rpc read %s: %r\n", verb);
		return -1;
	}
	reply[got] = 0;
	return got;
}

static int
expectok(char *reply, char *what)
{
	if(strcmp(reply, "ok") == 0)
		return 0;
	fprint(2, "%s: got %q want ok\n", what, reply);
	return -1;
}

static int
expectprefix(char *reply, char *prefix, char *what)
{
	int n;

	n = strlen(prefix);
	if(strncmp(reply, prefix, n) == 0)
		return 0;
	fprint(2, "%s: got %q want prefix %q\n", what, reply, prefix);
	return -1;
}

static int
expectcontains(char *reply, char *needle, char *what)
{
	if(strstr(reply, needle) != nil)
		return 0;
	fprint(2, "%s: got %q want substring %q\n", what, reply, needle);
	return -1;
}

static int
expecterror(char *reply, char *what)
{
	if(strncmp(reply, "error ", 6) == 0)
		return 0;
	fprint(2, "%s: got %q want error\n", what, reply);
	return -1;
}

static int
native_tpm(void)
{
	char *s;
	int ret;

	s = getenv("FACTOTUM_TEST_NATIVE_TPM");
	ret = s != nil && s[0] != 0 && strcmp(s, "0") != 0;
	free(s);
	return ret;
}

static int
replyattr(char *reply, char *name, char *buf, int nbuf)
{
	char *e, *p, *q;
	int n, quoted;

	n = strlen(name);
	for(p = reply; (p = strstr(p, name)) != nil; p++){
		if(p != reply && p[-1] != ' ' && p[-1] != '\t')
			continue;
		if(p[n] != '=')
			continue;
		q = p+n+1;
		quoted = 0;
		if(*q == '\'' || *q == '"'){
			quoted = *q++;
			for(e = q; *e != 0 && *e != quoted; e++)
				;
		}else{
			for(e = q; *e != 0 && *e != ' ' && *e != '\t' && *e != '\n'; e++)
				;
		}
		if(e-q >= nbuf){
			fprint(2, "attribute %s too large in %q\n", name, reply);
			return -1;
		}
		memmove(buf, q, e-q);
		buf[e-q] = 0;
		return 0;
	}
	fprint(2, "missing attribute %s in %q\n", name, reply);
	return -1;
}

static int
rpcstart(CFid *rpc, char *attrs)
{
	char reply[256];

	if(rpcdo(rpc, "start", attrs, strlen(attrs), reply, sizeof reply) < 0)
		return -1;
	return expectok(reply, attrs);
}

static int
exercise_tpm(CFsys *fs)
{
	CFid *rpc;
	char blob[2048], challenge[] = "0123456789abcdef0123456789abcdef";
	char reply[4096];
	char secret[] = "integration-secret";
	int is_native, ret;

	ret = -1;
	is_native = native_tpm();
	rpc = nil;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpm role=caps") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectcontains(reply, is_native ? "backend=tpm2" : "backend=mock",
	    "tpm caps") < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpm role=random n=16") < 0)
		goto out;
	if(rpcdo(rpc, "readhex", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok ", "tpm random") < 0)
		goto out;
	if((int)strlen(reply) != 3 + 2*16){
		fprint(2, "tpm random length: got %d want %d\n",
		    (int)strlen(reply), 3 + 2*16);
		goto out;
	}
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpmkey role=public name=alice-tpm") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok alg=tpm2-ecdsa-p256-sha256 pub=", "tpmkey public") < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpmkey role=sign name=alice-tpm hash=sha256") < 0)
		goto out;
	if(rpcdo(rpc, "writehex", seedhex, strlen(seedhex), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "tpmkey sign write digest") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectcontains(reply, "sig=", "tpmkey signature") < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, is_native ?
	    "proto=tpmseal role=seal name=vault policy=none !auth=login-secret" :
	    "proto=tpmseal role=seal name=vault policy=pcr:sha256:0,2,7 !auth=login-secret") < 0)
		goto out;
	if(rpcdo(rpc, "write", secret, strlen(secret), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "tpmseal write secret") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectcontains(reply, "blob=", "tpmseal blob") < 0)
		goto out;
	if(replyattr(reply, "blob", blob, sizeof blob) < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	snprint(reply, sizeof reply, "proto=tpmseal role=unseal name=vault blob=%s !auth=wrong-secret", blob);
	if(rpcstart(rpc, reply) < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expecterror(reply, "tpm unseal with wrong auth") < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	snprint(reply, sizeof reply, "proto=tpmseal role=unseal name=vault blob=%s !auth=login-secret", blob);
	if(rpcstart(rpc, reply) < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok integration-secret") != 0){
		fprint(2, "tpm unseal: got %q\n", reply);
		goto out;
	}
	fsclose(rpc);
	rpc = nil;

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpm role=quote pcrs=0,2,7") < 0)
		goto out;
	if(rpcdo(rpc, "write", challenge, strlen(challenge), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "tpm quote write nonce") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(is_native){
		if(expectcontains(reply, "attest=", "tpm quote attest") < 0
		|| expectcontains(reply, "sig=", "tpm quote sig") < 0)
			goto out;
	}else{
		if(expectcontains(reply, "quote=", "tpm quote") < 0)
			goto out;
	}
	fsclose(rpc);
	rpc = nil;

	if(writectl(fs, "lock") < 0)
		goto out;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpmkey role=sign name=alice-tpm hash=sha256") < 0)
		goto out;
	if(rpcdo(rpc, "writehex", seedhex, strlen(seedhex), reply, sizeof reply) < 0)
		goto out;
	if(expecterror(reply, "tpmkey sign while locked") < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if(writectl(fs, "unlock") < 0)
		goto out;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=tpmkey role=sign name=alice-tpm hash=sha256") < 0)
		goto out;
	if(rpcdo(rpc, "writehex", seedhex, strlen(seedhex), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "tpmkey sign after unlock write digest") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectcontains(reply, "sig=", "tpmkey signature after unlock") < 0)
		goto out;
	fsclose(rpc);
	rpc = nil;

	if(writectl(fs, "delkey proto=tpmkey name=alice-tpm destroy=1") < 0)
		goto out;
	ret = 0;

out:
	if(rpc != nil)
		fsclose(rpc);
	return ret;
}

static int
exercise_monocypher(CFsys *fs)
{
	CFid *rpc;
	char reply[512], sighex[2*64+1];
	int ret;

	ret = -1;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=monocypher role=sign user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "write", "factotum smoke", 14, reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "monocypher write message") < 0)
		goto out;
	if(rpcdo(rpc, "readhex", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok ", "monocypher signature") < 0)
		goto out;
	if((int)strlen(reply) != 3 + 2*64){
		fprint(2, "monocypher signature length: got %d want %d\n",
		    (int)strlen(reply), 3 + 2*64);
		goto out;
	}
	strecpy(sighex, sighex+sizeof sighex, reply+3);
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=monocypher role=public user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "readhex", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok ", "monocypher public key") < 0)
		goto out;
	if((int)strlen(reply) != 3 + 2*32){
		fprint(2, "monocypher public key length: got %d want %d\n",
		    (int)strlen(reply), 3 + 2*32);
		goto out;
	}
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=monocypher role=sigcell user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "write", "factotum smoke", 14, reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "monocypher sigcell write message") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok signed:", "monocypher sigcell") < 0)
		goto out;
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=monocypher role=verify user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "write", "factotum smoke", 14, reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "monocypher verify write message") < 0)
		goto out;
	if(rpcdo(rpc, "writehex", sighex, strlen(sighex), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "monocypher verify write signature") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok ok") != 0){
		fprint(2, "monocypher verify: got %q\n", reply);
		goto out;
	}
	ret = 0;

out:
	fsclose(rpc);
	return ret;
}

static int
exercise_ed25519(CFsys *fs)
{
	CFid *rpc;
	char reply[512], sighex[2*64+1];
	int ret;

	ret = -1;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=ed25519 role=sign user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "write", "r", 1, reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "ed25519 write message") < 0)
		goto out;
	if(rpcdo(rpc, "readhex", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok ", "ed25519 signature") < 0)
		goto out;
	if((int)strlen(reply) != 3 + 2*64){
		fprint(2, "ed25519 signature length: got %d want %d\n",
		    (int)strlen(reply), 3 + 2*64);
		goto out;
	}
	if(cistrcmp(reply+3,
	    "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
	    "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00") != 0){
		fprint(2, "Ed25519 RPC RFC signature mismatch\n");
		goto out;
	}
	strecpy(sighex, sighex+sizeof sighex, reply+3);
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=ed25519 role=public user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "readhex", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok ", "ed25519 public key") < 0)
		goto out;
	if((int)strlen(reply) != 3 + 2*32){
		fprint(2, "ed25519 public key length: got %d want %d\n",
		    (int)strlen(reply), 3 + 2*32);
		goto out;
	}
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=ed25519 role=verify user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "write", "r", 1, reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "ed25519 verify write message") < 0)
		goto out;
	if(rpcdo(rpc, "writehex", sighex, strlen(sighex), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "ed25519 verify write signature") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok ok") != 0){
		fprint(2, "ed25519 verify: got %q\n", reply);
		goto out;
	}
	fsclose(rpc);
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil)
		return -1;
	sighex[0] = sighex[0] == '0' ? '1' : '0';
	if(rpcstart(rpc, "proto=ed25519 role=verify user=alice") < 0
	|| rpcdo(rpc, "write", "r", 1, reply, sizeof reply) < 0
	|| expectok(reply, "ed25519 altered message") < 0
	|| rpcdo(rpc, "writehex", sighex, strlen(sighex), reply, sizeof reply) < 0
	|| expectok(reply, "ed25519 altered signature") < 0
	|| rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok signature does not verify") != 0)
		goto out;
	ret = 0;

out:
	fsclose(rpc);
	return ret;
}

static int
exercise_tpm9p(CFsys *fs)
{
	CFid *rpc;
	char reply[1024];
	char challenge[] = "0123456789abcdef0123456789abcdef";
	int ret;

	ret = -1;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc,
	    "proto=tpm9p role=proof authdom=rentonsoftworks.coin user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "write", challenge, strlen(challenge), reply, sizeof reply) < 0)
		goto out;
	if(expectok(reply, "tpm9p write challenge") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok signed:", "tpm9p proof") < 0)
		goto out;
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc,
	    "proto=tpm9p role=public authdom=rentonsoftworks.coin user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "readhex", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(expectprefix(reply, "ok ", "tpm9p public key") < 0)
		goto out;
	if((int)strlen(reply) != 3 + 2*32){
		fprint(2, "tpm9p public key length: got %d want %d\n",
		    (int)strlen(reply), 3 + 2*32);
		goto out;
	}
	ret = 0;

out:
	fsclose(rpc);
	return ret;
}

static int
exercise_oauth(CFsys *fs)
{
	CFid *rpc;
	char reply[256];
	int ret;

	ret = -1;
	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=oauth2 role=bearer service=keycloak user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok Bearer test-token") != 0){
		fprint(2, "oauth bearer: got %q\n", reply);
		goto out;
	}
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=oauth role=access service=keycloak user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok test-token") != 0){
		fprint(2, "oauth access: got %q\n", reply);
		goto out;
	}
	fsclose(rpc);

	if((rpc = fsopen(fs, "rpc", ORDWR)) == nil){
		fprint(2, "open rpc: %r\n");
		return -1;
	}
	if(rpcstart(rpc, "proto=oauth2 role=refresh service=keycloak user=alice") < 0)
		goto out;
	if(rpcdo(rpc, "read", nil, 0, reply, sizeof reply) < 0)
		goto out;
	if(strcmp(reply, "ok refresh-token") != 0){
		fprint(2, "oauth refresh: got %q\n", reply);
		goto out;
	}
	ret = 0;

out:
	fsclose(rpc);
	return ret;
}

static int
requireproto(char *protos, char *proto)
{
	if(hasproto(protos, proto))
		return 0;
	fprint(2, "missing protocol %s\nprotocols:\n%s", proto, protos);
	return -1;
}

void
threadmain(int argc, char **argv)
{
	CFsys *fs;
	char protos[4096], key[512];
	char *service, *status, *tpmkey, *vault;

	if(argc != 2){
		fprint(2, "usage: factotum-smoke-test service\n");
		threadexitsall("usage");
	}
	service = argv[1];
	status = "smoke";
	vault = getenv("FACTOTUM_TEST_TPM_VAULT_HANDLE");
	if(vault == nil || vault[0] == 0){
		free(vault);
		vault = strdup("0x81010000");
	}
	tpmkey = getenv("FACTOTUM_TEST_TPM_KEY_HANDLE");
	if(tpmkey == nil || tpmkey[0] == 0){
		free(tpmkey);
		tpmkey = strdup("0x81010010");
	}

	if((fs = connectservice(service)) == nil){
		fprint(2, "connect %s: %r\n", service);
		threadexitsall("connect");
	}
	if(readfile(fs, "proto", protos, sizeof protos) < 0)
		goto out;
	if(requireproto(protos, "dp9ik") < 0
	|| requireproto(protos, "p9any") < 0
	|| requireproto(protos, "monocypher") < 0
	|| requireproto(protos, "ed25519") < 0
	|| requireproto(protos, "tpm") < 0
	|| requireproto(protos, "tpm9p") < 0
	|| requireproto(protos, "tpmkey") < 0
	|| requireproto(protos, "tpmseal") < 0
	|| requireproto(protos, "oauth") < 0
	|| requireproto(protos, "oauth2") < 0)
		goto out;

	if(writectl(fs, "key proto=ed25519 user=alice !seed=4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb") < 0)
		goto out;
	snprint(key, sizeof key, "key proto=monocypher user=alice !seed=%s", seedhex);
	if(writectl(fs, key) < 0)
		goto out;
	snprint(key, sizeof key,
	    "key proto=tpm9p authdom=rentonsoftworks.coin user=alice !seed=%s", seedhex);
	if(writectl(fs, key) < 0)
		goto out;
	if(writectl(fs,
	    "key proto=oauth service=keycloak user=alice !access_token=test-token !refresh_token=refresh-token") < 0)
		goto out;
	if(writectl(fs,
	    "key proto=oauth2 service=keycloak user=alice !access_token=test-token !refresh_token=refresh-token") < 0)
		goto out;
	if(writectl(fs, "tpm probe") < 0)
		goto out;
	snprint(key, sizeof key, "tpm initvault handle=%s", vault);
	if(writectl(fs, key) < 0)
		goto out;
	snprint(key, sizeof key,
	    "genkey proto=tpmkey name=alice-tpm handle=%s alg=tpm2-ecdsa-p256-sha256 usage=sign",
	    tpmkey);
	if(writectl(fs, key) < 0)
		goto out;
	if(exercise_ed25519(fs) < 0)
		goto out;
	if(exercise_monocypher(fs) < 0)
		goto out;
	if(exercise_tpm9p(fs) < 0)
		goto out;
	if(exercise_oauth(fs) < 0)
		goto out;
	if(exercise_tpm(fs) < 0)
		goto out;

	print("factotum integration smoke ok\n");
	if(getenv("FACTOTUM_TEST_EXIT") != nil)
		requestexit(fs);
	status = nil;

out:
	free(vault);
	free(tpmkey);
	fsunmount(fs);
	threadexitsall(status);
}
