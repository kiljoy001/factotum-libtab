# Installation Guide

## Quick Start

```bash
# Build
make -j4

# Test that it works
./factotum -?

# Install to user's bin directory
mkdir -p ~/bin
cp factotum passwd netkey ~/bin/
```

## Detailed Instructions

### 1. Prerequisites

#### Linux (Debian/Ubuntu)
```bash
sudo apt-get install build-essential
```

#### Linux (Fedora/RHEL)
```bash
sudo dnf install gcc make glibc-devel
```

#### macOS
```bash
# Install Xcode Command Line Tools
xcode-select --install
```

#### Other Unix systems
Ensure you have:
- gcc or compatible C compiler
- make
- POSIX threads library
- Standard C library

### 2. Build

```bash
# Clone or extract the source
cd factotum-dp9ik

# Build with multiple jobs for faster compilation
make -j4

# The build produces:
# - ./factotum
# - ./passwd
# - ./netkey
# - *.a library files
```

### 3. Test

```bash
# Check that factotum runs
./factotum -?

# Should output usage information
```

### 4. Install

Choose one of the following installation methods:

#### Option A: User installation (recommended)

Install to your personal bin directory:

```bash
mkdir -p ~/bin
cp factotum passwd netkey ~/bin/

# Make sure ~/bin is in your PATH
echo 'export PATH="$HOME/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

#### Option B: System-wide installation

Install for all users (requires root/sudo):

```bash
sudo cp factotum passwd netkey /usr/local/bin/
sudo chmod 755 /usr/local/bin/factotum /usr/local/bin/passwd /usr/local/bin/netkey
```

#### Option C: Custom location

```bash
PREFIX=/opt/plan9
mkdir -p $PREFIX/bin
cp factotum passwd netkey $PREFIX/bin/
export PATH="$PREFIX/bin:$PATH"
```

### 5. Configuration

#### Set up mount point

Factotum needs a place to mount its file system:

```bash
# Create mount point
mkdir -p ~/mnt/factotum

# Start factotum
factotum -m ~/mnt/factotum &

# Or use the default /mnt/factotum (may require permissions)
sudo mkdir -p /mnt/factotum
sudo chown $USER /mnt/factotum
factotum &
```

#### Add authentication keys

```bash
# Add a key for dp9ik authentication
echo "key proto=dp9ik dom=mydomain user=myuser !password=mysecret" > ~/mnt/factotum/ctl

# Or use p9any to support both dp9ik and p9sk1
echo "key proto=p9any dom=mydomain user=myuser !password=mysecret" > ~/mnt/factotum/ctl

# Verify the key was added
cat ~/mnt/factotum/ctl
# Output: key proto=dp9ik dom=mydomain user=myuser !password?
```

### 6. Integration with Plan 9 Port

If you have a full Plan 9 Port installation:

```bash
# You can use this factotum instead of the built-in one
# Option 1: Put it earlier in PATH
export PATH="$HOME/bin:$PLAN9/bin:$PATH"

# Option 2: Replace the Plan 9 Port version (backup first!)
cp $PLAN9/bin/factotum $PLAN9/bin/factotum.old
cp ./factotum $PLAN9/bin/factotum
```

## Verification

### Test basic functionality

```bash
# 1. Start factotum
factotum -m ~/mnt/factotum &

# 2. Add a test key
echo "key proto=pass service=test user=testuser !password=testpass" > ~/mnt/factotum/ctl

# 3. Query the key
cat ~/mnt/factotum/ctl
# Should show: key proto=pass service=test user=testuser !password?

# 4. List supported protocols
cat ~/mnt/factotum/proto
# Should include: dp9ik p9sk1 p9any apop chap cram mschap pass rsa wpapsk
```

### Test dp9ik support

```bash
# Check that dp9ik protocol is available
cat ~/mnt/factotum/proto | grep dp9ik

# Should output: dp9ik
```

## Troubleshooting

### Build fails with missing headers

**Problem**: `fatal error: thread.h: No such file or directory`

**Solution**: The headers are in the `include/` directory. Check that `CFLAGS` in the Makefile includes `-Iinclude`.

### Build fails with pthread errors

**Problem**: `undefined reference to 'pthread_create'`

**Solution**: Make sure `-lpthread` is in LDFLAGS. On some systems you may need additional flags.

### Cannot mount factotum

**Problem**: `mount: permission denied`

**Solution**:
- Use a mount point you own: `mkdir -p ~/mnt/factotum && factotum -m ~/mnt/factotum`
- Or create /mnt/factotum with proper permissions: `sudo mkdir -p /mnt/factotum && sudo chown $USER /mnt/factotum`

### factotum starts but files don't appear

**Problem**: Mount point is empty

**Solution**:
- Check if factotum is running: `ps aux | grep factotum`
- Check for errors: `factotum -d` (debug mode)
- Try a different mount point

### Authentication fails with dp9ik

**Problem**: Cannot authenticate to 9front server

**Solution**:
1. Verify the server supports dp9ik: `echo 'proto=p9any' > /mnt/factotum/rpc`
2. Check that you're using the correct domain: `echo 'key proto=dp9ik dom=? user=? !password=?' > /mnt/factotum/ctl`
3. Use p9any to auto-negotiate: `echo 'key proto=p9any dom=mydomain user=myuser !password=mypass' > /mnt/factotum/ctl`

## Platform-Specific Notes

### Linux

Works out of the box on most distributions. Tested on:
- Ubuntu 20.04, 22.04, 24.04
- Debian 11, 12
- Fedora 38+
- Arch Linux

### macOS

May require additional flags in Makefile:
```make
ifeq ($(shell uname), Darwin)
    LDFLAGS += -lresolv
endif
```

### FreeBSD

Tested on FreeBSD 13+. May need:
```make
ifeq ($(shell uname), FreeBSD)
    LDFLAGS += -lutil
endif
```

### Other Unix

Should work on most POSIX-compatible systems. You may need to adjust:
- Compiler flags for thread support
- Library paths for resolver and utility libraries

## Uninstalling

```bash
# Remove binaries
rm ~/bin/factotum ~/bin/passwd ~/bin/netkey

# Or for system-wide install
sudo rm /usr/local/bin/factotum /usr/local/bin/passwd /usr/local/bin/netkey

# Clean build artifacts
cd factotum-dp9ik
make clean
```

## Next Steps

- Read `README.md` for usage examples
- See `man/man4/factotum.4` for detailed documentation
- Check out the test suite in `src/cmd/auth/factotum/` for more examples

## Getting Help

- Check the README.md for common usage patterns
- Review the manual page: `man man/man4/factotum.4` or `nroff -man man/man4/factotum.4 | less`
- Look at test files for protocol examples
- Consult Plan 9 Port documentation at https://9fans.github.io/plan9port/
