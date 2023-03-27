#ifndef CAN_J1939_H
#define CAN_J1939_H
#include <stdint.h>
#include <sys/can.h>
// для разбора PCAP файла
#define CAN_ID_OFFSET       0
#define CAN_DLC_OFFSET      4
#define CAN_DATA_OFFSET     5

// \see in include/linux/can.h: SocketCAN
typedef uint32_t canid_t;
struct can_frame {
	canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t can_dlc; /* frame payload length in byte (0 .. 8) */
	uint8_t data[8];// __attribute__((aligned(8)));
};
struct can_filter {
	canid_t can_id;
	canid_t can_mask;
};


/* J1939 использует длинные идентификаторы 29 бит
	PGN (Parameter Group Number) — это номер группы параметров, 
	определяющий содержимое сообщения CAN согласно SAE J1939/21.


	
Cосуществование сетей CANopen и J1939
CANopen Extended Frame allowed
NMT 	No
SYNC 	Yes, index 1005h
EMCY	Yes, index 1014h
TIME 	Yes, index 1012h
TxPDO 	Yes, index 1800h .. 19FFh
RxPDO 	Yes, index 1400h .. 15FFh
SDO 	No
NMT-EC	No // Error Control

Для обмена длинными сообщениями в J1939 применяется проктокол BAM (широковещательный) или CMDT 
Для проприетарных протоколов более 8 байт
Идентификаторы сообщений:
CMDT 0xFEBF 
BAM  0xFF00..0xFFFF - широковещательный

Совместимость с CANopen:
− the Non-CANopen network shall not use the identifier value 0 (CANopen NMT).
− the Non-CANopen network shall not use the identifiers for SDO and NMT-EC services.


PGNs with PDU-Format <  240 (format 1) identify point-to-point messages
PGNs with PDU-Format >= 240 (format 2) identify broadcast messages.

SAE J1939 transport layer 
uses two special point-to-point messages identified
by PGNs of format 1 to transport segmented messages, both with a fixed length of 8
bytes. These messages are called transport frames in the context of this document.

TP.CM is used for connection management. The first byte of the payload identifies its
role, which may be one of the following:
TP.CM_BAM is used to initiate a BAM transfer.
TP.CM_RTS is transmitted to initiate a CMDT transfer.
TP.CM_CTS is used for flow control during a CMDT transfer.
TP.CM_EndOfMsgAck indicates the end of a CMDT transfer.
TP.Conn_Abort indicates an error and terminates the CMDT transfer.
TP.DT contains a sequence number in the first byte and 7 bytes of data.
A single TP.CM or TP.DT frame, identified by a certain CAN Identifier, is used for
different PGs. The PGN of the transported PG is contained in the payload of the
TP.CM frames as specified in [16].

MilCAN _r =1
MilCAN dp - request
	*/
struct J1939_CAN_ID {
	unsigned  sa:8; // 21..28 
	unsigned  ps:8; // 13..20
	unsigned  pf:8; // 5..12 PF (задает PDU format) и 13..20 PS (задает PDU Specific) составляет PGN
	unsigned  dp:1; // 4 служит для выбора страницы данных из всего диапазона PGN (может принимать значения 0 или 1).
	unsigned edp:1; // 3 (резерв) Для передаваемых сообщений значение EDP всегда равно 0
	unsigned priority:3; // 0..2  приоритет сообщения (поле P). При Р=000 высокий 111 низкий
};

