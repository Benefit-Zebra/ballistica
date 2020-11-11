// Released under the MIT License. See LICENSE for details.

#include "ballistica/graphics/mesh/mesh_data_client_handle.h"

#include "ballistica/graphics/graphics.h"

namespace ballistica {

MeshDataClientHandle::MeshDataClientHandle(MeshData* d) : mesh_data(d) {
  g_graphics->AddMeshDataCreate(mesh_data);
}

MeshDataClientHandle::~MeshDataClientHandle() {
  g_graphics->AddMeshDataDestroy(mesh_data);
}

}  // namespace ballistica
