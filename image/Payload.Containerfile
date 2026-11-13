# SPDX-License-Identifier: LicenseRef-QOR-OS-Proprietary
# Data-only integration image. It cannot boot and contains no Qt/OS binaries.
FROM scratch
COPY . /
