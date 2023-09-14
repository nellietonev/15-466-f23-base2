#include "PlayMode.hpp"

#include "LitColorTextureProgram.hpp"

#include "DrawLines.hpp"
#include "Mesh.hpp"
#include "Load.hpp"
#include "gl_errors.hpp"
#include "data_path.hpp"

#include <glm/gtc/type_ptr.hpp>

#include <random>

GLuint beehive_meshes_for_lit_color_texture_program = 0;
Load< MeshBuffer > beehive_meshes(LoadTagDefault, []() -> MeshBuffer const * {
	MeshBuffer const *ret = new MeshBuffer(data_path("beehive.pnct"));
	beehive_meshes_for_lit_color_texture_program = ret->make_vao_for_program(lit_color_texture_program->program);
	return ret;
});

Load< Scene > beehive_scene(LoadTagDefault, []() -> Scene const * {
	return new Scene(data_path("beehive.scene"), [&](Scene &scene, Scene::Transform *transform, std::string const &mesh_name){
		Mesh const &mesh = beehive_meshes->lookup(mesh_name);

		scene.drawables.emplace_back(transform);
		Scene::Drawable &drawable = scene.drawables.back();

		drawable.pipeline = lit_color_texture_program_pipeline;

		drawable.pipeline.vao = beehive_meshes_for_lit_color_texture_program;
		drawable.pipeline.type = mesh.type;
		drawable.pipeline.start = mesh.start;
		drawable.pipeline.count = mesh.count;

	});
});

PlayMode::PlayMode() : scene(*beehive_scene) {
	/* get pointers to relevant meshes for runtime animations */
	beehive_pieces = std::array< Scene::Transform*, 7 >();
	uint8_t i = 0;

	for (auto &transform : scene.transforms) {
		if (transform.name.substr(0, 9) == "HivePiece" && i < beehive_pieces.size()) {
			beehive_pieces[i] = &transform;
			i++;
		}
		else if (transform.name == "Box") {
			hive_target_destination = &transform;
			hive_target_position = hive_target_destination->position;
		}
	}

	{ /* Error Handling */
		/* Checking array for any nullptr, from https://thispointer.com/check-if-any-element-in-array-is-null-in-c/ */
		auto has_null_ptr = [](const Scene::Transform *ptr) { return ptr == nullptr; };
		if (std::any_of(std::begin(beehive_pieces), std::end(beehive_pieces), has_null_ptr)) {
			throw std::runtime_error("At least one beehive piece not found.");
		}

		if (hive_target_destination == nullptr) throw std::runtime_error("Box not found (hive piece movement destination).");
	}

	/*
	//get pointers to leg for convenience:
	for (auto &transform : scene.transforms) {
		if (transform.name == "Hip.FL") hip = &transform;
		else if (transform.name == "UpperLeg.FL") upper_leg = &transform;
		else if (transform.name == "LowerLeg.FL") lower_leg = &transform;
	}
	if (hip == nullptr) throw std::runtime_error("Hip not found.");
	if (upper_leg == nullptr) throw std::runtime_error("Upper leg not found.");
	if (lower_leg == nullptr) throw std::runtime_error("Lower leg not found.");

	hip_base_rotation = hip->rotation;
	upper_leg_base_rotation = upper_leg->rotation;
	lower_leg_base_rotation = lower_leg->rotation;
	*/

	//get pointer to camera for convenience:
	if (scene.cameras.size() != 1) throw std::runtime_error("Expecting scene to have exactly one camera, but it has " + std::to_string(scene.cameras.size()));
	camera = &scene.cameras.front();
}

PlayMode::~PlayMode() {
}

