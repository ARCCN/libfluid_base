#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <sstream>

#include <event2/thread.h>

#include "BaseOFClient.hh"

namespace fluid_base {

struct ConnectionInfo {
    int id;
    std::string address;
    int port;
    EventLoop* event_loop;
};

extern bool evthread_use_pthreads_called;

BaseOFClient::BaseOFClient(const int thread_num) {
    // Prepare libevent for threads
    // This will leave a small, insignificant leak for us.
    // See: http://archives.seul.org/libevent/users/Jul-2011/msg00028.html
    if (!evthread_use_pthreads_called) {
        evthread_use_pthreads();
        evthread_use_pthreads_called = true;
    }

    // Ignore SIGPIPE so it becomes an EPIPE
    signal(SIGPIPE, SIG_IGN);

    for (int loop_id = 0; loop_id < thread_num; loop_id++) {
        EventLoopThread loop_thread;
        loop_thread.thread = pthread_t();
        loop_thread.loop = new EventLoop(loop_id);
        event_loop_threads.push_back(loop_thread);
    }
    current_event_loop = 0;
}

BaseOFClient::~BaseOFClient() {
    for (int loop_id = 0; loop_id < event_loop_threads.size(); loop_id++) {
        EventLoop* loop = get_loop(loop_id);
        delete loop;
    }
}

void BaseOFClient::add_connection(int id, const std::string& address,
                                  int port) {
    int sock;
    if ((sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        fprintf(stderr, "Error creating socket");
        return;
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(address.c_str());
    server.sin_port = htons(port);
    while (connect(sock, (struct sockaddr *) &server, sizeof(server)) < 0) {
        // Retry to connect after 100 milliseconds
        usleep(100);
    }

    EventLoop* event_loop = choose_event_loop();
    BaseOFConnection *c = new BaseOFConnection(id,
                                               this,
                                               event_loop,
                                               sock,
                                               false,
                                               address);
}

bool BaseOFClient::start() {
    // Start one thread for each event loop
    for (int loop_id = 0; loop_id < event_loop_threads.size(); loop_id++) {
        // Create thread for connections
        pthread_t* thread_ptr = get_thread(loop_id);
        EventLoop* loop = get_loop(loop_id);
        pthread_create(thread_ptr,
                       NULL,
                       EventLoop::thread_adapter,
                       loop);

#if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 12
        // Init thread name
        std::ostringstream thread_name;
        thread_name << "Fluid Client " << loop_id;
        pthread_setname_np(*thread_ptr, thread_name.str().c_str());
#endif
    }

    return true;
}

void BaseOFClient::free_data(void *data) {
    BaseOFConnection::free_data(data);
}

void BaseOFClient::stop() {
    for (int loop_id = 0; loop_id < event_loop_threads.size(); loop_id++) {
        pthread_t* thread_ptr = get_thread(loop_id);
        EventLoop* loop = get_loop(loop_id);

        loop->stop();
        pthread_join(*thread_ptr, NULL);
    }
}

void BaseOFClient::base_connection_callback(BaseOFConnection *conn,
                                            BaseOFConnection::Event event_type) {
    if (event_type == BaseOFConnection::EVENT_CLOSED) {
        delete conn;
    }
}

pthread_t* BaseOFClient::get_thread(int loop_id) {
    return &event_loop_threads[loop_id].thread;
}

EventLoop* BaseOFClient::get_loop(int loop_id) {
    return event_loop_threads[loop_id].loop;
}


EventLoop* BaseOFClient::choose_event_loop() {
    EventLoop* selected_loop = get_loop(current_event_loop);
    current_event_loop = (++current_event_loop) % event_loop_threads.size();
    return selected_loop;
}

}
