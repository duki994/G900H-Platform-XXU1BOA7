# Copyright 2013 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Compile time controlled V8 features.

{
  'variables': {
    'v8_compress_startup_data%': 'off',

    'v8_enable_debugger_support%': 1,

    'v8_enable_disassembler%': 0,

    'v8_enable_gdbjit%': 0,

    'v8_object_print%': 0,

    'v8_enable_verify_heap%': 0,

    'v8_use_snapshot%': 'true',

    # With post mortem support enabled, metadata is embedded into libv8 that
    # describes various parameters of the VM for use by debuggers. See
    # tools/gen-postmortem-metadata.py for details.
    'v8_postmortem_support%': 'false',

    # Interpreted regexp engine exists as platform-independent alternative
    # based where the regular expression is compiled to a bytecode.
    'v8_interpreted_regexp%': 0,

    # Enable ECMAScript Internationalization API. Enabling this feature will
    # add a dependency on the ICU library.
    'v8_enable_i18n_support%': 1,

    # Enable compiler warnings when using V8_DEPRECATED apis.
    'v8_deprecation_warnings%': 0,

    # Use the v8 provided v8::Platform implementation.
    'v8_use_default_platform%': 1,

    # SRUK performance patches
    'sruk_copywords': 'true',
    'sruk_clamp_to_8': 'true',

    'sruk_for_in_loop': 'true',
    'sruk_native_quick_sort': 'true',
    'sruk_enable_movw_movt': 'true',
    'sruk_dom_remove': 'true',
    'sruk_regexp_with_function': 'true', # see: src/v8/include/v8.h
    'sruk_mark_dont_inline': 'false',
    'sruk_dom_append': 'true',
    'sruk_push_data': 'false',

    # five webkit files changed to disable debugger support on 24/03/2014
    #1. third_party/WebKit/Source/bindings/v8/PageScriptDebugServer.cpp
    #2. third_party/WebKit/Source/bindings/v8/ScriptDebugServer.cpp
    #3. third_party/WebKit/Source/bindings/v8/V8Initializer.cpp
    #4. third_party/WebKit/Source/bindings/v8/WorkerScriptDebugServer.cpp
    #5. third_party/WebKit/Source/core/inspector/JavaScriptCallFrame.cpp

    'sruk_parseint_div': 'false',
    'sruk_ispras_func_size': 'false', # Do not use it along with 'swc_inlining_parameters_tuning'
    'sruk_cache_stringindexof': 'false', # before disabling see src/v8/include/v8.h
    'sruk_native_insertion_sort': 'true',
    'sruk_builtin_arrayreverse': 'false',
    'sruk_stringsplit': 'true',  # Affects only JS code.
    'sruk_freeze_runtime_flags%': 'false',
    'sruk_no_debugger_support%': 'false', # Enable at release!
    'sruk_building_shared_library': 'false', # should be enabled when using "lto" build

    # SWC performance patches
    'swc_old_generation_alloc': 'false', # Do not use it along with 'swc_heapgrow_globalhandles'
    'swc_pld_instruction': 'true',
    'swc_inlining_parameters_tuning': 'false',
    'swc_adaptive_lazy': 'true',
    'swc_heapgrow_globalhandles%': 'true',
    'swc_newspacearraychecklimit%': 'true',
    'swc_promoteoldgen_survivescavenge%': 'true',
    'srr_global_subexpression_elimination': 'true',
    'srr_v8_iself_optimization' : 'true', # Use only for ARM NEON code

    # SEC performance patches
    'sec_ssrm_mode': 'true',
  },
  'target_defaults': {
    'conditions': [
      ['v8_enable_debugger_support==1', {
        'defines': ['ENABLE_DEBUGGER_SUPPORT',],
      }],
      ['v8_enable_disassembler==1', {
        'defines': ['ENABLE_DISASSEMBLER',],
      }],
      ['v8_enable_gdbjit==1', {
        'defines': ['ENABLE_GDB_JIT_INTERFACE',],
      }],
      ['v8_object_print==1', {
        'defines': ['OBJECT_PRINT',],
      }],
      ['v8_enable_verify_heap==1', {
        'defines': ['VERIFY_HEAP',],
      }],
      ['v8_interpreted_regexp==1', {
        'defines': ['V8_INTERPRETED_REGEXP',],
      }],
      ['v8_deprecation_warnings==1', {
        'defines': ['V8_DEPRECATION_WARNINGS',],
      }],
      ['v8_enable_i18n_support==1', {
        'defines': ['V8_I18N_SUPPORT',],
      }],
      ['v8_use_default_platform==1', {
        'defines': ['V8_USE_DEFAULT_PLATFORM',],
      }],
      ['v8_compress_startup_data=="bz2"', {
        'defines': ['COMPRESS_STARTUP_DATA_BZ2',],
      }],
      # SRUK performance patches
      [ 'v8_target_arch=="arm" and sruk_parseint_div=="true"', {
        'defines': ['SRUK_PARSEINT_DIV',],
      }],
      [ 'v8_target_arch=="arm" and sruk_native_insertion_sort=="true"', {
        'defines': ['SRUK_NATIVE_INSERTION_SORT',],
      }],
      [ 'v8_target_arch=="arm" and sruk_for_in_loop=="true"', {
        'defines': ['SRUK_FOR_IN_LOOP',],
      }],
      [ 'v8_target_arch=="arm" and sruk_native_quick_sort=="true"', {
        'defines': ['SRUK_NATIVE_QUICK_SORT',],
      }],
      [ 'v8_target_arch=="arm" and sruk_regexp_with_function=="true"', {
        'defines': ['SRUK_REGEXP_WITH_FUNCTION',],
      }],
      [ 'v8_target_arch=="arm" and sruk_enable_movw_movt=="true"', {
        'defines': ['SRUK_ENABLE_MOVW_MOVT',],
      }],
      [ 'v8_target_arch=="arm" and sruk_dom_remove=="true"', {
        'defines': ['SRUK_DOM_REMOVE',],
      }],
      [ 'v8_target_arch=="arm" and sruk_mark_dont_inline=="true"', {
        'defines': ['SRUK_MARK_DONT_INLINE',],
      }],
      [ 'v8_target_arch=="arm" and sruk_dom_append=="true"', {
        'defines': ['SRUK_DOM_APPEND',],
      }],
      [ 'v8_target_arch=="arm" and sruk_push_data=="true"', {
        'defines': ['SRUK_PUSH_DATA',],
      }],
      [ 'v8_target_arch=="arm" and sruk_clamp_to_8=="true"', {
        'defines': ['SRUK_CLAMP_TO_8',],
      }],
      [ 'v8_target_arch=="arm" and sruk_copywords=="true"', {
        'defines': ['SRUK_COPYWORDS',],
      }],
      [ 'v8_target_arch=="arm" and sruk_ispras_func_size=="true"', {
        'defines': ['SRUK_ISPRAS_FUNC_SIZE',],
        'defines!': ['SWC_INLINING_PARAMETERS_TUNING'],
      }],
      [ 'v8_target_arch=="arm" and sruk_cache_stringindexof=="true"', {
        'defines': ['SRUK_CACHE_STRINGINDEXOF',],
      }],
      [ 'v8_target_arch=="arm" and sruk_builtin_arrayreverse=="true"', {
        'defines': ['SRUK_BUILTIN_ARRAYREVERSE'],
      }],
      [ 'v8_target_arch=="arm" and sruk_stringsplit=="true"', {
        'defines': ['SRUK_STRINGSPLIT'],
      }],
      [ 'v8_target_arch=="arm" and sruk_freeze_runtime_flags=="true"', {
        'defines': ['SRUK_FREEZE_RUNTIME_FLAGS'],
      }],
      [ 'v8_target_arch=="arm" and sruk_no_debugger_support=="true"', {
        'defines': ['SRUK_NO_DEBUGGER_SUPPORT'],
      }],
      [ 'v8_target_arch=="arm" and sruk_building_shared_library=="true"', {
        'defines': ['SRUK_BUILDING_SHARED_LIBRARY'],
      }],
      # SWC performance patches
      [ 'v8_target_arch=="arm" and swc_old_generation_alloc=="true"', {
        'defines': ['SWC_OLD_GENERATION_ALLOC'],
        'defines!': ['SWC_HEAPGROW_GLOBALHANDLES',],
      }],
      [ 'v8_target_arch=="arm" and swc_pld_instruction=="true"', {
        'defines': ['SWC_PLD_INSTRUCTION'],
      }],
      ['v8_target_arch=="arm" and swc_inlining_parameters_tuning=="true"', {
        'defines': ['SWC_INLINING_PARAMETERS_TUNING',],
        'defines!': ['SRUK_ISPRAS_FUNC_SIZE'],
      }],
      ['v8_target_arch=="arm" and swc_adaptive_lazy=="true"', {
        'defines': ['SWC_ADAPTIVE_LAZY',],
      }],
      ['v8_target_arch=="arm" and swc_heapgrow_globalhandles=="true"', {
        'defines': ['SWC_HEAPGROW_GLOBALHANDLES',],
        'defines!': ['SWC_OLD_GENERATION_ALLOC',],
      }],
      ['v8_target_arch=="arm" and swc_newspacearraychecklimit=="true"', {
        'defines': ['SWC_NEWSPACEARRAYCHECKLIMIT',],
      }],
      ['v8_target_arch=="arm" and swc_promoteoldgen_survivescavenge=="true"', {
        'defines': ['SWC_PROMOTEOLDGEN_SURVIVESCAVENGE',],
      }],
      ['v8_target_arch=="arm" and srr_global_subexpression_elimination=="true"', {
        'defines': ['SRR_GLOBAL_SUBEXPRESSION_ELIMINATION',],
      }],
      ['v8_target_arch=="arm" and srr_v8_iself_optimization=="true"', {
        'defines': ['SRR_V8_CODE_OPTIMIZATION',],
      }],
      ['v8_target_arch=="arm" and sec_ssrm_mode=="true"', {
        'defines': ['SEC_SSRM_MODE'],
      }],
    ],  # conditions
    'configurations': {
      'Debug': {
        'variables': {
          'v8_enable_extra_checks%': 1,
          'v8_enable_handle_zapping%': 1,
        },
        'conditions': [
          ['v8_enable_extra_checks==1', {
            'defines': ['ENABLE_EXTRA_CHECKS',],
          }],
          ['v8_enable_handle_zapping==1', {
            'defines': ['ENABLE_HANDLE_ZAPPING',],
          }],
          [ 'v8_target_arch=="arm" and sruk_no_debugger_support=="true"', {
            'defines!': ['SRUK_NO_DEBUGGER_SUPPORT'],
          }],
          [ 'v8_target_arch=="arm" and sruk_building_shared_library=="true"', {
            'defines!': ['SRUK_BUILDING_SHARED_LIBRARY'],
          }],
        ],
      },  # Debug
      'Release': {
        'variables': {
          'v8_enable_extra_checks%': 0,
          'v8_enable_handle_zapping%': 0,
        },
        'conditions': [
          ['v8_enable_extra_checks==1', {
            'defines': ['ENABLE_EXTRA_CHECKS',],
          }],
          ['v8_enable_handle_zapping==1', {
            'defines': ['ENABLE_HANDLE_ZAPPING',],
          }],
          [ 'v8_target_arch=="arm" and sruk_no_debugger_support=="true"', {
            'defines!': ['ENABLE_DEBUGGER_SUPPORT'],
          }],
        ],  # conditions
      },  # Release
    },  # configurations
  },  # target_defaults
}
