// internal
#include "physics_system.hpp"
#include "world_init.hpp"

// Returns the local bounding coordinates scaled by the current size of the entity
vec2 get_bounding_box(const Motion& motion)
{
	// abs is to avoid negative scale due to the facing direction.
	return { abs(motion.scale.x), abs(motion.scale.y) };
}

vec2 rotate(vec2 v, float angle) {
	mat2 m = mat2(cos(angle), -sin(angle), sin(angle), cos(angle));
	return m * v;
}

std::pair<float, float> player_left_right_most() {
    auto& player_registry = registry.players;
    auto& motion_registry = registry.motions;
    float left_most = window_width_px;
    float right_most = 0;
    for(uint i = 0; i < player_registry.size(); i ++) {
        Entity player = player_registry.entities.at(i);
        Motion& motion = motion_registry.get(player);
        Mesh* m = registry.meshPtrs.get(player);
        for(ColoredVertex& v: m->vertices) {
            vec2 vert = rotate({v.position.x * motion.scale.x, v.position.y * motion.scale.y}, motion.angle);
            vec2 p = vert + motion.position;
            left_most = min(left_most, p.x);
            right_most = max(right_most, p.x);
        }
    }
    return {left_most, right_most};
}


// This is a SUPER APPROXIMATE check that puts a circle around the bounding boxes and sees
// if the center point of either object is inside the other's bounding-box-circle. You can
// surely implement a more accurate detection
bool collides(const Motion& motion1, const Motion& motion2)
{
	vec2 dp = motion1.position - motion2.position;
	float dist_squared = dot(dp,dp);
	const vec2 other_bonding_box = get_bounding_box(motion1) / 2.f;
	const float other_r_squared = dot(other_bonding_box, other_bonding_box);
	const vec2 my_bonding_box = get_bounding_box(motion2) / 2.f;
	const float my_r_squared = dot(my_bonding_box, my_bonding_box);
	const float r_squared = max(other_r_squared, my_r_squared);
	if (dist_squared < r_squared)
		return true;
	return false;
}

