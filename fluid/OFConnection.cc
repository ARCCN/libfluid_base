#include "base/BaseOFConnection.hh"
#include "OFConnection.hh"

namespace fluid_base {

OFConnection::OFConnection(BaseOFConnection* c, OFHandler* ofhandler) {
    this->ofhandler = ofhandler;
    this->conn = c;
    this->conn->set_manager(this);
    this->id = c->get_id();
    this->peer_address = c->get_peer_address();
    this->set_state(STATE_HANDSHAKE);
    this->set_alive(true);
    this->set_version(0);
    this->application_data = NULL;
}

int OFConnection::get_id() const {
    return this->id;
}

std::string OFConnection::get_peer_address() const {
    return this->peer_address;
}

bool OFConnection::is_alive() const {
    return this->alive;
}

void OFConnection::set_alive(bool alive) {
    this->alive = alive;
}

void OFConnection::reset_echo_counter(int max_attempts) {
    this->echo_to_disc = max_attempts;
}

bool OFConnection::decrease_echo_counter() {
    if (this->echo_to_disc > 0) {
        this->echo_to_disc--;
        return true;
    }

    return false;
}

uint8_t OFConnection::get_state() const {
    return state;
}

void OFConnection::set_state(OFConnection::State state) {
    this->state = state;
}

uint8_t OFConnection::get_version() const {
    return this->version;
}

void OFConnection::set_version(uint8_t version) {
    this->version = version;
}

OFHandler* OFConnection::get_ofhandler() const {
    return this->ofhandler;
}

void OFConnection::send(void* data, size_t len) {
    if (this->conn != NULL)
        this->conn->send((uint8_t*) data, len);
}

void OFConnection::add_timed_callback(void* (*cb)(void*),
                                      int interval,
                                      void* arg,
                                      bool is_infinite) {
    if (this->conn != NULL){

        fprintf(stderr, "STARTED REQUESTS\n"); //debug
        this->conn->add_timed_callback(cb, interval, arg, is_infinite);
    }
}

void* OFConnection::get_application_data() const {
    return this->application_data;
}

void OFConnection::set_application_data(void* data) {
    this->application_data = data;
}

void OFConnection::close() {
    // Don't close twice
    if (this->conn == NULL)
        return;

    set_state(STATE_DOWN);
    // Close the BaseOFConnection. This will trigger
    // BaseOFHandler::base_connection_callback. Then BaseOFServer will take
    // care of freeing it for us, so we can lose track of it.
    this->conn->close();
    this->conn = NULL;
}
}
