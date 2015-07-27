# Copyright 2015 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Locations of 3rd party and FPL libraries.
FPL_ROOT:=$(FPLBASE_DIR)/../../libs
# If the dependencies directory exists either as a subdirectory or as the
# container of this project directory, assume the dependencies directory is
# the root directory for all libraries required by this project.
$(foreach dep_dir,$(wildcard $(FPLBASE_DIR)/dependencies) \
                  $(wildcard $(FPLBASE_DIR)/../../dependencies),\
  $(eval DEPENDENCIES_ROOT?=$(dep_dir)))
ifneq ($(DEPENDENCIES_ROOT),)
  THIRD_PARTY_ROOT:=$(DEPENDENCIES_ROOT)
  FPL_ROOT:=$(DEPENDENCIES_ROOT)
else
  THIRD_PARTY_ROOT:=$(FPL_ROOT)/../../../external
endif

FPLBASE_GENERATED_OUTPUT_DIR := $(FPLBASE_DIR)/gen/include
PREBUILTS_ROOT:=$(FPLBASE_DIR)/../../../../prebuilts


# Location of the Flatbuffers library.
DEPENDENCIES_FLATBUFFERS_DIR?=$(FPL_ROOT)/flatbuffers
# Location of the googletest library.
DEPENDENCIES_GTEST_DIR?=$(FPL_ROOT)/googletest
# Location of the MathFu library.
DEPENDENCIES_MATHFU_DIR?=$(FPL_ROOT)/mathfu
# Location of the webp library.
DEPENDENCIES_WEBP_DIR?=$(THIRD_PARTY_ROOT)/webp
# Location of the Google Play Games library.
DEPENDENCIES_GPG_DIR?=$(PREBUILTS_ROOT)/gpg-cpp-sdk/android
# Location of SDL
DEPENDENCIES_SDL_DIR?=$(THIRD_PARTY_ROOT)/sdl