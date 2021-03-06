# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //mojo/services/dbus_echo:bindings
      'target_name': 'mojo_echo_bindings',
      'type': 'static_library',
      'sources': [
        'services/dbus_echo/echo.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_html_viewer',
      'type': 'loadable_module',
      'dependencies': [
        '../net/net.gyp:net',
        '../skia/skia.gyp:skia',
        '../third_party/WebKit/public/blink.gyp:blink',
        '../ui/native_theme/native_theme.gyp:native_theme',
        '../url/url.gyp:url_lib',
        'mojo_application',
        'mojo_common_lib',
        'mojo_cpp_bindings',
        'mojo_environment_chromium',
        'mojo_navigation_bindings',
        'mojo_network_bindings',
        'mojo_launcher_bindings',
        'mojo_utility',
        'mojo_view_manager_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'include_dirs': [
        'third_party/WebKit'
      ],
      'sources': [
        'public/cpp/application/lib/mojo_main_chromium.cc',
        'services/html_viewer/blink_input_events_type_converters.cc',
        'services/html_viewer/blink_input_events_type_converters.h',
        'services/html_viewer/blink_platform_impl.cc',
        'services/html_viewer/blink_platform_impl.h',
        'services/html_viewer/html_viewer.cc',
        'services/html_viewer/html_document_view.cc',
        'services/html_viewer/html_document_view.h',
        'services/html_viewer/webmimeregistry_impl.cc',
        'services/html_viewer/webmimeregistry_impl.h',
        'services/html_viewer/webstoragenamespace_impl.cc',
        'services/html_viewer/webstoragenamespace_impl.h',
        'services/html_viewer/webthemeengine_impl.cc',
        'services/html_viewer/webthemeengine_impl.h',
        'services/html_viewer/webthread_impl.cc',
        'services/html_viewer/webthread_impl.h',
        'services/html_viewer/weburlloader_impl.cc',
        'services/html_viewer/weburlloader_impl.h',
      ],
    },
    {
      'target_name': 'mojo_input_events_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_INPUT_EVENTS_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_environment_chromium',
        'mojo_input_events_bindings',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        '<(mojo_system_for_component)',
      ],
      'sources': [
        'services/public/cpp/input_events/lib/input_events_type_converters.cc',
        'services/public/cpp/input_events/input_events_type_converters.h',
        'services/public/cpp/input_events/mojo_input_events_export.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/input_events:bindings
      'target_name': 'mojo_input_events_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/input_events/input_events.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
        'mojo_geometry_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/geometry:bindings
      'target_name': 'mojo_geometry_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/geometry/geometry.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_geometry_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_GEOMETRY_IMPLEMENTATION',
      ],
      'dependencies': [
        '../skia/skia.gyp:skia',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        '../ui/gfx/gfx.gyp:gfx',
      ],
      'sources': [
        'services/public/cpp/geometry/lib/geometry_type_converters.cc',
        'services/public/cpp/geometry/geometry_type_converters.h',
        'services/public/cpp/geometry/mojo_geometry_export.h',
      ],
    },
    {
      'target_name': 'mojo_surfaces_lib',
      'type': '<(component)',
      'defines': [
        'MOJO_SURFACES_IMPLEMENTATION',
      ],
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../gpu/gpu.gyp:gpu',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_environment_chromium',
        'mojo_geometry_lib',
        'mojo_surfaces_bindings',
        '<(mojo_system_for_component)',
      ],
      'export_dependent_settings': [
        'mojo_geometry_lib',
      ],
      'sources': [
        'services/public/cpp/surfaces/lib/surfaces_type_converters.cc',
        'services/public/cpp/surfaces/surfaces_type_converters.h',
        'services/public/cpp/surfaces/mojo_surfaces_export.h',
      ],
    },
    {
      'target_name': 'mojo_surfaces_lib_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../gpu/gpu.gyp:gpu',
        '../skia/skia.gyp:skia',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'mojo_environment_chromium',
        'mojo_geometry_lib',
        'mojo_run_all_unittests',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
      ],
      'sources': [
        'services/public/cpp/surfaces/tests/surface_unittest.cc',
      ],
    },
    {
      'target_name': 'mojo_gles2',
      'type': 'shared_library',
      'defines': [
        'MOJO_GLES2_IMPLEMENTATION',
        'GLES2_USE_MOJO',
      ],
      'include_dirs': [
        '..',
      ],
      'dependencies': [
        '../third_party/khronos/khronos.gyp:khronos_headers'
      ],
      'direct_dependent_settings': {
        'include_dirs': [
          '..',
        ],
        'defines': [
          'GLES2_USE_MOJO',
        ],
      },
      'sources': [
        'public/c/gles2/gles2.h',
        'public/c/gles2/gles2_export.h',
        'public/gles2/gles2_private.cc',
        'public/gles2/gles2_private.h',
      ],
      'conditions': [
        ['OS=="mac"', {
          'xcode_settings': {
            # Make it a run-path dependent library.
            'DYLIB_INSTALL_NAME_BASE': '@loader_path',
          },
        }],
      ],
    },
    {
      # GN version: //mojo/services/gles2:interfaces (for files generated from
      # the mojom file)
      # GN version: //mojo/services/gles2:bindings
      'target_name': 'mojo_gles2_bindings',
      'type': 'static_library',
      'sources': [
        'services/gles2/command_buffer.mojom',
        'services/gles2/command_buffer_type_conversions.cc',
        'services/gles2/command_buffer_type_conversions.h',
        'services/gles2/mojo_buffer_backing.cc',
        'services/gles2/mojo_buffer_backing.h',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        '../gpu/gpu.gyp:command_buffer_common',
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_gles2_service',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../gpu/gpu.gyp:command_buffer_service',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        '../ui/gl/gl.gyp:gl',
        'mojo_gles2_bindings',
      ],
      'export_dependent_settings': [
        'mojo_gles2_bindings',
      ],
      'sources': [
        'services/gles2/command_buffer_impl.cc',
        'services/gles2/command_buffer_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/native_viewport:bindings
      'target_name': 'mojo_native_viewport_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/native_viewport/native_viewport.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_geometry_bindings',
        'mojo_gles2_bindings',
        'mojo_input_events_bindings',
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_native_viewport_service',
      # This is linked directly into the embedder, so we make it a component.
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_gles2_service',
        'mojo_input_events_lib',
        'mojo_native_viewport_bindings',
        '<(mojo_system_for_component)',
      ],
      'defines': [
        'MOJO_NATIVE_VIEWPORT_IMPLEMENTATION',
      ],
      'sources': [
        'services/native_viewport/native_viewport.h',
        'services/native_viewport/native_viewport_android.cc',
        'services/native_viewport/native_viewport_mac.mm',
        'services/native_viewport/native_viewport_ozone.cc',
        'services/native_viewport/native_viewport_service.cc',
        'services/native_viewport/native_viewport_service.h',
        'services/native_viewport/native_viewport_stub.cc',
        'services/native_viewport/native_viewport_win.cc',
        'services/native_viewport/native_viewport_x11.cc',
      ],
      'conditions': [
        ['OS=="win" or OS=="android" or OS=="linux" or OS=="mac"', {
          'sources!': [
            'services/native_viewport/native_viewport_stub.cc',
          ],
        }],
        ['OS=="android"', {
          'dependencies': [
            'mojo_jni_headers',
          ],
        }],
        ['OS=="win"', {
          'dependencies': [
            '../ui/platform_window/win/win_window.gyp:win_window',
          ],
        }],
        ['use_x11==1', {
          'dependencies': [
            '../ui/platform_window/x11/x11_window.gyp:x11_window',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/navigation:bindings
      'target_name': 'mojo_navigation_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/navigation/navigation.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
        'mojo_network_bindings',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/network:bindings
      'target_name': 'mojo_network_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/network/network_error.mojom',
        'services/public/interfaces/network/network_service.mojom',
        'services/public/interfaces/network/url_loader.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_network_service_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
        'mojo_application',
        'mojo_common_lib',
        'mojo_environment_chromium',
        'mojo_network_bindings',
      ],
      'export_dependent_settings': [
        'mojo_network_bindings',
      ],
      'sources': [
        'services/network/network_context.cc',
        'services/network/network_context.h',
        'services/network/network_service_impl.cc',
        'services/network/network_service_impl.h',
        'services/network/url_loader_impl.cc',
        'services/network/url_loader_impl.h',
      ],
    },
    {
      'target_name': 'mojo_network_service',
      'type': 'loadable_module',
      'dependencies': [
        'mojo_network_bindings',
        'mojo_network_service_lib',
        '<(mojo_system_for_loadable_module)',
      ],
      'export_dependent_settings': [
        'mojo_network_bindings',
      ],
      'sources': [
        'services/network/main.cc',
      ],
    },
    {
      'target_name': 'mojo_surfaces_service',
      'type': 'shared_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../cc/cc.gyp:cc',
        '../cc/cc.gyp:cc_surfaces',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_cc_support',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_gles2',
        'mojo_surfaces_bindings',
        'mojo_surfaces_lib',
        'mojo_system_impl',
      ],
      'sources': [
        'services/surfaces/surfaces_impl.cc',
        'services/surfaces/surfaces_impl.h',
        'services/surfaces/surfaces_service_application.cc',
        'services/surfaces/surfaces_service_application.h',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      'target_name': 'mojo_view_manager_common',
      'type': 'static_library',
      'sources': [
        'services/public/cpp/view_manager/types.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/launcher:bindings
      'target_name': 'mojo_launcher_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/launcher/launcher.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
        'mojo_navigation_bindings',
      ],
    },
    {
      'target_name': 'mojo_launcher',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        '../url/url.gyp:url_lib',
        'mojo_application',
        'mojo_cpp_bindings',
        'mojo_environment_chromium',
        'mojo_launcher_bindings',
        'mojo_network_bindings',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'services/launcher/launcher.cc',
        'public/cpp/application/lib/mojo_main_chromium.cc',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/view_manager:bindings
      'target_name': 'mojo_view_manager_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/view_manager/view_manager.mojom',
        'services/public/interfaces/view_manager/view_manager_constants.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
        'mojo_geometry_bindings',
        'mojo_input_events_bindings',
      ],
    },
    {
      'target_name': 'mojo_view_manager_lib',
      'type': 'static_library',
      'dependencies': [
        '../base/base.gyp:base',
        '../skia/skia.gyp:skia',
        '../ui/events/events.gyp:events',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_geometry',
        'mojo_application',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_service_provider_bindings',
        'mojo_view_manager_bindings',
        'mojo_view_manager_common',
      ],
      'sources': [
        'services/public/cpp/view_manager/lib/node.cc',
        'services/public/cpp/view_manager/lib/node_observer.cc',
        'services/public/cpp/view_manager/lib/node_private.cc',
        'services/public/cpp/view_manager/lib/node_private.h',
        'services/public/cpp/view_manager/lib/view.cc',
        'services/public/cpp/view_manager/lib/view_private.cc',
        'services/public/cpp/view_manager/lib/view_private.h',
        'services/public/cpp/view_manager/lib/view_manager_client_impl.cc',
        'services/public/cpp/view_manager/lib/view_manager_client_impl.h',
        'services/public/cpp/view_manager/node.h',
        'services/public/cpp/view_manager/node_observer.h',
        'services/public/cpp/view_manager/view.h',
        'services/public/cpp/view_manager/view_manager.h',
        'services/public/cpp/view_manager/view_manager_delegate.h',
        'services/public/cpp/view_manager/view_observer.h',
        'services/public/cpp/view_manager/window_manager_delegate.h',
      ],
      'export_dependent_settings': [
        'mojo_view_manager_bindings',
      ],
    },
    {
      'target_name': 'mojo_view_manager_lib_unittests',
      'type': 'executable',
      'dependencies': [
        '../base/base.gyp:base',
        '../base/base.gyp:test_support_base',
        '../testing/gtest.gyp:gtest',
        '../ui/gfx/gfx.gyp:gfx',
        '../ui/gfx/gfx.gyp:gfx_test_support',
        'mojo_environment_chromium',
        'mojo_geometry_bindings',
        'mojo_geometry_lib',
        'mojo_shell_test_support',
        'mojo_view_manager_bindings',
        'mojo_view_manager_lib',
      ],
      'sources': [
        'services/public/cpp/view_manager/tests/node_unittest.cc',
        'services/public/cpp/view_manager/tests/view_unittest.cc',
        'services/public/cpp/view_manager/tests/view_manager_unittest.cc',
      ],
      'conditions': [
        ['use_aura==1', {
          'dependencies': [
            'mojo_view_manager_run_unittests'
          ],
        }, {  # use_aura==0
          'dependencies': [
            'mojo_run_all_unittests',
          ],
        }],
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/surfaces:bindings
      'target_name': 'mojo_surfaces_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/surfaces/surfaces.mojom',
        'services/public/interfaces/surfaces/surface_id.mojom',
        'services/public/interfaces/surfaces/quads.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
        'mojo_geometry_bindings',
        'mojo_native_viewport_bindings',
      ],
    },
    {
      # GN version: //mojo/services/test_service:bindings
      'target_name': 'mojo_test_service_bindings',
      'type': 'static_library',
      'sources': [
         # TODO(tim): Move to services/public/interfaces?
        'services/test_service/test_request_tracker.mojom',
        'services/test_service/test_service.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
    {
      'target_name': 'mojo_test_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_application',
        'mojo_environment_standalone',
        'mojo_test_service_bindings',
        'mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'public/cpp/application/lib/mojo_main_standalone.cc',
        'services/test_service/test_request_tracker_client_impl.cc',
        'services/test_service/test_request_tracker_client_impl.h',
        'services/test_service/test_service_application.cc',
        'services/test_service/test_service_application.h',
        'services/test_service/test_service_impl.cc',
        'services/test_service/test_service_impl.h',
        'services/test_service/test_time_service_impl.cc',
        'services/test_service/test_time_service_impl.h',
      ],
    },
    {
      'target_name': 'mojo_test_request_tracker_app',
      'type': 'loadable_module',
      'dependencies': [
        '../base/base.gyp:base',
        'mojo_application',
        'mojo_environment_standalone',
        'mojo_test_service_bindings',
        'mojo_utility',
        '<(mojo_system_for_loadable_module)',
      ],
      'sources': [
        'public/cpp/application/lib/mojo_main_standalone.cc',
        'services/test_service/test_request_tracker_client_impl.cc',
        'services/test_service/test_request_tracker_client_impl.h',
        'services/test_service/test_request_tracker_application.cc',
        'services/test_service/test_request_tracker_application.h',
        'services/test_service/test_time_service_impl.cc',
        'services/test_service/test_time_service_impl.h',
        'services/test_service/test_request_tracker_impl.cc',
        'services/test_service/test_request_tracker_impl.h',
      ],
    },
    {
      # GN version: //mojo/services/public/interfaces/window_manager:core_bindings
      'target_name': 'mojo_core_window_manager_bindings',
      'type': 'static_library',
      'sources': [
        'services/public/interfaces/window_manager/window_manager.mojom',
      ],
      'includes': [ 'public/tools/bindings/mojom_bindings_generator.gypi' ],
      'export_dependent_settings': [
        'mojo_cpp_bindings',
      ],
      'dependencies': [
        'mojo_cpp_bindings',
      ],
    },
  ],
  'conditions': [
    ['use_aura==1', {
      'targets': [
        {
          'target_name': 'mojo_view_manager',
          'type': '<(component)',
          'dependencies': [
            '../base/base.gyp:base',
            '../cc/cc.gyp:cc',
            '../skia/skia.gyp:skia',
            '../ui/aura/aura.gyp:aura',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/compositor/compositor.gyp:compositor',
            '../ui/events/events.gyp:events',
            '../ui/events/events.gyp:events_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            '../webkit/common/gpu/webkit_gpu.gyp:webkit_gpu',
            'mojo_application',
            'mojo_cc_support',
            'mojo_common_lib',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_gles2',
            'mojo_input_events_bindings',
            'mojo_input_events_lib',
            'mojo_native_viewport_bindings',
            'mojo_view_manager_bindings',
            'mojo_view_manager_common',
            '<(mojo_system_for_component)',
          ],
          'sources': [
            'public/cpp/application/lib/mojo_main_chromium.cc',
            'services/view_manager/ids.h',
            'services/view_manager/main.cc',
            'services/view_manager/node.cc',
            'services/view_manager/node.h',
            'services/view_manager/node_delegate.h',
            'services/view_manager/root_node_manager.cc',
            'services/view_manager/root_node_manager.h',
            'services/view_manager/root_view_manager.cc',
            'services/view_manager/root_view_manager.h',
            'services/view_manager/root_view_manager_delegate.h',
            'services/view_manager/screen_impl.cc',
            'services/view_manager/screen_impl.h',
            'services/view_manager/view.cc',
            'services/view_manager/view.h',
            'services/view_manager/view_manager_export.h',
            'services/view_manager/view_manager_init_service_impl.cc',
            'services/view_manager/view_manager_init_service_impl.h',
            'services/view_manager/view_manager_service_impl.cc',
            'services/view_manager/view_manager_service_impl.h',
            'services/view_manager/context_factory_impl.cc',
            'services/view_manager/context_factory_impl.h',
            'services/view_manager/window_tree_host_impl.cc',
            'services/view_manager/window_tree_host_impl.h',
          ],
          'defines': [
            'MOJO_VIEW_MANAGER_IMPLEMENTATION',
          ],
        },
        {
          'target_name': 'mojo_view_manager_run_unittests',
          'type': 'static_library',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../ui/gl/gl.gyp:gl',
          ],
          'sources': [
            'services/public/cpp/view_manager/lib/view_manager_test_suite.cc',
            'services/public/cpp/view_manager/lib/view_manager_test_suite.h',
            'services/public/cpp/view_manager/lib/view_manager_unittests.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../third_party/mesa/mesa.gyp:osmesa',
                'mojo_native_viewport_service',
              ],
            }],
            ['use_x11==1', {
              'dependencies': [
                '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
              ],
            }],
          ],
        },
        {
          'target_name': 'mojo_view_manager_unittests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../base/base.gyp:test_support_base',
            '../skia/skia.gyp:skia',
            '../testing/gtest.gyp:gtest',
            '../ui/aura/aura.gyp:aura',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/gl/gl.gyp:gl',
            'mojo_application',
            'mojo_environment_chromium',
            'mojo_geometry_bindings',
            'mojo_geometry_lib',
            'mojo_input_events_bindings',
            'mojo_input_events_lib',
            'mojo_service_manager',
            'mojo_shell_test_support',
            'mojo_system_impl',
            'mojo_view_manager_bindings',
            'mojo_view_manager_common',
            'mojo_view_manager_run_unittests',
          ],
          'sources': [
            'services/view_manager/test_change_tracker.cc',
            'services/view_manager/test_change_tracker.h',
            'services/view_manager/view_manager_unittest.cc',
          ],
        },
        {
          'target_name': 'package_mojo_view_manager',
          'variables': {
            'app_name': 'mojo_view_manager',
          },
          'includes': [ 'build/package_app.gypi' ],
        },
        {
          'target_name': 'mojo_core_window_manager',
          'type': 'loadable_module',
          'dependencies': [
            '../base/base.gyp:base',
            '../ui/base/ui_base.gyp:ui_base',
            '../ui/gfx/gfx.gyp:gfx',
            '../ui/gfx/gfx.gyp:gfx_geometry',
            '../ui/wm/wm.gyp:wm',
            'mojo_application',
            'mojo_aura_support',
            'mojo_common_lib',
            'mojo_core_window_manager_bindings',
            'mojo_environment_chromium',
            'mojo_view_manager_lib',
            '<(mojo_system_for_loadable_module)',
          ],
          'sources': [
            'public/cpp/application/lib/mojo_main_chromium.cc',
            'services/window_manager/window_manager_app.cc',
            'services/window_manager/window_manager_app.h',
            'services/window_manager/window_manager_service_impl.cc',
            'services/window_manager/window_manager_service_impl.h',
          ],
        },
        {
          'target_name': 'mojo_core_window_manager_unittests',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:test_support_base',
            '../testing/gtest.gyp:gtest',
            '../ui/gl/gl.gyp:gl',
            'mojo_core_window_manager_bindings',
            'mojo_environment_chromium',
            'mojo_service_manager',
            'mojo_shell_test_support',
            'mojo_system_impl',
            'mojo_view_manager_bindings',
            'mojo_view_manager_lib',
          ],
          'sources': [
            'services/window_manager/window_manager_api_unittest.cc',
            'services/window_manager/window_manager_unittests.cc',
          ],
          'conditions': [
            ['OS=="linux"', {
              'dependencies': [
                '../third_party/mesa/mesa.gyp:osmesa',
                'mojo_native_viewport_service',
              ],
            }],
            ['use_x11==1', {
              'dependencies': [
                '../ui/gfx/x/gfx_x11.gyp:gfx_x11',
              ],
            }],
          ],
        },
      ],
    }],
    ['OS=="linux"', {
      'targets': [
        {
          'target_name': 'mojo_dbus_echo_service',
          'type': 'executable',
          'dependencies': [
            '../base/base.gyp:base',
            '../build/linux/system.gyp:dbus',
            '../dbus/dbus.gyp:dbus',
            'mojo_application',
            'mojo_common_lib',
            'mojo_dbus_service',
            'mojo_echo_bindings',
            'mojo_environment_chromium',
            'mojo_system_impl',
          ],
          'sources': [
            'services/dbus_echo/dbus_echo_service.cc',
          ],
        },
      ],
    }],
  ],
}
