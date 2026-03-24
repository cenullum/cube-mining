embedded_components {
  id: "model"
  type: "model"
  data: "mesh: \"/builtins/assets/meshes/quad.dae\"\n"
  "name: \"{{NAME}}\"\n"
  "materials {\n"
  "  name: \"default\"\n"
  "  material: \"/assets/materials/particle_instanced.material\"\n"
  "}\n"
  ""
}
components {
  id: "script"
  component: "/main/scripts/particle.script"
}
