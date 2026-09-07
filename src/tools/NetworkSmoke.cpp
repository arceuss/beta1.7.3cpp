#include "tools/NetworkSmoke.h"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "client/Minecraft.h"
#include "ClientTarget.h"
#include "SharedConstants.h"
#include "client/gamemode/GameMode.h"
#include "network/NetHandler.h"
#include "network/Packet.h"
#include "network/PacketAlphaPlace.h"
#include "network/PacketCore.h"
#include "network/PacketDataStream.h"
#include "network/PacketEntity.h"
#include "network/PacketWindow.h"
#include "network/PacketWorld.h"
#include "client/spc/SPCCommand.h"
#include "world/level/Level.h"
#include "world/level/Explosion.h"
#include "world/entity/animal/Cow.h"
#include "world/entity/item/EntityPainting.h"
#include "world/entity/player/Player.h"
#include "world/inventory/BasicInventory.h"
#include "world/inventory/ContainerMenus.h"
#include "world/item/Items.h"
#include "world/item/Item.h"
#include "world/level/tile/entity/DispenserTileEntity.h"
#include "world/level/tile/entity/FurnaceTileEntity.h"
#include "world/level/tile/LadderTile.h"
#include "util/Mth.h"

namespace
{

using Bytes = std::vector<ubyte_t>;

bool expect(bool condition, const char *message)
{
	if (!condition)
		std::cerr << "FAILED: " << message << std::endl;
	return condition;
}

std::string toHex(const Bytes &bytes)
{
	std::ostringstream result;
	result << std::hex << std::setfill('0');
	for (ubyte_t byte : bytes)
		result << std::setw(2) << static_cast<int_t>(byte);
	return result.str();
}

bool expectBytes(const Bytes &actual, const Bytes &expected, const char *message)
{
	if (actual == expected)
		return true;
	std::cerr << "FAILED: " << message << "\n  expected " << toHex(expected)
		<< "\n  actual   " << toHex(actual) << std::endl;
	return false;
}

Bytes bytesFrom(const std::string &value)
{
	return Bytes(value.begin(), value.end());
}

Bytes bytesFromHex(const char *value)
{
	Bytes bytes;
	while (*value)
	{
		auto digit = [](char ch) -> ubyte_t
		{
			if (ch >= '0' && ch <= '9')
				return static_cast<ubyte_t>(ch - '0');
			return static_cast<ubyte_t>(ch - 'A' + 10);
		};
		bytes.push_back(static_cast<ubyte_t>((digit(value[0]) << 4) | digit(value[1])));
		value += 2;
	}
	return bytes;
}

Bytes encode(Packet &packet)
{
	std::ostringstream stream(std::ios::binary);
	PacketDataOutput output(stream);
	Packet::writePacket(packet, output);
	return bytesFrom(stream.str());
}

std::unique_ptr<Packet> decode(const Bytes &bytes, bool serverHandler)
{
	std::string value(bytes.begin(), bytes.end());
	std::istringstream stream(value, std::ios::binary);
	PacketDataInput input(stream);
	return Packet::readPacket(input, serverHandler);
}

class InspectableHandler : public NetHandler
{
public:
	Packet3Chat *chat = nullptr;
	Packet62Sound *sound = nullptr;
	Packet63Digging *digging = nullptr;

	bool isServerHandler() const override
	{
		return false;
	}

	void handleChat(Packet3Chat &packet) override
	{
		chat = &packet;
	}

	void handle62Sound(Packet62Sound &packet) override { sound = &packet; }
	void handle63Digging(Packet63Digging &packet) override { digging = &packet; }
};

class NetworkSmokeLevel : public Level
{
public:
	NetworkSmokeLevel() : Level(u"network-smoke", 0, 12345, false)
	{
	}
};

class LevelEventListener : public LevelListener
{
public:
	Player *player = nullptr;
	int_t event = 0;
	int_t x = 0;
	int_t y = 0;
	int_t z = 0;
	int_t data = 0;
	std::vector<jstring> sounds;

	void tileChanged(int_t, int_t, int_t) override {}
	void setTilesDirty(int_t, int_t, int_t, int_t, int_t, int_t) override {}
	void allChanged() override {}
	void playSound(const jstring &name, double, double, double, float, float) override { sounds.push_back(name); }
	void addParticle(const jstring &, double, double, double, double, double, double) override {}
	void playMusic(const jstring &, double, double, double, float) override {}
	void entityAdded(std::shared_ptr<Entity>) override {}
	void entityRemoved(std::shared_ptr<Entity>) override {}
	void skyColorChanged() override {}
	void playStreamingMusic(const jstring &, int_t, int_t, int_t) override {}
	void tileEntityChanged(int_t, int_t, int_t, std::shared_ptr<TileEntity>) override {}
	void levelEvent(Player *source, int_t eventId, int_t eventX, int_t eventY, int_t eventZ, int_t eventData) override
	{
		player = source;
		event = eventId;
		x = eventX;
		y = eventY;
		z = eventZ;
		data = eventData;
	}
};

class AlphaPlaceSmokeLevel : public NetworkSmokeLevel
{
public:
	int_t tileId = 1;
	int_t getTile(int_t, int_t, int_t) override { return tileId; }
};

class ClientTargetSmokeLevel : public NetworkSmokeLevel
{
public:
	TilePos ladder = TilePos(-1, 64, -2);
	std::vector<std::shared_ptr<Entity>> spawned;

	int_t getTile(int_t x, int_t y, int_t z) override
	{
		return y >= 0 && y < Level::DEPTH && x == ladder.x && y == ladder.y && z == ladder.z
			? Tile::ladder.id : 0;
	}

	bool addEntity(std::shared_ptr<Entity> entity) override
	{
		spawned.push_back(entity);
		return true;
	}
};

bool testClientTargetBehavior()
{
	bool ok = true;
#if defined(B173_TARGET_ALPHAPLACE)
	const bool alphaPlace = true;
	const jstring version = u"Alpha v1.2.6";
	const int_t protocol = 2000;
#else
	const bool alphaPlace = false;
	const jstring version = u"Beta 1.7.3";
	const int_t protocol = 14;
#endif
	ok &= expect(ClientTarget::isAlphaPlace() == alphaPlace, "compiled client target matches the build definition");
	ok &= expect(SharedConstants::VERSION_STRING == version && Minecraft::VERSION_STRING == u"Minecraft " + version,
		"shared and displayed version strings match the compiled target");
	ok &= expect(SharedConstants::NETWORK_PROTOCOL_VERSION == protocol && ClientTarget::loginProtocolVersion() == protocol,
		"shared protocol and login protocol agree for the compiled target");

	for (bool online : {false, true})
	{
		std::cout << "Client-target behavior: " << (alphaPlace ? "AlphaPlace" : "Beta")
			<< (online ? " multiplayer" : " singleplayer") << std::endl;
		const bool localSound = !(alphaPlace && online);
		ClientTargetSmokeLevel level;
		level.isOnline = online;
		LevelEventListener listener;
		level.addListener(listener);
		Cow ladderMob(level);
		ladderMob.setPos(-0.25, 64.25, -1.25);
		ok &= expect(Mth::floor(ladderMob.bb.y0) == 64, "ladder fixture uses the bounding-box feet height");
		for (int_t offset : {0, 1, 2, -1})
		{
			level.ladder = TilePos(-1, 64 + offset, -2);
			ok &= expect(ladderMob.onLadder() == (offset == 0 || (alphaPlace && offset == 1)),
				"only AlphaPlace accepts a ladder one block above the floored feet");
		}
		for (const TilePos &adjacent : {TilePos(0, 64, -2), TilePos(-1, 64, -1), TilePos(0, 65, -2), TilePos(-1, 65, -1)})
		{
			level.ladder = adjacent;
			ok &= expect(!ladderMob.onLadder(), "adjacent ladders do not count at negative coordinates");
		}
		ladderMob.setPos(-0.25, 127.25, -1.25);
		level.ladder = TilePos(-1, 127, -2);
		ok &= expect(ladderMob.onLadder(), "ladder at the highest valid feet block is usable");
		level.ladder = TilePos(-1, 128, -2);
		ok &= expect(!ladderMob.onLadder(), "ladder lookup above world height remains air");

		Cow cow(level);
		Random expectedAmbient = cow.auditRandom();
		if (localSound)
		{
			expectedAmbient.nextFloat();
			expectedAmbient.nextFloat();
		}
		cow.playAmbientSound();
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"mob.cow"} : std::vector<jstring>{}),
			"ambient sound is suppressed only for AlphaPlace multiplayer");
		ok &= expect(cow.auditRandom().rawState() == expectedAmbient.rawState(),
			"suppressed ambient sound does not consume pitch randomness");
		listener.sounds.clear();
		const int_t previousHealth = cow.health;
		cow.handleEntityEvent(2);
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"mob.cowhurt"} : std::vector<jstring>{}),
			"hurt-status sound follows the target multiplayer guard");
		ok &= expect(cow.hurtTime == 10 && cow.walkAnimSpeed == 1.5f && cow.health == previousHealth,
			"hurt-status animation survives sound suppression");
		listener.sounds.clear();
		cow.handleEntityEvent(3);
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"mob.cowhurt"} : std::vector<jstring>{}),
			"death-status sound follows the target multiplayer guard");
		ok &= expect(cow.health == 0, "death-status health transition survives sound suppression");
		listener.sounds.clear();
		level.spawned.clear();

		Explosion explosion(level, nullptr, 1.25, 64.0, -2.5, 2.0f);
		Random expectedExplosion = level.random;
		if (localSound)
		{
			expectedExplosion.nextFloat();
			expectedExplosion.nextFloat();
		}
		explosion.doExplosionB(false);
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"random.explode"} : std::vector<jstring>{}),
			"explosion sound follows the target multiplayer guard");
		ok &= expect(level.random.rawState() == expectedExplosion.rawState(),
			"suppressed explosion sound does not consume pitch randomness");
		listener.sounds.clear();

		Player player(level);
		player.inventory.setItem(0, ItemInstance(Items::arrow->getShiftedIndex(), 2, 0));
		ItemInstance bow(Items::bow->getShiftedIndex(), 1, 0);
		Items::bow->use(bow, level, player);
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"random.bow"} : std::vector<jstring>{}),
			"bow launch sound follows the target multiplayer guard");
		ok &= expect(player.inventory.getItem(0) && player.inventory.getItem(0)->stackSize == 1 &&
			level.spawned.size() == (online ? 0u : 1u), "bow consumption and offline spawning survive sound suppression");
		listener.sounds.clear();
		level.spawned.clear();
		ItemInstance snowball(Items::snowball->getShiftedIndex(), 2, 0);
		Items::snowball->use(snowball, level, player);
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"random.bow"} : std::vector<jstring>{}),
			"snowball launch sound follows the target multiplayer guard");
		ok &= expect(snowball.stackSize == 1 && level.spawned.size() == (online ? 0u : 1u),
			"snowball consumption and offline spawning survive sound suppression");
		listener.sounds.clear();
		level.spawned.clear();
		ItemInstance rod(Items::fishingRod->getShiftedIndex(), 1, 0);
		Items::fishingRod->use(rod, level, player);
		ok &= expect(listener.sounds == (localSound ? std::vector<jstring>{u"random.bow"} : std::vector<jstring>{}),
			"fishing launch sound follows the target multiplayer guard");
		ok &= expect(player.swinging && (player.fishEntity != nullptr) == !online &&
			level.spawned.size() == (online ? 0u : 1u), "fishing swing and offline spawning survive sound suppression");
		listener.sounds.clear();

		level.playSoundEffect(1.0, 64.0, 2.0, u"liquid.water", 0.5f, 1.0f);
		level.playSoundEffect(1.0, 64.0, 2.0, u"fire.fire", 0.5f, 1.0f);
		level.playSoundAtEntity(player, u"random.fizz", 0.5f, 1.0f);
		ok &= expect(listener.sounds == std::vector<jstring>{u"liquid.water", u"fire.fire", u"random.fizz"},
			"direct world and entity audio remain available in AlphaPlace multiplayer");
		level.removeListener(listener);
	}
	return ok;
}

