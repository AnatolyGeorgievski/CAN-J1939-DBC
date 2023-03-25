
# Генерация структур данных из матрицы сообщений CAN

Автор:
* __[Анатолий Георгиевский](https://github.com/AnatolyGeorgievski)__

## Назначение

Формат предназначен для формального описания матрицы сообщений CAN. 

Формат позволяет описывать структуру сообщений CAN для стандартых и расширенных пакетов, 
подходит для описания прикладных протоколов таких как CANopen, J1939.

На основе формата J1939DBC выполняется генерация структур данных для обработки, диагностики и разбора сообщений CAN.
Целью генерации могут быть исходные коды и заголовки Си, таблицы CSV, или SQL запросы на формирование и заполнение базы. 

Формат применяется для формализованного обмена сообщениями CAN между оборудованием разных производителей. 

Автоматический синтез структур данных позволяет свести к минимуму ошибки прораммиста, снизить расходы на разработку программ,
снизить трудозатраты связанные с синхронизацией изменений в коде при обновлении версий протокола и состава оборудования.


## Сборка из исходного кода

```shell
$ gcc can_dbc.c -o dbc `pkg-config.exe --cflags --libs glib-2.0`
```

## Состав пакета

* _sys/can.h_ -- структуры can_frame, can_filter и системные типы CAN
* _canopen.h_ -- заголовок для разбора стандарта CANopen CiA
* _can_j1939.h_ -- заголовок для разбора кадров стандарта SAE J1939
* _can_ev.h_ -- основной заголовок, содержит макросы разбора кадров 
* _can_ev.c_ -- сериализация данных для CAN, протокол EV-1.0
* _can_ev_proxy.c_ -- представление данных на устройстве EV-Gateway
* _can_ev_modbus.c_ -- сериализация данных для RS-485 Modbus RTU, протокол EV-1.0
* _can_ev_serial.c_ -- сериализация данных для UART point-to-point, протокол EV-1.0
* _can_ev_mqtt.c_ -- сериализация данных для протокола MQTT (Message Queuing Telemetry Transport)
* _can_ev_pcap.c_ -- экспорт данных в формат PCAP, Linktype = SocketCAN
* _can_ev_json.c_ -- экспорт данных в формат JSON
* _can_ev_sql.c_ -- экспорт данных в текстовый формат SQL
* _can_j1850_crc.c_ -- расчет контрольной суммы кадра CRC-8/SAE-J1850, компактаня реализация


## Описание формата J1939 DBC

Формат тестовый. Состоит из строк. Строки могут группироваться. Каждая строка начинается с ключевого слова.

Для формального описания используются обозначения:
>   |  выбор элемента из перечня вариантов\
>   (* *) комментарий\
>   [ ] - необязательный элемент\
>   { } - повторение 0 и более раз

### Структура документа 

```
VERSION: "EV 2023-03-25" (* Версия документа *)
NS_: (* Пространство имен *)
BS_: (* Характеристики линии CAN *)
BU_: (* Перечисление функциональных блоков, устройств на линии *)
BO_ ... (* Описание сообщений *) 
	SG_ ... (* Описание сигналов *) 
	SG_ ... (* Описание сигналов *) 
```

### Определение линии, скорость передачи данных

```
'BS_:' [baudrate ':' BTR1 ',' BTR2] ; 
BTR1 = unsigned_int ;
BTR2 = unsigned_int ;
```

### Определение узла, перечисление узлов в сети

```
'BU_:' {node_name} ;
node_name = C_identifyer ;
```

### Определение кадра сообщения

```
'BO_' message_id message_name ':' message_size transmitter {signal} ;
message_id = unsigned_int ; (* Идентификатор сообщения, число *)
message_name = C_identifyer ; (* Имя - идентифкатор для генерации *)
message_size = unsigned_int ; (* размер тела сообщения в байтах *)
transmitter = node_name | 'Vector_XXX' ; (* Идентификатор отправителя *)
```

### Определение сигнала

```
'SG_' signal_name multiplexor_id ':' start_bit '|'
	signal_size '@' byte_order value_type (factor ',' offset ')'
	'[' minimum '|' maximum ']' unit receiver { ',' receiver } ; 
signal_name = C_identifier ;
multiplexor_id = ' ' | 'M' | m multiplexer_switch_value ;
byte_order = '0' | '1' ; (* 0-little endian, 1-big endian *)
value_type = '+' | '-' ; (* + unsigned, - signed *)
unit = char_string ;
receiver = node_name | 'Vector_XXX' ;
```
Правила вычисления значений из числового формата 
```cpp
 physical_value = raw_value * factor + offset;
 raw_value = (physical_value - offset)/factor;
```

### Комментарии в коде

```
comment = 'CM_' object message_id node_name char_string ;
object = 'BU_'|'BO_'|'SG_' ;
```

### Определение атрибутов
```
'BA_DEF_' object attribute_name value_type Min_Max ;
value_type = 'INT' | 'STRING' | 'REAL' | 'ENUM' ; (* INT, STRING, float или REAL , ENUM *) 
Min_Max (* диапазон значений *)
'BA_DEF_DEF_' attribute_name default_value ;
default_value (* начальное значение атрибута *)
```

### Таблица значений для типов заданных перечислением

```
'VAL_' message_id signal_name N “DefineN” . . . 0 “Define0”;
```
### Атрибуты сообщений

Предполагается использование некоторых стандартизованных атрибутов

__GenMsgSendType__ -- Defines the send type of a message\
Тип отсылки может включать два или три времени: задержка на активацию, периодичность сообщений и временная привязка (time-triggered communication).
```cpp
enum GenMsgSendType {//!< Defines the send type of a message
    _none,
    _cyclic,
    _triggered,
    _cyclicIfActive,
    _cyclicAndTriggered,
    _cyclicIfActiveAndTriggered,
};
```

Определение: 
```
BA_DEF BO_ "GenMsgSendType" ENUM "cyclic","triggered","cyclicIfActive","cyclicAndTriggered","cyclicIfActiveAndTriggered"
BA_DEF_DEF "GenMsgSendType" ""
```

__GenMsgCycleTime__ -- Defines the cycle time of a message in ms. 
```
BA_DEF BO_ "GenMsgCycleTime" INT 0 0 (* определение типа атрибута *)
BA_DEF_DEF "GenMsgCycleTime" 0 (* значение по умолчанию *)
```

__GenMsgStartDelayTime__ -- Defines the allowed delay after startup this message must occure the first time in ms. 
```
BA_DEF BO_ "GenMsgStartDelayTime" INT 0 0 
BA_DEF_DEF "GenMsgStartDelayTime" 0
```

__GenMsgDelayTime__ -- Defines the allowed delay for a message in ms. 
```
BA_DEF BO_ "GenMsgDelayTime" INT 0 0 
BA_DEF_DEF "GenMsgDelayTime" 0
```

__VFrameFormat__ -- формат сообщения CAN
```
BA_DEF_ BO_ "VFrameFormat" ENUM  "StandardCAN","ExtendedCAN","J1939PG";
BA_DEF_DEF_  "VFrameFormat" "";
```

__ProtocolType__ -- Тип протокола: OBD2, J1939, CANopen ...
```
BA_DEF_  "ProtocolType" STRING ;
BA_DEF_DEF_  "ProtocolType" "";
```

### Атрибуты сигналов

__GenSigStartValue__ -- Defines the value as long as no value is set/received for this signal. 
```
BA_DEF SG_ "GenSigStartValue" INT 0 0 
BA_DEF_DEF "GenSigStartValue" 0
```

Рекомендуемое приложение для редактирования и просмотра таблиц
[Kvaser Database Editor 3](https://www.kvaser.com/download/)


## Генерация кода

Определения сигналов
`SG_ name`  преобразуются в набор определений и структур, состоящих из битовых полей.
```cpp
#define name##_Type -- тип: 'signed' 'unsigned' 'float' 'double' 'Enumerated' 'Boolean' 'BitString' 'String' 'Octets' 'Date' 'Time' 'OID'
#define name##_Pos  -- позиция поля в битах
#define name##_Bits -- длина поля в битах bit size
#define name##_Msk  -- маска выделения сигнала ULL до 64 бит
#define name##_Factor -- множитель для отображения значения
#define name##_Offset
#define name##_Min -- минимальное значение
#define name##_Max -- максимальное значение
```
Идентификаторы системных типов данных (совместимые идентификаторами BACnet)
```cpp
enum _SYS_TYPE { //!< базовые типы данных в протоколе BACnet и EV-1.0
	_TYPE_NULL	=0x0,//!< пустышка
	_TYPE_BOOLEAN	=0x1,//!< логическое значение
	_TYPE_UNSIGNED	=0x2,//!< целое без знака
	_TYPE_SIGNED	=0x3,//!< целое со знаком
	_TYPE_REAL	=0x4,//!< вещественное 32 бита
	_TYPE_DOUBLE	=0x5,//!< вещественное 64 бита
	_TYPE_OCTETS	=0x6,//!< байтовая строка
	_TYPE_STRING	=0x7,//!< текстовая строка UTF-8
	_TYPE_BIT_STRING=0x8,//!< битовая строка
	_TYPE_ENUMERATED=0x9,//!< перечисление
	_TYPE_DATE	=0xA,//!< дата или шаблон даты
	_TYPE_TIME	=0xB,//!< время с миллисекундах
	_TYPE_OID       =0xC,//!< уникальный идентификатор
};
```

Пример генерации определений:
```cpp
#define GNSS_NMEA_1_byte2_Pos           16
#define GNSS_NMEA_1_byte2_Bits          8
#define GNSS_NMEA_1_byte2_Factor        1
#define GNSS_NMEA_1_byte2_Offset        0
```

Пример генерации структуры данных:
```cpp
typedef struct _GNSS_NMEA_6 GNSS_NMEA_6_t;
struct _GNSS_NMEA_6 {
      signed GNSS_NMEA_6_byte0     :8;  // 0..7:
      signed GNSS_NMEA_6_byte1     :8;  // 8..15:
      signed GNSS_NMEA_6_byte2     :8;  // 16..23:
      signed GNSS_NMEA_6_byte3     :8;  // 24..31:
      signed GNSS_NMEA_6_byte4     :8;  // 32..39:
      signed GNSS_NMEA_6_byte5     :8;  // 40..47:
      signed GNSS_NMEA_6_byte6     :8;  // 48..55:
      signed GNSS_NMEA_6_byte7     :8;  // 56..63:
};
```

## Генерация таблиц SQL

## Экспорт данных в JSON

## Экспорт данных в формат PCAP


