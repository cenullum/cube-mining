#include "ve_world.h"
#include <math.h>
#include <stdlib.h>

#include <dmsdk/dlib/log.h>

void UpdateAllNPCs(float dt) {


    for (auto& npc : g_npcs) {
        if (!npc.instance) continue;

        if (npc.pos.getY() < -10.0f) {
            npc.health = 0;
        }

        if (npc.health <= 0 && !npc.is_dead) {
            npc.is_dead = true;
            
            dmMessage::URL receiver;
            dmMessage::ResetURL(&receiver);
            receiver.m_Socket = npc.socket;
            receiver.m_Path = dmGameObject::GetIdentifier(npc.instance);
            dmMessage::Post(0, &receiver, dmHashString64("died"), 0, 0, 0, 0, 0, 0);
        }

        if (npc.is_dead) continue;

        npc.timer += dt;
        if (npc.timer >= npc.state_duration) {
            npc.timer = 0;
            if (npc.state == 1) { // IDLE
                npc.state = 2; // WALK
                npc.state_duration = (float)(rand() % 300) / 100.0f + 1.0f; // 1-4s
                float angle = (float)(rand() % 628) / 100.0f;
                npc.move_dir = dmVMath::Vector3(cosf(angle), 0, sinf(angle));
            } else {
                npc.state = 1; // IDLE
                npc.state_duration = (float)(rand() % 300) / 100.0f + 4.0f; // 4-7s
                npc.move_dir = dmVMath::Vector3(0, 0, 0);
            }
        }

        float target_vel_x = 0;
        float target_vel_z = 0;
        if (npc.state == 2) { // WALK
            target_vel_x = npc.move_dir.getX() * npc.speed;
            target_vel_z = npc.move_dir.getZ() * npc.speed;
        }

        float damping = (npc.state == 2) ? 0.05f : 0.02f;
        npc.vel.setX(npc.vel.getX() + (target_vel_x - npc.vel.getX()) * damping);
        npc.vel.setZ(npc.vel.getZ() + (target_vel_z - npc.vel.getZ()) * damping);
        npc.vel.setY(npc.vel.getY() + npc.gravity * dt);

        bool grounded = false;
        MoveAndSlide(npc.pos, npc.vel, npc.size, dt, grounded);

        if (npc.state == 2 && grounded) {
            dmVMath::Vector3 dir = npc.move_dir;
            if (dmVMath::LengthSqr(dir) > 0.001f) {
                dir = dmVMath::Normalize(dir);
                float check_dist = 0.6f;
                
                dmVMath::Vector3 ground_check_min = npc.pos + dir * check_dist + dmVMath::Vector3(-0.1f, -1.6f, -0.1f);
                dmVMath::Vector3 ground_check_max = npc.pos + dir * (check_dist + 0.2f) + dmVMath::Vector3(0.1f, -0.1f, 0.1f);
                if (!CheckCollision(ground_check_min.getX(), ground_check_min.getY(), ground_check_min.getZ(),
                                   ground_check_max.getX(), ground_check_max.getY(), ground_check_max.getZ())) {
                    npc.move_dir = -npc.move_dir;
                    npc.timer = 0;
                    npc.vel.setX(0); npc.vel.setZ(0);
                } else {
                    float current_speed = dmVMath::Length(dmVMath::Vector3(npc.vel.getX(), 0, npc.vel.getZ()));
                    if (current_speed < npc.speed * 0.2f) {
                        dmVMath::Vector3 obs_check_min = npc.pos + dir * 0.5f + dmVMath::Vector3(-0.1f, 0.1f, -0.1f);
                        dmVMath::Vector3 obs_check_max = npc.pos + dir * 0.7f + dmVMath::Vector3(0.1f, 0.9f, 0.1f);
                        dmVMath::Vector3 clear_check_min = npc.pos + dir * 0.5f + dmVMath::Vector3(-0.1f, 1.1f, -0.1f);
                        dmVMath::Vector3 clear_check_max = npc.pos + dir * 0.7f + dmVMath::Vector3(0.1f, 1.9f, 0.1f);

                        if (CheckCollision(obs_check_min.getX(), obs_check_min.getY(), obs_check_min.getZ(),
                                          obs_check_max.getX(), obs_check_max.getY(), obs_check_max.getZ()) &&
                            !CheckCollision(clear_check_min.getX(), clear_check_min.getY(), clear_check_min.getZ(),
                                           clear_check_max.getX(), clear_check_max.getY(), clear_check_max.getZ())) {
                            npc.vel.setY(npc.jump_force);
                        } else {
                            float angle = (float)(rand() % 628) / 100.0f;
                            npc.move_dir = dmVMath::Vector3(cosf(angle), 0, sinf(angle));
                            npc.timer = 0;
                        }
                    }
                }
            }
        }

        dmGameObject::SetPosition(npc.instance, dmVMath::Point3(npc.pos.getX(), npc.pos.getY(), npc.pos.getZ()));
        if (npc.state == 2 && dmVMath::LengthSqr(npc.move_dir) > 0.01f) {
            float angle = atan2f(npc.move_dir.getX(), npc.move_dir.getZ());
            dmVMath::Quat rot = dmVMath::Quat::rotationY(angle + npc.rotation_offset_y * (3.14159265f / 180.0f));
            dmGameObject::SetRotation(npc.instance, rot);
        }
    }
}
