#include "std.h"
#include "dat.h"
#include "tpm_backend.h"

static QLock vaultlk;
static int initialized;
static int locked;
static char *vaultblob;

int
vaultinit(char *handle)
{
	char *blob;

	blob = nil;
	if(tpmbackendinitvault(handle, &blob) < 0)
		return -1;
	qlock(&vaultlk);
	free(vaultblob);
	vaultblob = blob;
	initialized = 1;
	locked = 0;
	qunlock(&vaultlk);
	flog("tpm vault initialized backend=%s handle=%q", tpmbackendname(), handle);
	return 0;
}

int
vaultlock(void)
{
	qlock(&vaultlk);
	if(!initialized){
		qunlock(&vaultlk);
		werrstr("vault is not initialized");
		return -1;
	}
	locked = 1;
	qunlock(&vaultlk);
	flog("tpm vault locked");
	return 0;
}

int
vaultunlock(void)
{
	char *blob;

	qlock(&vaultlk);
	if(!initialized){
		qunlock(&vaultlk);
		werrstr("vault is not initialized");
		return -1;
	}
	blob = estrdup(vaultblob);
	qunlock(&vaultlk);

	if(tpmbackendunlockvault(blob) < 0){
		free(blob);
		return -1;
	}
	free(blob);

	qlock(&vaultlk);
	locked = 0;
	qunlock(&vaultlk);
	flog("tpm vault unlocked");
	return 0;
}

int
vaultislocked(void)
{
	int r;

	qlock(&vaultlk);
	r = initialized && locked;
	qunlock(&vaultlk);
	return r;
}

int
vaultrequire(char *op)
{
	if(!vaultislocked())
		return 0;
	if(op == nil)
		op = "secret operation";
	werrstr("%s requires unlocked vault", op);
	return -1;
}
