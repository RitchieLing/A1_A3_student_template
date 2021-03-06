// Header
#include "world_system.hpp"
#include "world_init.hpp"

// stlib
#include <cassert>
#include <sstream>

#include "physics_system.hpp"

// Game configuration
const size_t MAX_EAGLES = 15;
const size_t MAX_BUG = 5;
const size_t EAGLE_DELAY_MS = 2000 * 3;
const size_t BUG_DELAY_MS = 5000 * 3;
const size_t EGG_DELAY_MS = 500; 
const float shoot_volocity = 100.f;

// Create the bug world
WorldSystem::WorldSystem()
	: points(0)
	, next_eagle_spawn(0.f)
	, next_bug_spawn(0.f) 
	, next_egg_spawn(0.f) {
	// Seeding rng with random device
	rng = std::default_random_engine(std::random_device()());
}

WorldSystem::~WorldSystem() {
	// Destroy music components
	if (background_music != nullptr)
		Mix_FreeMusic(background_music);
	if (chicken_dead_sound != nullptr)
		Mix_FreeChunk(chicken_dead_sound);
	if (chicken_eat_sound != nullptr)
		Mix_FreeChunk(chicken_eat_sound);
	Mix_CloseAudio();

	// Destroy all created components
	registry.clear_all_components();

	// Close the window
	glfwDestroyWindow(window);
}

// Debugging
namespace {
	void glfw_err_cb(int error, const char *desc) {
		fprintf(stderr, "%d: %s", error, desc);
	}
}

// World initialization
// Note, this has a lot of OpenGL specific things, could be moved to the renderer
GLFWwindow* WorldSystem::create_window() {
	///////////////////////////////////////
	// Initialize GLFW
	glfwSetErrorCallback(glfw_err_cb);
	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW");
		return nullptr;
	}

	//-------------------------------------------------------------------------
	// If you are on Linux or Windows, you can change these 2 numbers to 4 and 3 and
	// enable the glDebugMessageCallback to have OpenGL catch your mistakes for you.
	// GLFW / OGL Initialization
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
#if __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
	glfwWindowHint(GLFW_RESIZABLE, 0);

	// Create the main window (for rendering, keyboard, and mouse input)
	window = glfwCreateWindow(window_width_px, window_height_px, "Chicken Game Assignment", nullptr, nullptr);
	if (window == nullptr) {
		fprintf(stderr, "Failed to glfwCreateWindow");
		return nullptr;
	}

	// Setting callbacks to member functions (that's why the redirect is needed)
	// Input is handled using GLFW, for more info see
	// http://www.glfw.org/docs/latest/input_guide.html
	glfwSetWindowUserPointer(window, this);
	auto key_redirect = [](GLFWwindow* wnd, int _0, int _1, int _2, int _3) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_key(_0, _1, _2, _3); };
	auto cursor_pos_redirect = [](GLFWwindow* wnd, double _0, double _1) { ((WorldSystem*)glfwGetWindowUserPointer(wnd))->on_mouse_move({ _0, _1 }); };
	glfwSetKeyCallback(window, key_redirect);
	glfwSetCursorPosCallback(window, cursor_pos_redirect);

	//////////////////////////////////////
	// Loading music and sounds with SDL
	if (SDL_Init(SDL_INIT_AUDIO) < 0) {
		fprintf(stderr, "Failed to initialize SDL Audio");
		return nullptr;
	}
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) == -1) {
		fprintf(stderr, "Failed to open audio device");
		return nullptr;
	}

	background_music = Mix_LoadMUS(audio_path("music.wav").c_str());
	chicken_dead_sound = Mix_LoadWAV(audio_path("chicken_dead.wav").c_str());
	chicken_eat_sound = Mix_LoadWAV(audio_path("chicken_eat.wav").c_str());

	if (background_music == nullptr || chicken_dead_sound == nullptr || chicken_eat_sound == nullptr) {
		fprintf(stderr, "Failed to load sounds\n %s\n %s\n %s\n make sure the data directory is present",
			audio_path("music.wav").c_str(),
			audio_path("chicken_dead.wav").c_str(),
			audio_path("chicken_eat.wav").c_str());
		return nullptr;
	}

	return window;
}

