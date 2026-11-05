# Installer integration attribution

The branding descriptor, stylesheet, slideshow and candidate configuration are
original QOR OS data. The official mark comes from QOR.Kit's existing QorMark.
No Calamares, KPMcore, bootc or Anaconda source or binaries are vendored here.

Calamares is the proposed Qt installer framework, distributed under its own
per-file licences (principally GPL-3.0-or-later). Its partition module uses
KPMcore. Retain upstream copyright, licence notices and applicable source
distribution obligations when those separate components are packaged. The
branding data does not grant a licence to those components or to Qt.

Upstream interface references, inspected 5 November 2026:

- [Calamares branding](https://github.com/calamares/calamares/blob/calamares/src/branding/default/branding.desc)
- [Calamares partition settings](https://github.com/calamares/calamares/blob/calamares/src/modules/partition/partition.conf)
- [Calamares module sequencing](https://github.com/calamares/calamares/blob/calamares/settings.conf)
- [bootc installation](https://bootc.dev/bootc/bootc-install.html)
- [Current osbuild ISO inputs](https://osbuild.org/docs/developer-guide/projects/image-builder/advanced/bootc/isos/)

These references establish integration interfaces, not a tested QOR OS backend.