bool testAlphaPlacePackets()
{
	bool ok = true;
	// Original APClient decoder fixtures, kept outside the 57 standard round trips.
	const Bytes soundWire = bytesFromHex("3E000C72616E646F6D2E636C69636B3FF4000000000000C00400000000000040500000000000003F4000003FC00000");
	const Bytes diggingWire = bytesFromHex("3F01020304FFFFFFEF00000041053F200000");
	auto rejected = [](const Bytes &wire, bool serverHandler)
	{
		try { decode(wire, serverHandler); }
		catch (const std::runtime_error &) { return true; }
		return false;
	};
	for (const Bytes *wire : {&soundWire, &diggingWire})
	{
		ok &= expect(rejected(*wire, true), "AlphaPlace packets are rejected serverbound in both targets");
		if (!ClientTarget::isAlphaPlace())
			ok &= expect(rejected(*wire, false), "normal Beta rejects AlphaPlace packets clientbound");
	}

	Packet1Login login(u"AP", ClientTarget::loginProtocolVersion());
	const Bytes loginWire = encode(login);
	const Bytes expectedLoginPrefix = ClientTarget::isAlphaPlace()
		? bytesFromHex("01000007D0") : bytesFromHex("010000000E");
	ok &= expect(loginWire.size() >= expectedLoginPrefix.size() &&
		std::equal(expectedLoginPrefix.begin(), expectedLoginPrefix.end(), loginWire.begin()),
		"login encodes target protocol 2000 for AlphaPlace and 14 for Beta");
	if (!ClientTarget::isAlphaPlace())
		return ok;

	auto soundBase = decode(soundWire, false);
	auto *sound = dynamic_cast<Packet62Sound *>(soundBase.get());
	ok &= expect(sound && sound->sound == u"random.click" && sound->locX == 1.25 &&
		sound->locY == -2.5 && sound->locZ == 64.0 && sound->volume == 0.75f && sound->pitch == 1.5f,
		"Packet62Sound original Java decoder fixture");
	ok &= expect(sound && soundWire.size() == 47 && sound->getPacketSize() == 44,
		"Packet62Sound preserves the original reported size rather than its wire length");
	const long_t before = System::currentTimeMillis();
	auto diggingBase = decode(diggingWire, false);
	const long_t after = System::currentTimeMillis();
	auto *digging = dynamic_cast<Packet63Digging *>(diggingBase.get());
	ok &= expect(digging && digging->x == 0x01020304 && digging->y == -17 && digging->z == 65 &&
		digging->face == 5 && digging->progress == 0.625f && digging->getPacketSize() == 17 &&
		digging->timestamp >= before && digging->timestamp <= after,
		"Packet63Digging original fixture and local reception timestamp");
	if (sound && digging)
	{
		InspectableHandler handler;
		sound->processPacket(handler);
		digging->processPacket(handler);
		ok &= expect(handler.sound == sound && handler.digging == digging,
			"AlphaPlace packets dispatch to their dedicated callbacks");
		ok &= expectBytes(encode(*sound), {0x3e}, "Packet62Sound retains the empty Java writer");
		ok &= expectBytes(encode(*digging), {0x3f}, "Packet63Digging retains the empty Java writer");
	}

	Bytes modifiedUtf = bytesFromHex("3E000941C080EDA0BDEDB880");
	modifiedUtf.insert(modifiedUtf.end(), soundWire.begin() + 15, soundWire.end());
	auto utfBase = decode(modifiedUtf, false);
	auto *utf = dynamic_cast<Packet62Sound *>(utfBase.get());
	jstring expectedText = u"A";
	expectedText.push_back(0);
	expectedText.push_back(0xd83d);
	expectedText.push_back(0xde00);
	ok &= expect(utf && utf->sound == expectedText && utf->getPacketSize() == 36 && utf->pitch == 1.5f,
		"Packet62Sound decodes modified-UTF NUL and surrogate pairs and counts UTF-16 units");
	Bytes invalidUtf = bytesFromHex("3E000180");
	invalidUtf.insert(invalidUtf.end(), soundWire.begin() + 15, soundWire.end());
	ok &= expect(rejected(invalidUtf, false), "Packet62Sound rejects malformed modified UTF");
	Bytes signedFace = diggingWire;
	signedFace[13] = 0xff;
	auto signedBase = decode(signedFace, false);
	auto *signedPacket = dynamic_cast<Packet63Digging *>(signedBase.get());
	ok &= expect(signedPacket && signedPacket->face == -1, "Packet63Digging preserves a signed face byte");
	for (const Bytes *wire : {&soundWire, &diggingWire})
	{
		Bytes truncated(wire->begin(), wire->end() - 1);
		ok &= expect(decode(truncated, false) == nullptr, "truncated AlphaPlace payload returns stream EOF");
	}

	Bytes joined = soundWire;
	joined.insert(joined.end(), diggingWire.begin(), diggingWire.end());
	joined.push_back(0); // A following standard keepalive must remain correctly framed.
	std::istringstream stream(std::string(joined.begin(), joined.end()), std::ios::binary);
	PacketDataInput input(stream);
	auto first = Packet::readPacket(input, false);
	auto second = Packet::readPacket(input, false);
	auto third = Packet::readPacket(input, false);
	ok &= expect(first && first->getPacketId() == 62 && second && second->getPacketId() == 63 &&
		third && third->getPacketId() == 0 && Packet::readPacket(input, false) == nullptr,
		"AlphaPlace payloads preserve framing of consecutive AP and standard packets");
	return ok;
}

}