void PhysicsSystem::step(float elapsed_ms)
{
	// Move bug based on how much time has passed, this is to (partially) avoid
	// having entities move at different speed based on the machine.
    float delta_t = elapsed_ms * 0.001f;
	auto& motion_registry = registry.motions;
    for (uint i = 0; i < motion_registry.size(); i++)
    {
        // !!! TODO A1: update motion.position based on step_seconds and motion.velocity
        Motion& motion = motion_registry.components[i];
        Entity entity = motion_registry.entities[i];
        motion.position = motion.position + motion.velocity * delta_t;

        // make gravity working
        if (registry.shoots.has(entity)) {
            motion.velocity.y += 98 * delta_t;
        }
        (void)elapsed_ms; // placeholder to silence unused warning until implemented
    }

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A3: HANDLE EGG UPDATES HERE
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 3
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	// Check for collisions between all moving entities
    ComponentContainer<Motion>& motion_container = registry.motions;
    for (uint i = 0; i < motion_container.components.size(); i++) {
        Motion& motion_i = motion_container.components[i];
        Entity entity_i = motion_container.entities[i];

        // note starting j at i+1 to compare all (i,j) pairs only once (and to not compare with itself)
        for (uint j = i + 1; j < motion_container.components.size(); j++) {
            Motion& motion_j = motion_container.components[j];
            Entity entity_j = motion_container.entities[j];

            if (collides(motion_i, motion_j)) {
                // Create a collisions event
                // We are abusing the ECS system a bit in that we potentially insert muliple collisions for the same entity
                registry.collisions.emplace_with_duplicates(entity_i, entity_j);
                registry.collisions.emplace_with_duplicates(entity_j, entity_i);

                // bounce
                if (registry.physicals.has(entity_i) && registry.physicals.has(entity_j)) {
                    // Create a collisions event
                    // We are abusing the ECS system a bit in that we potentially insert muliple collisions for the same entity
                    Physical phy_i = registry.physicals.get(entity_i);
                    Physical phy_j = registry.physicals.get(entity_j);
                    float m_i = phy_i.mass;
                    vec2 v_i = motion_i.velocity;
                    float m_j = phy_j.mass;
                    vec2 v_j = motion_j.velocity;
                    //Law of conservation of momentum
                    motion_i.velocity = ((m_i - m_j) * v_i + 2 * m_j * v_j) / (m_i + m_j);
                    motion_j.velocity = ((m_j - m_i) * v_j + 2 * m_i * v_i) / (m_i + m_j);
                }
            }
        }
    }

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A2: HANDLE CHICKEN - WALL collisions HERE
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 2
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	// you may need the following quantities to compute wall positions
	(float)window_width_px; (float)window_height_px;
    
    auto play_lr_pair = player_left_right_most();
    for(uint i = 0; i< motion_registry.size(); i++) {
        Entity entity = motion_registry.entities[i];
        if (registry.players.has(entity)) {
            Motion& m = motion_registry.components[i];
            if (play_lr_pair.first <= 0) {
                m.position.x += (0 - play_lr_pair.first);
            } else if (play_lr_pair.second >= window_width_px) {
                m.position.x -= (play_lr_pair.second - window_width_px);
            }
        }
    }

	for(uint i = 0; i< motion_registry.size(); i++) {
        Motion& motion = motion_container.components[i];
        Entity entity = motion_registry.entities[i];
        if (registry.eatables.has(entity)) {
            const vec2 bonding_box = get_bounding_box(motion);
            float radius = sqrt(dot(bonding_box/2.f, bonding_box/2.f));
            if ((motion.position.x - radius) <= 0) {
                motion.velocity.x = motion.velocity.x * -1.f;
                motion.position.x = radius;
            } else if ((motion.position.x + radius) >= window_width_px) {
                motion.velocity.x = motion.velocity.x * -1.f;
                motion.position.x = window_width_px - radius;
            }
        }
    }

    //eggs wall collision
    for (size_t i = 0; i < motion_container.entities.size(); ++i) {
        Motion& motion = motion_container.components[i];
        Entity entity = motion_container.entities[i];
        if (registry.shoots.has(entity)) {
            const vec2 bonding_box = get_bounding_box(motion);
            float radius = sqrt(dot(bonding_box / 2.f, bonding_box / 2.f));
            if ((motion.position.x - radius) <= 0) {
                motion.velocity.x = motion.velocity.x * -1.f;
                motion.position.x = radius;
            }
            else if ((motion.position.x + radius) >= window_width_px) {
                motion.velocity.x = motion.velocity.x * -1.f;
                motion.position.x = window_width_px - radius;
            }
        }

    }

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A2: DRAW DEBUG INFO HERE on Chicken mesh collision
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 2
	// You will want to use the createLine from world_init.hpp
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	// debugging of bounding boxes
	if (debugging.in_debug_mode)
	{
		uint size_before_adding_new = (uint)motion_container.components.size();
		for (uint i = 0; i < size_before_adding_new; i++)
		{
			Motion& motion_i = motion_container.components[i];
			Entity entity_i = motion_container.entities[i];

			// don't draw debugging visuals around debug lines
			if (registry.debugComponents.has(entity_i))
				continue;

			// visualize the radius with two axis-aligned lines
			const vec2 bonding_box = get_bounding_box(motion_i);
			float radius = sqrt(dot(bonding_box/2.f, bonding_box/2.f));
			vec2 line_scale1 = { motion_i.scale.x / 15, 2*radius };
			vec2 line_scale2 = { 2*radius, motion_i.scale.x / 15};
			// vec2 position = motion_i.position;
			// Entity line1 = createLine(motion_i.position, line_scale1);
			// Entity line2 = createLine(motion_i.position, line_scale2);

			// !!! TODO A2: implement debug bounding boxes instead of crosses
            vec2 position_u = { motion_i.position.x, motion_i.position.y + radius };
            vec2 position_d = { motion_i.position.x, motion_i.position.y - radius };
            vec2 position_l = { motion_i.position.x - radius, motion_i.position.y };
            vec2 position_r = { motion_i.position.x + radius, motion_i.position.y };
            createLine(position_u, line_scale2);
            createLine(position_d, line_scale2);
            createLine(position_l, line_scale1);
            createLine(position_r, line_scale1);

            // draw chicken mesh
            auto& player_registry = registry.players;
            for(uint i = 0; i < player_registry.size(); i ++) {
                Entity player = player_registry.entities.at(i);
                Motion& motion = motion_registry.get(player);
                Mesh* m = registry.meshPtrs.get(player);
                vec2 point = {motion.scale.x/15,motion.scale.x/15};
                for(ColoredVertex& v: m->vertices) {
                    vec2 p {v.position.x * motion.scale.x, v.position.y * motion.scale.y};
                    createLine(rotate(p, motion.angle) + motion.position, point);
                }
            }
		}
	}

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A3: HANDLE EGG collisions HERE
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 3
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // Impliment that in above, don't really know how to handle bouonce only for eggs,it's weird in logic for me
}
