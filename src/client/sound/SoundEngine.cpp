#include "client/sound/SoundEngine.h"

#include "world/entity/Mob.h"
#include "util/Mth.h"
#include "java/String.h"

#include <fstream>
#include <iostream>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <cfloat>
#include <algorithm>

// stb_vorbis is compiled separately; we just need the decode function
extern "C" {
	typedef unsigned char uint8;
	int stb_vorbis_decode_memory(const uint8 *mem, int len, int *channels, int *sample_rate, short **output);
}

bool SoundEngine::loaded = false;

SoundEngine::SoundEngine()
{
	noMusicDelay = random.nextInt(12000);
}

SoundEngine::~SoundEngine()
{
	destroy();
}

// SoundManager.java:26-32
void SoundEngine::init(Options *options)
{
	this->options = options;
	this->streamingSounds.setTrimDigits(false);

	if (!loaded && (options == nullptr || options->sound != 0.0f || options->music != 0.0f))
	{
		if (initOpenAL())
			loaded = true;
	}
}

bool SoundEngine::initOpenAL()
{
	device = alcOpenDevice(nullptr);
	if (!device)
	{
		std::cerr << "Failed to open OpenAL device" << std::endl;
		return false;
	}

	context = alcCreateContext(device, nullptr);
	if (!context)
	{
		std::cerr << "Failed to create OpenAL context" << std::endl;
		alcCloseDevice(device);
		device = nullptr;
		return false;
	}

	if (!alcMakeContextCurrent(context))
	{
		std::cerr << "Failed to make OpenAL context current" << std::endl;
		alcDestroyContext(context);
		alcCloseDevice(device);
		context = nullptr;
		device = nullptr;
		return false;
	}

	freeSources.resize(MAX_SOURCES);
	alGenSources((ALsizei)freeSources.size(), freeSources.data());
	ALenum err = alGetError();
	if (err != AL_NO_ERROR)
	{
		std::cerr << "Failed to generate OpenAL sources: " << alGetString(err) << std::endl;
		cleanupOpenAL();
		return false;
	}

	alGenSources(1, &musicSource);
	alGenSources(1, &streamingSource);

	// Paulscode uses AL_INVERSE_DISTANCE (not CLAMPED)
	alDistanceModel(AL_INVERSE_DISTANCE);
	alDopplerFactor(1.0f);
	alDopplerVelocity(343.0f);

	alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
	alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	float orientation[6] = { 0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f };
	alListenerfv(AL_ORIENTATION, orientation);

	alGetError();
	return true;
}

void SoundEngine::cleanupOpenAL()
{
	for (auto &pair : activeSources)
	{
		alSourceStop(pair.second);
		alSourcei(pair.second, AL_BUFFER, 0);
	}
	activeSources.clear();
	sourceInfoMap.clear();

	if (!freeSources.empty())
	{
		alDeleteSources((ALsizei)freeSources.size(), freeSources.data());
		freeSources.clear();
	}

	if (musicSource != 0)
	{
		alSourceStop(musicSource);
		alSourcei(musicSource, AL_BUFFER, 0);
		alDeleteSources(1, &musicSource);
		musicSource = 0;
	}

	if (streamingSource != 0)
	{
		alSourceStop(streamingSource);
		alSourcei(streamingSource, AL_BUFFER, 0);
		alDeleteSources(1, &streamingSource);
		streamingSource = 0;
	}

	for (auto &pair : soundBuffers)
		alDeleteBuffers(1, &pair.second);
	soundBuffers.clear();

	if (context)
	{
		alcMakeContextCurrent(nullptr);
		alcDestroyContext(context);
		context = nullptr;
	}

	if (device)
	{
		alcCloseDevice(device);
		device = nullptr;
	}
}

// SoundManager.java:54-67
void SoundEngine::updateOptions()
{
	if (!loaded && options && (options->sound != 0.0f || options->music != 0.0f))
	{
		if (initOpenAL())
			loaded = true;
	}

	if (loaded)
	{
		if (options->music == 0.0f)
		{
			if (musicSource != 0)
			{
				alSourceStop(musicSource);
				alSourcei(musicSource, AL_BUFFER, 0);
			}
		}
		else if (musicSource != 0)
		{
			ALint state;
			alGetSourcei(musicSource, AL_SOURCE_STATE, &state);
			if (state == AL_PLAYING || state == AL_PAUSED)
				alSourcef(musicSource, AL_GAIN, options->music);
		}
	}
}

