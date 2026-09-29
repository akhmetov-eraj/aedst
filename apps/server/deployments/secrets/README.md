# Encrypted secrets

Secrets that must live in the repo (not in `.env`, which stays
local-only and untracked — see `.env.example`) are encrypted with
[SOPS](https://github.com/getsops/sops) using
[age](https://github.com/FiloSottile/age), configured in `.sops.yaml`.

## First-time setup

1. Generate a keypair (once, on your own machine):

   ```bash
   age-keygen -o key.txt
   ```

   This prints an `age1...` public key and writes the private key to
   `key.txt`. **Never commit `key.txt`** — keep it outside the repo, or in a
   password manager.

2. Open a PR adding your `age1...` public key to the `age:` list in
   `.sops.yaml` (comma-separated with existing keys). Once merged, anyone
   holding a previous private key re-encrypts for the new recipient list:

   ```bash
   sops updatekeys deployments/secrets/*.enc.yaml
   ```

3. Set `SOPS_AGE_KEY_FILE=/path/to/key.txt` (or `SOPS_AGE_KEY` with the raw
   key) in your shell so `sops` can decrypt without prompting.

## Day to day

```bash
task secrets:edit ENV=staging   # opens deployments/secrets/secrets.staging.enc.yaml decrypted in $EDITOR, re-encrypts on save
task secrets:view ENV=staging   # prints the decrypted content
```

Never run `sops` by hand against a file outside `deployments/secrets/` —
`.sops.yaml`'s `path_regex` only encrypts files matching
`secrets.*.enc.yaml` here; anything else silently stays plaintext.