glm::vec3 calculate_step_size(glm::vec3 total_distance, float elapsed) {
	return total_distance * elapsed;
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {
	if (evt.type == SDL_KEYDOWN) {
		if (evt.key.keysym.sym == SDLK_q) {
			Q.downs += 1;
			Q.pressed = true;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_e && !hive_piece_moving) {
			E.downs += 1;
			E.pressed = true;
			return true;
		}
	}
	else if (evt.type == SDL_KEYUP) {
		if (evt.key.keysym.sym == SDLK_q) {
			Q.pressed = false;
			return true;
		}
		else if (evt.key.keysym.sym == SDLK_e) {
			E.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	if (Q.pressed) {
		/* quit directly here if the Q button is clicked */
		PlayMode::set_current(nullptr);
		return;
	}

	if (E.pressed && !hive_piece_moving) {
		/* if there is a piece available to move, and one is not currently moving, start moving that piece */
		if (current_hive_piece < beehive_pieces.size() && !hive_piece_moving) {
			hive_piece_moving = true;
			current_hive_piece_base_position = beehive_pieces[current_hive_piece]->position;

			if (current_hive_piece > 0) hive_target_position += glm::uvec3(0.0f, 0.0f,
				beehive_meshes->lookup("HivePiece.00" + std::to_string(current_hive_piece)).max.y);

			move_distance = hive_target_position - current_hive_piece_base_position;
		}
	}

	// TODO: update which piece is current and eventually set moving to false
	/* continue moving already moving piece */
	if (hive_piece_moving) {
		beehive_pieces[current_hive_piece]->position += calculate_step_size(move_distance, elapsed);
		if (glm::all(glm::epsilonEqual(beehive_pieces[current_hive_piece]->position, hive_target_position, 0.01f))) {
			hive_piece_moving = false;
			current_hive_piece++;
		}
	}

//	if (hive_piece_moving) {
//		beehive_pieces[current_hive_piece]->position = hive_target_position;
//		hive_piece_moving = false;
//		current_hive_piece++;
//	}

	/*
	//slowly rotates through [0,1):
	wobble += elapsed / 10.0f;
	wobble -= std::floor(wobble);

	hip->rotation = hip_base_rotation * glm::angleAxis(
		glm::radians(5.0f * std::sin(wobble * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 1.0f, 0.0f)
	);
	upper_leg->rotation = upper_leg_base_rotation * glm::angleAxis(
		glm::radians(7.0f * std::sin(wobble * 2.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	lower_leg->rotation = lower_leg_base_rotation * glm::angleAxis(
		glm::radians(10.0f * std::sin(wobble * 3.0f * 2.0f * float(M_PI))),
		glm::vec3(0.0f, 0.0f, 1.0f)
	);
	 */

	//move camera:
//	{
//		//combine inputs into a move:
//		constexpr float PlayerSpeed = 30.0f;
//		glm::vec2 move = glm::vec2(0.0f);
//		if (left.pressed && !right.pressed) move.x =-1.0f;
//		if (!left.pressed && right.pressed) move.x = 1.0f;
//		if (down.pressed && !up.pressed) move.y =-1.0f;
//		if (!down.pressed && up.pressed) move.y = 1.0f;
//
//		//make it so that moving diagonally doesn't go faster:
//		if (move != glm::vec2(0.0f)) move = glm::normalize(move) * PlayerSpeed * elapsed;
//
//		glm::mat4x3 frame = camera->transform->make_local_to_parent();
//		glm::vec3 frame_right = frame[0];
//		//glm::vec3 up = frame[1];
//		glm::vec3 frame_forward = -frame[2];
//
//		camera->transform->position += move.x * frame_right + move.y * frame_forward;
//	}

	//reset button press counters:
	E.downs = 0;
	Q.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	//update camera aspect ratio for drawable:
	camera->aspect = float(drawable_size.x) / float(drawable_size.y);

	//set up light type and position for lit_color_texture_program:
	// TODO: consider using the Light(s) in the scene to do this
	glUseProgram(lit_color_texture_program->program);
	glUniform1i(lit_color_texture_program->LIGHT_TYPE_int, 1);
	glUniform3fv(lit_color_texture_program->LIGHT_DIRECTION_vec3, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f,-1.0f)));
	glUniform3fv(lit_color_texture_program->LIGHT_ENERGY_vec3, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 0.95f)));
	glUseProgram(0);

	// making the background color sky blue
	glClearColor(0.6235f, 0.8471f, 0.9373f, 1.0f);
	glClearDepth(1.0f); //1.0 is actually the default value to clear the depth buffer to, but FYI you can change it.
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS); //this is the default depth comparison function, but FYI you can change it.

	GL_ERRORS(); //print any errors produced by this setup code

	scene.draw(*camera);

	{ //use DrawLines to overlay some text:
		glDisable(GL_DEPTH_TEST);
		float aspect = float(drawable_size.x) / float(drawable_size.y);
		DrawLines lines(glm::mat4(
			1.0f / aspect, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f
		));

		constexpr float H = 0.09f;
		lines.draw_text("Press E to move a piece of honeycomb into the box; press Q to quit game",
			glm::vec3(-aspect + 0.1f * H, -1.0 + 0.1f * H, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00));
		float ofs = 2.0f / drawable_size.y;
		lines.draw_text("Press E to move a piece of honeycomb into the box; press Q to quit game",
			glm::vec3(-aspect + 0.1f * H + ofs, -1.0 + + 0.1f * H + ofs, 0.0),
			glm::vec3(H, 0.0f, 0.0f), glm::vec3(0.0f, H, 0.0f),
			glm::u8vec4(0x08, 0x08, 0x08, 0x00));
	}
}
