components {
  id: "camera_script"
  component: "/main/scripts/character_controller.script"
  properties {
    id: "mouse_factory"
    value: "/camera#mouse_factory"
    type: PROPERTY_TYPE_URL
  }
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
  id: "camera"
  type: "camera"
  data: "aspect_ratio: 1.0\n"
  "fov: 45.0\n"
  "near_z: 0.1\n"
  "far_z: 1000.0\n"
  "auto_aspect_ratio: 1\n"
  ""
}
embedded_components {
  id: "voxel_mesh_factory"
  type: "factory"
  data: "prototype: \"/main/voxel_mesh.go\"\n"
  ""
}
embedded_components {
  id: "mouse_factory"
  type: "factory"
  data: "prototype: \"/main/entity/mouse.go\"\n"
  ""
}