void WorldSystem::init(RenderSystem* renderer_arg) {
	this->renderer = renderer_arg;
	// Playing background music indefinitely
	Mix_PlayMusic(background_music, -1);
	fprintf(stderr, "Loaded music\n");

	// Set all states to default
    restart_game();
}

bool compareEntity(Entity i, Entity j)
{
	return registry.renderRequests.get(j).depth < registry.renderRequests.get(i).depth;
}

// Update our game world
bool WorldSystem::step(float elapsed_ms_since_last_update) {
	// Updating window title with points
	std::stringstream title_ss;
	title_ss << "Points: " << points;
	glfwSetWindowTitle(window, title_ss.str().c_str());

	// Remove debug info from the last step
	while (registry.debugComponents.entities.size() > 0)
	    registry.remove_all_components_of(registry.debugComponents.entities.back());

	// Removing out of screen entities
	auto& motions_registry = registry.motions;

	// Remove entities that leave the screen on the left side
	// Iterate backwards to be able to remove without unterfering with the next object to visit
	// (the containers exchange the last element with the current)
	for (int i = (int)motions_registry.components.size()-1; i>=0; --i) {
	    Motion& motion = motions_registry.components[i];
		if (motion.position.x + abs(motion.scale.x) < 0.f) {
			if(!registry.players.has(motions_registry.entities[i])) // don't remove the player
				registry.remove_all_components_of(motions_registry.entities[i]);
		}
	}

	// Spawning new eagles
	next_eagle_spawn -= elapsed_ms_since_last_update * current_speed;
    if (mode.committed) {
        
    } else {
        if (registry.deadlys.components.size() <= MAX_EAGLES && next_eagle_spawn < 0.f) {
            // Reset timer
            next_eagle_spawn = (EAGLE_DELAY_MS / 2) + uniform_dist(rng) * (EAGLE_DELAY_MS / 2);
            // Create eagle with random initial position
            createEagle(renderer, vec2(50.f + uniform_dist(rng) * (window_width_px - 100.f), 100.f));
	    }
    }

	// Spawning new bug
	next_bug_spawn -= elapsed_ms_since_last_update * current_speed;
	if (registry.eatables.components.size() <= MAX_BUG && next_bug_spawn < 0.f) {
		// !!!  TODO A1: Create new bug with createBug({0,0}), as for the Eagles above
		next_bug_spawn = (BUG_DELAY_MS / 2) + uniform_dist(rng) * (BUG_DELAY_MS / 2);
		Entity bug = createBug(renderer, vec2(50.f + uniform_dist(rng) * (window_width_px - 100.f), 100.f));
        auto& motion = registry.motions.get(bug);
        motion.velocity = {uniform_dist(rng) * 500 -250 , motion.velocity.y};
	}

	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A3: HANDLE EGG SPAWN HERE
	// DON'T WORRY ABOUT THIS UNTIL ASSIGNMENT 3
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//Spawning new egg
	
	//
	next_egg_spawn -= elapsed_ms_since_last_update * current_speed;
	if (next_egg_spawn < 0.f) {
		next_egg_spawn = (EGG_DELAY_MS / 2) + uniform_dist(rng) * (EGG_DELAY_MS / 2);
		Motion player_motion = registry.motions.get(player_chicken);
		Entity egg = createEgg(player_motion.position, vec2(15, 15));
		auto& motion = registry.motions.get(egg);
		motion.angle = (((rand() % 45) - 22.5f) * M_PI / 180.0) - player_motion.angle;
		motion.position += vec2(cos(motion.angle), sin(motion.angle)) * 20.f;
		motion.velocity = vec2(cos(motion.angle)* shoot_volocity, sin(motion.angle* shoot_volocity));
		float scale = 20.0f + float(rand() % 5);
		motion.scale = vec2(scale);
		registry.physicals.get(egg).mass = scale * 0.2f;
	}
	// Processing the chicken state
	assert(registry.screenStates.components.size() <= 1);
    ScreenState &screen = registry.screenStates.components[0];

    float min_counter_ms = 3000.f;
	for (Entity entity : registry.deathTimers.entities) {
		// progress timer
		DeathTimer& counter = registry.deathTimers.get(entity);
		counter.counter_ms -= elapsed_ms_since_last_update;
		if(counter.counter_ms < min_counter_ms){
		    min_counter_ms = counter.counter_ms;
		}

		// restart the game once the death timer expired
		if (counter.counter_ms < 0) {
			registry.deathTimers.remove(entity);
			screen.darken_screen_factor = 0;
            restart_game();
			return true;
		}
	}
	// reduce window brightness if any of the present chickens is dying
	screen.darken_screen_factor = 1 - min_counter_ms / 3000;

	// !!! TODO A1: update LightUp timers and remove if time drops below zero, similar to the death counter
	if (registry.lightUps.has(player_chicken)) {
		LightUp& lightUp = registry.lightUps.get(player_chicken);
		lightUp.timer_ms -= elapsed_ms_since_last_update;

		//remove
		if (lightUp.timer_ms < 0) {
			registry.lightUps.remove(player_chicken);
			return true;
		}
	}

	registry.renderRequests.sort(compareEntity);
	return true;
	
}

