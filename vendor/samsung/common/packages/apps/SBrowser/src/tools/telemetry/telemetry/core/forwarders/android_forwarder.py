# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import re
import socket
import struct
import subprocess
import sys

from telemetry.core import forwarders
from telemetry.core import util
from telemetry.core.backends import adb_commands

util.AddDirToPythonPath(util.GetChromiumSrcDir(), 'build', 'android')
try:
  from pylib import forwarder  # pylint: disable=F0401
except Exception:
  forwarder = None


class AndroidForwarderFactory(forwarders.ForwarderFactory):

  def __init__(self, adb, use_rndis):
    super(AndroidForwarderFactory, self).__init__()
    self._adb = adb
    self._rndis_configurator = None
    if use_rndis:
      self._rndis_configurator = AndroidRndisConfigurator(self._adb)

  def Create(self, port_pairs):
    if self._rndis_configurator:
      return AndroidRndisForwarder(self._adb, self.host_ip,
                                   self._rndis_configurator.device_iface,
                                   port_pairs)
    return AndroidForwarder(self._adb, port_pairs)

  @property
  def host_ip(self):
    if self._rndis_configurator:
      return self._rndis_configurator.host_ip
    return super(AndroidForwarderFactory, self).host_ip

  @property
  def does_forwarder_override_dns(self):
    return bool(self._rndis_configurator)


class AndroidForwarder(forwarders.Forwarder):

  def __init__(self, adb, port_pairs):
    super(AndroidForwarder, self).__init__(port_pairs)
    self._adb = adb.Adb()
    forwarder.Forwarder.Map([p for p in port_pairs if p], self._adb)
    # TODO(tonyg): Verify that each port can connect to host.

  def Close(self):
    for port_pair in self._port_pairs:
      if port_pair:
        forwarder.Forwarder.UnmapDevicePort(port_pair.local_port, self._adb)
    super(AndroidForwarder, self).Close()


class AndroidRndisForwarder(forwarders.Forwarder):
  """Forwards traffic using RNDIS. Assumes the device has root access."""

  def __init__(self, adb, host_ip, device_iface, port_pairs):
    super(AndroidRndisForwarder, self).__init__(port_pairs)

    self._adb = adb
    self._device_iface = device_iface
    self._host_ip = host_ip
    self._original_dns = None, None, None
    self._RedirectPorts(port_pairs)
    if port_pairs.dns:
      self._OverrideDns()
    # TODO(tonyg): Verify that each port can connect to host.

  @property
  def host_ip(self):
    return self._host_ip

  def Close(self):
    self._SetDns(*self._original_dns)
    super(AndroidRndisForwarder, self).Close()

  def _RedirectPorts(self, port_pairs):
    """Sets the local to remote pair mappings to use for RNDIS."""
    self._adb.RunShellCommand('iptables -F -t nat')  # Flush any old nat rules.
    for port_pair in port_pairs:
      if not port_pair or port_pair.local_port == port_pair.remote_port:
        continue
      protocol = 'udp' if port_pair.remote_port == 53 else 'tcp'
      self._adb.RunShellCommand(
          'iptables -t nat -A OUTPUT -p %s --dport %d'
          ' -j DNAT --to-destination :%d' %
          (protocol, port_pair.remote_port, port_pair.local_port))

  def _OverrideDns(self):
    """Overrides DNS on device to point at the host."""
    self._original_dns = self._GetCurrentDns()
    if not self._original_dns[0]:
      # No default route. Install one via the host. This is needed because
      # getaddrinfo in bionic uses routes to determine AI_ADDRCONFIG.
      self._adb.RunShellCommand('route add default gw %s dev %s' %
                                (self.host_ip, self._device_iface))
    self._SetDns(self._device_iface, self.host_ip, self.host_ip)

  def _SetDns(self, iface, dns1, dns2):
    """Overrides device's DNS configuration.

    Args:
      iface: name of the network interface to make default
      dns1, dns2: nameserver IP addresses
    """
    if not iface:
      return  # If there is no route, then nobody cares about DNS.
    # DNS proxy in older versions of Android is configured via properties.
    # TODO(szym): run via su -c if necessary.
    self._adb.system_properties['net.dns1'] = dns1
    self._adb.system_properties['net.dns2'] = dns2
    dnschange = self._adb.system_properties['net.dnschange']
    if dnschange:
      self._adb.system_properties['net.dnschange'] = int(dnschange) + 1
    # Since commit 8b47b3601f82f299bb8c135af0639b72b67230e6 to frameworks/base
    # the net.dns1 properties have been replaced with explicit commands for netd
    self._adb.RunShellCommand('ndc netd resolver setifdns %s %s %s' %
                              (iface, dns1, dns2))
    # TODO(szym): if we know the package UID, we could setifaceforuidrange
    self._adb.RunShellCommand('ndc netd resolver setdefaultif %s' % iface)

  def _GetCurrentDns(self):
    """Returns current gateway, dns1, and dns2."""
    routes = self._adb.RunShellCommand('cat /proc/net/route')[1:]
    routes = [route.split() for route in routes]
    default_routes = [route[0] for route in routes if route[1] == '00000000']
    return (
      default_routes[0] if default_routes else None,
      self._adb.system_properties['net.dns1'],
      self._adb.system_properties['net.dns2'],
    )


