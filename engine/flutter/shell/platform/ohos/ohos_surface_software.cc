// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// 2021.4.30 platform view adapt ohos.
//           Copyright (c) 2021 Huawei Device Co., Ltd. All rights reserved.

#include "flutter/shell/platform/ohos/ohos_surface_software.h"

#include <memory>
#include <vector>

#include "flutter/fml/logging.h"
#include "flutter/fml/trace_event.h"

#include "display_type.h"

namespace flutter {

namespace {

bool GetSkColorType(int32_t buffer_format,
                    SkColorType* color_type,
                    SkAlphaType* alpha_type)
{
    switch (buffer_format) {
        case PIXEL_FMT_RGB_565:
            *color_type = kRGB_565_SkColorType;
            *alpha_type = kOpaque_SkAlphaType;
            return true;
        case PIXEL_FMT_RGBA_8888:
            *color_type = kRGBA_8888_SkColorType;
            *alpha_type = kPremul_SkAlphaType;
            return true;
        default:
            return false;
    }
    return false;
}

}  // anonymous namespace

OhosSurfaceSoftware::OhosSurfaceSoftware()
{
    GetSkColorType(PIXEL_FMT_RGBA_8888, &target_color_type_, &target_alpha_type_);
}

bool OhosSurfaceSoftware::IsValid() const
{
    return true;
}

std::unique_ptr<Surface> OhosSurfaceSoftware::CreateGPUSurface()
{
    if (!IsValid()) {
        return nullptr;
    }

    auto surface = std::make_unique<GPUSurfaceSoftware>(this);

    if (!surface->IsValid()) {
        return nullptr;
    }

    return surface;
}

void OhosSurfaceSoftware::SetPlatformWindow(OHOS::Window* window)
{
    if (window == nullptr) {
        FML_LOG(ERROR) << "OhosSurfaceSoftware::SetPlatformWindow, window is nullptr";
        return;
    }
    window_ = window;
    window_->GetRequestConfig(requestConfig_);
    surface_ = window->GetSurface();
    if (surface_ == nullptr) {
        FML_LOG(ERROR) << "OhosSurfaceSoftware::SetPlatformWindow, surface_ is nullptr";
        return;
    }
    // Set buffer size to 5 for enough buffer
    surface_->SetQueueSize(5);
}

sk_sp<SkSurface> OhosSurfaceSoftware::AcquireBackingStore(
    const SkISize& size)
{
    TRACE_EVENT0("flutter", "OhosSurfaceSoftware::AcquireBackingStore");
    if (!IsValid()) {
        return nullptr;
    }

    if (sk_surface_ != nullptr && SkISize::Make(sk_surface_->width(), sk_surface_->height()) == size) {
        // The old and new surface sizes are the same. Nothing to do here.
        return sk_surface_;
    }

    SkImageInfo image_info = SkImageInfo::Make(
        size.fWidth, size.fHeight, target_color_type_, target_alpha_type_, SkColorSpace::MakeSRGB());

    sk_surface_ = SkSurface::MakeRaster(image_info);

    return sk_surface_;
}

bool OhosSurfaceSoftware::PresentBackingStore(
    sk_sp<SkSurface> backing_store)
{
    TRACE_EVENT0("flutter", "OhosSurfaceSoftware::PresentBackingStore");
    if (!IsValid() || backing_store == nullptr) {
        return false;
    }

    FML_LOG(INFO) << "OhosSurfaceSoftware peek pixels";
    SkPixmap pixmap;
    if (!backing_store->peekPixels(&pixmap)) {
        return false;
    }


    if (surface_ == nullptr) {
        FML_LOG(ERROR) << "OhosSurfaceSoftware surface is nullptr";
        return false;
    }


    int32_t PIXEL_BASE = 16;
    int32_t convertWidth = requestConfig_.width % PIXEL_BASE == 0 ? requestConfig_.width
        : (requestConfig_.width / PIXEL_BASE + 1) * PIXEL_BASE;
    int32_t alignment = 8;
    OHOS::BufferRequestConfig requestConfig = {
        .width = convertWidth,
        .height = requestConfig_.height,
        .strideAlignment = alignment,
        .format = PIXEL_FMT_RGBA_8888,
        .usage = HBM_USE_CPU_READ | HBM_USE_CPU_WRITE | HBM_USE_MEM_DMA,
        .timeout = 0,
    };
    OHOS::sptr<OHOS::SurfaceBuffer> surfaceBuffer;
    int32_t releaseFence;

    OHOS::SurfaceError ret = surface_->RequestBuffer(surfaceBuffer, releaseFence, requestConfig);
    if (ret != OHOS::SURFACE_ERROR_OK) {
        FML_LOG(ERROR) << "OhosSurfaceSoftware RequestBuffer failed";
        return false;
    }

    if (surfaceBuffer == nullptr) {
        FML_LOG(ERROR) << "OhosSurfaceSoftware surfaceBuffer is nullptr";
        return false;
    }

    if (surfaceBuffer->GetSize() == 0) {
        FML_LOG(ERROR) << "OhosSurfaceSoftware surfaceBuffer size error";
        return false;
    }

    SkColorType color_type;
    SkAlphaType alpha_type;
    if (GetSkColorType(requestConfig.format, &color_type, &alpha_type)) {
        SkImageInfo native_image_info = SkImageInfo::Make(
            requestConfig.width, requestConfig.height, color_type, alpha_type);

        std::unique_ptr<SkCanvas> canvas = SkCanvas::MakeRasterDirect(
            native_image_info, surfaceBuffer->GetVirAddr(),
            surfaceBuffer->GetSize() / requestConfig.height);

        if (canvas) {
            SkBitmap bitmap;
            if (bitmap.installPixels(pixmap)) {
                canvas->drawBitmapRect(bitmap, SkRect::MakeIWH(requestConfig.width, requestConfig.height), nullptr);
            }
        }
    }

    FML_LOG(INFO) << "OhosSurfaceSoftware flush buffer";
    OHOS::BufferFlushConfig flushConfig = {
        .damage = {
            .x = 0,
            .y = 0,
            .w = requestConfig_.width,
            .h = requestConfig_.height,
        },
        .timestamp = 0
    };
    surface_->FlushBuffer(surfaceBuffer, -1, flushConfig);

    return true;
}

ExternalViewEmbedder* OhosSurfaceSoftware::GetExternalViewEmbedder()
{
    return nullptr;
}

}  // namespace flutter
