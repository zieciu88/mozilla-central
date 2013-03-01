/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureClient.h"
#include "ImageClient.h"
#include "BasicLayers.h"
#include "mozilla/layers/ShadowLayers.h"
#include "SharedTextureImage.h"
#include "ImageContainer.h" // For PlanarYCbCrImage
#include "mozilla/layers/SharedRGBImage.h"
#include "mozilla/layers/SharedPlanarYCbCrImage.h"

namespace mozilla {
namespace layers {

/* static */ TemporaryRef<ImageClient>
CompositingFactory::CreateImageClient(LayersBackend aParentBackend,
                                      CompositableType aCompositableHostType,
                                      CompositableForwarder* aForwarder,
                                      TextureFlags aFlags)
{
  RefPtr<ImageClient> result = nullptr;
  switch (aCompositableHostType) {
  case BUFFER_IMAGE_SINGLE:
    if (aParentBackend == LAYERS_OPENGL || aParentBackend == LAYERS_D3D11) {
      result = new ImageClientTexture(aForwarder, aFlags);
    }
    break;
  case BUFFER_IMAGE_BUFFERED:
    if (aParentBackend == LAYERS_OPENGL || aParentBackend == LAYERS_D3D11) {
      result = new ImageClientTextureBuffered(aForwarder, aFlags);
    }
    break;
  case BUFFER_BRIDGE:
    if (aParentBackend == LAYERS_OPENGL) {
      result = new ImageClientBridge(aForwarder, aFlags);
    }
    break;
  case BUFFER_UNKNOWN:
    result = nullptr;
    break;
  default:
    MOZ_NOT_REACHED("unhandled program type");
  }

  NS_ASSERTION(result, "Failed to create ImageClient");

  return result.forget();
}


ImageClient::ImageClient(CompositableForwarder* aFwd)
: CompositableClient(aFwd)
, mFilter(gfxPattern::FILTER_GOOD)
, mLastPaintedImageSerial(0)
{}

void
ImageClient::UpdatePictureRect(nsIntRect aRect)
{
  if (mPictureRect == aRect) {
    return;
  }
  mPictureRect = aRect;
  MOZ_ASSERT(mForwarder);
  GetForwarder()->UpdatePictureRect(this, aRect);
}

ImageClientTexture::ImageClientTexture(CompositableForwarder* aFwd,
                                       TextureFlags aFlags)
  : ImageClient(aFwd)
  , mFlags(aFlags)
  , mType(TEXTURE_SHMEM)
{
}

void
ImageClientTexture::EnsureTextureClient(TextureClientType aType)
{
  if (mTextureClient && mType == aType) {
    return;
  }
  mType = aType;
  mTextureClient = CreateTextureClient(aType, mFlags);
}

bool
ImageClientTexture::UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags)
{
  AutoLockImage autoLock(aContainer);
  Image *image = autoLock.GetImage();

  if (mLastPaintedImageSerial == image->GetSerial()) {
    return true;
  }

  if (image->GetFormat() == PLANAR_YCBCR) {
    EnsureTextureClient(TEXTURE_YCBCR);
    PlanarYCbCrImage* ycbcr = static_cast<PlanarYCbCrImage*>(image);

    if (false && ycbcr->AsSharedPlanarYCbCrImage()) { // WIP (nical)
      SurfaceDescriptor* desc = mTextureClient->LockSurfaceDescriptor();
      if (!ycbcr->AsSharedPlanarYCbCrImage()->ToSurfaceDescriptor(*desc)) {
        mTextureClient->Unlock();
        return false;
      }
      mTextureClient->Unlock();
    } else {
      AutoLockYCbCrClient clientLock(mTextureClient);
      if (!clientLock.Update(ycbcr)) {
        NS_WARNING("failed to update TextureClient (YCbCr)");
        return false;
      }
    }

    UpdatePictureRect(ycbcr->GetData()->GetPictureRect());
  } else if (image->GetFormat() == SHARED_TEXTURE) {
    EnsureTextureClient(TEXTURE_SHARED_GL_EXTERNAL);
    SharedTextureImage* sharedImage = static_cast<SharedTextureImage*>(image);
    const SharedTextureImage::Data *data = sharedImage->GetData();

    SharedTextureDescriptor texture(data->mShareType, 
                                    data->mHandle, 
                                    data->mSize, 
                                    data->mInverted);
    mTextureClient->SetDescriptor(SurfaceDescriptor(texture));
  } else {
    nsRefPtr<gfxASurface> surface;
    surface = image->GetAsSurface();
    MOZ_ASSERT(surface);

    EnsureTextureClient(TEXTURE_SHMEM);

    nsRefPtr<gfxPattern> pattern = new gfxPattern(surface);
    pattern->SetFilter(mFilter);

    AutoLockShmemClient clientLock(mTextureClient);
    if (!clientLock.Update(image, aContentFlags, pattern)) {
      NS_WARNING("failed to update TextureClient");
      return false;
    }
  }
  mLastPaintedImageSerial = image->GetSerial();
  aContainer->NotifyPaintedImage(image);
  return true;
}

/*void
ImageClientTexture::SetBuffer(const TextureInfo& aTextureInfo,
                              const SurfaceDescriptor& aBuffer)
{
  mTextureClient->SetDescriptor(aBuffer);
}*/

void
ImageClientTexture::Updated()
{
  mTextureClient->Updated();
}

ImageClientBridge::ImageClientBridge(CompositableForwarder* aFwd,
                                     TextureFlags aFlags)
: ImageClient(aFwd)
, mAsyncContainerID(0)
, mLayer(nullptr)
{
}

bool
ImageClientBridge::UpdateImage(ImageContainer* aContainer, uint32_t aContentFlags)
{
  if (!GetForwarder() || !mLayer) {
    return false;
  }
  if (mAsyncContainerID == aContainer->GetAsyncContainerID()) {
    return true;
  }
  mAsyncContainerID = aContainer->GetAsyncContainerID();
  GetForwarder()->AttachAsyncCompositable(mAsyncContainerID, mLayer);
  AutoLockImage autoLock(aContainer);
  aContainer->NotifyPaintedImage(autoLock.GetImage());
  return true;
}

already_AddRefed<Image>
ImageClient::CreateImage(const uint32_t *aFormats,
                         uint32_t aNumFormats)
{
  nsRefPtr<Image> img;
  for (uint32_t i = 0; i < aNumFormats; i++) {
    switch (aFormats[i]) {
      case PLANAR_YCBCR:
        img = new SharedPlanarYCbCrImage(this);
        return img.forget();
//      case SHARED_RGB:  // TODO[nical]
//        img = new SharedRGBImage(this);
//        return img.forget();
#ifdef MOZ_WIDGET_GONK
      case GONK_IO_SURFACE:
        img = new GonkIOSurfaceImage();
        return img.forget();
      case GRALLOC_PLANAR_YCBCR:
        img = new GrallocPlanarYCbCrImage();
        return img.forget();
#endif
    }
  }
  return nullptr;
}

}
}