class AndroidRndisConfigurator(object):
  """Configures a linux host to connect to an android device via RNDIS.

  Note that we intentionally leave RNDIS running on the device. This is
  because the setup is slow and potentially flaky and leaving it running
  doesn't seem to interfere with any other developer or bot use-cases.
  """

  _RNDIS_DEVICE = '/sys/class/android_usb/android0'
  _NETWORK_INTERFACES = '/etc/network/interfaces'
  _TELEMETRY_MARKER = '# Added by Telemetry for RNDIS forwarding #'

  def __init__(self, adb):
    is_root_enabled = adb.Adb().EnableAdbRoot()
    assert is_root_enabled, 'RNDIS forwarding requires a rooted device.'
    self._adb = adb.Adb()

    self._device_ip = None
    self._host_iface = None
    self._host_ip = None
    self.device_iface = None

    assert self._IsRndisSupported(), 'Device does not support RNDIS.'
    self._CheckConfigureNetwork()

  @property
  def host_ip(self):
    return self._host_ip

  def _IsRndisSupported(self):
    """Checks that the device has RNDIS support in the kernel."""
    return self._adb.FileExistsOnDevice(
        '%s/f_rndis/device' % self._RNDIS_DEVICE)

  def _WaitForDevice(self):
    self._adb.Adb().SendCommand('wait-for-device')

  def _FindDeviceRndisInterface(self):
    """Returns the name of the RNDIS network interface if present."""
    config = self._adb.RunShellCommand('netcfg')
    interfaces = [line.split()[0] for line in config]
    candidates = [iface for iface in interfaces if re.match('rndis|usb', iface)]
    if candidates:
      assert len(candidates) == 1, 'Found more than one rndis device!'
      return candidates[0]

  def _EnumerateHostInterfaces(self):
    if sys.platform.startswith('linux'):
      return subprocess.check_output(['ip', 'addr']).splitlines()
    elif sys.platform == 'darwin':
      return subprocess.check_output(['ifconfig']).splitlines()
    raise Exception('Platform %s not supported!' % sys.platform)

  def _FindHostRndisInterface(self):
    """Returns the name of the host-side network interface."""
    interface_list = self._EnumerateHostInterfaces()
    ether_address = self._adb.GetFileContents(
        '%s/f_rndis/ethaddr' % self._RNDIS_DEVICE)[0]
    interface_name = None
    for line in interface_list:
      if not line.startswith(' '):
        interface_name = line.split()[1].strip(':')
      elif ether_address in line:
        return interface_name

  def _WriteProtectedFile(self, path, contents):
    subprocess.check_call(
        ['sudo', 'bash', '-c', 'echo -e "%s" > %s' % (contents, path)])

  def _DisableRndis(self):
    self._adb.system_properties['sys.usb.config'] = 'adb'
    self._WaitForDevice()

  def _EnableRndis(self):
    """Enables the RNDIS network interface."""
    script_prefix = '/data/local/tmp/rndis'
    # This could be accomplished via "svc usb setFunction rndis" but only on
    # devices which have the "USB tethering" feature.
    # Also, on some devices, it's necessary to go through "none" function.
    script = """
trap '' HUP
trap '' TERM
trap '' PIPE

function manual_config() {
  echo %(functions)s > %(dev)s/functions
  echo 224 > %(dev)s/bDeviceClass
  echo 1 > %(dev)s/enable
  start adbd
  setprop sys.usb.state %(functions)s
}

# This function kills adb transport, so it has to be run "detached".
function doit() {
  setprop sys.usb.config none
  while [ `getprop sys.usb.state` != "none" ]; do
    sleep 1
  done
  manual_config
  # For some combinations of devices and host kernels, adb won't work unless the
  # interface is up, but if we bring it up immediately, it will break adb.
  #sleep 1
  #ifconfig rndis0 192.168.42.2 netmask 255.255.255.0 up
  echo DONE >> %(prefix)s.log
}

doit &
    """ % {'dev': self._RNDIS_DEVICE, 'functions': 'rndis,adb',
           'prefix': script_prefix }
    self._adb.SetFileContents('%s.sh' % script_prefix, script)
    # TODO(szym): run via su -c if necessary.
    self._adb.RunShellCommand('rm %s.log' % script_prefix)
    self._adb.RunShellCommand('. %s.sh' % script_prefix)
    self._WaitForDevice()
    result = self._adb.GetFileContents('%s.log' % script_prefix)
    assert any('DONE' in line for line in result), 'RNDIS script did not run!'

  def _CheckEnableRndis(self, force):
    """Enables the RNDIS network interface, retrying if necessary.
    Args:
      force: Disable RNDIS first, even if it appears already enabled.
    Returns:
      device_iface: RNDIS interface name on the device
      host_iface: corresponding interface name on the host
    """
    for _ in range(3):
      if not force:
        device_iface = self._FindDeviceRndisInterface()
        if device_iface:
          host_iface = self._FindHostRndisInterface()
          if host_iface:
            return device_iface, host_iface
      self._DisableRndis()
      self._EnableRndis()
      force = False
    raise Exception('Could not enable RNDIS, giving up.')

  def _GetHostAddresses(self, iface):
    """Returns the IP addresses on host's interfaces, breaking out |iface|."""
    interface_list = self._EnumerateHostInterfaces()
    addresses = []
    iface_address = None
    found_iface = False
    for line in interface_list:
      if not line.startswith(' '):
        found_iface = iface in line
      match = re.search('(?<=inet )\S+', line)
      if match:
        address = match.group(0)
        if found_iface:
          assert not iface_address, (
            'Found %s twice when parsing host interfaces.' % iface)
          iface_address = address
        else:
          addresses.append(address)
    return addresses, iface_address

  def _GetDeviceAddresses(self, excluded_iface):
    """Returns the IP addresses on all connected devices.
    Excludes interface |excluded_iface| on the selected device.
    """
    my_device = self._adb.GetDevice()
    addresses = []
    for device in adb_commands.GetAttachedDevices():
      adb = adb_commands.AdbCommands(device).Adb()
      if device == my_device:
        excluded = excluded_iface
      else:
        excluded = 'no interfaces excluded on other devices'
      addresses += [line.split()[2] for line in adb.RunShellCommand('netcfg')
                    if excluded not in line]
    return addresses

  def _ConfigureNetwork(self, device_iface, host_iface):
    """Configures the |device_iface| to be on the same network as |host_iface|.
    """
    def _Ip2Long(addr):
      return struct.unpack('!L', socket.inet_aton(addr))[0]

    def _Long2Ip(value):
      return socket.inet_ntoa(struct.pack('!L', value))

    def _Length2Mask(length):
      return 0xFFFFFFFF & ~((1 << (32 - length)) - 1)

    def _IpPrefix2AddressMask(addr):
      addr, masklen = addr.split('/')
      return _Ip2Long(addr), _Length2Mask(int(masklen))

    def _IsNetworkUnique(network, addresses):
      return all((addr & mask != network & mask) for addr, mask in addresses)

    def _NextUnusedAddress(network, netmask, used_addresses):
      # Excludes '0' and broadcast.
      for suffix in range(1, 0xFFFFFFFF & ~netmask):
        candidate = network | suffix
        if candidate not in used_addresses:
          return candidate

    orig_interfaces = open(self._NETWORK_INTERFACES, 'r').read()
    if self._TELEMETRY_MARKER not in orig_interfaces:
      interfaces = '\n'.join([
          orig_interfaces,
          '',
          self._TELEMETRY_MARKER,
          'auto %s' % host_iface,
          'iface %s inet static' % host_iface,
          '  address 192.168.123.1',
          '  netmask 255.255.255.0',
          ])
      subprocess.call(['sudo', 'stop', 'network-manager'])
      self._WriteProtectedFile(self._NETWORK_INTERFACES, interfaces)
      subprocess.check_call(['sudo', '/etc/init.d/networking', 'restart'])

    def HasHostAddress():
      _, host_address = self._GetHostAddresses(host_iface)
      return bool(host_address)
    logging.info('Waiting for RNDIS connectivity...')
    util.WaitFor(HasHostAddress, 10)

    addresses, host_address = self._GetHostAddresses(host_iface)
    assert host_address, 'Interface %s could not be configured.' % host_iface

    addresses = [_IpPrefix2AddressMask(addr) for addr in addresses]
    host_ip, netmask = _IpPrefix2AddressMask(host_address)

    network = host_ip & netmask

    if not _IsNetworkUnique(network, addresses):
      logging.warning(
        'The IP address configuration %s of %s is not unique!\n'
        'Check your /etc/network/interfaces. If this overlap is intended,\n'
        'you might need to use: ip rule add from <device_ip> lookup <table>\n'
        'or add the interface to a bridge in order to route to this network.'
        % (host_address, host_iface)
      )

    # Find unused IP address.
    used_addresses = [addr for addr, _ in addresses]
    used_addresses += [_IpPrefix2AddressMask(addr)[0]
                       for addr in self._GetDeviceAddresses(device_iface)]
    used_addresses += [host_ip]

    device_ip = _NextUnusedAddress(network, netmask, used_addresses)
    assert device_ip, ('The network %s on %s is full.' %
                       (host_address, host_iface))

    host_ip = _Long2Ip(host_ip)
    device_ip = _Long2Ip(device_ip)
    netmask = _Long2Ip(netmask)

    # TODO(szym) run via su -c if necessary.
    self._adb.RunShellCommand('ifconfig %s %s netmask %s up' %
                              (device_iface, device_ip, netmask))
    # Enabling the interface sometimes breaks adb.
    self._WaitForDevice()
    self._host_iface = host_iface
    self._host_ip = host_ip
    self.device_iface = device_iface
    self._device_ip = device_ip

  def _TestConnectivity(self):
    with open(os.devnull, 'wb') as devnull:
      return subprocess.call(['ping', '-q', '-c1', '-W1',
                              '-I', self._host_iface, self._device_ip],
                              stdout=devnull) == 0

  def _CheckConfigureNetwork(self):
    """Enables RNDIS and configures it, retrying until we have connectivity."""
    force = False
    for _ in range(3):
      device_iface, host_iface = self._CheckEnableRndis(force)
      self._ConfigureNetwork(device_iface, host_iface)
      if self._TestConnectivity():
        return
      force = True
    raise Exception('No connectivity, giving up.')
