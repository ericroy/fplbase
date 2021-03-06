// Copyright 2014 Google Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "precompiled.h"

#include <utility>

#include "fplbase/environment.h"
#include "fplbase/flatbuffer_utils.h"
#include "fplbase/internal/type_conversions_gl.h"
#include "fplbase/mesh.h"
#include "fplbase/renderer.h"
#include "fplbase/utilities.h"

#include "mesh_generated.h"

using mathfu::mat4;
using mathfu::vec2;
using mathfu::vec2i;
using mathfu::vec3;
using mathfu::vec4;
using mathfu::vec4i;

namespace fplbase {

struct MeshImpl {
  MeshImpl() : vbo(InvalidBufferHandle()), vao(InvalidBufferHandle()) {}

  BufferHandle vbo;
  BufferHandle vao;
};

// Even though these functions are identical in each implementation, the
// definition of MeshImpl is different, so these functions cannot be in
// mesh_common.cpp.
MeshImpl *Mesh::CreateMeshImpl() { return new MeshImpl; }
void Mesh::DestroyMeshImpl(MeshImpl *impl) { delete impl; }

uint32_t Mesh::GetPrimitiveTypeFlags(Mesh::Primitive primitive) {
  switch (primitive) {
    case Mesh::kLines:
      return GL_LINES;
    case Mesh::kPoints:
      return GL_POINTS;
    case Mesh::kTriangleStrip:
      return GL_TRIANGLE_STRIP;
    case Mesh::kTriangleFan:
      return GL_TRIANGLE_FAN;
    default:
      return GL_TRIANGLES;
  }
}

namespace {

void SetAttributes(GLuint vbo, const Attribute *attributes, int stride,
                   const char *buffer) {
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  size_t offset = 0;
  for (;;) {
    switch (*attributes++) {
      case kPosition3f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributePosition));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributePosition, 3, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 3 * sizeof(float);
        break;
      case kNormal3f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeNormal));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeNormal, 3, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 3 * sizeof(float);
        break;
      case kTangent4f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTangent));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTangent, 4, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 4 * sizeof(float);
        break;
      case kTexCoord2f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTexCoord));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTexCoord, 2, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 2 * sizeof(float);
        break;
      case kTexCoordAlt2f:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeTexCoordAlt));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeTexCoordAlt, 2, GL_FLOAT,
                                      false, stride, buffer + offset));
        offset += 2 * sizeof(float);
        break;
      case kColor4ub:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeColor));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeColor, 4,
                                      GL_UNSIGNED_BYTE, true, stride,
                                      buffer + offset));
        offset += 4;
        break;
      case kBoneIndices4ub:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeBoneIndices));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeBoneIndices, 4,
                                      GL_UNSIGNED_BYTE, false, stride,
                                      buffer + offset));
        offset += 4;
        break;
      case kBoneWeights4ub:
        GL_CALL(glEnableVertexAttribArray(Mesh::kAttributeBoneWeights));
        GL_CALL(glVertexAttribPointer(Mesh::kAttributeBoneWeights, 4,
                                      GL_UNSIGNED_BYTE, true, stride,
                                      buffer + offset));
        offset += 4;
        break;

      case kEND:
        return;
    }
  }
}

void UnSetAttributes(const Attribute *attributes) {
  for (;;) {
    switch (*attributes++) {
      case kPosition3f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributePosition));
        break;
      case kNormal3f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeNormal));
        break;
      case kTangent4f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeTangent));
        break;
      case kTexCoord2f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeTexCoord));
        break;
      case kTexCoordAlt2f:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeTexCoordAlt));
        break;
      case kColor4ub:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeColor));
        break;
      case kBoneIndices4ub:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeBoneIndices));
        break;
      case kBoneWeights4ub:
        GL_CALL(glDisableVertexAttribArray(Mesh::kAttributeBoneWeights));
        break;
      case kEND:
        return;
    }
  }
}

void BindAttributes(BufferHandle vao, BufferHandle vbo,
                    const Attribute *attributes, size_t vertex_size) {
  if (ValidBufferHandle(vao)) {
    glBindVertexArray(GlBufferHandle(vao));
  } else {
    SetAttributes(GlBufferHandle(vbo), attributes,
                  static_cast<int>(vertex_size), nullptr);
  }
}

void UnbindAttributes(BufferHandle vao, const Attribute *attributes) {
  if (ValidBufferHandle(vao)) {
    glBindVertexArray(0);  // TODO(wvo): could probably omit this?
  } else {
    UnSetAttributes(attributes);
  }
}

void DrawElement(Renderer &renderer, int32_t count, int32_t instances,
                 uint32_t index_type, GLenum gl_primitive) {
  if (instances == 1) {
    GL_CALL(glDrawElements(gl_primitive, count, index_type, 0));
  } else {
    (void)renderer;
    assert(renderer.feature_level() == kFeatureLevel30);
    GL_CALL(
        glDrawElementsInstanced(gl_primitive, count, index_type, 0, instances));
  }
}

}  // namespace

bool Mesh::IsValid() { return ValidBufferHandle(impl_->vbo); }

void Mesh::ClearPlatformDependent() {
  if (ValidBufferHandle(impl_->vbo)) {
    auto vbo = GlBufferHandle(impl_->vbo);
    GL_CALL(glDeleteBuffers(1, &vbo));
    impl_->vbo = InvalidBufferHandle();
  }
  if (ValidBufferHandle(impl_->vao)) {
    auto vao = GlBufferHandle(impl_->vao);
    GL_CALL(glDeleteVertexArrays(1, &vao));
    impl_->vao = InvalidBufferHandle();
  }
  for (auto it = indices_.begin(); it != indices_.end(); ++it) {
    auto ibo = GlBufferHandle(it->ibo);
    GL_CALL(glDeleteBuffers(1, &ibo));
  }
}

