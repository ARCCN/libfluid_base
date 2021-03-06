#include "OFClient.hh"
#include "fluid/base/of.hh"
#include <sys/param.h>

uint64_t htonll(uint64_t n)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    return n; 
#else
    return (((uint64_t)htonl(n)) << 32) + htonl(n >> 32);
#endif
}


uint64_t ntohll(uint64_t n)
{
#if __BYTE_ORDER == __BIG_ENDIAN
    return n; 
#else
    return (((uint64_t)ntohl(n)) << 32) + ntohl(n >> 32);
#endif
}



namespace fluid_base {

OFClient::OFClient(int thread_num, OFServerSettings ofsc) :
        BaseOFClient(thread_num), ofsc_default(ofsc) {
    pthread_mutex_init(&ofconnections_lock, NULL);
}

void OFClient::add_threads(int thread_num){
    BaseOFClient::add_threads(thread_num);
}

OFClient::~OFClient() {
    this->lock_ofconnections();
    while (!this->ofconnections.empty()) {
        OFConnection* ofconn = this->ofconnections.begin()->second;
        this->ofconnections.erase(this->ofconnections.begin());
        delete ofconn;
    }
    this->ofconnections.clear();
    this->unlock_ofconnections();
    lock_ofsc_list();
    this->ofsc_list.clear();
    unlock_ofsc_list();
}

bool OFClient::start() {
    return BaseOFClient::start();
}


bool OFClient::add_connection(int id, const std::string& address, int port){
    add_connection(id, address, port, ofsc_default);
}


bool OFClient::add_connection(int id, const std::string& address, int port,
                            OFServerSettings ofsc) {

    lock_ofsc_list();
    this->ofsc_list[id] = ofsc;
    unlock_ofsc_list();
    if (!BaseOFClient::add_connection(id, address, port)) {
        lock_ofsc_list();
        ofsc_list.erase(id);
        unlock_ofsc_list();
        return false;
    }
    return true;
}

void OFClient::remove_connection(int id){
    this->lock_ofconnections();
    ofconnections[id]->close();
    ofconnections.erase(id);
    this->unlock_ofconnections();
    lock_ofsc_list();
    this->ofsc_list.erase(id);
    unlock_ofsc_list();
}

void OFClient::stop() {
    // Close all connections
    this->lock_ofconnections();
    for (std::map<int, OFConnection*>::iterator it = this->ofconnections.begin();
         it != this->ofconnections.end();
         it++) {
        if (it->second != NULL) {
            it->second->close();
        }
        this->ofconnections.erase(it);
    }
    this->unlock_ofconnections();
    lock_ofsc_list();
    this->ofsc_list.clear();
    unlock_ofsc_list();
    // Stop BaseOFClient
    BaseOFClient::stop();
}

OFConnection* OFClient::get_ofconnection(int id) {
    this->lock_ofconnections();
    OFConnection* cc = ofconnections[id];
    this->unlock_ofconnections();
    return cc;
}

void OFClient::base_message_callback(BaseOFConnection* c, void* data, size_t len) {


    uint8_t type = ((uint8_t*) data)[1];
    
    OFConnection* cc = (OFConnection*) c->get_manager();
    int id = c->get_id();
    // We trust that the other end is using the negotiated protocol
    // version. Should we?

    lock_ofsc_list();
    if (ofsc_list[id].liveness_check() and type == OFPT_ECHO_REQUEST) {
        // Just change the type and send back dont forget htons
        ((uint8_t*) data)[1] = OFPT_ECHO_REPLY;
        c->send(data, htons(((uint16_t*) data)[1]));
        if (ofsc_list[id].dispatch_all_messages()) goto dispatch; else goto done;
    }

    if (ofsc_list[id].handshake() and type == OFPT_HELLO) {
        uint8_t version = ((uint8_t*) data)[0];
        if (not this->ofsc_list[id].supported_versions() & (1 << (version - 1))) {
            uint8_t msg[12];
            memset((void*) msg, 0, 8);
            msg[0] = version;
            msg[1] = OFPT_ERROR;
            ((uint16_t*) msg)[1] = htons(12);
            ((uint32_t*) msg)[1] = ((uint32_t*) data)[1];
            ((uint16_t*) msg)[4] = htons(OFPET_HELLO_FAILED);
            ((uint16_t*) msg)[5] = htons(OFPHFC_INCOMPATIBLE);
            cc->send(msg, 12);
            cc->close();
            cc->set_state(OFConnection::STATE_FAILED);
            connection_callback(cc, OFConnection::EVENT_FAILED_NEGOTIATION);
        } else {
            if (ofsc_list[id].is_controller()) {
                struct ofp_header msg;
                msg.version = ((uint8_t*) data)[0];
                msg.type = OFPT_FEATURES_REQUEST;
                msg.length = htons(8);
                msg.xid = ((uint32_t*) data)[1];
                c->send(&msg, 8);
            }
        }

        if (ofsc_list[id].dispatch_all_messages()) goto dispatch; else goto done;
    }

    if (ofsc_list[id].liveness_check() and type == OFPT_ECHO_REPLY) {
        if (ntohl(((uint32_t*) data)[1]) == ECHO_XID) {
            cc->set_alive(true);
        }

        if (ofsc_list[id].dispatch_all_messages()) goto dispatch; else goto done;
    }

    if (ofsc_list[id].handshake() and !ofsc_list[id].is_controller() and type == OFPT_FEATURES_REQUEST) {
        struct ofp_switch_features reply;

        cc->set_version(((uint8_t*) data)[0]);
        cc->set_state(OFConnection::STATE_RUNNING);
        reply.header.version = ((uint8_t*) data)[0];
        reply.header.type = OFPT_FEATURES_REPLY;
        reply.header.length = htons(sizeof(reply));
        reply.header.xid = ((uint32_t*) data)[1];
        reply.datapath_id = htonll(ofsc_list[id].datapath_id());
        reply.n_buffers = htonl(ofsc_list[id].n_buffers()); //add htonl?
        reply.n_tables = htonl(ofsc_list[id].n_tables()); // add htonl? htons?
        reply.auxiliary_id = ofsc_list[id].auxiliary_id();
        reply.capabilities = htonl(ofsc_list[id].capabilities());
        cc->send(&reply, sizeof(reply));

        if (ofsc_list[id].liveness_check())
            c->add_timed_callback(send_echo, ofsc_list[id].echo_interval() * 1000, cc);
        connection_callback(cc, OFConnection::EVENT_ESTABLISHED);

        if (ofsc_list[id].dispatch_all_messages()) goto dispatch; else goto done;
    }

    // Handle feature replies
    if (ofsc_list[id].handshake() and ofsc_list[id].is_controller() and type == OFPT_FEATURES_REPLY) {
        cc->set_version(((uint8_t*) data)[0]);
        cc->set_state(OFConnection::STATE_RUNNING);
        if (ofsc_list[id].liveness_check())
            c->add_timed_callback(send_echo, ofsc_list[id].echo_interval() * 1000, cc);
        connection_callback(cc, OFConnection::EVENT_ESTABLISHED);
        goto dispatch;
    }


    goto dispatch;

    // Dispatch a message and goto done
    dispatch:
        unlock_ofsc_list();
        message_callback(cc, type, data, len);
        if (this->ofsc_list[id].keep_data_ownership())
            this->free_data(data);
        return;

    
    // Free the message and return
    done:
        unlock_ofsc_list();
        c->free_data(data);
        return;
}

void OFClient::base_connection_callback(BaseOFConnection* c, BaseOFConnection::Event event_type) {
    /* If the connection was closed, destroy it.
    There's no need to notify the user, since a DOWN event already
    means a CLOSED event will happen and nothing should be expected from
    the connection. */
    if (event_type == BaseOFConnection::EVENT_CLOSED) {
        BaseOFClient::base_connection_callback(c, event_type);
        // TODO: delete the OFConnection?
        return;
    }

    OFConnection* cc;
    int conn_id = c->get_id();
    lock_ofsc_list();

    if (event_type == BaseOFConnection::EVENT_UP) {
        if (ofsc_list[conn_id].handshake()) {
            struct ofp_hello msg;
            msg.header.version = this->ofsc_list[conn_id].max_supported_version();
            msg.header.type = OFPT_HELLO;
            msg.header.length = htons(8);
            msg.header.xid = htonl(HELLO_XID);
            c->send(&msg, 8);
        }

        cc = new OFConnection(c, this);
        lock_ofconnections();
        ofconnections[conn_id] = cc;
        unlock_ofconnections();

        connection_callback(cc, OFConnection::EVENT_STARTED);
    }
    else if (event_type == BaseOFConnection::EVENT_DOWN) {
        // fprintf(stderr, "%d\n", ofsc_list.size());
        ofsc_list.erase(conn_id);
        // fprintf(stderr, "%d\n", ofsc_list.size());
        cc = get_ofconnection(conn_id);
        connection_callback(cc, OFConnection::EVENT_CLOSED);
        cc->close();
        lock_ofconnections();
        ofconnections.erase(conn_id);
        unlock_ofconnections();
    }
    unlock_ofsc_list();
}


void OFClient::free_data(void* data) {
    BaseOFClient::free_data(data);
}

void* OFClient::send_echo(void* arg) {
    OFConnection* cc = static_cast<OFConnection*>(arg);

    if (!cc->is_alive()) { //ERASE?
        cc->close();
        cc->get_ofhandler()->connection_callback(cc, OFConnection::EVENT_DEAD);
        return NULL;
    }

    uint8_t msg[8];
    memset((void*) msg, 0, 8);
    msg[0] = (uint8_t) cc->get_version();
    msg[1] = OFPT_ECHO_REQUEST;
    ((uint16_t*) msg)[1] = htons(8);
    ((uint32_t*) msg)[1] = htonl(ECHO_XID);
    
    cc->send(msg, 8);

    return NULL;
}


}

