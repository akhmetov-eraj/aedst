package outbox

import (
	"context"

	"github.com/jackc/pgx/v5"
)

// Writer is what a module's repository calls inside its own transaction to
// record an integration event atomically with the state change. Delivery is
// not triggered here: writing the row is the whole commit-time cost (one
// INSERT, no queue job per event), and a Relay delivers it afterwards.
type Writer struct {
	repo Repository
}

// NewWriter builds a Writer that stores rows through repo.
func NewWriter(repo Repository) *Writer {
	return &Writer{repo: repo}
}

// Write inserts row inside tx.
func (w *Writer) Write(ctx context.Context, tx pgx.Tx, row Row) error {
	return w.repo.Insert(ctx, tx, row)
}
