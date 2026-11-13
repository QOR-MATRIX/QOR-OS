# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Development bootstrap only; the driver must check the local tools image ID.
ARG QOR_VM_TOOLS=qor-os/vm-tooling:20261106-v8
FROM ${QOR_VM_TOOLS} AS common
ARG QOR_SOURCE_REVISION
LABEL org.opencontainers.image.revision=${QOR_SOURCE_REVISION}
COPY runtime/ /
COPY branding/ /
COPY runtime-provenance.json /usr/share/qor-os/runtime-provenance.json
COPY menu-mark.png menu-mark.png.json menu-mark.png.license /usr/share/qor-os/boot-menu/
COPY check_vm_runtime.py /usr/libexec/qor-os/check_vm_runtime.py
COPY vmguard.py guest_inventory.py vm_install_backend.py vm_payload.py vm_display.py vm_session.py /usr/libexec/qor-os/
RUN QOR_VM_RUNTIME_AUDIT=1 python3 /usr/libexec/qor-os/check_vm_runtime.py
COPY --chmod=0644 display.service /usr/lib/systemd/system/qor-vm-display.service
COPY --chmod=0644 first-run.service /usr/lib/systemd/system/qor-vm-first-run.service
COPY --chmod=0644 guest-agent.service /usr/lib/systemd/system/qor-vm-agent.service
# Cleanup below touches generated state in this disposable image only. Keep
# installed libraries, licenses and RPM database content under /usr intact.
RUN test -n "$QOR_SOURCE_REVISION" && test -x /usr/bin/unshare && \
    mkdir -p /var/roothome /etc/systemd/system/graphical.target.wants /etc/systemd/system/multi-user.target.wants \
      /usr/lib/dracut/dracut.conf.d /usr/lib/modules-load.d /usr/lib/sysusers.d /usr/lib/tmpfiles.d /usr/lib/udev/rules.d && \
    printf 'u qortest 1000 "QOR OS local test account" /var/home/qortest /usr/sbin/nologin\n' > /usr/lib/sysusers.d/qor-vm.conf && \
    systemd-sysusers && \
    printf 'd /var/home/qortest 0700 qortest qortest -\nd /var/home/qortest/.local 0700 qortest qortest -\nd /var/home/qortest/.local/state 0700 qortest qortest -\nd /var/lib/plymouth 0755 root root -\nd /var/spool/plymouth 0755 root root -\nd /var/log/qemu-ga 0755 root root -\n' > /usr/lib/tmpfiles.d/qor-vm.conf && \
    printf 'KERNEL=="tty2", OWNER="qortest", GROUP="tty", MODE="0600"\n' > /usr/lib/udev/rules.d/70-qor-vm-console.rules && \
    printf '%s\n' '# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary' \
      'SUBSYSTEM=="virtio-ports", ATTR{name}=="org.qemu.guest_agent.0", TAG+="systemd", ENV{SYSTEMD_WANTS}="qor-vm-agent.service"' \
      'SUBSYSTEM=="virtio-ports", ATTR{name}=="dev.qorsync.vm-evidence.0", TAG+="systemd", ENV{SYSTEMD_WANTS}="qor-vm-first-run.service"' \
      > /usr/lib/udev/rules.d/99-qor-vm-ports.rules && \
    udevadm verify --no-summary /usr/lib/udev/rules.d/99-qor-vm-ports.rules && \
    printf '[general]\n' > /usr/share/qor-os/vm-agent.conf && \
    printf 'qemu_fw_cfg\nvirtio_console\n' > /usr/lib/modules-load.d/qor-vm.conf && \
    rm -rf -- /boot/grub2 /run/dnf /var/cache/libX11 /var/cache/libdnf5 /var/lib/dnf && \
    rm -f -- /var/cache/ldconfig/aux-cache /var/log/dnf5.log /var/log/dnf5.log.1 && \
    systemctl mask bootloader-update.service bootc-generic-growpart.service bootc-fetch-apply-updates.timer \
      getty@tty2.service autovt@tty2.service qemu-guest-agent.service && \
    ln -sfn /usr/lib/systemd/system/graphical.target /etc/systemd/system/default.target && \
    printf 'add_dracutmodules+=" qemu plymouth "\ninstall_items+=" /usr/lib64/libseccomp.so.2 "\nearly_microcode="no"\nsysloglvl=0\nstdloglvl=4\n' > /usr/lib/dracut/dracut.conf.d/40-qor-vm.conf

