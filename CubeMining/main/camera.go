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
