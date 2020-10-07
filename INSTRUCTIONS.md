# intrade-bar-console-bot
![logo](doc/logo_mini.png)

*Консольный робот для торговли у брокера* https://intrade.bar/




## Особенности

* Бесплатный бот, без регистрации по рефералки и СМС.
* Полностью открытый исходный код.
* Написан на легком, понятном и удобном C++11.
* Наличие REST API.
* Библиотека для связи советника из МТ4 c программой.




## Как использовать

Готовые сборки программы находятся в архивах в папке *bin*, как для x64 так и для x32.

Если вы хотите сами собрать бота, но имете мало опыта в программировании, настоятельно рекомендую использовать *Code::Blocks*, чтобы не заниматься переносом проекта в другую среду.
Также в *Code::Blocks* сначала рекоменду настроить компилятор, для х64 скачайте и установите *x86_64-7.3.0-release-posix-seh-rt_v5-rev0*, а для  x32 *i686-7.3.0-release-posix-dwarf-rt_v5-rev0*.

### Настройка бота

Бот в момент запуска ищет файл **config.json**, который расположен в папке с программой. Файл **config.json** содержит [json структуру](https://json.org/json-ru.html), которая определяет настройки бота.

Настройки бота в файле выглядят примерно так:

```json
{
	"point":"1.intrade.bar",
	"email":"example@mail.com",
	"password":"12345",
	"demo_account":true,
	"rub_currency":false,
	"reboot":true,
	"price_stream":false,
	"delay_bets_ms":1000
}
```
* point - Точка доступа к брокеру
* email - Эмейл аккаунта
* password - Пароль аккаунта
* demo_account - Параметр определяет, использовать демо аккаунт, или реальный.
* rub_currency - Параметр определяет тип валюты баланса (RUB или USD)
* reboot - Перезагружать бота в 0:00 по UTC или нет
* price_stream - Флаг, включает поток котировок в совтеник для МТ4. Котировки обновляются раз в 1 сек.
* delay_bets_ms - Задержка между сделками. Если бот получает две и более сделки подряд, то он может открывать сделки с задержкой, если она установлена. Задержка может быть нужна, если брокер по каким-то причинам не разрешает одновременные сделки.

Тем не менее, многие настройки можно задать и без файла, через команды. Пример:

```
intrade-bar-console-bot.exe -email "example@mail.com" -password "12345" /demo /usd
```

Выберите один из способов задания настроек и укажите свой *email* и *password*. Файл *config.json* можно отредактировать любым блокнотом. Не забудьте, что файл *config.json* это структура json, т.е. сохраняйте синтаксис (*не удаляйте скобочки, запятые*).

После запуска программы она будет ждать соединения через named pipe. При этом программа выступает в роли сервера именованных каналов. По умолчанию сервер называется *intrade_bar_console_bot*.
Робот отсылает через **named pipe** баланс, состояние соединения с брокером, цены и состояния сделок, а принимает команды на открытие сделок.

### Настройки советника МТ4

Советник для своей работы требует разрешения на использование *dll библиотек*. 



## Библиотека для MT4

Для работы с советником проект имеет библиотеку, которая упрощает взаимодействие по REST API

```cpp
#include "..\Include\intrade_bar_bot_api.mqh"

IntradeBarConsoleBotApi api;        // API для работы с роботом

void OnTimer() {
	/* проверяем наличие соединения */
	if(api.connected()) {
		int expiration = 3;
		ENUM_BO_TYPE bo_type = SPRINT;// Тип опциона
		double amount = 50;
		api.open_deal("EURUSD", "note: Bla bla bla", BUY, expiration, bo_type, amount);
	} else {
		/* соединение отсутствует, подключаемся */
		if(api.connect(pipe_name)) {
			Print("Успешное соединение с ", pipe_name);
		} else {
			Print("Соединение не удалось");
		}
	}
	/* обновляем состояние API */
	api.update(timer_period);
	if(api.check_balance_change()) {
		Print("Баланс: ",DoubleToString(api.get_balance(),2)); 
	}
	if(api.check_broker_connection_change()) {
		Print("Соединение с брокером: ",api.check_broker_connection()); 
	}
}

void IntradeBarConsoleBotApi::on_update_bet(IntradeBarBet &bet) {
   string str_status;
   if(bet.bet_status == IntradeBarBetStatus::WIN) str_status = "WIN";
   else if(bet.bet_status == IntradeBarBetStatus::LOSS) str_status = "LOSS";
   else if(bet.bet_status == IntradeBarBetStatus::UNKNOWN_STATE) str_status = "UNKNOWN_STATE";
   else if(bet.bet_status == IntradeBarBetStatus::CHECK_ERROR) str_status = "CHECK_ERROR";
   else if(bet.bet_status == IntradeBarBetStatus::OPENING_ERROR) str_status = "OPENING_ERROR";
   else if(bet.bet_status == IntradeBarBetStatus::WAITING_COMPLETION) str_status = "WAITING_COMPLETION";
   Print("update bet: signal_id = ", bet.note,", amount = ", bet.amount, " status = ", str_status);
}

void IntradeBarConsoleBotApi::on_update_prices(string &symbols[], double &prices[]) {
   Print(symbols[0], " price: ", prices[0]);
}
```

Все функции вы можете просмотреть в файле **intrade_bar_bot_api.mqh**.
Также не забудьте изучить пример советника для MT4.

### API бота

Бот имеет библиотеку для удобного взаимодействия с его API. Библиотека реализована в виде класса и расположена в файле *intrade_bar_bot_api.mqh*. 

Класс использует следующие структуры и перечисления:

```cpp
/// Состояния сделки
enum IntradeBarBetStatus {
   UNKNOWN_STATE,
   OPENING_ERROR,
   CHECK_ERROR,        /**< Ошибка проверки результата сделки */
   WAITING_COMPLETION,
   WIN,
   LOSS
};
   
/** \brief Структура для хранения параметров сделки в отчете о ее состоянии
 */
struct IntradeBarBet {
   uint api_bet_id;           /**< ID сделки внутри API */
   uint broker_bet_id;        /**< ID сделки у брокера */
   string symbol;             /**< Символ */
   string note;               /**< Заметка, которая передается в функции открытия сделки */
   int contract_type;         /**< Тип контракта BUY или SELL */
   ulong duration;            /**< Длительность контракта в секундах */
   ulong send_timestamp;      /**< Метка времени начала контракта */
   ulong opening_timestamp;   /**< Метка времени начала контракта */
   ulong closing_timestamp;   /**< Метка времени конца контракта */
   double amount;             /**< Размер ставки в RUB или USD */
   double profit;             /**< Размер выиграша */
   double payout;             /**< Процент выплат */
   double open_price;         /**< Цена открытия сделки */
   IntradeBarBetStatus bet_status;  /**< Состояние сделки */
};

class IntradeBarConsoleBotApi {
public:
	//...
	
	/// Типы ордеров (направление ставки)
	enum ENUM_BO_ORDER_TYPE {
		BUY = 0,
		SELL = 1,
	};
   
	/// Типы бинарных опционов (Классические или спринт)
	enum ENUM_BO_TYPE {
		CLASSICAL = 0,
		SPRINT = 1,
	};
   
	//...
};
```

Класс имеет следующие методы:

```cpp
/** \brief Подключиться к боту
 * \param api_pipe_name Имя именованного канала
 * \return Вернет true, если соединение удалось
 */ 
bool connect(string api_pipe_name);
```

```cpp
/** \brief Состояние соединения
 * \return Вернет true, если есть соединение с ботом
 */ 
bool connected();
```

```cpp
/** \brief Открыть сделку
 * \param symbol Имя символа
 * \param direction Направление (BUY или SELL, 1 или -1).
 * \param expiration Экспирация (в минутах)
 * \param type Тип опциона (CLASSICAL или SPRINT)
 * \param amount Размер ставки в валюте счета
 * \return Вернет true в случае успешного отправления
 */
bool open_deal(
	string symbol, 
	int direction, 
	datetime expiration, 
	int type, 
	double amount);
```

```cpp
/** \brief Открыть сделку
 * \param symbol Имя символа
 * \param note Заметка. Она будет возвращена в функции обратного вызова состояния сделки
 * \param direction Направление (BUY или SELL, 1 или -1).
 * \param expiration Экспирация (в минутах)
 * \param type Тип опциона (CLASSICAL или SPRINT)
 * \param amount Размер ставки в валюте счета
 * \return Вернет true в случае успешного отправления
 */
bool open_deal(
	string symbol, 
	string note, 
	int direction, 
	datetime expiration, 
	int type, 
	double amount);
```

```cpp
/** \brief Получить баланс
 * \return Вернет размер баланса
 */
 double get_balance();
```

```cpp
/** \brief Проверить изменение баланса
 * \return Вернет true, если баланс изменился
 */
bool check_balance_change();
```

```cpp
/** \brief Проверить состояние подключения к брокеру
 * \return Вернет true, если есть подключение к брокеру
 */
bool check_broker_connection();
```

```cpp
/** \brief Проверить изменение состояния подключения к брокеру
 * \return Вернет true, если состояние подключения к брокеру изменилось
 */
bool check_broker_connection_change();
```

```cpp
/** \brief Callback-функция для получения состояния бинарных опционов
 * \param new_bet Структура параметров бинарного опциона
 */
virtual void on_update_bet(IntradeBarBet &new_bet);
```

Пример сallback-функции для получения состояния бинарных опционов:

```cpp
void IntradeBarConsoleBotApi::on_update_bet(IntradeBarBet &bet) {
   string str_status;
   if(bet.bet_status == IntradeBarBetStatus::WIN) str_status = "WIN";
   else if(bet.bet_status == IntradeBarBetStatus::LOSS) str_status = "LOSS";
   else if(bet.bet_status == IntradeBarBetStatus::UNKNOWN_STATE) str_status = "UNKNOWN_STATE";
   else if(bet.bet_status == IntradeBarBetStatus::CHECK_ERROR) str_status = "CHECK_ERROR";
   else if(bet.bet_status == IntradeBarBetStatus::OPENING_ERROR) str_status = "OPENING_ERROR";
   else if(bet.bet_status == IntradeBarBetStatus::WAITING_COMPLETION) str_status = "WAITING_COMPLETION";
   Print("update bet: signal_id = ", bet.note,", amount = ", bet.amount, " status = ", str_status);
}
```

```cpp
/** \brief Callback-функция для получения котировок валютных пар
 * \param symbols Массив символов
 * \param prices Массив цен
 */
virtual void on_update_prices(string &symbols[], double &prices[]);
```

Пример сallback-функции для получения котировок брокера:

```cpp
void IntradeBarConsoleBotApi::on_update_prices(string &symbols[], double &prices[]) {
   //Print(symbols[0], " price: ", prices[0]);
}
```

```cpp
/** \brief Обновить состояние API
 * \param delay Задержка между вызовами функции
 */
void update(int delay);
```

```cpp
/** \brief Получить метку времени закрытия CLASSIC бинарного опциона
 * \param timestamp Метка времени (в секундах)
 * \param expiration Экспирация (в минутах)
 * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
 */
datetime get_classic_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration);
```

```cpp
/** \brief Получить метку времени закрытия CLASSIC бинарного опциона на закрытие бара
 * \param timestamp Метка времени (в секундах)
 * \param expiration Экспирация (в минутах)
 * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
 */
datetime get_candle_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration);
```

```cpp
/** \brief Получить проценты выплаты брокера
* \symbol Символ
* \duration Длительность опциона
* \param timestamp Метка времени
* \param amount Размер ставки
* \return Вернет процент выплаты брокера в виде дробного числа
*/
double calc_payout(string symbol, const ulong duration, const datetime timestamp, const double amount);
```

```cpp
/** \brief Проверить на демо аккаунт
 * \return Вернет true если демо аккаунт
 */
bool check_demo();
```

```cpp
/** \brief Проверить на рублевый аккаунт
 * \return Вернет true если аккаунт рублевый
 */
bool check_rub();
```

```cpp
/** \brief Закрыть соединение
 */
void close();
```
