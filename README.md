<img width="2476" height="744" alt="4" src="https://github.com/user-attachments/assets/10566272-4e3d-46bf-8141-1be5c9d70c88" />

# Cube Mining

A 3D voxel mining example built with the **Defold Game Engine**. This project demonstrates how to handle dynamic voxel environments, efficient mesh generation, and real-time performance monitoring.

# Play the demo here: https://cenullum.itch.io/cubemining

[aaaa.webm](https://github.com/user-attachments/assets/75f88298-c303-4e95-97e7-847ef5146cf9)



## Features

- **Greedy Meshing Algorithm**: Optimizes rendering by merging adjacent faces into larger quads, significantly reducing vertex count and draw calls.
- **Runtime Sprite-to-Mesh Voxelization**: Dynamically converts 2D pixel art sprites (like the held gun or sword) into 3D voxel meshes at runtime, giving them real depth and volume.
- **Dynamic Voxel Interaction**: Real-time destroying and placing of blocks with immediate mesh updates.
- **Performance Monitoring**: Integrated overlay providing real-time FPS, 1% low frames, RAM usage, and mesh statistics.
- **Custom Shader-based UV Mapping**: Precise texture tiling and atlas-based UV wrapping for voxel surfaces.

## Controls

- **WASD**: Move around
- **Shift**: Sprint (Move faster)
- **Space**: Ascend
- **Ctrl**: Descend
- **Left Click**: Break block / Attack animation
- **Right Click**: Place block
- **Q**: Cycle held items (Gun, Pickaxe, Sword, etc.)
- **M**: Toggle Performance Overlay (Off / Text / Text + Graph)

## Scope & Technical Notes

- **Scale**: The project is currently focused on a single **16x16x16** voxel cube. It does not implement a chunking system or procedural terrain generation like Minecraft.
- **Threading**: Mesh generation and GPU buffer updates are currently handled on the **main thread**. So it fps frops until it create whole mesh.
- **Optimization**: To maximize performance, surfaces of cubes that are not visible (i.e., touching another solid block) are excluded from the mesh. As you can see in the image, faces are only added when "holes" are created, ensuring efficient rendering.
- **Item Voxelization Architecture**: Unlike the world mesh which uses a greedy culling approach, the `voxelizer` script analyzes sprite transparency to extrude edges. It generates a high-detail mesh with front/back quads and individual side faces for every solid pixel, creating a true 3D effect from a 2D source.


<img width="963" height="681" alt="Screenshot from 2026-02-04 03-09-35" src="https://github.com/user-attachments/assets/949a8c8d-a483-44a7-bafe-b49c6f37decd" />
<img width="965" height="678" alt="Screenshot from 2026-02-04 03-09-04" src="https://github.com/user-attachments/assets/ce13d4ab-a806-4f67-bc29-c13766086044" />



## Inspiration & Acknowledgement

This project was inspired by the [Meshes-In-Defold](https://github.com/mozok/Meshes-In-Defold) repository. While it served as a starting point, most of the original code has been substantially rewritten to implement greedy meshing, optimize performance.

## Key Scripts

- **[generate_voxel.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/generate_voxel.script)**: The core engine for the voxel world. It manages the grid state, implements the greedy meshing algorithm, and handles GPU buffer updates.
- **[voxelizer.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/voxelizer.script)**: An optimized runtime system that converts 2D sprite data into 3D voxelized meshes. It handles atlas metadata caching and direct buffer streaming for high-performance mesh generation.
- **[item_sway.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/item_sway.script)**: Manages smooth procedural animations for held items, including mouse-based swaying and click-triggered attack movements.
- **[performance_overlay.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/performance_overlay.script)**: Provides a visual debugger and control instructions. Features a toggleable graph for performance analysis.
- **[cube_cursor.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/cube_cursor.script)**: Manages 3D cursor interaction, allowing users to accurately target and modify the voxel world.

## Contributing

Contributions are welcome! Whether it's optimization, new features, or bug fixes, feel free to fork the repository and submit a pull request. 

## Support the Project

If you find this project helpful and would like to support its development, you can make a donation here:
[https://cenullum.com/donation](https://cenullum.com/donation)



## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
