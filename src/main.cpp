#include <rtweekend.h>
#include <hittable_list.h>
#include <sphere.h>
#include <moving_sphere.h>
#include <bvh.h>
#include <ray.h>
#include <camera.h>
#include <material.h>
#include <aarect.h>
#include <box.h>
#include <constant_medium.h>

#include <luisa/core/clock.h>
#include <cxxopts.hpp>

#include <iostream>
#include <exception> // std::exception

#define MAX_DEPTH 50


namespace {

hittable_list random_scene();
hittable_list two_spheres();
hittable_list two_perlin_spheres(Device &d, Stream &s);
hittable_list earth(Device &d, Stream &s);
hittable_list simple_light(Device &d, Stream &s);
hittable_list cornell_box();
hittable_list cornell_smoke();
hittable_list final_scene(Device &d, Stream &s);

Float3 ray_color(
    const ray &r_,
    Float3 background,
    const hittable &world,
    UInt max_depth,
    UInt &seed
);

[[nodiscard]]
cxxopts::ParseResult parse_cli_options(
    int argc,
    const char *const *argv
) noexcept;

} // namespace anonymous end


int main(int argc, char *argv[]) {
    luisa::string_view program_name = argv[0];
    auto options = parse_cli_options(argc, argv);
    auto backend_name = options["backend"].as<luisa::string>();

    // Init
    Context context { program_name };
    Device device = context.create_device(backend_name);
    Stream stream = device.create_stream();

    // Image
    float aspect_ratio = 16.0f / 9.0f;
    uint image_width = 1920;
    std::size_t samples_per_pixel = options["samples"].as<std::size_t>();
    uint max_depth = MAX_DEPTH;

    // World
    hittable_list world;

    float3 lookfrom {};
    float3 lookat {};
    float vfov { 40.0f };
    float aperture { 0.0f };
    float3 background { 0.0f, 0.0f, 0.0f };

    // select scene
    switch (options["scene"].as<int>()) {
        case 1: {
            world = random_scene();
            background = float3 { 0.70f, 0.80f, 1.00f };
            lookfrom = float3 { 13.0f, 2.0f, 3.0f };
            lookat = float3 { 0.0f, 0.0f, 0.0f };
            vfov = 20.0f;
            aperture = 0.1f;
            break;
        }
        case 2: {
            world = two_spheres();
            background = float3 { 0.70f, 0.80f, 1.00f };
            lookfrom = float3 { 13.0f, 2.0f, 3.0f };
            lookat = float3 { 0.0f, 0.0f, 0.0f };
            vfov = 20.0f;
            break;
        }
        case 3: {
            world = two_perlin_spheres(device, stream);
            background = float3(0.70f, 0.80f, 1.00f);
            lookfrom = float3(13.0f, 2.0f, 3.0f);
            lookat = float3(0.0f, 0.0f, 0.0f);
            vfov = 20.0f;
            break;
        }
        case 4: {
            world = earth(device, stream);
            background = float3(0.70f, 0.80f, 1.00f);
            lookfrom = float3(13.0f, 2.0f, 3.0f);
            lookat = float3(0.0f, 0.0f, 0.0f);
            vfov = 20.0f;
            break;
        }
        case 5: {
            world = simple_light(device, stream);
            // samples_per_pixel = 400;
            background = float3(0, 0, 0);
            lookfrom = float3(26, 3, 6);
            lookat = float3(0, 2, 0);
            vfov = 20.0f;
            break;
        }
        case 6: {
            world = cornell_box();
            aspect_ratio = 1.0f;
            image_width = 600;
            // samples_per_pixel = 200;
            background = float3(0, 0, 0);
            lookfrom = float3(278, 278, -800);
            lookat = float3(278, 278, 0);
            vfov = 40.0f;
            break;
        }
        case 7: {
            world = cornell_smoke();
            aspect_ratio = 1.0f;
            image_width = 600;
            lookfrom = float3(278, 278, -800);
            lookat = float3(278, 278, 0);
            vfov = 40.0f;
            break;
        }
        case 8: {
            world = final_scene(device, stream);
            aspect_ratio = 1.0f;
            image_width = 800;
            background = float3(0, 0, 0);
            lookfrom = float3(478, 278, -600);
            lookat = float3(278, 278, 0);
            vfov = 40.0f;
            break;
        }
        default: {}
    }

    // Camera
    float3 vup { 0.0f, 1.0f, 0.0f };
    float dist_to_focus { 10.0f };
    camera cam(
        lookfrom,
        lookat,
        vup,
        vfov,
        aspect_ratio,
        aperture,
        dist_to_focus,
        0.0f,
        1.0f
    );

    // Render
    uint image_height = static_cast<uint>(static_cast<float>(image_width) / aspect_ratio);
    uint2 resolution = make_uint2(image_width, image_height);
    Image<uint> seed_image = device.create_image<uint>(PixelStorage::INT1, resolution, 1u, false, false);
    Image<float> accum_image = device.create_image<float>(PixelStorage::FLOAT4, resolution, 1u, false, false);
    luisa::vector<std::byte> host_image(accum_image.view().size_bytes());

    Kernel2D render_kernel = [&](
        ImageUInt seed_image,
        ImageFloat accum_image,
        UInt sample_index
    ) {
        UInt2 coord = dispatch_id().xy();
        UInt2 size = dispatch_size().xy();
        $if (sample_index == 0u) {
            seed_image.write(
                coord,
                make_uint4(tea(coord.x, coord.y))
            );
            accum_image.write(
                coord,
                make_float4(make_float3(0.0f), 1.0f)
            );
        };

        UInt seed = seed_image.read(coord).x;
        Float2 uv = make_float2(
            (cast<Float>(coord.x) + frand(seed)) / (cast<Float>(size.x) - 1.0f),
            (cast<Float>(size.y - 1u - coord.y) + frand(seed)) / (cast<Float>(size.y) - 1.0f)
        );
        ray r = cam.get_ray(uv, seed);
        Float3 pixel_color = ray_color(r, background, world, max_depth, seed);

        Float3 accum_color = lerp(
            accum_image.read(coord).xyz(),
            pixel_color,
            1.0f / (cast<Float>(sample_index) + 1.0f)
        );
        accum_image.write(
            coord,
            make_float4(accum_color, 1.0f)
        );
        seed_image.write(
            coord,
            make_uint4(seed)
        );
    };

    auto render = device.compile(render_kernel);

    Clock clk;
    for (std::size_t sample_index = 0; sample_index < samples_per_pixel; ++sample_index) {
        stream << render(seed_image, accum_image, sample_index).dispatch(resolution)
            << [sample_index, samples_per_pixel, &clk] () {
                LUISA_INFO(
                    "Samples: {} / {} ({:.1f}s)",
                    sample_index + 1u,
                    samples_per_pixel,
                    clk.toc() * 1e-3
                );
            };
    }

    // Gamma Correct
    Kernel2D gamma_kernel = [&](ImageFloat accum_image, ImageFloat output) {
        UInt2 coord = dispatch_id().xy();
        output.write(
            coord,
            make_float4(sqrt(accum_image.read(coord).xyz()), 1.0f)
        );
    };

    auto gamma_correct = device.compile(gamma_kernel);
    auto output_image = device.create_image<float>(PixelStorage::BYTE4, resolution);

    stream << gamma_correct(accum_image, output_image).dispatch(resolution);
    stream << output_image.copy_to(host_image.data()) << synchronize();
    stbi_write_png(
        (options["outfile"].as<luisa::string>() + ".png").c_str(),
        static_cast<int>(resolution.x),
        static_cast<int>(resolution.y),
        4,
        host_image.data(),
        0
    );

    return 0;
}