/*! Предлагается хранение, в другом порядке 
Linux with SocketCAN
◦ https://www.kernel.org/doc/html/v4.17/networking/can.html
can-utils
◦ https://github.com/linux-can/can-utils
Additional configuration for the BBB can be found at 
  https://github.com/SystemsCyber/TruckCapeProjects/blob/master/OSBuildInstructions.md

J1939 Network Layers:
J1939-9X Network Security
J1939-8X Network Management
J1939-7X Application layers
SAE J1939-71 (7 Applications)
SAE J1939-73 (Diagnostics)
	Defines how to interpret and compose J1939 messages with engineering values
SAE J1939-31 (3 Network) Clarifies the concept of a gateway between two separate networks
SAE J1939-21 (2 Data Link) Describes how to make a J1939 PDU.Includes details on sending messages up to 1785 bytes long.
SAE J1939-1x (1 Physical) Defined connectors, transceivers, wiring, pinouts, and signaling.


Порядок изучения:
	\see A Digital Annex (J1939DA) has the applications defined in an Excel spreadsheet
	\see SAE J1939-21-2022 Data Link Layer Уровень Канала передачи данных

PRIORITY_MASK 	= 0x1C000000
EDP_MASK 		= 0x02000000
DP_MASK 		= 0x01000000
PF_MASK 		= 0x00FF0000
PS_MASK 		= 0x0000FF00
SA_MASK 		= 0x000000FF
PDU1_PGN_MASK 	= 0x03FF0000
PDU2_PGN_MASK 	= 0x03FFFF00

PGN 18 бит:
PDU format #1: is for point-to-point messages
	+ EDP, DP, PF, дополняется нулями
	+ PS becomes the destination address (DA)
PDU format #2: is for broadcast messages
	+ PF value must be greater than or equal to 240 (0xF0)
	+ EDP, DP, PF and PS create the Parameter Group Number (PGN)

DP PGN Range 	 #PGNs 
0 000000 – 00EE00 239 	SAE PDU1 = Peer-to-Peer
0 00EF00 		  1 	MF 	PDU1 = Peer-to-Peer
0 00F000 – 00FEFF 3840 	SAE PDU2 = Broadcast
0 00FF00 – 00FFFF 256 	MF 	PDU2 = Broadcast
1 010000 – 01EE00 239 	SAE PDU1 = Peer-to-Peer
1 01EF00 		  1 	MF 	PDU1 = Peer-to-Peer
1 01F000 – 01FEFF 3840 	SAE PDU2 = Broadcast
1 01FF00 – 01FFFF 256 	MF	PDU2 = Broadcas


SAE = Assigned by SAE
MF = Manufacturer Specific – Proprietary Messages
*/

struct J1939_CAN_STD_ID {
	unsigned pri:3; // 0..2  приоритет сообщения (поле P). При Р=000 высокий 111 низкий
	unsigned  sa:8; // 21..28 
};
/* 
Если поле PS принимает значения от 0 до 239, то оно содержит десятичный идентификационный адрес устройства-получателя сообщения DA
(Destination Address), который обозначается PDU1. Формат PDU1 позволяет передать данные напрямую указанному в адресе DA устройству-
получателю сообщения.
Если поле PS принимает значения от 240 до 255, то оно обозначает широкоформатную адресацию, обозначаемую PDU2.
Формат PDU2 может использоваться только для обмена сообщениями, у которых не указан конкретный адрес DA устройства-получателя
сообщения.
*/
// Список объектов состоит из записией 
typedef int16_t (* PgnHandler_fn) (uint8_t * pubDataV,uint16_t uwSizeV,uint8_t ubSrcAddrV);
struct J1939_PGN_Entry {
	uint16_t control;	/*!< Holds additional information for PGN */
	uint32_t PGN;		/*!< Parameter group number */
	uint16_t cycle;		/*!< Cycle time in ticks (counter reload value) */
	uint16_t count;		/*!< Counter */
	uint8_t	 addr;		/*!< Source or Destination address */
	uint8_t  data_size;
	PgnHandler_fn pfnPgnHandler;/*!< Pointer to handler function */
};
struct J1939_Bus_object {// BO_
	uint32_t can_id; // для длинных пакетов 29бит
	uint16_t object_id;// идентификатор объекта задан по таблице 
	uint8_t dlen:4;// 0..8
	uint8_t unit_id;// BU_ - идентификатор устройства задан перечислением, имя по таблице
};
struct J1939_fields {// SG_
	unsigned  start_pos:6;	// в битах от начала
	unsigned  length:8;	// длина в битах
	unsigned  sign:1;
	unsigned  type:1; // measured
	unsigned  SPN:16;		/*!< идентификатор описания */
};
//
struct J1939_SPN {
	uint16_t param_id;// идентификатор параметра - кварк или индекс в таблице имен
	uint16_t units;// идентификатор единицы измерения - кварк или enum
	float factor, offset;
	unsigned min, max;
	// получатели пакета - список идентификаторов
};

#endif//CAN_J1939_H