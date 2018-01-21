// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The include directives are put into Javascript-style comments to prevent
// parsing errors in non-flattened mode. The flattener still sees them.
// Note that this makes the flattener to comment out the first line of the
// included file but that's all right since any javascript file should start
// with a copyright comment anyway.

//<include src="error_util.js"/>

//<include src="../../../webui/resources/js/cr.js"/>
//<include src="../../../webui/resources/js/load_time_data.js"/>

//<include src="../../../webui/resources/js/event_tracker.js"/>

//<include src="../../../webui/resources/js/cr/ui.js"/>
//<include src="../../../webui/resources/js/cr/event_target.js"/>

//<include src="../../../webui/resources/js/cr/ui/position_util.js"/>
//<include src="../../../webui/resources/js/cr/ui/menu_item.js"/>
//<include src="../../../webui/resources/js/cr/ui/menu.js"/>
//<include src="../../../webui/resources/js/cr/ui/menu_button.js"/>
//<include src="../../../webui/resources/js/cr/ui/context_menu_handler.js"/>

(function() {
'use strict';

//<include src="../../file_manager/common/js/util.js"/>
//<include src="../../file_manager/foreground/js/media/media_controls.js"/>
//<include src="../../file_manager/foreground/js/media/mouse_inactivity_watcher.js"/>

//<include src="cast/cast_extension_discoverer.js"/>
//<include src="cast/cast_video_element.js"/>
//<include src="cast/load_cast_extension_api.js"/>
//<include src="cast/caster.js"/>

//<include src="video_player.js"/>

window.unload = unload;

})();
