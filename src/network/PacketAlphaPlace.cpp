#include "network/PacketAlphaPlace.h"

#include <memory>

#include "java/System.h"
#include "network/NetHandler.h"
#include "network/PacketDataStream.h"

namespace
{

template<typename T>
std::unique_ptr<Packet> makePacket()
{
	return std::make_unique<T>();
}

}

Packet62Sound::Packet62Sound() : Packet(62)
{
}

void Packet62Sound::readPacketData(PacketDataInput &input)
{
	// APClient uses DataInputStream.readUTF(), not Packet.readString().
	sound = input.readUTF();
	locX = input.readDouble();
	locY = input.readDouble();
	locZ = input.readDouble();
	volume = input.readFloat();
	pitch = input.readFloat();
}

void Packet62Sound::writePacketData(PacketDataOutput &)
{
	// APClient's write method is empty; this packet is clientbound only.
}

void Packet62Sound::processPacket(NetHandler &handler)
{
	handler.handle62Sound(*this);
}

int_t Packet62Sound::getPacketSize() const
{
	// The original estimate counts UTF-16 code units, not encoded wire bytes.
	return static_cast<int_t>(sound.size()) + 32;
}

Packet63Digging::Packet63Digging() : Packet(63)
{
}

void Packet63Digging::readPacketData(PacketDataInput &input)
{
	x = input.readInt();
	y = input.readInt();
	z = input.readInt();
	face = input.readByte();
	progress = input.readFloat();
	timestamp = System::currentTimeMillis();
}

void Packet63Digging::writePacketData(PacketDataOutput &)
{
	// Reception time is local state; APClient writes no payload at all.
}

void Packet63Digging::processPacket(NetHandler &handler)
{
	handler.handle63Digging(*this);
}

int_t Packet63Digging::getPacketSize() const
{
	return 17;
}

void registerAlphaPlacePackets()
{
	Packet::addIdClassMapping(62, true, false, &makePacket<Packet62Sound>, "Packet62Sound");
	Packet::addIdClassMapping(63, true, false, &makePacket<Packet63Digging>, "Packet63Digging");
}
