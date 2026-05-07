# Standalone Makefile for factotum-dp9ik

CC = gcc
AR = ar
ARFLAGS = rsc
CFLAGS = -Wall -O2 -g -Iinclude -Isrc/lib9 -Isrc/libthread -DPLAN9PORT -std=gnu11 -fno-common
LDFLAGS = -lpthread

# OS-specific flags
ifeq ($(shell uname), Linux)
	CFLAGS += -D_GNU_SOURCE
	LDFLAGS += -lutil -lresolv
endif

# Libraries (ordered by dependency)
LIBS = lib9pclient.a lib9p.a libauthsrv.a libauth.a libndb.a libip.a libmux.a libsec.a libmp.a libthread.a libbio.a lib9.a

# Group libs to handle circular dependencies
LIB_GROUP = -Wl,--start-group $(LIBS) -Wl,--end-group

# Commands
CMDS = factotum passwd netkey

all: $(LIBS) $(CMDS)

# lib9
LIB9_DIR = src/lib9
LIB9_SRCS = $(addprefix $(LIB9_DIR)/, \
	_exits.c _p9dialparse.c _p9dir.c announce.c argv0.c atexit.c atoi.c atol.c atoll.c \
	atnotify.c await.c cistrcmp.c cistrncmp.c cistrstr.c cleanname.c convD2M.c convM2D.c \
	convM2S.c convS2M.c crypt.c ctime.c dial.c dirfstat.c dirfwstat.c dirmodefmt.c \
	dirstat.c dirwstat.c dup.c encodefmt.c errstr.c exec.c execl.c exitcode.c fcallfmt.c \
	fmtlock2.c frand.c frexp.c get9root.c getcallerpc.c getenv.c getfields.c getnetconn.c getns.c getuser.c \
	getwd.c jmp.c lrand.c lnrand.c main.c malloc.c malloctag.c mallocz.c nan.c \
	needsrcquote.c needstack.c netcrypt.c netmkaddr.c notify.c nrand.c nulldir.c open.c \
	opentemp.c pin.c pipe.c post9p.c postnote.c qlock.c quote.c rand.c read9pmsg.c \
	readcons.c readn.c rfork.c searchpath.c sendfd.c sleep.c strdup.c strecpy.c \
	sysfatal.c syslog.c sysname.c time.c tm2sec.c tokenize.c truerand.c u16.c u32.c \
	u64.c unsharp.c wait.c waitpid.c write.c zoneinfo.c)

FMT_SRCS = $(addprefix $(LIB9_DIR)/fmt/, \
	dofmt.c fltfmt.c fmt.c fmtfd.c fmtfdflush.c fmtlocale.c fmtnull.c \
	fmtprint.c fmtquote.c fmtrune.c fmtstr.c fmtvprint.c fprint.c nan64.c \
	print.c runefmtstr.c runeseprint.c runesmprint.c runesnprint.c runesprint.c \
	runevseprint.c runevsmprint.c runevsnprint.c seprint.c smprint.c snprint.c \
	sprint.c strtod.c vfprint.c vseprint.c vsmprint.c vsnprint.c charstod.c pow10.c)

UTF_SRCS = $(addprefix $(LIB9_DIR)/utf/, \
	rune.c runestrcat.c runestrchr.c runestrcmp.c runestrcpy.c runestrdup.c \
	runestrlen.c runestrecpy.c runestrncat.c runestrncmp.c runestrncpy.c \
	runestrrchr.c runestrstr.c runetype.c utfecpy.c utflen.c utfnlen.c \
	utfrrune.c utfrune.c utfutf.c)

LIB9_OBJS = $(LIB9_SRCS:.c=.o) $(FMT_SRCS:.c=.o) $(UTF_SRCS:.c=.o)

lib9.a: $(LIB9_OBJS)
	$(AR) $(ARFLAGS) $@ $^

$(LIB9_DIR)/get9root.o: $(LIB9_DIR)/get9root.c
	$(CC) $(CFLAGS) -DPLAN9_TARGET='"$(shell pwd)"' -c $< -o $@

