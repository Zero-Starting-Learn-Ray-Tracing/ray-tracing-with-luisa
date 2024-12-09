#pragma once

#include "rtweekend.h"


class ray {
public:
    Float3 orig {};
    Float3 dir {};
    Float tm {};

public:
    ray() = default;
    ray(Float3 origin, Float3 direction, Float time = 0.0f)
        : orig(std::move(origin))
        , dir(std::move(direction))
        , tm(std::move(time))
    {}

    [[nodiscard]]
    Float3 origin() const {
        return orig;
    }

    [[nodiscard]]
    Float3 direction() const {
        return dir;
    }

    [[nodiscard]]
    Float time() const {
        return tm;
    }

    [[nodiscard]]
    Float3 at(Float t) const {
        return orig + t * dir;
    }
};
