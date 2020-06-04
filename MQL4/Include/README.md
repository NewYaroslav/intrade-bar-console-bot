## Файлы библиотек

* *hash.mqh* и *json.mqh* - источник: https://www.mql5.com/en/code/11134 библиотеки отредактированы *winloosewin*: https://www.mql5.com/en/forum/28928/page4#comment_14321676
* *named_pipe_client.mqh* - источник: https://github.com/NewYaroslav/simple-named-pipe-server
* *intrade_bar_console_bot_api.mqh* - библиотека данного проекта

## Основные методы

Класс, расположенный в файле *intrade_bar_console_bot_api.mqh*, имеет следующие методы

**bool connect(string api_pipe_name)**

подключиться к программе-серверу по именованному каналу

**bool connected()**

проверить состояние соединения к программе-серверу по именованному каналу

**bool open_deal(string symbol, int direction, datetime expiration, int type, double amount)**

**bool open_deal(string symbol, string strategy_name, int direction, datetime expiration, int type, double amount)** 

открыть сделку

* *symbol* - имя символа (например, EURUSD).
* *direction* - направление ставки. Допустимсые значения:

```cpp
enum ENUM_BO_ORDER_TYPE {
	BUY = 0,
	SELL = 1,
};
```

* *strategy_name* - Имя стратегии. Можно не указывать.
* *expiration* - Экспирация, в минутах.
* *type* - Тип сделки. Допустимсые значения:

```cpp
enum ENUM_BO_TYPE {
	CLASSICAL = 0,
	SPRINT = 1,
};
```

* *amount* - размер ставки.

**double get_balance()**

получить баланс

**bool check_balance_change()**

проверить изменения баланса

**bool check_broker_connection()**

проверить состояние соединения с брокером

**bool check_broker_connection_change()**

проверить изменение состояние соединения с брокером

**void update(int delay)**

метод для обработки входящих сообщений. Вызывать после всех расчетов советника или индикатора. На вход метод принимает размер задержки времени между ее вызовами. Размер задержки времени нужен для внутренней работы класса, чтобы отправляыть сообщения ping

**void close()**
  
закрыть соединение



