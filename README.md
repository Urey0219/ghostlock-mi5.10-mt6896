# ghostlock-mi5.10-mt6896

A proof-of-concept privilege escalation for the build below, reimplemented from
scratch in portable C++ for research and audit.

## Target

| | |
|---|---|
| Device | Redmi pearl (MediaTek MT6895) |
| Kernel | `5.10.136-android12-9-00020-gc9f59ef34367-ab9585114` |
| ROM | `V14.0.5.0.TLHCNXM` (`ro.build.type=user`) |
| Reference | CVE-2026-43499 |

The kernel symbol addresses and structure offsets are specific to that build.
On any other build the PoC will not work and may crash the device.

## Layout

```
src/    sources and build scripts
bin/    prebuilt binaries (see below)
```

## Build

Requires the Android NDK (r27c). Either place it at `.toolchain/android-ndk-r27c/`
next to this repository, or pass the path explicitly.

```sh
cd src
./build-aarch64.sh                 # -> src/build-ndk/ghostlock
NDK=/path/to/ndk ./build-aarch64.sh

./build-shared.sh                  # -> src/build-ndk/libghostlock.so
```

`libghostlock.so` starts the full flow the moment it is loaded (constructor);
there is no switch to defer it. `gl_payload_main` is its only exported symbol
and returns the same status as the executable's exit code.

## Usage

```sh
adb push bin/ghostlock /data/local/tmp/
adb shell chmod 755 /data/local/tmp/ghostlock
adb shell /data/local/tmp/ghostlock
```

No arguments. No configuration. No output on success; exit code `0` means the
escalation succeeded and root was handed to KernelSU (`ksud late-load`).

## Prebuilt binaries

| File | md5 | Size |
|---|---|---|
| `bin/ghostlock` | `341cbc0d212773afde19f3d85b649ab9` | 17880 |
| `bin/libghostlock.so` | `afed48bbf23a0b288cdc0de2886f0d5e` | 17456 |

Both were built with the NDK r27c toolchain from the sources in `src/`.
They are provided for convenience; rebuilding from source is recommended.

## Status

The escalation chain was verified on the target device above:
`uid=0`, `CapEff=000001ffffffffff`, SELinux context `u:r:kernel:s0`.
The published build has not been re-run on hardware since the final
source cleanup; treat the binaries as unverified until you run them yourself.

## Scope

Provided for research on devices you own or are explicitly authorised to test.
Do not run it on hardware you do not control.

## License

GNU General Public License v3.0 — see [LICENSE](LICENSE).
