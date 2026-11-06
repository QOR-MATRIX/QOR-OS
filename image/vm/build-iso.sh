#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Runs only in the image build container. No host mounts or block devices.
set -euo pipefail
root=${QOR_IMAGE_ROOT:-/rootfs}
iso=${QOR_ISO_TREE:-/iso}
out=/out
if [ "$root" = / ]; then
  # Only the builder's disposable container layer is writable. Remove Docker's
  # generated container detector and replace its private generated identity/DNS
  # files with neutral image contents; none belongs in a booted OS image.
  test -f /.dockerenv || { echo 'Expected disposable Docker composition container' >&2; exit 1; }
  rm -- /.dockerenv
  printf '127.0.0.1 localhost\n::1 localhost\n' > /etc/hosts
  : > /etc/hostname
  : > /etc/resolv.conf
fi
mkdir -p "$iso/images/pxeboot" "$iso/boot/grub" "$out"
mapfile -t kernels < <(find "$root/usr/lib/modules" -mindepth 1 -maxdepth 1 -type d)
[ "${#kernels[@]}" -eq 1 ] || { echo 'Exactly one image kernel is required' >&2; exit 1; }
kernel=${kernels[0]}
test -s "$kernel/vmlinuz"
test -s "$kernel/initramfs.img"
cp "$kernel/vmlinuz" "$iso/images/pxeboot/vmlinuz"
cp "$kernel/initramfs.img" "$iso/images/pxeboot/initrd.img"
mkdir -p "$iso/LiveOS"
# Apply the device policy while packing; the build container needs no host
# mount privileges or permission to set security.selinux on its overlay.
# squashfs-tools-ng's xattr walker trims a bare '/' prefix to an empty
# pathname for the root inode. '/.' preserves the same root and its xattrs.
gensquashfs --pack-dir "${root%/}/." --one-file-system --compressor zstd --num-jobs 1 --queue-backlog 2 \
  --keep-xattr --selinux "$root/etc/selinux/targeted/contexts/files/file_contexts" \
  --quiet "$iso/LiveOS/squashfs.img"
cp "$root/usr/share/qor-os/boot-menu/menu-mark.png" "$iso/boot/grub/qor-mark.png"
cp "$root/usr/share/qor-os/boot-menu/menu-mark.png.json" "$iso/boot/grub/qor-mark.png.json"
cp "$root/usr/share/qor-os/boot-menu/menu-mark.png.license" "$iso/boot/grub/qor-mark.png.license"
font=$(find /usr/share/grub* -name unicode.pf2 -print -quit)
test -n "$font"
cp "$font" "$iso/boot/grub/unicode.pf2"
cat > "$iso/boot/grub/theme.txt" <<'THEME'
desktop-color: "#0b0e14"
title-text: "QOR OS — private VM development"
title-color: "#eeeeef"
+ image { left = 48 top = 48 width = 112 height = 112 file = "qor-mark.png" }
+ boot_menu { left = 15% top = 40% width = 70% height = 40% item_color = "#eeeeef" selected_item_color = "#b7baff" item_height = 48 item_padding = 12 }
THEME
cat > "$iso/boot/grub/grub.cfg" <<'GRUB'
set timeout=3
set default=0
insmod all_video
insmod png
if loadfont /boot/grub/unicode.pf2; then
  set gfxmode=1280x800,1024x768,auto
  terminal_output gfxterm
  set theme=/boot/grub/theme.txt
  export theme
fi
menuentry 'QOR OS — live display probe (installation unavailable)' {
  linux /images/pxeboot/vmlinuz root=live:CDLABEL=QOR_OS_LIVE rd.live.image rd.live.overlay.overlayfs=1 ro console=ttyS0,115200 console=tty0 rhgb plymouth.ignore-serial-consoles
  initrd /images/pxeboot/initrd.img
}
menuentry 'QOR OS — diagnostics, readable boot output' {
  linux /images/pxeboot/vmlinuz root=live:CDLABEL=QOR_OS_LIVE rd.live.image rd.live.overlay.overlayfs=1 ro console=ttyS0,115200 console=tty0 plymouth.enable=0 systemd.show_status=1 systemd.unit=multi-user.target
  initrd /images/pxeboot/initrd.img
}
GRUB
cat > /tmp/early.cfg <<'GRUB'
search --no-floppy --label QOR_OS_LIVE --set=root
# Keep the embedded memdisk module prefix. The ISO carries configuration and
# artwork, not a second copy of GRUB's modules.
configfile ($root)/boot/grub/grub.cfg
GRUB
grub2-mkstandalone -O x86_64-efi --modules='part_gpt part_msdos fat iso9660 normal configfile linux search search_label all_video gfxterm gfxmenu png font' -o /tmp/BOOTX64.EFI 'boot/grub/grub.cfg=/tmp/early.cfg'
truncate -s 64M "$iso/images/efiboot.img"
mformat -i "$iso/images/efiboot.img" -F ::
mmd -i "$iso/images/efiboot.img" ::/EFI ::/EFI/BOOT
mcopy -i "$iso/images/efiboot.img" /tmp/BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI
xorriso -as mkisofs -iso-level 3 -V QOR_OS_LIVE -e images/efiboot.img -no-emul-boot -o "$out/qor-os-live-probe.iso" "$iso"
sha256sum "$out/qor-os-live-probe.iso" > "$out/SHA256SUMS"
