components {
  id: "cube_cursor"
  component: "/assets/scripts/cube_cursor.script"
}
embedded_components {
  id: "mesh"
  type: "mesh"
  data: "material: \"/assets/materials/cursor.material\"\n"
  "vertices: \"/empty.buffer\"\n"
  "position_stream: \"position\"\n"
  "normal_stream: \"position\"\n"
  ""
}
