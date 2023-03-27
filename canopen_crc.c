#include <stdint.h>
#define CRC16_POLY 0x1021
#define CRC16_XMODEM_CHECK 0x31c3

#if 0 // Альтернативная реализация, авторская 
uint16_t canopen_crc1(unsigned char* data, size_t len){
	uint16_t crc = 0;
	int i;
	for (i=0; i<len; i++){
		crc ^= __builtin_bswap16((uint16_t)data[i]);
		crc = (crc << 4) ^ (CRC16_POLY * (crc >> 12));
		crc = (crc << 4) ^ (CRC16_POLY * (crc >> 12));
	}
	return crc;
}
#endif
static uint16_t	CRC16_update_8(uint16_t x, uint8_t *data)
{
	x = *data;
	x = x ^ x>>4;
	return x <<12 ^ x <<5 ^ x;
}
static uint16_t	CRC16_update_16(uint16_t x, uint8_t *data)
{
	x = x ^ __builtin_bswap16(*(uint16_t*)data);
	x = x ^ x>>4 ^ x>>8 ^ x>>11 ^ x>>12;
	return x<<12 ^ x<<5 ^ x;
}
/*! \brief Алгоритм расчета контрольной суммы кадра CRC-16/XMODEM
	\param 
	\retval CRC16 - значение циклической контрольной суммы

	используется в протоколе SDO CANopen для загрузки данных

CRC-16/XMODEM
	width=16 poly=0x1021 init=0x0000 refin=false refout=false xorout=0x0000 check=0x31c3 name="CRC-16/XMODEM"
	\see CANopen application layer and communication profile
*/ 
uint16_t canopen_crc(unsigned char* data, size_t len){
	uint16_t crc = 0;
	int i=0;
	if (len&1) {
		crc = CRC16_update_8(crc, data);
		i++;
	}
	for (; i<len; i+=2){
		crc = CRC16_update_16(crc, data+i);
	}
	return crc;
}
#ifdef TEST_CRC16
#include <stdio.h>
int main(){
	unsigned char test[] = "123456789";
	uint16_t crc = canopen_crc(test, 9);
	printf ("CRC = %04X ..%s\n", crc, crc==CRC16_XMODEM_CHECK?"ok":"fail");
}
#endif