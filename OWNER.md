# Where QOR OS stands

For Andrew, 10 November 2026. This continuation began at `10104ad` on
`integration/owner-brief` with five uncommitted Prism candidate files. Their
hashes and the original saved patch still match; earlier evidence is preserved.

## IDE and local validation

Qt's installed CMake 3.30.5 works at C:/Qt/Tools/CMake_64/bin/cmake.exe.
Windows IDE presets now identify that executable even without the developer
shell's PATH; Linux presets do not inherit it. VS Code's automatic configure
and build triggers are off. Build/lint tasks select the separate ci-windows
folder, protecting windows-release. Both platforms parse the corrected presets;
the live notification's dismissal was not observed. No global PATH or tool
installation changed.

Current focused checks pass 63 Windows cases with four platform skips and
84 cases under x86-64 WSL, Windows Subsystem for Linux. These include bounded
ELF (executable-file format) and Qt metadata tests, real CMake host refusal, and
17 product-harness fixture tests. Fixtures do not run actual products or ARM64
code. All repository hooks pass; full compiler, product and
sanitizer acceptance is not claimed.

## Prism resource blocker

With WSL resident after the VS Code restart, Windows had 1,443 MiB available
against the unchanged 3,072-MiB floor: 1,629 MiB short before compiler working
memory. No heavy Prism job was started, no competing QOR worker was found,
and the actual original heavy-job lock was free. No host resource/security
setting or unrelated service was changed.

The earlier Qt metadata differential on these same Prism headers records twelve
warnings on the committed baseline and zero on the candidate. That Windows
code-generation result was not repeated here and is not Linux compilation or
runtime evidence. The five files stay uncommitted pending those checks and the
address/undefined-behavior sanitizer run; existing patch and logs remain intact.

## Q Pi target qualification

The read-only probe now rejects Windows, Android and HarmonyOS target metadata,
including ambiguous, dynamic, commented and oversized inputs, without executing
a kit's CMake toolchain. Real installed 6.12.0 kits have been inspected. WSL's
kit reports Linux GCC metadata but is x86-64; no Linux ARM64 kit or qualified
native builder was identified. Enterprise Qt is owner-reported, not verification
of a target kit, dependency ABI (binary compatibility) or distribution scope.

The native QQ presets/harness remain available for a qualified builder.
No credential file was read, tool installed, physical medium changed or device
provisioned. **No ARM64 QOR app or bootable Pi image was built or tested.**
Next: clear the resident-WSL RAM guard for bounded Prism validation; qualify a
native ARM64 builder, entitled target kit, dependency closure and physical
scratch budget before the actual Q Pi application build.
