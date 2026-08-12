#include "hdr.h"

#include <windows.h>

#include <vector>

namespace hdr {

bool any_display_on() {
    UINT32 pathCount = 0;
    UINT32 modeCount = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS) {
        return false;
    }

    std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
    if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount,
                           modes.data(), nullptr) != ERROR_SUCCESS) {
        return false;
    }
    paths.resize(pathCount);

    for (const DISPLAYCONFIG_PATH_INFO& path : paths) {
        DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO info{};
        info.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO;
        info.header.size = sizeof(info);
        info.header.adapterId = path.targetInfo.adapterId;
        info.header.id = path.targetInfo.id;

        if (DisplayConfigGetDeviceInfo(&info.header) == ERROR_SUCCESS &&
            info.advancedColorEnabled) {
            return true;
        }
    }
    return false;
}

} // namespace hdr
