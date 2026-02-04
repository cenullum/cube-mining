# Cube Mining

A 3D voxel mining example built with the **Defold Game Engine**. This project demonstrates how to handle dynamic voxel environments, efficient mesh generation, and real-time performance monitoring.

## Features

- **Greedy Meshing Algorithm**: Optimizes rendering by merging adjacent faces into larger quads, significantly reducing vertex count and draw calls.
- **Dynamic Voxel Interaction**: Real-time destroying and placing of blocks with immediate mesh updates.
- **Performance Monitoring**: Integrated overlay providing real-time FPS, 1% low frames, RAM usage, and mesh statistics.
- **Custom Shader-based UV Mapping**: Precise texture tiling and atlas-based UV wrapping for voxel surfaces.

## Controls

- **WASD**: Move around
- **Shift**: Sprint (Move faster)
- **Space**: Ascend
- **Ctrl**: Descend
- **Left Click**: Break block
- **Right Click**: Place block
- **F6**: Toggle Performance Overlay (Off / Text / Text + Graph)

## Scope & Technical Notes

- **Scale**: The project is currently focused on a single **16x16x16** voxel cube. It does not implement a chunking system or procedural terrain generation like Minecraft.
- **Threading**: Mesh generation and GPU buffer updates are currently handled on the **main thread**. So it fps frops until it create whole mesh.
- **Optimization**: To maximize performance, surfaces of cubes that are not visible (i.e., touching another solid block) are excluded from the mesh. As you can see in the image, faces are only added when "holes" are created, ensuring efficient rendering.

## Inspiration & Acknowledgement

This project was inspired by the [Meshes-In-Defold](https://github.com/mozok/Meshes-In-Defold) repository. While it served as a starting point, most of the original code has been substantially rewritten to implement greedy meshing, optimize performance.

## Key Scripts

- **[generate_voxel.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/generate_voxel.script)**: The core engine for the voxel world. It manages the grid state, implements the greedy meshing algorithm, and handles GPU buffer updates.
- **[performance_overlay.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/performance_overlay.script)**: Provides a visual debugger and control instructions. Features a toggleable graph for performance analysis.
- **[cube_cursor.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/cube_cursor.script)**: Manages 3D cursor interaction, allowing users to accurately target and modify the voxel world.

## Contributing

Contributions are welcome! Whether it's optimization, new features, or bug fixes, feel free to fork the repository and submit a pull request. 

## Support the Project

If you find this project helpful and would like to support its development, you can make a donation here:
[https://cenullum.com/donation](https://cenullum.com/donation)

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
