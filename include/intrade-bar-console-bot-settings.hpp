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
#ifndef INTRADE_BAR_CONSOLE_BOT_SETTINGS_HPP_INCLUDED
#define INTRADE_BAR_CONSOLE_BOT_SETTINGS_HPP_INCLUDED

#include "intrade-bar-console-bot-common.hpp"

namespace intrade_bar_console_bot {

    /** \brief Класс настроек
     */
    class Settings {
    public:
        std::string json_settings_file;                     /**< Файл настроек */
        std::string email;
        std::string password;
        std::string sert_file = "curl-ca-bundle.crt";       /**< Файл сертификата */
        std::string cookie_file = "intrade-bar.cookie";     /**< Файл cookie */
        std::string named_pipe = "intrade_bar_console_bot"; /**< Имя именованного канала */
        bool is_demo_account = true;    /**< Флаг использования демо счета */
        bool is_rub_currency = true;    /**< Флаг использования рублевого счета */
        bool is_error = false;

        Settings() {};

        Settings(const int argc, char **argv) {
            /* обрабатываем аргументы командой строки */
            json j;
            bool is_default = false;
            if(!process_arguments(
                    argc,
                    argv,
                    [&](
                        const std::string &key,
                        const std::string &value) {
                /* аргумент json_file указываает на файл с настройками json */
                if(key == "json_settings_file" || key == "jsf") {
                    json_settings_file = value;
                } else
                if(key == "email" || key == "e") {
                    email = value;
                } else
                if(key == "password" || key == "pass" || key == "p") {
                    password = value;
                } else
                if(key == "demo_account" || key == "demo" || key == "d") {
                    is_demo_account = true;
                } else
                if(key == "real_account" || key == "real" || key == "r") {
                    is_demo_account = false;
                } else
                if(key == "rub_currency" || key == "rub") {
                    is_rub_currency = true;
                } else
                if(key == "usd_currency" || key == "usd") {
                    is_rub_currency = false;
                } else
                if(key == "named_pipe" || key == "np") {
                    named_pipe = value;
                }
            })) {
                /* параметры не были указаны */
                if(!open_json_file("config.json", j)) {
                    is_error = true;
                    return;
                }
                is_default = true;
            }

            if(json_settings_file.size() == 0 && !is_default) {
                if(password.size() == 0 || email .size() == 0) is_error = true;
                return;
            }

            if(!is_default && !open_json_file(json_settings_file, j)) {
                is_error = true;
                return;
            }

            /* разбираем json сообщение */
            try {
                if(j["email"] != nullptr) email = j["email"];
                if(j["password"] != nullptr) password = j["password"];
                if(j["demo"] != nullptr) is_demo_account = j["demo"];
                if(j["demo_account"] != nullptr) is_demo_account = j["demo_account"];
                if(j["rub"] != nullptr) is_rub_currency = j["rub"];
                if(j["rub_currency"] != nullptr) is_rub_currency = j["rub_currency"];
                if(j["named_pipe"] != nullptr) named_pipe = j["named_pipe"];
            }
            catch(...) {
                is_error = true;
            }
            if(password.size() == 0 || email.size() == 0) is_error = true;
        }
    };



};

#endif // INTRADE_BAR_CONSOLE_BOT_SETTINGS_HPP_INCLUDED
