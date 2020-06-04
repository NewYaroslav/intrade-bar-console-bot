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
#include "intrade-bar-console-bot.hpp"

int main(int argc, char **argv) {
    std::cout << "start intrade-bar-console-bot" << std::endl;
    intrade_bar_console_bot::Settings settings(argc, argv);
    if(settings.is_error) {
        std::cout << "Settings error!" << std::endl;
        return EXIT_FAILURE;
    }


    while(true) {
        intrade_bar::IntradeBarApi api(0,nullptr,false,settings.sert_file,settings.cookie_file,"","","");
        double balance = 0.0;
        /* подключаемся к брокеру */
        int err = api.connect(settings.email,settings.password,settings.is_demo_account,settings.is_rub_currency);
        if(err != intrade_bar_common::OK) {
            std::cout << "Authorization error! Are all the settings set exactly?" << std::endl;
            std::cout << "email: " << settings.email << std::endl;
            return EXIT_FAILURE;
        }
        /* обновляем баланс */
        api.update_balance(false);
        balance =  api.get_balance();
        std::cout << "authorization successful!" << std::endl;
        std::cout << "email: " << settings.email << std::endl;
        std::cout << "balance: " << balance << std::endl;
        std::cout << "demo: " << api.demo_account() << std::endl;
        std::cout << "rub currency: " << api.account_rub_currency() << std::endl;

        /* далее реализуем named pipe сервер */
        std::shared_ptr<SimpleNamedPipe::NamedPipeServer> named_pipe_server;
        intrade_bar_console_bot::init_named_pipe_server(named_pipe_server, settings, api);

        xtime::timestamp_t restart_timestamp = xtime::get_last_timestamp_day(xtime::get_timestamp());
        while(xtime::get_timestamp() < restart_timestamp) {
            const int DEALY = 1000;
            std::this_thread::sleep_for(std::chrono::milliseconds(DEALY));
            intrade_bar_console_bot::update_ping(named_pipe_server, DEALY);
            intrade_bar_console_bot::update_balance(named_pipe_server, api, DEALY);
            intrade_bar_console_bot::update_connection(named_pipe_server, api);
        }
    }
    return 0;
}
