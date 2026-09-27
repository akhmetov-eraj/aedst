// Package architecturetest statically enforces the module boundary rules
// documented in docs/architecture.md and docs/decisions/0006-module-boundaries.md.
// A violation here means a module extraction (docs/architecture.md#extraction-path)
// would no longer be a mechanical move — this test exists to keep that
// property true, so it must run in CI on every change.
package architecturetest

import (
	"fmt"
	"strings"
	"testing"

	"golang.org/x/tools/go/packages"
)

const modulePrefix = "github.com/aedst/server/internal/modules/"

func TestModuleBoundaries(t *testing.T) {
	pkgs := loadPackages(t)

	for _, pkg := range pkgs {
		ownerModule := moduleOf(pkg.PkgPath)

		for _, imp := range pkg.Imports {
			importedModule := moduleOf(imp.PkgPath)
			if importedModule == "" || importedModule == ownerModule {
				continue // not a module import, or importing its own module: always fine.
			}

			layer := layerOf(imp.PkgPath, importedModule)
			if layer == "api" || layer == "" {
				continue // the public boundary (or the module root) is always importable.
			}

			t.Errorf(
				"boundary violation: %s imports %s directly — modules may only depend on another module's api/ package (see docs/decisions/0006-module-boundaries.md)",
				pkg.PkgPath, imp.PkgPath,
			)
		}
	}
}

// TestDomainDoesNotImportInfrastructure enforces layering within a module:
// domain must never depend on infrastructure (see docs/modules.md "Module
// internal structure").
func TestDomainDoesNotImportInfrastructure(t *testing.T) {
	pkgs := loadPackages(t)

	for _, pkg := range pkgs {
		module := moduleOf(pkg.PkgPath)
		if module == "" || layerOf(pkg.PkgPath, module) != "domain" {
			continue
		}

		for _, imp := range pkg.Imports {
			if moduleOf(imp.PkgPath) == module && layerOf(imp.PkgPath, module) == "infrastructure" {
				t.Errorf("layering violation: %s (domain) imports %s (infrastructure)", pkg.PkgPath, imp.PkgPath)
			}
		}
	}
}

func loadPackages(t *testing.T) []*packages.Package {
	t.Helper()

	cfg := &packages.Config{Mode: packages.NeedName | packages.NeedImports | packages.NeedDeps}
	pkgs, err := packages.Load(cfg, "../internal/...")
	if err != nil {
		t.Fatalf("load packages: %v", err)
	}
	if packages.PrintErrors(pkgs) > 0 {
		t.Fatalf("packages failed to load cleanly")
	}
	return pkgs
}

// moduleOf returns the module name ("auth", "users", ...) for a package path
// under internal/modules/, or "" if pkgPath isn't a module package.
func moduleOf(pkgPath string) string {
	if !strings.HasPrefix(pkgPath, modulePrefix) {
		return ""
	}
	rest := strings.TrimPrefix(pkgPath, modulePrefix)
	parts := strings.SplitN(rest, "/", 2)
	return parts[0]
}

// layerOf returns the top-level layer directory of pkgPath within module
// (e.g. "domain", "api"), or "" if pkgPath is the module's root package.
func layerOf(pkgPath, module string) string {
	prefix := fmt.Sprintf("%s%s/", modulePrefix, module)
	if !strings.HasPrefix(pkgPath, prefix) {
		return ""
	}
	rest := strings.TrimPrefix(pkgPath, prefix)
	parts := strings.SplitN(rest, "/", 2)
	return parts[0]
}
