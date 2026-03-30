components {
  id: "camera_script"
  component: "/assets/scripts/character_controller.script"
  properties {
    id: "mouse_factory"
    value: "/camera#mouse_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "bomb_factory"
    value: "/camera#bomb_factory"
    type: PROPERTY_TYPE_URL
  }
}
components {
  id: "hand_slot_manager"
  component: "/assets/scripts/hand_slot_manager.script"
  properties {
    id: "bomb_factory"
    value: "/camera#bomb_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "mouse_factory"
    value: "/camera#mouse_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "pistol_shot_factory"
    value: "/camera#pistol_shot_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "submachine_gun_shot_factory"
    value: "/camera#smg_shot_factory"
    type: PROPERTY_TYPE_URL
  }
}
components {
  id: "voxelizer"
  component: "/assets/scripts/item_mesh_gen.script"
  properties {
    id: "mesh_prefab"
    value: "/camera#voxel_mesh_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "torch_prefab"
    value: "/camera#torch_hand_factory"
    type: PROPERTY_TYPE_URL
  }
  properties {
    id: "bomb_prefab"
    value: "/camera#bomb_factory"
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
  data: "prototype: \"/assets/gameobjects/voxel_mesh.go\"\n"
  ""
}
embedded_components {
  id: "mouse_factory"
  type: "factory"
  data: "prototype: \"/assets/entity_gameobjects/mouse.go\"\n"
  ""
}
embedded_components {
  id: "bomb_factory"
  type: "factory"
  data: "prototype: \"/assets/entity_gameobjects/bomb.go\"\n"
  ""
}
embedded_components {
  id: "torch_hand_factory"
  type: "factory"
  data: "prototype: \"/assets/entity_gameobjects/torch.go\"\n"
  ""
}
embedded_components {
  id: "pistol_shot_factory"
  type: "factory"
  data: "prototype: \"/assets/gameobjects/pistol_shot_emitter.go\"\n"
  ""
}
embedded_components {
  id: "smg_shot_factory"
  type: "factory"
  data: "prototype: \"/assets/gameobjects/smg_shot_emitter.go\"\n"
  ""
}