namespace {

hittable_list random_scene() {
    hittable_list world;

    auto checker = make_shared<checker_texture>(
        float3(0.2, 0.3, 0.1),
        float3(0.9, 0.9, 0.9)
    );
    world.add(
        make_shared<sphere>(
            float3(0, -1000, 0),
            1000,
            make_shared<lambertian>(checker)
        )
    );

    // auto ground_material = make_shared<lambertian>(float3(0.5, 0.5, 0.5));
    // materials.push_back(ground_material);
    // world.add(make_shared<sphere>(float3(0, -1000, 0), 1000, ground_material));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            auto choose_mat = random_float();
            float3 center(
                static_cast<float>(a) + 0.9f * random_float(),
                0.2f,
                static_cast<float>(b) + 0.9f * random_float()
            );

            if (length(center - float3(4, 0.2, 0)) > 0.9f) {
                shared_ptr<material> sphere_material;

                if (choose_mat < 0.8f) {
                    // diffuse
                    auto albedo = make_float3(random_float(), random_float(), random_float()) * make_float3(random_float(), random_float(), random_float());
                    sphere_material = make_shared<lambertian>(albedo);
                    auto center2 = center + float3(0, random_float(0, 0.5), 0);
                    world.add(make_shared<moving_sphere>(center, center2, 0.0, 1.0, 0.2, sphere_material));
                } else if (choose_mat < 0.95f) {
                    // metal
                    auto albedo = make_float3(random_float(0.5, 1), random_float(0.5, 1), random_float(0.5, 1));
                    auto fuzz = random_float(0, 0.5);
                    sphere_material = make_shared<metal>(albedo, fuzz);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                } else {
                    // glass
                    sphere_material = make_shared<dielectric>(1.5);
                    world.add(make_shared<sphere>(center, 0.2, sphere_material));
                }
            }
        }
    }

    auto material1 = make_shared<dielectric>(1.5);
    world.add(make_shared<sphere>(float3(0, 1, 0), 1.0, material1));

    auto material2 = make_shared<lambertian>(float3(0.4, 0.2, 0.1));
    world.add(make_shared<sphere>(float3(-4, 1, 0), 1.0, material2));

    auto material3 = make_shared<metal>(float3(0.7, 0.6, 0.5), 0.0);
    world.add(make_shared<sphere>(float3(4, 1, 0), 1.0, material3));

    world = hittable_list(make_shared<bvh_node>(world));

    return world;
}

