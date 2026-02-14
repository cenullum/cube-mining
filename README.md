![545237458-10566272-4e3d-46bf-8141-1be5c9d70c88](https://github.com/user-attachments/assets/dc76a10c-c135-40af-b671-8bd5601a37ef)


# Cube Mining

A 3D voxel mining example built with the **Defold Game Engine**. This project demonstrates how to handle dynamic voxel environments, efficient mesh generation, and real-time performance monitoring.

## Play the demo here: https://cenullum.itch.io/cubemining

[aaaa.webm](https://github.com/user-attachments/assets/75f88298-c303-4e95-97e7-847ef5146cf9)

![aa](https://github.com/user-attachments/assets/e0bb405c-9b9c-4741-8166-3dc3645603db)

## Features

- **Greedy Meshing Algorithm**: Optimizes rendering by merging adjacent faces into larger quads, significantly reducing vertex count and draw calls.
- **Swept AABB Collision System**: High-performance collision detection and resolution with sub-stepping to prevent tunneling through blocks.
- **Runtime Sprite-to-Mesh Voxelization**: Dynamically converts 2D pixel art sprites (like the held gun or sword) into 3D voxel meshes at runtime, giving them real depth and volume.
- **Golden Ore & Unbreakable Layers**: New block types including rare golden ore and indestructible bedrock at the bottom of the world.
- **Dynamic Voxel Interaction**: Real-time destroying and placing of blocks with immediate mesh updates.
- **Performance Monitoring**: Integrated overlay providing real-time FPS, 1% low frames, RAM usage, mesh statistics, and **live player position tracking**.
- **Custom Shader-based UV Mapping**: Precise texture tiling and atlas-based UV wrapping for voxel surfaces.

## Controls

- **WASD**: Move around
- **Shift**: Sprint (Move faster)
- **Space**: Ascend (Jump in Walk mode)
- **Ctrl**: Descend
- **Left Click**: Break block / Attack animation
- **Right Click**: Place block
- **Q**: Cycle held items (Gun, Pickaxe, Sword, etc.)
- **M**: Toggle Performance Overlay (Off / Text / Text + Graph)
- **P**: Toggle Noclip / Free Cam mode
- **T**: Spawn mouse 
at targeted block
- **Y**: Spawn mice rapidly while held

## Scope & Technical Notes

- **Scale**: The project is currently focused on a single **16x16x16** voxel cube. It does not implement a chunking system or procedural terrain generation like Minecraft.
- **Threading**: Mesh generation and GPU buffer updates are currently handled on the **main thread**. So it fps frops until it create whole mesh.
- **Optimization**: To maximize performance, surfaces of cubes that are not visible (i.e., touching another solid block) are excluded from the mesh. As you can see in the image, faces are only added when "holes" are created, ensuring efficient rendering.
- **Item Voxelization Architecture**: Unlike the world mesh which uses a greedy culling approach, the `voxelizer` script analyzes sprite transparency to extrude edges. It generates a high-detail mesh with front/back quads and individual side faces for every solid pixel, creating a true 3D effect from a 2D source.


![544664750-ce13d4ab-a806-4f67-bc29-c13766086044](https://github.com/user-attachments/assets/42c935b8-5b09-4c1f-b4b4-25eeee3ebdcf)
![544664770-949a8c8d-a483-44a7-bafe-b49c6f37decd](https://github.com/user-attachments/assets/b4c08286-4e98-4f9e-9cc4-fc57b469ec31)



## Inspiration & Acknowledgement

This project was inspired by the [Meshes-In-Defold](https://github.com/mozok/Meshes-In-Defold) repository. While it served as a starting point, most of the original code has been substantially rewritten to implement greedy meshing, optimize performance.

## Key Scripts

- **[generate_voxel.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/generate_voxel.script)**: The core engine for the voxel world. It manages the grid state, implements the greedy meshing algorithm, and handles GPU buffer updates.
- **[voxelizer.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/voxelizer.script)**: An optimized runtime system that converts 2D sprite data into 3D voxelized meshes. It handles atlas metadata caching and direct buffer streaming for high-performance mesh generation.
- **[physics.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/physics.lua)**: Shared physics module implementing AABB collision detection and "move and slide" resolution with sub-stepped checks to prevent tunneling.
- **[character_controller.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/character_controller.script)**: Manages player movement, input handling, and camera logic, supporting both walking and free-cam modes.
- **[item_sway.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/item_sway.script)**: Manages smooth procedural animations for held items, including mouse-based swaying and click-triggered attack movements.
- **[performance_overlay.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/performance_overlay.script)**: Provides a visual debugger, including FPS, RAM usage, and camera position/rotation.
- **[cube_cursor.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/cube_cursor.script)**: Manages 3D cursor interaction for targeting and modifying the voxel world.
- **[world.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/world.lua)**: Centralized world data management, providing shared access to the voxel grid and dimensions.
- **[block_data.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/block_data.lua)**: Extensible system for defining block properties, transparency, and UV metadata.
- **[npc.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/npc.lua)**: Base module for NPC logic and AI movement within the voxel world.

## Contributing

Contributions are welcome! Whether it's optimization, new features, or bug fixes, feel free to fork the repository and submit a pull request. 

## Support the Project

If you find this project helpful and would like to support its development, you can make a donation here:
[https://cenullum.com/donation](https://cenullum.com/donation)



## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
