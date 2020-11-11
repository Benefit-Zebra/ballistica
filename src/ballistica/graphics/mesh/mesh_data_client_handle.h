// Released under the MIT License. See LICENSE for details.

#ifndef BALLISTICA_GRAPHICS_MESH_MESH_DATA_CLIENT_HANDLE_H_
#define BALLISTICA_GRAPHICS_MESH_MESH_DATA_CLIENT_HANDLE_H_

#include "ballistica/core/object.h"

namespace ballistica {

// Client-side (game-thread) handle to server-side (graphics-thread) mesh data.
// Server-side data will be created when this object is instantiated and
// cleared when this object goes down.
class MeshDataClientHandle : public Object {
 public:
  explicit MeshDataClientHandle(MeshData* d);
  ~MeshDataClientHandle() override;
  MeshData* mesh_data;
};

}  // namespace ballistica

#endif  // BALLISTICA_GRAPHICS_MESH_MESH_DATA_CLIENT_HANDLE_H_
