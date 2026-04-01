components {
  id: "script"
  component: "/main/scripts/bomb.script"
}
components {
  id: "fog_entity"
  component: "/main/scripts/fog_entity.script"
}
embedded_components {
  id: "model"
  type: "model"
  data: "mesh: \"/assets/model/bomb.glb\"\n"
  "name: \"unnamed\"\n"
  "materials {\n"
  "  name: \"default\"\n"
  "  material: \"/assets/materials/entity.material\"\n"
  "  textures {\n"
  "    sampler: \"tex0\"\n"
  "    texture: \"/assets/images/entity/bomb.png\"\n"
  "  }\n"
  "}\n"
  ""
}
