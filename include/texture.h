#pragma once

#include "rtweekend.h"
#include "perlin.h"

#include <utility>


class texture {
public:
    [[nodiscard]]
    virtual Float3 value(Float u, Float v, const Float3 &p) const = 0;
};


class solid_color : public texture {
public:
    float3 color_value {};

public:
    solid_color() = default;

    explicit solid_color(float3 c)
        : color_value(c)
    {}

    solid_color(float red, float green, float blue)
        : solid_color(float3(red, green, blue))
    {}

    [[nodiscard]]
    Float3 value(Float u, Float v, const Float3 &p) const override {
        return color_value;
    }
};


class checker_texture : public texture {
public:
    luisa::shared_ptr<texture> odd;
    luisa::shared_ptr<texture> even;

public:
    checker_texture() = default;

    checker_texture(
        luisa::shared_ptr<texture> _even,
        luisa::shared_ptr<texture> _odd
    )
        : even(std::move(_even))
        , odd(std::move(_odd))
    {}

    checker_texture(float3 c1, float3 c2)
        : even(luisa::make_shared<solid_color>(c1))
        , odd(luisa::make_shared<solid_color>(c2))
    {}

    [[nodiscard]]
    Float3 value(Float u, Float v, const Float3 &p) const override {
        Float3 ret {};

        auto sines = sin(10.0f * p.x) * sin(10.0f * p.y) * sin(10.0f * p.z);
        $if (sines < 0.0f) {
            ret = odd->value(u, v, p);
        } $else {
            ret = even->value(u, v, p);
        };

        return ret;
    }
};


class noise_texture : public texture {
public:
    perlin noise;
    float scale {};

public:
    noise_texture(Device &d, Stream &s)
        : noise(perlin(d, s))
        , scale(0.0f)
    {}

    noise_texture(Device &d, Stream &s, float sc)
        : noise(perlin(d, s))
        , scale(sc)
    {}

    [[nodiscard]]
    Float3 value(Float u, Float v, const Float3 &p) const override {
        return 0.5f * Float3(1.0f, 1.0f, 1.0f) * (
            1.0f + sin(scale * p.z + 10.0f * noise.turb(p))
        );
    }
};


class image_texture : public texture {
public:
    unsigned char *data { nullptr };
    int width {};
    int height {};
    int bytes_per_scanline {};
    Buffer<int> dataBuf;
    const static int bytes_per_pixel { 4 };

public:
    image_texture()
        : data(nullptr)
        , width(0)
        , height(0)
        , bytes_per_scanline(0)
    {}

    image_texture(Device &device, Stream &stream, const char *filename) {
        auto components_per_pixel = bytes_per_pixel;

        data = stbi_load(
            filename,
            &width,
            &height,
            &components_per_pixel,
            components_per_pixel
        );

        if (!data) {
            LUISA_ERROR("ERROR: Could not load texture image file '{}'.\n", filename);
            width = height = 0;
        }
        int pos = 1024 * 256 + 512;

        bytes_per_scanline = bytes_per_pixel * width;

        dataBuf = device.create_buffer<int>(width * height);
        stream << dataBuf.copy_from(data) << synchronize();
    }

    ~image_texture() {
        delete data;
    }

    [[nodiscard]]
    Float3 value(Float u, Float v, const Float3 &p) const override {
        // If we have no texture data, then return solid cyan as a debugging aid.
        if (data == nullptr) {
            return { 0.0f, 1.0f, 1.0f };
        }

        // Clamp input texture coordinates to [0,1] x [1,0]
        u = clamp(u, 0.0f, 1.0f);
        v = 1.0f - clamp(v, 0.0f, 1.0f);// Flip V to image coordinates

        auto i = static_cast<Int>(u * cast<Float>(width));
        auto j = static_cast<Int>(v * cast<Float>(height));

        // Clamp integer mapping, since actual coordinates should be less than 1.0
        $if (i >= width) { i = width - 1; };
        $if (j >= height) { j = height - 1; };

        const Float color_scale = 1.0f / 255.0f;
        Int pixel = dataBuf->read(j * width + i);
        Int pixel_0 = pixel & 255;
        Int pixel_1 = (pixel >> 8) & 255;
        Int pixel_2 = (pixel >> 16) & 255;

        return {
            color_scale * cast<Float>(pixel_0),
            color_scale * cast<Float>(pixel_1),
            color_scale * cast<Float>(pixel_2)
        };
    }
};