// SoundManager.java:69-74
void SoundEngine::destroy()
{
	if (loaded)
	{
		cleanupOpenAL();
		loaded = false;
	}
}

// SoundManager.java:76-78
void SoundEngine::add(const jstring &name, const std::string &filePath)
{
	sounds.add(name, filePath);
}

// SoundManager.java:80-82
void SoundEngine::addStreaming(const jstring &name, const std::string &filePath)
{
	streamingSounds.add(name, filePath);
}

// SoundManager.java:84-86
void SoundEngine::addMusic(const jstring &name, const std::string &filePath)
{
	songs.add(name, filePath);
}

// SoundManager.java:88-106
void SoundEngine::playMusicTick()
{
	if (!loaded || !options || options->music == 0.0f || musicSource == 0)
		return;

	ALint musicState;
	alGetSourcei(musicSource, AL_SOURCE_STATE, &musicState);

	ALint streamState = AL_STOPPED;
	if (streamingSource != 0)
		alGetSourcei(streamingSource, AL_SOURCE_STATE, &streamState);

	if (musicState == AL_PLAYING || streamState == AL_PLAYING)
	{
		if (noMusicDelay > 0)
			noMusicDelay--;
		return;
	}

	if (noMusicDelay > 0)
	{
		noMusicDelay--;
		return;
	}

	Sound *song = songs.any();
	if (song != nullptr)
	{
		noMusicDelay = random.nextInt(12000) + 12000;

		ALuint buffer = loadOGGFile(song->filePath);
		if (buffer != 0)
		{
			alSourceStop(musicSource);
			alSourcei(musicSource, AL_BUFFER, buffer);
			alSourcei(musicSource, AL_LOOPING, AL_TRUE);
			alSourcef(musicSource, AL_GAIN, options->music);
			alSource3f(musicSource, AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSourcef(musicSource, AL_REFERENCE_DISTANCE, 1.0f);
			alSourcef(musicSource, AL_MAX_DISTANCE, 10000.0f);
			alSourcePlay(musicSource);
		}
	}
}

// SoundManager.java:108-127 (func_338_a - listener update)
void SoundEngine::update(Mob *player, float a)
{
	if (!loaded || !options || options->sound == 0.0f || !player)
		return;

	float yRot = player->yRotO + (player->yRot - player->yRotO) * a;
	double x = player->xo + (player->x - player->xo) * (double)a;
	double y = player->yo + (player->y - player->yo) * (double)a;
	double z = player->zo + (player->z - player->zo) * (double)a;

	float yCos = Mth::cos(-yRot * Mth::DEGRAD - Mth::PI);
	float ySin = Mth::sin(-yRot * Mth::DEGRAD - Mth::PI);
	float forwardX = -ySin;
	float forwardY = 0.0f;
	float forwardZ = -yCos;

	alListener3f(AL_POSITION, (float)x, (float)y, (float)z);
	float orientation[6] = { forwardX, forwardY, forwardZ, 0.0f, 1.0f, 0.0f };
	alListenerfv(AL_ORIENTATION, orientation);

	listenerX = (float)x;
	listenerY = (float)y;
	listenerZ = (float)z;

	updateSourceGains();
	checkAndReleaseFinishedSources();
}

void SoundEngine::checkAndReleaseFinishedSources()
{
	std::vector<std::string> toRelease;
	for (auto it = activeSources.begin(); it != activeSources.end(); ++it)
	{
		ALuint source = it->second;
		ALint state;
		alGetSourcei(source, AL_SOURCE_STATE, &state);

		if (state == AL_STOPPED || state == AL_INITIAL)
		{
			alSourceStop(source);
			alSourcei(source, AL_BUFFER, 0);
			alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
			alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
			alSourcef(source, AL_GAIN, 1.0f);
			alSourcef(source, AL_PITCH, 1.0f);
			alSourcef(source, AL_REFERENCE_DISTANCE, 1.0f);
			alSourcef(source, AL_MAX_DISTANCE, FLT_MAX);
			alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
			alSourcei(source, AL_LOOPING, AL_FALSE);
			alGetError();

			freeSources.push_back(source);
			toRelease.push_back(it->first);
		}
	}

	for (const auto &id : toRelease)
	{
		activeSources.erase(id);
		sourceInfoMap.erase(id);
	}
}

// Paulscode SourceLWJGLOpenAL.java:calculateGain (lines 426-446)
// attModel 2: linear distance attenuation with manual gain
static float calculateLinearGain(float srcX, float srcY, float srcZ,
                                  float lisX, float lisY, float lisZ,
                                  float distOrRoll)
{
	float dX = srcX - lisX;
	float dY = srcY - lisY;
	float dZ = srcZ - lisZ;
	float dist = std::sqrt(dX * dX + dY * dY + dZ * dZ);

	float gain;
	if (dist <= 0.0f)
		gain = 1.0f;
	else if (dist >= distOrRoll)
		gain = 0.0f;
	else
		gain = 1.0f - dist / distOrRoll;

	if (gain > 1.0f) gain = 1.0f;
	if (gain < 0.0f) gain = 0.0f;
	return gain;
}

// SourceLWJGLOpenAL.java:positionChanged (lines 199-208)
void SoundEngine::updateSourceGains()
{
	for (auto &pair : activeSources)
	{
		ALuint source = pair.second;
		ALint state;
		alGetSourcei(source, AL_SOURCE_STATE, &state);
		if (state != AL_PLAYING && state != AL_PAUSED)
			continue;

		auto infoIt = sourceInfoMap.find(pair.first);
		if (infoIt == sourceInfoMap.end())
			continue;

		const SourceInfo &info = infoIt->second;
		float gain = 1.0f;
		if (info.attModel == 2)
			gain = calculateLinearGain(info.x, info.y, info.z, listenerX, listenerY, listenerZ, info.distOrRoll);

		alSourcef(source, AL_GAIN, gain * info.sourceVolume);
	}

	// Streaming source (not in activeSources)
	auto streamingInfoIt = sourceInfoMap.find("streaming");
	if (streamingInfoIt != sourceInfoMap.end() && streamingSource != 0)
	{
		ALint state;
		alGetSourcei(streamingSource, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING || state == AL_PAUSED)
		{
			const SourceInfo &info = streamingInfoIt->second;
			float gain = 1.0f;
			if (info.attModel == 2)
				gain = calculateLinearGain(info.x, info.y, info.z, listenerX, listenerY, listenerZ, info.distOrRoll);
			alSourcef(streamingSource, AL_GAIN, gain * info.sourceVolume);
		}
	}
}

// SoundManager.java:129-151
void SoundEngine::playStreaming(const jstring &name, float x, float y, float z, float volume, float pitch)
{
	if (!loaded || !options || options->sound == 0.0f || streamingSource == 0)
		return;

	std::string id = "streaming";

	// Stop current streaming
	ALint state;
	alGetSourcei(streamingSource, AL_SOURCE_STATE, &state);
	if (state == AL_PLAYING || state == AL_PAUSED)
	{
		alSourceStop(streamingSource);
		alSourcei(streamingSource, AL_BUFFER, 0);
	}

	if (name.empty())
	{
		alSourceStop(streamingSource);
		alSourcei(streamingSource, AL_BUFFER, 0);
		sourceInfoMap.erase(id);
		return;
	}

	Sound *sound = streamingSounds.get(name);
	if (sound == nullptr || volume <= 0.0f)
		return;

	// Stop background music if streaming starts
	if (musicSource != 0)
	{
		alGetSourcei(musicSource, AL_SOURCE_STATE, &state);
		if (state == AL_PLAYING)
		{
			alSourceStop(musicSource);
			alSourcei(musicSource, AL_BUFFER, 0);
		}
	}

	// dist * 4.0F = 64.0F for streaming
	float dist = 16.0f * 4.0f;

	ALuint buffer = loadOGGFile(sound->filePath, true);
	if (buffer == 0)
	{
		sourceInfoMap.erase(id);
		return;
	}

	alSourcei(streamingSource, AL_BUFFER, 0);
	alSource3f(streamingSource, AL_POSITION, x, y, z);
	alSource3f(streamingSource, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	alSourcei(streamingSource, AL_SOURCE_RELATIVE, AL_FALSE);
	alSourcei(streamingSource, AL_BUFFER, buffer);

	// attModel 2: disable OpenAL distance attenuation, we calculate manually
	alSourcef(streamingSource, AL_ROLLOFF_FACTOR, 0.0f);

	alSourcef(streamingSource, AL_PITCH, pitch);

	// SoundManager.java:145 - soundSystem.setVolume(id, 0.5F * this.options.sound)
	float sourceVolume = 0.5f * options->sound;
	sourceInfoMap[id] = SourceInfo(x, y, z, dist, sourceVolume, 2);

	float gain = calculateLinearGain(x, y, z, listenerX, listenerY, listenerZ, dist);
	alSourcef(streamingSource, AL_GAIN, gain * sourceVolume);
	alSourcei(streamingSource, AL_LOOPING, AL_FALSE);

	alGetError();
	alSourcePlay(streamingSource);
}

// SoundManager.java:153-175
void SoundEngine::play(const jstring &name, float x, float y, float z, float volume, float pitch)
{
	if (!loaded || !options || options->sound == 0.0f)
		return;

	Sound *sound = sounds.get(name);
	if (sound == nullptr || volume <= 0.0f)
		return;

	// SoundManager.java:156-157
	idCounter = (idCounter + 1) % 256;
	std::string id = "sound_" + std::to_string(idCounter);

	// SoundManager.java:158-160
	float dist = 16.0f;
	if (volume > 1.0f)
		dist *= volume;

	ALuint source = getOrCreateSource(id, false);
	if (source == 0)
		return;

	ALuint buffer;
	if (!loadSound(*sound, buffer))
		return;

	alSourceStop(source);
	alSourcei(source, AL_BUFFER, 0);
	alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);

	alSourcei(source, AL_BUFFER, buffer);
	alSource3f(source, AL_POSITION, x, y, z);
	alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);

	// attModel 2: disable OpenAL distance attenuation
	alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);

	// Paulscode clamps pitch between 0.5F and 2.0F (Library.java:401-416)
	float clampedPitch = pitch;
	if (clampedPitch < 0.5f)
		clampedPitch = 0.5f;
	else if (clampedPitch > 2.0f)
		clampedPitch = 2.0f;
	alSourcef(source, AL_PITCH, clampedPitch);

	// SoundManager.java:165-167 - clamp volume after pitch
	float finalVolume = volume > 1.0f ? 1.0f : volume;
	float sourceVolume = finalVolume * options->sound;
	sourceInfoMap[id] = SourceInfo(x, y, z, dist, sourceVolume, 2);

	float gain = calculateLinearGain(x, y, z, listenerX, listenerY, listenerZ, dist);
	alSourcef(source, AL_GAIN, gain * sourceVolume);
	alSourcei(source, AL_LOOPING, AL_FALSE);

	alGetError();
	alSourcePlay(source);
}

