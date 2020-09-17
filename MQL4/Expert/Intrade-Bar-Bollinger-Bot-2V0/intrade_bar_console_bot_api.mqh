//+------------------------------------------------------------------+
//|                                  intrade_bar_console_bot_api.mqh |
//|                         Copyright 2019-2020, Yaroslav Barabanov. |
//|                                https://t.me/BinaryOptionsScience |
//+------------------------------------------------------------------+
// Для версии бота 1.3
//+------------------------------------------------------------------+
#property copyright "Copyright 2019-2020, Yaroslav Barabanov."
#property link      "https://t.me/BinaryOptionsScience"
#property strict

#include "named_pipe_client.mqh"
#include "hash.mqh"
#include "json.mqh"
#include <Tools\DateTime.mqh>

/// Состояния сделки
enum IntradeBarBetStatus {
   UNKNOWN_STATE,
   OPENING_ERROR,
   CHECK_ERROR,        /**< Ошибка проверки результата сделки */
   WAITING_COMPLETION,
   WIN,
   LOSS
};
   
struct IntradeBarBet {
   uint api_bet_id;
   uint broker_bet_id;
   string symbol;
   string note;
   int contract_type;        /**< Тип контракта BUY или SELL */
   ulong duration;           /**< Длительность контракта в секундах */
   ulong send_timestamp;     /**< Метка времени начала контракта */
   ulong opening_timestamp;  /**< Метка времени начала контракта */
   ulong closing_timestamp;  /**< Метка времени конца контракта */
   double amount;            /**< Размер ставки в RUB или USD */
   double profit;            /**< Размер выиграша */
   double payout;            /**< Процент выплат */
   double open_price;
   //double close_price = 0;
   IntradeBarBetStatus bet_status;
};

class IntradeBarConsoleBotApi {
private:
   NamedPipeClient pipe;
   bool is_connected;
   bool is_broker_connected;
   bool is_broker_prev_connected;
   bool is_rub_currency;
   bool is_demo;
   int tick;            // тики для подсчета времени отправки ping
   double balance;      // баланс
   double prev_balance; // предыдущий баланс
   string body;
public:

   enum ENUM_BO_ORDER_TYPE {
   	BUY = 0,
   	SELL = 1,
   };
   
   enum ENUM_BO_TYPE {
   	CLASSICAL = 0,
   	SPRINT = 1,
   };

   IntradeBarConsoleBotApi() {
      pipe.set_buffer_size(2048);
      is_connected = false;
      is_broker_connected = false;
      is_broker_prev_connected = false;
      is_rub_currency = false;
      is_demo = false;
      tick = 0;
      balance = 0;
      prev_balance = 0;
   }
   
   ~IntradeBarConsoleBotApi() {
      close();
   }
   
   virtual void on_update_bet(void);
   
   bool connect(string api_pipe_name) {
      if(is_connected) return true;
      is_connected = pipe.open(api_pipe_name);
      return is_connected;
   }
   
   bool connected() {
      return is_connected;
   }
   
