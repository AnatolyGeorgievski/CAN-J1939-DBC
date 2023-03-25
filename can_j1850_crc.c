/*! \file can_crc.c 

	\brief Вычисление контрольной суммы SAE J1850 CRC

Есть два пути расчета CRC от кадра, аппаратно или программно.
Этот вариант программный с небольшой таблицей. Синтезировать таблицу можно любого размера для любого шага.

    \see Каталог контрольных сумм СRC    <http://reveng.sourceforge.net/crc-catalogue/>

	Определение для алгоритма CRC:
CRC-8/SAE-J1850
	width=8 poly=0x1d init=0xff refin=false refout=false xorout=0xff check=0x4b name="CRC-8/SAE-J1850"
	Polynomial 0x11D (x8 + x4 + x3 + x2 + 1)

CRC-8/Autostar H2F
	width=8 poly=0x2F init=0xff refin=false refout=false xorout=0xff check=0xDF name="CRC-8/???"

\see Specification of CRC Routines AUTOSAR CP R19-11

CRC-16/CCITT-FALSE
	width=16 poly=0x1021 init=0xFFFF refin=false refout=false xorout=0x0000 check=0x29B1 name="CRC-16/"
	Alias: CRC-16/AUTOSAR, CRC-16/CCITT-FALSE, CRC-16/IBM-3740
	
CRC-16/XMODEM
	width=16 poly=0x1021 init=0x0000 refin=false refout=false xorout=0x0000 check=0x31c3 name="CRC-16/XMODEM"
	используется в протоколе SDO CANopen для загрузки данных
	\see CANopen application layer and communication profile
	
CRC-16/ARC
	width=16 poly=0x8005 init=0x0000 refin=true refout=true xorout=0x0000 check=0xBB3D name="CRC-16/ARC"
	
CRC32/IEEE-802.3
	width=32 poly=0x04C11DB7 init=0xFFFFFFFF refin=true refout=true xorout=0xFFFFFFFF check=0xCBF43926 name="CRC-32/"
CRC32/
	width=32 poly=0xF4ACFB13 init=0xFFFFFFFF refin=true refout=true xorout=0xFFFFFFFF check=0x1697D06A name="CRC-32/"

Тестирование:
$ gcc -DTEST_CRC can_j1850_crc.c -o crc.exe
$ ./crc.exe
CRC = 4B ..ok

*/
#include <stddef.h>

#define CRC8_J1850_INIT 	0xFF
#define CRC8_J1850_POLY 	0x1D
#define CRC8_J1850_XOR 		0xFF
#define CRC8_J1850_CHECK 	0x4B
static const unsigned char CRC8I_Lookup4[16] = {
0x00, 0x1D, 0x3A, 0x27,
0x74, 0x69, 0x4E, 0x53,
0xE8, 0xF5, 0xD2, 0xCF,
0x9C, 0x81, 0xA6, 0xBB,
};

unsigned char can_j1850_crc(unsigned char* buf, size_t len){
	unsigned char crc = CRC8_J1850_INIT;
	int i;
	for(i=0; i<len; i++) {
		crc^= *buf++;
		crc = (crc << 4) ^ CRC8I_Lookup4[(crc&0xFF) >> 4];
		crc = (crc << 4) ^ CRC8I_Lookup4[(crc&0xFF) >> 4];
	}
	return crc^CRC8_J1850_XOR;
}

#ifdef TEST_CRC
#include <stdio.h>
int main(){
	unsigned char test[] = "123456789";
	unsigned char crc = can_j1850_crc(test, 9);
	printf ("CRC = %02X ..%s\n", crc, crc==CRC8_J1850_CHECK?"ok":"fail");
}
#endif