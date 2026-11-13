# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Dependency bootstrap, not a reproducible release recipe. Freeze the resolved
# local image ID, package versions and runtime inputs before image acceptance.
FROM quay.io/fedora/fedora-bootc@sha256:0cd45c0431f553a1f40a4717fb53f59f24a7f93f0d2962d581ac9b9b8860008e
RUN dnf -y --setopt=max_parallel_downloads=1 --setopt=keepcache=True install \
    qemu-system-x86-core qemu-device-display-virtio-vga qemu-guest-agent edk2-ovmf python3 \
    xorriso squashfs-tools squashfs-tools-ng mtools grub2-efi-x64-cdboot \
    grub2-efi-x64-modules grub2-tools-extra dracut-live dnf5-plugins \
    libglvnd-glx libglvnd-egl libglvnd-opengl libxkbcommon libinput \
    libX11-xcb libatomic fontconfig pulseaudio-libs pipewire-libs plymouth plymouth-plugin-script && \
    rpm -qa --qf '%{NAME}-%{VERSION}-%{RELEASE}.%{ARCH}\n' | sort > /qor-tooling-rpms.txt
