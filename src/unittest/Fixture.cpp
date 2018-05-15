#include "Fixture.hpp"

#include "../opengl.hpp"

#include <boost/test/unit_test.hpp>
#include <iostream>
#include <jngl.hpp>

Fixture::Fixture() {
	jngl::showWindow("unit test", 320, 70, false, {32, 7}, {32, 7});
	reset();
	emptyAsciiArt = getAsciiArt();
}

Fixture::~Fixture() {
	jngl::hideWindow();
}

std::string Fixture::getAsciiArt() const {
	const int w = jngl::getWindowWidth();
	const int h = jngl::getWindowHeight();
	std::vector<float> buffer(3 * w * h);
	glReadPixels(0, 0, w, h, GL_RGB, GL_FLOAT, buffer.data());
	int reduceFactor = 5;
	assert(w % reduceFactor == 0);
	assert(h % reduceFactor == 0);
	size_t reducedW = w / reduceFactor;
	size_t reducedH = h / reduceFactor;
	std::vector<std::vector<std::vector<float>>> reduced;
	size_t index = 0;
	for (int y = 0; y < h; y += reduceFactor) {
		reduced.emplace_back(reducedW, std::vector<float>(3, 0.0f));
		for (int i = 0; i < reduceFactor; ++i) {
			for (int x = 0; x < w; x += reduceFactor) {
				for (int j = 0; j < reduceFactor; ++j) {
					for (auto& cell : reduced.back().at(x / reduceFactor)) {
						assert(0.0f <= cell && cell <= 1.0f);
						cell += buffer[index] / float(reduceFactor * reduceFactor);
						++index;
					}
				}
			}
		}
	}
	assert(reduced.size() == reducedH);
	std::string out = "\n"; // Start with a newline for prettier output by Boost.Test
	for (const auto& row : reduced) {
		assert(row.size() == reducedW);
		for (const auto& cell : row) {
			// ASCII:
			// const static std::vector<std::string> chars = { "@", "#", "%", "x", "o",
			//                                                 ";", ":", ",", ".", " " };

			// UTF-8:
			const static std::vector<std::string> chars = {"█", "▓", "▒", "░", " "};

			float gray = (cell.at(0) + cell.at(1) + cell.at(2)) / 3.0f;
			const size_t index = std::lround(gray * (chars.size() - 1));
			assert(0 <= index && index < chars.size());
			out += chars[index];
		}
		out += "\n";
	}
	assert(emptyAsciiArt != out); // We shouldn't call this function without drawing something first
	reset();
	return out;
}

void Fixture::reset() const {
	jngl::swapBuffers();
	jngl::updateInput();

	const double frameSize = 5;
	const jngl::Vec2 screen(jngl::getScreenWidth(),
	                        jngl::getScreenHeight());
	jngl::drawRect(-screen.x / 2, screen.y / 2 - frameSize, screen.x, frameSize); // top
	jngl::drawRect(-screen.x / 2, -screen.y / 2, screen.x, frameSize);            // left
	jngl::drawRect(screen.x / 2 - frameSize, -screen.y / 2, frameSize, screen.y); // right
	jngl::drawRect(-screen.x / 2, -screen.y / 2, frameSize, screen.y);            // bottom
}