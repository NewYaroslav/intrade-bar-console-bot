//+------------------------------------------------------------------+
//|                         	     Intrade-Bar-Bollinger-Bot-2V2.mq4 |
//|                         	   Copyright 2020, Yaroslav Barabanov. |
//|                                https://t.me/BinaryOptionsScience |
//+------------------------------------------------------------------+
#property copyright "Copyright 2020, Yaroslav Barabanov."
#property link      "https://t.me/BinaryOptionsScience"
#property version   "2.00"
#property strict

#include "intrade_bar_bot_api.mqh"
#include "simple_label.mqh"

/* функция для получения массива символов */
ulong get_symbols(string &SymbolsList[]);
bool make_text_label();

//--- input parameters
input string   pipe_name = "intrade_bar_console_bot"; // Имя именнованного канала
input int      timer_period = 10;                  // Период таймера (мс)
input ENUM_TIMEFRAMES timeframe = PERIOD_CURRENT;  // Таймфрейм
input int      expiration_bb_1 = 5;                // Экспирация для первого Боллинджера
input int      expiration_bb_2 = 5;                // Экспирация для второго Боллинджера
input double   amount_1 = 1;                       // Размер ставки для первого Боллинджера
input double   amount_2 = 1;                       // Размер ставки для второго Боллинджера
input ENUM_BO_TYPE bo_type = CLASSICAL;            // Тип опциона
input string   user_symbols_list = 
         "EURUSD,USDJPY,USDCHF,USDCAD,EURJPY,AUDUSD,NZDUSD,"
         "EURGBP,EURCHF,AUDJPY,GBPJPY,EURCAD,AUDCAD,CADJPY,"
         "NZDJPY,AUDNZD,GBPAUD,EURAUD,GBPCHF,AUDCHF,GBPNZD";   // Массив используемых валютных пар
input int      period_bb_1 = 15;                   // Период первого Боллинджера
input int      period_bb_2 = 30;                   // Период второго Боллинджера
input double   deviation_bb_1 = 2.0;               // Множитель стандартного отклонения первого Боллинджера
input double   deviation_bb_2 = 2.0;               // Множитель стандартного отклонения второго Боллинджера

static string symbol_name[];
static uint num_symbol = 0;                        // количество символов
static datetime opening_time_candle_signal_1[];    // Время открытия бара для первого сигнала (нужно чтобы в течении одного бара был лишь один сигнал)
static datetime opening_time_candle_signal_2[];    // Время открытия бара для второго сигнала (нужно чтобы в течении одного бара был лишь один сигнал)
static datetime expiration = 0;                    // Экспирация
static MqlRates rates[];

IntradeBarConsoleBotApi api;                       // API для работы с роботом

int OnInit() {
   /* получаем список символов */
   string real_symbols[];
   ulong num_real_symbol = get_symbols(real_symbols);
   if(num_real_symbol == 0) return(INIT_FAILED);

   /* парсим массив валютных пар */
   string sep=",";
   ushort u_sep;
   u_sep = StringGetCharacter(sep,0);
   int k = StringSplit(user_symbols_list, u_sep, symbol_name);
   num_symbol = ArraySize(symbol_name);

   for(uint s = 0; s < num_symbol; ++s) {
      bool is_found = false;
      for(uint rs = 0; rs < num_real_symbol; ++rs) {
         if(real_symbols[rs] == symbol_name[s]) {  
            is_found = true;
            break;
         }
      }
      if(!is_found) {
         Print("Error! ",symbol_name[s]," symbol not found!");
         ArrayFree(real_symbols);
         ArrayFree(symbol_name);
         return(INIT_FAILED);
      }
   }
   ArrayFree(real_symbols);
   
   /* инициализируем массивы */
   ArraySetAsSeries(rates,true);
   ArrayResize(opening_time_candle_signal_1, num_symbol);
   ArrayResize(opening_time_candle_signal_2, num_symbol);
   ArrayInitialize(opening_time_candle_signal_1, 0);
   ArrayInitialize(opening_time_candle_signal_2, 0);
   
   /* инициализируем метки времени */
   for(uint s = 0; s < num_symbol; ++s) {
      int err = CopyRates(symbol_name[s], timeframe, 0, 1, rates);
      if(err == 0) continue;
      opening_time_candle_signal_1[s] = rates[0].time;
      opening_time_candle_signal_2[s] = rates[0].time;
   }
   
   /* инициализируем таймер */
   if(!EventSetMillisecondTimer(timer_period)) return(INIT_FAILED);
   
   /* инициализируем метку с балансом */
   if(!make_text_label()) return(INIT_FAILED);
   
   Print("bot start");
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) {
   api.close();
   ArrayFree(symbol_name);
   ArrayFree(rates);
   LabelDelete(0,"text_broker");
   LabelDelete(0,"text_status");
   LabelDelete(0,"text_balance");
   ChartRedraw();
   EventKillTimer();
}

