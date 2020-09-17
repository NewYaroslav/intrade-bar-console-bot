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
#define INTRADE_BAR_BOT_VERSION_B

#include "intrade-bar-console-bot.hpp"

#define PROGRAM_VERSION "2.0"
#define PROGRAM_DATE "17.09.2020"

int main(int argc, char **argv) {
    std::cout << "intrade-bar-console-bot " PROGRAM_VERSION << std::endl << std::endl;
    std::cout << "start" << std::endl;
    intrade_bar_console_bot::Settings settings(argc, argv);
    if(settings.is_error) {
        std::cout << "Settings error!" << std::endl;
        return EXIT_FAILURE;
    }

    while(true) {
        intrade_bar::IntradeBarApi api(0,nullptr,false,true,false,false,settings.sert_file,settings.cookie_file,"","","");
        double balance = 0.0;
        /* подключаемся к брокеру */
        int err = api.connect(settings.email, settings.password, settings.is_demo_account, settings.is_rub_currency);
        if(err != intrade_bar_common::OK) {
            std::cout << "Authorization error! Are all the settings set exactly?" << std::endl;
            std::cout << "email: " << settings.email << std::endl;
            return EXIT_FAILURE;
        }
        /* обновляем баланс */
        api.update_balance(false);
        balance = api.get_balance();
        std::cout << std::endl << "Authorization successful!" << std::endl;
        std::cout << "email: " << settings.email << std::endl;
        std::cout << "balance: " << balance << std::endl;
        std::cout << "demo: " << api.demo_account() << std::endl;
        std::cout << "rub currency: " << api.account_rub_currency() << std::endl;
        std::cout << "delay between bets: " << settings.delay_bets_ms << " ms" << std::endl;
        std::cout << std::endl;

        api.set_bets_delay((double)settings.delay_bets_ms / 1000.0d);
        intrade_bar_console_bot::Bot bot;
        bot.is_program_bot = true;
        bot.init_named_pipe_server(settings.named_pipe, api);

        int last_second_minute = 0;
        int second_minute = 0;
        xtime::timestamp_t restart_timestamp = xtime::get_last_timestamp_day(xtime::get_timestamp());
        while(xtime::get_timestamp() < restart_timestamp) {
            while(true) {
                second_minute = xtime::get_second_minute(api.get_server_timestamp());
                if(second_minute != last_second_minute) break;
                const int DEALY = 10;
                std::this_thread::sleep_for(std::chrono::milliseconds(DEALY));
            };
            last_second_minute = second_minute;
            const int DEALY = 1000;
            bot.update_ping(DEALY);
            bot.update_balance(api, DEALY);
            bot.update_connection(api);
            bot.update_prices(api);
            bot.check_exit(DEALY);
            std::this_thread::yield();
        }
    }
    return 0;
}
