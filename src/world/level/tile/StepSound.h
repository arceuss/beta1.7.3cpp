#pragma once

#include "java/String.h"

struct StepSound
{
	jstring name;
	float volume;
	float pitch;

	StepSound(const jstring &name, float volume, float pitch)
		: name(name), volume(volume), pitch(pitch) {}

	float getVolume() const { return volume; }
	float getPitch() const { return pitch; }

	// Break sound name
	virtual jstring stepSoundDir() const { return u"step." + name; }
	// Walk/place sound name
	virtual jstring getStepResourcePath() const { return u"step." + name; }

	virtual ~StepSound() = default;
};

// Sand overrides break sound to gravel
struct StepSoundSand : StepSound
{
	using StepSound::StepSound;
	jstring stepSoundDir() const override { return u"step.gravel"; }
};

// Glass overrides break sound to random.glass
struct StepSoundStone : StepSound
{
	using StepSound::StepSound;
	jstring stepSoundDir() const override { return u"random.glass"; }
};