// Reset the world state to its initial state
void WorldSystem::restart_game() {
	// Debugging for memory/component leaks
	registry.list_all_components();
	printf("Restarting\n");

	// Reset the game speed
	current_speed = 1.f;

	// Remove all entities that we created
	// All that have a motion, we could also iterate over all bug, eagles, ... but that would be more cumbersome
	while (registry.motions.entities.size() > 0)
	    registry.remove_all_components_of(registry.motions.entities.back());

	// Debugging for memory/component leaks
	registry.list_all_components();

	// Create a new chicken
	player_chicken = createChicken(renderer, { window_width_px/2, window_height_px - 200 });
	registry.colors.insert(player_chicken, {1, 0.8f, 0.8f});
    mode.committed = mode.advanced;
    if (mode.committed) {
        createEagle(renderer, vec2(50.f + uniform_dist(rng) * (window_width_px - 100.f), 100.f));
    }

	// !! TODO A3: Enable static eggs on the ground
	// Create eggs on the floor for reference

	for (uint i = 0; i < 20; i++) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);
		float radius = 30 * (uniform_dist(rng) + 0.3f); // range 0.3 .. 1.3
		Entity egg = createEgg({ uniform_dist(rng) * w, h - uniform_dist(rng) * 20 },
			         { radius, radius });
		float brightness = uniform_dist(rng) * 0.5 + 0.5;
		registry.colors.insert(egg, { brightness, brightness, brightness});
	}
	
	
}

// Compute collisions between entities
void WorldSystem::handle_collisions() {
	// Loop over all collisions detected by the physics system
	auto& collisionsRegistry = registry.collisions; // TODO: @Tim, is the reference here needed?
	for (uint i = 0; i < collisionsRegistry.components.size(); i++) {
		// The entity and its collider
		Entity entity = collisionsRegistry.entities[i];
		Entity entity_other = collisionsRegistry.components[i].other;

		// For now, we are only interested in collisions that involve the chicken
		if (registry.players.has(entity)) {
			//Player& player = registry.players.get(entity);

			// Checking Player - Deadly collisions
			if (registry.deadlys.has(entity_other)) {
				// initiate death unless already dying
				if (!registry.deathTimers.has(entity)) {
					// Scream, reset timer, and make the chicken sink
					registry.deathTimers.emplace(entity);
					Mix_PlayChannel(-1, chicken_dead_sound, 0);

					// !!! TODO A1: change the chicken orientation and color on death
					Motion& player_motion = registry.motions.get(player_chicken);
					player_motion.velocity = vec2{ 0, 1000 };
					player_motion.angle = M_PI;
					vec3& color = registry.colors.get(player_chicken);
					color = { 0.9, 0.2, 0.3 };
					
				}
			}
			// Checking Player - Eatable collisions
			else if (registry.eatables.has(entity_other)) {
				if (!registry.deathTimers.has(entity)) {
					// chew, count points, and set the LightUp timer
					registry.remove_all_components_of(entity_other);
					Mix_PlayChannel(-1, chicken_eat_sound, 0);
					++points;

					// !!! TODO A1: create a new struct called LightUp in components.hpp and add an instance to the chicken entity by modifying the ECS registry
					if (registry.lightUps.has(player_chicken)) {
						registry.lightUps.remove(player_chicken);
					}
					registry.lightUps.emplace(player_chicken);
				}
			}
		}
	}

	// Remove all collisions from this simulation step
	registry.collisions.clear();
}

