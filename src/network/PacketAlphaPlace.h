#pragma once

#include "network/Packet.h"

class Packet62Sound : public Packet
{
public:
	jstring sound;
	double locX = 0.0;
	double locY = 0.0;
	double locZ = 0.0;
	float volume = 0.0f;
	float pitch = 0.0f;

	Packet62Sound();

	void readPacketData(PacketDataInput &input) override;
	void writePacketData(PacketDataOutput &output) override;
	void processPacket(NetHandler &handler) override;
	int_t getPacketSize() const override;
};

class Packet63Digging : public Packet
{
public:
	int_t x = 0;
	int_t y = 0;
	int_t z = 0;
	int_t face = 0;
	float progress = 0.0f;
	long_t timestamp = 0;

	Packet63Digging();

	void readPacketData(PacketDataInput &input) override;
	void writePacketData(PacketDataOutput &output) override;
	void processPacket(NetHandler &handler) override;
	int_t getPacketSize() const override;
};

void registerAlphaPlacePackets();
