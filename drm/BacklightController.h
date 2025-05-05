/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <string>

namespace android {

class BacklightController {
 public:
  static auto CreateInstance(std::string name)
      -> std::unique_ptr<BacklightController>;

  BacklightController(const BacklightController &) = delete;
  BacklightController &operator=(const BacklightController &) = delete;
  ~BacklightController() = default;

  bool SetBrightness(std::optional<float> brightness);

  auto name() const -> std::string;

 private:
  BacklightController(std::string name, std::string path, bool powered, int max,
                      bool log_scale)
      : name_(name),
        sysfs_path_(std::move(path)),
        powered_(powered),
        max_(max),
        log_scale_(log_scale) {
  }
  auto BrightnessToSliderPosition(float linear) -> float;

  std::string name_;
  std::string sysfs_path_;
  bool powered_;
  int max_;
  bool log_scale_;
};

}  // namespace android