void Mesh::LoadFromMemory(const void *vertex_data, size_t count,
                          size_t vertex_size, const Attribute *format,
                          vec3 *max_position, vec3 *min_position) {
  assert(count > 0);
  vertex_size_ = vertex_size;
  num_vertices_ = count;
  default_bone_transform_inverses_ = nullptr;

  set_format(format);
  GLuint vbo = 0;
  GL_CALL(glGenBuffers(1, &vbo));
  impl_->vbo = BufferHandleFromGl(vbo);
  GL_CALL(glBindBuffer(GL_ARRAY_BUFFER, vbo));
  GL_CALL(glBufferData(GL_ARRAY_BUFFER, count * vertex_size, vertex_data,
                       GL_STATIC_DRAW));

  if (RendererBase::Get()->feature_level() >= kFeatureLevel30) {
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    impl_->vao = BufferHandleFromGl(vao);
    glBindVertexArray(vao);
    SetAttributes(vbo, format_, static_cast<int>(vertex_size_), nullptr);
    glBindVertexArray(0);
  }

  // Determine the min and max position
  if (max_position && min_position) {
    max_position_ = *max_position;
    min_position_ = *min_position;
  } else {
    auto data = static_cast<const float *>(vertex_data);
    const Attribute *attribute = format;
    data += VertexSize(attribute, kPosition3f) / sizeof(float);
    const size_t step = vertex_size / sizeof(float);
    min_position_ = vec3(data);
    max_position_ = min_position_;
    for (size_t vertex = 1; vertex < count; vertex++) {
      data += step;
      min_position_ = vec3::Min(min_position_, vec3(data));
      max_position_ = vec3::Max(max_position_, vec3(data));
    }
  }
}

void Mesh::AddIndices(const void *index_data, int count, Material *mat,
                      bool is_32_bit) {
  indices_.push_back(Indices());
  auto &idxs = indices_.back();
  idxs.count = count;
  GLuint ibo = 0;
  GL_CALL(glGenBuffers(1, &ibo));
  idxs.ibo = BufferHandleFromGl(ibo);
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo));
  GL_CALL(
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                   count * (is_32_bit ? sizeof(uint32_t) : sizeof(uint16_t)),
                   index_data, GL_STATIC_DRAW));
  idxs.index_type = (is_32_bit ? GL_UNSIGNED_INT : GL_UNSIGNED_SHORT);
  idxs.mat = mat;
}

void Mesh::Render(Renderer &renderer, bool ignore_material, size_t instances) {
  BindAttributes(impl_->vao, impl_->vbo, format_, vertex_size_);
  if (!indices_.empty()) {
    for (auto it = indices_.begin(); it != indices_.end(); ++it) {
      if (!ignore_material) it->mat->Set(renderer);
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GlBufferHandle(it->ibo)));
      DrawElement(renderer, it->count, static_cast<int32_t>(instances),
                  it->index_type, primitive_);
    }
  } else {
    glDrawArrays(primitive_, 0, static_cast<int32_t>(num_vertices_));
  }
  UnbindAttributes(impl_->vao, format_);
}

void Mesh::RenderStereo(Renderer &renderer, const Shader *shader,
                        const Viewport *viewport, const mat4 *mvp,
                        const vec3 *camera_position, bool ignore_material,
                        size_t instances) {
  BindAttributes(impl_->vao, impl_->vbo, format_, vertex_size_);
  auto prep_stereo = [&](size_t i) {
        renderer.set_camera_pos(camera_position[i]);
        renderer.set_model_view_projection(mvp[i]);
        renderer.SetViewport(viewport[i]);
        shader->Set(renderer);
  };
  if (!indices_.empty()) {
    for (auto it = indices_.begin(); it != indices_.end(); ++it) {
      if (!ignore_material) it->mat->Set(renderer);
      GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, GlBufferHandle(it->ibo)));
      for (size_t i = 0; i < 2; ++i) {
        prep_stereo(i);
        DrawElement(renderer, it->count, static_cast<int32_t>(instances),
                    it->index_type, primitive_);
      }
    }
  } else {
    for (size_t i = 0; i < 2; ++i) {
      prep_stereo(i);
      glDrawArrays(primitive_, 0, static_cast<int32_t>(num_vertices_));
    }
  }
  UnbindAttributes(impl_->vao, format_);
}

void Mesh::RenderArray(Primitive primitive, int index_count,
                       const Attribute *format, int vertex_size,
                       const void *vertices, const unsigned short *indices) {
  SetAttributes(0, format, vertex_size,
                reinterpret_cast<const char *>(vertices));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  auto gl_primitive = GetPrimitiveTypeFlags(primitive);
  GL_CALL(
      glDrawElements(gl_primitive, index_count, GL_UNSIGNED_SHORT, indices));
  UnSetAttributes(format);
}

void Mesh::RenderArray(Primitive primitive, int vertex_count,
                       const Attribute *format, int vertex_size,
                       const void *vertices) {
  SetAttributes(0, format, vertex_size,
                reinterpret_cast<const char *>(vertices));
  GL_CALL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));
  auto gl_primitive = GetPrimitiveTypeFlags(primitive);
  GL_CALL(glDrawArrays(gl_primitive, 0, vertex_count));
  UnSetAttributes(format);
}

}  // namespace fplbase
