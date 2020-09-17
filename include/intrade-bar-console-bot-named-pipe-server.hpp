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
#include <set>
#include "intrade-bar-console-bot-settings.hpp"
#include "named-pipe-server.hpp"
#include "intrade-bar-api.hpp"
#include "nlohmann/json.hpp"
#include "xtime.hpp"

namespace intrade_bar_console_bot {

    /** \brief Класс бота для связи MT4 с брокером
     */
    class Bot {
    public:
        std::atomic<bool> is_program_bot = ATOMIC_VAR_INIT(false);
    private:

        /* чушь, муть и компот, не трогать, даже автор не помнит что и зачем нужно
         * BEGIN
         */
        //std::vector<intrade_bar::IntradeBarApi::Bet> history_open_bets;
        std::map<uint64_t, intrade_bar::IntradeBarApi::Bet> open_bets;          /**< Массив сделок */
        std::mutex open_bets_mutex;

        std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;               /**< Сервер именованных каналов */

        std::mutex auth_mutex;
        std::string last_email;
        std::string last_password;

        std::atomic<bool> is_wait_broker_connection = ATOMIC_VAR_INIT(false);
        std::atomic<int> connection_counter = ATOMIC_VAR_INIT(0);
        const uint64_t error_bet_id_offset = 0xFFFF;
        uint64_t error_bet_id = 0;
        uint32_t null_connections_counter = 0;                                  /**< Счетчик отсутстивя соединений. Нужен для автоматического выхода из программы */

        /* чушь, муть и компот, не трогать, даже автор не помнит что и зачем нужно
         * END
         */
    public:
        Bot() {};
        ~Bot() {};

        void send_bet(const intrade_bar::IntradeBarApi::Bet &new_bet) {
#ifdef INTRADE_BAR_BOT_VERSION_B
            std::string text("{\"update_bet\":");
            text += "{\"symbol\":\"";
            text += new_bet.symbol_name;
            text += "\",\"note\":\"";
            text += new_bet.note;
            text += "\",\"api_id\":";
            text += std::to_string(new_bet.api_bet_id);
            text += ",\"broker_id\":";
            text += std::to_string(new_bet.broker_bet_id);
            text += ",\"open_price\":";
            text += std::to_string(new_bet.open_price);
            text += ",\"direction\":";
            if(new_bet.contract_type == intrade_bar_common::BUY) text += "\"buy\"";
            else if(new_bet.contract_type == intrade_bar_common::SELL) text += "\"sell\"";
            else text += "\"none\"";
            text += ",\"status\":";
            if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) text += "\"win\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) text += "\"loss\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) text += "\"error\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) text += "\"wait\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE) text += "\"unknown\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::CHECK_ERROR) text += "\"error\"";
            else text += "\"none\"";
            text += ",\"amount\":";
            text += std::to_string(new_bet.amount);
            text += ",\"profit\":";
            text += std::to_string(new_bet.profit);
            text += ",\"payout\":";
            text += std::to_string(new_bet.payout);
            text += ",\"duration\":";
            text += std::to_string(new_bet.duration);
            text += ",\"opening_time\":";
            text += std::to_string(new_bet.opening_timestamp);
            text += ",\"send_time\":";
            text += std::to_string(new_bet.send_timestamp);
            text += ",\"closing_time\":";
            text += std::to_string(new_bet.closing_timestamp);
            text += "}}";
            server->send_all(text);
#endif
        }

        void init_named_pipe_server(
                const std::string &named_pipe,
                intrade_bar::IntradeBarApi &api) {

            //static std::mutex auth_mutex;
            //static std::string last_email;
            //static std::string last_password;
            //static std::atomic<bool> is_wait_broker_connection = ATOMIC_VAR_INIT(false);
            //static std::atomic<int> connection_counter = ATOMIC_VAR_INIT(0);
            //const uint64_t error_bet_id_offset = 0xFFFF;
            //static uint64_t error_bet_id = 0;

            server = std::make_shared<SimpleNamedPipe::NamedPipeServer>(named_pipe);

            server->on_open = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
                PrintThread{} << "open, handle: " << connection->get_handle() << std::endl;
                //PrintThread{} << "connection_counter: " << connection_counter << std::endl;
                if(api.connected() && (is_program_bot || connection_counter > 1)) {
                    double balance = api.get_balance();
                    server->send_all("{\"balance\":" + std::to_string(balance) + ",\"rub\":" + std::to_string(api.account_rub_currency()) + "}");
                    server->send_all("{\"connection\":1}");
                    /* отправляем историю сделок */
                    //std::vector<intrade_bar::IntradeBarApi::Bet> bets;
                    //{
                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                        //bets = history_open_bets;
                   // }
                    //send_history_open_bet(api, bets);
                } else {
                    server->send_all("{\"connection\":0}");
                    std::lock_guard<std::mutex> lock(auth_mutex);
                    last_email.clear();
                    last_password.clear();
                    is_wait_broker_connection = false;
                }
                connection_counter++;
            };

