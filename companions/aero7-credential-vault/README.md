# Aero7 Credential Manager

An optional encrypted vault, off by default. Enable **Encrypted Credential Vault**
in **Turn Aero7 features on or off**, then sign out and back in. Encryption and
password/permission prompts are provided by KWallet; this application does not
implement its own cryptography. Choose a non-empty vault password when prompted.

The Aero7 interface adds, edits, removes and locks generic credentials. It uses
the dedicated **Aero7 Credentials** wallet and **Generic Credentials** folder.
It does not import browser passwords, implement Windows domain credentials,
automatically fill passwords, or expose passwords in the entry list or logs.

Removing the optional package keeps encrypted wallet files and user settings.
Sign out afterwards to stop the current session's backend. Reinstalling the
feature makes the retained vault available again with its original password.
Per-user KWallet and portal overrides take priority over package defaults.
There is no vault-password recovery or plaintext-export feature.

Status: implementation under test; not yet packaged into release media or
published. Source and VM acceptance must pass before this is advertised as ready.
