#include <iostream>
#include "named-pipe-server.hpp"
#include "nlohmann/json.hpp"
#include "xtime.hpp"

using namespace std;

int main() {
    using json = nlohmann::json;
    std::string named_pipe_name("intrade_bar_console_bot");
    SimpleNamedPipe::NamedPipeServer server(named_pipe_name);

    server.on_open = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
        std::cout << "open, handle: " << connection->get_handle() << std::endl;
    };

    server.on_message = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::string &in_message) {
        /* обрабатываем входящие сообщения */
        std::cout << "message " << in_message << ", handle: " << connection->get_handle() << std::endl;
        /* парисм */
        try {
            json j = json::parse(in_message);
            if(j.find("contract") != j.end()) {
                /* пришел запрос на открытие сделки */
                std::cout << "contract" << std::endl;
                json j_contract = j["contract"];
                std::string symbol = j_contract["symbol"];
                if(j_contract.find("amount") != j_contract.end()) {
                    double amount = j_contract["amount"];
                    std::cout << "amount: " << amount << std::endl;
                }
                if(j_contract.find("date_expiry") != j_contract.end()) {
                    if(j_contract["date_expiry"].is_number()) {
                        const xtime::timestamp_t timestamp_server = xtime::get_timestamp();
                        xtime::timestamp_t temp_date_expiry = j_contract["date_expiry"];
                        if(temp_date_expiry < xtime::SECONDS_IN_DAY) {
                            /* пользователь задал время экспирации */
                            const xtime::timestamp_t date_expiry_offset = (timestamp_server + 3*xtime::SECONDS_IN_MINUTE);
                            xtime::timestamp_t date_expiry = date_expiry_offset - (date_expiry_offset % temp_date_expiry) + temp_date_expiry;
                            date_expiry += xtime::SECONDS_IN_HOUR * 3;
                            std::cout << "date_expiry: " << xtime::get_str_date_time(date_expiry) << std::endl;
                        }

                    }
                } else
                if(j_contract.find("duration") != j_contract.end()) {
                    if(j_contract["duration"].is_number()) {
                        const uint32_t duration = j_contract["duration"];
                        std::cout << "duration: " << duration << std::endl;
                    }
                }
            }
        }
        catch(...) {
            std::cout << "error: json::parse" << std::endl;
        }
    };
    server.on_close = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
        std::cout << "close, handle: " << connection->get_handle() << std::endl;
    };
    server.on_error = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::error_code &ec) {
        std::cout << "error, handle: " << connection->get_handle() << ", what " << ec.value() << std::endl;
    };

    /* запускаем сервер */
    server.start();
    while(true) {
        server.send_all("{\"ping\":1}");
        server.send_all("{\"balance\":1230.0}");
        server.send_all("{\"connection\":1}");
        std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    }
    return EXIT_SUCCESS;
}
