/*
* intrade-bar-console-bot - C++ Console robot for trading with an intrade.bar broker
*
* Copyright (c) 2020 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef INTRADE_BAR_CONSOLE_BOT_NAMED_PIPE_SERVER_HPP_INCLUDED
#define INTRADE_BAR_CONSOLE_BOT_NAMED_PIPE_SERVER_HPP_INCLUDED

#include <iostream>
#include "intrade-bar-console-bot-settings.hpp"
#include "named-pipe-server.hpp"
#include "intrade-bar-api.hpp"
#include "nlohmann/json.hpp"
#include "xtime.hpp"

namespace intrade_bar_console_bot {

    void init_named_pipe_server(
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> &server,
            Settings &settings,
            intrade_bar::IntradeBarApi &api) {
        server = std::make_shared<SimpleNamedPipe::NamedPipeServer>(settings.named_pipe);

        server->on_open = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
            PrintThread{} << "open, handle: " << connection->get_handle() << std::endl;
            if(api.connected()) server->send_all("{\"connection\":1}");
            else server->send_all("{\"connection\":0}");
            double balance = api.get_balance();
            server->send_all("{\"balance\":" + std::to_string(balance) + "}");
        };

        server->on_message = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::string &in_message) {
            /* обрабатываем входящие сообщения */
            //std::cout << "message " << in_message << ", handle: " << connection->get_handle() << std::endl;
            /* парисм */
            try {
                json j = json::parse(in_message);
                if(j.find("contract") != j.end()) {
                    /* пришел запрос на открытие сделки */
                    //std::cout << "contract" << std::endl;
                    json j_contract = j["contract"];

                    /* параметры опциона */
                    std::string symbol;
                    std::string strategy_name;
                    double amount = 0;
                    int contract_type = intrade_bar_common::INVALID_ARGUMENT;
                    uint32_t duration = 0;
                    xtime::timestamp_t date_expiry = 0;
                    uint64_t api_bet_id = 0;

                    /* получаем все параметры опицона */
                    if(j_contract.find("symbol") != j_contract.end()) {
                        symbol = j_contract["symbol"];
                        //std::cout << "symbol: " << symbol << std::endl;
                    }
                    if(j_contract.find("amount") != j_contract.end()) {
                        amount = j_contract["amount"];
                        //std::cout << "amount: " << amount << std::endl;
                    }
                    if(j_contract.find("direction") != j_contract.end()) {
                        if(j_contract["direction"] == "BUY") contract_type = intrade_bar_common::BUY;
                        else if(j_contract["direction"] == "SELL") contract_type = intrade_bar_common::SELL;
                        //std::cout << "contract_type: " << contract_type << std::endl;
                    }
                    if(j_contract.find("date_expiry") != j_contract.end()) {
                        if(j_contract["date_expiry"].is_number()) {
                            const xtime::timestamp_t timestamp_server = xtime::get_timestamp();
                            xtime::timestamp_t temp_date_expiry = j_contract["date_expiry"];
                            if(temp_date_expiry < xtime::SECONDS_IN_DAY) {
                                /* пользователь задал время экспирации */
                                const xtime::timestamp_t date_expiry_offset = (timestamp_server + 3*xtime::SECONDS_IN_MINUTE);
                                date_expiry = date_expiry_offset - (date_expiry_offset % temp_date_expiry) + temp_date_expiry;
                                date_expiry += xtime::SECONDS_IN_HOUR * 3;
                                //std::cout << "date_expiry: " << xtime::get_str_date_time(date_expiry) << std::endl;
                            }
                        }
                    } else
                    if(j_contract.find("duration") != j_contract.end()) {
                        if(j_contract["duration"].is_number()) {
                            duration = j_contract["duration"];
                            //std::cout << "duration: " << duration << std::endl;
                        }
                    }

                    if (symbol.size() > 0 &&
                        amount > 0 &&
                        (contract_type == intrade_bar_common::BUY ||
                         contract_type == intrade_bar_common::SELL) &&
                        duration > 0) {
                        /* открываем сделку типа SPRINT */
                        int err = api.open_bo(symbol, strategy_name, amount,
                            contract_type, duration, api_bet_id,
                                [&](const intrade_bar::IntradeBarApi::Bet &bet) {
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) {

                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                PrintThread{} << "win" << std::endl;
                                api.update_balance();
                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                PrintThread{} << "loss" << std::endl;
                                api.update_balance();
                            } else
                            if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                PrintThread{} << "deal opennig error (server response)" << std::endl;
                            }
                        });
                        if(err != intrade_bar_common::ErrorType::OK) {
                            PrintThread{} << "deal opening error" << std::endl;
                        } else {
                            api.update_balance();
                            PrintThread{} << "deal open, symbol: " << symbol << std::endl;
                        } //
                    } else {
                        PrintThread{} << "binary options parameters error" << std::endl;
                    }

                }
            }
            catch(...) {
                PrintThread{} << "error: json::parse" << std::endl;
            }
        };
        server->on_close = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
            PrintThread{} << "close, handle: " << connection->get_handle() << std::endl;
        };
        server->on_error = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::error_code &ec) {
            PrintThread{} << "error, handle: " << connection->get_handle() << ", what " << ec.value() << std::endl;
        };

        /* запускаем сервер */
        server->start();
    }

    void update_connection(
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> &server,
            intrade_bar::IntradeBarApi &api) {
        static bool last_connected = false;
        if(last_connected == api.connected()) return;
        if(api.connected()) server->send_all("{\"connection\":1}");
        else server->send_all("{\"connection\":0}");
        last_connected = api.connected();
        PrintThread{} << "connection: " << last_connected << std::endl;
    }

    void update_ping(
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> &server,
            const int delay) {
        const int MAX_TICK = 10000;
        static int tick = 0;
        tick += delay;
        if(tick > MAX_TICK) {
            server->send_all("{\"ping\":1}");
            tick = 0;
        }
    }

    void update_balance(
            std::shared_ptr<SimpleNamedPipe::NamedPipeServer> &server,
            intrade_bar::IntradeBarApi &api,
            const int delay) {
        static double balance = 0;
        if(balance != api.get_balance()) {
            balance = api.get_balance();
            server->send_all("{\"balance\":" + std::to_string(balance) + "}");
            PrintThread{} << "balance: " << balance << std::endl;
        }
        const int MAX_TICK = 10000;
        static int tick = 0;
        tick += delay;
        if(tick > MAX_TICK) {
            api.update_balance();
            tick = 0;
        }
    }
};

#endif // INTRADE_BAR_CONSOLE_BOT_NAMED_PIPE_SERVER_HPP_INCLUDED
