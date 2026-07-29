# factotum-dp9ik

Standalone distribution of Plan 9 Port's `factotum` authentication agent with dp9ik (modern 9front auth) support.

This is a self-contained build of the factotum changes from [plan9port PR](https://github.com/9fans/plan9port/pull/786), extracted for use by people who need dp9ik authentication support without waiting for upstream merge.

## What is this?

This package provides:
- **factotum**: Authentication agent supporting p9sk1, dp9ik, and p9any negotiation
- **passwd**: Password changing utility
- **netkey**: Network key management utility
- **Monocypher-backed signing**: Native libtab EdDSA+BLAKE2b signing support
- **tpm9p proofs**: Factotum protocol for tpm9p/libtab user authorization proofs
- **OAuth/OAuth2 token storage**: Factotum-managed bearer token retrieval

All Plan 9 dependencies (lib9, libauth, libsec, etc.) are included and built statically.

## What is dp9ik?

dp9ik is the modern authentication protocol used by 9front, implementing:
- AuthPAK (Password Authenticated Key exchange)
- Ed448-Goldilocks elliptic curve cryptography
- SPAKE2-EE (Elliptic curve Edition)
- Modern cryptographic primitives (ChaCha20-Poly1305, SHA-512, HKDF)

This allows plan9port systems to authenticate against modern 9front auth servers.

## Installation

### Prerequisites

- gcc
- make
- pthread library
- Linux: util and resolv libraries (usually installed by default)

### Building

```bash
make -j4
```

This builds all libraries and commands. The resulting binaries are:
- `./factotum`
- `./passwd`
- `./netkey`

### Installing

Copy the binaries to your preferred location:

```bash
# Example: install to ~/bin
cp factotum passwd netkey ~/bin/

# Or system-wide (requires appropriate permissions)
cp factotum passwd netkey /usr/local/bin/
```

## Usage

### Starting factotum

```bash
# Start with default settings
factotum

# Start with specific auth server
factotum -a tcp!auth.mydomain.com!567

# Debug mode (print authentication details)
factotum -d

# Mount at specific location
factotum -m /mnt/factotum
```

### Adding keys

Keys can be added via the `ctl` file:

```bash
# Add a p9sk1 key
echo "key proto=p9sk1 dom=example.com user=glenda !password=secret" > /mnt/factotum/ctl

# Add a dp9ik key
echo "key proto=dp9ik dom=example.com user=glenda !password=secret" > /mnt/factotum/ctl

# Add a key that works for both protocols
echo "key proto=p9any dom=example.com user=glenda !password=secret" > /mnt/factotum/ctl

# Add a libtab-compatible Monocypher signing key.
# !seed may be 32 bytes encoded as hex or padded base64url.
echo "key proto=monocypher user=alice !seed=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" > /mnt/factotum/ctl

# Add the same key for tpm9p policy proofs.
echo "key proto=tpm9p authdom=rentonsoftworks.coin user=alice !seed=000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f" > /mnt/factotum/ctl

# Store an OAuth2 access token in factotum.
echo "key proto=oauth2 service=keycloak issuer=https://sso.example.com user=alice !access_token=ey..." > /mnt/factotum/ctl
```

### Querying keys

```bash
# List all keys (secrets are shown as attribute?)
cat /mnt/factotum/ctl

# Get specific key
factotum -g "proto=dp9ik dom=example.com user=glenda !password?"
```

### Protocols supported

- **monocypher**: Monocypher-backed signing. The first supported algorithm is
  `libtab-eddsa-blake2b-v1`, which is Monocypher `crypto_eddsa_*`
  over Edwards25519 with BLAKE2b. It is intentionally not RFC8032 Ed25519.
- **tpm9p**: Creates current tpm9p authorization proof cells. The client writes
  a server challenge and reads a libtab `signed:<body>:<sig>` cell for
  `tpm9p-user-auth-v1`.
- **oauth/oauth2**: Stores access tokens in factotum private attributes and
  returns raw or `Bearer ...` tokens for clients that need an identity-layer
  bridge.
- **p9any**: Meta-protocol that negotiates dp9ik or p9sk1 automatically
- **dp9ik**: Modern 9front authentication protocol
- **p9sk1**: Classic Plan 9 shared-key authentication
- **apop**: APOP email authentication
- **chap**: CHAP authentication
- **cram**: CRAM authentication
- **mschap**: MS-CHAP authentication
- **pass**: Plain password authentication
- **rsa**: RSA public-key authentication
- **wpapsk**: WPA-PSK authentication

### Monocypher RPC examples

The generic Monocypher protocol signs bytes and returns a raw 64-byte
signature:

```text
start proto=monocypher role=sign user=alice
write <message bytes>
read
```

Use `role=sigcell` to receive the released libtab signed-cell format instead:

```text
signed:<base64url-body>:<base64url-signature>
```

Use `role=public` to read the 32-byte public key, and `role=verify` to verify a
message/signature pair.

### tpm9p RPC example

```text
start proto=tpm9p role=client authdom=rentonsoftworks.coin user=alice
write <16+ byte challenge from tpm9p>
read
```

The response is a libtab signed cell whose body is:

```text
tpm9p-user-auth-v1 NUL authdom NUL user NUL challenge
```

### OAuth/OAuth2 RPC examples

```text
start proto=oauth2 role=access service=keycloak user=alice
read

start proto=oauth2 role=bearer service=keycloak user=alice
read
```

`role=access` returns the raw access token. `role=bearer` returns an HTTP
Authorization header value. `role=refresh` returns `!refresh_token` only when
that private attribute is present.

## Testing

The build includes test utilities:

```bash
# Build the dp9ik unit tests
cd src/libauthsrv
make dp9iktest
./dp9iktest

# For live testing against a 9front auth server, see:
# src/cmd/auth/factotum/dp9iklive.sh
```

## Documentation

See `man/man4/factotum.4` for the full manual page.

Key concepts:
- **Keys**: Stored credentials with attributes (proto, dom, user, password, etc.)
- **RPC interface**: Programs authenticate by writing to `/mnt/factotum/rpc`
- **Confirmation**: Some operations may require user confirmation via `/mnt/factotum/confirm`

## Architecture

This standalone build includes:

**Libraries:**
- `lib9`: Plan 9 C library (fmt, utf, etc.)
- `libmp`: Multi-precision arithmetic
- `libsec`: Cryptographic primitives (AES, ChaCha20, SHA-2, etc.)
- `libauthsrv`: Authentication server protocol handling
- `libauth`: Authentication client interface
- `libbio`: Buffered I/O
- `lib9p`: 9P protocol implementation
- `libthread`: Plan 9 threading library
- `lib9pclient`: 9P client library
- `libip`: IP address parsing and formatting
- `libmux`: Multiplexing I/O
- `libndb`: Network database parsing

**Commands:**
- `factotum`: Main authentication agent
- `passwd`: Change password on auth server
- `netkey`: Manage network authentication keys

## Changes from plan9port

This standalone distribution includes the dp9ik patches from commit `7f034292`:

- Added dp9ik protocol support in factotum
- Updated p9any to negotiate dp9ik when available
- Imported AuthPAK, Ed448, SPAKE2-EE crypto from 9front
- Added ChaCha20-Poly1305, SHA-512, HKDF, PBKDF2 to libsec
- Added Monocypher as a libsec component for libtab/tpm9p signing protocols
- Added `monocypher`, `tpm9p`, `oauth`, and `oauth2` factotum protocols
- Extended libmp with constant-time field operations
- Updated authsrv structures for form1 authentication
- Added comprehensive test coverage

See commit message and `git show 7f034292` in the plan9port repo for full details.

## Compatibility

- **Authentication**: Works with both 9front auth servers (dp9ik) and older Plan 9 auth servers (p9sk1)
- **Systems**: Linux (primary target), should work on other Unix-like systems with minor Makefile adjustments
- **Plan 9 Port**: Can coexist with a full plan9port installation, but don't mix binaries

## Development

### Project structure

```
factotum-dp9ik/
├── Makefile           # Main build file
├── include/           # Headers (auth.h, authsrv.h, libsec.h, mp.h, etc.)
├── src/               # Source code
│   ├── lib9/          # Plan 9 C library
│   ├── libmp/         # Multi-precision math
│   ├── libsec/        # Cryptography
│   ├── libauthsrv/    # Auth server protocol
│   ├── libauth/       # Auth client
│   ├── lib*.../       # Other libraries
│   └── cmd/auth/      # Commands (factotum, passwd, netkey)
└── man/               # Manual pages
```

### Cleaning

```bash
make clean
```

### Rebuilding

```bash
make clean && make -j4
```

## License

This code is derived from Plan 9 Port, which is derived from Plan 9 from Bell Labs.

See the Plan 9 license (Lucent Public License) in the plan9port distribution for details.

The dp9ik protocol implementation incorporates code adapted from 9front and drawterm.

## Credits

- **Plan 9 Port**: Russ Cox and contributors
- **dp9ik implementation**: Original patch by Scott J Guyton
- **9front crypto**: 9front developers (AuthPAK, Ed448, etc.)
- **Original Plan 9**: Bell Labs

## Links

- [Plan 9 Port](https://9fans.github.io/plan9port/)
- [Original PR](https://github.com/9fans/plan9port/pull/786)
- [9front](http://9front.org/)
- [Plan 9 Authentication](http://man.cat-v.org/9front/8/authsrv)

## Support

This is an unofficial standalone distribution. For issues:

1. Check if the issue exists in the original PR
2. File issues related to the standalone build in this repository
3. For general factotum questions, consult Plan 9 Port documentation

## See Also

- `factotum(4)` - The factotum manual page
- Plan 9 Port's auth documentation
- 9front's authsrv(6) and dp9ik documentation
