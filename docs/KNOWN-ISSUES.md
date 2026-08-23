# Known issues and exact limitations

- The validated VM screenshot path now produces fully opaque compositor
  captures. Visual comparison is still limited to the VM's VirtIO/llvmpipe
  renderer and does not substitute for physical-GPU inspection.
- No physical USB device, GPU, suspend/resume cycle, mixed-DPI monitor, or real
  connector hotplug is available in the VM. The nested KWin suite exercises
  three outputs, hot disable/re-enable, and 1.25 scaling.
- The public `kscreen-doctor` interface does not expose a safe output-cloning
  transaction, so validation covers extend, position, scale, primary, disable,
  and restore rather than claiming unverified mirror support.
- Physical mouse gesture feel still needs hardware validation even though the
  KWin scripts and window-state endpoints are tested.
- Plasma 6.7 logs `QML MenuRepresentation: trying to show an empty dialog`
  while dynamically opening SevenStart. The dialog is visibly populated in
  the same interaction, Start remains single-instance and functional, and no
  crash or failed service results. Explicitly hiding both the dynamic dialog
  and its component did not suppress the framework timing diagnostic, so it
  is recorded rather than hidden behind an ineffective workaround.

These are test-environment limitations, not hidden passes or alternate shell
implementations.
