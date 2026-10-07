package security_test

import (
	"testing"

	"golang.org/x/crypto/bcrypt"

	"github.com/aedst/server/internal/modules/auth/infrastructure/security"
)

func TestArgon2idHasher_HashVerifyRoundTrip(t *testing.T) {
	h, err := security.NewArgon2idHasher()
	if err != nil {
		t.Fatal(err)
	}
	hash, err := h.Hash("correct horse battery staple")
	if err != nil {
		t.Fatal(err)
	}
	if !h.Verify(hash, "correct horse battery staple") || h.Verify(hash, "wrong") {
		t.Fatal("Verify() did not accept the right password and reject a wrong one")
	}
	if h.NeedsRehash(hash) {
		t.Error("a fresh hash must not need a rehash")
	}
}

func TestArgon2idHasher_LegacyBcryptVerifiesAndNeedsRehash(t *testing.T) {
	h, err := security.NewArgon2idHasher()
	if err != nil {
		t.Fatal(err)
	}
	legacy, err := bcrypt.GenerateFromPassword([]byte("old-password"), bcrypt.MinCost)
	if err != nil {
		t.Fatal(err)
	}
	if !h.Verify(string(legacy), "old-password") || h.Verify(string(legacy), "nope") {
		t.Fatal("legacy bcrypt hash verification broken")
	}
	if !h.NeedsRehash(string(legacy)) {
		t.Error("bcrypt hash must be flagged for upgrade")
	}
}

func TestArgon2idHasher_GarbageHashNeverVerifies(t *testing.T) {
	h, _ := security.NewArgon2idHasher()
	if h.Verify("not-a-hash", "x") || h.Verify("", "") {
		t.Fatal("malformed hash must not verify")
	}
}
