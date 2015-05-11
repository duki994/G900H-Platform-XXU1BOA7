# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""cros deploy: Deploy the packages onto the target device."""

import os
import logging
import urlparse

from chromite import cros
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import remote_access


@cros.CommandDecorator('deploy')
class DeployCommand(cros.CrosCommand):
  """Deploy the requested packages to the target device.

  This command assumes the requested packages are already built in the
  chroot. This command needs to run inside the chroot for inspecting
  the installed packages.

  Note: If the rootfs on your device is read-only, this command
  remounts it as read-write. If the rootfs verification is enabled on
  your device, this command disables it.
  """

  EPILOG = """
To deploy packages:
  cros deploy device power_manager cherrypy

To uninstall packages:
  cros deploy --unmerge cherrypy

For more information of cros build usage:
  cros build -h
"""

  DEVICE_WORK_DIR = '/tmp/cros-deploy'

  # Override base class property to enable stats upload.
  upload_stats = True

  def __init__(self, options):
    """Initializes DeployCommand."""
    cros.CrosCommand.__init__(self, options)
    self.emerge = True
    self.ssh_hostname = None
    self.ssh_port = None

  @classmethod
  def AddParser(cls, parser):
    """Add a parser."""
    super(cls, DeployCommand).AddParser(parser)
    parser.add_argument(
        'device', help='IP[:port] address of the target device.')
    parser.add_argument(
        'packages', help='Packages to install.', nargs='+')
    parser.add_argument(
        '--board', default=None, help='The board to use. By default it is '
        'automatically detected. You can override the detected board with '
        'this option.')
    parser.add_argument(
        '--unmerge',  dest='emerge', action='store_false', default=True,
        help='Unmerge requested packages.')
    parser.add_argument(
        '--emerge-args', default=None,
        help='Extra arguments to pass to emerge.')

  def GetLatestPackage(self, board, pkg):
    """Returns the path to the latest |pkg| for |board|."""
    sysroot = cros_build_lib.GetSysroot(board=board)
    cpv = portage_utilities.SplitCPV(pkg, strict=False)

    category = cpv.category
    package_name = cpv.package
    version = cpv.version
    if not category:
      # If category is not given, find possible matches across all categories.
      matches = portage_utilities.FindPackageNameMatches(
          package_name, sysroot=sysroot)
      if not matches:
        raise ValueError('Package %s is not installed!' % pkg)

      idx = 0
      if len(matches) > 1:
        # Ask user to pick among multiple matches.
        idx = cros_build_lib.GetChoice(
            'Multiple matches found for %s: ' % pkg,
            [os.path.join(x.category, x.package) for x in matches])

      category = matches[idx].category

    if not version:
      # If version is not given, pick the best visible binary package.
      try:
        bv_cpv = portage_utilities.BestVisible(
            os.path.join(category, package_name),
            board=board, pkg_type='binary')
        version = bv_cpv.version
      except cros_build_lib.RunCommandError:
        raise ValueError('Package %s is not installed!' % pkg)

    return portage_utilities.GetBinaryPackagePath(
        category, package_name, version, sysroot=sysroot)

  def _Emerge(self, device, board, pkg, extra_args=None):
    """Copies |pkg| to |device| and emerges it.

    Args:
      device: A ChromiumOSDevice object.
      board: The board to use for retrieving |pkg|.
      pkg: A package name.
      extra_args: Extra arguments to pass to emerge.
    """
    latest_pkg = self.GetLatestPackage(board, pkg)
    if not latest_pkg:
      cros_build_lib.Die('Missing package %s.' % pkg)

    pkgroot = os.path.join(device.work_dir, 'packages')
    pkg_name = os.path.basename(latest_pkg)
    pkg_dirname = os.path.basename(os.path.dirname(latest_pkg))
    pkg_dir = os.path.join(pkgroot, pkg_dirname)
    device.RunCommand(['mkdir', '-p', pkg_dir])

    logging.info('Copying %s to device...', latest_pkg)
    device.CopyToDevice(latest_pkg, pkg_dir)

    logging.info('Installing %s...', latest_pkg)
    pkg_path = os.path.join(pkg_dir, pkg_name)

    extra_env = {'FEATURES': '-sandbox', 'PKGDIR': pkgroot}
    cmd = ['emerge', '--usepkg', pkg_path]
    if extra_args:
      cmd.append(extra_args)

    try:
      result = device.RunCommand(cmd, extra_env=extra_env)
      logging.debug(result.output)
    except Exception:
      logging.error('Failed to emerge package %s', pkg)
      raise
    else:
      logging.info('%s has been installed.', pkg)

  def _Unmerge(self, device, pkg):
    """Unmerges |pkg| on |device|.

    Args:
      device: A RemoteDevice object.
      pkg: A package name.
    """
    logging.info('Unmerging %s...', pkg)
    cmd = ['emerge', '--unmerge', pkg]
    try:
      result = device.RunCommand(cmd)
      logging.debug(result.output)
    except Exception:
      logging.error('Failed to unmerge package %s', pkg)
      raise
    else:
      logging.info('%s has been uninstalled.', pkg)

  def _ReadOptions(self):
    """Processes options and set variables."""
    self.emerge = self.options.emerge
    device = self.options.device
    # pylint: disable=E1101
    if urlparse.urlparse(device).scheme == '':
      # For backward compatibility, prepend ssh:// ourselves.
      device = 'ssh://%s' % device

    parsed = urlparse.urlparse(device)

    if parsed.scheme == 'ssh':
      self.ssh_hostname = parsed.hostname
      self.ssh_port = parsed.port
    else:
      cros_build_lib.Die('Does not support device %s' % self.options.device)

  def Run(self):
    """Run cros deploy."""
    cros_build_lib.AssertInsideChroot()
    self._ReadOptions()

    try:
      with remote_access.ChromiumOSDeviceHandler(
          self.ssh_hostname, port=self.ssh_port,
          work_dir=self.DEVICE_WORK_DIR) as device:
        board = cros_build_lib.GetBoard(device_board=device.board,
                                        override_board=self.options.board)
        logging.info('Board is %s', board)

        if not device.MountRootfsReadWrite():
          cros_build_lib.Die('Cannot remount rootfs as read-write. Exiting...')

        for pkg in self.options.packages:
          if self.emerge:
            self._Emerge(device, board, pkg,
                         extra_args=self.options.emerge_args)
          else:
            self._Unmerge(device, pkg)

    except Exception:
      logging.error('Cros Deploy terminated before completing!')
      raise
    else:
      logging.info('All packages are processed.')