FROM common AS deploy
LABEL dev.qorsync.vm-profile=installed
RUN kver=$(find /usr/lib/modules -mindepth 1 -maxdepth 1 -type d -printf '%f\n') && \
    test $(printf '%s\n' "$kver" | wc -l) -eq 1 && \
    DRACUT_NO_XATTR=1 dracut --force --no-hostonly --no-hostonly-cmdline --kver "$kver" /usr/lib/modules/$kver/initramfs.img > /var/tmp/qor-dracut.log 2>&1 && \
    cat /var/tmp/qor-dracut.log && ! grep -E 'dracut\[E\]|dracut-install: ERROR|FAILED:' /var/tmp/qor-dracut.log && \
    rm -f -- /var/tmp/qor-dracut.log && bootc container lint --fatal-warnings

FROM common AS live
ARG QOR_VM_INSTALLATION=0
COPY --chmod=0644 display-probe.service /usr/lib/systemd/system/qor-vm-display-probe.service
COPY --chmod=0644 installer.service /usr/lib/systemd/system/qor-vm-installer.service
RUN mkdir -p /var/roothome /etc/systemd/system/graphical.target.wants /usr/lib/dracut/dracut.conf.d /usr/lib/modules-load.d && \
    mkdir -p /usr/lib/qor-os && touch /usr/lib/qor-os/live-session && \
    if [ "$QOR_VM_INSTALLATION" = 1 ]; then \
      test -x /usr/bin/qor-install && \
      printf '\n%s\n' '# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary' \
        '/usr/bin/qor-install -- system_u:object_r:install_exec_t:s0' \
        >> /etc/selinux/targeted/contexts/files/file_contexts.local && \
      chmod 0644 /etc/selinux/targeted/contexts/files/file_contexts.local && \
      test "$(matchpathcon -n /usr/bin/qor-install)" = 'system_u:object_r:install_exec_t:s0' && \
      sed -i 's/qor-vm-first-run.service/qor-vm-installer.service/' /usr/lib/udev/rules.d/99-qor-vm-ports.rules; \
    else \
      ln -s /usr/lib/systemd/system/qor-vm-display-probe.service /etc/systemd/system/graphical.target.wants/qor-vm-display-probe.service; \
    fi && \
    udevadm verify --no-summary /usr/lib/udev/rules.d/99-qor-vm-ports.rules && \
    ln -sfn /usr/lib/systemd/system/graphical.target /etc/systemd/system/default.target && \
    systemctl mask bootloader-update.service bootc-generic-growpart.service bootc-fetch-apply-updates.timer \
      getty@tty2.service autovt@tty2.service && \
    printf 'add_dracutmodules+=" qemu dmsquash-live plymouth "\ninstall_items+=" /usr/lib64/libseccomp.so.2 "\nearly_microcode="no"\nsysloglvl=0\nstdloglvl=4\n' > /usr/lib/dracut/dracut.conf.d/40-qor-live.conf && \
    kver=$(find /usr/lib/modules -mindepth 1 -maxdepth 1 -type d -printf '%f\n') && \
    test $(printf '%s\n' "$kver" | wc -l) -eq 1 && \
    DRACUT_NO_XATTR=1 dracut --force --no-hostonly --no-hostonly-cmdline --kver "$kver" /usr/lib/modules/$kver/initramfs.img > /var/tmp/qor-dracut.log 2>&1 && \
    cat /var/tmp/qor-dracut.log && ! grep -E 'dracut\[E\]|dracut-install: ERROR|FAILED:' /var/tmp/qor-dracut.log && \
    rm -f -- /var/tmp/qor-dracut.log && bootc container lint --fatal-warnings
# ISO composition runs separately with a one-CPU/768-MiB cgroup limit. Do not
# COPY this whole root into a second build stage: that exhausted host memory.