void OnTick() {
}

void OnTimer() {
   /* проверяем наличие соединения */
   if(api.connected()) {
      /* соединение есть, проверяем условия для открытия сделки */
      
      RefreshRates(); // Далее гарантируется, что все данные обновлены
      
      /* получаем время GMT */
      const datetime timestamp = TimeGMT();
      
      /* проходим весь список символов */
      for(uint s = 0; s < num_symbol; ++s) {
         
         /* получаем значения индикаторов и цены */
         int err = CopyRates(symbol_name[s], timeframe, 0, 1, rates);
         if(err == 0) continue;
         const double upper_1 = iBands(symbol_name[s], timeframe, period_bb_1, deviation_bb_1, 0, PRICE_CLOSE, MODE_UPPER, 0);
         const double lower_1 = iBands(symbol_name[s], timeframe, period_bb_1, deviation_bb_1, 0, PRICE_CLOSE, MODE_LOWER, 0);
         const double upper_2 = iBands(symbol_name[s], timeframe, period_bb_2, deviation_bb_2, 0, PRICE_CLOSE, MODE_UPPER, 0);
         const double lower_2 = iBands(symbol_name[s], timeframe, period_bb_2, deviation_bb_2, 0, PRICE_CLOSE, MODE_LOWER, 0);
         const double close = rates[0].close;
         
         /* формируем первый сигнал */
         if(bo_type == CLASSICAL) {
            if(opening_time_candle_signal_1[s] < rates[0].time) {
               //if(close < lower_1) {
               if(true) {
                  /* в начлии есть сигнал вверх */
                  
                  /* получаем процент выплаты */
                  const double payout = api.calc_payout(symbol_name[s], expiration_bb_1, TimeGMT(), amount_1);
                  
                  /* не обрабатываем сигнал, если процент выплаты низкий */
                  if(payout < 0.79) {
                     opening_time_candle_signal_1[s] = rates[0].time; // запоминаем бар, где был сигнал
                     Print("пропущена сделка (низкий процент выплат): ", symbol_name[s]);
                     continue;
                  }
                  
                  /* получаем дату окончания экспирации  */
                  expiration = api.get_classic_bo_closing_timestamp(TimeGMT(), expiration_bb_1);
                  
                  opening_time_candle_signal_1[s] = rates[0].time; // запоминаем бар, где был сигнал
   			      string signal_id = IntegerToString(GetTickCount())+IntegerToString(MathRand());
                  api.open_deal(symbol_name[s], signal_id, BUY, expiration, bo_type, amount_1);
                  Print("сделка: ", symbol_name[s]);
               } else
               if(true) {
               //if(close > upper_1) {
                  /* в начлии есть сигнал вниз */
                  
                  /* получаем процент выплаты */
                  const double payout = api.calc_payout(symbol_name[s], expiration_bb_1, TimeGMT(), amount_1);
                  
                  /* не обрабатываем сигнал, если процент выплаты низкий */
                  if(payout < 0.79) {
                     opening_time_candle_signal_1[s] = rates[0].time; // запоминаем бар, где был сигнал
                     Print("пропущена сделка (низкий процент выплат): ", symbol_name[s]);
                     continue;
                  }
                  
                  /* получаем дату окончания экспирации  */
                  expiration = api.get_classic_bo_closing_timestamp(TimeGMT(), expiration_bb_1);
                  
                  opening_time_candle_signal_1[s] = rates[0].time; // запоминаем бар, где был сигнал
   			      string signal_id = IntegerToString(GetTickCount())+IntegerToString(MathRand());
                  api.open_deal(symbol_name[s], signal_id, SELL, expiration, bo_type, amount_1); 
                  Print("сделка: ", symbol_name[s]);
               }
               
            }
         }

         /* формируем второй сигнал */
         if(bo_type == SPRINT) {
            if(opening_time_candle_signal_2[s] < rates[0].time) {
               if(close < lower_2) {
                  /* в начлии есть сигнал вверх */
                  
                  opening_time_candle_signal_2[s] = rates[0].time; // запоминаем бар, где был сигнал
   			      string signal_id = IntegerToString(GetTickCount())+IntegerToString(MathRand());
                  api.open_deal(symbol_name[s], signal_id, BUY, expiration_bb_2, bo_type, amount_2); 
                  Print("сделка: ",symbol_name[s]);  
               } else
               if(close > upper_2) {
                  /* в начлии есть сигнал вниз */

                  opening_time_candle_signal_2[s] = rates[0].time; // запоминаем бар, где был сигнал
   			      string signal_id = IntegerToString(GetTickCount())+IntegerToString(MathRand());
                  api.open_deal(symbol_name[s], signal_id, SELL, expiration_bb_2, bo_type, amount_2);
                  Print("сделка: ",symbol_name[s]); 
               }
            } 
         }
      }
   } else {
      /* соединение отсутствует, подключаемся */
      if(api.connect(pipe_name)) {
         Print("Успешное соединение с ", pipe_name);
      } else {
         //Print("Соединение не удалось");
         LabelTextChange(0,"text_balance","balance: none");
         LabelTextChange(0,"text_status", "disconnected");
         ChartRedraw();
      }
   }
   
   /* обновляем состояние API */
   api.update(timer_period);
   if(api.check_balance_change()) {
      string str_account_currency;
      string str_account_type;
      
      /* определяем, RUB или USD аккаунт */
      if(api.check_rub()) {
         str_account_currency = "RUB";
      } else {
         str_account_currency = "USD";
      }
      
      /* определяем, DEMO или REAL аккаунт */
      if(api.check_demo()) {
         str_account_type = "DEMO";
      } else {
         str_account_type = "REAL";
      }
      
      /* выводим данные */
      LabelTextChange(0,"text_balance","balance: " + DoubleToString(api.get_balance(),2) + " " + str_account_currency + " " + str_account_type);
      Print("Баланс: ",DoubleToString(api.get_balance(),2)); 
      ChartRedraw();
   }
   
   if(api.check_broker_connection_change()) {
      if(!api.check_broker_connection()) LabelTextChange(0,"text_status", "disconnected");
      else LabelTextChange(0,"text_status", "connected");
      Print("Соединение с брокером: ",api.check_broker_connection()); 
      ChartRedraw();
   }
}