            server->on_message = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::string &in_message) {
                /* обрабатываем входящие сообщения */
                //std::cout << "message " << in_message << ", handle: " << connection->get_handle() << std::endl;
                /* парисм */
                try {
                    json j = json::parse(in_message);
                    if(j.find("auth") != j.end()) {
                        json j_auth = j["auth"];
                        std::string email = j_auth["email"];
                        std::string password = j_auth["pass"];
                        bool is_demo_account = j_auth["demo"];
                        bool is_rub_currency = j_auth["rub"];

                        if((api.connected() && (email != last_email || password != last_password)) || (!api.connected() && !is_wait_broker_connection)) {
                            {
                                std::lock_guard<std::mutex> lock(auth_mutex);
                                last_email = email;
                                last_password = password;
                            }

                            std::thread connect_thread([&,email,password,is_demo_account,is_rub_currency](){
                                std::cout << "wait authorization" << std::endl;
                                uint32_t tick = 0;
                                while(is_wait_broker_connection) {
                                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                                    ++tick;
                                    if(tick > 60) return;
                                }
                                /* подключаемся к брокеру */
                                is_wait_broker_connection = true;
                                std::cout << "authorization " << email << " demo " << is_demo_account << " rub " << is_rub_currency << std::endl;
                                int err = api.connect(email, password, is_demo_account, is_rub_currency);
                                if(err != intrade_bar_common::OK) {
                                    server->send_all("{\"connection\":-1,\"code\":" + std::to_string(err) + "}");
                                    std::cout << "authorization error, code: " << err << std::endl;
                                    std::lock_guard<std::mutex> lock(auth_mutex);
                                    last_email.clear();
                                    last_password.clear();
                                    is_wait_broker_connection = false;
                                    return;
                                } else {
                                    /* обновляем баланс */
                                    api.update_balance(false);
                                    double balance = api.get_balance();
                                    server->send_all("{\"balance\":" + std::to_string(balance) + "}");
                                    server->send_all("{\"connection\":1}");
                                    std::cout << std::endl << "authorization successful!" << std::endl;
                                    std::cout << "balance: " << balance << std::endl;
                                    std::cout << "demo: " << api.demo_account() << std::endl;
                                    std::cout << "rub currency: " << api.account_rub_currency() << std::endl;

                                    /* отправляем историю сделок */
                                    //std::vector<intrade_bar::IntradeBarApi::Bet> bets;
                                    //{
                                    //    std::lock_guard<std::mutex> lock(open_bets_mutex);
                                    //    bets = history_open_bets;
                                    //}
                                    //send_history_open_bet(api, bets);
                                }
                                is_wait_broker_connection = false;
                            });
                            connect_thread.detach();
                        } else
                        if(api.connected() && is_demo_account != api.demo_account()) {
                            std::thread connect_thread([&,is_demo_account]() {
                                std::cout << "set demo" << std::endl;
                                int err = api.set_demo_account(is_demo_account);
                                if(err != intrade_bar_common::OK) {
                                    server->send_all("{\"connection\":-1,\"code\":" + std::to_string(err) + "}");
                                    std::cout << "set demo account error, code: " << err << std::endl;
                                    return;
                                } else {
                                    std::cout << "set demo account successful!" << std::endl;
                                    /* обновляем баланс */
                                    api.update_balance(false);
                                    double balance = api.get_balance();
                                    server->send_all("{\"balance\":" + std::to_string(balance) + ",\"rub\":" + std::to_string(api.account_rub_currency()) + "}");
                                    server->send_all("{\"connection\":1}");
                                    std::cout << "set demo account successful!" << std::endl;
                                    std::cout << "demo: " << api.demo_account() << std::endl;
                                }
                            });
                            connect_thread.detach();
                        } else
                        if(api.connected() && is_rub_currency != api.account_rub_currency()) {
                            std::thread connect_thread([&,is_rub_currency]() {
                                std::cout << "set rub" << std::endl;
                                int err = api.set_rub_account_currency(is_rub_currency);
                                if(err != intrade_bar_common::OK) {
                                    server->send_all("{\"connection\":-1,\"code\":" + std::to_string(err) + "}");
                                    std::cout << "set rub account error, code: " << err << std::endl;
                                    return;
                                } else {
                                    /* обновляем баланс */
                                    api.update_balance(false);
                                    double balance = api.get_balance();
                                    server->send_all(
										"{\"balance\":" + std::to_string(balance) +
										",\"rub\":" + std::to_string(api.account_rub_currency()) +
										",\"demo\":" + std::to_string(api.demo_account()) +
										"}");
                                    server->send_all("{\"connection\":1}");
                                    std::cout << "set rub account successful!" << std::endl;
                                    std::cout << "demo: " << api.demo_account() << std::endl;
                                }
                            });
                            connect_thread.detach();
                        } else {
                            std::cout << "no set, connected " << api.connected() << std::endl;
                        }
                    }
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
                        }
                        if(j_contract.find("strategy_name") != j_contract.end()) {
                            strategy_name = j_contract["strategy_name"];
                        }
                        if(j_contract.find("note") != j_contract.end()) {
                            strategy_name = j_contract["note"];
                        }
                        if(j_contract.find("amount") != j_contract.end()) {
                            amount = j_contract["amount"];
                        }
                        if(j_contract.find("direction") != j_contract.end()) {
                            if(j_contract["direction"] == "BUY") contract_type = intrade_bar_common::BUY;
                            else if(j_contract["direction"] == "SELL") contract_type = intrade_bar_common::SELL;
                        }
                        if(j_contract.find("date_expiry") != j_contract.end()) {
                            if(j_contract["date_expiry"].is_number()) {
                                const xtime::timestamp_t timestamp_server = api.get_server_timestamp();
                                xtime::timestamp_t temp_date_expiry = j_contract["date_expiry"];
                                if(temp_date_expiry < xtime::SECONDS_IN_DAY) {
                                    /* пользователь задал время экспирации */
                                    date_expiry = intrade_bar_common::get_classic_bo_closing_timestamp(timestamp_server, temp_date_expiry);
                                } else {
                                    date_expiry = temp_date_expiry;
                                }
                                //std::cout << "date_expiry: " << xtime::get_str_date_time(date_expiry) << std::endl;
                            } else {
                                //std::cout << "date_expiry !is_number()" << std::endl;
                            }
                        } else
                        if(j_contract.find("duration") != j_contract.end()) {
                            if(j_contract["duration"].is_number()) {
                                duration = j_contract["duration"];
                            }
                        }

                        if (symbol.size() > 0 &&
                            amount > 0 &&
                            (contract_type == intrade_bar_common::BUY ||
                             contract_type == intrade_bar_common::SELL) &&
                            duration > 0) {
                            /* открываем сделку типа SPRINT */
                            int err = api.open_bo(
                                    symbol,
                                    strategy_name,
                                    amount,
                                    intrade_bar_common::TypesBinaryOptions::SPRINT,
                                    contract_type,
                                    duration,
                                    api_bet_id,
                                    [&,symbol](const intrade_bar::IntradeBarApi::Bet &bet) {
                                send_bet(bet);
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    //send_open_bet(api);
                                    PrintThread{} << "bet " << bet.symbol_name
                                        << " id: " << bet.broker_bet_id
                                        << " server " << xtime::get_str_date_time(api.get_server_timestamp())
                                        << " opening " << xtime::get_str_date_time(bet.opening_timestamp)
                                        << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    PrintThread{} << "win, id: " << bet.broker_bet_id << std::endl;
                                    api.update_balance();
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    api.update_balance();
                                    PrintThread{} << "loss, id: " << bet.broker_bet_id << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    //send_open_bet(api);
                                    PrintThread{} << "deal opennig error (server response)" << std::endl;
                                }
                            });
                            if(err != intrade_bar_common::ErrorType::OK) {
                                PrintThread{} << "deal opening error, symbol: " << symbol << std::endl;

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR;
                                bet.symbol_name = symbol;
                                bet.note = strategy_name;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                {
                                    //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                    //open_bets[bet.api_bet_id + error_bet_id_offset + error_bet_id] = bet;
                                    ++error_bet_id;
                                }
                                //send_open_bet(api);
                                send_bet(bet);
                            } else {
                                api.update_balance();

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE;
                                bet.symbol_name = symbol;
                                bet.note = strategy_name;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                {
                                    //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                    //open_bets[bet.api_bet_id] = bet;
                                }
                                //send_open_bet(api);
                                send_bet(bet);
                                PrintThread{} << "deal open, symbol: " << symbol << std::endl;
                            } //
                        } else
                        if (symbol.size() > 0 &&
                            amount > 0 &&
                            (contract_type == intrade_bar_common::BUY ||
                             contract_type == intrade_bar_common::SELL) &&
                            date_expiry > 0) {
                            /* открываем сделку типа CLASSIC */
                            int err = api.open_bo(
                                    symbol,
                                    strategy_name,
                                    amount,
                                    intrade_bar_common::TypesBinaryOptions::CLASSIC,
                                    contract_type,
                                    date_expiry,
                                    api_bet_id,
                                    [&,symbol](const intrade_bar::IntradeBarApi::Bet &bet) {
                                send_bet(bet);
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    //send_open_bet(api);
                                    PrintThread{} << "bet " << bet.symbol_name
                                        << " id: " << bet.broker_bet_id
                                        << " server " << xtime::get_str_date_time(api.get_server_timestamp())
                                        << " opening " << xtime::get_str_date_time(bet.opening_timestamp)
                                        << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    api.update_balance();
                                    PrintThread{} << "win, id: " << bet.broker_bet_id << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    api.update_balance();
                                    PrintThread{} << "loss, id: " << bet.broker_bet_id << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                    {
                                        //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                        //open_bets[bet.api_bet_id] = bet;
                                    }
                                    //send_open_bet(api);
                                    PrintThread{} << "deal opennig error (server response)" << std::endl;
                                }
                            });
                            if(err != intrade_bar_common::ErrorType::OK) {
                                PrintThread{} << "deal opening error, symbol: " << symbol << std::endl;

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR;
                                bet.symbol_name = symbol;
                                bet.note = strategy_name;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                {
                                    //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                    //open_bets[bet.api_bet_id  + error_bet_id_offset + error_bet_id] = bet;
                                    ++error_bet_id;
                                }
                                //send_open_bet(api);
                                send_bet(bet);
                            } else {
                                api.update_balance();

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE;
                                bet.symbol_name = symbol;
                                bet.note = strategy_name;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                {
                                    //std::lock_guard<std::mutex> lock(open_bets_mutex);
                                    //open_bets[bet.api_bet_id] = bet;
                                }
                                //send_open_bet(api);
                                send_bet(bet);
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
                connection_counter--;
                PrintThread{} << "close, handle: " << connection->get_handle() << std::endl;
            };
            server->on_error = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::error_code &ec) {
                PrintThread{} << "error, handle: " << connection->get_handle() << ", what " << ec.value() << std::endl;
            };

            /* запускаем сервер */
            server->start();

        }

        void update_connection(intrade_bar::IntradeBarApi &api) {
            static bool last_connected = false;
            if(last_connected == api.connected()) return;
            if(api.connected()) server->send_all("{\"connection\":1}");
            else server->send_all("{\"connection\":0}");
            last_connected = api.connected();
            PrintThread{} << "connection: " << last_connected << std::endl;
        }

        void update_ping(const int delay) {
            const int MAX_TICK = 10000;
            static int tick = 0;
            tick += delay;
            if(tick > MAX_TICK) {
                server->send_all("{\"ping\":1}");
                tick = 0;
            }
        }

        void update_balance(intrade_bar::IntradeBarApi &api, const int delay) {
            static double balance = 0;
            if(balance != api.get_balance()) {
                balance = api.get_balance();
                server->send_all("{\"balance\":" + std::to_string(balance) + ",\"rub\":" + std::to_string(api.account_rub_currency()) + "}");
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

        void update_prices(intrade_bar::IntradeBarApi &api) {
            std::string msg("{\"prices\":[");
            for(size_t i = 0; i < intrade_bar_common::CURRENCY_PAIRS; ++i) {
                const double p = api.get_price(i);
                //const std::string s = intrade_bar_common::currency_pairs[i];
                msg += format("%.5f",p);
                if(i != (intrade_bar_common::CURRENCY_PAIRS - 1)) msg += ",";
            }
            msg += "]}";
            server->send_all(msg);
        }

        bool check_exit(const int delay) {
            const size_t connections = server->get_connections();
            if(connections == 0) null_connections_counter += delay;
            else null_connections_counter = 0;
            if(null_connections_counter > 10000) return true;
            return false;
        }
    };
};

#endif // INTRADE_BAR_CONSOLE_BOT_NAMED_PIPE_SERVER_HPP_INCLUDED
