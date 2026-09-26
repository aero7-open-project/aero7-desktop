# Third-party material

`packaging/repository/aero7-repository.asc` is the public signing key from
`memegeko/aero7-repo` at local revision `443f8df`. It is redistributed solely
to authenticate packages from the configured Aero7 repository. Its expected
fingerprint is `72C79ABBBBE96446DD3324042694BFE1090F4FD6` and its source-file
SHA-256 is `803a1d8b3fd3a92aa9d2a6f8e9dea5c8b017e392029325fa746a4032251ce9cb`.

Runtime dependencies and bundled visual assets retain their upstream licences
and rights notices. The established AeroThemePlasma icon pack is used by bundled
components; its artwork must not be described as newly created Aero7 artwork or
as covered solely by a component's source-code licence. Preserve the pack's
upstream attribution and notices with distributed copies.

The complete MIT-licensed Aero7 Desktop Gadgets 3.0 source is included in
`companions/aero7-gadgets`. It was developed in the Aero7 package repository
before being consolidated here. Its host, gallery, persistence, networking and
painted gadget implementations are maintained here. The app's bundled pack
icons have separate [asset notices](companions/aero7-gadgets/assets/icons/LICENSES.md).

The complete MIT-licensed Aero7 Internet Explorer compatibility source is
included in `companions/aero7-internet-explorer`. It supplies an original
Aero7 launcher, backend-selection library, tests, and documentation. Its reused
pack icons are documented in the [asset provenance](companions/aero7-internet-explorer/docs/ASSET-PROVENANCE.md)
and accompanying upstream notices; the MIT source licence does not relabel them.

The [Media Player skin](assets/media-player/ASSET-PROVENANCE.md) embeds
unmodified controls and launcher icons from the same AeroThemePlasma pack.
Its backdrops are original Aero7 SVG artwork; the imported icons retain the
pack's separate AGPL-3.0-or-later and upstream rights notices.

The optional [Credential Vault](companions/aero7-credential-vault/README.md)
uses KWallet for encrypted storage. Its [native-dialog integration](integration/credential-vault/kwallet/README.md)
contains a downstream patch for KWallet 6.29.0 and LGPL-2.0-or-later presentation
code; upstream KWallet files retain their own SPDX licences. The integration
embeds the optional component's unchanged
[pack icon](companions/aero7-credential-vault/icons/PROVENANCE.md). Test packages
must carry its `UPSTREAM-LICENSE`, `UPSTREAM-README.md` and provenance alongside
the upstream software notices. Local source/build testing does not mean that
this integration has been published or selected for a release.
