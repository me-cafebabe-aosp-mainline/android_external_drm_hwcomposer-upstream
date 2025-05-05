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

#define LOG_TAG "drmhwc"

#include <linux/fb.h>
#include <vector>

#include <android-base/file.h>
#include <android-base/parseint.h>
#include <android-base/strings.h>

#include "BacklightController.h"
#include "DrmConnector.h"
#include "DrmDevice.h"
#include "utils/log.h"
#include "utils/properties.h"

namespace android {

auto BacklightController::CreateInstance(std::string name)
    -> std::unique_ptr<BacklightController> {
  static const char kBasePath[] = "/sys/class/backlight/";
  std::ostringstream path;
  path << kBasePath << name << "/";

  struct stat buf{};
  // This connector may not have a backlight, check for the backlight directory.
  if (stat(path.str().c_str(), &buf) != 0 || !S_ISDIR(buf.st_mode))
    return nullptr;

  std::string power_path(path.str());
  power_path += "bl_power";
  std::string file_contents;
  if (!::android::base::ReadFileToString(power_path, &file_contents)) {
    ALOGE("Could not read power from %s", power_path.c_str());
    return nullptr;
  }

  file_contents = ::android::base::Trim(file_contents);
  int power = 0;
  if (!::android::base::ParseInt(file_contents, &power)) {
    ALOGE("Could not parse power (%s) from %s (%d)", file_contents.c_str(),
          power_path.c_str(), errno);
    return nullptr;
  }

  bool powered = false;
  if (power == FB_BLANK_UNBLANK) {
    powered = true;
  } else if (power == FB_BLANK_POWERDOWN) {
    powered = false;
  } else {
    ALOGE("Read undefined value from bl_power (%s) = %d\n", power_path.c_str(),
          power);
  }

  std::string max_brightness_path(path.str());
  max_brightness_path += "max_brightness";
  if (!::android::base::ReadFileToString(max_brightness_path, &file_contents)) {
    ALOGE("Could not read max brightness from %s", max_brightness_path.c_str());
    return nullptr;
  }

  file_contents = ::android::base::Trim(file_contents);
  int max = 0;
  if (!::android::base::ParseInt(file_contents, &max)) {
    ALOGE("Could not parse max brightness (%s) from %s (%d)",
          file_contents.c_str(), max_brightness_path.c_str(), errno);
    return nullptr;
  }

  std::string scale_path(path.str());
  scale_path += "scale";
  if (!::android::base::ReadFileToString(scale_path, &file_contents)) {
    ALOGE("Could not read backlight scale from %s (%d)", scale_path.c_str(),
          errno);
    return nullptr;
  }
  bool log_scale = file_contents == "non-linear";

  ALOGI("Backlight %s (powered=%s max=%d scale=%slinear)\n", path.str().c_str(),
        powered ? "yes" : "no", max, log_scale ? "non-" : "");

  return std::unique_ptr<BacklightController>(
      new BacklightController(name, path.str(), powered, max, log_scale));
}

// The Framework does not report the slider position, but rather a non-linear
// value which approximates the HLG EOTF. This can cause issues with backlights
// which desire a linear scale and do the value->perceived brightness in
// firmware. This function converts the given non-linear (relative to the
// slider) brightness value back to the slider position.
//
// This function is modeled from:
//   com.android.settingslib.display.BrightnessUtils.convertLinearToGamma
auto BacklightController::BrightnessToSliderPosition(float linear) -> float {
  static const float kR = 0.5f;
  static const float kA = 0.17883277f;
  static const float kB = 0.28466892f;
  static const float kC = 0.55991073f;

  float hlg = linear * 12;
  float ret;
  if (hlg <= 1.0f) {
    ret = sqrt(hlg) * kR;
  } else {
    ret = kA * log(hlg - kB) + kC;
  }

  return ret;
}

auto BacklightController::SetBrightness(std::optional<float> brightness)
    -> bool {
  std::string power_path(sysfs_path_);
  power_path += "bl_power";

  if (brightness == std::nullopt) {
    if (powered_) {
      powered_ = false;
      if (!::android::base::WriteStringToFile(std::to_string(
                                                  FB_BLANK_POWERDOWN),
                                              power_path)) {
        ALOGE("Failed to turn backlight off at %s (%d)", power_path.c_str(),
              errno);
        return false;
      }
    }
    return true;
  }

  if (!powered_) {
    if (!::android::base::WriteStringToFile(std::to_string(FB_BLANK_UNBLANK),
                                            power_path)) {
      ALOGE("Failed to turn backlight on at %s (%d)", power_path.c_str(),
            errno);
      return false;
    }
    powered_ = true;
  }

  float val = log_scale_ ? *brightness
                         : BrightnessToSliderPosition(*brightness);

  int normalized = static_cast<int>(max_ * val);
  std::string brightness_path(sysfs_path_);
  brightness_path += "brightness";
  if (!::android::base::WriteStringToFile(std::to_string(normalized),
                                          brightness_path)) {
    ALOGE("Failed to set brightness to %d at %s (%d)", normalized,
          brightness_path.c_str(), errno);
    return false;
  }
  return true;
}

}  // namespace android
