# Contributing to factotum-dp9ik

Thank you for your interest in contributing to the standalone factotum-dp9ik distribution!

## About This Project

This is a standalone extraction of the dp9ik authentication support added to Plan 9 Port. The goal is to provide a self-contained, easy-to-build version for people who need dp9ik support now.

This is NOT a fork of plan9port - it's a focused distribution of specific changes that are awaiting upstream merge.

## Relationship to Upstream

- **Upstream PR**: https://github.com/9fans/plan9port/pull/786
- This distribution tracks the dp9ik changes from the above PR
- If the PR is merged upstream, this standalone build will remain useful for:
  - Systems that need just factotum without full plan9port
  - People who want to track upstream at a different pace
  - Minimal installations

## Ways to Contribute

### 1. Testing

The most valuable contribution is testing on different platforms:

- Test on your Linux distribution
- Test on macOS
- Test on *BSD systems
- Test authentication against 9front servers
- Test backward compatibility with old Plan 9 auth servers

**How to report test results:**

```markdown
Platform: Ubuntu 22.04 x86_64
Compiler: gcc 11.3.0
Build: Success
Runtime: Success
Authentication: dp9ik to 9front server - Success
Notes: All tests passed
```

### 2. Bug Fixes

If you find bugs in the standalone build (not the underlying protocol):

1. Check if the bug exists in the original PR
2. If it's build-system specific, file an issue here
3. If it's protocol-level, report to the upstream PR

**Types of bugs we handle here:**
- Build system issues (Makefile)
- Platform-specific compilation problems
- Standalone packaging issues
- Documentation errors

**Types of bugs for upstream:**
- Protocol implementation bugs
- Authentication failures
- Memory leaks or crashes
- Security issues

### 3. Documentation

Improvements to documentation are always welcome:

- Clarify installation instructions
- Add platform-specific notes
- Document common issues
- Add usage examples
- Improve README clarity

### 4. Build System

Improvements to the Makefile and build process:

- Better platform detection
- Support for additional compilers
- Optimized build flags
- Parallel build improvements
- Cross-compilation support

### 5. Packaging

Help package factotum-dp9ik for distributions:

- Debian/Ubuntu packages
- Fedora/RHEL RPMs
- Arch AUR packages
- Homebrew formulas
- BSD ports

## Development Setup

### Getting the code

```bash
# If this is a git repo
git clone <repo-url>
cd factotum-dp9ik

# Or if you're working from a tarball
tar xzf factotum-dp9ik-*.tar.gz
cd factotum-dp9ik-*
```

### Building for development

```bash
# Clean build
make clean
make -j4

# Build with debug symbols
make CFLAGS="-g -O0 -Wall -Iinclude -Isrc/lib9 -Isrc/libthread -DPLAN9PORT -std=gnu11"

# Run tests
cd src/libauthsrv
make dp9iktest
./dp9iktest
```

### Testing changes

Before submitting changes:

1. Build cleanly: `make clean && make`
2. Test basic functionality: `./factotum -?`
3. Test protocol list: Start factotum and check `cat /mnt/factotum/proto`
4. Test key operations: Add/query/delete keys
5. If possible, test actual authentication

## Submitting Changes

### For Issues

Include:
- Platform (OS, version, architecture)
- Compiler (gcc version, clang version, etc.)
- Build output (error messages)
- Steps to reproduce
- Expected vs actual behavior

### For Pull Requests

1. **Small focused changes**: One fix per PR
2. **Test your changes**: Ensure it builds and works
3. **Document**: Update README/docs if needed
4. **Commit message format**:
   ```
   component: brief description

   Longer explanation of what changed and why.

   Fixes: #issue-number (if applicable)
   ```

5. **Sign your work**: We follow the Developer Certificate of Origin (DCO)
   ```bash
   git commit -s -m "your message"
   ```

### Examples of good PRs

**Good:**
- "Makefile: add FreeBSD support"
- "README: clarify installation on macOS"
- "Fix compilation warning on gcc 13"

**Not appropriate for this repo:**
- "Fix dp9ik authentication bug" → upstream PR
- "Add SSH support" → upstream feature request
- "Refactor entire codebase" → too large, not focused

## Code Style

Since this is Plan 9-derived code, we follow Plan 9 conventions:

- Tab indentation (width 8)
- No trailing whitespace
- Opening brace on same line for functions
- K&R style
- Minimal comments (code should be clear)

For C code:
```c
void
function(int arg)
{
	if(condition){
		statement;
		statement;
	}
}
```

For Makefiles:
- Use tabs for recipe indentation
- Clear variable names
- Comment complex logic

## Upstream Synchronization

When the upstream PR changes:

1. Someone with access to both repos will pull changes
2. Test that the standalone build still works
3. Update version/commit references in README
4. Tag a new release if significant

You can help by:
- Monitoring the upstream PR for changes
- Testing updated versions
- Reporting integration issues

## Release Process

Releases are tagged when:
- Significant upstream changes are merged
- Important build fixes are made
- Platform support is added

Version format: `YYYY.MM.DD` based on upstream commit date

## Communication

- **Issues**: For bugs and feature requests
- **Discussions**: For questions and ideas
- **Pull Requests**: For code changes
- **Upstream PR**: For protocol-level issues

## Legal

By contributing, you agree that your contributions will be licensed under the same MIT license as the rest of the project (see LICENSE file).

Please include a sign-off in your commits:
```bash
git commit -s
```

This indicates you agree to the Developer Certificate of Origin:
```
Developer Certificate of Origin
Version 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```

## Thank You!

Every contribution, no matter how small, helps make factotum-dp9ik better for everyone.

Special thanks to:
- Russ Cox and the Plan 9 Port maintainers
- The 9front developers for the dp9ik protocol
- Everyone who has tested and reported issues
