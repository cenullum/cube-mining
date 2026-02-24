![545237458-10566272-4e3d-46bf-8141-1be5c9d70c88](https://github.com/user-attachments/assets/dc76a10c-c135-40af-b671-8bd5601a37ef)


# Cube Mining

A 3D voxel mining example built with the **Defold Game Engine**. This project includes mesh generation, collision detection, and performance monitoring.

## Play the demo here: https://cenullum.itch.io/cubemining

![recording_20260214_014218](https://github.com/user-attachments/assets/577a2205-77c9-462d-b8ac-08a0a5fded29)

![gggg](https://github.com/user-attachments/assets/4377dab7-8b8c-4126-b5de-5ebf12c138ed)



## Features

- **Greedy Meshing Algorithm**: Reduces vertex count and draw calls by merging adjacent faces into quads.
- **Swept AABB Collision System**: Collision detection and resolution with sub-stepping to prevent tunneling.
- **Runtime Sprite-to-Mesh Voxelization**: Converts 2D pixel art sprites into 3D voxel meshes at runtime.
- **Vertex-based Ambient Occlusion**: Calculated per-vertex to darken corners and edges. Toggleable with 'O'.
- **Distance-based Fog**: Distance-based fog applied to terrain and voxelized items.
- **Dynamic Item System**: A variety of holdable items including blocks, tools (pickaxes, swords), and guns (pistol, submachine gun with ammo/reload mechanics).
- **Block Health & Mining Power**: Blocks have varying durability, and tools have distinct mining/damage power.
- **Sound System**: Spatial and triggered sound effects for interactions, placing, breaking, and weapon mechanics.
- **Throwables**: Physics-based items like throwable bombs with screen shake effects.
- **Voxel Interaction**: Block destruction and placement with mesh updates.
- **Performance Monitoring**: Overlay showing FPS, 1% low frames, RAM usage, mesh statistics, and player position.
- **Shader-based UV Mapping**: Texture tiling and atlas-based UV wrapping for voxel surfaces.

## Controls

- **WASD**: Move around
- **Shift**: Sprint (Move faster)
- **Space**: Jump / Ascend
- **Ctrl**: Descend
- **Left Click**: Break / Shoot
- **Right Click**: Place / ADS (Aim Down Sights)
- **1 / 2**: Next / Previous active item
- **R**: Reload weapon
- **M**: Toggle Performance Overlay (Off / Text / Graph)
- **O**: Toggle Ambient Occlusion
- **P**: Toggle Fly / Walk mode
- **T**: Spawn mouse at targeted block
- **Y**: Spawn mice rapidly (Hold)

## Scope & Technical Notes

- **Scale**: The project is focused on a single 16x16x16 voxel cube. It does not implement a chunking system or procedural terrain generation.
- **Threading**: Mesh generation and GPU buffer updates are handled on the main thread.
- **Culling**: Surfaces that are not visible (touching another solid block) are excluded from the mesh. Faces are added when "holes" are created.
- **Item Voxelization**: Analyzes sprite transparency to extrude edges. Generates mesh with front/back quads and side faces for solid pixels.

<img width="1920" height="1080" alt="Screenshot from 2026-02-19 14-57-07" src="https://github.com/user-attachments/assets/f7e17863-a4b6-4cc2-af10-5a087cc3be77" />

<img width="1920" height="1080" alt="Screenshot from 2026-02-19 14-57-52" src="https://github.com/user-attachments/assets/6416113f-ec0b-434f-849d-88889dc9e5ac" />



## Inspiration & Acknowledgement

This project was inspired by the [Meshes-In-Defold](https://github.com/mozok/Meshes-In-Defold) repository. While it served as a starting point, most of the original code has been substantially rewritten to implement greedy meshing, optimize performance.

## Key Scripts

- **[generate_voxel.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/generate_voxel.script)**: Manages the voxel grid state, implements greedy meshing, and handles GPU buffer updates.
- **[voxelizer.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/voxelizer.script)**: Converts 2D sprite data into 3D meshes at runtime.
- **[physics.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/physics.lua)**: Shared physics module implementing AABB collision detection and resolution.
- **[character_controller.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/character_controller.script)**: Manages player movement, input handling, and camera logic.
- **[hand_slot_manager.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/hand_slot_manager.script)**: Manages the player's active hand item, inventory, and dynamic switching.
- **[performance_overlay.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/performance_overlay.script)**: Provides a visual debugger for FPS, RAM usage, and camera state.
- **[cube_cursor.script](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/cube_cursor.script)**: Manages 3D cursor interaction for targeting and modifying the voxel world.
- **[sound_manager.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/sound_manager.lua)**: Optimizes and handles audio playback, throttling, and spatial sounds.
- **[items.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/items.lua)**: Defines available blocks, tools, and weapons with their properties (health, sound, damage).
- **[world.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/world.lua)**: Centralized world data management, providing shared access to the voxel grid and dimensions.
- **[block_data.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/block_data.lua)**: Extensible system for defining block properties, transparency, and UV metadata.
- **[npc.lua](file:///home/cenker/Documents/GitHub/cube-mining/CubeMining/main/scripts/npc.lua)**: Base module for NPC logic and AI movement within the voxel world.

## Credits & Assets

- **Textures & Models**: All textures and models are made by **cenullum** and are licensed under **CC0** (Public Domain).
- **Sound Effects** (Sourced from Pixabay):
  - [Stone Hit](https://pixabay.com/sound-effects/film-special-effects-hit-rock-03-266305/)
  - [Bomb / Rock Break](https://pixabay.com/sound-effects/film-special-effects-rock-break-hard-184891/)
  - [Stone Falling / Debris](https://pixabay.com/sound-effects/film-special-effects-stones-falling-6375/)
  - [Sword Swing](https://pixabay.com/sound-effects/film-special-effects-sword-air-swing-2-437695/)
  - [Gun Holster](https://pixabay.com/sound-effects/film-special-effects-holster-pistol-7132/)
  - [Submachine Gun Shot / Reload](https://pixabay.com/sound-effects/film-special-effects-mp5-168858/)
  - [Pistol Reload](https://pixabay.com/sound-effects/film-special-effects-9mm-pistol-load-and-chamber-98830/)
  - [Pistol Shot](https://pixabay.com/sound-effects/film-special-effects-usp-pistol-sfx-80490/)
  - [Put Cube](https://pixabay.com/sound-effects/film-special-effects-put-item-83043/)

## Contributing

Contributions are welcome! Whether it's optimization, new features, or bug fixes, feel free to fork the repository and submit a pull request. 

## Support the Project

If you find this project helpful and would like to support its development, you can make a donation here:
[https://cenullum.com/donation](https://cenullum.com/donation)



## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
