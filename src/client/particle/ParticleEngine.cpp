#include "client/particle/ParticleEngine.h"

#include "client/particle/Particle.h"
#include "client/particle/TerrainParticle.h"
#include "client/renderer/Tesselator.h"
#include "client/renderer/Textures.h"

#include "world/level/Level.h"
#include "world/level/tile/Tile.h"

#include "util/Mth.h"
#include "java/String.h"
#include "util/Memory.h"

#include "OpenGL.h"

ParticleEngine::ParticleEngine(Level *level, Textures *textures)
	: level(level), textures(textures), random(Random())
{
	// Beta: Initialize particle lists (ParticleEngine.java:32-34)
	for (int_t i = 0; i < 4; i++)
	{
		particles[i].clear();
	}
}

void ParticleEngine::add(std::unique_ptr<Particle> p)
{
	// Beta: Add particle to appropriate list (ParticleEngine.java:37-40)
	int_t t = p->getParticleTexture();
	if (t >= 0 && t < 4)
	{
		particles[t].push_back(std::move(p));
	}
}

void ParticleEngine::tick()
{
	// Beta: Update all particles (ParticleEngine.java:42-52)
	for (int_t tt = 0; tt < 4; tt++)
	{
		for (int_t i = 0; i < (int_t)particles[tt].size(); i++)
		{
			particles[tt][i]->tick();
			if (particles[tt][i]->removed)
			{
				particles[tt].erase(particles[tt].begin() + i);
				i--;
			}
		}
	}
}

void ParticleEngine::render(Entity &player, float a)
{
	// Beta: Calculate view angles (ParticleEngine.java:54-59)
	float xa = Mth::cos(player.yRot * Mth::PI / 180.0f);
	float za = Mth::sin(player.yRot * Mth::PI / 180.0f);
	float xa2 = -za * Mth::sin(player.xRot * Mth::PI / 180.0f);
	float za2 = xa * Mth::sin(player.xRot * Mth::PI / 180.0f);
	float ya = Mth::cos(player.xRot * Mth::PI / 180.0f);

	// Beta: Set particle offset (ParticleEngine.java:60-62)
	Particle::xOff = player.xOld + (player.x - player.xOld) * a;
	Particle::yOff = player.yOld + (player.y - player.yOld) * a;
	Particle::zOff = player.zOld + (player.z - player.zOld) * a;

	// Beta: Render particles grouped by texture (ParticleEngine.java:64-90)
	for (int_t tt = 0; tt < 3; tt++)
	{
		if (!particles[tt].empty())
		{
			int_t id = 0;
			if (tt == 0)
			{
				id = textures->loadTexture(u"/particles.png");
			}
			else if (tt == 1)
			{
				id = textures->loadTexture(u"/terrain.png");
			}
			else if (tt == 2)
			{
				id = textures->loadTexture(u"/gui/items.png");
			}

			glBindTexture(GL_TEXTURE_2D, id);
			Tesselator &t = Tesselator::instance;
			t.begin();

			for (auto &p : particles[tt])
			{
				p->render(t, a, xa, ya, za, xa2, za2);
			}

			t.end();
		}
	}
}

void ParticleEngine::renderLit(Entity &player, float a)
{
	// Beta: Render lit particles (ParticleEngine.java:93-103)
	int_t tt = 3;
	if (!particles[tt].empty())
	{
		Tesselator &t = Tesselator::instance;

		for (auto &p : particles[tt])
		{
			p->render(t, a, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}

void ParticleEngine::setLevel(Level *level)
{
	this->level = level;

	// Beta: Clear all particles (ParticleEngine.java:105-111)
	for (int_t tt = 0; tt < 4; tt++)
	{
		particles[tt].clear();
	}
}

void ParticleEngine::destroy(int_t x, int_t y, int_t z)
{
	if (level == nullptr)
		return;

	// Beta: Spawn block break particles (ParticleEngine.java:113-130)
	int_t tid = level->getTile(x, y, z);
	if (tid != 0 && Tile::tiles[tid] != nullptr)
	{
		Tile *tile = Tile::tiles[tid];
		int_t data = level->getData(x, y, z);
		int_t SD = 4;

		for (int_t xx = 0; xx < SD; xx++)
		{
			for (int_t yy = 0; yy < SD; yy++)
			{
				for (int_t zz = 0; zz < SD; zz++)
				{
					double xp = x + (xx + 0.5) / SD;
					double yp = y + (yy + 0.5) / SD;
					double zp = z + (zz + 0.5) / SD;
					int_t face = random.nextInt(6);

					auto particle = Util::make_unique<TerrainParticle>(
						*level, xp, yp, zp, xp - x - 0.5, yp - y - 0.5, zp - z - 0.5, tile, face, data);
					particle->init(x, y, z);
					add(std::move(particle));
				}
			}
		}
	}
}

void ParticleEngine::crack(int_t x, int_t y, int_t z, int_t face)
{
	if (level == nullptr)
		return;

	// Beta: Spawn block mining particles (ParticleEngine.java:132-166)
	int_t tid = level->getTile(x, y, z);
	if (tid != 0 && Tile::tiles[tid] != nullptr)
	{
		Tile *tile = Tile::tiles[tid];
		int_t data = level->getData(x, y, z);
		float r = 0.1f;
		double xp = x + random.nextDouble() * (tile->xx1 - tile->xx0 - r * 2.0f) + r + tile->xx0;
		double yp = y + random.nextDouble() * (tile->yy1 - tile->yy0 - r * 2.0f) + r + tile->yy0;
		double zp = z + random.nextDouble() * (tile->zz1 - tile->zz0 - r * 2.0f) + r + tile->zz0;

		// Beta: Position particle on face (ParticleEngine.java:140-162)
		if (face == 0)
		{
			yp = y + tile->yy0 - r;
		}
		else if (face == 1)
		{
			yp = y + tile->yy1 + r;
		}
		else if (face == 2)
		{
			zp = z + tile->zz0 - r;
		}
		else if (face == 3)
		{
			zp = z + tile->zz1 + r;
		}
		else if (face == 4)
		{
			xp = x + tile->xx0 - r;
		}
		else if (face == 5)
		{
			xp = x + tile->xx1 + r;
		}

		auto particle = Util::make_unique<TerrainParticle>(*level, xp, yp, zp, 0.0, 0.0, 0.0, tile, face, data);
		particle->init(x, y, z);
		particle->setPower(0.2f);
		particle->scale(0.6f);
		add(std::move(particle));
	}
}

jstring ParticleEngine::countParticles()
{
	// Beta: Count particles (ParticleEngine.java:168-170)
	int_t count = (int_t)particles[0].size() + (int_t)particles[1].size() + (int_t)particles[2].size();
	return String::toString(count);
}
