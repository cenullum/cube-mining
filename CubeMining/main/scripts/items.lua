local hash = hash

local M = {}

M.definitions = {
    [1] = {
        name = "pickaxe",
        type = "voxel",
        power = 1,
        id = hash("pickaxe")
    },
    [2] = {
        name = "torch",
        type = "model",
        power = 1,
        id = hash("torch")
    },
    [3] = {
        name = "bomb",
        type = "model",
        power = 1,
        id = hash("bomb")
    },
    [4] = {
        name = "gun",
        type = "voxel",
        power = 1,
        id = hash("gun")
    },
    [5] = {
        name = "diamond_sword",
        type = "voxel",
        power = 2,
        id = hash("diamond_sword")
    }
}

return M
