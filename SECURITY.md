# Reporting a vulnerability

meta-vf2-selftest builds a bench test image: it enables passwordless root login & an
SSH server on purpose. Those are not vulnerabilities; do not deploy the image.

Report anything else on the [issues](https://github.com/OOHehir/meta-vf2-selftest/issues)
page. For an issue that should not be public yet, open an issue asking for a private
contact & leave the details out until one is given. Include the recipe, its version & the
`VF2_KERNEL` mode.

Only the branch matching the current oe-core release is maintained.
