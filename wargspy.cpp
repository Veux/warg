#include <enet/enet.h>
#include <cstdio>
#include <chrono>
#include <cstring>
#include <cassert>

constexpr uint16_t PORT = 1234;
constexpr uint8_t CODE_REPLY_NULL = 0;
constexpr uint8_t CODE_REPLY_ADDR = 1;
constexpr double TIMEOUT_DURATION = 3;

void die(const char *message)
{
  fprintf(stderr, "%s\nexiting...\n", message);
  exit(EXIT_FAILURE);
}

void log(const char *message)
{
  fprintf(stderr, "%s\n", message);
}

void initialize()
{
  if (enet_initialize() != 0)
    die("failed to initialize enet\n");
}

ENetHost *host_create()
{
  ENetAddress addr;
  addr.host = ENET_HOST_ANY;
  addr.port = PORT;
  ENetHost *host = enet_host_create(&addr, 32, 2, 0, 0);
  if (!host)
    die("failed to create enet host\n");
  return host;
}

int host_service(ENetHost *host, ENetEvent *event)
{
  assert(host);
  assert(event);
  int res = enet_host_service(host, event, 100); 
  if (res < 0)
    die("enet_host_service failed\n");
  return res;
}

ENetPacket *addr_packet(enet_uint32 addr)
{
  constexpr size_t size = 1 + sizeof addr;

  uint8_t data[size];
  data[0] = CODE_REPLY_ADDR;
  memcpy(data + 1, &addr, sizeof addr);
  ENetPacket *packet = enet_packet_create(data, size, ENET_PACKET_FLAG_RELIABLE);
  if (!packet)
    die("failed to create addr packet\n");
  return packet;
}

ENetPacket *null_packet()
{
  uint8_t data = CODE_REPLY_NULL;
  ENetPacket *packet =  enet_packet_create(&data, sizeof data, ENET_PACKET_FLAG_RELIABLE);
  if (!packet)
    die("failed to create null packet\n");
  return packet;
}

void peer_send(ENetPeer *peer, ENetPacket *packet)
{
  assert(peer);
  assert(packet);
  if (enet_peer_send(peer, 0, packet))
      die("failed to send packet\n");
}

int main()
{
  initialize();

  ENetHost *host = host_create();

  enet_uint32 game_server_host = 0;
  std::chrono::time_point<std::chrono::steady_clock> last_announce;

  while (true)
  {
    ENetEvent e;
    if (host_service(host, &e) > 0)
    {
      if (e.type == ENET_EVENT_TYPE_RECEIVE
	        && e.packet->dataLength == 1
	        && e.packet->data[0] == 1)
      {
        if (e.peer->address.host != game_server_host)
	          fprintf(stderr, "game server announced\n");
        game_server_host = e.peer->address.host;
        last_announce = std::chrono::steady_clock::now();
      }
      else if (e.type == ENET_EVENT_TYPE_RECEIVE
	        && e.packet->dataLength == 1
	        && e.packet->data[0] == 2
	        && game_server_host != 0)
      {
        peer_send(e.peer, addr_packet(game_server_host));
        fprintf(stderr, "sending game server address\n");
      }
      else if (e.type == ENET_EVENT_TYPE_RECEIVE
	        && e.packet->dataLength == 1
	        && e.packet->data[0] == 2
	        && game_server_host == 0)
      {
        peer_send(e.peer, null_packet());
        fprintf(stderr, "sending null\n");
      }
    }

    if (game_server_host != 0
        && (std::chrono::steady_clock::now() - last_announce)
            > std::chrono::duration<double>(TIMEOUT_DURATION))
    {
      game_server_host = 0;
      fprintf(stderr, "game server dropped\n");
    }
  }
}