/* получаем список символов */
ulong get_symbols(string &SymbolsList[]) {
   // Открываем файл  symbols.raw
   int hFile = FileOpenHistory("symbols.raw", FILE_BIN|FILE_READ);
   if(hFile < 0) return(-1);
   // Определяем количество символов, зарегистрированных в файле
   ulong SymbolsNumber = FileSize(hFile) / 1936;
   if(SymbolsNumber == 0) {
      FileClose(hFile);
      return(SymbolsNumber);
   }
   ArrayResize(SymbolsList, (int)SymbolsNumber);
   // Считываем символы из файла
   for(ulong i = 0; i < SymbolsNumber; ++i) {
      SymbolsList[(int)i] = FileReadString(hFile, 12);
      FileSeek(hFile, 1924, SEEK_CUR);
   }
   FileClose(hFile);
   // Возвращаем общее количество инструментов
   return(SymbolsNumber);
}

bool make_text_label() {
   const int font_size = 10;
   const int indent = 8;
   long y_distance;
   uint text_broker_width = 140;
   if(!ChartGetInteger(0,CHART_HEIGHT_IN_PIXELS,0,y_distance)) {
      Print("Failed to get the chart width! Error code = ",GetLastError());
      return false;
   }
   LabelCreate(0,"text_broker",0,indent, (int)y_distance - 2*font_size - 2*indent,CORNER_LEFT_UPPER,"intrade.bar status:","Arial",font_size,clrAliceBlue);
   LabelCreate(0,"text_status",0,indent + text_broker_width, (int)y_distance - 2*font_size - 2*indent,CORNER_LEFT_UPPER,"disconnected","Arial",font_size,clrAqua);
   LabelCreate(0,"text_balance",0,indent, (int)y_distance - font_size - indent,CORNER_LEFT_UPPER,"balance: 0","Arial",font_size,clrLightGreen);
   ChartRedraw();
   return true;
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
   //Print(symbols[0], " price: ", prices[0]);
}