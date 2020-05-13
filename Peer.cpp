#include "stdafx.h"
#include "Peer.h"

using std::make_unique;

void Local_Peer::push_to_peer(unique_ptr<Message> message)
{
  std::lock_guard<std::mutex> guard(from_server_mutex);
  from_server.push(std::move(message));
}

std::vector<unique_ptr<Message>> Local_Peer::pull_from_peer()
{
  std::lock_guard<std::mutex> guard(to_server_mutex);
  std::vector<unique_ptr<Message>> messages;
  while (to_server.size())
  {
    messages.push_back(std::move(to_server.front()));
    to_server.pop();
  }
  return messages;
}

void Local_Peer::push_to_server(unique_ptr<Message> message)
{
  std::lock_guard<std::mutex> guard(to_server_mutex);
  to_server.push(std::move(message));
}

std::vector<unique_ptr<Message>> Local_Peer::pull_from_server()
{
  std::lock_guard<std::mutex> guard(from_server_mutex);
  std::vector<unique_ptr<Message>> messages;
  while (from_server.size())
  {
    messages.push_back(std::move(from_server.front()));
    from_server.pop();
  }
  return messages;
}