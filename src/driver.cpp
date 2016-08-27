//=======================================================================
// Copyright (c) 2015-2016 Baptiste Wicht
// Distributed under the terms of the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include "asgard/driver.hpp"

namespace {

// Configuration
std::vector<asgard::KeyValue> config;

asgard::driver_connector driver;

// The remote IDs
int source_id = -1;
int play_action_id = -1;
int next_action_id = -1;
int previous_action_id = -1;

void stop(){
    std::cout << "asgard:system: stop the driver" << std::endl;

    asgard::unregister_action(driver, source_id, next_action_id);
    asgard::unregister_action(driver, source_id, previous_action_id);
    asgard::unregister_action(driver, source_id, play_action_id);
    asgard::unregister_source(driver, source_id);

    // Close the socket
    close(driver.socket_fd);
}

void terminate(int){
    stop();

    std::exit(0);
}

bool web_request(const std::string& kodi_server, const std::string& json_request){
    auto http_request = "http://" + kodi_server + json_request;

    auto result = asgard::exec_command("wget -O - '" + http_request + "' 2>&1");

    if(result.first){
        std::cout << "Pause failed(" << result.first << ") with result: \n" << result.second << std::endl;
        return false;
    } else {
        return true;
    }
}

} //End of anonymous namespace

int main(){
    // Load the configuration file
    asgard::load_config(config);

    // Open the connection
    if(!asgard::open_driver_connection(driver, asgard::get_string_value(config, "server_socket_addr").c_str(), asgard::get_int_value(config, "server_socket_port"))){
        return 1;
    }

    auto kodi_server = asgard::get_string_value(config, "kodi_server");

    // Register signals for "proper" shutdown
    signal(SIGTERM, terminate);
    signal(SIGINT, terminate);

    // Register the source and actions
    source_id = asgard::register_source(driver, "kodi");
    play_action_id = asgard::register_action(driver, source_id, "SIMPLE", "play");
    next_action_id = asgard::register_action(driver, source_id, "SIMPLE", "next");
    previous_action_id = asgard::register_action(driver, source_id, "SIMPLE", "previous");

    // Listen for messages from the server
    while(true){
        if(asgard::receive_message(driver.socket_fd, driver.receive_buffer, asgard::buffer_size)){
            std::string message(driver.receive_buffer);
            std::stringstream message_ss(message);

            std::string command;
            message_ss >> command;

            if(command == "ACTION"){
                std::string action;
                message_ss >> action;

                if(action == "play"){
                    web_request(kodi_server, "/jsonrpc?request={\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"Player.PlayPause\",\"params\":{\"playerid\":0}}");
                } else if(action == "next"){
                    web_request(kodi_server, "/jsonrpc?request={\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"Player.GoTo\",\"params\":{\"playerid\":0,\"to\":\"next\"}}");
                } else if(action == "previous"){
                    web_request(kodi_server, "/jsonrpc?request={\"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"Player.GoTo\",\"params\":{\"playerid\":0,\"to\":\"previous\"}}");
                } else {
                    std::cout << "asgard:echo: unknown action: " << action << std::endl;
                }
            } else {
                std::cout << "asgard:echo: unknown command: " << command << std::endl;
            }
        }
    }

    stop();

    return 0;
}