// SoundManager.java:177-195
void SoundEngine::playUI(const jstring &name, float volume, float pitch)
{
	if (!loaded || !options || options->sound == 0.0f)
		return;

	Sound *sound = sounds.get(name);
	if (sound == nullptr)
		return;

	idCounter = (idCounter + 1) % 256;
	std::string id = "sound_" + std::to_string(idCounter);

	// SoundManager.java:184-186
	if (volume > 1.0f)
		volume = 1.0f;
	volume *= 0.25f;

	ALuint source = getOrCreateSource(id, false);
	if (source == 0)
		return;

	ALuint buffer;
	if (!loadSound(*sound, buffer))
		return;

	// Non-positional (attModel 0)
	alSourceStop(source);
	alSourcei(source, AL_BUFFER, 0);
	alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);

	alSourcei(source, AL_BUFFER, buffer);
	alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
	alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
	alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f);
	alSourcef(source, AL_PITCH, pitch);
	alSourcef(source, AL_GAIN, volume * options->sound);
	alSourcei(source, AL_LOOPING, AL_FALSE);

	alGetError();
	alSourcePlay(source);
}

ALuint SoundEngine::getOrCreateSource(const std::string &id, bool streaming)
{
	if (streaming)
		return streamingSource;

	checkAndReleaseFinishedSources();

	if (!freeSources.empty())
	{
		ALuint source = freeSources.back();
		freeSources.pop_back();

		alSourceStop(source);
		alSourcei(source, AL_BUFFER, 0);
		alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
		alSourcef(source, AL_GAIN, 1.0f);
		alSourcef(source, AL_PITCH, 1.0f);
		alSourcef(source, AL_REFERENCE_DISTANCE, 1.0f);
		alSourcei(source, AL_LOOPING, AL_FALSE);

		activeSources[id] = source;
		return source;
	}

	// Steal oldest source when pool exhausted
	if (!activeSources.empty())
	{
		auto oldest = activeSources.begin();
		ALuint source = oldest->second;

		alSourceStop(source);
		alSourcei(source, AL_BUFFER, 0);
		alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
		alSourcef(source, AL_GAIN, 1.0f);
		alSourcef(source, AL_PITCH, 1.0f);
		alSourcef(source, AL_REFERENCE_DISTANCE, 1.0f);
		alSourcei(source, AL_LOOPING, AL_FALSE);

		sourceInfoMap.erase(oldest->first);
		activeSources.erase(oldest);
		activeSources[id] = source;
		return source;
	}

	return 0;
}

