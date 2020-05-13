#include "stdafx.h"
#include "Session.h"

void server_loop(std::shared_ptr<Warg_Server> server)
{
  float64 current_time = get_real_time();
  float64 last_time = 0.0;
  float64 elapsed_time = 0.0;
  while (server->running)
  {
    const float64 time = get_real_time();
    elapsed_time = time - current_time;
    last_time = current_time;
    while (current_time + dt < last_time + elapsed_time)
    {
      current_time += dt;
      server->update(dt);
      // SDL_Delay(random_between(0, 3));
    }
    SDL_Delay(5);
  }
}

Local_Session::Local_Session()
{
  peer = std::make_shared<Local_Peer>();

  server = std::make_shared<Warg_Server>();
  server->connect(peer);
  server->running = true;
  server_thread = std::thread(server_loop, std::ref(server));
}

void Local_Session::push(unique_ptr<Message> message)
{
  peer->push_to_server(std::move(message));
}

std::vector<unique_ptr<Message>> Local_Session::pull()
{
  return peer->pull_from_server();
}

void Local_Session::end()
{
  server->running = false;
  server_thread.join();
}
