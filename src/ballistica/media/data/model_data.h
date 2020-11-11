// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_MEDIA_DATA_MODEL_DATA_H_
#define BALLISTICA_MEDIA_DATA_MODEL_DATA_H_

#include <string>
#include <vector>

#include "ballistica/media/data/media_component_data.h"
#include "ballistica/media/data/model_renderer_data.h"

namespace ballistica {

class ModelData : public MediaComponentData {
 public:
  ModelData() = default;
  explicit ModelData(const std::string& file_name_in);
  void DoPreload() override;
  void DoLoad() override;
  void DoUnload() override;
  auto GetMediaType() const -> MediaType override { return MediaType::kModel; }
  auto GetName() const -> std::string override {
    if (!file_name_full_.empty()) {
      return file_name_full_;
    } else {
      return "invalid Model";
    }
  }
  auto renderer_data() const -> ModelRendererData* {
    assert(renderer_data_.exists());
    return renderer_data_.get();
  }
  auto vertices() const -> const std::vector<VertexObjectFull>& {
    return vertices_;
  }
  auto indices8() const -> const std::vector<uint8_t>& { return indices8_; }
  auto indices16() const -> const std::vector<uint16_t>& { return indices16_; }
  auto indices32() const -> const std::vector<uint32_t>& { return indices32_; }
  auto GetIndexSize() const -> int {
    switch (format_) {
      case MeshFormat::kUV16N8Index8:
        return 1;
      case MeshFormat::kUV16N8Index16:
        return 2;
      case MeshFormat::kUV16N8Index32:
        return 4;
      default:
        throw Exception();
    }
  }

 private:
  Object::Ref<ModelRendererData> renderer_data_;
  std::string file_name_;
  std::string file_name_full_;
  MeshFormat format_{};
  std::vector<VertexObjectFull> vertices_;
  std::vector<uint8_t> indices8_;
  std::vector<uint16_t> indices16_;
  std::vector<uint32_t> indices32_;
  friend class ModelRendererData;
  BA_DISALLOW_CLASS_COPIES(ModelData);
};

}  // namespace ballistica

#endif  // BALLISTICA_MEDIA_DATA_MODEL_DATA_H_
