// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "txt/platform.h"

namespace txt {

std::string GetDefaultFontFamily() {
  return "Arial";
}

sk_sp<SkFontMgr> GetDefaultFontManager() {
  return SkFontMgr::RefDefault();
}

FontManagerType GetDefaultFontManagerType() {
  return FontManagerType::NONE;
}

}  // namespace txt
