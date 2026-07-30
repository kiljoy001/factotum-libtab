#include "std.h"
#include "dat.h"

static int
hasprivate(Attr *a)
{
	for(; a; a = a->next)
		if(a->name != nil && a->name[0] == '!')
			return 1;
	return 0;
}

int
tpmctl(char *args)
{
	Attr *a;
	char *cmd, *handle, *p, *s;
	char caps[4096];
	int ret;

	s = estrdup(args);
	for(cmd = s; *cmd == ' ' || *cmd == '\t'; cmd++)
		;
	if((p = strchr(cmd, ' ')) != nil){
		*p++ = 0;
		while(*p == ' ' || *p == '\t')
			p++;
	}else
		p = "";

	ret = -1;
	if(strcmp(cmd, "probe") == 0){
		if(tpmprobe(caps, sizeof caps) < 0)
			goto out;
		flog("tpm probe: %s", caps);
		ret = 0;
		goto out;
	}
	if(strcmp(cmd, "initvault") == 0){
		a = parseattr(p);
		handle = strfindattr(a, "handle");
		if(handle == nil || handle[0] == 0)
			werrstr("tpm initvault requires handle");
		else
			ret = vaultinit(handle);
		freeattr(a);
		goto out;
	}
	werrstr("unknown tpm command %s", cmd);

out:
	free(s);
	return ret;
}

int
tpmgenkeyctl(char *args)
{
	Attr *a;
	Key *k;
	Proto *proto;
	TpmKeyInfo i;
	char *protoattr, *pub64;
	uchar *pub;
	int npub, ret;

	ret = -1;
	pub = nil;
	pub64 = nil;
	a = parseattr(args);
	if(a == nil){
		werrstr("empty genkey attributes");
		return -1;
	}
	protoattr = strfindattr(a, "proto");
	if(protoattr == nil || strcmp(protoattr, "tpmkey") != 0){
		werrstr("genkey currently supports only proto=tpmkey");
		goto out;
	}
	if(hasprivate(a)){
		werrstr("genkey does not accept private attributes");
		goto out;
	}
	tpmkeyinfoattrs(a, &i);
	if(i.name == nil || i.name[0] == 0){
		werrstr("missing name");
		goto out;
	}
	if(i.handle == nil || i.handle[0] == 0){
		werrstr("missing handle");
		goto out;
	}
	if(!tpmkeyalgok(i.alg)){
		werrstr("unsupported TPM key alg %s", i.alg == nil ? "" : i.alg);
		goto out;
	}
	if(i.usage == nil || i.usage[0] == 0){
		werrstr("missing usage");
		goto out;
	}
	if(!tpmkeycanuse(&i, "sign")){
		werrstr("TPM key usage must include sign");
		goto out;
	}
	if(tpmgenkey(&i, &pub, &npub) < 0)
		goto out;
	pub64 = tpmb64(pub, npub);
	a = addattr(a, "pub=%q backend=%q", pub64, tpmbackendname());

	proto = protolookup("tpmkey");
	if(proto == nil){
		werrstr("missing tpmkey protocol");
		goto out;
	}
	k = emalloc(sizeof *k);
	k->attr = copyattr(a);
	k->privattr = nil;
	k->ref = 1;
	k->proto = proto;
	if(proto->checkkey != nil && (*proto->checkkey)(k) < 0){
		keyclose(k);
		goto out;
	}
	flog("adding generated TPM key: %A", k->attr);
	keyadd(k);
	keyclose(k);
	ret = 0;

out:
	if(pub != nil){
		memset(pub, 0, npub);
		free(pub);
	}
	free(pub64);
	freeattr(a);
	return ret;
}
