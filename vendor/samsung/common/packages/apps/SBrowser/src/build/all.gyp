# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    # A hook that can be overridden in other repositories to add additional
    # compilation targets to 'All'. Only used on Android.
    'android_app_targets%': [],
  },
  'targets': [
    {
      'target_name': 'All',
      'type': 'none',
      'xcode_create_dependents_test_runner': 1,
      'dependencies': [
        'some.gyp:*',
        '../base/base.gyp:*',
        '../components/components.gyp:*',
        '../components/components_tests.gyp:*',
        '../content/content.gyp:*',
        '../crypto/crypto.gyp:*',
        '../net/net.gyp:*',
        '../sdch/sdch.gyp:*',
        '../sql/sql.gyp:*',
        '../testing/gmock.gyp:*',
        '../testing/gtest.gyp:*',
        '../third_party/icu/icu.gyp:*',
        '../third_party/libxml/libxml.gyp:*',
        '../third_party/sqlite/sqlite.gyp:*',
        '../third_party/zlib/zlib.gyp:*',
        '../ui/accessibility/accessibility.gyp:*',
        '../ui/snapshot/snapshot.gyp:*',
        '../ui/ui.gyp:*',
        '../url/url.gyp:*',
      ],
      'conditions': [
        ['OS=="ios"', {
          'dependencies': [
            '../ios/ios.gyp:*',
            '../ui/gfx/gfx.gyp:gfx_unittests',
            '../ui/ui_unittests.gyp:ui_unittests',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            '../content/content_shell_and_tests.gyp:content_shell_apk',
            '../mojo/mojo.gyp:mojo_shell_apk',
            '<@(android_app_targets)',
            'android_builder_tests',
            '../android_webview/android_webview.gyp:android_webview_apk',
            '../chrome/chrome.gyp:chromium_testshell',
            '../remoting/remoting.gyp:remoting_apk',
            '../tools/telemetry/telemetry.gyp:*#host',
            # TODO(nyquist) This should instead by a target for sync when all of
            # the sync-related code for Android has been upstreamed.
            # See http://crbug.com/159203
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_javalib',
          ],
          'conditions': [
            ['enable_s_android_browser==1', {
              'dependencies': [
                
              ],
            }],
          ],
        }, {
          'dependencies': [
            '../content/content_shell_and_tests.gyp:*',
            # TODO: This should build on Android and the target should move to the list above.
            '../sync/sync.gyp:*',
          ],
        }],
        ['OS!="ios" and OS!="android"', {
          'dependencies': [
            '../third_party/re2/re2.gyp:re2',
            '../chrome/chrome.gyp:*',
            '../cc/cc_tests.gyp:*',
            '../device/bluetooth/bluetooth.gyp:*',
            '../device/device_tests.gyp:*',
            '../device/usb/usb.gyp:*',
            '../gin/gin.gyp:*',
            '../gpu/gpu.gyp:*',
            '../gpu/tools/tools.gyp:*',
            '../ipc/ipc.gyp:*',
            '../jingle/jingle.gyp:*',
            '../media/cast/cast.gyp:*',
            '../media/media.gyp:*',
            '../mojo/mojo.gyp:*',
            '../ppapi/ppapi.gyp:*',
            '../ppapi/ppapi_internal.gyp:*',
            '../ppapi/tools/ppapi_tools.gyp:*',
            '../printing/printing.gyp:*',
            '../skia/skia.gyp:*',
            '../third_party/WebKit/public/all.gyp:*',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:*',
            '../third_party/codesighs/codesighs.gyp:*',
            '../third_party/ffmpeg/ffmpeg.gyp:*',
            '../third_party/iccjpeg/iccjpeg.gyp:*',
            '../third_party/libpng/libpng.gyp:*',
            '../third_party/libusb/libusb.gyp:*',
            '../third_party/libwebp/libwebp.gyp:*',
            '../third_party/libxslt/libxslt.gyp:*',
            '../third_party/lzma_sdk/lzma_sdk.gyp:*',
            '../third_party/mesa/mesa.gyp:*',
            '../third_party/modp_b64/modp_b64.gyp:*',
            '../third_party/npapi/npapi.gyp:*',
            '../third_party/ots/ots.gyp:*',
            '../third_party/qcms/qcms.gyp:*',
            '../tools/gn/gn.gyp:*',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../tools/telemetry/telemetry.gyp:*',
            '../v8/tools/gyp/v8.gyp:*',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:*',
            '<(libjpeg_gyp_path):*',
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          'dependencies': [
            '../chrome/tools/profile_reset/jtl_compiler.gyp:*',
          ],
        }],
        ['os_posix==1 and OS!="android" and OS!="ios"', {
          'dependencies': [
            '../third_party/yasm/yasm.gyp:*#host',
          ],
        }],
        ['OS=="mac" or OS=="ios" or OS=="win"', {
          'dependencies': [
            '../third_party/nss/nss.gyp:*',
           ],
        }],
        ['OS=="win" or OS=="ios" or OS=="linux"', {
          'dependencies': [
            '../breakpad/breakpad.gyp:*',
           ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../third_party/ocmock/ocmock.gyp:*',
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../courgette/courgette.gyp:*',
            '../dbus/dbus.gyp:*',
            '../sandbox/sandbox.gyp:*',
          ],
          'conditions': [
            ['branding=="Chrome"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_packages_<(channel)',
              ],
            }],
            ['chromeos==0', {
              'dependencies': [
                '../third_party/cros_dbus_cplusplus/cros_dbus_cplusplus.gyp:*',
                '../third_party/libmtp/libmtp.gyp:*',
                '../third_party/mtpd/mtpd.gyp:*',
              ],
            }],
            ['enable_ipc_fuzzer==1', {
              'dependencies': [
                '../tools/ipc_fuzzer/ipc_fuzzer.gyp:*',
              ],
            }],
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../tools/xdisplaycheck/xdisplaycheck.gyp:*',
          ],
        }],
        ['toolkit_uses_gtk==1', {
          'dependencies': [
            '../tools/gtk_clipboard_dump/gtk_clipboard_dump.gyp:*',
          ],
        }],
        ['OS=="win"', {
          'conditions': [
            ['win_use_allocator_shim==1', {
              'dependencies': [
                '../base/allocator/allocator.gyp:*',
              ],
            }],
            # Don't enable dependencies that don't work on Win64.
            ['target_arch!="x64"', {
              'dependencies': [
                # TODO(jschuh) Enable Win64 Memory Watcher. crbug.com/176877
                '../tools/memory_watcher/memory_watcher.gyp:*',
              ],
            }],
          ],
          'dependencies': [
            '../chrome_elf/chrome_elf.gyp:*',
            '../cloud_print/cloud_print.gyp:*',
            '../courgette/courgette.gyp:*',
            '../rlz/rlz.gyp:*',
            '../sandbox/sandbox.gyp:*',
            '<(angle_path)/src/build_angle.gyp:*',
            '../third_party/bspatch/bspatch.gyp:*',
            '../tools/win/static_initializers/static_initializers.gyp:*',
          ],
        }, {
          'dependencies': [
            '../third_party/libevent/libevent.gyp:*',
          ],
        }],
        ['toolkit_views==1', {
          'dependencies': [
            '../ui/views/controls/webview/webview.gyp:*',
            '../ui/views/views.gyp:*',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/aura/aura.gyp:*',
            '../ui/oak/oak.gyp:*',
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
            '../ash/ash.gyp:*',
          ],
        }],
        ['remoting==1', {
          'dependencies': [
            '../remoting/remoting.gyp:*',
          ],
        }],
        ['use_openssl==0', {
          'dependencies': [
            '../net/third_party/nss/ssl.gyp:*',
          ],
        }],
        ['enable_app_list==1', {
          'dependencies': [
            '../ui/app_list/app_list.gyp:*',
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          'dependencies': [
            '../google_apis/gcm/gcm.gyp:*',
          ],
        }],
      ],
    }, # target_name: All
    {
      'target_name': 'All_syzygy',
      'type': 'none',
      'conditions': [
        ['OS=="win" and fastbuild==0 and target_arch=="ia32"', {
          'dependencies': [
            '../chrome/installer/mini_installer_syzygy.gyp:*',
          ],
        }],
      ],
    }, # target_name: All_syzygy
    {
      # Note: Android uses android_builder_tests below.
      # TODO: Consider merging that with this target.
      'target_name': 'chromium_builder_tests',
      'type': 'none',
      'dependencies': [
        '../base/base.gyp:base_unittests',
        '../components/components_tests.gyp:components_unittests',
        '../crypto/crypto.gyp:crypto_unittests',
        '../net/net.gyp:net_unittests',
        '../sql/sql.gyp:sql_unittests',
        '../sync/sync.gyp:sync_unit_tests',
        '../ui/gfx/gfx.gyp:gfx_unittests',
        '../ui/ui_unittests.gyp:ui_unittests',
        '../url/url.gyp:url_unittests',
      ],
      'conditions': [
        ['OS!="ios" and OS!="android"', {
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chromedriver_tests',
            '../chrome/chrome.gyp:chromedriver_unittests',
            # TODO(kkania): Remove these after infra no longer depends on them.
            '../chrome/chrome.gyp:chromedriver2_unittests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_shell',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../gin/gin.gyp:gin_unittests',
            '../google_apis/google_apis.gyp:google_apis_unittests',
            '../gpu/gles2_conform_support/gles2_conform_support.gyp:gles2_conform_support',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/cast/cast.gyp:cast_unittests',
            '../media/media.gyp:media_unittests',
            '../mojo/mojo.gyp:mojo',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/WebKit/public/all.gyp:all_blink',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/telemetry/telemetry.gyp:*',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../chrome/chrome.gyp:crash_service',
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:mini_installer_test',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../chrome_elf/chrome_elf.gyp:chrome_elf_unittests',
            '../content/content_shell_and_tests.gyp:copy_test_netscape_plugin',
            '../courgette/courgette.gyp:courgette_unittests',
            '../sandbox/sandbox.gyp:sbox_integration_tests',
            '../sandbox/sandbox.gyp:sbox_unittests',
            '../sandbox/sandbox.gyp:sbox_validation_tests',
            '../ui/app_list/app_list.gyp:app_list_unittests',
            '../ui/views/views.gyp:views_unittests',
          ],
          'conditions': [
            # remoting_host_installation uses lots of non-trivial GYP that tend
            # to break because of differences between ninja and msbuild. Make
            # sure this target is built by the builders on the main waterfall.
            # See http://crbug.com/180600.
            ['wix_exists == "True" and sas_dll_exists == "True"', {
              'dependencies': [
                '../remoting/remoting.gyp:remoting_host_installation',
              ],
            }],
            ['syzyasan==1', {
              'variables': {
                # Disable incremental linking for all modules.
                # 0: inherit, 1: disabled, 2: enabled.
                'msvs_debug_link_incremental': '1',
                'msvs_large_module_debug_link_mode': '1',
                # Disable RTC. Syzygy explicitly doesn't support RTC
                # instrumented binaries for now.
                'win_debug_RuntimeChecks': '0',
              },
              'defines': [
                # Disable iterator debugging (huge speed boost).
                '_HAS_ITERATOR_DEBUGGING=0',
              ],
              'msvs_settings': {
                'VCLinkerTool': {
                  # Enable profile information (necessary for SyzyAsan
                  # instrumentation). This is incompatible with incremental
                  # linking.
                  'Profile': 'true',
                },
              }
            }],
          ],
        }],
        ['OS=="linux"', {
          'dependencies': [
            '../dbus/dbus.gyp:dbus_unittests',
            '../sandbox/sandbox.gyp:sandbox_linux_unittests',
          ],
        }],
        ['OS=="mac"', {
          'dependencies': [
            '../ui/app_list/app_list.gyp:app_list_unittests',
            '../ui/message_center/message_center.gyp:*',
          ],
        }],
        ['test_isolation_mode != "noop"', {
          'dependencies': [
            'chromium_swarm_tests',
          ],
        }],
        ['OS!="android" and OS!="ios"', {
          'dependencies': [
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
          ],
        }],
        ['enable_printing!=0', {
          'dependencies': [
            '../printing/printing.gyp:printing_unittests',
          ],
        }],
        ['use_aura==1', {
          'dependencies': [
            '../ui/app_list/app_list.gyp:app_list_unittests',
            '../ui/aura/aura.gyp:aura_unittests',
            '../ui/compositor/compositor.gyp:compositor_unittests',
            '../ui/keyboard/keyboard.gyp:keyboard_unittests',
          ],
        }],
        ['use_aura==1 or toolkit_views==1', {
          'dependencies': [
            '../ui/events/events.gyp:events_unittests',
          ],
        }],
        ['use_ash==1', {
          'dependencies': [
            '../ash/ash.gyp:ash_unittests',
          ],
        }],
      ],
    }, # target_name: chromium_builder_tests
    {
      'target_name': 'chromium_2010_builder_tests',
      'type': 'none',
      'dependencies': [
        'chromium_builder_tests',
      ],
    }, # target_name: chromium_2010_builder_tests
  ],
  'conditions': [
    ['OS!="ios"', {
      'targets': [
        {
          'target_name': 'blink_tests',
          'type': 'none',
          'dependencies': [
            '../third_party/WebKit/public/all.gyp:all_blink',
          ],
          'conditions': [
            ['OS=="android" and disable_breakpad!=1', {
              'dependencies': [
                '../content/content_shell_and_tests.gyp:content_shell_apk',
                '../breakpad/breakpad.gyp:dump_syms#host',
                '../breakpad/breakpad.gyp:minidump_stackwalk#host',
              ],
            }, {  # OS!="android"
              'dependencies': [
                '../content/content_shell_and_tests.gyp:content_shell',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../content/content_shell_and_tests.gyp:content_shell_crash_service',
                '../content/content_shell_and_tests.gyp:layout_test_helper',
              ],
            }],
            ['OS!="win" and OS!="android"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="mac"', {
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms#host',
                '../content/content_shell_and_tests.gyp:layout_test_helper',
              ],
            }],
            ['OS=="linux" and disable_breakpad!=1', {
              'dependencies': [
                '../breakpad/breakpad.gyp:dump_syms',
              ],
            }],
          ],
        }, # target_name: blink_tests
        {
          # TODO(jochen): Eventually remove this target after everybody and
          # the bots started to use blink_tests only.
          'target_name': 'all_webkit',
          'type': 'none',
          'dependencies': [
            'blink_tests',
          ],
        }, # target_name: all_webkit
      ],
    }], # OS!=ios
    ['OS!="ios" and OS!="android"', {
      'targets': [
        {
          'target_name': 'chromium_builder_nacl_win_integration',
          'type': 'none',
          'dependencies': [
            'chromium_builder_qa', # needed for pyauto
            'chromium_builder_tests',
          ],
        }, # target_name: chromium_builder_nacl_win_integration
        {
          'target_name': 'chromium_builder_perf',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_perftests',
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../chrome/chrome.gyp:sync_performance_tests',
            '../media/media.gyp:media_perftests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../tools/telemetry/telemetry.gyp:*',
          ],
          'conditions': [
            ['OS!="ios" and OS!="win" and disable_breakpad!=1', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_symbols'
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service_win64',
              ],
            }],
          ],
        }, # target_name: chromium_builder_perf
        {
          'target_name': 'chromium_gpu_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_gl_tests',
            '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',
            '../gpu/gpu.gyp:gl_tests',
            '../gpu/gpu.gyp:angle_unittests',
            '../tools/telemetry/telemetry.gyp:*',
          ],
          'conditions': [
            ['OS!="ios" and OS!="win" and disable_breakpad!=1', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_symbols'
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service_win64',
              ],
            }],
          ],
        }, # target_name: chromium_gpu_builder
        {
          'target_name': 'chromium_gpu_debug_builder',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_gl_tests',
            '../gpu/gles2_conform_support/gles2_conform_test.gyp:gles2_conform_test',
            '../gpu/gpu.gyp:gl_tests',
            '../gpu/gpu.gyp:angle_unittests',
            '../tools/telemetry/telemetry.gyp:*',
          ],
          'conditions': [
            ['OS!="ios" and OS!="win" and disable_breakpad!=1', {
              'dependencies': [
                '../breakpad/breakpad.gyp:minidump_stackwalk',
              ],
            }],
            ['OS=="linux"', {
              'dependencies': [
                '../chrome/chrome.gyp:linux_symbols'
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service_win64',
              ],
            }],
          ],
        }, # target_name: chromium_gpu_debug_builder
        {
          'target_name': 'chromium_builder_qa',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',
            # Dependencies of pyauto_functional tests.
            '../remoting/remoting.gyp:remoting_webapp',
            '../chrome/chrome.gyp:pyautolib',
          ],
          'conditions': [
            ['OS=="mac"', {
              'dependencies': [
                '../remoting/remoting.gyp:remoting_me2me_host_archive',
              ],
            }],
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service_win64',
              ],
            }],
            ['OS=="win" and component != "shared_library" and wix_exists == "True" and sas_dll_exists == "True"', {
              'dependencies': [
                '../remoting/remoting.gyp:remoting_host_installation',
              ],
            }],
          ],
        }, # target_name: chromium_builder_qa
        {
          'target_name': 'chromium_builder_perf_av',
          'type': 'none',
          'dependencies': [
            'blink_tests', # to run layout tests
            'chromium_builder_qa',  # needed for perf pyauto tests
          ],
        },  # target_name: chromium_builder_perf_av
        {
          # This target contains everything we need to run tests on the special
          # device-equipped WebRTC bots. We have device-requiring tests in
          # PyAuto, browser_tests and content_browsertests.
          'target_name': 'chromium_builder_webrtc',
          'type': 'none',
          'dependencies': [
            'chromium_builder_qa',  # needed for perf pyauto tests
            '../chrome/chrome.gyp:browser_tests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../third_party/libjingle/libjingle.gyp:peerconnection_server',
            '../third_party/webrtc/tools/tools.gyp:frame_analyzer',
            '../third_party/webrtc/tools/tools.gyp:rgba_to_i420_converter',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service',
              ],
            }],
          ],
        },  # target_name: chromium_builder_webrtc
        {
          'target_name': 'chromium_builder_chromedriver',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chromedriver',
            '../chrome/chrome.gyp:chromedriver_tests',
            '../chrome/chrome.gyp:chromedriver_unittests',
          ],
        },  # target_name: chromium_builder_chromedriver
        {
          'target_name': 'chromium_builder_asan',
          'type': 'none',
          'dependencies': [
            '../chrome/chrome.gyp:chrome',

            # We refer to content_shell directly rather than blink_tests
            # because we don't want the _unittests binaries.
            '../content/content_shell_and_tests.gyp:content_shell',
          ],
          'conditions': [
            ['OS!="win"', {
              'dependencies': [
                '../net/net.gyp:dns_fuzz_stub',
                '../skia/skia.gyp:filter_fuzz_stub',
              ],
            }],
            ['enable_ipc_fuzzer==1 and OS=="linux" and component!="shared_library"', {
              'dependencies': [
                '../tools/ipc_fuzzer/ipc_fuzzer.gyp:*',
              ],
            }],
            ['chromeos==0', {
              'dependencies': [
                '../v8/src/d8.gyp:d8#host',
              ],
            }],
            ['internal_filter_fuzzer==1', {
              'dependencies': [
                '../skia/tools/clusterfuzz-data/fuzzers/filter_fuzzer/filter_fuzzer.gyp:filter_fuzzer',
              ],
            }], # internal_filter_fuzzer
            ['OS=="win" and fastbuild==0 and target_arch=="ia32"', {
              'dependencies': [
                '../chrome/chrome_syzygy.gyp:chrome_dll_syzygy',
                '../content/content_shell_and_tests.gyp:content_shell_syzyasan',
              ],
              'conditions': [
                ['chrome_multiple_dll==1', {
                  'dependencies': [
                    '../chrome/chrome_syzygy.gyp:chrome_child_dll_syzygy',
                  ],
                }],
              ],
            }],
          ],
        },
      ],  # targets
    }], #OS!=ios and OS!=android
    ['OS=="android"', {
      'targets': [
        {
          # The current list of tests for android.  This is temporary
          # until the full set supported.  If adding a new test here,
          # please also add it to build/android/pylib/gtest/gtest_config.py,
          # else the test is not run.
          #
          # WARNING:
          # Do not add targets here without communicating the implications
          # on tryserver triggers and load.  Discuss with
          # chrome-infrastructure-team please.
          'target_name': 'android_builder_tests',
          'type': 'none',
          'dependencies': [
            '../android_webview/android_webview.gyp:android_webview_unittests',
            '../base/android/jni_generator/jni_generator.gyp:jni_generator_tests',
            '../base/base.gyp:base_unittests',
#            '../breakpad/breakpad.gyp:breakpad_unittests_stripped',
            # Also compile the tools needed to deal with minidumps, they are
            # needed to run minidump tests upstream.
#            '../breakpad/breakpad.gyp:dump_syms#host',
#            '../breakpad/breakpad.gyp:symupload#host',
#            '../breakpad/breakpad.gyp:minidump_dump#host',
#            '../breakpad/breakpad.gyp:minidump_stackwalk#host',
            '../build/android/tests/multiple_proguards/multiple_proguards.gyp:multiple_proguards_test_apk',
            '../cc/cc_tests.gyp:cc_perftests_apk',
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:unit_tests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_gl_tests',
            '../content/content_shell_and_tests.gyp:chromium_linker_test_apk',
            '../content/content_shell_and_tests.gyp:content_shell_test_apk',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../gpu/gpu.gyp:gl_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../media/media.gyp:media_perftests_apk',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../sandbox/sandbox.gyp:sandbox_linux_unittests_stripped',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/WebKit/public/all.gyp:*',
            '../tools/android/android_tools.gyp:android_tools',
            '../tools/android/android_tools.gyp:memconsumer',
            '../tools/android/findbugs_plugin/findbugs_plugin.gyp:findbugs_plugin_test',
            '../ui/ui_unittests.gyp:ui_unittests',
            # Required by ui_unittests.
            # TODO(wangxianzhu): It'd better let ui_unittests depend on it, but
            # this would cause circular gyp dependency which needs refactoring the
            # gyps to resolve.
            '../chrome/chrome_resources.gyp:packed_resources',
          ],
          'conditions': [
            ['"<(gtest_target_type)"=="shared_library"', {
              'dependencies': [
                # Unit test bundles packaged as an apk.
                '../android_webview/android_webview.gyp:android_webview_unittests_apk',
                '../base/base.gyp:base_unittests_apk',
                '../cc/cc_tests.gyp:cc_unittests_apk',
                '../chrome/chrome.gyp:unit_tests_apk',
                '../components/components_tests.gyp:components_unittests_apk',
                '../content/content_shell_and_tests.gyp:content_browsertests_apk',
                '../content/content_shell_and_tests.gyp:content_gl_tests_apk',
                '../content/content_shell_and_tests.gyp:content_unittests_apk',
                '../content/content_shell_and_tests.gyp:video_decode_accelerator_unittest_apk',
                '../gpu/gpu.gyp:gl_tests_apk',
                '../gpu/gpu.gyp:gpu_unittests_apk',
                '../ipc/ipc.gyp:ipc_tests_apk',
                '../media/media.gyp:media_unittests_apk',
                '../net/net.gyp:net_unittests_apk',
                '../sandbox/sandbox.gyp:sandbox_linux_jni_unittests_apk',
                '../sql/sql.gyp:sql_unittests_apk',
                '../sync/sync.gyp:sync_unit_tests_apk',
                '../ui/ui_unittests.gyp:ui_unittests_apk',
                '../android_webview/android_webview.gyp:android_webview_test_apk',
                '../chrome/chrome.gyp:chromium_testshell_test_apk',
                '../chrome/chrome.gyp:chromium_testshell_uiautomator_tests',
                '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests_apk'
              ],
            }],
            ['disable_breakpad!=1', {
            'dependencies': [
                '../breakpad/breakpad.gyp:breakpad_unittests_stripped',
                # Also compile the tools needed to deal with minidumps, they are
                # needed to run minidump tests upstream.
                '../breakpad/breakpad.gyp:dump_syms#host',
                '../breakpad/breakpad.gyp:symupload#host',
                '../breakpad/breakpad.gyp:minidump_dump#host',
                '../breakpad/breakpad.gyp:minidump_stackwalk#host',
            ],
           }],
          ],
        },
        {
          # WebRTC Android APK tests.
          'target_name': 'android_builder_webrtc',
          'type': 'none',
          'variables': {
            # Set default value for include_tests to '0'. It is normally only
            # used in WebRTC GYP files. It is set to '1' only when building
            # WebRTC for Android, inside a Chromium checkout.
            'include_tests%': 0,
          },
          'conditions': [
            ['"<(gtest_target_type)"=="shared_library" and include_tests==1', {
              'dependencies': [
                '../third_party/webrtc/build/apk_tests.gyp:*',
              ],
            }],
          ],
        },  # target_name: android_builder_webrtc
        {
          # WebRTC Chromium tests to run on Android.
          'target_name': 'android_builder_chromium_webrtc',
          'type': 'none',
          'dependencies': [
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../tools/android/android_tools.gyp:android_tools',
            '../tools/android/android_tools.gyp:memconsumer',
          ],
          'conditions': [
            ['"<(gtest_target_type)"=="shared_library"', {
              'dependencies': [
                # Unit test bundles packaged as an apk.
                '../content/content_shell_and_tests.gyp:content_browsertests_apk',
              ],
            }],
          ],
        },  # target_name: android_builder_chromium_webrtc
        {
          # Build the java portions of the binary size analysis tool.
          'target_name': 'binary_size_tool',
          'type': 'none',
          'dependencies': [
            '../tools/binary_size/binary_size.gyp:binary_size_java',
          ],
        },
      ], # targets
    }], # OS="android"
    ['OS=="mac"', {
      'targets': [
        {
          # Target to build everything plus the dmg.  We don't put the dmg
          # in the All target because developers really don't need it.
          'target_name': 'all_and_dmg',
          'type': 'none',
          'dependencies': [
            'All',
            '../chrome/chrome.gyp:build_app_dmg',
          ],
        },
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder_dbg',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../rlz/rlz.gyp:*',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../tools/telemetry/telemetry.gyp:*',
            '../ui/gfx/gfx.gyp:gfx_unittests',
            '../ui/ui_unittests.gyp:ui_unittests',
            '../url/url.gyp:url_unittests',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_rel',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../tools/telemetry/telemetry.gyp:*',
            '../ui/gfx/gfx.gyp:gfx_unittests',
            '../ui/ui_unittests.gyp:ui_unittests',
            '../url/url.gyp:url_unittests',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_mac',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          # TODO(dpranke): Update the bots to refer to 'chromium_builder_asan'.
          'target_name': 'chromium_builder_asan_mac',
          'type': 'none',
          'dependencies': [
            'chromium_builder_asan'
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_valgrind_mac',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../ui/gfx/gfx.gyp:gfx_unittests',
            '../ui/ui_unittests.gyp:ui_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
      ],  # targets
    }], # OS="mac"
    ['OS=="win"', {
      'targets': [
        # These targets are here so the build bots can use them to build
        # subsets of a full tree for faster cycle times.
        {
          'target_name': 'chromium_builder',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:gcapi_test',
            '../chrome/chrome.gyp:installer_util_unittests',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:mini_installer_test',
            '../chrome/chrome.gyp:performance_browser_tests',
            '../chrome/chrome.gyp:performance_ui_tests',
            '../chrome/chrome.gyp:sync_integration_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../content/content_shell_and_tests.gyp:copy_test_netscape_plugin',
            # mini_installer_tests depends on mini_installer. This should be
            # defined in installer.gyp.
            '../chrome/installer/mini_installer.gyp:mini_installer',
            '../courgette/courgette.gyp:courgette_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../gpu/gpu.gyp:gpu_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../sync/sync.gyp:sync_unit_tests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../tools/perf/clear_system_cache/clear_system_cache.gyp:*',
            '../tools/telemetry/telemetry.gyp:*',
            '../ui/events/events.gyp:events_unittests',
            '../ui/gfx/gfx.gyp:gfx_unittests',
            '../ui/ui_unittests.gyp:ui_unittests',
            '../ui/views/views.gyp:views_unittests',
            '../url/url.gyp:url_unittests',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_win_cf',
          'type': 'none',
        },
        {
          'target_name': 'chromium_builder_dbg_tsan_win',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'chromium_builder_dbg_drmemory_win',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests',
            '../chrome/chrome.gyp:unit_tests',
            '../chrome/chrome.gyp:browser_tests',
            '../cloud_print/cloud_print.gyp:cloud_print_unittests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../crypto/crypto.gyp:crypto_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../ipc/ipc.gyp:ipc_tests',
            '../jingle/jingle.gyp:jingle_unittests',
            '../media/media.gyp:media_unittests',
            '../net/net.gyp:net_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../printing/printing.gyp:printing_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../sql/sql.gyp:sql_unittests',
            '../third_party/cacheinvalidation/cacheinvalidation.gyp:cacheinvalidation_unittests',
            '../third_party/libaddressinput/libaddressinput.gyp:libaddressinput_unittests',
            '../third_party/libphonenumber/libphonenumber.gyp:libphonenumber_unittests',
            '../url/url.gyp:url_unittests',
          ],
        },
        {
          'target_name': 'webkit_builder_win',
          'type': 'none',
          'dependencies': [
            'blink_tests',
          ],
        },
      ],  # targets
      'conditions': [
        ['branding=="Chrome"', {
          'targets': [
            {
              'target_name': 'chrome_official_builder',
              'type': 'none',
              'dependencies': [
                '../base/base.gyp:base_unittests',
                '../chrome/chrome.gyp:crash_service',
                '../chrome/chrome.gyp:gcapi_dll',
                '../chrome/chrome.gyp:pack_policy_templates',
                '../chrome/installer/mini_installer.gyp:mini_installer',
                '../cloud_print/cloud_print.gyp:cloud_print',
                '../courgette/courgette.gyp:courgette',
                '../courgette/courgette.gyp:courgette64',
                '../ipc/ipc.gyp:ipc_tests',
                '../media/media.gyp:media_unittests',
                '../net/net.gyp:net_unittests_run',
                '../printing/printing.gyp:printing_unittests',
                '../remoting/remoting.gyp:remoting_webapp',
                '../sql/sql.gyp:sql_unittests',
                '../sync/sync.gyp:sync_unit_tests',
                '../third_party/widevine/cdm/widevine_cdm.gyp:widevinecdmadapter',
                '../ui/gfx/gfx.gyp:gfx_unittests',
                '../ui/ui_unittests.gyp:ui_unittests',
                '../ui/views/views.gyp:views_unittests',
                '../url/url.gyp:url_unittests',
              ],
              'conditions': [
                ['internal_pdf', {
                  'dependencies': [
                    '../pdf/pdf.gyp:pdf',
                  ],
                }], # internal_pdf
                ['target_arch=="ia32"', {
                  'dependencies': [
                    '../chrome/chrome.gyp:crash_service_win64',
                  ],
                }],
                ['component != "shared_library" and wix_exists == "True" and \
                    sas_dll_exists == "True"', {
                  'dependencies': [
                    '../remoting/remoting.gyp:remoting_host_installation',
                  ],
                }], # component != "shared_library"
              ]
            },
          ], # targets
        }], # branding=="Chrome"
       ], # conditions
    }], # OS="win"
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'aura_builder',
          'type': 'none',
          'dependencies': [
            '../cc/cc_tests.gyp:cc_unittests',
            '../chrome/chrome.gyp:browser_tests',
            '../chrome/chrome.gyp:chrome',
            '../chrome/chrome.gyp:interactive_ui_tests',
            '../chrome/chrome.gyp:unit_tests',
            '../components/components_tests.gyp:components_unittests',
            '../content/content_shell_and_tests.gyp:content_browsertests',
            '../content/content_shell_and_tests.gyp:content_unittests',
            '../device/device_tests.gyp:device_unittests',
            '../google_apis/gcm/gcm.gyp:gcm_unit_tests',
            '../ppapi/ppapi_internal.gyp:ppapi_unittests',
            '../remoting/remoting.gyp:remoting_unittests',
            '../ui/app_list/app_list.gyp:*',
            '../ui/aura/aura.gyp:*',
            '../ui/compositor/compositor.gyp:*',
            '../ui/events/events.gyp:*',
            '../ui/gfx/gfx.gyp:gfx_unittests',
            '../ui/keyboard/keyboard.gyp:*',
            '../ui/message_center/message_center.gyp:*',
            '../ui/snapshot/snapshot.gyp:snapshot_unittests',
            '../ui/ui_unittests.gyp:ui_unittests',
            '../ui/views/views.gyp:views',
            '../ui/views/views.gyp:views_examples_with_content_exe',
            '../ui/views/views.gyp:views_unittests',
            '../webkit/renderer/compositor_bindings/compositor_bindings_tests.gyp:webkit_compositor_bindings_unittests',
            'blink_tests',
          ],
          'conditions': [
            ['OS=="win"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service',
              ],
            }],
            ['OS=="win" and target_arch=="ia32"', {
              'dependencies': [
                '../chrome/chrome.gyp:crash_service_win64',
              ],
            }],
            ['use_ash==1', {
              'dependencies': [
                '../ash/ash.gyp:ash_shell',
                '../ash/ash.gyp:ash_unittests',
              ],
            }],
            ['OS=="linux"', {
              # Tests that currently only work on Linux.
              'dependencies': [
                '../base/base.gyp:base_unittests',
                '../ipc/ipc.gyp:ipc_tests',
                '../sql/sql.gyp:sql_unittests',
                '../sync/sync.gyp:sync_unit_tests',
              ],
            }],
            ['chromeos==1', {
              'dependencies': [
                '../chromeos/chromeos.gyp:chromeos_unittests',
              ],
            }],
          ],
        },
      ],  # targets
    }, {
      'conditions': [
        ['OS=="linux"', {
          # TODO(thakis): Remove this once the linux gtk bot no longer references
          # it (probably after the first aura release on linux), see r249162
          'targets': [
            {
              'target_name': 'aura_builder',
              'type': 'none',
              'dependencies': [
                '../chrome/chrome.gyp:chrome',
              ],
            },
          ],  # targets
      }]], # OS=="linux"
    }], # "use_aura==1"
    ['test_isolation_mode != "noop"', {
      'targets': [
        {
          'target_name': 'chromium_swarm_tests',
          'type': 'none',
          'dependencies': [
            '../base/base.gyp:base_unittests_run',
            '../chrome/chrome.gyp:browser_tests_run',
            '../chrome/chrome.gyp:interactive_ui_tests_run',
            '../chrome/chrome.gyp:sync_integration_tests_run',
            '../chrome/chrome.gyp:unit_tests_run',
            '../net/net.gyp:net_unittests_run',
          ],
        }, # target_name: chromium_swarm_tests
      ],
    }],
  ], # conditions
}
