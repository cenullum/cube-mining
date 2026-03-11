#include "ve_world.h"
#include <math.h>

bool CheckCollision(float min_x, float min_y, float min_z, float max_x, float max_y, float max_z) {
    float offset = (float)g_grid_size / -2.0f + 0.5f;
    float origin_x = offset;
    float origin_y = offset;
    float origin_z = 490.0f;

    // Add small epsilon to avoid being stuck on boundaries
    int start_x = (int)floorf(min_x - origin_x + 0.501f);
    int end_x   = (int)floorf(max_x - origin_x + 0.499f);
    int start_y = (int)floorf(min_y - origin_y + 0.501f);
    int end_y   = (int)floorf(max_y - origin_y + 0.499f);
    int start_z = (int)floorf(min_z - origin_z + 0.501f);
    int end_z   = (int)floorf(max_z - origin_z + 0.499f);

    for (int x = start_x; x <= end_x; x++) {
        for (int y = start_y; y <= end_y; y++) {
            for (int z = start_z; z <= end_z; z++) {
                if (IsSolid(x, y, z)) return true;
            }
        }
    }
    return false;
}

void MoveAndSlide(dmVMath::Vector3& pos, dmVMath::Vector3& vel, const dmVMath::Vector3& size, float dt, bool& is_grounded) {
    is_grounded = false;
    float step_size = 0.4f;
    float total_dist = dmVMath::Length(vel * dt);
    int steps = (int)ceilf(total_dist / step_size);
    if (steps < 1) steps = 1;

    float dt_step = dt / (float)steps;
    float half_x = size.getX() * 0.5f;
    float half_z = size.getZ() * 0.5f;

    for (int s = 0; s < steps; s++) {
        float dx = vel.getX() * dt_step;
        if (dx != 0) {
            float next_x = pos.getX() + dx;
            // Check collision with a small vertical offset (0.1f) to avoid hitting the ground the NPC is standing on
            if (CheckCollision(next_x - half_x, pos.getY() + 0.1f, pos.getZ() - half_z,
                               next_x + half_x, pos.getY() + size.getY() - 0.1f, pos.getZ() + half_z)) {
                vel.setX(0);
            } else {
                pos.setX(next_x);
            }
        }

        float dz = vel.getZ() * dt_step;
        if (dz != 0) {
            float next_z = pos.getZ() + dz;
            if (CheckCollision(pos.getX() - half_x, pos.getY() + 0.1f, next_z - half_z,
                               pos.getX() + half_x, pos.getY() + size.getY() - 0.1f, next_z + half_z)) {
                vel.setZ(0);
            } else {
                pos.setZ(next_z);
            }
        }

        float dy = vel.getY() * dt_step;
        if (dy != 0) {
            float next_y = pos.getY() + dy;
            float check_min_y = next_y;
            float check_max_y = next_y + size.getY();
            if (dy > 0) check_min_y += 0.01f; // Skip floor slightly when jumping
            else if (dy < 0) check_max_y -= 0.01f; // Skip ceiling slightly when falling

            // Check collision with a small horizontal buffer (0.1f) to avoid snagging on walls while jumping/falling
            if (CheckCollision(pos.getX() - half_x + 0.1f, check_min_y, pos.getZ() - half_z + 0.1f,
                               pos.getX() + half_x - 0.1f, check_max_y, pos.getZ() + half_z - 0.1f)) {
                if (vel.getY() < 0) is_grounded = true;
                vel.setY(0);
            } else {
                pos.setY(next_y);
            }
        }
    }

    if (!is_grounded && vel.getY() <= 0) {
        float check_dist = 0.05f;
        if (CheckCollision(pos.getX() - half_x + 0.1f, pos.getY() - check_dist, pos.getZ() - half_z + 0.1f,
                          pos.getX() + half_x - 0.1f, pos.getY(), pos.getZ() + half_z - 0.1f)) {
            is_grounded = true;
            vel.setY(0);
        }
    }
}

bool RayAABBIntersection(const dmVMath::Vector3& ray_origin, const dmVMath::Vector3& ray_dir, 
                               const dmVMath::Vector3& box_min, const dmVMath::Vector3& box_max, float& t_out) {
    float t1 = (box_min.getX() - ray_origin.getX()) / (ray_dir.getX() + 1e-6f);
    float t2 = (box_max.getX() - ray_origin.getX()) / (ray_dir.getX() + 1e-6f);
    float t3 = (box_min.getY() - ray_origin.getY()) / (ray_dir.getY() + 1e-6f);
    float t4 = (box_max.getY() - ray_origin.getY()) / (ray_dir.getY() + 1e-6f);
    float t5 = (box_min.getZ() - ray_origin.getZ()) / (ray_dir.getZ() + 1e-6f);
    float t6 = (box_max.getZ() - ray_origin.getZ()) / (ray_dir.getZ() + 1e-6f);

    float tmin = fmaxf(fmaxf(fminf(t1, t2), fminf(t3, t4)), fminf(t5, t6));
    float tmax = fminf(fminf(fmaxf(t1, t2), fmaxf(t3, t4)), fmaxf(t5, t6));

    if (tmax < 0 || tmin > tmax) return false;
    t_out = (tmin < 0) ? tmax : tmin;
    return true;
}