void SoundEngine::releaseSource(const std::string &id)
{
	auto it = activeSources.find(id);
	if (it != activeSources.end())
	{
		alSourceStop(it->second);
		alSourcei(it->second, AL_BUFFER, 0);
		freeSources.push_back(it->second);
		activeSources.erase(it);
		sourceInfoMap.erase(id);
	}
}

// OGG loading via stb_vorbis, with optional MUS XOR decryption
// MUS decryption: MusInputStream.java - XOR each byte with (hash >> 8), evolve hash
ALuint SoundEngine::loadOGGFile(const std::string &filePath, bool isMUS)
{
	std::string cacheKey = filePath + (isMUS ? "_mus" : "");
	auto it = soundBuffers.find(cacheKey);
	if (it != soundBuffers.end())
		return it->second;

	std::ifstream file(filePath, std::ios::binary | std::ios::ate);
	if (!file.is_open())
	{
		std::cerr << "Failed to open sound file: " << filePath << std::endl;
		return 0;
	}

	size_t fileSize = (size_t)file.tellg();
	if (fileSize == 0 || fileSize > 100 * 1024 * 1024)
	{
		file.close();
		return 0;
	}

	file.seekg(0, std::ios::beg);
	std::vector<unsigned char> fileData(fileSize);
	if (!file.read((char *)fileData.data(), fileSize))
	{
		file.close();
		return 0;
	}
	file.close();

	// MUS decryption (MusInputStream.java)
	if (isMUS)
	{
		// Extract filename for hash seed
		size_t lastSlash = filePath.find_last_of("/\\");
		std::string filename = (lastSlash != std::string::npos) ? filePath.substr(lastSlash + 1) : filePath;

		// Java String.hashCode()
		int32_t seed = 0;
		for (size_t i = 0; i < filename.length(); i++)
			seed = (int32_t)((uint32_t)seed * 31u + (uint32_t)(unsigned char)filename[i]);

		// XOR decryption with evolving hash
		for (size_t i = 0; i < fileData.size(); i++)
		{
			uint8_t key = (uint8_t)((uint32_t)seed >> 8);
			uint8_t val = (uint8_t)(fileData[i] ^ key);
			fileData[i] = val;
			// hash = hash * 498729871 + 85731 * (int8_t)val
			seed = (int32_t)((uint32_t)seed * 498729871u +
			                  (uint32_t)(85731 * (int32_t)(int8_t)val));
		}
	}

	int channels, sampleRate;
	short *output = nullptr;
	int samples = stb_vorbis_decode_memory((const uint8 *)fileData.data(), (int)fileSize, &channels, &sampleRate, &output);

	if (samples <= 0 || output == nullptr || channels <= 0 || channels > 2)
	{
		if (output) free(output);
		return 0;
	}

	ALenum format = (channels == 1) ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;

	ALuint buffer = 0;
	alGenBuffers(1, &buffer);
	if (buffer == 0)
	{
		free(output);
		return 0;
	}

	alBufferData(buffer, format, output, samples * channels * (int)sizeof(short), sampleRate);
	free(output);

	ALenum err = alGetError();
	if (err != AL_NO_ERROR)
	{
		alDeleteBuffers(1, &buffer);
		return 0;
	}

	soundBuffers[cacheKey] = buffer;
	return buffer;
}

bool SoundEngine::loadSound(const Sound &sound, ALuint &buffer, bool isMUS)
{
	buffer = loadOGGFile(sound.filePath, isMUS);
	return buffer != 0;
}