int runNetworkSmoke()
{
	bool ok = true;
	Tile::initTiles();
	Items::initItems();
	ok &= testAlphaPlacePackets();
	ok &= testClientTargetBehavior();

	struct PacketOracleCase
	{
		int_t id;
		bool clientbound;
		bool serverbound;
		bool roundTrip;
		const char *message;
		const char *hex;
	};
	const PacketOracleCase packetOracle[] = {
		{0, true, true, true, "Packet0KeepAlive Java oracle golden round trip", "00"},
		{1, true, true, true, "Packet1Login Java oracle golden round trip", "010000000E000C006F007200610063006C0065002D006C006F00670069006E0102030405060708FF"},
		{2, true, true, true, "Packet2Handshake Java oracle golden round trip", "020010006F007200610063006C0065002D00680061006E0064007300680061006B0065"},
		{3, true, true, true, "Packet3Chat Java oracle golden round trip", "03000B006F007200610063006C0065002D0063006800610074"},
		{4, true, false, true, "Packet4UpdateTime Java oracle golden round trip", "040102030405060708"},
		{5, true, false, true, "Packet5PlayerInventory Java oracle golden round trip", "0501020304000201080007"},
		{6, true, false, true, "Packet6SpawnPosition Java oracle golden round trip", "060001E24000000041FFFC6BB9"},
		{7, false, true, true, "Packet7UseEntity Java oracle golden round trip", "07000000110000001701"},
		{8, true, false, true, "Packet8UpdateHealth Java oracle golden round trip", "080011"},
		{9, true, true, true, "Packet9Respawn Java oracle golden round trip", "09FF"},
		{10, true, true, true, "Packet10Flying Java oracle golden round trip", "0A01"},
		{11, true, true, true, "Packet11PlayerPosition Java oracle golden round trip", "0B40288000000000004050200000000000405087AE147AE148C040E0000000000001"},
		{12, true, true, true, "Packet12PlayerLook Java oracle golden round trip", "0C42F70000C235000001"},
		{13, true, true, true, "Packet13PlayerLookMove Java oracle golden round trip", "0D40288000000000004050200000000000405087AE147AE148C040E0000000000042F70000C235000001"},
		{14, false, true, true, "Packet14BlockDig Java oracle golden round trip", "0E02FFFFFFEF400000001F05"},
		{15, false, true, true, "Packet15Place Java oracle golden round trip", "0F000000643FFFFFFF9C010108030007"},
		{16, false, true, true, "Packet16BlockItemSwitch Java oracle golden round trip", "100006"},
		{17, true, false, true, "Packet17Sleep Java oracle golden round trip", "1101020304030000000C46FFFFFFE9"},
		{18, true, true, true, "Packet18Animation Java oracle golden round trip", "120102030402"},
		{19, false, true, true, "Packet19EntityAction Java oracle golden round trip", "130102030401"},
		{20, true, false, true, "Packet20NamedEntitySpawn Java oracle golden round trip", "1401020304000D006F007200610063006C0065002D0070006C00610079006500720000014000000800FFFFFD8040E00114"},
		{21, true, false, true, "Packet21PickupSpawn Java oracle golden round trip", "150102030401080300070000014100000801FFFFFD81212C37"},
		{22, true, false, true, "Packet22Collect Java oracle golden round trip", "160102030411121314"},
		{23, true, false, true, "Packet23VehicleSpawn Java oracle golden round trip", "17010203043D000004D200000929FFFFF280111213140190FE0C0258"},
		{24, true, false, false, "Packet24MobSpawn Java oracle constructor-only metadata record", "180102030432000004D200000929FFFFF28060E80012212345423456789A634148000084000B006F007200610063006C0065002D006D006500740061A50108030007C60000000700000041FFFFFFF77F"},
		{25, true, false, true, "Packet25EntityPainting Java oracle golden round trip", "19010203040005004B0065006200610062FFFFFFF5000000480000001D00000003"},
		{27, false, true, true, "Packet27Position Java oracle golden round trip", "1B3E800000BF0000003F400000BFA000000101"},
		{28, true, false, true, "Packet28EntityVelocity Java oracle golden round trip", "1C0102030404B0F7040D48"},
		{29, true, false, true, "Packet29DestroyEntity Java oracle golden round trip", "1D01020304"},
		{30, true, false, true, "Packet30Entity Java oracle golden round trip", "1E01020304"},
		{31, true, false, true, "Packet31RelEntityMove Java oracle golden round trip", "1F0102030405FA07"},
		{32, true, false, true, "Packet32EntityLook Java oracle golden round trip", "200102030440E0"},
		{33, true, false, true, "Packet33RelEntityMoveLook Java oracle golden round trip", "210102030405FA0740E0"},
		{34, true, false, true, "Packet34EntityTeleport Java oracle golden round trip", "2201020304000004D200000929FFFFF28040E0"},
		{38, true, false, true, "Packet38EntityStatus Java oracle golden round trip", "260102030403"},
		{39, true, false, true, "Packet39AttachEntity Java oracle golden round trip", "270102030411121314"},
		{40, true, false, true, "Packet40EntityMetadata Java oracle golden round trip", "28010203040012212345423456789A634148000084000B006F007200610063006C0065002D006D006500740061A50108030007C60000000700000041FFFFFFF77F"},
		{50, true, false, true, "Packet50PreChunk Java oracle golden round trip", "3200000011FFFFFFE901"},
		{51, true, false, false, "Packet51MapChunk Java oracle private-payload record", "33000000110020FFFFFFE9010203000000050102030405"},
		{52, true, false, true, "Packet52MultiBlockChange Java oracle golden round trip", "3400000011FFFFFFE90003123456789ABC01142A03070F"},
		{53, true, false, true, "Packet53BlockChange Java oracle golden round trip", "35FFFFFFEF410000001F3605"},
		{54, true, false, true, "Packet54PlayNoteBlock Java oracle golden round trip", "36FFFFFFEF00410000001F020C"},
		{60, true, false, false, "Packet60Explosion Java oracle Java6 HashSet record", "3C40591000000000004050200000000000C034C000000000004090000000000003FEFFFE010101040400"},
		{61, true, false, true, "Packet61DoorChange Java oracle golden round trip", "3D000003E9000000411F00000007FFFFFFEF"},
		{70, true, false, true, "Packet70Bed Java oracle golden round trip", "4602"},
		{71, true, false, true, "Packet71Weather Java oracle golden round trip", "47010203041F00000001FFFFFFEF00000041"},
		{100, true, false, true, "Packet100OpenWindow Java oracle golden round trip", "640201000C4F7261636C652043686573741B"},
		{101, true, true, true, "Packet101CloseWindow Java oracle golden round trip", "6502"},
		{102, false, true, true, "Packet102WindowClick Java oracle golden round trip", "6602001101012C010108030007"},
		{103, true, false, true, "Packet103SetSlot Java oracle golden round trip", "670200110108030007"},
		{104, true, false, true, "Packet104WindowItems Java oracle golden round trip", "680200030108030007FFFF01090C0001"},
		{105, true, false, true, "Packet105UpdateProgressbar Java oracle golden round trip", "6902000100C8"},
		{106, true, true, true, "Packet106Transaction Java oracle golden round trip", "6A02012C01"},
		{130, true, true, true, "Packet130UpdateSign Java oracle golden round trip", "82FFFFFFEF00410000001F0006004C0069006E006500200031000800A70061004C0069006E0065002000320006004C0069006E0065002000330006004C0069006E006500200034"},
		{131, true, false, true, "Packet131MapData Java oracle golden round trip", "830166000705010203FEFF"},
		{200, true, false, true, "Packet200Statistic Java oracle golden round trip", "C8000003E82A"},
		{255, true, true, true, "Packet255KickDisconnect Java oracle golden round trip", "FF0011004F007200610063006C006500200064006900730063006F006E006E006500630074"}
	};
	ok &= expect(sizeof(packetOracle) / sizeof(packetOracle[0]) == 57,
		"Java packet oracle contains exactly 57 packet records");
	for (const PacketOracleCase &oracle : packetOracle)
	{
		if (oracle.id == 51)
			continue;
		Bytes wire = bytesFromHex(oracle.hex);
		auto verifyDirection = [&](bool serverHandler, bool allowed)
		{
			try
			{
				auto packet = decode(wire, serverHandler);
				if (!allowed)
					return expect(false, oracle.message);
				return expect(packet && packet->getPacketId() == oracle.id &&
					(!oracle.roundTrip || encode(*packet) == wire),
					oracle.message);
			}
			catch (const std::runtime_error &)
			{
				return expect(!allowed, oracle.message);
			}
		};
		ok &= verifyDirection(false, oracle.clientbound);
		ok &= verifyDirection(true, oracle.serverbound);
	}

	{
		auto watcher = std::make_shared<DataWatcher>();
		watcher->addObject(0, static_cast<byte_t>(0x12));
		watcher->addObject(1, static_cast<short_t>(0x2345));
		watcher->addObject(2, static_cast<int_t>(0x3456789a));
		watcher->addObject(3, 12.5f);
		watcher->addObject(4, jstring(u"oracle-meta"));
		watcher->addObject(5, ItemInstance(264, 3, 7));
		watcher->addObject(6, TilePos(7, 65, -9));
		Packet24MobSpawn packet;
		packet.entityId = 0x01020304;
		packet.type = 50;
		packet.xPosition = 1234;
		packet.yPosition = 2345;
		packet.zPosition = -3456;
		packet.yaw = 96;
		packet.pitch = -24;
		packet.metaData = watcher;
		watcher.reset();
		ok &= expectBytes(encode(packet), bytesFromHex(
			"180102030432000004D200000929FFFFF28060E80012212345423456789A6341480000"
			"84000B006F007200610063006C0065002D006D006500740061A50108030007C6000000"
			"0700000041FFFFFFF77F"), "Packet24MobSpawn Java oracle golden bytes");
	}

	{
		Packet51MapChunk packet;
		packet.xPosition = 17;
		packet.yPosition = 32;
		packet.zPosition = -23;
		packet.xSize = 2;
		packet.ySize = 3;
		packet.zSize = 4;
		packet.chunkSize = 5;
		packet.chunk = {1, 2, 3, 4, 5};
		ok &= expectBytes(encode(packet), bytesFromHex("33000000110020FFFFFFE9010203000000050102030405"),
			"Packet51MapChunk Java oracle golden bytes");
	}

	{
		Bytes wire = bytesFromHex(
			"33000000110020FFFFFFE901010000000012789C63646266616563E7E0E4020000E60038");
		auto decodedBase = decode(wire, false);
		auto *packet = dynamic_cast<Packet51MapChunk *>(decodedBase.get());
		ok &= expect(packet && packet->xSize == 2 && packet->ySize == 2 && packet->zSize == 1 &&
			packet->chunk == std::vector<byte_t>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10}),
			"Packet51MapChunk focused compressed decode");
		bool rejected = false;
		try
		{
			decode(wire, true);
		}
		catch (const std::runtime_error &)
		{
			rejected = true;
		}
		ok &= expect(rejected, "Packet51MapChunk rejects the serverbound direction");
	}

	{
		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		output.writeByte(-1);
		output.writeShort(-2);
		output.writeInt(0x01020304);
		output.writeInt(-2);
		output.writeLong(0x0102030405060708LL);
		output.writeFloat(1.0f);
		output.writeDouble(-2.0);
		ok &= expectBytes(bytesFrom(stream.str()), {
			0xff, 0xff, 0xfe, 0x01, 0x02, 0x03, 0x04, 0xff, 0xff, 0xff, 0xfe,
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
			0x3f, 0x80, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		}, "Java big-endian primitive encoding");
	}

	{
		int_t sourceFloatBits = 0x7fa12345;
		float sourceFloat;
		std::memcpy(&sourceFloat, &sourceFloatBits, sizeof(sourceFloat));
		long_t sourceDoubleBits = 0x7ff123456789abcdLL;
		double sourceDouble;
		std::memcpy(&sourceDouble, &sourceDoubleBits, sizeof(sourceDouble));

		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		output.writeFloat(sourceFloat);
		output.writeDouble(sourceDouble);
		ok &= expectBytes(bytesFrom(stream.str()), {
			0x7f, 0xc0, 0x00, 0x00, 0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
		}, "DataOutputStream canonical NaN encoding");
	}

	const jstring unicode = {u'A', 0, 0x00e9, 0xd83d, 0xde00};
	{
		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		Packet::writeString(unicode, output);
		Bytes encoded = bytesFrom(stream.str());
		ok &= expectBytes(encoded, {
			0x00, 0x05, 0x00, 0x41, 0x00, 0x00, 0x00, 0xe9, 0xd8, 0x3d, 0xde, 0x00
		}, "packet UTF-16BE string encoding");
		std::istringstream inputStream(stream.str(), std::ios::binary);
		PacketDataInput input(inputStream);
		ok &= expect(Packet::readString(input, 5) == unicode, "packet UTF-16BE string decoding");
	}

	{
		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		output.writeUTF(unicode);
		ok &= expectBytes(bytesFrom(stream.str()), {
			0x00, 0x0b, 0x41, 0xc0, 0x80, 0xc3, 0xa9,
			0xed, 0xa0, 0xbd, 0xed, 0xb8, 0x80
		}, "Java modified UTF-8 encoding");
		std::istringstream inputStream(stream.str(), std::ios::binary);
		PacketDataInput input(inputStream);
		ok &= expect(input.readUTF() == unicode, "Java modified UTF-8 decoding");
	}

	{
		Packet1Login packet(u"Az", 14);
		packet.mapSeed = 0x0102030405060708LL;
		packet.dimension = -1;
		ok &= expectBytes(encode(packet), {
			0x01, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x41, 0x00, 0x7a,
			0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0xff
		}, "Packet1Login golden bytes");
		ok &= expect(packet.getPacketSize() == 15, "Packet1Login preserves stale declared size");
		auto decodedBase = decode(encode(packet), true);
		auto *decoded = dynamic_cast<Packet1Login *>(decodedBase.get());
		ok &= expect(decoded && decoded->protocolVersion == 14 && decoded->username == u"Az" &&
			decoded->mapSeed == packet.mapSeed && decoded->dimension == -1,
			"Packet1Login round trip");
	}

	{
		Packet3Chat packet(jstring(120, u'x'));
		ok &= expect(packet.message.size() == 119, "Packet3Chat constructor truncates to 119 code units");
		InspectableHandler handler;
		packet.processPacket(handler);
		ok &= expect(handler.chat == &packet, "Packet3Chat dispatches handleChat");
	}

	{
		Packet15Place packet(0x01020304, 255, -2, 255, nullptr);
		ok &= expectBytes(encode(packet), {
			0x0f, 0x01, 0x02, 0x03, 0x04, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff
		}, "Packet15Place null stack golden bytes");
		ItemInstance item(300, -3, 0x1234);
		Packet15Place present(0x01020304, 255, -2, 255, &item);
		ok &= expectBytes(encode(present), {
			0x0f, 0x01, 0x02, 0x03, 0x04, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
			0x01, 0x2c, 0xfd, 0x12, 0x34
		}, "Packet15Place present stack golden bytes");
		item.stackSize = -2;
		item.itemID = 301;
		item.itemDamage = 0x1235;
		item = ItemInstance();
		ok &= expectBytes(encode(present), {
			0x0f, 0x01, 0x02, 0x03, 0x04, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
			0x01, 0x2d, 0xfe, 0x12, 0x35
		}, "Packet15Place retains the replaced Java ItemStack identity");
	}

	{
		auto item = std::make_unique<ItemInstance>(264, 3, 7);
		Packet102WindowClick packet(2, 17, 1, true, item.get(), 300);
		item->stackSize = 4;
		item.reset();
		ok &= expectBytes(encode(packet), bytesFromHex("6602001101012C010108040007"),
			"Packet102WindowClick retains mutable ItemStack reference and lifetime");
	}

	{
		Packet100OpenWindow packet;
		packet.windowId = -1;
		packet.inventoryType = 2;
		packet.windowTitle = {u'A', 0, 0x00e9};
		packet.slotsCount = 27;
		ok &= expectBytes(encode(packet), {
			0x64, 0xff, 0x02, 0x00, 0x05, 0x41, 0xc0, 0x80, 0xc3, 0xa9, 0x1b
		}, "Packet100OpenWindow modified UTF golden bytes");
	}

	{
		Packet104WindowItems packet;
		packet.windowId = -1;
		packet.itemStack.resize(2);
		packet.itemStack[1] = std::make_unique<ItemInstance>(300, 2, 3);
		ok &= expectBytes(encode(packet), {
			0x68, 0xff, 0x00, 0x02, 0xff, 0xff, 0x01, 0x2c, 0x02, 0x00, 0x03
		}, "Packet104WindowItems mixed stack golden bytes");
		ok &= expect(packet.getPacketSize() == 13, "Packet104WindowItems declared size counts null as five bytes");
	}

	{
		Packet200Statistic packet;
		packet.field_27052_a = 0x01020304;
		packet.field_27051_b = -1;
		ok &= expectBytes(encode(packet), {0xc8, 0x01, 0x02, 0x03, 0x04, 0xff},
			"Packet200Statistic golden bytes");
		ok &= expect(packet.getPacketSize() == 6, "Packet200Statistic preserves stale declared size");
	}

	{
		Packet50PreChunk packet;
		packet.xPosition = 0x01020304;
		packet.yPosition = -2;
		packet.mode = true;
		ok &= expectBytes(encode(packet), {
			0x32, 0x01, 0x02, 0x03, 0x04, 0xff, 0xff, 0xff, 0xfe, 0x01
		}, "Packet50PreChunk golden bytes");
	}

	{
		Packet52MultiBlockChange packet;
		packet.xPosition = 1;
		packet.zPosition = -1;
		packet.size = 2;
		packet.coordinateArray = {0x1234, -2};
		packet.typeArray = {7, static_cast<byte_t>(-1)};
		packet.metadataArray = {3, 4};
		ok &= expectBytes(encode(packet), {
			0x34, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00, 0x02,
			0x12, 0x34, 0xff, 0xfe, 0x07, 0xff, 0x03, 0x04
		}, "Packet52MultiBlockChange golden bytes");
	}

	{
		Packet53BlockChange packet;
		packet.xPosition = 1;
		packet.yPosition = 255;
		packet.zPosition = -2;
		packet.type = 200;
		packet.metadata = 15;
		ok &= expectBytes(encode(packet), {
			0x35, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xc8, 0x0f
		}, "Packet53BlockChange golden bytes");
	}

	{
		Packet54PlayNoteBlock packet;
		packet.xLocation = 1;
		packet.yLocation = -2;
		packet.zLocation = 3;
		packet.instrumentType = 255;
		packet.pitch = 128;
		ok &= expectBytes(encode(packet), {
			0x36, 0x00, 0x00, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x03, 0xff, 0x80
		}, "Packet54PlayNoteBlock golden bytes");
	}

	{
		Packet60Explosion packet;
		packet.explosionX = 100.25;
		packet.explosionY = 64.5;
		packet.explosionZ = -20.75;
		packet.explosionSize = 4.5f;
		packet.destroyedBlockPositions.emplace(101, 65, -19);
		packet.destroyedBlockPositions.emplace(98, 63, -22);
		packet.destroyedBlockPositions.emplace(104, 68, -20);
		ok &= expectBytes(encode(packet), bytesFromHex(
			"3C40591000000000004050200000000000C034C000000000004090000000000003"
			"FEFFFE010101040400"), "Packet60Explosion Java oracle golden bytes");
	}

	{
		Bytes wire = bytesFromHex(
			"3C40591000000000004050200000000000C034C000000000004090000000000003"
			"FEFFFE010101040400");
		auto decodedBase = decode(wire, false);
		auto *packet = dynamic_cast<Packet60Explosion *>(decodedBase.get());
		std::vector<TilePos> positions;
		if (packet)
		{
			for (const TilePos &position : packet->destroyedBlockPositions)
				positions.push_back(position);
		}
		ok &= expect(positions == std::vector<TilePos>({
			TilePos(101, 65, -19), TilePos(98, 63, -22), TilePos(104, 68, -20)}),
			"Packet60Explosion focused Java 6 HashSet decode order");
		bool rejected = false;
		try
		{
			decode(wire, true);
		}
		catch (const std::runtime_error &)
		{
			rejected = true;
		}
		ok &= expect(rejected, "Packet60Explosion rejects the serverbound direction");
	}

	{
		Packet60Explosion packet;
		packet.explosionX = 1.0;
		packet.explosionY = -2.0;
		packet.explosionZ = 3.5;
		packet.explosionSize = 4.0f;
		packet.destroyedBlockPositions.emplace(2, -3, 5);
		ok &= expectBytes(encode(packet), {
			0x3c,
			0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x40, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x40, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0xff, 0x02
		}, "Packet60Explosion golden bytes");
	}

	{
		Packet60Explosion packet;
		packet.explosionX = std::numeric_limits<double>::quiet_NaN();
		packet.explosionY = std::numeric_limits<double>::infinity();
		packet.explosionZ = -std::numeric_limits<double>::infinity();
		packet.destroyedBlockPositions.emplace(1, 0, 0);
		ok &= expectBytes(encode(packet), {
			0x3c,
			0x7f, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x7f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00
		}, "Packet60Explosion preserves Java narrowing and wrapped offsets");
	}

	{
		Packet61DoorChange packet;
		packet.field_28050_a = 1;
		packet.field_28053_c = 2;
		packet.field_28052_d = -3;
		packet.field_28051_e = 4;
		packet.field_28049_b = 5;
		ok &= expectBytes(encode(packet), {
			0x3d, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0xfd,
			0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05
		}, "Packet61DoorChange golden bytes");
		ok &= expect(packet.getPacketSize() == 20, "Packet61DoorChange preserves stale declared size");
	}

	{
		Packet70Bed packet;
		packet.field_25019_b = -1;
		ok &= expectBytes(encode(packet), {0x46, 0xff}, "Packet70Bed golden bytes");
		ok &= expect(Packet70Bed::field_25020_a[0] && *Packet70Bed::field_25020_a[0] == u"tile.bed.notValid" &&
			Packet70Bed::field_25020_a[1] == nullptr && Packet70Bed::field_25020_a[2] == nullptr,
			"Packet70Bed preserves Beta message table");
	}

	{
		Packet71Weather packet;
		packet.field_27054_a = 1;
		packet.field_27055_e = -2;
		packet.field_27053_b = 3;
		packet.field_27057_c = 4;
		packet.field_27056_d = 5;
		ok &= expectBytes(encode(packet), {
			0x47, 0x00, 0x00, 0x00, 0x01, 0xfe, 0x00, 0x00, 0x00, 0x03,
			0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05
		}, "Packet71Weather golden bytes");
	}

	{
		auto signLines = std::make_shared<std::array<jstring, 4>>(
			std::array<jstring, 4>{{u"a", u"b", u"c", u"d"}});
		Packet130UpdateSign packet(1, -2, 3, signLines);
		signLines->at(0) = u"z";
		signLines.reset();
		ok &= expectBytes(encode(packet), {
			0x82, 0x00, 0x00, 0x00, 0x01, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x03,
			0x00, 0x01, 0x00, 0x7a, 0x00, 0x01, 0x00, 0x62,
			0x00, 0x01, 0x00, 0x63, 0x00, 0x01, 0x00, 0x64
		}, "Packet130UpdateSign golden bytes");
		ok &= expect(packet.getPacketSize() == 4, "Packet130UpdateSign preserves stale declared size");
	}

	{
		Packet131MapData packet;
		packet.field_28055_a = 0x1234;
		packet.field_28054_b = -2;
		packet.field_28056_c = {0, 127, static_cast<byte_t>(-1)};
		ok &= expectBytes(encode(packet), {
			0x83, 0x12, 0x34, 0xff, 0xfe, 0x03, 0x00, 0x7f, 0xff
		}, "Packet131MapData golden bytes");
		ok &= expect(packet.getPacketSize() == 7, "Packet131MapData preserves stale declared size");
	}

	{
		WatchableObjectList objects;
		objects.push_back(std::make_shared<WatchableObject>(0, 0, static_cast<byte_t>(-2)));
		objects.push_back(std::make_shared<WatchableObject>(1, 1, static_cast<short_t>(-3)));
		objects.push_back(std::make_shared<WatchableObject>(2, 2, 0x01020304));
		objects.push_back(std::make_shared<WatchableObject>(3, 3, 1.0f));
		objects.push_back(std::make_shared<WatchableObject>(4, 4, jstring(u"A")));
		objects.push_back(std::make_shared<WatchableObject>(5, 5, ItemInstance(300, 2, 3)));
		objects.push_back(std::make_shared<WatchableObject>(6, 6, TilePos(1, -2, 3)));
		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		DataWatcher::writeObjectsInListToStream(&objects, output);
		ok &= expectBytes(bytesFrom(stream.str()), {
			0x00, 0xfe, 0x21, 0xff, 0xfd, 0x42, 0x01, 0x02, 0x03, 0x04,
			0x63, 0x3f, 0x80, 0x00, 0x00, 0x84, 0x00, 0x01, 0x00, 0x41,
			0xa5, 0x01, 0x2c, 0x02, 0x00, 0x03,
			0xc6, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x03,
			0x7f
		}, "DataWatcher types 0 through 6 golden bytes");

		std::istringstream inputStream(stream.str(), std::ios::binary);
		PacketDataInput input(inputStream);
		auto decoded = DataWatcher::readWatchableObjects(input);
		ok &= expect(decoded && decoded->size() == 7 && decoded->at(0)->getByte() == -2 &&
			decoded->at(1)->getShort() == -3 && decoded->at(2)->getInt() == 0x01020304 &&
			decoded->at(3)->getFloat() == 1.0f && decoded->at(4)->getString() == u"A" &&
			decoded->at(5)->getItem().itemID == 300 && decoded->at(6)->getCoordinates() == TilePos(1, -2, 3),
			"DataWatcher types 0 through 6 decode");
	}

	{
		DataWatcher watcher;
		auto item = std::make_shared<ItemInstance>(264, 3, 7);
		auto coordinates = std::make_shared<TilePos>(1, 2, 3);
		watcher.addObject(5, item);
		watcher.addObject(6, coordinates);
		item->stackSize = 4;
		item->itemDamage = 8;
		coordinates->x = -1;
		item.reset();
		coordinates.reset();
		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		watcher.writeWatchableObjects(output);
		ok &= expectBytes(bytesFrom(stream.str()), {
			0xa5, 0x01, 0x08, 0x04, 0x00, 0x08,
			0xc6, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02,
			0x00, 0x00, 0x00, 0x03, 0x7f
		}, "DataWatcher retains mutable ItemStack and ChunkCoordinates references");

		DataWatcher invalidItem;
		invalidItem.addObject(5, ItemInstance(31999, 1, 0));
		bool rejected = false;
		try
		{
			std::ostringstream invalidStream(std::ios::binary);
			PacketDataOutput invalidOutput(invalidStream);
			invalidItem.writeWatchableObjects(invalidOutput);
		}
		catch (const std::runtime_error &)
		{
			rejected = true;
		}
		ok &= expect(rejected, "DataWatcher type 5 resolves ItemStack.getItem before writing");

		DataWatcher wrongType;
		wrongType.addObject(0, static_cast<byte_t>(1));
		wrongType.updateObject(0, jstring(u"wrong"));
		rejected = false;
		try
		{
			std::ostringstream invalidStream(std::ios::binary);
			PacketDataOutput invalidOutput(invalidStream);
			wrongType.writeWatchableObjects(invalidOutput);
		}
		catch (const std::runtime_error &)
		{
			rejected = true;
		}
		ok &= expect(rejected, "DataWatcher preserves Java wrong-runtime-type cast failure");
	}

	{
		std::string encoded{static_cast<char>(0xe0), static_cast<char>(0x7f)};
		std::istringstream stream(encoded, std::ios::binary);
		PacketDataInput input(stream);
		auto decoded = DataWatcher::readWatchableObjects(input);
		ok &= expect(decoded && decoded->size() == 1 && !decoded->at(0),
			"DataWatcher preserves unknown type-7 null entries");

		std::istringstream emptyStream(std::string(1, static_cast<char>(0x7f)), std::ios::binary);
		PacketDataInput emptyInput(emptyStream);
		ok &= expect(!DataWatcher::readWatchableObjects(emptyInput),
			"DataWatcher terminator-only stream returns null list");
	}

	{
		DataWatcher watcher;
		watcher.addObject(0, static_cast<byte_t>(0));
		watcher.addObject(16, static_cast<byte_t>(0));
		watcher.addObject(17, jstring());
		watcher.addObject(18, 0);
		std::ostringstream stream(std::ios::binary);
		PacketDataOutput output(stream);
		watcher.writeWatchableObjects(output);
		ok &= expectBytes(bytesFrom(stream.str()), {
			0x91, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x52, 0x00, 0x00, 0x00, 0x00, 0x7f
		}, "DataWatcher writes Wolf metadata in Java 6 HashMap iteration order");
	}

	{
		Packet20NamedEntitySpawn packet;
		packet.entityId = 0x01020304;
		packet.name = u"A";
		packet.xPosition = -1;
		packet.yPosition = 2;
		packet.zPosition = -3;
		packet.rotation = -4;
		packet.pitch = 5;
		packet.currentItem = 300;
		ok &= expectBytes(encode(packet), {
			0x14, 0x01, 0x02, 0x03, 0x04, 0x00, 0x01, 0x00, 0x41,
			0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0xfd,
			0xfc, 0x05, 0x01, 0x2c
		}, "Packet20NamedEntitySpawn golden bytes");
	}

	{
		Packet21PickupSpawn packet;
		packet.entityId = 1;
		packet.itemID = 300;
		packet.count = -2;
		packet.itemDamage = 0x1234;
		packet.xPosition = 2;
		packet.yPosition = 3;
		packet.zPosition = 4;
		packet.rotation = -1;
		packet.pitch = 2;
		packet.roll = -3;
		ok &= expectBytes(encode(packet), {
			0x15, 0x00, 0x00, 0x00, 0x01, 0x01, 0x2c, 0xfe, 0x12, 0x34,
			0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
			0xff, 0x02, 0xfd
		}, "Packet21PickupSpawn golden bytes");
	}

	{
		Packet22Collect packet;
		packet.collectedEntityId = 1;
		packet.collectorEntityId = -2;
		ok &= expectBytes(encode(packet), {
			0x16, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfe
		}, "Packet22Collect golden bytes");
	}

	{
		Packet23VehicleSpawn packet;
		packet.entityId = 1;
		packet.type = 63;
		packet.xPosition = 2;
		packet.yPosition = 3;
		packet.zPosition = 4;
		packet.field_28044_i = 5;
		packet.field_28047_e = -1;
		packet.field_28046_f = 2;
		packet.field_28045_g = -3;
		ok &= expectBytes(encode(packet), {
			0x17, 0x00, 0x00, 0x00, 0x01, 0x3f,
			0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
			0x00, 0x00, 0x00, 0x05, 0xff, 0xff, 0x00, 0x02, 0xff, 0xfd
		}, "Packet23VehicleSpawn golden bytes");
		ok &= expect(packet.getPacketSize() == 6, "Packet23VehicleSpawn preserves precedence-broken size");
		packet.field_28044_i = -30;
		ok &= expect(packet.getPacketSize() == 0, "Packet23VehicleSpawn precedence size can return zero");
		packet.field_28044_i = std::numeric_limits<int_t>::max();
		ok &= expect(packet.getPacketSize() == 0,
			"Packet23VehicleSpawn size preserves Java signed overflow");
	}

	{
		Bytes wire = {
			0x18, 0x00, 0x00, 0x00, 0x01, 0x32,
			0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04,
			0xff, 0x02, 0x00, 0x05, 0x41, 0x01, 0x02, 0x03, 0x04, 0x7f
		};
		auto decodedBase = decode(wire, false);
		auto *packet = dynamic_cast<Packet24MobSpawn *>(decodedBase.get());
		WatchableObjectList *metadata = packet ? packet->getMetadata() : nullptr;
		ok &= expect(packet && packet->entityId == 1 && packet->type == 50 && packet->yaw == -1 &&
			metadata && metadata->size() == 2 && metadata->at(0)->getByte() == 5 &&
			metadata->at(1)->getInt() == 0x01020304,
			"Packet24MobSpawn metadata decode");
	}

	{
		Packet25EntityPainting packet;
		packet.entityId = 1;
		packet.title = u"A";
		packet.xPosition = 2;
		packet.yPosition = 3;
		packet.zPosition = 4;
		packet.direction = -1;
		ok &= expectBytes(encode(packet), {
			0x19, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x41,
			0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03,
			0x00, 0x00, 0x00, 0x04, 0xff, 0xff, 0xff, 0xff
		}, "Packet25EntityPainting golden bytes");
		NetworkSmokeLevel level;
		EntityPainting painting(level, 2, 3, 4, 1, PaintingArt::kebab().title);
		painting.entityId = 7;
		Packet25EntityPainting copied(painting);
		ok &= expect(copied.entityId == 7 && copied.xPosition == 2 && copied.yPosition == 3 &&
			copied.zPosition == 4 && copied.direction == 1 &&
			copied.title == PaintingArt::kebab().title,
			"Packet25EntityPainting constructor copies the Java fields");
	}

	{
		ok &= expect(Mth::floor(std::numeric_limits<double>::quiet_NaN()) == 0 &&
			Mth::floor(std::numeric_limits<double>::infinity()) == std::numeric_limits<int_t>::max() &&
			Mth::floor(-std::numeric_limits<double>::infinity()) == std::numeric_limits<int_t>::max() &&
			Mth::floor(static_cast<double>(std::numeric_limits<int_t>::min())) ==
				std::numeric_limits<int_t>::min() &&
			Mth::floor(static_cast<double>(std::numeric_limits<int_t>::min()) - 1.0) ==
				std::numeric_limits<int_t>::max() && Mth::floor(-1.25) == -2,
			"Mth double floor preserves Java narrowing and wrap edges");
		ok &= expect(Mth::floor(std::numeric_limits<float>::quiet_NaN()) == 0 &&
			Mth::floor(std::numeric_limits<float>::infinity()) == std::numeric_limits<int_t>::max() &&
			Mth::floor(-std::numeric_limits<float>::infinity()) == std::numeric_limits<int_t>::max(),
			"Mth float floor preserves Java narrowing and wrap edges");
	}

	{
		Bytes wire = {
			0x1b, 0x3f, 0x80, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00,
			0x40, 0x40, 0x00, 0x00, 0x40, 0x80, 0x00, 0x00, 0x01, 0x00
		};
		auto packet = decode(wire, true);
		ok &= expect(packet && encode(*packet) == wire, "Packet27Position golden round trip");
	}

	{
		Packet28EntityVelocity packet(1, 4.5, -4.5, std::numeric_limits<double>::quiet_NaN());
		ok &= expectBytes(encode(packet), {
			0x1c, 0x00, 0x00, 0x00, 0x01, 0x79, 0xe0, 0x86, 0x20, 0x00, 0x00
		}, "Packet28EntityVelocity clamp and NaN golden bytes");
	}

	{
		Packet29DestroyEntity packet;
		packet.entityId = -2;
		ok &= expectBytes(encode(packet), {0x1d, 0xff, 0xff, 0xff, 0xfe},
			"Packet29DestroyEntity golden bytes");
	}

	{
		Packet30Entity packet;
		packet.entityId = 1;
		ok &= expectBytes(encode(packet), {0x1e, 0x00, 0x00, 0x00, 0x01},
			"Packet30Entity golden bytes");
		Packet31RelEntityMove move;
		move.entityId = 1;
		move.xPosition = -1;
		move.yPosition = 2;
		move.zPosition = -3;
		ok &= expectBytes(encode(move), {0x1f, 0x00, 0x00, 0x00, 0x01, 0xff, 0x02, 0xfd},
			"Packet31RelEntityMove golden bytes");
		Packet32EntityLook look;
		look.entityId = 1;
		look.yaw = -1;
		look.pitch = 2;
		ok &= expectBytes(encode(look), {0x20, 0x00, 0x00, 0x00, 0x01, 0xff, 0x02},
			"Packet32EntityLook golden bytes");
		Packet33RelEntityMoveLook both;
		both.entityId = 1;
		both.xPosition = -1;
		both.yPosition = 2;
		both.zPosition = -3;
		both.yaw = 4;
		both.pitch = -5;
		ok &= expectBytes(encode(both), {
			0x21, 0x00, 0x00, 0x00, 0x01, 0xff, 0x02, 0xfd, 0x04, 0xfb
		}, "Packet33RelEntityMoveLook golden bytes");
	}

	{
		Packet34EntityTeleport packet;
		packet.entityId = 1;
		packet.xPosition = 2;
		packet.yPosition = 3;
		packet.zPosition = 4;
		packet.yaw = -1;
		packet.pitch = 2;
		ok &= expectBytes(encode(packet), {
			0x22, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02,
			0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0xff, 0x02
		}, "Packet34EntityTeleport golden bytes");
		ok &= expect(packet.getPacketSize() == 34, "Packet34EntityTeleport preserves stale declared size");
	}

	{
		Packet38EntityStatus status;
		status.entityId = 1;
		status.entityStatus = -2;
		ok &= expectBytes(encode(status), {0x26, 0x00, 0x00, 0x00, 0x01, 0xfe},
			"Packet38EntityStatus golden bytes");
		Packet39AttachEntity attach;
		attach.entityId = 1;
		attach.vehicleEntityId = -2;
		ok &= expectBytes(encode(attach), {
			0x27, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xfe
		}, "Packet39AttachEntity golden bytes");
	}

	{
		Bytes wire = {0x28, 0x00, 0x00, 0x00, 0x01, 0x00, 0xfe, 0x7f};
		auto decodedBase = decode(wire, false);
		auto *packet = dynamic_cast<Packet40EntityMetadata *>(decodedBase.get());
		WatchableObjectList *metadata = packet ? packet->func_21047_b() : nullptr;
		ok &= expect(packet && metadata && metadata->size() == 1 && metadata->at(0)->getByte() == -2,
			"Packet40EntityMetadata golden decode");
		ok &= expect(packet && packet->getPacketSize() == 5,
			"Packet40EntityMetadata preserves stale declared size");
	}

	{
		const std::vector<int_t> expectedIds = {
			0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
			20, 21, 22, 23, 24, 25, 27, 28, 29, 30, 31, 32, 33, 34, 38, 39, 40,
			50, 51, 52, 53, 54, 60, 61, 70, 71,
			100, 101, 102, 103, 104, 105, 106, 130, 131, 200, 255
		};
		std::size_t registered = 0;
		for (int_t id = 0; id <= 255; ++id)
		{
			auto packet = Packet::getNewPacket(id);
			if (packet)
			{
				++registered;
				ok &= expect(packet->getPacketId() == id, "packet factory preserves registered ID");
			}
		}
		ok &= expect(registered == expectedIds.size() + (ClientTarget::isAlphaPlace() ? 2 : 0),
			"packet registry contains exactly 57 Beta IDs plus two only for AlphaPlace");
		ok &= expect((Packet::getNewPacket(62) != nullptr) == ClientTarget::isAlphaPlace() &&
			(Packet::getNewPacket(63) != nullptr) == ClientTarget::isAlphaPlace(),
			"packet IDs 62 and 63 are registered only in the AlphaPlace target");
		for (int_t id : expectedIds)
			ok &= expect(Packet::getNewPacket(id) != nullptr, "expected packet ID is registered");
	}

	{
		Packet13PlayerLookMove packet(1.25, -2.5, 3.75, 4.5, -90.0f, 45.0f, true);
		std::unique_ptr<Packet10Flying> copy = packet.copyForSend();
		ok &= expect(dynamic_cast<Packet13PlayerLookMove *>(copy.get()) != nullptr &&
			copy->getPacketId() == 13 && encode(*copy) == encode(packet),
			"Packet10Flying copy preserves the echoed runtime subtype and fields");
	}

	{
		auto flying = decode({0x0a}, true);
		auto *packet = dynamic_cast<Packet10Flying *>(flying.get());
		ok &= expect(packet && packet->onGround, "Packet10Flying raw EOF byte becomes true like DataInputStream.read");
	}

	{
		bool rejected = false;
		try
		{
			decode({0x04, 0, 0, 0, 0, 0, 0, 0, 0}, true);
		}
		catch (const std::runtime_error &error)
		{
			rejected = std::string(error.what()) == "Bad packet id 4";
		}
		ok &= expect(rejected, "client-only packet is rejected by a server handler");

		rejected = false;
		try
		{
			decode({0x1a}, false);
		}
		catch (const std::runtime_error &error)
		{
			rejected = std::string(error.what()) == "Bad packet id 26";
		}
		ok &= expect(rejected, "unregistered packet ID is rejected");
	}

	{
		bool rejected = false;
		try
		{
			std::ostringstream stream(std::ios::binary);
			PacketDataOutput output(stream);
			Packet::writeString(jstring(32768, u'x'), output);
		}
		catch (const std::runtime_error &error)
		{
			rejected = std::string(error.what()) == "String too big";
		}
		ok &= expect(rejected, "packet strings reject more than Short.MAX_VALUE code units");
	}

	{
		auto previousMessages = SPCCommand::messages;
		bool previousOutputEnabled = SPCCommand::outputEnabled;
		SPCCommand::messages.clear();
		SPCCommand::outputEnabled = false;
		SPCCommand::addMessage(u"hidden command output");
		SPCCommand::addChatMessage(u"visible server chat");
		ok &= expect(SPCCommand::messages.size() == 1
			&& SPCCommand::messages.front().text == u"visible server chat",
			"server and system chat bypasses the single-player command output toggle");
		SPCCommand::messages = std::move(previousMessages);
		SPCCommand::outputEnabled = previousOutputEnabled;
	}

	{
		NetworkSmokeLevel level;
		LevelEventListener listener;
		level.addListener(listener);
		level.levelEvent(2001, -12, 64, 35, 0x1234);
		ok &= expect(listener.player == nullptr && listener.event == 2001 && listener.x == -12 &&
			listener.y == 64 && listener.z == 35 && listener.data == 0x1234,
			"World.func_28106_e dispatches the complete auxiliary level event to listeners");
		level.removeListener(listener);

		LevelChunk chunk(level, 0, 0);
		chunk.heightmap.fill(99);
		chunk.minHeight = 99;
		chunk.unsaved = false;

		const ubyte_t transparentBlocks[] = {
			20, 6, 37, 38, 39, 40, 50, 51,
			55, 59, 63, 65, 66, 68, 69, 70
		};
		std::vector<byte_t> input(24);
		for (int_t i = 0; i < 12; i++)
			input[i] = static_cast<byte_t>(transparentBlocks[i]);
		for (int_t i = 12; i < static_cast<int_t>(input.size()); i++)
			input[i] = static_cast<byte_t>(i - 24);

		int_t offset = chunk.setBlocksAndData(input.data(), 2, 5, 3, 4, 8, 5);
		ok &= expect(offset == 24, "Chunk.setChunkData returns the exact consumed byte offset");

		int_t inputOffset = 0;
		bool blocksMatch = true;
		for (int_t x = 2; x < 4; x++)
		{
			for (int_t z = 3; z < 5; z++)
			{
				for (int_t y = 5; y < 8; y++)
				{
					int_t index = (x << 11) | (z << 7) | y;
					blocksMatch &= chunk.blocks[index] == static_cast<ubyte_t>(input[inputOffset++]);
				}
			}
		}
		ok &= expect(blocksMatch && chunk.blocks[(2 << 11) | (3 << 7) | 4] == 0,
			"Chunk.setChunkData copies block columns in Java x/z/y order and honors half-open bounds");

		DataLayer *layers[] = {&chunk.data, &chunk.blockLight, &chunk.skyLight};
		bool layersMatch = true;
		for (DataLayer *layer : layers)
		{
			for (int_t x = 2; x < 4; x++)
			{
				for (int_t z = 3; z < 5; z++)
				{
					int_t index = ((x << 11) | (z << 7) | 5) >> 1;
					for (int_t i = 0; i < 1; i++)
						layersMatch &= layer->data[index + i] == input[inputOffset++];
				}
			}
		}
		ok &= expect(layersMatch && inputOffset == offset,
			"Chunk.setChunkData copies metadata, block light, then sky light without repacking and truncates odd nibble heights");
		ok &= expect(chunk.minHeight == 0 && chunk.unsaved &&
			std::all_of(chunk.heightmap.begin(), chunk.heightmap.end(), [](ubyte_t height) { return height == 0; }),
			"Chunk.setChunkData regenerates the complete height map immediately after block bytes");
	}

	{
		NetworkSmokeLevel level;
		Player player(level);
		player.name = u"SkinOwner";
		player.prepareCustomTextures();
		ok &= expect(player.cloakTexture == u"http://s3.amazonaws.com/MinecraftCloaks/SkinOwner.png",
			"EntityPlayer.updateCloak derives the Beta cloak URL from the username");
		ok &= expect(player.customTextureUrl2 == player.cloakTexture,
			"EntityPlayer.updateCloak publishes playerCloakUrl through Entity.cloakUrl");
		auto playerMenu = std::dynamic_pointer_cast<ContainerPlayer>(player.inventorySlots);
		ok &= expect(playerMenu != nullptr && player.craftingInventory == player.inventorySlots
			&& playerMenu->windowId == 0 && playerMenu->slots.size() == 45,
			"EntityPlayer starts with the exact 45-slot ContainerPlayer as both window 0 menus");

		std::vector<std::unique_ptr<ItemInstance>> playerItems(45);
		playerItems[36] = std::make_unique<ItemInstance>(1, 3, 0);
		playerMenu->putStacksInSlots(playerItems);
		ok &= expect(player.inventory.mainInventory[0].itemID == 1
			&& player.inventory.mainInventory[0].stackSize == 3,
			"window 0 bulk content maps protocol slot 36 to hotbar slot 0");

		player.inventory.setCarried(ItemInstance(2, 5, 0));
		ok &= expect(player.inventory.getCarried() != nullptr
			&& player.inventory.getCarried()->stackSize == 5,
			"window -1 carried-stack state is retained by InventoryPlayer");

		short_t action = playerMenu->getNextTransactionId(player.inventory);
		for (int_t i = 1; i < 32768; ++i)
			action = playerMenu->getNextTransactionId(player.inventory);
		ok &= expect(action == std::numeric_limits<short_t>::min(),
			"container transaction IDs increment and wrap with Java short semantics");
		playerMenu->transactionRejected(action);
		playerMenu->transactionAccepted(action);

		auto chest = std::make_shared<BasicInventory>(u"Server Chest", 27);
		auto chestMenu = std::make_shared<ContainerChest>(player.inventory, chest);
		chestMenu->windowId = 7;
		std::vector<std::unique_ptr<ItemInstance>> chestItems(63);
		chestItems[0] = std::make_unique<ItemInstance>(4, 8, 0);
		chestItems[27] = std::make_unique<ItemInstance>(5, 9, 0);
		chestItems[54] = std::make_unique<ItemInstance>(6, 10, 0);
		chestMenu->putStacksInSlots(chestItems);
		ok &= expect(chestMenu->slots.size() == 63 && chest->getItem(0).itemID == 4
			&& player.inventory.mainInventory[9].itemID == 5
			&& player.inventory.mainInventory[0].itemID == 6,
			"chest bulk content preserves chest, main-inventory, and hotbar slot ordering");

		auto furnace = std::make_shared<FurnaceTileEntity>();
		auto furnaceMenu = std::make_shared<ContainerFurnace>(player.inventory, furnace);
		furnaceMenu->updateProgressBar(0, 123);
		furnaceMenu->updateProgressBar(1, 456);
		furnaceMenu->updateProgressBar(2, 789);
		ok &= expect(furnaceMenu->slots.size() == 39 && furnace->cookTime == 123
			&& furnace->burnTime == 456 && furnace->currentItemBurnTime == 789,
			"furnace window progress IDs update the exact three client fields");

		auto dispenser = std::make_shared<DispenserTileEntity>();
		ContainerDispenser dispenserMenu(player.inventory, dispenser);
		ContainerWorkbench workbenchMenu(player.inventory, level, 0, 0, 0);
		ok &= expect(dispenserMenu.slots.size() == 45 && workbenchMenu.slots.size() == 46,
			"dispenser and workbench menus expose the exact Beta 1.7.3 slot counts");
	}

	{
		NetworkSmokeLevel level;
		auto player = std::make_shared<Player>(level);
		level.entities.push_back(player);
		level.players.push_back(player);
		level.players.push_back(player);
		level.updateEntityList();
		level.removeEntity(player);
		ok &= expect(player->removed && std::find(level.entities.begin(), level.entities.end(), player) != level.entities.end()
			&& level.players.size() == 1,
			"setEntityDead removes one Java-list player entry without immediately unlinking the entity");
		level.updateEntityList();
		ok &= expect(std::find(level.entities.begin(), level.entities.end(), player) == level.entities.end(),
			"updateEntityList unlinks a dead player on the next explicit entity-list flush");
	}

	{
		Minecraft minecraft(1, 1, false);
		GameMode gameMode(minecraft);
		NetworkSmokeLevel level;
		Player player(level);
		auto workbenchMenu = std::make_shared<ContainerWorkbench>(player.inventory, level, 0, 0, 0);
		workbenchMenu->windowId = 3;
		player.craftingInventory = workbenchMenu;
		gameMode.closeContainer(workbenchMenu->windowId, player);
		ok &= expect(player.craftingInventory == player.inventorySlots,
			"singleplayer controller close runs Container.onClosed and restores window 0");

		level.isOnline = true;
		auto onlineWorkbench = std::make_shared<ContainerWorkbench>(player.inventory, level, 0, 0, 0);
		onlineWorkbench->craftMatrix.setInventorySlotContents(0, ItemInstance(1, 1, 0));
		onlineWorkbench->onClosed(player);
		ok &= expect(onlineWorkbench->craftMatrix.getStackInSlot(0) != nullptr,
			"online workbench close preserves server-owned crafting inputs");
	}

	{
		Minecraft minecraft(1, 1, false);
		auto level = std::make_shared<AlphaPlaceSmokeLevel>();
		minecraft.level = level;
		Player player(*level);
		GameRenderer &renderer = minecraft.gameRenderer;
		minecraft.levelRenderer.destroyProgress = 0.35f;
		Packet63Digging packet;
		packet.x = 2;
		packet.y = 64;
		packet.z = 3;
		packet.face = 1;
		packet.progress = 0.25f;
		packet.timestamp = 10000;
		renderer.updateAlphaPlaceDigging(packet);
		packet.face = 4;
		packet.progress = 0.75f;
		packet.timestamp = 11000;
		renderer.updateAlphaPlaceDigging(packet);
		Packet63Digging zero = packet;
		zero.face = 2;
		zero.progress = -0.0f;
		zero.timestamp = 20000;
		renderer.updateAlphaPlaceDigging(zero);
		++zero.x;
		renderer.updateAlphaPlaceDigging(zero);
		ok &= expect(renderer.alphaPlaceDigging.size() == 1,
			"remote digging replaces by coordinates and zero progress does not create an entry");
		if (!renderer.alphaPlaceDigging.empty())
		{
			const auto entry = renderer.alphaPlaceDigging.front();
			ok &= expect(entry.face == 4 && entry.progress == 0.75f && entry.timestamp == 11000,
				"new zero-progress packet preserves the preceding packet's progress, face and timestamp");
			player.setPos(10.0, 64.0, 3.0);
			ok &= expect(!renderer.isAlphaPlaceDiggingExpired(entry, player, 12000),
				"remote digging remains visible at exactly 1000ms and eight blocks");
			ok &= expect(renderer.isAlphaPlaceDiggingExpired(entry, player, 12001),
				"remote digging expires after 1000ms");
			player.setPos(10.001, 64.0, 3.0);
			ok &= expect(renderer.isAlphaPlaceDiggingExpired(entry, player, 12000),
				"remote digging expires beyond eight blocks from integer block coordinates");
			player.setPos(2.0, 64.0, 3.0);
			level->tileId = 0;
			ok &= expect(renderer.isAlphaPlaceDiggingExpired(entry, player, 11000),
				"remote digging expires when the block becomes air");
		}
		++packet.x;
		renderer.updateAlphaPlaceDigging(packet);
		// Both fixture timestamps have expired, so this exercises removal without GL calls.
		renderer.renderAlphaPlaceDigging(player, 0.0f);
		ok &= expect(renderer.alphaPlaceDigging.empty() && minecraft.levelRenderer.destroyProgress == 0.35f,
			"expiry removes adjacent entries and remote state never changes local destroy progress");
	}

	if (ok)
		std::cout << "Network protocol smoke passed" << std::endl;
	return ok ? 0 : 1;
}
