#include "deimos/modern_presentation_runtime.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace {

class CaptureBackend final : public deimos::ModernPresentationBackend {
public:
    bool should_fail = false;
    int calls = 0;
    deimos::ModernPresentationFrame captured{};

    bool present(const deimos::ModernPresentationFrame& frame, std::string* error) override {
        ++calls;
        captured = frame;
        if (should_fail) {
            if (error) *error = "synthetic backend failure";
            return false;
        }
        return true;
    }
};

} // namespace

int main() {
    using namespace deimos;

    // Exact 5->8 bit expansion and channel order.
    assert((expand_xrgb1555_to_rgba8888(0x0000) == std::array<std::uint8_t,4>{{0,0,0,255}}));
    assert((expand_xrgb1555_to_rgba8888(0x7fff) == std::array<std::uint8_t,4>{{255,255,255,255}}));
    assert((expand_xrgb1555_to_rgba8888(0x7c00) == std::array<std::uint8_t,4>{{255,0,0,255}}));
    assert((expand_xrgb1555_to_rgba8888(0x03e0) == std::array<std::uint8_t,4>{{0,255,0,255}}));
    assert((expand_xrgb1555_to_rgba8888(0x001f) == std::array<std::uint8_t,4>{{0,0,255,255}}));
    // xRGB bit 15 is intentionally ignored.
    assert(expand_xrgb1555_to_rgba8888(0xffff) == expand_xrgb1555_to_rgba8888(0x7fff));

    ModernViewport viewport;
    std::string error;
    assert(plan_modern_viewport(640, 480, {1920, 1080}, ModernScalingMode::AspectFit, viewport, &error));
    assert((viewport == ModernViewport{240, 0, 1440, 1080}));

    assert(plan_modern_viewport(640, 480, {1920, 1080}, ModernScalingMode::IntegerFit, viewport, &error));
    assert((viewport == ModernViewport{320, 60, 1280, 960}));

    // Integer mode falls back to aspect-fit for a drawable smaller than 1x.
    assert(plan_modern_viewport(640, 480, {320, 200}, ModernScalingMode::IntegerFit, viewport, &error));
    assert((viewport == ModernViewport{27, 0, 266, 200}));

    assert(plan_modern_viewport(640, 480, {1920, 1080}, ModernScalingMode::Stretch, viewport, &error));
    assert((viewport == ModernViewport{0, 0, 1920, 1080}));

    LegacyPresentationConfig legacy;
    LegacyRasterSurface canonical(640, 480, 0);
    canonical.pixels[0] = 0x7c00;
    canonical.pixels[1] = 0x03e0;
    canonical.pixels[2] = 0x001f;
    canonical.pixels[3] = 0x7fff;

    ModernPresentationOptions options;
    options.scaling = ModernScalingMode::AspectFit;
    options.sampling = ModernSamplingMode::Nearest;
    options.clear_rgba = {{1, 2, 3, 255}};

    ModernPresentationFrame frame;
    assert(build_modern_presentation_frame(canonical, legacy, {1280, 720}, options, frame, &error));
    assert(frame.valid());
    assert(frame.source_width == 640);
    assert(frame.source_height == 480);
    assert(frame.row_bytes == 2560);
    assert((frame.drawable == ModernDrawableSize{1280, 720}));
    assert((frame.viewport == ModernViewport{160, 0, 960, 720}));
    assert(frame.sampling == ModernSamplingMode::Nearest);
    assert((frame.clear_rgba == std::array<std::uint8_t,4>{{1,2,3,255}}));
    assert(frame.rgba8888.size() == 640u * 480u * 4u);
    assert((std::array<std::uint8_t,4>{{frame.rgba8888[0], frame.rgba8888[1], frame.rgba8888[2], frame.rgba8888[3]}} ==
            std::array<std::uint8_t,4>{{255,0,0,255}}));
    assert((std::array<std::uint8_t,4>{{frame.rgba8888[4], frame.rgba8888[5], frame.rgba8888[6], frame.rgba8888[7]}} ==
            std::array<std::uint8_t,4>{{0,255,0,255}}));

    std::vector<std::uint8_t> reference_drawable;
    assert(rasterize_modern_presentation_reference(frame, reference_drawable, &error));
    assert(reference_drawable.size() == 1280u * 720u * 4u);
    const auto pixel_at = [&](int x, int y) {
        const std::size_t i = (static_cast<std::size_t>(y) * 1280u + static_cast<std::size_t>(x)) * 4u;
        return std::array<std::uint8_t,4>{{reference_drawable[i], reference_drawable[i+1],
                                          reference_drawable[i+2], reference_drawable[i+3]}};
    };
    assert((pixel_at(0, 0) == std::array<std::uint8_t,4>{{1,2,3,255}}));
    assert((pixel_at(159, 10) == std::array<std::uint8_t,4>{{1,2,3,255}}));
    assert((pixel_at(160, 0) == std::array<std::uint8_t,4>{{255,0,0,255}}));
    // 960/640 = 1.5: first source pixel occupies two host pixels with the
    // integer-ratio nearest mapping used by the reference presenter.
    assert((pixel_at(161, 0) == std::array<std::uint8_t,4>{{255,0,0,255}}));
    assert((pixel_at(162, 0) == std::array<std::uint8_t,4>{{0,255,0,255}}));

    ModernPresentationFrame linear_frame = frame;
    linear_frame.sampling = ModernSamplingMode::Linear;
    error.clear();
    assert(!rasterize_modern_presentation_reference(linear_frame, reference_drawable, &error));
    assert(!error.empty());

    CaptureBackend backend;
    ModernPresentationResult result;
    assert(present_modern_frame(canonical, legacy, {2560, 1440}, options, backend, result, &error));
    assert(result.converted);
    assert(result.submitted);
    assert(result.upload_bytes == 640u * 480u * 4u);
    assert((result.viewport == ModernViewport{320, 0, 1920, 1440}));
    assert(backend.calls == 1);
    assert(backend.captured.valid());

    // Failed backend submission must not report a successful present.
    backend.should_fail = true;
    error.clear();
    assert(!present_modern_frame(canonical, legacy, {640, 480}, options, backend, result, &error));
    assert(result.converted);
    assert(!result.submitted);
    assert(error == "synthetic backend failure");

    // Prevent accidental double-scaling of an already host-sized legacy frame.
    LegacyRasterSurface wrong_size(800, 600, 0);
    error.clear();
    assert(!build_modern_presentation_frame(wrong_size, legacy, {800, 600}, options, frame, &error));
    assert(!error.empty());

    return 0;
}
