#pragma once

typedef struct TpmBackend TpmBackend;

struct TpmBackend
{
	char	*name;
	int		(*probe)(char*, int);
	int		(*random)(uchar*, int);
	int		(*genkey)(TpmKeyInfo*, uchar**, int*);
	int		(*keypublic)(TpmKeyInfo*, uchar**, int*);
	int		(*keysign)(TpmKeyInfo*, uchar*, int, uchar**, int*);
	int		(*destroykey)(TpmKeyInfo*);
	int		(*seal)(char*, char*, uchar*, int, char**);
	int		(*unseal)(char*, char*, uchar**, int*);
	int		(*quote)(char*, uchar*, int, char**);
	int		(*initvault)(char*, char**);
	int		(*unlockvault)(char*);
};

extern TpmBackend	tpmbackendmock;
extern TpmBackend	tpmbackendtss;

int		tpmbackendinitvault(char*, char**);
int		tpmbackendunlockvault(char*);