   bool open_deal(string symbol, int direction, datetime expiration, int type, double amount) {
      if(!is_connected) return false;
      if(direction != BUY && direction != SELL) return false;
      if(type != CLASSICAL && type != SPRINT) return false;
      string json_body;
      string str_direction = direction == BUY ? "BUY" : "SELL";
      json_body = StringConcatenate(json_body,"{\"contract\":{\"symbol\":\"",symbol,"\",\"direction\":\"",str_direction,"\",\"amount\":",amount,",");
      if(type == CLASSICAL) {
         if(expiration < 86400) expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"date_expiry\":",(long)expiration);
      } else {
         expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"duration\":",(long)expiration);
      }
      json_body = StringConcatenate(json_body,"}}");
      Print("json_body",json_body);
      if(pipe.write(json_body)) return true;
      close();
      return false;
   }
   
   bool open_deal(string symbol, string strategy_name, int direction, datetime expiration, int type, double amount) {
      if(!is_connected) return false;
      if(direction != BUY && direction != SELL) return false;
      if(type != CLASSICAL && type != SPRINT) return false;
      string json_body;
      string str_direction = direction == BUY ? "BUY" : "SELL";
      json_body = StringConcatenate(json_body,"{\"contract\":{\"symbol\":\"",symbol,"\",\"strategy_name\":\"",strategy_name,"\",\"direction\":\"",str_direction,"\",\"amount\":",amount,",");
      if(type == CLASSICAL) {
         if(expiration < 86400) expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"date_expiry\":",(long)expiration);
      } else {
         expiration *= 60; // переводим время в секунды
         json_body = StringConcatenate(json_body,"\"duration\":",(long)expiration);
      }
      json_body = StringConcatenate(json_body,"}}");
      if(pipe.write(json_body)) return true;
      close();
      return false;
   }
   
   double get_balance() {
      return balance;
   }
   
   bool check_balance_change() {
      if(prev_balance != balance) {
         prev_balance = balance;
         return true;
      }
      return false;
   }
   
   bool check_broker_connection() {
      return is_broker_connected;
   } 
   
   bool check_broker_connection_change() {
      if(is_broker_prev_connected != is_broker_connected) {
         is_broker_prev_connected = is_broker_connected;
         return true;
      }
      return false;
   } 
   
   /** \brief Callback-функция для получения состояния бинарных опционов
    * \param new_bet Структура параметров бинарного опциона
    */
   virtual void on_update_bet(IntradeBarBet &new_bet);
   
   /** \brief Callback-функция для получения котировок валютных пар
    * \param symbols Массив символов
    * \param prices Массив цен
    */
   virtual void on_update_prices(string &symbols[], double &prices[]);
   
   /** \brief Обновить состояние API
    * \param delay Задержка между вызовами функции
    */
   void update(int delay) {
      if(!is_connected) return;
      const int MAX_TICK = 10000;
      tick += delay;
      if(tick > MAX_TICK) {
         tick = 0;
         string json_body = "{\"ping\":1}";
         if(!pipe.write(json_body)) {
            close();
         }
      }
      while(pipe.get_bytes_read() > 0) {
         body += pipe.read();
         //Print("body f: ", body); 
         if(StringGetChar(body, StringLen(body) - 1) != '}') {
            continue;
         }
         //Print("body: ", body);
         
         /* парсим json сообщение */
         JSONParser *parser = new JSONParser();
         JSONValue *jv = parser.parse(body);
         body = "";
         if(jv == NULL) {
            Print("error:"+(string)parser.getErrorCode() + parser.getErrorMessage());
         } else {
            if(jv.isObject()) {
               JSONObject *jo = jv;
               
               /* получаем значения состояния сделок */
               JSONObject *update_bet = jo.getObject("update_bet");
               if(update_bet != NULL){
                  IntradeBarBet bet;
                  bet.api_bet_id = update_bet.getInt("api_id");
                  bet.broker_bet_id = update_bet.getInt("broker_id");
                  bet.symbol = update_bet.getString("symbol");
                  bet.note = update_bet.getString("note"); 
                  bet.duration = update_bet.getLong("duration");
                  bet.bet_status = IntradeBarBetStatus::UNKNOWN_STATE;         
                  bet.send_timestamp = update_bet.getLong("send_time");  
                  bet.opening_timestamp = update_bet.getLong("opening_time");
                  bet.closing_timestamp = update_bet.getLong("closing_time");
                  bet.amount = update_bet.getDouble("amount");           
                  bet.profit = update_bet.getDouble("profit");         
                  bet.payout = update_bet.getDouble("payout");           
                  bet.open_price = update_bet.getDouble("open_price");
                  bet.contract_type = 0;  
                  string dir_str = update_bet.getString("direction");
                  if(dir_str == "buy") bet.contract_type = 1;
                  else if(dir_str == "sell") bet.contract_type = -1;
                  //UNKNOWN_STATE
                  //OPENING_ERROR
                  //CHECK_ERROR     
                  //WAITING_COMPLETION
                  //WIN
                  //LOSS
                  
                  string status_str = update_bet.getString("status"); 
                  if(status_str == "win") bet.bet_status = IntradeBarBetStatus::WIN;
                  else if(status_str == "loss") bet.bet_status = IntradeBarBetStatus::LOSS;
                  else if(status_str == "unknown") bet.bet_status = IntradeBarBetStatus::UNKNOWN_STATE;
                  else if(status_str == "check_error") bet.bet_status = IntradeBarBetStatus::CHECK_ERROR; 
                  else if(status_str == "opening_error") bet.bet_status = IntradeBarBetStatus::OPENING_ERROR; 
                  else if(status_str == "wait") bet.bet_status = IntradeBarBetStatus::WAITING_COMPLETION; 
                  
                  on_update_bet(bet);
                  //Print("update bet, amount ",bet.amount, " status ", status_str);
               }
               
               /* получаем котировки */
               JSONArray *j_array_prices = jo.getArray("prices");
               if(j_array_prices != NULL){
                  string intrade_bar_currency_pairs[26] = {
                       "EURUSD","USDJPY","GBPUSD","USDCHF",
                       "USDCAD","EURJPY","AUDUSD","NZDUSD",
                       "EURGBP","EURCHF","AUDJPY","GBPJPY",
                       "CHFJPY","EURCAD","AUDCAD","CADJPY",
                       "NZDJPY","AUDNZD","GBPAUD","EURAUD",
                       "GBPCHF","EURNZD","AUDCHF","GBPNZD",
                       "GBPCAD","XAUUSD"
                  };
                  double array_prices[26];
                  for(int i = 0; i < 26; ++i) {
                     array_prices[i] = j_array_prices.getDouble(i);
                  }
                  on_update_prices(intrade_bar_currency_pairs, array_prices);
               }

               /* получаем баланс */
               double dtemp = 0;
               if(jo.getDouble("balance", dtemp)){
                  balance = dtemp;
                  //Print("balance: ", balance);
               }
               
               /* получить флаг рублевого аккаунта */
               int itemp = 0;
               if(jo.getInt("rub", itemp)){
                  is_rub_currency = (itemp == 1);
               }
               
               /* получить флаг демо аккаунта */
               itemp = 0;
               if(jo.getInt("demo", itemp)){
                  is_demo = (itemp == 1);
               }
               
               /* проверяем сообщение ping */
               itemp = 0;
               if(jo.getInt("ping", itemp)){
                  //Print("ping: ",itemp);
                  string json_body = "{\"pong\":1}";
                  if(!pipe.write(json_body)) {
                     close();
                  }
               }
               
               /* проверяем состояние соединения */
               if(jo.getInt("connection", itemp)){
                  if(itemp == 1) is_broker_connected = true;
                  else is_broker_connected = false;
                  //Print("connection: ",itemp);
               }
            }
            delete jv;
         }
         delete parser;
      }
   }
   
   /** \brief Получить метку времени закрытия CLASSIC бинарного опциона
    * \param timestamp Метка времени (в секундах)
    * \param expiration Экспирация (в минутах)
    * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
    */
   datetime get_classic_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration) {
      if((user_expiration % 5) != 0 || user_expiration < 5) return 0;
      const datetime classic_bet_timestamp_future = (datetime)(user_timestamp + (user_expiration + 3) * 60);
      return (classic_bet_timestamp_future - classic_bet_timestamp_future % (5 * 60));
   }
   
   /** \brief Получить метку времени закрытия CLASSIC бинарного опциона на закрытие бара
    * \param timestamp Метка времени (в секундах)
    * \param expiration Экспирация (в минутах)
    * \return Вернет метку времени закрытия CLASSIC бинарного опциона либо 0, если ошибка.
    */
   datetime get_candle_bo_closing_timestamp(const datetime user_timestamp, const ulong user_expiration) {
      if((user_expiration % 5) != 0 || user_expiration < 5) return 0;
      if(user_expiration == 5) return get_classic_bo_closing_timestamp(user_timestamp, user_expiration);
      else {
         datetime end_timestamp = get_classic_bo_closing_timestamp(user_timestamp, user_expiration);
         if(((end_timestamp / 60) % user_expiration) == 0) return end_timestamp;
         else return end_timestamp - 5*60;
      }
      return 0;
   }
   
   /** \brief Получить проценты выплаты брокера
    * \symbol Символ
    * \duration Длительность опциона
    * \param timestamp Метка времени
    * \param amount Размер ставки
    * \param is_rub Экспирация
    * \return Вернет процент выплаты брокера в виде дробного числа
    */
   double calc_payout(string symbol, const ulong duration, const datetime timestamp, const double amount) {
      string intrade_bar_currency_pairs[21] = {
           "EURUSD","USDJPY","USDCHF",
           "USDCAD","EURJPY","AUDUSD",
           "NZDUSD","EURGBP","EURCHF",
           "AUDJPY","GBPJPY","EURCAD",
           "AUDCAD","CADJPY","NZDJPY",
           "AUDNZD","GBPAUD","EURAUD",
           "GBPCHF","AUDCHF","GBPNZD"
      };
      bool is_found = false;
      for(int i = 0; i < 21; ++i) {
         if(symbol == intrade_bar_currency_pairs[i]) {
            is_found = true;
            break;
         }
      }
      if(!is_found) return 0.0;
      if(duration == 120) return 0.0;
      const ulong max_duration = 3600*24*360*10;
      if(duration > 30000 && duration < max_duration) return 0.0;
      if((!is_rub_currency && amount < 1)|| (is_rub_currency && amount < 50)) return 0.0;
      CDateTime date_time;
      date_time.DateTime(timestamp);
      
      /* пропускаем выходные дни */
      if(date_time.day_of_week == 0 || date_time.day_of_week == 6) return 0.0;
      /* пропускаем 0 час */
      if(date_time.hour == 0) return 0.0;
      if(date_time.hour >= 21) return 0.0;
      /* с 4 часа по МСК до 9 утра по МСК процент выполат 60%
       * с 17 часов по МСК  процент выплат в течении 3 минут в начале часа и конце часа также составляет 60%
       */
      if(date_time.hour <= 6 || date_time.hour >= 14) {
          /* с 4 часа по МСК до 9 утра по МСК процент выполат 60%
           * с 17 часов по МСК  процент выплат в течении 3 минут в начале часа и конце часа также составляет 60%
           */
          if(date_time.min >= 57 || date_time.min <= 2) {
              return 0.6;
          }
      }
      if(date_time.hour == 13 && date_time.min >= 57) {
          return 0.6;
      }
      /* Если счет в долларах и ставка больше 80 долларов или счет в рублях и ставка больше 5000 рублей */
      if((!is_rub_currency && amount >= 80)||
          (is_rub_currency && amount >= 5000)) {
          if(duration == 60) return 0.63;
          return 0.85;
      } else {
          /* Если счет в долларах и ставка меньше 80 долларов или счет в рублях и ставка меньше 5000 рублей */
          if(duration == 60) {
              /* Если продолжительность экспирации 3 минуты
               * Процент выплат составит 82 (0,82)
               */
              return 0.6;
          } else
          if(duration == 180) {
              /* Если продолжительность экспирации 3 минуты
               * Процент выплат составит 82 (0,82)
               */
              return 0.82;
          } else {
               /* Если продолжительность экспирации от 4 до 500 минут */
               return 0.79; 
          }
      }
      return 0.0;       
   }
   
   /** \brief Проверить на демо аккаунт
    * \return Вернет true если демо аккаунт
    */
   bool check_demo() {
      return is_demo;
   }
   
   /** \brief Проверить на рублевый аккаунт
    * \return Вернет true если аккаунт рублевый
    */
   bool check_rub() {
      return is_rub_currency;
   }
   
   /** \brief Закрыть соединение
    */
   void close() {
      if(is_connected) pipe.close();
      is_connected = false;
      is_broker_connected = false;
      is_broker_prev_connected = false;
      is_rub_currency = false;
      is_demo = false;
      tick = 0;
      balance = 0;
      prev_balance = 0;
   }
};

