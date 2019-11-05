#ifndef ClientConnectedEvent_hpp
#define ClientConnectedEvent_hpp

#include "Event.hpp"

class ClientConnectedEvent : public Event {
  public:
    explicit ClientConnectedEvent(uint32_t client_id)
        : Event(EventType::EVENT_TYPE_CLIENT_CONNECTED), m_ClientId(client_id){};

    uint32_t getClientId() { return m_ClientId; };

  private:
    uint32_t m_ClientId;
};

#endif /* ClientConnectedEvent_hpp */
