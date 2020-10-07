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

#define PROGRAM_VERSION "2.3"
#define PROGRAM_DATE "04.10.2020"

int main(int argc, char **argv) {
    std::cout << "intrade.bar console bot " PROGRAM_VERSION << std::endl << std::endl;
    std::cout << "intrade.bar bot: start" << std::endl;
    intrade_bar_console_bot::Settings settings(argc, argv);
    if(settings.is_error) {
        std::cout << "intrade.bar bot: settings error!" << std::endl;
        intrade_bar_console_bot::wait_space();
        return EXIT_FAILURE;
    }

    while(true) {
        intrade_bar::IntradeBarApi api(settings.point,0,nullptr,false,true,false,false,settings.sert_file,settings.cookie_file,"","","");
        double balance = 0.0;
        /* подключаемся к брокеру */
        int err = api.connect(settings.email, settings.password, settings.is_demo_account, settings.is_rub_currency);
        if(err != intrade_bar_common::OK) {
            std::cout << "intrade.bar bot: authorization error! Are all the settings set exactly?" << std::endl;
            intrade_bar_console_bot::wait_space();
            return EXIT_FAILURE;
        }
        /* обновляем баланс */
        api.update_balance(false);
        balance = api.get_balance();
        api.set_bets_delay((double)settings.delay_bets_ms / 1000.0d);

        std::cout << "intrade.bar bot: authorization successful!" << std::endl << std::endl;
        std::cout << "intrade.bar bot: email = " << settings.email << std::endl;
        std::cout << "intrade.bar bot: balance = " << balance << std::endl;
        std::cout << "intrade.bar bot: demo = " << api.demo_account() << std::endl;
        std::cout << "intrade.bar bot: rub currency = " << api.account_rub_currency() << std::endl;
        std::cout << "intrade.bar bot: delay between bets = " << settings.delay_bets_ms << " ms" << std::endl;
        std::cout << std::endl;

        intrade_bar_console_bot::Bot bot;
        bot.init_named_pipe_server(settings.named_pipe, api);

        int last_second_minute = 0;
        int second_minute = 0;
        xtime::timestamp_t restart_timestamp = settings.is_reboot ? xtime::get_last_timestamp_day(xtime::get_timestamp()) : xtime::get_timestamp(1,1,2100);
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
            if(settings.is_price_stream) bot.update_prices(api);
            bot.check_exit(DEALY);
            std::this_thread::yield();
        }
        std::cout << "intrade.bar bot: restart" << std::endl;
    }
    return 0;
}
