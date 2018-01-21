# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'targets': [
    {
      # GN version: //components/domain_reliability
      'target_name': 'domain_reliability',
      'type': '<(component)',
      'dependencies': [
        '../base/base.gyp:base',
        '../components/components.gyp:keyed_service_core',
        '../content/content.gyp:content_browser',
        '../net/net.gyp:net',
        '../url/url.gyp:url_lib',
      ],
      'include_dirs': [
        '..',
      ],
      'defines': [
        'DOMAIN_RELIABILITY_IMPLEMENTATION',
      ],
      'sources': [
        # Note: sources list duplicated in GN build.
        'domain_reliability/baked_in_configs.h',
        'domain_reliability/beacon.cc',
        'domain_reliability/beacon.h',
        'domain_reliability/clear_mode.h',
        'domain_reliability/config.cc',
        'domain_reliability/config.h',
        'domain_reliability/context.cc',
        'domain_reliability/context.h',
        'domain_reliability/dispatcher.cc',
        'domain_reliability/dispatcher.h',
        'domain_reliability/domain_reliability_export.h',
        'domain_reliability/monitor.cc',
        'domain_reliability/monitor.h',
        'domain_reliability/scheduler.cc',
        'domain_reliability/scheduler.h',
        'domain_reliability/service.cc',
        'domain_reliability/service.h',
        'domain_reliability/uploader.cc',
        'domain_reliability/uploader.h',
        'domain_reliability/util.cc',
        'domain_reliability/util.h',
      ],
      'actions': [
        {
          'action_name': 'bake_in_configs',
          'variables': {
            'bake_in_configs_script': 'domain_reliability/bake_in_configs.py',
            'baked_in_configs_cc':
                '<(INTERMEDIATE_DIR)/domain_reliability/baked_in_configs.cc',
            'baked_in_configs': [
              'domain_reliability/baked_in_configs/accounts_google_com.json',
              'domain_reliability/baked_in_configs/ad_doubleclick_net.json',
              'domain_reliability/baked_in_configs/apis_google_com.json',
              'domain_reliability/baked_in_configs/c_admob_com.json',
              'domain_reliability/baked_in_configs/csi_gstatic_com.json',
              'domain_reliability/baked_in_configs/ddm_google_com.json',
              'domain_reliability/baked_in_configs/docs_google_com.json',
              'domain_reliability/baked_in_configs/drive_google_com.json',
              'domain_reliability/baked_in_configs/e_admob_com.json',
              'domain_reliability/baked_in_configs/fonts_googleapis_com.json',
              'domain_reliability/baked_in_configs/googleads4_g_doubleclick_net.json',
              'domain_reliability/baked_in_configs/googleads_g_doubleclick_net.json',
              'domain_reliability/baked_in_configs/gstatic_com.json',
              'domain_reliability/baked_in_configs/lh3_ggpht_com.json',
              'domain_reliability/baked_in_configs/lh4_ggpht_com.json',
              'domain_reliability/baked_in_configs/lh5_ggpht_com.json',
              'domain_reliability/baked_in_configs/lh6_ggpht_com.json',
              'domain_reliability/baked_in_configs/mail_google_com.json',
              'domain_reliability/baked_in_configs/media_admob_com.json',
              'domain_reliability/baked_in_configs/pagead2_googlesyndication_com.json',
              'domain_reliability/baked_in_configs/partner_googleadservices_com.json',
              'domain_reliability/baked_in_configs/pubads_g_doubleclick_net.json',
              'domain_reliability/baked_in_configs/redirector_googlevideo_com.json',
              'domain_reliability/baked_in_configs/redirector_gvt1_com.json',
              'domain_reliability/baked_in_configs/s0_2mdn_net.json',
              'domain_reliability/baked_in_configs/ssl_gstatic_com.json',
              'domain_reliability/baked_in_configs/t0_gstatic_com.json',
              'domain_reliability/baked_in_configs/t1_gstatic_com.json',
              'domain_reliability/baked_in_configs/t2_gstatic_com.json',
              'domain_reliability/baked_in_configs/t3_gstatic_com.json',
              'domain_reliability/baked_in_configs/themes_googleusercontent_com.json',
              'domain_reliability/baked_in_configs/www_google_com.json',
              'domain_reliability/baked_in_configs/www_googleadservices_com.json',
              'domain_reliability/baked_in_configs/www_gstatic_com.json',
              'domain_reliability/baked_in_configs/www_youtube_com.json',
            ],
          },
          'inputs': [
            '<(bake_in_configs_script)',
            '<@(baked_in_configs)',
          ],
          'outputs': [
            '<(baked_in_configs_cc)'
          ],
          'action': ['python',
                     '<(bake_in_configs_script)',
                     '<@(baked_in_configs)',
                     '<(baked_in_configs_cc)'],
          'process_outputs_as_sources': 1,
          'message': 'Baking in Domain Reliability configs',
        },
      ],
    },
  ],
}