hittable_list two_spheres() {
    hittable_list objects;

    auto checker = luisa::make_shared<checker_texture>(
        float3 { 0.2f, 0.3f, 0.1f },
        float3 { 0.9f, 0.9f, 0.9f }
    );

    objects.add(
        luisa::make_shared<sphere>(
            float3(0.0f, -10.0f, 0.0f),
            10.0f,
            luisa::make_shared<lambertian>(checker)
        )
    );
    objects.add(
        luisa::make_shared<sphere>(
            float3 { 0.0f, 10.0f, 0.0f },
            10.0f,
            luisa::make_shared<lambertian>(checker)
        )
    );

    return objects;
}

hittable_list two_perlin_spheres(Device &d, Stream &s) {
    hittable_list objects;

    auto pertext = make_shared<noise_texture>(d, s, 4);
    objects.add(make_shared<sphere>(float3(0, -1000, 0), 1000, make_shared<lambertian>(pertext)));
    objects.add(make_shared<sphere>(float3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

    return objects;
}

hittable_list earth(Device &d, Stream &s) {
    auto earth_texture = make_shared<image_texture>(d, s, "assets/earthmap.jpg");
    auto earth_surface = make_shared<lambertian>(earth_texture);
    auto globe = make_shared<sphere>(float3(0, 0, 0), 2, earth_surface);

    return { globe };
}

hittable_list simple_light(Device &d, Stream &s) {
    hittable_list objects;

    auto pertext = make_shared<noise_texture>(d, s, 4);
    objects.add(make_shared<sphere>(float3(0, -1000, 0), 1000, make_shared<lambertian>(pertext)));
    objects.add(make_shared<sphere>(float3(0, 2, 0), 2, make_shared<lambertian>(pertext)));

    auto difflight = make_shared<diffuse_light>(float3(4, 4, 4));
    objects.add(make_shared<xy_rect>(3, 5, 1, 3, -2, difflight));

    return objects;
}

hittable_list cornell_box() {
    hittable_list objects;

    auto red = make_shared<lambertian>(float3(.65, .05, .05));
    auto white = make_shared<lambertian>(float3(.73, .73, .73));
    auto green = make_shared<lambertian>(float3(.12, .45, .15));
    auto light = make_shared<diffuse_light>(float3(15, 15, 15));

    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
    objects.add(make_shared<xz_rect>(213, 343, 227, 332, 554, light));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
    objects.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

    shared_ptr<hittable> box1 = make_shared<box>(float3(0, 0, 0), float3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, float3(265, 0, 295));
    objects.add(box1);

    shared_ptr<hittable> box2 = make_shared<box>(float3(0, 0, 0), float3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, float3(130, 0, 65));
    objects.add(box2);

    return objects;
}

hittable_list cornell_smoke() {
    hittable_list objects;

    auto red = make_shared<lambertian>(float3(.65, .05, .05));
    auto white = make_shared<lambertian>(float3(.73, .73, .73));
    auto green = make_shared<lambertian>(float3(.12, .45, .15));
    auto light = make_shared<diffuse_light>(float3(7, 7, 7));

    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 555, green));
    objects.add(make_shared<yz_rect>(0, 555, 0, 555, 0, red));
    objects.add(make_shared<xz_rect>(113, 443, 127, 432, 554, light));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 555, white));
    objects.add(make_shared<xz_rect>(0, 555, 0, 555, 0, white));
    objects.add(make_shared<xy_rect>(0, 555, 0, 555, 555, white));

    shared_ptr<hittable> box1 = make_shared<box>(float3(0, 0, 0), float3(165, 330, 165), white);
    box1 = make_shared<rotate_y>(box1, 15);
    box1 = make_shared<translate>(box1, float3(265, 0, 295));

    shared_ptr<hittable> box2 = make_shared<box>(float3(0, 0, 0), float3(165, 165, 165), white);
    box2 = make_shared<rotate_y>(box2, -18);
    box2 = make_shared<translate>(box2, float3(130, 0, 65));

    objects.add(make_shared<constant_medium>(box1, 0.01, float3(0, 0, 0)));
    objects.add(make_shared<constant_medium>(box2, 0.01, float3(1, 1, 1)));

    return objects;
}

