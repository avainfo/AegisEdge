#pragma once

#include <string>

enum class LinkState {
    NORMAL,
    DEGRADED,
    LOST
};

inline std::string LinkStateToString(LinkState state) {
    switch (state) {
        case LinkState::NORMAL: return "NORMAL";
        case LinkState::DEGRADED: return "DEGRADED";
        case LinkState::LOST: return "LOST";
        default: return "LOST";
    }
}
