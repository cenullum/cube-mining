components {
  id: "camera_script"
  component: "/main/scripts/free_camera.script"
}
embedded_components {
  id: "camera"
  type: "camera"
  data: "aspect_ratio: 1.0\n"
  "fov: 45.0\n"
  "near_z: 0.1\n"
  "far_z: 1000.0\n"
  "auto_aspect_ratio: 1\n"
  ""
}
components {
  id: "voxelizer"
  component: "/main/scripts/voxelizer.script"
  properties {
    id: "mesh_prefab"
    value: "/camera#voxel_mesh_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "game_atlas"
    value: "/assets/images/game.atlas"
    type: PROPERTY_TYPE_HASH
  }
}
embedded_components {
  id: "voxel_mesh_factory"
  type: "factory"
  data: "prototype: \"/main/voxel_mesh.go\"\n"
  ""
}
