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
    private:

        std::shared_ptr<SimpleNamedPipe::NamedPipeServer> server;               /**< Сервер именованных каналов */
        std::recursive_mutex server_mutex;

        std::atomic<int> null_connections_counter = ATOMIC_VAR_INIT(0);         /**< Счетчик отсутстивя соединений. Нужен для автоматического выхода из программы */
        std::atomic<int> update_ping_tick = ATOMIC_VAR_INIT(0);
        std::atomic<int> update_balance_tick = ATOMIC_VAR_INIT(0);
        std::atomic<double> last_balance = ATOMIC_VAR_INIT(-1.0);
        std::atomic<bool> is_last_connected = ATOMIC_VAR_INIT(false);
        std::atomic<bool> is_init = ATOMIC_VAR_INIT(false);
    public:
        Bot() {};
        ~Bot() {};

        void send_bet(const intrade_bar::IntradeBarApi::Bet &new_bet) {
            if(!is_init) return;
            std::string text("{\"update_bet\":");
            text += "{\"s\":\"";
            text += new_bet.symbol_name;
            text += "\",\"note\":\"";
            text += new_bet.note;
            text += "\",\"aid\":";
            text += std::to_string(new_bet.api_bet_id);
            text += ",\"id\":";
            text += std::to_string(new_bet.broker_bet_id);
            text += ",\"op\":";
            text += std::to_string(new_bet.open_price);
            text += ",\"dir\":";
            if(new_bet.contract_type == intrade_bar_common::BUY) text += "\"buy\"";
            else if(new_bet.contract_type == intrade_bar_common::SELL) text += "\"sell\"";
            else text += "\"none\"";
            text += ",\"status\":";
            if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) text += "\"win\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) text += "\"loss\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) text += "\"open_error\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) text += "\"wait\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE) text += "\"unknown\"";
            else if(new_bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::CHECK_ERROR) text += "\"error\"";
            else text += "\"none\"";
            text += ",\"a\":";
            text += std::to_string(new_bet.amount);
            text += ",\"profit\":";
            text += std::to_string(new_bet.profit);
            text += ",\"payout\":";
            text += std::to_string(new_bet.payout);
            text += ",\"dur\":";
            text += std::to_string(new_bet.duration);
            text += ",\"ot\":";
            text += std::to_string(new_bet.opening_timestamp);
            text += ",\"st\":";
            text += std::to_string(new_bet.send_timestamp);
            text += ",\"ct\":";
            text += std::to_string(new_bet.closing_timestamp);
            text += "}}";
            std::lock_guard<std::recursive_mutex> lock(server_mutex);
            if(server) server->send_all(text);
        }

        /** \brief Инициализировать сервер named pipe
         * \param named_pipe Именнованные каналы
         * \param api API брокера
         */
        void init_named_pipe_server(
                const std::string &named_pipe,
                intrade_bar::IntradeBarApi &api) {
            is_init = false;
            is_last_connected = false;
            update_ping_tick = 0;
            update_balance_tick = 0;
            last_balance = -1.0;
            std::lock_guard<std::recursive_mutex> lock(server_mutex);
            server = std::make_shared<SimpleNamedPipe::NamedPipeServer>(named_pipe);

            server->on_open = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
                PrintThread{} << "intrade.bar bot: open connection with MT4, handle = " << connection->get_handle() << std::endl;
                if(api.connected()) {
                    const double balance = api.get_balance();
                    server->send_all(
                        "{\"b\":" + std::to_string(balance) +
                        ",\"rub\":" + std::to_string(api.account_rub_currency()) +
                        ",\"demo\":" + std::to_string(api.demo_account()) +
                        "}");
                    server->send_all("{\"conn\":1}");
                } else {
                    server->send_all("{\"conn\":0}");
                }
            };

            server->on_message = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::string &in_message) {
                /* обрабатываем входящие сообщения */

                //std::cout << "message " << in_message << ", handle: " << connection->get_handle() << std::endl;

                /* парисм */
                try {
                    json j = json::parse(in_message);

                    if(j.find("pong") != j.end()) {
                        PrintThread{} << "intrade.bar bot: MT4 send pong" << std::endl;
                    } else
                    if(j.find("ping") != j.end()) {
                        PrintThread{} << "intrade.bar bot: MT4 send ping" << std::endl;
                    } else
                    if(j.find("contract") != j.end()) {

                        /* пришел запрос на открытие сделки */

                        json j_contract = j["contract"];

                        /* параметры опциона */
                        std::string symbol;
                        std::string note;
                        double amount = 0;
                        int contract_type = intrade_bar_common::INVALID_ARGUMENT;
                        uint32_t duration = 0;
                        xtime::timestamp_t date_expiry = 0;
                        uint64_t api_bet_id = 0;

                        /* получаем все параметры опицона */
                        if(j_contract.find("s") != j_contract.end()) {
                            symbol = j_contract["s"];
                        }
                        if(j_contract.find("note") != j_contract.end()) {
                            note = j_contract["note"];
                        }
                        if(j_contract.find("a") != j_contract.end()) {
                            amount = j_contract["a"];
                        }
                        if(j_contract.find("dir") != j_contract.end()) {
                            if(j_contract["dir"] == "BUY") contract_type = intrade_bar_common::BUY;
                            else if(j_contract["dir"] == "SELL") contract_type = intrade_bar_common::SELL;
                        }
                        if(j_contract.find("exp") != j_contract.end()) {
                            if(j_contract["exp"].is_number()) {
                                const xtime::timestamp_t timestamp_server = api.get_server_timestamp();
                                xtime::timestamp_t temp_date_expiry = j_contract["exp"];
                                if(temp_date_expiry < xtime::SECONDS_IN_DAY) {
                                    /* пользователь задал время экспирации */
                                    date_expiry = intrade_bar_common::get_classic_bo_closing_timestamp(timestamp_server, temp_date_expiry);
                                } else {
                                    date_expiry = temp_date_expiry;
                                }
                                //std::cout << "date_expiry: " << xtime::get_str_date_time(date_expiry) << std::endl;
                            }
                        } else
                        if(j_contract.find("dur") != j_contract.end()) {
                            if(j_contract["dur"].is_number()) {
                                duration = j_contract["dur"];
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
                                    note,
                                    amount,
                                    intrade_bar_common::TypesBinaryOptions::SPRINT,
                                    contract_type,
                                    duration,
                                    api_bet_id,
                                    [&,symbol](const intrade_bar::IntradeBarApi::Bet &bet) {
                                send_bet(bet);
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE) {
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) {
                                    PrintThread{} << "intrade.bar bot: bo-bet, symbol = " << bet.symbol_name
                                        << ", id = " << bet.broker_bet_id
                                        << ", st " << xtime::get_str_date_time(api.get_server_timestamp())
                                        << ", ot " << xtime::get_str_date_time(bet.opening_timestamp)
                                        << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                    PrintThread{} << "intrade.bar bot: " << bet.symbol_name << " win, id = " << bet.broker_bet_id << std::endl;
                                    api.update_balance();
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                    api.update_balance();
                                    PrintThread{} << "intrade.bar bot: " << bet.symbol_name << " loss, id = " << bet.broker_bet_id << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                    PrintThread{} << "intrade.bar bot: bo-bet opennig error (server response), symbol = " << symbol << std::endl;
                                }
                            });
                            if(err != intrade_bar_common::ErrorType::OK) {
                                PrintThread{} << "intrade.bar bot: bo-bet opening error, symbol = " << symbol << std::endl;

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR;
                                bet.symbol_name = symbol;
                                bet.note = note;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                send_bet(bet);
                            } else {
                                api.update_balance();
                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE;
                                bet.symbol_name = symbol;
                                bet.note = note;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                send_bet(bet);
                                PrintThread{} << "intrade.bar bot: bo-bet open, symbol = " << symbol << std::endl;
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
                                    note,
                                    amount,
                                    intrade_bar_common::TypesBinaryOptions::CLASSIC,
                                    contract_type,
                                    date_expiry,
                                    api_bet_id,
                                    [&,symbol](const intrade_bar::IntradeBarApi::Bet &bet) {
                                send_bet(bet);
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE) {

                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WAITING_COMPLETION) {
                                    PrintThread{} << "intrade.bar bot: bo-bet, symbol = " << bet.symbol_name
                                        << ", id = " << bet.broker_bet_id
                                        << ", st " << xtime::get_str_date_time(api.get_server_timestamp())
                                        << ", ot " << xtime::get_str_date_time(bet.opening_timestamp)
                                        << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::WIN) {
                                    api.update_balance();
                                    PrintThread{} << "intrade.bar bot: " << bet.symbol_name << " win, id = " << bet.broker_bet_id << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::LOSS) {
                                    api.update_balance();
                                    PrintThread{} << "intrade.bar bot: " << bet.symbol_name << " loss, id = " << bet.broker_bet_id << std::endl;
                                } else
                                if(bet.bet_status == intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR) {
                                    PrintThread{} << "intrade.bar bot: bo-bet opennig error (server response), symbol = " << symbol << std::endl;
                                }
                            });
                            if(err != intrade_bar_common::ErrorType::OK) {
                                PrintThread{} << "intrade.bar bot: bo-bet opening error, symbol = " << symbol << std::endl;

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::OPENING_ERROR;
                                bet.symbol_name = symbol;
                                bet.note = note;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;
                                send_bet(bet);
                            } else {
                                api.update_balance();

                                intrade_bar::IntradeBarApi::Bet bet;
                                bet.api_bet_id = api_bet_id;
                                bet.bet_status = intrade_bar::IntradeBarHttpApi::BetStatus::UNKNOWN_STATE;
                                bet.symbol_name = symbol;
                                bet.note = note;
                                bet.amount = amount;
                                bet.contract_type = contract_type;
                                bet.send_timestamp = api.get_server_timestamp();
                                bet.duration = duration;

                                send_bet(bet);
                                PrintThread{} << "intrade.bar bot: open bo-bet, symbol = " << symbol << std::endl;
                            } //
                        } else {
                            PrintThread{} << "intrade.bar bot: bo-bet parameters error, symbol = " << symbol << std::endl;
                        }

                    }
                }
                catch(...) {
                    PrintThread{} << "intrade.bar bot: error, json::parse" << std::endl;
                }
            };
            server->on_close = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection) {
                PrintThread{} << "intrade.bar bot: close connection with MT4, handle = " << connection->get_handle() << std::endl;
            };
            server->on_error = [&](SimpleNamedPipe::NamedPipeServer::Connection* connection, const std::error_code &ec) {
                PrintThread{} << "intrade.bar bot: error connection with MT4, handle = " << connection->get_handle() << ", what " << ec.value() << std::endl;
            };

            /* запускаем сервер */
            server->start();
            is_init = true;
        }

        /** \brief Обновить состояние соединения
         * \param api API брокера
         */
        void update_connection(intrade_bar::IntradeBarApi &api) {
            if(!is_init) return;
            if(is_last_connected == api.connected()) return;

            if(api.connected()) {
                std::lock_guard<std::recursive_mutex> lock(server_mutex);
                if(server) server->send_all("{\"conn\":1}");
            } else {
                std::lock_guard<std::recursive_mutex> lock(server_mutex);
                if(server) server->send_all("{\"conn\":0}");
            }
            is_last_connected = api.connected();
            PrintThread{} << "intrade.bar bot: connection: " << is_last_connected << std::endl;
        }

        /** \brief Обновить пинг
         * \param delay Задержка между вызовом метода
         */
        void update_ping(const int delay) {
            if(!is_init) return;
            const int MAX_TICK = 30000;
            update_ping_tick += delay;
            if(update_ping_tick > MAX_TICK) {
                update_ping_tick = 0;
                std::lock_guard<std::recursive_mutex> lock(server_mutex);
                if(server) {
                    server->send_all("{\"ping\":1}");
                    PrintThread{} << "intrade.bar bot: send ping" << std::endl;
                }
            }
        }

        /** \brief Обновить баланс счета
         * \param api API брокера
         * \param delay Задержка между вызовом метода
         */
        void update_balance(intrade_bar::IntradeBarApi &api, const int delay) {
            if(!is_init) return;
            if(last_balance != api.get_balance()) {
                last_balance = api.get_balance();
                PrintThread{} << "intrade.bar bot: balance: " << last_balance << std::endl;
                std::lock_guard<std::recursive_mutex> lock(server_mutex);
                if(server) server->send_all(
                    "{\"b\":" + std::to_string(last_balance) +
                    ",\"rub\":" + std::to_string(api.account_rub_currency()) +
                    ",\"demo\":" + std::to_string(api.demo_account()) +
                    "}");
            }
            const int MAX_TICK = 10000;
            update_balance_tick += delay;
            if(update_balance_tick > MAX_TICK) {
                update_balance_tick = 0;
                api.update_balance();
            }
        }

        /** \brief Обновить цены
         * \param api API брокера
         */
        void update_prices(intrade_bar::IntradeBarApi &api) {
            if(!is_init) return;
            std::string msg("{\"prices\":[");
            for(size_t i = 0; i < intrade_bar_common::CURRENCY_PAIRS; ++i) {
                const double p = api.get_price(i);
                //const std::string s = intrade_bar_common::currency_pairs[i];
                msg += format("%.5f",p);
                if(i != (intrade_bar_common::CURRENCY_PAIRS - 1)) msg += ",";
            }
            msg += "]}";
            std::lock_guard<std::recursive_mutex> lock(server_mutex);
            if(server) server->send_all(msg);
        }

        /** \brief Проверить выход из программы
         * \param delay Задержка между вызовом метода
         */
        bool check_exit(const int delay) {
            if(!is_init) return false;
            std::lock_guard<std::recursive_mutex> lock(server_mutex);
            if(!server) return false;
            const size_t connections = server->get_connections();
            if(connections == 0) null_connections_counter += delay;
            else null_connections_counter = 0;
            if(null_connections_counter > 30000) return true;
            return false;
        }
    };
};

#endif // INTRADE_BAR_CONSOLE_BOT_NAMED_PIPE_SERVER_HPP_INCLUDED
