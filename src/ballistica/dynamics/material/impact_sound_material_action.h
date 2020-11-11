// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_DYNAMICS_MATERIAL_IMPACT_SOUND_MATERIAL_ACTION_H_
#define BALLISTICA_DYNAMICS_MATERIAL_IMPACT_SOUND_MATERIAL_ACTION_H_

#include <vector>

#include "ballistica/ballistica.h"
#include "ballistica/dynamics/material/material_action.h"
#include "ballistica/media/component/sound.h"

namespace ballistica {

// A sound created based on collision forces parallel to the collision normal.
class ImpactSoundMaterialAction : public MaterialAction {
 public:
  ImpactSoundMaterialAction() = default;
  ImpactSoundMaterialAction(const std::vector<Sound*>& sounds_in,
                            float target_impulse_in, float volume_in)
      : sounds(PointersToRefs(sounds_in)),
        target_impulse_(target_impulse_in),
        volume_(volume_in) {}
  std::vector<Object::Ref<Sound> > sounds;
  void Apply(MaterialContext* context, const Part* src_part,
             const Part* dst_part,
             const Object::Ref<MaterialAction>& p) override;
  auto GetType() const -> Type override { return Type::IMPACT_SOUND; }
  auto GetFlattenedSize() -> size_t override;
  void Flatten(char** buffer, GameStream* output_stream) override;
  void Restore(const char** buffer, ClientSession* cs) override;

 private:
  float target_impulse_{};
  float volume_{};
};

}  // namespace ballistica

#endif  // BALLISTICA_DYNAMICS_MATERIAL_IMPACT_SOUND_MATERIAL_ACTION_H_
