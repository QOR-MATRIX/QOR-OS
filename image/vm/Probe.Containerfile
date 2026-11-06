# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Development bootstrap only; the driver must check the local tools image ID.
ARG QOR_VM_TOOLS=qor-os/vm-tooling:20261106-v7
FROM ${QOR_VM_TOOLS} AS live
COPY runtime/ /
COPY branding/ /
COPY runtime-provenance.json /usr/share/qor-os/runtime-provenance.json
COPY menu-mark.png menu-mark.png.json menu-mark.png.license /usr/share/qor-os/boot-menu/
COPY check_vm_runtime.py /usr/libexec/qor-os/check_vm_runtime.py
COPY vmguard.py guest_inventory.py vm_install_backend.py vm_display.py /usr/libexec/qor-os/
RUN QOR_VM_RUNTIME_AUDIT=1 python3 /usr/libexec/qor-os/check_vm_runtime.py
COPY display-probe.service /usr/lib/systemd/system/qor-vm-display-probe.service
RUN mkdir -p /var/roothome /etc/systemd/system/graphical.target.wants /usr/lib/dracut/dracut.conf.d /usr/lib/modules-load.d && \
    printf 'qemu_fw_cfg\n' > /usr/lib/modules-load.d/qor-vm.conf && \
    ln -s /usr/lib/systemd/system/qor-vm-display-probe.service /etc/systemd/system/graphical.target.wants/qor-vm-display-probe.service && \
    ln -sfn /usr/lib/systemd/system/graphical.target /etc/systemd/system/default.target && \
    systemctl mask bootloader-update.service bootc-generic-growpart.service bootc-fetch-apply-updates.timer \
      getty@tty2.service autovt@tty2.service && \
    printf 'add_dracutmodules+=" qemu dmsquash-live plymouth "\ninstall_items+=" /usr/lib64/libseccomp.so.2 "\nearly_microcode="no"\nsysloglvl=0\nstdloglvl=4\n' > /usr/lib/dracut/dracut.conf.d/40-qor-live.conf && \
    kver=$(find /usr/lib/modules -mindepth 1 -maxdepth 1 -type d -printf '%f\n') && \
    test $(printf '%s\n' "$kver" | wc -l) -eq 1 && \
    DRACUT_NO_XATTR=1 dracut --force --no-hostonly --no-hostonly-cmdline --kver "$kver" /usr/lib/modules/$kver/initramfs.img > /var/tmp/qor-dracut.log 2>&1 && \
    cat /var/tmp/qor-dracut.log && ! grep -E 'dracut\[E\]|dracut-install: ERROR|FAILED:' /var/tmp/qor-dracut.log
# ISO composition runs separately with a one-CPU/768-MiB cgroup limit. Do not
# COPY this whole root into a second build stage: that exhausted host memory.
