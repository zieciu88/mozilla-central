/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GFX_LAYERSTYPES_H
#define GFX_LAYERSTYPES_H

namespace mozilla {
namespace layers {
enum LayersBackend {
  LAYERS_NONE = 0,
  LAYERS_BASIC,
  LAYERS_OPENGL,
  LAYERS_D3D9,
  LAYERS_D3D10,
  LAYERS_LAST
};

enum BufferMode {
  BUFFER_NONE,
  BUFFER_BUFFERED
};

// The kinds of mask layer a shader can support
// We rely on the items in this enum being sequential
enum MaskType {
  MaskNone = 0,   // no mask layer
  Mask2d,         // mask layer for layers with 2D transforms
  Mask3d,         // mask layer for layers with 3D transforms
  NumMaskTypes
};

}
}

#endif /* GFX_LAYERSTYPES_H */