$(LIB9_DIR)/fmt/%.o: $(LIB9_DIR)/fmt/%.c
	$(CC) $(CFLAGS) -I$(LIB9_DIR)/fmt -c $< -o $@

$(LIB9_DIR)/utf/%.o: $(LIB9_DIR)/utf/%.c
	$(CC) $(CFLAGS) -I$(LIB9_DIR)/utf -c $< -o $@

# libmp
LIBMP_DIR = src/libmp/port
LIBMP_FILES = mpaux mpfmt strtomp mptobe mptole betomp letomp mpadd mpsub mpcmp \
	mpfactorial cnfield gmfield mpfield mpmul mpmodop mpleft mpright mpnrand \
	mpsel mpvecadd mpvecsub mpvecdigmuladd mpvectscmp mptober mpveccmp mpdigdiv \
	mpdiv mpexp mpmod mpextendedgcd mpinvert mprand crt mptoi mptoui mptov mptouv
LIBMP_OBJS = $(addprefix $(LIBMP_DIR)/, $(addsuffix .o, $(LIBMP_FILES)))

libmp.a: $(LIBMP_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libsec
LIBSEC_DIR = src/libsec/port
LIBSEC_FILES = aes blowfish ccpoly chacha chachablock decodepem des des3CBC \
	des3ECB desCBC desECB desmodes dsaalloc dsagen dsaprimes dsaprivtopub \
	dsasign dsaverify egalloc egdecrypt egencrypt eggen egprivtopub egsign \
	egverify fastrand genprime genrandom gensafeprime genstrongprime hmac hkdf \
	md4 md5 md5block md5pickle nfastrand pbkdf2 poly1305 prng probably_prime \
	rc4 readcert rsaalloc rsadecrypt rsaencrypt rsafill rsagen rsaprivtopub \
	sha1 sha1block sha1pickle sha2_64 sha2block64 smallprimetest thumb tlshand \
	tsmemcmp x509
LIBSEC_OBJS = $(addprefix $(LIBSEC_DIR)/, $(addsuffix .o, $(LIBSEC_FILES)))

libsec.a: $(LIBSEC_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libauth
LIBAUTH_DIR = src/libauth
LIBAUTH_FILES = amount_getkey.c attr.c auth_attr.c auth_challenge.c auth_getkey.c \
	auth_getuserpasswd.c auth_proxy.c auth_respond.c auth_rpc.c \
	auth_userpasswd.c fsamount.c nsamount.c
LIBAUTH_OBJS = $(addprefix $(LIBAUTH_DIR)/, $(LIBAUTH_FILES:.c=.o))

libauth.a: $(LIBAUTH_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libauthsrv
LIBAUTHSRV_DIR = src/libauthsrv
LIBAUTHSRV_FILES = _asrequest.c _asgetticket.c _asrdresp.c authpak.c authdial.c convA2M.c \
	convM2A.c convM2PR.c convM2T.c convM2TR.c convPR2M.c convT2M.c convTR2M.c \
	form1.c nvcsum.c opasstokey.c passtokey.c readnvram.c
LIBAUTHSRV_OBJS = $(addprefix $(LIBAUTHSRV_DIR)/, $(LIBAUTHSRV_FILES:.c=.o))

libauthsrv.a: $(LIBAUTHSRV_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libndb
LIBNDB_DIR = src/libndb
LIBNDB_FILES = ipattr.o ndbaux.o ndbcache.o ndbcat.o ndbconcatenate.o \
	ndbdiscard.o ndbfree.o ndbgetipaddr.o ndbgetval.o ndbhash.o \
	ndbipinfo.o ndblookval.o ndbopen.o ndbparse.o ndbreorder.o \
	ndbsubstitute.o sysdnsquery.o
LIBNDB_OBJS = $(addprefix $(LIBNDB_DIR)/, $(LIBNDB_FILES))

libndb.a: $(LIBNDB_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libbio
LIBBIO_DIR = src/libbio
LIBBIO_FILES = bbuffered.o bfildes.o bflush.o bgetc.o bgetrune.o bgetd.o \
	binit.o boffset.o bprint.o bputc.o bputrune.o brdline.o brdstr.o \
	bread.o bseek.o bvprint.o bwrite.o
LIBBIO_OBJS = $(addprefix $(LIBBIO_DIR)/, $(LIBBIO_FILES))

libbio.a: $(LIBBIO_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# lib9p
LIB9P_DIR = src/lib9p
LIB9P_FILES = dirread.o fid.o file.o intmap.o mem.o req.o parse.o srv.o \
	tpost.o uid.o util.o
LIB9P_OBJS = $(addprefix $(LIB9P_DIR)/, $(LIB9P_FILES))

lib9p.a: $(LIB9P_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libthread
LIBTHREAD_DIR = src/libthread
LIBTHREAD_FILES = bg.o channel.o daemonize.o exec.o ioproc.o iorw.o \
	pthread.o ref.o thread.o wait.o
LIBTHREAD_OBJS = $(addprefix $(LIBTHREAD_DIR)/, $(LIBTHREAD_FILES))

libthread.a: $(LIBTHREAD_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# lib9pclient
LIB9PCLIENT_DIR = src/lib9pclient
LIB9PCLIENT_FILES = access.o auth.o close.o create.o dirread.o fs.o ns.o \
	open.o openfd.o print.o read.o remove.o seek.o stat.o walk.o write.o wstat.o
LIB9PCLIENT_OBJS = $(addprefix $(LIB9PCLIENT_DIR)/, $(LIB9PCLIENT_FILES))

lib9pclient.a: $(LIB9PCLIENT_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libip
LIBIP_DIR = src/libip
LIBIP_FILES = bo.o classmask.o eipfmt.o freeipifc.o ipaux.o \
	myetheraddr.o myipaddr.o parseether.o parseip.o ptclbsum.o udp.o
LIBIP_OBJS = $(addprefix $(LIBIP_DIR)/, $(LIBIP_FILES))

libip.a: $(LIBIP_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# libmux
LIBMUX_DIR = src/libmux
LIBMUX_FILES = io.o mux.o queue.o thread.o
LIBMUX_OBJS = $(addprefix $(LIBMUX_DIR)/, $(LIBMUX_FILES))

libmux.a: $(LIBMUX_OBJS)
	$(AR) $(ARFLAGS) $@ $^

# Commands
FACTOTUM_DIR = src/cmd/auth/factotum
FACTOTUM_FILES = apop.c chap.c dsa.c httpdigest.c p9any.c p9cr.c p9sk1.c pass.c rsa.c wep.c \
	attr.c confirm.c conv.c ctl.c fs.c key.c log.c main.c plan9.c pkcs1.c proto.c rpc.c secstore.c util.c xio.c
FACTOTUM_OBJS = $(addprefix $(FACTOTUM_DIR)/, $(FACTOTUM_FILES:.c=.o))

factotum: $(FACTOTUM_OBJS) $(LIBS)
	$(CC) $(CFLAGS) -o $@ $(FACTOTUM_OBJS) $(LIB_GROUP) $(LDFLAGS)

passwd: src/cmd/auth/passwd.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ src/cmd/auth/passwd.o $(LIB_GROUP) $(LDFLAGS)

netkey: src/cmd/netkey.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ src/cmd/netkey.o $(LIB_GROUP) $(LDFLAGS)

# Dependencies
src/libauthsrv/authpak.o: src/libauthsrv/authpak.c \
	src/libauthsrv/msqrt.mpc src/libauthsrv/decaf.mpc \
	src/libauthsrv/edwards.mpc src/libauthsrv/elligator2.mpc \
	src/libauthsrv/spake2ee.mpc src/libauthsrv/ed448.mpc

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(LIBS) $(CMDS) $(LIB9_OBJS) $(LIBMP_OBJS) $(LIBSEC_OBJS) $(LIBAUTH_OBJS) $(LIBAUTHSRV_OBJS) $(LIBNDB_OBJS) $(LIBBIO_OBJS) $(LIB9P_OBJS) $(LIBTHREAD_OBJS) $(LIB9PCLIENT_OBJS) $(LIBIP_OBJS) $(LIBMUX_OBJS) $(FACTOTUM_OBJS) src/cmd/auth/passwd.o src/cmd/netkey.o

.PHONY: all clean
