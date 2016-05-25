
Goldmine v2 protocol
====================

Сообщения протокола состоят из нескольких частей. Референсная имплементация использует zeromq.
Серверы открывают ROUTER-сокет, клиенты должны учитывать enveloping.

Сообщение имеет следующую структуру:

+-----------------+
|  peer ID        |
+-----------------+
| <empty frame>   |
+-----------------+
| message type    |
+-----------------+
| ...... ... ...  |
+-----------------+

Message type занимает 4 байта, и может принимать следующие значения:

 * 0x01 - Control. Следующий фрейм содержит json-документ.
 * 0x02 - Data. Следующий фрейм содержит тиковые или баровые данные.
 * 0x03 - Service. Служебные фреймы
 * 0x04 - Event. Следующий фрейм содержит описание события, которое произошло.

Control-сообщения для всех типов нод
------------------------------------

### Capability request

Запрос:
    {
        "command" : "request-capabilities"
    }

Ответ:
    {
        "node-type" : "quotesource",
        "protocol-version" : 2
    }

### Heartbeat request
Запрос:
    {
        "command" : "heartbeat-request",
        "period" : 1
    }

Ответа на это сообщение нет. После данной команды участник протокола начинает слать контрагенту периодические
сервисные сообщения. Эти сообщения должны поддерживаться как клиентами, так и сервером.

Quotesource-сообщения
---------------------

### Запрос потока

Запрос:
    {
        "command" : "start-stream",
        "manual-mode" : false,
        "tickers" : ["t:RIM6/price,best_bid,best_offer", "1min:MMM6/price"],
        "from" : "2016-05-19 10:00:00.000000",
        "to" : "2016-05-19 13:00:00.123456"
    }

В ответ, сервер начинает посылку данных указанных тикеров с указанными временными рамками.
Если manual-mode равно true, то посылка каждого пакета совершается только после приема соответствующего
сервисного сообщения от клиента.

Тикеры указываются следующим образом:
<timeframe>:<ticker>[/<comma-separated-selectors>]

timeframe ::= 't' | <int-number> 'min' | <int-number> 'h' | <int-number> 'd'

ticker может содержать в конце астериск "\*" (но только в конце)

comma-separated-selectors - разделённая запятыми последовательность следующих *селекторов*:

 * price - цена последней сделки
 * best\_bid - цена лучшего бида
 * best\_offer - цена лучшего оффера
 * open\_interest - открытый интерес
 * depth - стакан
 * total\_supply - общее предложение
 * total\_demand - общий спрос

### Остановка потока

Запрос:
    {
        "command" : "stop-stream"
    }

Останавливает текущий поток

### Формат потока данных
Message type == 0x02.
Следующий фрейм содержит одну или несколько структур Tick или Summary:


	struct decimal_fixed
	{
		int64_t value;
		int32_t fractional; // 1e-9 parts
    };

	struct Tick
	{
		uint32_t packet_type; // = 0x01
		uint64_t timestamp;
		uint32_t useconds;
		uint32_t datatype;
		decimal_fixed value;
		int32_t volume;
	};

	struct Summary
	{
		uint32_t packet_type; // = 0x02
		uint64_t timestamp;
		uint32_t useconds;
		uint32_t datatype;
		decimal_fixed open;
		decimal_fixed high;
		decimal_fixed low;
		decimal_fixed close;
		int32_t volume;
		uint32_t summary_period_seconds;
	};

Структуры следуют друг за другом непрерывно. Тип следующей структуры в потоке можно определить по
полю `packet_type`.

Broker-сообщения
----------------

Broker
Ноды этого типа служат интерфейсом между инфраструктурой goldmine и брокером. В их функции входит: посылка/отмена ордеров и уведомление о статусах ордеров.
Ноды типа брокер открывают control-сокет типа ROUTER. Общение между клиентом и нодой происходит с помощью JSON-пакетов

Для создания нового ордера:

    {'order' : {
      'id' : 1,
      'account' : 'NL0080000043#467',
      'security' : 'SPBFUT#RIH6',
      'type' : 'limit',
      'price' : 19.73,
      'amount' : 2,
      'operation': 'buy'
     }
    }

    { 'order' : {
      'id' : 2,
      'account' : 'NL0080000043#467',
      'security' : 'SPBFUT#RIH6',
      'type' : 'market',
      'amount' : 2,
      'operation': 'buy'
     }
    }

    { 'order' : {
      'id' : 3,
      'account' : 'NL0080000043#467',
      'security' : 'SPBFUT#RIH6',
      'type' : 'stop',
      'price' : 19.00,
      'stop-price' : 19.30,
      'amount' : 2,
      'operation': 'buy'
     }
    }

    { 'order' : {
      'id' : 4,
      'account' : 'NL0080000043#467',
      'security' : 'SPBFUT#RIH6',
      'type' : 'take-profit',
      'price' : 19.00,
      'spread' : 0.20,
      'amount' : 2,
      'operation': 'sell'
     }
    }

    { 'cancel-order' : { 'id' : 4, 'account' : 'FOO' } }

При изменении состояния ордера брокер будет слать сообщения клиенту

    { 'order' : { 'id' : 1, 'new-state' : 'submitted' } }
    { 'order' : { 'id' : 2, 'new-state' : 'rejected', 'reason' : 'Not enough money' } }


Для получения текущих ордеров:
    { 'get' : 'orders' }

Ордера будут выглядеть так же, как и при их создании:
    [ {'order' : {
      'id' : 1,
      'account' : 'NL0080000043#467',
      'security' : 'SPBFUT#RIH6',
      'type' : 'limit',
      'price' : 19.73,
      'amount' : 2,
      'operation': 'buy'
     }
    },
    { 'order' : {
      'id' : 3,
      'account' : 'NL0080000043#467',
      'security' : 'SPBFUT#RIH6',
      'type' : 'stop',
      'price' : 19.00,
      'stop-price' : 19.30,
      'amount' : 2,
      'operation': 'buy'
     }
    } ]


Для получения одного ордера:
    { 'get' : 'order',
      'id' : 1
    }


Для получения открытых позиций:
    { 'get' : 'positions'}

Ответ:
    [ {'position' : { 'account' : 'NL0080000043#467',
                      'security' : 'SPBFUT#RIH6',
                      'size' : -3,
                      'entry-price' : 68420
    }},
      {'position' : { 'account' : 'NL0080000043#467',
                      'security' : 'SUR',
                      'size' : 100000.0
    }}]

При исполнении ордера Broker посылает клиенту соотв	етствующее сообщение:


Service-сообщения
-----------------

Общий вид service-сообщения:

+-----------------+
|  peer ID        |
+-----------------+
| <empty frame>   |
+-----------------+
| message type    |
+-----------------+
| ServiceData     |
+-----------------+

ServiceData начинается с 4-х байт, характеризующих тип сервисного сообщения.

 * 0x01 - HeartBeat 
 * 0x02 - NextMessage

