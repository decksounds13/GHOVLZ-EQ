#pragma once

/**
    Spec3D offline export (timeline region -> MP4 + DAW audio) is sandboxed.
    Set to 1 when integrating into a standalone host or when ready to ship export
    in the plugin. Leave 0 so SharedCode does not compile/link the export job.

    Code under Source/Export/ is retained; only the integration switch is off.
*/
#ifndef SPEC3D_EXPORT_ENABLED
#define SPEC3D_EXPORT_ENABLED 0
#endif
