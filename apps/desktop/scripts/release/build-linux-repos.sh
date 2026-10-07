#!/usr/bin/env bash
# Builds GPG-signed APT and RPM repositories from the release packages (ADR-0029).
#
#   AEDST_GPG_PRIVATE_KEY="$(cat key.asc)" AEDST_GPG_PASSPHRASE=... \
#   scripts/release/build-linux-repos.sh --deb dist/aedst-desktop_1.6.0_amd64.deb \
#                                        --rpm dist/aedst-desktop-1.6.0.x86_64.rpm \
#                                        --out repo --suite stable
#
# Output:
#   repo/apt/pool/main/*.deb, repo/apt/dists/<suite>/{Release,InRelease,Release.gpg,main/binary-amd64/Packages(.gz)}
#   repo/rpm/*.rpm (each signed), repo/rpm/repodata/repomd.xml(.asc)
#   repo/aedst-archive-keyring.gpg (binary, for /usr/share/keyrings) and repo/aedst-archive-keyring.asc (for rpm --import)
#
# Clients pin the key, not the transport:
#   deb [signed-by=/usr/share/keyrings/aedst-archive-keyring.gpg] https://packages.aedst.com/apt stable main
#   gpgcheck=1, repo_gpgcheck=1, gpgkey=https://packages.aedst.com/aedst-archive-keyring.asc   (rpm)
#
# The key is used from a throw-away GNUPGHOME and never written anywhere else. Requires: gpg, apt-ftparchive (apt-utils),
# createrepo_c, rpm (rpmsign), gzip.
set -euo pipefail

debs=() rpms=() out="repo" suite="stable"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --deb) debs+=("$2"); shift 2 ;;
    --rpm) rpms+=("$2"); shift 2 ;;
    --out) out="$2"; shift 2 ;;
    --suite) suite="$2"; shift 2 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
[[ ${#debs[@]} -gt 0 || ${#rpms[@]} -gt 0 ]] || { echo "nothing to publish (--deb/--rpm)" >&2; exit 2; }
[[ -n "${AEDST_GPG_PRIVATE_KEY:-}" ]] || { echo "AEDST_GPG_PRIVATE_KEY (armored private key) is required" >&2; exit 1; }
[[ "$suite" =~ ^[a-z]+$ ]] || { echo "suite must be lowercase letters" >&2; exit 2; }

export GNUPGHOME
GNUPGHOME="$(mktemp -d)"
chmod 700 "$GNUPGHOME"
trap 'gpgconf --kill gpg-agent >/dev/null 2>&1 || true; rm -rf "$GNUPGHOME"' EXIT

printf '%s\n' "$AEDST_GPG_PRIVATE_KEY" | gpg --batch --quiet --import
key_fpr="$(gpg --batch --list-secret-keys --with-colons | awk -F: '/^fpr:/ {print $10; exit}')"
[[ -n "$key_fpr" ]] || { echo "no secret key was imported" >&2; exit 1; }

gpg_sign=(gpg --batch --yes --pinentry-mode loopback --local-user "$key_fpr")
if [[ -n "${AEDST_GPG_PASSPHRASE:-}" ]]; then
  gpg_sign+=(--passphrase-fd 3)
fi
sign() { # sign <gpg args...>; the passphrase goes through fd 3, never argv
  if [[ -n "${AEDST_GPG_PASSPHRASE:-}" ]]; then
    "${gpg_sign[@]}" "$@" 3<<<"$AEDST_GPG_PASSPHRASE"
  else
    "${gpg_sign[@]}" "$@"
  fi
}

mkdir -p "$out"
gpg --batch --export "$key_fpr" > "$out/aedst-archive-keyring.gpg"
gpg --batch --armor --export "$key_fpr" > "$out/aedst-archive-keyring.asc"

# ---- APT --------------------------------------------------------------------
if [[ ${#debs[@]} -gt 0 ]]; then
  apt_root="$out/apt"
  mkdir -p "$apt_root/pool/main" "$apt_root/dists/$suite/main/binary-amd64"
  cp -- "${debs[@]}" "$apt_root/pool/main/"
  (
    cd "$apt_root"
    apt-ftparchive packages pool/main > "dists/$suite/main/binary-amd64/Packages"
    gzip -9nkf "dists/$suite/main/binary-amd64/Packages"
    apt-ftparchive \
      -o "APT::FTPArchive::Release::Origin=AEDST" \
      -o "APT::FTPArchive::Release::Label=AEDST" \
      -o "APT::FTPArchive::Release::Suite=$suite" \
      -o "APT::FTPArchive::Release::Codename=$suite" \
      -o "APT::FTPArchive::Release::Architectures=amd64" \
      -o "APT::FTPArchive::Release::Components=main" \
      release "dists/$suite" > "dists/$suite/Release"
  )
  sign --clearsign --output "$apt_root/dists/$suite/InRelease" "$apt_root/dists/$suite/Release"
  sign --armor --detach-sign --output "$apt_root/dists/$suite/Release.gpg" "$apt_root/dists/$suite/Release"
fi

# ---- RPM --------------------------------------------------------------------
if [[ ${#rpms[@]} -gt 0 ]]; then
  rpm_root="$out/rpm"
  mkdir -p "$rpm_root"
  cp -- "${rpms[@]}" "$rpm_root/"
  # rpmsign needs the key name and a non-interactive passphrase command.
  rpmmacros="$GNUPGHOME/rpmmacros"
  {
    echo "%_gpg_name $key_fpr"
    echo "%_gpg_path $GNUPGHOME"
    if [[ -n "${AEDST_GPG_PASSPHRASE:-}" ]]; then
      echo "%__gpg_sign_cmd %{__gpg} gpg --batch --no-verbose --no-armor --pinentry-mode loopback --passphrase-fd 3 --no-secmem-warning -u \"%{_gpg_name}\" -sbo %{__signature_filename} --digest-algo sha256 %{__plaintext_filename}"
    fi
  } > "$rpmmacros"
  for pkg in "$rpm_root"/*.rpm; do
    if [[ -n "${AEDST_GPG_PASSPHRASE:-}" ]]; then
      rpmsign --load "$rpmmacros" --addsign "$pkg" 3<<<"$AEDST_GPG_PASSPHRASE"
    else
      rpmsign --load "$rpmmacros" --addsign "$pkg"
    fi
  done
  createrepo_c --general-compress-type=gz "$rpm_root"
  sign --armor --detach-sign --output "$rpm_root/repodata/repomd.xml.asc" "$rpm_root/repodata/repomd.xml"
fi

echo "repositories written to $out (signing key $key_fpr)"
