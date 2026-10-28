# Building the Enigma kernel

## Reconstruct source

On an ARM64 Linux build host or x86_64 Linux/WSL cross compiler host:

```bash
./scripts/fetch-enigma.sh r20
# or: ./scripts/fetch-enigma.sh r21   # EXPERIMENTAL, not boot tested
```

The script clones https://gitlab.com/l4t-community/kernel/linux-nx, checks out `80f1e68e163ac77d688f0244b03cf05f791b4781`, then applies the same patch series verified on 2026-10-28. It fails rather than overwriting an existing `work/linux-enigma`.

The file `configs/enigma-r17-reference.config` is an **archived working-directory kernel configuration** from the R17 build. It has not been independently validated as producing a byte-identical R20 or R21 build. A build for a new target must be reviewed and recorded.

## Cross compilation (example)

Install an AArch64 GCC toolchain, GNU make and required kernel build dependencies (`flex`, `bison`, `bc`, `openssl` development headers, `libelf` development headers, `dtc`, etc.). Debian / Ubuntu packages usually include `gcc-aarch64-linux-gnu`, `binutils-aarch64-linux-gnu`, `build-essential`, `libssl-dev`, `libelf-dev`, `device-tree-compiler`.

```bash
mkdir -p work/build-enigma
cp configs/enigma-r17-reference.config work/build-enigma/.config
make -C work/linux-enigma O=\"$PWD/work/build-enigma\" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make -C work/linux-enigma O=\"$PWD/work/build-enigma\" ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j\"$(nproc)\" Image dtbs
```

Expected targets include `arch/arm64/boot/Image` and the board DTB `arch/arm64/boot/dts/nvidia/tegra210b01-vali.dtb` under the build output. This does **not** automatically produce a verified Hekate boot package. The tested WSL R21 build used GCC 15 and completed `Image dtbs`; offline artifact hashes are recorded in [TESTING](TESTING.md).

## Packaging and hardware deployment

The physical test setup used U-Boot-wrapped `uImage`, `nx-plat.dtimg`, Hekate entries and specific `/switchroot/` profiles. Their packaging workflow requires case-by-case validation and is **not presented here as a safe generic installer**. Do not blindly copy `Image` as `uImage`, or modify the primary boot entry. An independent deployment script can be contributed after documenting prerequisites, SHA256 checks, rollback and real-hardware outcomes.
