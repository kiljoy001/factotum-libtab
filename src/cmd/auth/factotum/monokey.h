#define MonoAlg		"libtab-eddsa-blake2b-v1"
#define MonoAlgShort	"eddsa-blake2b"

enum
{
	MonoSeedLen	= 32,
	MonoPubLen	= 32,
	MonoKeyLen	= 64,
	MonoSigLen	= 64,
};

typedef struct MonoKey MonoKey;
struct MonoKey
{
	uchar	sk[MonoKeyLen];
	uchar	pk[MonoPubLen];
	int	havesk;
	int	ed25519;
};

int	monoalg(char*);
int	monocheckkey(Key*);
int	ed25519checkkey(Key*);
void	monoclosekey(Key*);
int	monosign(Key*, uchar*, int, uchar[MonoSigLen]);
int	monoverify(Key*, uchar*, int, uchar[MonoSigLen]);
char	*monob64(uchar*, int);
char	*monosignedcell(uchar*, int, uchar[MonoSigLen]);
