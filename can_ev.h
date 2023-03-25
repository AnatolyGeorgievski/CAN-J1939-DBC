#ifndef _CAN_EV_H
#define _CAN_EV_H

#include <stdint.h>
#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */

#define CAN_SFF_ID_BITS 11
#define CAN_EFF_ID_BITS 29

/*
Правила составления идентификаторов:
SG_ "name"  преобразуется в 

#define name##_Type -- тип: 'signed' 'unsigned' 'float' 'enumerated' 'boolean'
#define name##_Name -- текстовая константа название параметра
#define name##_Pos  -- позиция поля в битах
#define name##_Bits -- длина поля в битах bit size
#define name##_Msk  -- маска выделения сигнала ULL до 64 бит
#define name##_Factor -- множитель для отображения значения
#define name##_Offset -- смещение (float)
#define name##_Min -- минимальное  значение
#define name##_Max -- максимальное значение (float)
*/
// это годится для составления 32 битных масок
#define MASK(name) (((~0UL) >> (32-(name##_Bits)))<<(name##_Pos))
#define MASK64(name) (((~0ULL) >> (64-(name##_Bits)))<<(name##_Pos))
// выделить целое значение из битового поля
#define UVAL(name,v) (((v) & (name##_Msk))>>(name##_Pos))
// создание текстовой константы из имени
#define NAME(name) #name

// макросы для выделения сигналов из определений DBC
#define CAN_DBC_FIELD(value, name) ((value & (name##_Msk))>>(name##_Pos))
#define CAN_DBC_SIGNAL_GET(value, name) ((value & (name##_Msk))>>(name##_Pos))
#define CAN_DBC_SIGNAL_SET(obj, value, name) (((obj)&~(name##_Msk)) | (((value) << (name##_Pos))&(name##_Msk)))
#define CAN_DBC_VALUE(val, name) ((val) * (name##_Factor)+(name##_Offset))
#define CAN_DBC_TYPE(obj) (obj##_Type)
#define CAN_DBC_NAME(obj) #obj
// перенести в sys/can.h
typedef uint32_t canid_t;
// \see in include/linux/can.h: SocketCAN
struct can_frame {
	canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	union {
		uint8_t len;	/* frame payload length in byte (0 .. 8) */
		uint8_t can_dlc; // deprecated
	};
	uint8_t    __pad;   /* padding */
	uint8_t    __res0;  /* reserved / padding */
	uint8_t    __res1;  /* reserved / padding */
	uint8_t data[8] __attribute__((aligned(8)));
};
struct canfd_frame {
	canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t    len;     /* frame payload length in byte (0 .. 64) */
	uint8_t    flags;   /* additional flags for CAN FD */
	uint8_t    __res0;  /* reserved / padding */
	uint8_t    __res1;  /* reserved / padding */
	uint8_t    data[64] __attribute__((aligned(8)));
};

// Определение фильтров
struct can_filter {
	canid_t can_id;
	canid_t can_mask;
};

#define CAN_MTU   (sizeof(struct can_frame))   // == 16  => Classical CAN frame
#define CANFD_MTU (sizeof(struct canfd_frame)) // == 72  => CAN FD frame




typedef struct can_frame can_frame_t;
// вычисление контрольной суммы кадра
unsigned char can_j1850_crc(unsigned char* buf, size_t len);

#endif//_CAN_EV_H 