hittable_list final_scene(Device &d, Stream &s) {
    hittable_list boxes1;
    auto ground = make_shared<lambertian>(float3(0.48, 0.83, 0.53));

    const int boxes_per_side = 20;
    for (int i = 0; i < boxes_per_side; i++) {
        for (int j = 0; j < boxes_per_side; j++) {
            auto w = 100.0f;
            auto x0 = -1000.0f + static_cast<float>(i) * w;
            auto z0 = -1000.0f + static_cast<float>(j) * w;
            auto y0 = 0.0f;
            auto x1 = x0 + w;
            auto y1 = random_float(1.0f, 101.0f);
            auto z1 = z0 + w;

            boxes1.add(
                make_shared<box>(
                    float3(x0, y0, z0),
                    float3(x1, y1, z1),
                    ground
                )
            );
        }
    }

    hittable_list objects;

    objects.add(make_shared<bvh_node>(boxes1));

    auto light = make_shared<diffuse_light>(float3(7, 7, 7));
    objects.add(make_shared<xz_rect>(123, 423, 147, 412, 554, light));

    auto center1 = float3(400, 400, 200);
    auto center2 = center1 + float3(30, 0, 0);
    auto moving_sphere_material = make_shared<lambertian>(float3(0.7, 0.3, 0.1));
    objects.add(make_shared<moving_sphere>(center1, center2, 0, 1, 50, moving_sphere_material));

    objects.add(make_shared<sphere>(float3(260, 150, 45), 50, make_shared<dielectric>(1.5)));
    objects.add(make_shared<sphere>(
        float3(0, 150, 145), 50, make_shared<metal>(float3(0.8, 0.8, 0.9), 1.0)));

    auto boundary = make_shared<sphere>(float3(360, 150, 145), 70, make_shared<dielectric>(1.5));
    objects.add(boundary);
    objects.add(make_shared<constant_medium>(boundary, 0.2, float3(0.2, 0.4, 0.9)));
    boundary = make_shared<sphere>(float3(0, 0, 0), 5000, make_shared<dielectric>(1.5));
    objects.add(make_shared<constant_medium>(boundary, .0001, float3(1, 1, 1)));

    // requires earthmap.jpg
    // auto emat = make_shared<lambertian>(make_shared<image_texture>(d, s, "assets/earthmap.jpg"));
    // objects.add(make_shared<sphere>(float3(400, 200, 400), 100, emat));
    auto pertext = make_shared<noise_texture>(d, s, 0.1);
    objects.add(make_shared<sphere>(float3(220, 280, 300), 80, make_shared<lambertian>(pertext)));

    hittable_list boxes2;
    auto white = make_shared<lambertian>(float3(.73, .73, .73));
    int ns = 1000;
    for (int j = 0; j < ns; j++) {
        boxes2.add(make_shared<sphere>(float3(random_float(0, 165)), 10, white));
    }

    objects.add(make_shared<translate>(
        make_shared<rotate_y>(
            make_shared<bvh_node>(boxes2), 15),
        float3(-100, 270, 395)));

    return objects;
}

