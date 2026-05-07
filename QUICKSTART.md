# Quick Start Guide

Get up and running with factotum-dp9ik in 5 minutes.

## 1. Build (30 seconds)

```bash
make -j4
```

Expected output: Compiles cleanly, produces `factotum`, `passwd`, `netkey` binaries.

## 2. Install (10 seconds)

```bash
mkdir -p ~/bin
cp factotum passwd netkey ~/bin/
export PATH="$HOME/bin:$PATH"
```

## 3. Test (30 seconds)

```bash
# Create mount point
mkdir -p ~/mnt/factotum

# Start factotum
factotum -m ~/mnt/factotum &

# Verify it's running
ls ~/mnt/factotum
# Expected: confirm  ctl  log  needkey  proto  rpc

# Check for dp9ik support
cat ~/mnt/factotum/proto | grep dp9ik
# Expected: dp9ik
```

## 4. Add a Key (30 seconds)

```bash
# For 9front servers (dp9ik)
echo "key proto=dp9ik dom=your-domain user=yourname !password=yourpass" > ~/mnt/factotum/ctl

# Or use p9any for auto-negotiation (dp9ik or p9sk1)
echo "key proto=p9any dom=your-domain user=yourname !password=yourpass" > ~/mnt/factotum/ctl

# Verify
cat ~/mnt/factotum/ctl
# Expected: key proto=dp9ik dom=your-domain user=yourname !password?
```

## 5. Use It

Now any Plan 9 programs can authenticate through factotum:

```bash
# Example: 9p mount
9 mount 'tcp!server!564' /n/remote

# factotum will handle authentication automatically
```

## Common Scenarios

### Connecting to a 9front Server

```bash
# Start factotum
factotum &

# Add 9front credentials
echo "key proto=dp9ik dom=9front.local user=glenda !password=secret" > /mnt/factotum/ctl

# Use with any Plan 9 program
9 srv tcp!9front.local!564 remote
9 mount /srv/remote /n/remote
```

### Connecting to an Old Plan 9 Server

```bash
# Start factotum
factotum &

# Add p9sk1 credentials
echo "key proto=p9sk1 dom=plan9.local user=glenda !password=secret" > /mnt/factotum/ctl

# Or use p9any to auto-detect
echo "key proto=p9any dom=plan9.local user=glenda !password=secret" > /mnt/factotum/ctl
```

### Running with Plan 9 Port

If you have full plan9port installed:

```bash
# Use factotum-dp9ik by putting it first in PATH
export PATH="$HOME/bin:$PLAN9/bin:$PATH"

# Or set FACTOTUM environment variable
export FACTOTUM="$HOME/bin/factotum"
```

## Troubleshooting

### "Cannot mount"

```bash
# Check if mount point exists
ls ~/mnt/factotum

# If not, create it
mkdir -p ~/mnt/factotum

# Try with sudo for /mnt/factotum
sudo mkdir -p /mnt/factotum
sudo chown $USER /mnt/factotum
```

### "dp9ik not in proto list"

```bash
# Rebuild from scratch
make clean && make -j4

# Check build output for errors
```

### "Authentication failed"

```bash
# Run in debug mode
factotum -d

# Check logs
cat /mnt/factotum/log

# Verify key is correct
cat /mnt/factotum/ctl
```

## What's Next?

- Read [README.md](README.md) for detailed information
- See [INSTALL.md](INSTALL.md) for installation options
- Check [man/man4/factotum.4](man/man4/factotum.4) for the manual

## Quick Reference

### Common Commands

```bash
# List protocols
cat /mnt/factotum/proto

# List keys
cat /mnt/factotum/ctl

# Add key
echo "key proto=... dom=... user=... !password=..." > /mnt/factotum/ctl

# Remove key
echo "delkey proto=... dom=... user=..." > /mnt/factotum/ctl

# View log
cat /mnt/factotum/log

# Stop factotum
killall factotum
```

### Key Attributes

- `proto=`: Protocol (dp9ik, p9sk1, p9any, etc.)
- `dom=`: Authentication domain
- `user=`: Username
- `!password=`: Password (! marks it as secret)
- `server=`: Server address (optional)
- `service=`: Service name (optional)

### Protocols

- `p9any`: Auto-negotiate (tries dp9ik first, falls back to p9sk1)
- `dp9ik`: Modern 9front protocol
- `p9sk1`: Classic Plan 9 protocol
- `pass`: Plain password
- `apop`, `chap`, `cram`, `mschap`: Various challenge-response
- `rsa`: RSA public-key
- `wpapsk`: WPA-PSK for wireless

## Example: Complete Setup

```bash
# 1. Build and install
cd factotum-dp9ik
make -j4
mkdir -p ~/bin
cp factotum passwd netkey ~/bin/

# 2. Add to PATH (add to ~/.bashrc for persistence)
export PATH="$HOME/bin:$PATH"

# 3. Create mount point
mkdir -p ~/mnt/factotum

# 4. Start factotum
factotum -m ~/mnt/factotum &

# 5. Add keys for different servers
echo "key proto=p9any dom=9front.server user=glenda !password=9front_pass" > ~/mnt/factotum/ctl
echo "key proto=p9any dom=plan9.server user=glenda !password=plan9_pass" > ~/mnt/factotum/ctl

# 6. Test
cat ~/mnt/factotum/ctl

# Done! Now all Plan 9 programs can authenticate.
```

## Getting Help

- Check [README.md](README.md) for detailed documentation
- See [INSTALL.md](INSTALL.md) for installation troubleshooting
- Read the man page: `man man/man4/factotum.4`
- File issues on the project repository

---

That's it! You now have a working factotum with dp9ik support.
