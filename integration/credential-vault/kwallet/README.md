# Native credential-vault presentation

This downstream integration changes KWallet's native dialogs only for the exact
wallet name `Aero7 Credentials`. The existing optional Credential Vault keeps
using KWallet encryption and Secret Service; it does not collect a second copy
of the vault password or impersonate a requesting application.

Status: source implementation and isolated Qt tests. Not yet selected in the
Beta 2 package manifest. Installed-VM acceptance is required before closing the
vault presentation release gate. This directory is not a separately published
GitHub fork and does not claim to be one.

## Scope and invariants

- Unlock, wrong-password retry, setup, cipher-choice introduction, permission
  requests and password-change dialogs receive Aero7 presentation.
- An empty caller ID remains anonymous; it is not labelled as Aero7 or a trusted
  system request. Supplied caller names and error descriptions are HTML-escaped.
- All non-Aero7 wallet presentation remains upstream. Global failed-access
  notifications have no specific wallet context and remain upstream too.
- Wallet names, service names, handles, access-policy keys, allow/deny results,
  encryption choices/defaults, empty-password policy, cancellation, password
  checks, lock/save behavior and credential files are not changed.
- The dialog icon is embedded from the existing pack, not resolved through the
  active icon theme and not newly drawn. See the optional component's
  [icon provenance](../../../companions/aero7-credential-vault/icons/PROVENANCE.md).
- This does not fix Wayland parenting of simultaneous callers or replace GPG
  pinentry, the global first-run KWallet wizard, or unrelated wallet management.

## Pinned inputs

Use the [KDE KWallet 6.29.0 source archive](https://download.kde.org/stable/frameworks/6.29/kwallet-6.29.0.tar.xz),
SHA-256 `66a47fc170ea074cce8b916fa313f309d7c9497bd2132e0598d4b63bbad2ac88`.
The reference Arch recipe is commit
`2fa55cbc2a8a2b9b1e7ffcc201b45ccc199be7e1`, tag `6.29.0-1`, in
[Arch's KWallet packaging repository](https://gitlab.archlinux.org/archlinux/packaging/packages/kwallet).
Archive checksum verification is not a claim of verified upstream signatures.

`prepare.sh` requires pristine hashed inputs, dry-runs the patch with zero fuzz,
and refuses existing overlay files. It must target an extracted build source,
not the host installation. Retain upstream source licenses with the patch and
the optional component's icon license and attribution with any built package.

```sh
bash /path/to/aero7-desktop/integration/credential-vault/kwallet/prepare.sh /path/to/kwallet-6.29.0
cmake -S /path/to/aero7-desktop/integration/credential-vault/kwallet -B /path/to/presentation-build -G Ninja -DKWALLET_SOURCE_DIR=/path/to/kwallet-6.29.0
cmake --build /path/to/presentation-build -j2
ctest --test-dir /path/to/presentation-build --output-on-failure
```

Tests compile the real upstream setup wizard and permission dialog, checking
both Aero7 and unrelated wallets, unchanged cipher choices and permission
results, cancellation, escaped text, and a usable embedded icon under a missing
theme. Password helper checks are not a substitute for replaying the built
daemon's actual requests, incorrect passwords, cancellation and retained data.
