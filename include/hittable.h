#pragma once

// #include <utility>

#include "ray.h"
#include "aabb.h"


// class material;

class hit_record {
public:
    Float3 p {};
    Float3 normal {};
    UInt mat_id {};
    // shared_ptr<material> mat_ptr;
    Float t {};
    Float u {};
    Float v {};
    Bool front_face {};

    void set_face_normal(const ray &r, const Float3 &outward_normal) {
        front_face = dot(r.direction(), outward_normal) < 0.0f;
        normal = select(-outward_normal, outward_normal, front_face);
    }
};


class hittable {
public:
    virtual Bool hit(
        const ray &r,
        Float t_min,
        Float t_max,
        hit_record &rec,
        UInt &seed
    ) const = 0;
    virtual bool bounding_box(aabb &output_box) const = 0;
};


class translate : public hittable {
public:
    luisa::shared_ptr<hittable> ptr;
    float3 offset {};

public:
    translate(
        luisa::shared_ptr<hittable> p,
        const float3 &displacement
    )
        : ptr(std::move(p))
        , offset(displacement)
    {}

    Bool hit(
        const ray &r,
        Float t_min,
        Float t_max,
        hit_record &rec,
        UInt &seed
    ) const override;

    bool bounding_box(aabb &output_box) const override;
};

Bool translate::hit(
    const ray &r,
    Float t_min,
    Float t_max,
    hit_record &rec,
    UInt &seed
) const {
    Bool ret { true };
    ray moved_r(
        r.origin() - offset,
        r.direction(),
        r.time()
    );

    $if (!ptr->hit(moved_r, t_min, t_max, rec, seed)) {
        ret = false;
    } $else {
        rec.p += offset;
        rec.set_face_normal(moved_r, rec.normal);
        ret = true;
    };

    return ret;
}

bool translate::bounding_box(aabb &output_box) const {
    if (!ptr->bounding_box(output_box)) {
        return false;
    }

    output_box = aabb(
        output_box.min() + offset,
        output_box.max() + offset
    );

    return true;
}


class rotate_y : public hittable {
public:
    shared_ptr<hittable> ptr;
    float sin_theta {};
    float cos_theta {};
    bool hasbox {};
    aabb bbox;

public:
    rotate_y(shared_ptr<hittable> p, float angle);

    Bool hit(
        const ray &r,
        Float t_min,
        Float t_max,
        hit_record &rec,
        UInt &seed
    ) const override;

    bool bounding_box(aabb &output_box) const override {
        output_box = bbox;
        return hasbox;
    }
};

rotate_y::rotate_y(
    luisa::shared_ptr<hittable> p,
    float angle
)
    : ptr(std::move(p))
{
    auto radians = luisa::radians(angle);
    sin_theta = luisa::sin(radians);
    cos_theta = luisa::cos(radians);
    hasbox = ptr->bounding_box(bbox);

    float3 min { infinity, infinity, infinity };
    float3 max { -infinity, -infinity, -infinity };

    for (int i = 0; i < 2; i++) {
        for (int j = 0; j < 2; j++) {
            for (int k = 0; k < 2; k++) {
                auto x = static_cast<float>(i) * bbox.max().x + static_cast<float>(1 - i) * bbox.min().x;
                auto y = static_cast<float>(j) * bbox.max().y + static_cast<float>(1 - j) * bbox.min().y;
                auto z = static_cast<float>(k) * bbox.max().z + static_cast<float>(1 - k) * bbox.min().z;

                auto newx = cos_theta * x + sin_theta * z;
                auto newz = -sin_theta * x + cos_theta * z;

                float3 tester(newx, y, newz);

                for (std::size_t c = 0; c < 3; c++) {
                    min[c] = fmin(min[c], tester[c]);
                    max[c] = fmax(max[c], tester[c]);
                }
            }
        }
    }

    bbox = aabb(min, max);
}

Bool rotate_y::hit(
    const ray &r,
    Float t_min,
    Float t_max,
    hit_record &rec,
    UInt &seed
) const {
    Bool ret { true };
    Float3 origin = make_float3(
        cos_theta * r.origin()[0] - sin_theta * r.origin()[2],
        r.origin()[1],
        sin_theta * r.origin()[0] + cos_theta * r.origin()[2]
    );
    Float3 direction = make_float3(
        cos_theta * r.direction()[0] - sin_theta * r.direction()[2],
        r.direction()[1],
        sin_theta * r.direction()[0] + cos_theta * r.direction()[2]
    );
    ray rotated_r(origin, direction, r.time());

    $if (!ptr->hit(rotated_r, t_min, t_max, rec, seed)) {
        ret = false;
    } $else {
        // auto p = rec.p;
        Float3 p = make_float3(
            cos_theta * rec.p[0] + sin_theta * rec.p[2],
            rec.p[1],
            -sin_theta * rec.p[0] + cos_theta * rec.p[2]
        );

        Float3 normal = make_float3(
            cos_theta * rec.normal[0] + sin_theta * rec.normal[2],
            rec.normal[1],
            -sin_theta * rec.normal[0] + cos_theta * rec.normal[2]
        );

        rec.p = p;
        rec.set_face_normal(rotated_r, normal);

        ret = true;
    };

    return ret;
}
