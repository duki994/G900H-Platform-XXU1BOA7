#!/usr/bin/python
# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for chromite.lib.git and helpers for testing that module."""

import functools
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__)))))

from chromite.lib import cros_build_lib
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import git
from chromite.lib import partial_mock

import mock


class ManifestMock(partial_mock.PartialMock):
  """Partial mock for git.Manifest."""
  TARGET = 'chromite.lib.git.Manifest'
  ATTRS = ('_RunParser',)

  def _RunParser(self, *_args):
    pass


class ManifestCheckoutMock(partial_mock.PartialMock):
  """Partial mock for git.ManifestCheckout."""
  TARGET = 'chromite.lib.git.ManifestCheckout'
  ATTRS = ('_GetManifestsBranch',)

  def _GetManifestsBranch(self, _root):
    return 'default'


class NormalizeRefTest(cros_test_lib.TestCase):
  """Test the Normalize*Ref functions."""

  def _TestNormalize(self, functor, tests):
    """Helper function for testing Normalize*Ref functions.

    Args:
      functor: Normalize*Ref functor that only needs the input
        ref argument.
      tests: Dict of test inputs to expected test outputs.
    """
    for test_input, test_output in tests.iteritems():
      result = functor(test_input)
      msg = ('Expected %s to translate %r to %r, but got %r.' %
             (functor.__name__, test_input, test_output, result))
      self.assertEquals(test_output, result, msg)

  def testNormalizeRef(self):
    """Test git.NormalizeRef function."""
    tests = {
        # These should all get 'refs/heads/' prefix.
        'foo': 'refs/heads/foo',
        'foo-bar-123': 'refs/heads/foo-bar-123',

        # If input starts with 'refs/' it should be left alone.
        'refs/foo/bar': 'refs/foo/bar',
        'refs/heads/foo': 'refs/heads/foo',

        # Plain 'refs' is nothing special.
        'refs': 'refs/heads/refs',

        None: None,
    }
    self._TestNormalize(git.NormalizeRef, tests)

  def testNormalizeRemoteRef(self):
    """Test git.NormalizeRemoteRef function."""
    remote = 'TheRemote'
    tests = {
        # These should all get 'refs/remotes/TheRemote' prefix.
        'foo': 'refs/remotes/%s/foo' % remote,
        'foo-bar-123': 'refs/remotes/%s/foo-bar-123' % remote,

        # These should be translated from local to remote ref.
        'refs/heads/foo': 'refs/remotes/%s/foo' % remote,
        'refs/heads/foo-bar-123': 'refs/remotes/%s/foo-bar-123' % remote,

        # These should be moved from one remote to another.
        'refs/remotes/OtherRemote/foo': 'refs/remotes/%s/foo' % remote,

        # These should be left alone.
        'refs/remotes/%s/foo' % remote: 'refs/remotes/%s/foo' % remote,
        'refs/foo/bar': 'refs/foo/bar',

        # Plain 'refs' is nothing special.
        'refs': 'refs/remotes/%s/refs' % remote,

        None: None,
    }

    # Add remote arg to git.NormalizeRemoteRef.
    functor = functools.partial(git.NormalizeRemoteRef, remote)
    functor.__name__ = git.NormalizeRemoteRef.__name__

    self._TestNormalize(functor, tests)


class ProjectCheckoutTest(cros_test_lib.TestCase):
  """Tests for git.ProjectCheckout"""

  def setUp(self):
    self.fake_unversioned_patchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/chromite',
             revision='remotes/for/master'))
    self.fake_unversioned_unpatchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/platform/somethingsomething/chromite',
             # Pinned to a SHA1.
             revision='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf'))
    self.fake_versioned_patchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/chromite',
             revision='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf',
             upstream='remotes/for/master'))
    self.fake_versioned_unpatchable = git.ProjectCheckout(
        dict(name='chromite',
             path='src/chromite',
             revision='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf',
             upstream='1deadbeeaf1deadbeeaf1deadbeeaf1deadbeeaf'))

  def testIsPatchable(self):
    self.assertTrue(self.fake_unversioned_patchable.IsPatchable())
    self.assertFalse(self.fake_unversioned_unpatchable.IsPatchable())
    self.assertTrue(self.fake_versioned_patchable.IsPatchable())
    self.assertFalse(self.fake_versioned_unpatchable.IsPatchable())


class GitPushTest(cros_test_lib.MockTestCase):
  """Tests for git.GitPush function."""

  # Non fast-forward push error message.
  NON_FF_PUSH_ERROR = ('To https://localhost/repo.git\n'
      '! [remote rejected] master -> master (non-fast-forward)\n'
      'error: failed to push some refs to \'https://localhost/repo.git\'\n')

  # List of possible GoB transient errors.
  TRANSIENT_ERRORS = (
      # Hook error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: (-)To https://localhost/repo.git\n'
       '! [remote rejected] 6c78ca083c3a9d64068c945fd9998eb1e0a3e739 -> '
       'stabilize-4636.B (error in hook)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # 'failed to lock' error when creating a new branch from SHA1 ref.
      ('remote: Processing changes: done\nTo https://localhost/repo.git\n'
       '! [remote rejected] 4ea09c129b5fedb261bae2431ce2511e35ac3923 -> '
       'stabilize-daisy-4319.96.B (failed to lock)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # Hook error when pushing branch.
      ('remote: Processing changes: (\)To https://localhost/repo.git\n'
       '! [remote rejected] temp_auto_checkin_branch -> '
       'master (error in hook)\n'
       'error: failed to push some refs to \'https://localhost/repo.git\'\n'),

      # Another kind of error when pushing a branch.
      'fatal: remote error: Internal Server Error',
  )

  def setUp(self):
    self.StartPatcher(mock.patch('time.sleep'))

  @staticmethod
  def _RunGitPush():
    """Runs git.GitPush with some default arguments."""
    git.GitPush('some_repo_path', 'local-ref',
                git.RemoteRef('some-remote', 'remote-ref'),
                dryrun=True, retry=True)

  def testPushSuccess(self):
    """Test handling of successful git push."""
    with cros_build_lib_unittest.RunCommandMock() as rc_mock:
      rc_mock.AddCmdResult(partial_mock.In('push'), returncode=0)
      self._RunGitPush()

  def testNonFFPush(self):
    """Non fast-forward push error propagates to the caller."""
    with cros_build_lib_unittest.RunCommandMock() as rc_mock:
      rc_mock.AddCmdResult(partial_mock.In('push'), returncode=128,
                           error=self.NON_FF_PUSH_ERROR)
      self.assertRaises(cros_build_lib.RunCommandError, self._RunGitPush)

  def testPersistentTransientError(self):
    """GitPush fails if transient error occurs multiple times."""
    for error in self.TRANSIENT_ERRORS:
      with cros_build_lib_unittest.RunCommandMock() as rc_mock:
        rc_mock.AddCmdResult(partial_mock.In('push'), returncode=128,
                             error=error)
        self.assertRaises(cros_build_lib.RunCommandError, self._RunGitPush)

  def testOneTimeTransientError(self):
    """GitPush retries transient errors."""
    for error in self.TRANSIENT_ERRORS:
      with cros_build_lib_unittest.RunCommandMock() as rc_mock:
        results = [
            rc_mock.CmdResult(128, '', error),
            rc_mock.CmdResult(0, 'success', ''),
        ]
        side_effect = lambda *_args, **_kwargs: results.pop(0)
        rc_mock.AddCmdResult(partial_mock.In('push'), side_effect=side_effect)
        self._RunGitPush()


if __name__ == '__main__':
  cros_test_lib.main()