Float3 ray_color(
    const ray &r_,
    const Float3 background,
    const hittable &world,
    UInt max_depth,
    UInt &seed
) {
    Float3 ret {};

    ArrayFloat3<MAX_DEPTH + 1> emittedRec;
    ArrayFloat3<MAX_DEPTH + 1> attenuationRec;
    ray r = r_;
    UInt depth = max_depth;
    hit_record rec;
    $loop {
        // If we've exceeded the ray bounce limit, no more light is gathered.
        $if (depth <= 0) {
            emittedRec[depth] = make_float3(0);
            attenuationRec[depth] = make_float3(0);
            $break;
        };

        // If the ray hits nothing, return the background color.
        $if (!world.hit(r, 0.001f, infinity, rec, seed)) {
            emittedRec[depth] = make_float3(0);
            attenuationRec[depth] = background;
            $break;
        };

        ray scattered;
        Float3 attenuation;
        Float3 emitted;
        Bool hasScatter;

        for (uint mat_id = 0; mat_id < materials.size(); mat_id++) {
            $if (rec.mat_id == mat_id) {
                emitted = materials[mat_id]->emitted(rec.u, rec.v, rec.p);
                hasScatter = materials[mat_id]->scatter(r, rec, attenuation, scattered, seed);
            };
        }

        $if (!hasScatter) {
            emittedRec[depth] = emitted;
            attenuationRec[depth] = make_float3(0);
            $break;
        };

        emittedRec[depth] = emitted;
        attenuationRec[depth] = attenuation;
        r = scattered;
        depth -= 1u;
    };

    ret = make_float3(1.0f);
    $loop {
        ret = emittedRec[depth] + attenuationRec[depth] * ret;
        depth += 1u;

        $if (depth > max_depth) {
            $break;
        };
    };
    return ret;
};

cxxopts::ParseResult parse_cli_options(
    int argc,
    const char *const *argv
) noexcept {
    cxxopts::Options cli {
        "RayTracing",
        "ray tracing with luisa compute"
    };
    cli.add_option("", "h", "help", "Display this help message", cxxopts::value<bool>()->default_value("false"), "");
    cli.add_option("", "b", "backend", "Compute backend name", cxxopts::value<luisa::string>(), "<backend>");
    cli.add_option("", "s", "samples", "Samples per pixel", cxxopts::value<std::size_t>()->default_value("100"), "<numbers>");
    cli.add_option(
        "",
        "i",
        "scene",
        R"(scene id,
        1: random scene;
        2: two_spheres;
        3: two_perlin_shperes;
        4: earth;
        5: simple_light;
        6: cornell_box;
        7: cornell_smoke;
        8: final_scene
        )",
        cxxopts::value<int>()->default_value("1"),
        "<scene_id>"
    );
    cli.add_option("", "o", "outfile", "output image file name", cxxopts::value<luisa::string>()->default_value("./test"), "<image_name>");

    const cxxopts::ParseResult options = [&] {
        try {
            return cli.parse(argc, argv);
        } catch (const std::exception &e) {
            LUISA_WARNING_WITH_LOCATION(
                "Failed to parse command line arguments: {}.",
                e.what()
            );
            std::cout << cli.help() << std::endl;
            exit(-1);
        }
    }();
    if (options["help"].as<bool>()) {
        std::cout << cli.help() << std::endl;
        exit(0);
    }

    return options;
}

} // namespace anonymous end
