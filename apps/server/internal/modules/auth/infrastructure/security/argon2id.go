// Package security implements application.PasswordHasher with argon2id
// (OWASP / RFC 9106 recommended password hash), while still verifying the
// bcrypt hashes written by earlier versions so no user is locked out;
// application.Service upgrades them transparently on the next successful
// login (NeedsRehash).
package security

import (
	"crypto/rand"
	"strings"

	"github.com/alexedwards/argon2id"
	"golang.org/x/crypto/bcrypt"
)

// params follow RFC 9106 §4 "second recommended option" (t=3, p=4, 64 MiB),
// which is also above the OWASP Password Storage Cheat Sheet minimums. Raise
// Memory/Iterations as hardware allows; NeedsRehash upgrades stored hashes.
var params = &argon2id.Params{
	Memory:      64 * 1024, // KiB
	Iterations:  3,
	Parallelism: 4,
	SaltLength:  16,
	KeyLength:   32,
}

// maxConcurrentHashes bounds simultaneous argon2id computations. Each holds
// 64 MiB, so an unbounded burst of login attempts would otherwise be a
// memory-exhaustion vector (independent of the request rate limits).
const maxConcurrentHashes = 8

type Argon2idHasher struct {
	slots     chan struct{}
	dummyHash string
}

// NewArgon2idHasher builds a hasher and precomputes a throwaway hash used to
// equalise timing for unknown accounts.
func NewArgon2idHasher() (*Argon2idHasher, error) {
	dummy, err := argon2id.CreateHash(rand.Text(), params)
	if err != nil {
		return nil, err
	}
	return &Argon2idHasher{slots: make(chan struct{}, maxConcurrentHashes), dummyHash: dummy}, nil
}

func (h *Argon2idHasher) acquire() func() {
	h.slots <- struct{}{}
	return func() { <-h.slots }
}

func (h *Argon2idHasher) Hash(password string) (string, error) {
	defer h.acquire()()
	return argon2id.CreateHash(password, params)
}

// Verify checks password against hash in constant time (the comparison is
// done by the argon2id library with crypto/subtle). Legacy bcrypt hashes are
// accepted.
func (h *Argon2idHasher) Verify(hash, password string) bool {
	if isBcrypt(hash) {
		return bcrypt.CompareHashAndPassword([]byte(hash), []byte(password)) == nil
	}
	defer h.acquire()()
	ok, err := argon2id.ComparePasswordAndHash(password, hash)
	return err == nil && ok
}

// NeedsRehash is true for bcrypt hashes and for argon2id hashes using
// parameters other than the current ones.
func (h *Argon2idHasher) NeedsRehash(hash string) bool {
	if isBcrypt(hash) {
		return true
	}
	p, _, _, err := argon2id.DecodeHash(hash)
	if err != nil {
		return true
	}
	return p.Memory != params.Memory || p.Iterations != params.Iterations || p.Parallelism != params.Parallelism || p.KeyLength != params.KeyLength
}

func (h *Argon2idHasher) VerifyDummy(password string) {
	defer h.acquire()()
	_, _ = argon2id.ComparePasswordAndHash(password, h.dummyHash)
}

func isBcrypt(hash string) bool {
	return strings.HasPrefix(hash, "$2a$") || strings.HasPrefix(hash, "$2b$") || strings.HasPrefix(hash, "$2y$")
}