// Should the game be over ?
bool WorldSystem::is_over() const {
	return bool(glfwWindowShouldClose(window));
}

// On key callback
void WorldSystem::on_key(int key, int, int action, int mod) {
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: HANDLE CHICKEN MOVEMENT HERE
	// key is of 'type' GLFW_KEY_
	// action can be GLFW_PRESS GLFW_RELEASE GLFW_REPEAT
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

	
	Motion& player_motion = registry.motions.get(player_chicken);

	//up
	if (registry.deathTimers.has(player_chicken)) {
		player_motion.velocity = { 0, 0 };
	}
	else if (key == GLFW_KEY_UP) {
		if (action == GLFW_RELEASE)
			player_motion.velocity = { 0, 0 };
		else
		{
			float angle = player_motion.angle + M_PI / 2;
			player_motion.velocity.x = 200 * cos(angle);
			player_motion.velocity.y = -200 * sin(angle);
		}

	}
	//down
	else if (key == GLFW_KEY_DOWN) {
		if (action == GLFW_RELEASE)
			player_motion.velocity = { 0, 0 };
		else
		{
			float angle = player_motion.angle + M_PI / 2;
			player_motion.velocity.x = -200 * cos(angle);
			player_motion.velocity.y = 200 * sin(angle);
		}

	}
	// Left
	else if (key == GLFW_KEY_LEFT) {
		if (action == GLFW_RELEASE)
			player_motion.velocity = { 0, 0 };
		else
		{
			float angle = player_motion.angle + M_PI / 2;
			player_motion.velocity.x = -200 * sin(angle);
			player_motion.velocity.y = -200 * cos(angle);
		}
	}
	//right
	else if (key == GLFW_KEY_RIGHT) {
		if (action == GLFW_RELEASE)
			player_motion.velocity = { 0, 0 };
		else
		{
			float angle = player_motion.angle + M_PI / 2;
			player_motion.velocity.x = 200 * sin(angle);
			player_motion.velocity.y = 200 * cos(angle);
		}

	}


		
		
	if (key == GLFW_KEY_A) {
        if (action == GLFW_RELEASE) {
            mode.advanced = true;
        }
    }

    if (key == GLFW_KEY_B) {
        if (action == GLFW_RELEASE) {
            mode.advanced = false;
        }
    }

	
	// Resetting game
	if (action == GLFW_RELEASE && key == GLFW_KEY_R) {
		int w, h;
		glfwGetWindowSize(window, &w, &h);

        restart_game();
	}

	// Debugging
	if (key == GLFW_KEY_D) {
		if (action == GLFW_RELEASE)
			debugging.in_debug_mode = false;
		else
			debugging.in_debug_mode = true;
	}

	// Control the current speed with `<` `>`
	if (action == GLFW_RELEASE && (mod & GLFW_MOD_SHIFT) && key == GLFW_KEY_COMMA) {
		current_speed -= 0.1f;
		printf("Current speed = %f\n", current_speed);
	}
	if (action == GLFW_RELEASE && (mod & GLFW_MOD_SHIFT) && key == GLFW_KEY_PERIOD) {
		current_speed += 0.1f;
		printf("Current speed = %f\n", current_speed);
	}
	current_speed = fmax(0.f, current_speed);
}

void WorldSystem::on_mouse_move(vec2 mouse_position) {
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	// TODO A1: HANDLE CHICKEN ROTATION HERE
	// xpos and ypos are relative to the top-left of the window, the chicken's
	// default facing direction is (1, 0)
	// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	if (registry.deathTimers.has(player_chicken)) {
		return;
	}
	Motion& player_motion = registry.motions.get(player_chicken);
	float old_angle = atan2(mouse_position.y - player_motion.position.y , mouse_position.x - player_motion.position.x);
	player_motion.angle = M_PI - old_angle;
	(vec2)mouse_position; // dummy to avoid compiler warning
}
