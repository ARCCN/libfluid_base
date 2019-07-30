#include <event2/event.h>
#include <event2/bufferevent.h>
#include <fluid/base/BaseOFConnection.hh>

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string>

#include <string.h>
#include <arpa/inet.h>

#include <fluid/base/BaseOFConnection.hh>
#include <fluid/OFConnection.hh>
#include <fluid/OFServer.hh>

#include "base/BaseOFClient.hh"

/**
Classes for creating an OpenFlow client that creates connections and handles
events.
*/
namespace fluid_base {
/**
An OFClient manages an OpenFlow outgoing connection and abstract events through
callbacks. It provides some of the basic functionalities: OpenFlow connection
setup and liveness check.

Typically a switch or low-level switch controller base class will inherit from
OFClient and implement the message_callback and connection_callback methods
to implement further functionality.
*/
class OFClient : private BaseOFClient, public OFHandler {
public:
    /**
    Create a OFClient.

    @param thread_num number of event loops to run. Connections will be
                      attributed to event loops running on threads on a
                      round-robin fashion.
    */
    OFClient(int thread_num = 1);

    virtual ~OFClient();

    /**
    Start the client. It will connect to the address and port declared in the
    constructor.
    */
    virtual bool start();

    /**
    Add new client connection.

    @param id connection id
    @param address address to connect to
    @param port port to connect to
    */
    virtual bool add_connection(int id, const std::string& address, int port,
                            OFServerSettings ofsc);
    /**
    remove connecton by id

    @param id connection id
    */
    virtual void remove_connection(int id);
    
    /**
    Stop the client. It will close the connection, ask the thead handling
    connections to finish.

    It will eventually unblock OFClient::start if it is blocking.
    */
    virtual void stop();

    /**
    Retrieve an OFConnection object associated with this OFClient with a given
    id.

    @param id OFConnection id
    */
    OFConnection* get_ofconnection(int id);

    // Implement your logic here
    virtual void connection_callback(OFConnection* conn, OFConnection::Event event_type) {};
    virtual void message_callback(OFConnection* conn, uint8_t type, void* data, size_t len) {};

    void free_data(void* data);

protected:
    //OFConnection* conn;
    std::map<int, OFConnection*> ofconnections;
    pthread_mutex_t ofconnections_lock;

    std::map<int, OFServerSettings> sw_list; 
    inline void lock_ofconnections() {
        pthread_mutex_lock(&ofconnections_lock);
    }

    inline void unlock_ofconnections() {
        pthread_mutex_unlock(&ofconnections_lock);
    }

private:
    
    void base_message_callback(BaseOFConnection* c, void* data, size_t len);
    void base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type);
    static void* send_echo(void* arg);
};

}
