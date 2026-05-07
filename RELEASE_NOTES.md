# Release Notes - factotum-dp9ik 2026.04.24

## Overview

This is the initial standalone distribution of Plan 9 Port's factotum with dp9ik (modern 9front) authentication support.

## What's New

### Features

- **dp9ik protocol**: Full support for modern 9front authentication
  - AuthPAK password-authenticated key exchange
  - Ed448-Goldilocks elliptic curve cryptography
  - SPAKE2-EE protocol implementation
  - Modern cryptographic primitives

- **p9any negotiation**: Automatically negotiates between dp9ik and p9sk1
  - Tries dp9ik first for 9front servers
  - Falls back to p9sk1 for older Plan 9 servers
  - Seamless compatibility with both

- **Enhanced cryptography**:
  - ChaCha20-Poly1305 AEAD cipher
  - SHA-512 hash function
  - HKDF key derivation
  - PBKDF2 password-based key derivation
  - Constant-time field arithmetic in libmp

### Standalone Distribution

- Self-contained build system
- All dependencies included (lib9, libauth, libsec, libmp, etc.)
- No need for full plan9port installation
- Simple GNU Make build process
- Cross-platform support (Linux, macOS, *BSD)

### Testing

- Unit tests for dp9ik protocol
- Integration tests for factotum operations
- Live testing support against real auth servers
- Comprehensive test coverage

## Components

This distribution includes:

### Commands
- `factotum`: Authentication agent with dp9ik support
- `passwd`: Password change utility
- `netkey`: Network key management

### Libraries (all statically linked)
- lib9: Plan 9 C library
- libmp: Multi-precision arithmetic with constant-time operations
- libsec: Cryptographic library with modern ciphers
- libauthsrv: Authentication server protocol handling
- libauth: Authentication client interface
- libbio, lib9p, libthread, lib9pclient, libip, libmux, libndb

### Documentation
- README.md: Comprehensive overview
- QUICKSTART.md: 5-minute getting started guide
- INSTALL.md: Detailed installation instructions
- CONTRIBUTING.md: Contribution guidelines
- man/man4/factotum.4: Full manual page

### Development
- test.sh: Automated test suite
- Makefile: Complete build system
- .gitignore: Version control configuration

## Installation

```bash
# Build
make -j4

# Install to user directory
mkdir -p ~/bin
cp factotum passwd netkey ~/bin/

# Or system-wide (requires appropriate permissions)
cp factotum passwd netkey /usr/local/bin/
```

See INSTALL.md for detailed instructions.

## Usage

```bash
# Start factotum
factotum &

# Add a key for 9front server
echo "key proto=dp9ik dom=mydomain user=myuser !password=mypass" > /mnt/factotum/ctl

# Or use p9any for auto-negotiation
echo "key proto=p9any dom=mydomain user=myuser !password=mypass" > /mnt/factotum/ctl

# Verify protocols are available
cat /mnt/factotum/proto
```

See QUICKSTART.md for more examples.

## Platform Support

### Tested Platforms
- Linux (Ubuntu 20.04, 22.04, 24.04)
- Linux (Debian 11, 12)
- Linux (Fedora 38+)
- Linux (Arch Linux)

### Should Work On
- macOS (with Xcode Command Line Tools)
- FreeBSD 13+
- OpenBSD 7+
- Other POSIX-compliant Unix systems

## Technical Details

### Based On
- Plan 9 Port commit: 7f03429239e19bdab0be36f39a24ce24054ae12c
- Date: 2026-04-24
- PR: https://github.com/9fans/plan9port/pull/786

### Protocol Implementation
The dp9ik implementation includes cryptographic code adapted from:
- 9front (http://9front.org/)
- drawterm authentication code

All code remains under the MIT/Lucent license.

### Changes from plan9port
- Added dp9ik protocol support
- Updated p9any to negotiate dp9ik
- Imported modern cryptographic primitives
- Extended libmp with constant-time operations
- Enhanced libsec with ChaCha20-Poly1305, SHA-512, HKDF, PBKDF2
- Comprehensive test suite

See git log for detailed commit history.

## Known Issues

- Test suite requires 9P support for full key operation tests
- FUSE mounting may require additional setup on some systems
- Live tests require access to a 9front auth server

## Compatibility

### Forward Compatibility
- Works with 9front auth servers (dp9ik)
- Works with modern Plan 9 auth servers (p9sk1)
- Compatible with plan9port programs

### Backward Compatibility
- Maintains full p9sk1 support
- Compatible with older Plan 9 systems
- Does not break existing authentication

## Security Notes

This implementation includes modern cryptographic primitives and follows the 9front authentication design. However:

- Always use strong passwords
- Keep factotum's mount point secure (mode 0700)
- Protect key files with appropriate permissions
- Use encrypted connections when possible
- Review security best practices in the manual

## Future Plans

This standalone distribution will:
- Track upstream plan9port dp9ik changes
- Accept build system improvements
- Support additional platforms
- Improve documentation
- Maintain compatibility

For protocol-level changes, contribute to the upstream PR.

## Credits

- **Plan 9 Port**: Russ Cox and contributors
- **dp9ik implementation**: Scott J Guyton
- **9front crypto**: 9front developers
- **Original Plan 9**: Bell Labs

## Links

- [Plan 9 Port](https://9fans.github.io/plan9port/)
- [Upstream PR](https://github.com/9fans/plan9port/pull/786)
- [9front](http://9front.org/)
- [Plan 9 Authentication](http://man.cat-v.org/9front/8/authsrv)

## License

MIT License (same as Plan 9 Port)

See LICENSE file for full details.

---

## Checksums

```
# To be added for release tarball
# sha256sum factotum-dp9ik-2026.04.24.tar.gz
```

## Support

- File issues on the project repository
- Check INSTALL.md for troubleshooting
- Consult Plan 9 Port documentation
- For protocol issues, see the upstream PR

Thank you for using factotum-dp9ik!
