#ifndef CONSOLE_INFO_H
#define CONSOLE_INFO_H

#include <array>
#include <cstdint>
#include <string>

#include "sha1digest.h"
#include "nocashFooter.h"

struct consoleInfo {
    bool isRetail{true};
    bool tmdPatched{false};
    bool tmdInvalid{false};
    bool tmdFound{true};
    bool tmdGood{false};
    bool UnlaunchHNAAtmdFound{false};
    bool needsNocashFooterToBeWritten{false};
    uint32_t launcherVersion{0};
    std::string launcherTmdPath;
    std::string launcherAppPath;
    std::array<uint8_t, 520> recoveryTmdData;
    Sha1Digest recoveryTmdDataSha;
    NocashFooter nocashFooter;
};

#endif
