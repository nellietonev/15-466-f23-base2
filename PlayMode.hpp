#include "Mode.hpp"

#include "Scene.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} E, Q;

	//local copy of the game scene (so code can change it during gameplay):
	Scene scene;

	// relevant meshes and information about the animation
	std::array< Scene::Transform*, 7 > beehive_pieces;
	bool hive_piece_moving = false;

	uint8_t current_hive_piece = 0;
	glm::vec3 current_hive_piece_base_position;

	Scene::Transform *hive_target_destination;
	glm::vec3 hive_target_position;
	glm::vec3 distance_to_move;
	
	//camera:
	Scene::Camera *camera = nullptr;

};
