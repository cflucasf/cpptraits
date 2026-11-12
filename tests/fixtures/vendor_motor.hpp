#pragma once

// Stand-in for a third-party header: deliberately independent of traits.hpp.
namespace vendor {
    struct Motor {
        double speed; // Angular velocity in radians per second.
        // Some other vendored props and functions.
    };
}
