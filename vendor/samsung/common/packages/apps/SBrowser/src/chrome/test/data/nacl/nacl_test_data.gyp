# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'includes': [
    '../../../../ppapi/ppapi_nacl_test_common.gypi',
  ],
  'targets': [
    {
      'target_name': 'shared_test_files',
      'type': 'none',
      'variables': {
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'test_files': [
          # TODO(ncbray) move into chrome/test/data/nacl when all tests are
          # converted.
          '<(DEPTH)/ppapi/native_client/tests/ppapi_browser/progress_event_listener.js',
          '<(DEPTH)/ppapi/native_client/tools/browser_tester/browserdata/nacltest.js',
          # Files that aren't assosiated with any particular executable.
          'bad/ppapi_bad.html',
          'bad/ppapi_bad.js',
          'bad/ppapi_bad_native.html',
          'bad/ppapi_bad_doesnotexist.nmf',
          'bad/ppapi_bad_magic.nmf',
          'bad/ppapi_bad_manifest_uses_nexes.nmf',
          'bad/ppapi_bad_manifest_bad_files.nmf',
          'bad/ppapi_bad_manifest_nexe_arch.nmf',
          'crash/ppapi_crash.html',
          'manifest_file/test_file.txt',
        ],
      },
    },
    {
      'target_name': 'simple_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'simple',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'nacl_load_test.html',
        ],
      },
    },
    {
      'target_name': 'exit_status_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_exit_status_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'exit_status/pm_exit_status_test.cc',
        ],
        'test_files': [
          'exit_status/pm_exit_status_test.html',
        ],
      },
    },
    {
      'target_name': 'sysconf_nprocessors_onln_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'sysconf_nprocessors_onln_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          'sysconf_nprocessors_onln/sysconf_nprocessors_onln_test.cc',
        ],
        'test_files': [
          'sysconf_nprocessors_onln/sysconf_nprocessors_onln_test.html',
        ],
      },
    },
    {
      'target_name': 'ppapi_test_lib',
      'type': 'none',
      'variables': {
        'nlib_target': 'libppapi_test_lib.a',
        'nso_target': 'libppapi_test_lib.so',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          # TODO(ncbray) move these files once SCons no longer depends on them.
          '../../../../ppapi/native_client/tests/ppapi_test_lib/get_browser_interface.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/internal_utils.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/module_instance.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/testable_callback.cc',
          '../../../../ppapi/native_client/tests/ppapi_test_lib/test_interface.cc',
        ]
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'nacl_ppapi_util',
      'type': 'none',
      'variables': {
        'nlib_target': 'libnacl_ppapi_util.a',
        'nso_target': 'libnacl_ppapi_util.so',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'sources': [
          # TODO(ncbray) move these files once SCons no longer depends on them.
          '../../../../ppapi/native_client/src/untrusted/nacl_ppapi_util/string_buffer.cc',
          '../../../../ppapi/native_client/src/untrusted/nacl_ppapi_util/nacl_ppapi_util.cc',
          '../../../../ppapi/native_client/src/untrusted/nacl_ppapi_util/ppapi_srpc_main.c',
        ]
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ],
    },
    {
      'target_name': 'ppapi_progress_events',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_progress_events',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'progress_events/ppapi_progress_events.cc',
        ],
        'test_files': [
          'progress_events/ppapi_progress_events.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_initialize',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_initialize',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_initialize.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_initialize_crash',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_initialize_crash',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_initialize_crash.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_no_ppp_instance',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_no_ppp_instance',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_no_ppp_instance.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_get_ppp_instance_crash',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_get_ppp_instance_crash',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_get_ppp_instance_crash.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_instance_didcreate',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_instance_didcreate',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_instance_didcreate.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_bad_ppp_instance_didcreate_crash',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_bad_ppp_instance_didcreate_crash',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'bad/ppapi_bad_ppp_instance_didcreate_crash.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_via_check_failure',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_via_check_failure',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_via_check_failure.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_via_exit_call',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_via_exit_call',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_via_exit_call.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_in_callback',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_in_callback',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_in_callback.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_off_main_thread',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_off_main_thread',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_off_main_thread.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_crash_ppapi_off_main_thread',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_crash_ppapi_off_main_thread',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'crash/ppapi_crash_ppapi_off_main_thread.cc',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pm_redir_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_redir_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'postmessage_redir/pm_redir_test.cc',
        ],
        'test_files': [
          'postmessage_redir/pm_redir_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
      ],
    },
    {
      'target_name': 'pm_manifest_file',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_manifest_file',
        'build_newlib': 1,
        'build_glibc': 1,
        # TODO(ncbray) support file injection into PNaCl manifest.
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lnacl_ppapi_util',
          '-lppapi_cpp',
          '-lppapi',
          '-lsrpc',
          '-lplatform',
          '-lgio',
          '-limc',
          '-limc_syscalls',
          '-lweak_ref',
        ],
        'sources': [
          'manifest_file/pm_manifest_file_test.cc',
        ],
        'create_nmf_args_portable': [
          '-xtest_file:test_file.txt',
          '-xnmf says hello world:test_file.txt',
        ],
        'test_files': [
          'manifest_file/pm_manifest_file_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:srpc_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.gyp:weak_ref_lib',
        'nacl_ppapi_util',
      ],
    },
    {
      'target_name': 'pm_pre_init_manifest_file',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_pre_init_manifest_file',
        'build_newlib': 1,
        'build_glibc': 1,
        # TODO(ncbray) support file injection into PNaCl manifest.
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lnacl_ppapi_util',
          '-lppapi_cpp',
          '-lppapi',
          '-lsrpc',
          '-lplatform',
          '-lgio',
          '-limc',
          '-limc_syscalls',
          '-lweak_ref',
        ],
        'sources': [
          'manifest_file/pm_pre_init_manifest_file_test.cc',
        ],
        'create_nmf_args_portable': [
          '-xtest_file:test_file.txt',
          '-xnmf says hello world:test_file.txt',
        ],
        'test_files': [
          'manifest_file/pm_pre_init_manifest_file_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:srpc_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.gyp:weak_ref_lib',
        'nacl_ppapi_util',
      ],
    },
    {
      'target_name': 'irt_manifest_file',
      'type': 'none',
      'variables': {
        'nexe_target': 'irt_manifest_file',
        'build_newlib': 1,
        # Linking problems - can't find __nacl_irt_query.
        'build_glibc': 0,
        # TODO(ncbray) support file injection into PNaCl manifest.
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lnacl_ppapi_util',
          '-lppapi_cpp',
          '-lppapi',
          '-lsrpc',
          '-lplatform',
          '-lgio',
          '-limc',
          '-limc_syscalls',
          '-lweak_ref',
          '-lnacl',
        ],
        'sources': [
          'manifest_file/irt_manifest_file_test.cc',
        ],
        'create_nmf_args_portable': [
          '-xtest_file:test_file.txt',
          '-xnmf says hello world:test_file.txt',
        ],
        'test_files': [
          'manifest_file/irt_manifest_file_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:srpc_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_lib',
        '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.gyp:weak_ref_lib',
        'nacl_ppapi_util',
      ],
    },
    {
      'target_name': 'pm_nameservice_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pm_nameservice_test',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lnacl_ppapi_util',
          '-lppapi_cpp',
          '-lppapi',
          '-lsrpc',
          '-lplatform',
          '-lgio',
          '-limc',
          '-limc_syscalls',
          '-lweak_ref',
        ],
        'sources': [
          'nameservice/pm_nameservice_test.cc',
        ],
        'test_files': [
          'nameservice/pm_nameservice_test.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/native_client/src/shared/srpc/srpc.gyp:srpc_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/imc/imc.gyp:imc_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:imc_syscalls_lib',
        '<(DEPTH)/native_client/src/trusted/weak_ref/weak_ref.gyp:weak_ref_lib',
        'nacl_ppapi_util',
      ],
    },
    {
      'target_name': 'ppapi_extension_mime_handler',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_extension_mime_handler',
        'build_newlib': 1,
        'build_glibc': 0,
        'build_pnacl_newlib': 0,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'extension_mime_handler/ppapi_extension_mime_handler.cc',
        ],
        'test_files': [
          'extension_mime_handler/ppapi_extension_mime_handler.html',
          'extension_mime_handler/mime_test_data.dat',
          # For faking the file's MIME type.
          'extension_mime_handler/mime_test_data.dat.mock-http-headers',
          # Turns the test data directory into an extension.  Hackish.
          # Note that the .nexe names are embedded in this file.
          'extension_mime_handler/manifest.json',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pnacl_error_handling_test',
      'type': 'none',
      'variables': {
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        # No need to translate AOT.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        # Use prebuilt NMF files.
        'generate_nmf': 0,
        'test_files': [
          'pnacl_error_handling/pnacl_error_handling.html',
          'pnacl_error_handling/bad.pexe',
          'pnacl_error_handling/pnacl_bad_pexe.nmf',
          'pnacl_error_handling/pnacl_bad_doesnotexist.nmf',
          'pnacl_error_handling/pnacl_illformed_manifest.nmf',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ]
    },
    {
      'target_name': 'pnacl_mime_type_test',
      'type': 'none',
      'variables': {
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        # No need to translate AOT.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        'test_files': [
          'pnacl_mime_type/pnacl_mime_type.html',
        ],
      },
    },
    {
      'target_name': 'pnacl_options_test',
      'type': 'none',
      'variables': {
        'nexe_target': 'pnacl_options',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        # No need to translate these AOT, when we just need the pexe.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        'sources': [
          'simple.cc',
        ],
        'test_files': [
          'pnacl_nmf_options/pnacl_options.html',
          'pnacl_nmf_options/pnacl_o_0.nmf',
          'pnacl_nmf_options/pnacl_o_2.nmf',
          'pnacl_nmf_options/pnacl_o_large.nmf',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
      ]
    },
    {
      'target_name': 'pnacl_dyncode_syscall_disabled_test',
      'type': 'none',
      'variables': {
        # This tests that nexes produced by translation in the browser are not
        # able to use the dyncode syscalls.  Pre-translated nexes are not
        # subject to this constraint, so we do not test them.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        'nexe_target': 'pnacl_dyncode_syscall_disabled',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
          # The "_private" variant of the library calls the syscalls
          # directly, which allows us to test the syscalls directly,
          # even when the dyncode IRT interface is also disabled under
          # PNaCl.
          '-lnacl_dyncode_private',
        ],
        'sources': [
          'pnacl_dyncode_syscall_disabled/pnacl_dyncode_syscall_disabled.cc',
        ],
        'test_files': [
          'pnacl_dyncode_syscall_disabled/pnacl_dyncode_syscall_disabled.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_dyncode_private_lib',
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'pnacl_exception_handling_disabled_test',
      'type': 'none',
      'variables': {
        # This tests that nexes produced by translation in the browser are not
        # able to use hardware exception handling.  Pre-translated nexes are
        # not subject to this constraint, so we do not test them.
        'enable_x86_32': 0,
        'enable_x86_64': 0,
        'enable_arm': 0,
        'nexe_target': 'pnacl_exception_handling_disabled',
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
          # The "_private" variant of the library calls the syscalls
          # directly, which allows us to test the syscalls directly,
          # even when the exception-handling IRT interface is also
          # disabled under PNaCl.
          '-lnacl_exception_private',
        ],
        'sources': [
          'pnacl_exception_handling_disabled/pnacl_exception_handling_disabled.cc',
        ],
        'test_files': [
          'pnacl_exception_handling_disabled/pnacl_exception_handling_disabled.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/untrusted/nacl/nacl.gyp:nacl_exception_private_lib',
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    # Legacy NaCl PPAPI interface tests being here.
    {
      'target_name': 'ppapi_ppb_core',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppb_core',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppb_core/ppapi_ppb_core.cc',
        ],
        'test_files': [
          'ppapi/ppb_core/ppapi_ppb_core.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_ppb_instance',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppb_instance',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppb_instance/ppapi_ppb_instance.cc',
        ],
        'test_files': [
          'ppapi/ppb_instance/ppapi_ppb_instance.html',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
    {
      'target_name': 'ppapi_ppp_instance',
      'type': 'none',
      'variables': {
        'nexe_target': 'ppapi_ppp_instance',
        'build_newlib': 1,
        'build_glibc': 1,
        'build_pnacl_newlib': 1,
        'nexe_destination_dir': 'nacl_test_data',
        'link_flags': [
          '-lppapi',
          '-lppapi_test_lib',
          '-lplatform',
          '-lgio',
        ],
        'sources': [
          'ppapi/ppp_instance/ppapi_ppp_instance.cc',
        ],
        'test_files': [
          'ppapi/ppp_instance/ppapi_ppp_instance.html',
          'ppapi/ppp_instance/ppapi_ppp_instance.js',
        ],
      },
      'dependencies': [
        '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
        '<(DEPTH)/native_client/src/shared/platform/platform.gyp:platform_lib',
        '<(DEPTH)/native_client/src/shared/gio/gio.gyp:gio_lib',
        '<(DEPTH)/ppapi/native_client/native_client.gyp:ppapi_lib',
        '<(DEPTH)/ppapi/ppapi_untrusted.gyp:ppapi_cpp_lib',
        'ppapi_test_lib',
      ],
    },
  ],
  'conditions': [
    ['target_arch!="arm"', {
      # Source file does not have asm for ARM.
      'targets': [
        {
          'target_name': 'partly_invalid',
          'type': 'none',
          'variables': {
            'nexe_target': 'partly_invalid',
            'build_newlib': 1,
            'build_glibc': 0,
            'build_pnacl_newlib': 0,
            'nexe_destination_dir': 'nacl_test_data',
            'sources': [
              '<(DEPTH)/native_client/tests/stubout_mode/partly_invalid.c',
            ],
          },
          'dependencies': [
            '<(DEPTH)/native_client/tools.gyp:prep_toolchain',
          ],
        },
      ],
    }],
  ],
}
