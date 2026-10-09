#
# SPDX-License-Identifier: MIT
#

import os

from oeqa.runtime.case import OERuntimeTestCase
from oeqa.core.decorator.depends import OETestDepends
from oeqa.runtime.decorator.package import OEHasPackage


class VF2SelfTest(OERuntimeTestCase):
    """Run vf2-selftest on the target; fail on any FAIL.

    With VF2_SELFTEST_BASELINE (a results.tsv on the build host), fail only on
    regressions against it. Reports are copied to ${WORKDIR}/vf2-selftest/.
    """

    @OETestDepends(['ssh.SSHTest.test_ssh'])
    @OEHasPackage(['vf2-selftest'])
    def test_vf2_selftest(self):
        remote = '/tmp/vf2-selftest-oeqa'
        args = (self.td.get('VF2_SELFTEST_ARGS') or '').strip()

        baseline = (self.td.get('VF2_SELFTEST_BASELINE') or '').strip()
        if baseline:
            self.target.copyTo(baseline, '/tmp/vf2-selftest-baseline.tsv')
            args += ' --baseline /tmp/vf2-selftest-baseline.tsv'

        self.target.run('rm -rf %s' % remote)
        status, output = self.target.run('vf2-selftest -o %s %s' % (remote, args), timeout=1800)

        localdir = os.path.join(self.td.get('WORKDIR'), 'vf2-selftest')
        os.makedirs(localdir, exist_ok=True)
        for f in ('ktap.txt', 'results.tsv', 'report.md'):
            self.target.copyFrom('%s/%s' % (remote, f), localdir)
        self.logger.info('vf2-selftest reports copied to %s' % localdir)

        failing = [l for l in output.splitlines()
                   if l.startswith('not ok') or l.startswith('# REGRESSION')]
        msg = 'vf2-selftest exit %d:\n%s\n(full log: %s/ktap.txt)' % (
            status, '\n'.join(failing), localdir)
        self.assertEqual(status, 0, msg=msg)
