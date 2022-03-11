// internal
#include "ai_system.hpp"
#include "world_init.hpp"

float safe_distance = 250;

void AISystem::step(float elapsed_ms)
{
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A2: HANDLE BUG AI HERE
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 2
	// You will likely want to write new functions and need to create
	// new data structures to implement a more sophisticated Bug AI.
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	(void)elapsed_ms; // placeholder to silence unused warning until implemented
    auto& motion_registry = registry.motions;
    Entity player = registry.players.entities.at(0);
    Motion& player_motion = motion_registry.get(player);
    for(int i = 0; i < registry.eatables.components.size(); i ++) {
        Entity bug = registry.eatables.entities[i];
        Motion& bug_motion = motion_registry.get(bug);        
        vec2 dp = player_motion.position - bug_motion.position;
        float dist_squared = dot(dp,dp);
        if (dist_squared < safe_distance * safe_distance) {
            bug_motion.velocity = {bug_motion.position.x < player_motion.position.x ? -100 : 100, 0};
        } else {
            bug_motion.velocity.y = 50;
        }
    }
    if (mode.committed) {
        for(int i = 0; i < registry.deadlys.components.size(); i ++) {
            Entity eagle = registry.deadlys.entities[i];
            Motion& eagle_motion = motion_registry.get(eagle);
            vec2 dp = player_motion.position - eagle_motion.position;
            float norm = sqrt(dot(dp, dp));
            dp = {dp.x / norm, dp.y / norm};
            eagle_motion.velocity = {dp.x * 100, dp.y * 100};
            for(int i = 0; i < registry.eatables.components.size(); i ++) {
                Entity bug = registry.eatables.entities[i];
                Motion& bug_motion = motion_registry.get(bug);        
                vec2 eb_dp = eagle_motion.position - bug_motion.position;
                float dist_squared = dot(eb_dp,eb_dp);
                if (dist_squared < safe_distance * safe_distance) {
                    eagle_motion.velocity = {eagle_motion.position.x < bug_motion.position.x ? -50 : 50, dp.y * 100};
                } else {
                    eagle_motion.velocity = {dp.x * 100, dp.y * 100};
                }
            }
        }
    }

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A2: DRAW DEBUG INFO HERE on AI path
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 2
	// You will want to use the createLine from world_init.hpp
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    if (debugging.in_debug_mode) {
        for(int i = 0; i < registry.eatables.components.size(); i ++) {
            Entity bug = registry.eatables.entities[i];
            Motion& bug_motion = motion_registry.get(bug);
            
            vec2 line_scale1 = { bug_motion.scale.x / 15, 2*safe_distance };
            vec2 line_scale2 = { 2*safe_distance, bug_motion.scale.x / 15};
            vec2 velocity = {bug_motion.position.x < player_motion.position.x ? -50 : 50, bug_motion.scale.x / 15};
            float d = bug_motion.position.x < player_motion.position.x ? -25 : 25;
            createLine({bug_motion.position.x + safe_distance, bug_motion.position.y}, line_scale1);
            createLine({bug_motion.position.x - safe_distance, bug_motion.position.y}, line_scale1);
			createLine({bug_motion.position.x, bug_motion.position.y - safe_distance}, line_scale2);
			createLine({bug_motion.position.x, bug_motion.position.y + safe_distance}, line_scale2);
            createLine({bug_motion.position.x + d, bug_motion.position.y}, velocity);
        }
    }
}