#pragma once

#include "rtweekend.h"
#include "ray.h"

class camera {
public:
    camera(
        float3 lookfrom,
        float3 lookat,
        float3 vup,
        float vfov, // vertical field-of-view in degrees
        float aspect_ratio,
        float aperture,
        float focus_dist,
        float _time0 = 0.0f,
        float _time1 = 0.0f
    ) {
        float theta = luisa::radians(vfov);
        float h = luisa::tan(theta / 2.0f);
        float viewport_height = 2.0f * h;
        float viewport_width = aspect_ratio * viewport_height;

        w = luisa::normalize(lookfrom - lookat);
        u = luisa::normalize(luisa::cross(vup, w));
        v = luisa::cross(w, u);

        origin = lookfrom;
        horizontal = focus_dist * viewport_width * u;
        vertical = focus_dist * viewport_height * v;
        lower_left_corner = origin - horizontal / 2.0f - vertical / 2.0f - focus_dist * w;

        lens_radius = aperture / 2.0f;
        time0 = _time0;
        time1 = _time1;
    }

    ray get_ray(Float2 uv, UInt &seed) const {
        Float3 rd = lens_radius * random_in_unit_disk(seed);
        Float3 offset = u * rd.x + v * rd.y;

        return {
            origin + offset,
            lower_left_corner
                + uv.x * horizontal
                + uv.y * vertical
                - origin - offset,
            frand(seed, time0, time1)
        };
    }

private:
    float3 origin {};
    float3 lower_left_corner {};
    float3 horizontal {};
    float3 vertical {};
    float3 u {};
    float3 v {};
    float3 w {};
    float lens_radius {};
    float time0 {}; // shutter open/close times
    float time1 {};
};
