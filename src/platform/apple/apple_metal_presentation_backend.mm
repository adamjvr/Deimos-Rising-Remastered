#include "deimos/apple_metal_presentation_backend.hpp"

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace deimos {
namespace {

bool fail(std::string* error, const std::string& message) {
    if (error) *error = message;
    return false;
}

std::string ns_error_text(NSError* error, const char* fallback) {
    if (error != nil) {
        NSString* text = error.localizedDescription;
        if (text != nil) return std::string(text.UTF8String ?: fallback);
    }
    return std::string(fallback);
}

constexpr const char* kShaderSource = R"METAL(
#include <metal_stdlib>
using namespace metal;

struct DeimosVertexOut {
    float4 position [[position]];
    float2 texcoord;
};

vertex DeimosVertexOut deimos_present_vertex(uint vertex_id [[vertex_id]]) {
    constexpr float2 positions[4] = {
        float2(-1.0,  1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
    };
    constexpr float2 texcoords[4] = {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0),
    };

    DeimosVertexOut out;
    out.position = float4(positions[vertex_id], 0.0, 1.0);
    out.texcoord = texcoords[vertex_id];
    return out;
}

fragment float4 deimos_present_fragment(
    DeimosVertexOut in [[stage_in]],
    texture2d<float> source [[texture(0)]],
    sampler source_sampler [[sampler(0)]]) {
    return source.sample(source_sampler, in.texcoord);
}
)METAL";

} // namespace

struct AppleMetalPresentationBackend::Impl {
    __strong CAMetalLayer* layer = nil;
    __strong id<MTLDevice> device = nil;
    __strong id<MTLCommandQueue> queue = nil;
    __strong id<MTLLibrary> library = nil;
    __strong id<MTLRenderPipelineState> pipeline = nil;
    __strong id<MTLSamplerState> nearest_sampler = nil;
    __strong id<MTLSamplerState> linear_sampler = nil;
    __strong id<MTLTexture> source_texture = nil;
    MTLPixelFormat pipeline_pixel_format = MTLPixelFormatInvalid;
    int texture_width = 0;
    int texture_height = 0;

    explicit Impl(void* layer_handle) {
        set_layer(layer_handle);
    }

    void reset_pipeline() {
        pipeline = nil;
        library = nil;
        pipeline_pixel_format = MTLPixelFormatInvalid;
    }

    void reset_device_resources() {
        source_texture = nil;
        texture_width = 0;
        texture_height = 0;
        nearest_sampler = nil;
        linear_sampler = nil;
        queue = nil;
        reset_pipeline();
    }

    void set_layer(void* layer_handle) {
        CAMetalLayer* next = (__bridge CAMetalLayer*)layer_handle;
        if (layer == next) return;
        layer = next;
        reset_device_resources();
        device = nil;
    }

    bool ensure_device(std::string* error) {
        if (layer == nil) return fail(error, "Metal backend has no CAMetalLayer");

        id<MTLDevice> wanted = layer.device;
        if (wanted == nil) {
            wanted = MTLCreateSystemDefaultDevice();
            if (wanted == nil)
                return fail(error, "Metal is unavailable: MTLCreateSystemDefaultDevice returned nil");
            layer.device = wanted;
        }

        if (device != wanted) {
            reset_device_resources();
            device = wanted;
        }

        if (queue == nil) {
            queue = [device newCommandQueue];
            if (queue == nil) return fail(error, "Metal failed to create a command queue");
            queue.label = @"Deimos Rising presentation queue";
        }

        return true;
    }

    bool ensure_samplers(std::string* error) {
        if (nearest_sampler != nil && linear_sampler != nil) return true;

        MTLSamplerDescriptor* nearest = [[MTLSamplerDescriptor alloc] init];
        nearest.minFilter = MTLSamplerMinMagFilterNearest;
        nearest.magFilter = MTLSamplerMinMagFilterNearest;
        nearest.sAddressMode = MTLSamplerAddressModeClampToEdge;
        nearest.tAddressMode = MTLSamplerAddressModeClampToEdge;
        nearest.label = @"Deimos Rising nearest sampler";
        nearest_sampler = [device newSamplerStateWithDescriptor:nearest];

        MTLSamplerDescriptor* linear = [[MTLSamplerDescriptor alloc] init];
        linear.minFilter = MTLSamplerMinMagFilterLinear;
        linear.magFilter = MTLSamplerMinMagFilterLinear;
        linear.sAddressMode = MTLSamplerAddressModeClampToEdge;
        linear.tAddressMode = MTLSamplerAddressModeClampToEdge;
        linear.label = @"Deimos Rising linear sampler";
        linear_sampler = [device newSamplerStateWithDescriptor:linear];

        if (nearest_sampler == nil || linear_sampler == nil)
            return fail(error, "Metal failed to create presentation samplers");
        return true;
    }

    bool ensure_pipeline(std::string* error) {
        const MTLPixelFormat format = layer.pixelFormat;
        if (format == MTLPixelFormatInvalid)
            return fail(error, "CAMetalLayer has an invalid pixel format");
        if (pipeline != nil && pipeline_pixel_format == format) return true;

        NSError* library_error = nil;
        NSString* source = [NSString stringWithUTF8String:kShaderSource];
        library = [device newLibraryWithSource:source options:nil error:&library_error];
        if (library == nil)
            return fail(error, ns_error_text(library_error, "Metal failed to compile presentation shaders"));

        id<MTLFunction> vertex = [library newFunctionWithName:@"deimos_present_vertex"];
        id<MTLFunction> fragment = [library newFunctionWithName:@"deimos_present_fragment"];
        if (vertex == nil || fragment == nil)
            return fail(error, "Metal presentation shader functions are missing");

        MTLRenderPipelineDescriptor* descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        descriptor.label = @"Deimos Rising presentation pipeline";
        descriptor.vertexFunction = vertex;
        descriptor.fragmentFunction = fragment;
        descriptor.colorAttachments[0].pixelFormat = format;

        NSError* pipeline_error = nil;
        pipeline = [device newRenderPipelineStateWithDescriptor:descriptor error:&pipeline_error];
        if (pipeline == nil)
            return fail(error, ns_error_text(pipeline_error, "Metal failed to create the presentation pipeline"));

        pipeline_pixel_format = format;
        return true;
    }

