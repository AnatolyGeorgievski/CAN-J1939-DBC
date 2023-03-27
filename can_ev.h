#ifndef _CAN_EV_H
#define _CAN_EV_H

#include <stdint.h>
#include <sys/can.h>

#define CAN_SFF_ID_Pos  0
#define CAN_SFF_ID_Bits 11
#define CAN_EFF_ID_Pos  0
#define CAN_EFF_ID_Bits 29

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
#define ID(name) name

// макросы для выделения сигналов из определений DBC
#define CAN_DBC_SIGNAL(value, name) ((value & (name##_Msk))>>(name##_Pos))
#define CAN_DBC_SIGNAL_GET(value, name) ((value & (name##_Msk))>>(name##_Pos))
#define CAN_DBC_SIGNAL_SET(obj, value, name) (((obj)&~(name##_Msk)) | (((value) << (name##_Pos))&(name##_Msk)))
#define CAN_DBC_VALUE(val, name) ((val) * (name##_Factor)+(name##_Offset))
#define CAN_DBC_TYPE(obj) (obj##_Type)
#define CAN_DBC_NAME(obj) #obj

// вычисление контрольной суммы кадра
unsigned char can_j1850_crc(unsigned char* buf, size_t len);

#endif//_CAN_EV_H 