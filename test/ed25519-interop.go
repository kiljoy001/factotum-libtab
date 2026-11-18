// Run from the repository root with: go run test/ed25519-interop.go
package main

import (
	"bytes"
	"crypto/ed25519"
	"crypto/rand"
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
)

func main() {
	if err := run(); err != nil {
		fmt.Fprintln(os.Stderr, err)
		os.Exit(1)
	}
}
func run() error {
	dir, err := os.MkdirTemp("", "factotum-ed25519-interop-")
	if err != nil {
		return err
	}
	defer os.RemoveAll(dir)
	cc := os.Getenv("CC")
	if cc == "" {
		cc = "cc"
	}
	bin := filepath.Join(dir, "interop")
	cmd := exec.Command(cc, "-std=c99", "-O2", "-Iinclude", "test/ed25519-interop.c", "src/libsec/port/monocypher.c", "src/libsec/port/monocypher-ed25519.c", "-o", bin)
	if out, err := cmd.CombinedOutput(); err != nil {
		return fmt.Errorf("build: %w\n%s", err, out)
	}
	lengths := []int{0, 1, 2, 31, 32, 63, 64, 111, 112, 127, 128, 129, 255, 256, 1023, 8192, 16384}
	for _, n := range lengths {
		seed := make([]byte, ed25519.SeedSize)
		msg := make([]byte, n)
		if _, err := rand.Read(seed); err != nil {
			return err
		}
		if _, err := rand.Read(msg); err != nil {
			return err
		}
		sk := ed25519.NewKeyFromSeed(seed)
		pk := sk.Public().(ed25519.PublicKey)
		sig := ed25519.Sign(sk, msg)
		input := append(append(append([]byte{}, seed...), sig...), msg...)
		cmd := exec.Command(bin)
		cmd.Stdin = bytes.NewReader(input)
		out, err := cmd.Output()
		if err != nil {
			return fmt.Errorf("C verification length %d: %w", n, err)
		}
		if len(out) != 96 || !bytes.Equal(out[:32], pk) || !bytes.Equal(out[32:], sig) || !ed25519.Verify(pk, msg, out[32:]) {
			return fmt.Errorf("interop mismatch at length %d", n)
		}
	}
	fmt.Printf("Ed25519 C/Go key derivation, signing and bidirectional verification passed (%d lengths)\n", len(lengths))
	return nil
}