    bool ensure_texture(int width, int height, std::string* error) {
        if (source_texture != nil && texture_width == width && texture_height == height) return true;
        if (width <= 0 || height <= 0) return fail(error, "Metal source texture dimensions must be positive");

        MTLTextureDescriptor* descriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:static_cast<NSUInteger>(width)
            height:static_cast<NSUInteger>(height)
            mipmapped:NO];
        descriptor.usage = MTLTextureUsageShaderRead;
        descriptor.storageMode = MTLStorageModeShared;
        source_texture = [device newTextureWithDescriptor:descriptor];
        if (source_texture == nil) return fail(error, "Metal failed to create the source texture");
        source_texture.label = @"Deimos Rising canonical RGBA frame";
        texture_width = width;
        texture_height = height;
        return true;
    }

    bool ready(std::string* error) {
        if (!ensure_device(error)) return false;
        if (!ensure_samplers(error)) return false;
        if (!ensure_pipeline(error)) return false;
        return true;
    }

    ModernDrawableSize drawable_size() const noexcept {
        if (layer == nil) return {};
        const CGSize size = layer.drawableSize;
        return {
            static_cast<int>(std::llround(size.width)),
            static_cast<int>(std::llround(size.height)),
        };
    }

    bool present(const ModernPresentationFrame& frame, std::string* error) {
        if (!frame.valid()) return fail(error, "Metal backend received an invalid modern presentation frame");
        if (!ready(error)) return false;

        const ModernDrawableSize actual = drawable_size();
        if (actual != frame.drawable) {
            return fail(error,
                "Metal drawable size does not match the modern presentation frame; rebuild the frame after resize");
        }

        if (!ensure_texture(frame.source_width, frame.source_height, error)) return false;

        const MTLRegion region = MTLRegionMake2D(
            0, 0,
            static_cast<NSUInteger>(frame.source_width),
            static_cast<NSUInteger>(frame.source_height));
        [source_texture replaceRegion:region
                         mipmapLevel:0
                           withBytes:frame.rgba8888.data()
                         bytesPerRow:static_cast<NSUInteger>(frame.row_bytes)];

        id<CAMetalDrawable> drawable = [layer nextDrawable];
        if (drawable == nil) return fail(error, "CAMetalLayer returned no drawable");

        id<MTLCommandBuffer> command = [queue commandBuffer];
        if (command == nil) return fail(error, "Metal failed to create a command buffer");
        command.label = @"Deimos Rising presentation command buffer";

        MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];
        pass.colorAttachments[0].texture = drawable.texture;
        pass.colorAttachments[0].loadAction = MTLLoadActionClear;
        pass.colorAttachments[0].storeAction = MTLStoreActionStore;
        pass.colorAttachments[0].clearColor = MTLClearColorMake(
            static_cast<double>(frame.clear_rgba[0]) / 255.0,
            static_cast<double>(frame.clear_rgba[1]) / 255.0,
            static_cast<double>(frame.clear_rgba[2]) / 255.0,
            static_cast<double>(frame.clear_rgba[3]) / 255.0);

        id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
        if (encoder == nil) return fail(error, "Metal failed to create a render encoder");
        encoder.label = @"Deimos Rising presentation encoder";
        [encoder setRenderPipelineState:pipeline];

        const MTLViewport viewport = {
            static_cast<double>(frame.viewport.x),
            static_cast<double>(frame.viewport.y),
            static_cast<double>(frame.viewport.width),
            static_cast<double>(frame.viewport.height),
            0.0,
            1.0,
        };
        [encoder setViewport:viewport];
        [encoder setFragmentTexture:source_texture atIndex:0];
        [encoder setFragmentSamplerState:
            (frame.sampling == ModernSamplingMode::Linear ? linear_sampler : nearest_sampler)
                              atIndex:0];
        [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
        [encoder endEncoding];

        [command presentDrawable:drawable];
        [command commit];
        return true;
    }
};

AppleMetalPresentationBackend::AppleMetalPresentationBackend(void* cametal_layer)
    : impl_(std::make_unique<Impl>(cametal_layer)) {}

AppleMetalPresentationBackend::~AppleMetalPresentationBackend() = default;
AppleMetalPresentationBackend::AppleMetalPresentationBackend(AppleMetalPresentationBackend&&) noexcept = default;
AppleMetalPresentationBackend& AppleMetalPresentationBackend::operator=(AppleMetalPresentationBackend&&) noexcept = default;

void AppleMetalPresentationBackend::set_layer(void* cametal_layer) {
    impl_->set_layer(cametal_layer);
}

void* AppleMetalPresentationBackend::layer_handle() const noexcept {
    return impl_->layer == nil ? nullptr : (__bridge void*)impl_->layer;
}

ModernDrawableSize AppleMetalPresentationBackend::drawable_size() const noexcept {
    return impl_->drawable_size();
}

bool AppleMetalPresentationBackend::ready(std::string* error) {
    return impl_->ready(error);
}

bool AppleMetalPresentationBackend::present(
    const ModernPresentationFrame& frame,
    std::string* error) {
    return impl_->present(frame, error);
}

} // namespace deimos
