#
# SPDX-License-Identifier: MIT
#

import os

from oeqa.runtime.case import OERuntimeTestCase
from oeqa.core.decorator.depends import OETestDepends
from oeqa.runtime.decorator.package import OEHasPackage


class VF2VulkanWSI(OERuntimeTestCase):
    """Run vf2-vulkan-wsi-test on the target; fail on any 'not ok'.

    Logs are copied to ${WORKDIR}/vf2-vulkan-wsi-test/.
    """

    @OETestDepends(['ssh.SSHTest.test_ssh'])
    @OEHasPackage(['vf2-vulkan-wsi-test'])
    def test_vf2_vulkan_wsi(self):
        remote = '/tmp/vf2-vulkan-wsi-test-oeqa'
        args = (self.td.get('VF2_VULKAN_WSI_TEST_ARGS') or '').strip()

        self.target.run('rm -rf %s' % remote)
        status, output = self.target.run('vf2-vulkan-wsi-test -o %s %s' % (remote, args), timeout=1800)

        localdir = os.path.join(self.td.get('WORKDIR'), 'vf2-vulkan-wsi-test')
        os.makedirs(localdir, exist_ok=True)
        self.target.run('tar -C %s -cf /tmp/vf2-vulkan-wsi-test.tar .' % remote)
        self.target.copyFrom('/tmp/vf2-vulkan-wsi-test.tar', localdir)
        self.logger.info('vf2-vulkan-wsi-test logs copied to %s' % localdir)

        if '1..0 # SKIP' in output:
            self.skipTest(output.splitlines()[-1])

        failing = [l for l in output.splitlines() if l.startswith('not ok') or l.startswith('# ')]
        msg = 'vf2-vulkan-wsi-test exit %d:\n%s\n(logs: %s)' % (status, '\n'.join(failing), localdir)
        self.assertEqual(status, 0, msg=msg)
