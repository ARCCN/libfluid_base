/** @file */
#ifndef __BASEOFCLIENT_HH__
#define __BASEOFCLIENT_HH__

#include <pthread.h>

#include <string>
#include <map>
#include <vector>

#include <fluid/base/EventLoop.hh>
#include <fluid/base/BaseOFConnection.hh>

namespace fluid_base {

struct EventLoopThread {
    pthread_t thread;
    EventLoop* loop;
};

struct ConnectionInfo {
    int sock;
    std::string address;
    int port;
    EventLoop* event_loop;
};

/**
A BaseOFClient manages the very basic functions of an OpenFlow client. It
connects to a server and wait for messages and connection events. It is an
abstract class that should be overriden by another class to provide OpenFlow
features.
*/
class BaseOFClient : public BaseOFHandler {
public:
    /**
    Create a BaseOFClient.

    @param thread_num number of event loops to run. Connections will be
                      attributed to event loops running on threads on a
                      round-robin fashion.
    */
    BaseOFClient(int thread_num = 1);

    ~BaseOFClient();

    /**
    Start the client. It will connect at the address and port declared in the
    constructor and wait for events.
    */
    virtual bool start();

    /**
    Add new client connection.

    @param id connection id
    @param address address to connect to
    @param port port to connect to
    */
    virtual void add_connection(int id, const std::string& address, int port);
    virtual void remove_connection(int id);
    /**
    Stop the client. It will close the connection and signal the event loop to
    stop running.

    It will eventually unblock BaseOFClient::start if it is blocking.
    */
    virtual void stop();


    virtual void base_connection_callback(BaseOFConnection *conn,
                                          BaseOFConnection::Event event_type);

    virtual void base_message_callback(BaseOFConnection *conn,
                                       void *data,
                                       size_t len) = 0;

    virtual void free_data(void *data);

private:
    std::vector<EventLoopThread> event_loop_threads;
    std::map<int, ConnectionInfo> client_conn_info;
    int current_event_loop;

    inline pthread_t* get_thread(int loop_id);
    inline EventLoop* get_loop(int loop_id);
    EventLoop* choose_event_loop();

};

}

#endif
