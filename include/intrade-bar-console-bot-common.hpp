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
#ifndef INTRADE_BAR_CONSOLE_BOT_COMMON_HPP_INCLUDED
#define INTRADE_BAR_CONSOLE_BOT_COMMON_HPP_INCLUDED

#include <iostream>
#include <fstream>
#include <sstream>
#include <mutex>
#include <nlohmann/json.hpp>

namespace intrade_bar_console_bot {
    using json = nlohmann::json;

    /** \brief Открыть файл JSON
     *
     * Данная функция прочитает файл с JSON и запишет данные в JSON структуру
     * \param file_name Имя файла
     * \param auth_json Структура JSON с данными из файла
     * \return Вернет true в случае успешного завершения
     */
    bool open_json_file(const std::string &file_name, json &auth_json) {
        std::ifstream auth_file(file_name);
        if(!auth_file) {
            std::cerr << "intrade_bar_console_bot::ifstream, open file " << file_name << " error" << std::endl;
            return false;
        }
        try {
            auth_file >> auth_json;
        }
        catch (json::parse_error &e) {
            std::cerr << "intrade_bar_console_bot::open_json_file, json parser error: " << std::string(e.what()) << std::endl;
            auth_file.close();
            return false;
        }
        catch (std::exception e) {
            std::cerr << "intrade_bar_console_bot::open_json_file, json parser error: " << std::string(e.what()) << std::endl;
            auth_file.close();
            return false;
        }
        catch(...) {
            std::cerr << "intrade_bar_console_bot::open_json_file, json parser error" << std::endl;
            auth_file.close();
            return false;
        }
        auth_file.close();
        return true;
    }

    /** \brief Обработать аргументы
     *
     * Данная функция обрабатывает аргументы от командной строки, возвращая
     * результат как пара ключ - значение.
     * \param argc количество аргуметов
     * \param argv вектор аргументов
     * \param f лябмда-функция для обработки аргументов командной строки
     * \return Вернет true если ошибок нет
     */
    bool process_arguments(
        const int argc,
        char **argv,
        std::function<void(
            const std::string &key,
            const std::string &value)> f) noexcept {
        if(argc <= 1) return false;
        bool is_error = true;
        for(int i = 1; i < argc; ++i) {
            std::string key = std::string(argv[i]);
            if(key.size() > 0 && (key[0] == '-' || key[0] == '/')) {
                uint32_t delim_offset = 0;
                if(key.size() > 2 && (key.substr(2) == "--") == 0) delim_offset = 1;
                std::string value;
                if((i + 1) < argc) value = std::string(argv[i + 1]);
                is_error = false;
                if(f != nullptr) f(key.substr(delim_offset), value);
            }
        }
        return !is_error;
    }

    /** \brief Форматированная строка
     */
    std::string format(const char *fmt, ...) {
        va_list args;
        va_start(args, fmt);
        std::vector<char> v(1024);
        while (true)
        {
            va_list args2;
            va_copy(args2, args);
            int res = vsnprintf(v.data(), v.size(), fmt, args2);
            if ((res >= 0) && (res < static_cast<int>(v.size())))
            {
                va_end(args);
                va_end(args2);
                return std::string(v.data());
            }
            size_t size;
            if (res < 0)
                size = v.size() * 2;
            else
                size = static_cast<size_t>(res) + 1;
            v.clear();
            v.resize(size);
            va_end(args2);
        }
    }

    class PrintThread: public std::ostringstream {
    private:
        static inline std::mutex _mutexPrint;

    public:
        PrintThread() = default;

        ~PrintThread() {
            std::lock_guard<std::mutex> guard(_mutexPrint);
            std::cout << this->str();
        }
    };
};

#endif // INTRADE_BAR_CONSOLE_BOT_COMMON_HPP_INCLUDED
