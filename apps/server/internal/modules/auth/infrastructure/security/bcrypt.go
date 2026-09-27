// Package security implements application.PasswordHasher with bcrypt.
package security

import "golang.org/x/crypto/bcrypt"

const cost = bcrypt.DefaultCost

type BcryptHasher struct{}

func NewBcryptHasher() BcryptHasher { return BcryptHasher{} }

func (BcryptHasher) Hash(password string) (string, error) {
	hash, err := bcrypt.GenerateFromPassword([]byte(password), cost)
	if err != nil {
		return "", err
	}
	return string(hash), nil
}

func (BcryptHasher) Verify(hash, password string) bool {
	return bcrypt.CompareHashAndPassword([]byte(hash), []byte(password)) == nil
}
