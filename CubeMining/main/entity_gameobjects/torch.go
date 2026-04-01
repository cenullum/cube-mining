components {
  id: "fog_entity"
  component: "/main/scripts/fog_entity.script"
}
embedded_components {
  id: "model"
  type: "model"
  data: "mesh: \"/assets/model/torch.glb\"\n"
  "name: \"unnamed\"\n"
  "materials {\n"
  "  name: \"default\"\n"
  "  material: \"/assets/materials/entity.material\"\n"
  "  textures {\n"
  "    sampler: \"tex0\"\n"
  "    texture: \"/assets/images/entity/torch.png\"\n"
  "  }\n"
  "}\n"
  ""
  position {
    y: -0.5
  }
}
