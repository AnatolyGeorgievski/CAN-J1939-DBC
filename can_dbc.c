/*! \brief Чтение и разбор формата SAE J1939 DBC
	\author Анатолий Георгиевский <anatoly.georgievski@gmail.com>, 2023
	\date 16-03-2023

	Сборка
$ gcc can_dbc.c -o dbc `pkg-config.exe --cflags --libs glib-2.0`

Описание формата
|  выбор элемента
(* *) комментарий
[] - необязательный элемент
{} - повторение 0 и более раз
1. Шапка 

2. Пространство имен
<pre>
VERSION "created by canmatrix"


NS_ : 
	NS_DESC_
	CM_
	BA_DEF_
	BA_
	VAL_
	CAT_DEF_
	CAT_
	FILTER
	BA_DEF_DEF_
	EV_DATA_
	ENVVAR_DATA_
	SGTYPE_
	SGTYPE_VAL_
	BA_DEF_SGTYPE_
	BA_SGTYPE_
	SIG_TYPE_REF_
	VAL_TABLE_
	SIG_GROUP_
	SIG_VALTYPE_
	SIGTYPE_VALTYPE_
	BO_TX_BU_
	BA_DEF_REL_
	BA_REL_
	BA_DEF_DEF_REL_
	BU_SG_REL_
	BU_EV_REL_
	BU_BO_REL_
	SG_MUL_VAL_

BS_:
BU_:
BO_
	SG_
	SG_
</pre>

3. Определение линии, скорость передачи
<pre>
bit_timing = 'BS_:' [baudrate ':' BTR1 ',' BTR2] ; 
BTR1 = unsigned_int ;
BTR2 = unsigned_int ;
</pre>

4. Определение узла, перечисление узлов в сети
<pre>
node = 'BU_:' {node_name} ;
node_name = C_identifyer ;
</pre>

5. Определение кадра
<pre>
messages = {message} ;
message = 'BO_' message_id message_name ':' message_size transmitter {signal};
message_id = unsigned_int ;
transmitter = node_name | 'Vector_XXX' ;
</pre>

6. Определение сигнала
<pre>
signal = 'SG_' signal_name multiplexor_id ':' start_bit '|'
	signal_size '@' byte_order value_type (factor ',' offset ')'
	'[' minimum '|' maximum ']' unit receiver { ',' receiver } ; 
signal_name = C_identifier ;
multiplexor_id = ' ' | 'M' | m multiplexer_switch_value;
byte_order = '0' | '1' ; (* 0-little endian, 1-big endian *)
value_type = '+' | '-' ; (* + unsigned, - signed *)
unit = char_string ;
receiver = node_name | 'Vector_XXX' ;
</pre>

Правила получения числового формата 
<pre>
 physiacal_value = raw_value * factor + offset;
 raw+value = (physiacal_value - offset)/factor;
</pre>

7. Определение сообщений и комментариев, расшифровка кодов, анотация
<pre>
comment = 'CM_' Object message_id node_name '"' Comment '"';
Object = 'BU_'|'BO_'|'SG_' ;
</pre>

8. Определение атрибутов
<pre>
'BA_DEF_' Object AttributeName ValueType Min Max;
ValueType = 'INT' | 'STRING' | 'REAL' | 'ENUM' ; (* int, STRING, float или REAL , ENUM *) 
Min Max - диапазон значений
'BA_DEF_DEF_' AttributeName DefaultValue;
DefaultValue - начальное значение атрибута
</pre>

9. Таблица значений для типов заданных перечислением, синтезирует enum
<pre>
'VAL_' message_id signal_name N “DefineN” …… 0 “Define0”;
</pre>
10. Атрибуты сообщений
GenMsgSendType -- Defines the send type of a message. Supported types:
    cyclic
    triggered
    cyclicIfActive
    cyclicAndTriggered
    cyclicIfActiveAndTriggered
    none 
Definition: 
BA_DEF BO_ "GenMsgSendType" ENUM "cyclic","triggered","cyclicIfActive","cyclicAndTriggered","cyclicIfActiveAndTriggered","none" Default: none 

Definition: 
BA_DEF_DEF "GenMsgSendType" "none"

GenMsgCycleTime -- Defines the cycle time of a message in ms. 
Definition: BA_DEF BO_ "GenMsgCycleTime" INT 0 0 
Default: 0 
Definition: BA_DEF_DEF "GenMsgCycleTime" 0

GenMsgStartDelayTime -- Defines the allowed delay after startup this message must occure the first time in ms. 
Definition: BA_DEF BO_ "GenMsgStartDelayTime" INT 0 0 
Default: 0 (=GenMsgCycleTime) 
Definition: BA_DEF_DEF "GenMsgStartDelayTime" 0
GenMsgDelayTime -- Defines the allowed delay for a message in ms. 
Definition: 
BA_DEF BO_ "GenMsgDelayTime" INT 0 0 
Default: 0 
Definition: BA_DEF_DEF "GenMsgDelayTime" 0

## Signal Attributes
_GenSigStartValue -- Defines the value as long as no value is set/received for this signal. 
Definition: 
BA_DEF SG_ "GenSigStartValue" INT 0 0 
Default: 0 
Definition: BA_DEF_DEF "GenSigStartValue" 0

	\see Kvaser Database Editor 3
*/
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <glib.h>

#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */

#define CAN_SFF_ID_BITS 11
#define CAN_EFF_ID_BITS 29

/* для работы макросов нужно определить ряд констант по каждому сигналу SG_ и по каждому сообщению BO_

Правила составления имен
BO_: message_id message_name : message_size transmitter 

#define message_name message_id -- идентификатор сообщения

Правила составления идентификаторов
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

Для типа enumerated создается перечисление на основе 
VAL_ message_id signal_name 
#define CAN_DBC_VALUE_ID(v) используется для получения значения по имени
#define CAN_DBC_VALUE_NAME(v) используется для получения текстового имени
 */
// MilCAN:			if (can_id & J1939_EDP_Msk) -- MilCAN
#define MilCAN_COMMAND_Msk	0x80000000
#define MilCAN_SEGMENT_Msk	0x40000000
// J1939: разбор форматов
#define J1939_PRIORITY_Msk	0x1C000000
#define J1939_PRIORITY_Pos	26
#define J1939_EDP_Msk 		0x02000000
#define J1939_EDP_Pos 		25
#define J1939_DP_Msk 		0x01000000
#define J1939_DP_Pos 		24
#define J1939_PF_Msk 		0x00FF0000
#define J1939_PF_Pos 		16
#define J1939_PS_Msk 		0x0000FF00
#define J1939_PS_Pos 		8
#define J1939_SA_Msk 		0x000000FF
#define J1939_SA_Pos 		0
#define J1939_PDU1_PGN_Msk 	0x03FF0000
#define J1939_PDU1_PGN_Pos	8	
#define J1939_PDU2_PGN_Msk	0x03FFFF00
#define J1939_PDU2_PGN_Pos	8	

#define J1939_PF2_MASK 		0x00F00000	// >=240 broadcast
#define J1939_DA_BROADCAST 0xFF 	// Destination Address -- Broadcast


//#include <evm_objects.h>
// идентификатор объекта
#define MASK(name) (((~0UL) >> (32-(name##_Bits)))<<(name##_Pos))
#define UVAL(name,v) (((v) & (name##_Msk))>>(name##_Pos))

#define EVM_OID_TYPE_Bits	10
#define EVM_OID_TYPE_Pos	(32-EVM_OID_TYPE_Bits)
#define EVM_OID_TYPE_Msk	MASK(EVM_OID_TYPE)
#define EVM_OID_ID_Bits		(32-EVM_OID_TYPE_Bits)
#define EVM_OID_ID_Pos		0
#define EVM_OID_ID_Msk		MASK(EVM_OID_ID)

#define EVM_OID(type, id) ((id)|((type)<<EVM_OID_TYPE_Pos))

#define EVM_TIME_Bits 32
#define EVM_TIME_FRAC_Bits		15
#define EVM_TIME_MINS_Bits		6
#define EVM_TIME_SEC_Bits		6
#define EVM_TIME_HOURS_Bits		5

#define EVM_TIME_FRAC_Pos		0
#define EVM_TIME_SEC_Pos		(EVM_TIME_FRAC_Pos+EVM_TIME_FRAC_Bits)
#define EVM_TIME_MINS_Pos		(EVM_TIME_SEC_Pos +EVM_TIME_SEC_Bits )
#define EVM_TIME_HOURS_Pos		(EVM_TIME_MINS_Pos+EVM_TIME_MINS_Bits)


enum SG_TYPE_ { // в точности соответствует типам данных в протоколе BACnet и EVM
	SG_TYPE_NULL	        =0x0,
	SG_TYPE_BOOLEAN         =0x1,
	SG_TYPE_UNSIGNED        =0x2,
	SG_TYPE_INTEGER        	=0x3,
	SG_TYPE_REAL	        =0x4,
	SG_TYPE_DOUBLE	        =0x5,
	SG_TYPE_OCTETS    		=0x6,
	SG_TYPE_STRING    		=0x7,
	SG_TYPE_BIT_STRING		=0x8,
	SG_TYPE_ENUMERATED	 	=0x9,
	SG_TYPE_DATE			=0xA,
	SG_TYPE_TIME			=0xB,
	SG_TYPE_OID            	=0xC,// уникальный идентификатор состоит из 
};

#define EVM_TAG(v) ((v)&0xF0)
#define EVM_TYPE_CONTEXT 0x08
#define EVM_SIZE_Msk 	0x07

static inline uint8_t* evm_decode_tag(uint8_t * data, unsigned *tag, int *len){
	*tag = EVM_TAG(data[0]);
	uint32_t l = *data++ & 0xF;
	if (l>5){
		*len = -1;
	} else {
		if (l!=0) {
			if (l<5){
				l = 1u<<l; // 1,2,4,8
			} else
			if (l==5) {
				l = *data++;
			}
		}
	}
	*len = l;
	return data;
}
 
#define CAN_DBC_FIELD(value, name) ((value & (name##_Msk))>>(name##_Pos))
#define CAN_DBC_SIGNAL_GET(value, name) ((value & (name##_Msk))>>(name##_Pos))
#define CAN_DBC_SIGNAL_SET(obj, value, name) (((obj)&~(name##_Msk)) | (((value) << (name##_Pos))&(name##_Msk)))
#define CAN_DBC_VALUE(val, name) ((val) * (name##_Factor)+(name##_Offset))
#define CAN_DBC_TYPE(obj) (obj##_Type)
#define CAN_DBC_NAME(obj) (obj##_Name)

typedef uint32_t canid_t;
struct can_frame {
	canid_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t can_dlc; /* frame payload length in byte (0 .. 8) */
	uint8_t    __pad;   /* padding */
	uint8_t    __res0;  /* reserved / padding */
	uint8_t    __res1;  /* reserved / padding */
	uint8_t data[8] __attribute__((aligned(8)));
};

typedef struct can_frame can_frame_t;
typedef struct _can_dbc can_dbc_t;
typedef struct _can_dbc_object can_dbc_object_t;
typedef struct _can_dbc_signal can_dbc_signal_t;
struct _can_dbc_unit {
	uint32_t oid;// OID(BU,id)
	GQuark name_id;
};
struct _can_dbc_object {
	uint32_t oid;// OID(BO,id)
	GQuark name_id;
	GQuark transmitter;
	
	can_dbc_signal_t* signals;// список полей
	uint16_t sg_size;// число полей
	uint8_t data_len;
//
	GData * attrs;//!< атрибуты
	GSList* sg_list;
	char* comment; //!< комментарий CM_ BO_ 
};

//1. разбор формата DBC, получаем структуру can_dbc_t
struct _can_dbc {
	char* version;	//!< версия файла
	// BU_:
	char** block_units;	//!< таблица имен блоков
	uint8_t  bu_size;	//!< размер таблицы имен
	uint16_t bo_size;	//!< размер таблицы сообщений
	// BO_:
	GTree* objects;
	GTree* signals;
//	can_dbc_object_t* objects;//!< таблица объектов
	// SG_:
//	can_dbc_signal_t* signals;//!< таблица сигналов
	// BS_:
	uint32_t baudrate;//!< скорость передачи данных на линии
	// CM_
	GString comments;//!< коментарии к проекту
};
// таблицы имен идентификаторов, используются для разбора и без разбора
typedef struct _Names Names_t;
struct _Names {
	uint32_t key; 
	const char* name;
};
// Имена типов используемые для синтеза
const char* names_type[] = {
	[SG_TYPE_NULL]	        ="unsigned",
	[SG_TYPE_BOOLEAN]     	="unsigned",
	[SG_TYPE_UNSIGNED]      ="unsigned",
	[SG_TYPE_INTEGER]       ="signed",
	[SG_TYPE_REAL]	        ="float",
	[SG_TYPE_DOUBLE]	    ="double",
	[SG_TYPE_OCTETS]    	="Octets_t",
	[SG_TYPE_STRING]    	="String_t",// строка UTF-8 может быть ограниченной длины
	[SG_TYPE_BIT_STRING]	="BitString_t",
	[SG_TYPE_ENUMERATED]	="Enumerated_t",
	[SG_TYPE_DATE]			="Date_t",
	[SG_TYPE_TIME]			="Time_t",
	[SG_TYPE_OID]           ="OID_t",// уникальный идентификатор
};
typedef uint32_t OID_t;
typedef unsigned Enumerated_t;

typedef struct _Octets Octets_t;
typedef struct _BitString BitString_t;
typedef struct _Date Date_t;
typedef struct _Time Time_t;
struct _BitString {
	uint8_t *data;
	uint16_t bits;
};
struct _Octets {
	uint8_t *data;
	uint16_t size;
};
typedef struct _String String_t;
struct _String {
	uint8_t *data;
	uint16_t len;
};

//2. если к пакету can_frame применить разбор can_dbc_decode() или can_dbc_debug()
struct _can_dbc_signal{
	unsigned pos:6;	// в битах от начала
	unsigned len:6;	// длина в битах 0-64
	unsigned type:4; // data type UNSIGNED, SIGNED, FLOAT

	unsigned mux:1; // поле является мультиплексором
	unsigned byte_order:1; // поле 
//	unsigned  SPN:16;		/*!< идентификатор описания */
	GQuark name_id;// идентификатор параметра - кварк или индекс в таблице имен
// вынести 
	GQuark units;// идентификатор единицы измерения - кварк или enum
	float factor, offset;
	unsigned min, max;
	struct {
		const Names_t* vals;
		int size;
	} enumerated;
	char* comment;
};
static
uint64_t can_signal_value(can_frame_t *frame, can_dbc_signal_t* sg)
{
	uint64_t val = *(uint64_t*)frame->data;
	if (sg->byte_order){
	}
	if (sg->type == SG_TYPE_INTEGER) {
		val = (val>>sg->pos) & (~0ULL)>>(63-sg->len);
	} else
	return val;
//	return val*sg->factor + sg->offset;
}


/*! \brief бинарный поиск по массиву идентификаторов 

// bsearch(uint32_tconst void *key, const void *base, size_t num, size_t size,
//              int (*cmp)(const void *key, const void *))
*/
static const char * get_name(uint32_t key, const Names_t * names, size_t num){
	size_t l = 0, u = num;
	while (l < u) {
		register const size_t mid = (l + u)>>1;
		register int result = key - names[mid].key;
		if (result < 0)
			u = mid;
		else if (result > 0)
			l = mid + 1;
		else
			return names[mid].name;
	}
	return NULL;
}
/* 	\brief выделяет из массива описаний объектов CAN по индексу
	\param dbc_objects - массив описаний объектов, упорядоченный по идентификатору сообщения
	\param size - длина массива, число записей
	\param index - идентификатор сообщения
	\return NULL если объект не найден
 */
const can_dbc_object_t* can_dbc_object_get(const can_dbc_object_t * dbc_objects, unsigned int size, unsigned index);
// далее есть два варианта - сохранить фрейм целиком, 64 бита, или выделить поле
// мы не выделяем 64 битные поля
const can_dbc_signal_t* can_dbc_signal_get(const can_dbc_signal_t *dbc_sg, unsigned int size,  unsigned int signal_id);

static gint oid_cmp (  gconstpointer a,  gconstpointer b){
	return a-b;
}
static gint cmp_pos_cb (  gconstpointer a,  gconstpointer b){
	const can_dbc_signal_t* as = a;
	const can_dbc_signal_t* bs = b;
	return as->pos - bs->pos;
}
can_dbc_t* can_dbc_init(can_dbc_t* dbc)
{
	if (dbc==NULL) dbc = g_new0(can_dbc_t,1);
	dbc->objects = g_tree_new (oid_cmp);
//	dbc->signals = g_tree_new (oid_cmp);
	return dbc;
}
void can_dbc_free(can_dbc_t* dbc){
	g_tree_destroy(dbc->objects);
	g_free(dbc);
}

int can_dbc_debug(can_frame_t *frame, can_dbc_t* dbc,  GError* err)
{
	canid_t can_id = frame->can_id;
	uint32_t idx = 0; //
	if (can_id & CAN_EFF_FLAG){// Extended Frame Format
		// CANopen:
		

		
	 // J1939
		uint8_t priority;
		// uint32_t PGN;
		uint8_t SA, DA; //Source and Destination Address

		priority = CAN_DBC_FIELD(can_id, J1939_PRIORITY);
		SA = CAN_DBC_FIELD(can_id, J1939_PS);
		if ((can_id & J1939_PF2_MASK) == J1939_PF2_MASK) {// >=240 broadcast
			idx = (can_id & J1939_PDU2_PGN_Msk)>>J1939_PDU2_PGN_Pos;
			DA  = J1939_DA_BROADCAST;
		} else {
			idx = (can_id & J1939_PDU1_PGN_Msk)>>J1939_PDU1_PGN_Pos;
			DA  = SA;
		}
		// priority,PGN,DA,SA
	} else {// Standard Frame Format
		idx = can_id & CAN_SFF_MASK;// standard frame format (SFF) has a 11 bit
	}
	

	can_dbc_object_t * obj = g_tree_lookup(dbc->objects, GUINT_TO_POINTER(idx));// хеш таблица или бинарное дерево
	char* block_name = dbc->block_units[obj->name_id];// имена функциональных блоков, bsearch
	//GSList * sg_rec  = obj->sg_list;
	int i;
	for (i=0; i< obj->sg_size; i++) 
	{
		can_dbc_signal_t *sg = &obj->signals[i];
		uint64_t value = *(uint64_t*)&frame->data[0];
		if (sg->mux) {// мультиплексор, перейти к разбору строки
			printf("Multiplexed PDO idx=%d:\n", value);
			//sg_rec = _mux_record(sg_rec->mux_records, value);
		} else {
//			printf(" %s" 
			switch (sg->type){
			case SG_TYPE_NULL: break;
			case SG_TYPE_UNSIGNED:
				printf(" %u",  (unsigned)(value*sg->factor+sg->offset));
				break;
			case SG_TYPE_INTEGER:
				printf(" %d",  (signed)(value*sg->factor+sg->offset));
				break;
			case SG_TYPE_ENUMERATED: {
				const char* val = get_name((uint32_t)value, sg->enumerated.vals, sg->enumerated.size);
				printf(" %s(%d)", val, value);
			} break;
			case SG_TYPE_REAL:
				printf(" %g",  value*sg->factor+sg->offset);
				break;
			case SG_TYPE_STRING: {
			} break;
			}
			if (sg->units!=0) printf (" %s", g_quark_to_string(sg->units));
			if (sg->comment!=NULL) printf (" -- %s\n", sg->comment);
			else printf("\n");
		}
	}
	return 0;
}
static gboolean _object_define_print_cb(  gpointer key,  gpointer value,  gpointer user_data  )
{
	can_dbc_object_t* obj = value;
	GString* str = user_data;
	if (obj->sg_list==NULL) return FALSE;
	GSList* sg_list = obj->sg_list;
	while (sg_list){
		can_dbc_signal_t *sg = sg_list->data;
		const char *name = g_quark_to_string(sg->name_id);
//		g_string_append_printf(str, "#define %s_Name  \t\"%s\"\n", name, name);
		g_string_append_printf(str, "#define %s_Pos   \t%d\n", name, sg->pos);
//		g_string_append_printf(str, "#define %s_Msk   \t0x%016llXULL\n", name, (~0ULL)>>(64-sg->len)<<sg->pos);
		g_string_append_printf(str, "#define %s_Bits  \t%d\n", name, sg->len);
		g_string_append_printf(str, "#define %s_Factor\t%g\n", name, sg->factor);
		g_string_append_printf(str, "#define %s_Offset\t%g\n", name, sg->offset);
		if (sg->units!=0) {
			const char* units = g_quark_to_string(sg->units);
			g_string_append_printf(str, "#define %s_Units\t\"%s\"\n", name, units);
		}
		g_string_append_c(str, '\n');
		sg_list = sg_list->next;
	}
	return FALSE;
}
static gboolean _object_struct_print_cb(  gpointer key,  gpointer value,  gpointer user_data  )
{
	can_dbc_object_t* obj = value;
	if (obj->sg_list==NULL) return FALSE;
	const char* name = g_quark_to_string(obj->name_id);

	g_string_append_printf((GString*) user_data, "typedef struct _%s %s_t;\n", name, name);
	g_string_append_printf((GString*) user_data, "struct _%s {\n", name);

	int i;
	int offset=0;
	//for (i=0; i< obj->sg_size; i++) 
	GSList* sg_list = obj->sg_list;
	while (sg_list){
		//can_dbc_signal_t *sg = &obj->signals[i];
		can_dbc_signal_t *sg = sg_list->data;
		const char *type = names_type[sg->type];
		const char *name = g_quark_to_string(sg->name_id);
		if (offset<sg->pos) {
			g_string_append_printf((GString*) user_data, "%12s __r%d:%d;\t// %d..%d:\n",
				"unsigned",offset,sg->pos - offset, offset, sg->pos-1);
			offset = sg->pos;
			
		}
		g_string_append_printf((GString*) user_data, "%12s %-22s:%d;\t// %d..%d:\n", type, name, 
			sg->len, sg->pos, sg->pos+sg->len-1);
		offset += sg->len;
		sg_list = sg_list->next;
	}
	g_string_append((GString*) user_data, "};\n\n");
	return FALSE;
}
/*! \brief генерация перечисления блоков */
static gboolean _object_enumerate_print_cb(  gpointer key,  gpointer value,  gpointer user_data  )
{
	can_dbc_object_t* obj = value;
	GString* str = user_data;
	g_string_append_printf(str, "\tBO_%-20s\t=0x%X,", 
			g_quark_to_string(obj->name_id), GPOINTER_TO_UINT(key) & 0x1FFFF);
	if (obj->comment) 
		g_string_append_printf(str, "\t/*!< %s */", obj->comment);
	g_string_append_c(str,'\n');
	return FALSE;
}
/*! \brief генерация исходников */
GString* can_dbc_gen_header(can_dbc_t *dbc, const char* filename)
{
	int i;
	GString* str = g_string_new_len(NULL, 1024*4);
	char* header = g_ascii_strup(filename, -1);
	str = g_string_append (str, "#ifndef _");
	str = g_string_append (str, header);
	str = g_string_append (str, "\n#define _");
	str = g_string_append (str, header);
	str = g_string_append (str, "\n\n");

	str = g_string_append (str, "\nenum BU_ {\n");
	for (i=0; i< dbc->bu_size; i++){
		g_string_append_printf (str, "\tBU_%s,\n", dbc->block_units[i]);
	}
	str = g_string_append (str, "};\n");

	str = g_string_append (str, "\nenum BO_ {\n");
	g_tree_foreach (dbc->objects, _object_enumerate_print_cb, str);
	str = g_string_append (str, "};\n");

	str = g_string_append (str, "/* Defines */\n");
	g_tree_foreach (dbc->objects, _object_define_print_cb, str);

	str = g_string_append (str, "/* Messages */\n");
	g_tree_foreach (dbc->objects, _object_struct_print_cb, str);

	str = g_string_append (str, "\n#endif//_");
	str = g_string_append (str, header);
	str = g_string_append (str, "\n");
	g_free(header);
	return str;
}

/*! \brief разбор формата */
int can_dbc_parse(char *buf, int size, can_dbc_t *dbc)
{
//	GSList_t **tail = &dbc->value.list;
	char* str = buf;
	while(isspace(str[0])) str++;
	char ch = *str++;
	while(ch!='\0') {
		
	}
	return size;
}
static GQuark _id(char* s, int len){
	char ch = s[len];
	s[len] = '\0';
	GQuark id = g_quark_from_string(s);
	s[len] = ch;
	return id;
}

/* Синтезировать структуру разбора сообщений для заданного устройства 
 */
#include <locale.h>

typedef struct _MainOptions MainOptions;
struct _MainOptions {
    gchar *  input_file;
    gchar * output_file;
    gchar * config_file;
    gboolean verbose  ;
};
static MainOptions options = {
    .input_file = NULL, // подписка по опросу устройств
    .output_file = "test.h",
    .verbose = FALSE,
};
static GOptionEntry entries[] =
{

  { "input",    'i', 0, G_OPTION_ARG_FILENAME,  &options.input_file,    "input  file name",  "*.xml" },
  { "config",   'c', 0, G_OPTION_ARG_FILENAME,  &options.config_file,   "config file name", "*.conf" },
  { "output",   'o', 0, G_OPTION_ARG_FILENAME,  &options.output_file,   "output file name", "*.xml" },

  { "verbose",  'v', 0, G_OPTION_ARG_NONE,      &options.verbose,       "Be verbose",       NULL },
  { NULL }
};

int main (int argc, char*argv[])
{
    setlocale(LC_ALL, "");
    setlocale(LC_NUMERIC, "C");
    GError* error = NULL;
    GOptionContext *context;
    context = g_option_context_new ("- command line interface");
    g_option_context_add_main_entries (context, entries, NULL/*GETTEXT_PACKAGE*/);
    if (!g_option_context_parse (context, &argc, &argv, &error))
    {
        g_print ("option parsing failed: %s\n", error->message);
        exit (1);
    }
    g_option_context_free (context);

	int verbose=options.verbose;
	if (argc<2) return 1;
	FILE* fp = fopen(argv[1], "r");
	if (fp==NULL) return 1; 
	if (verbose) printf("File %s\n", argv[1]);
	char buf[1024];
	can_dbc_t * dbc =  can_dbc_init(NULL);
	can_dbc_object_t* object = NULL;
	while (fgets(buf, 1024, fp)!=NULL){
		char *s = buf;
		bool mux=false;
		while (s[0]==' ') s++;
		if (strncmp(s, "BO_ ", 4)==0){// bus object
			s+=4;
			uint64_t idx = strtol(s, &s, 10);
			while (isspace(s[0])) s++;
			if (isalpha(s[0])) {
				char* name = s++;
				while (isalnum(s[0]) || s[0]=='_') s++;
				int len = s - name;
				int size=0;
				while (isspace(s[0])) s++;
				if (s[0]=='M') {
					mux = true;
					s++;
					while (isspace(s[0])) s++;
				}
				if (s[0]==':'){
					s++;
					while (isspace(s[0])) s++;
				}
				if (isdigit(s[0]))
					size = strtol(s, &s, 10);
				while (isspace(s[0])) s++;
				char* unit = NULL;
				if (isalpha(s[0])){
					unit = s++;
					while (isalnum(s[0]) || s[0]=='_') s++;
				}
				
				if (verbose) printf ("BO_ %d %-.*s : %d %-.*s\n", idx, len, name, size, s-unit, unit);
				object = g_slice_new0(can_dbc_object_t);
				object->name_id = _id(name, len);
				object->transmitter = _id(unit, s-unit);
				object->data_len = size;
				g_tree_insert(dbc->objects, GUINT_TO_POINTER(idx), object);
			}
		} else
		if (strncmp(s, "SG_ ", 4)==0){// signals
			s+=4;
			can_dbc_signal_t* sg = g_slice_new0(can_dbc_signal_t);
			int len = 0, pos=0, bits=0, ulen=0;
			bool order_le=false, sign=false;
			char* name = NULL;
			char* units= NULL;
			if (isalpha(s[0])) {
				name = s++;
				while (isalnum(s[0]) || s[0]=='_') s++;
				len = s - name;
				while (isspace(s[0])) s++;
			}
			if (s[0]=='M') {
				mux = true;
				s++;
				while (isspace(s[0])) s++;
			}
			if (s[0]==':'){
				s++;
				while (isspace(s[0])) s++;
			}
			if (isdigit(s[0]))
				pos = strtol(s, &s, 10);
			if (s[0]=='|') s++;
			if (isdigit(s[0]))
				bits = strtol(s, &s, 10);
			if (s[0]=='@') s++;
			if (s[0]=='1') order_le = true;
			s++;
			if (s[0]=='-') sign = true;
			s++;
			while (isspace(s[0])) s++;
			if (s[0]=='('){// множитель и смещение
				s++;
				sg->factor = strtof(s, &s);
				if (s[0]==',') s++; 
				sg->offset = strtof(s, &s);
				if (s[0]==')') s++;
				while (isspace(s[0])) s++;
			}
			if (s[0]=='['){// минимум и максимум
				s++;
				sg->min = strtof(s, &s);
				if (s[0]=='|') s++; 
				sg->max = strtof(s, &s);
				if (s[0]==']') s++;
				while (isspace(s[0])) s++;
			}
			if (s[0]=='"' && (s[1]!='"'&& s[1]!='\0')){// единицы измерения
				s++;
				units = s++;
				while (s[0]!='"' && s[0]!='\0') s++;
				ulen = s - units;
				if (s[0]=='"') s++;
				while (isspace(s[0])) s++;
			}

			if (verbose) 
				printf (" SG_ %-.*s : %d|%d@%c%c\n", len, name, pos, bits, 
						(order_le?'1':'0'), (sign?'-':'+'));
			if (!order_le) pos -= bits-1; 
			
			
			sg->pos = pos, sg->len = bits;
			sg->name_id = _id(name, len);
			if (units!=NULL) 
				sg->units   = _id(units, ulen);
			if (sign) sg->type = SG_TYPE_INTEGER;
			else sg->type = SG_TYPE_UNSIGNED;
			if (object!=NULL) 
				object->sg_list = g_slist_insert_sorted(object->sg_list, sg, cmp_pos_cb);
		} else
		if (strncmp(s, "CM_ ", 4)==0){// comment
			s+=4;
			if (strncmp(s, "BU_ ", 4)==0){
				s+=4;
				char* name = NULL, *comment=NULL;
				int len = 0;
				if (isalpha(s[0])) {
					name = s++;
					while (isalnum(s[0]) || s[0]=='_') s++;
					len = s - name;
					while (isspace(s[0])) s++;
				}
				if (s[0]=='"' && (s[1]!='"'&& s[1]!='\0')){
					s++;
					comment = s++;
					while (s[0]!='"' && s[0]!='\0') s++;
				}
				if (verbose) printf ("CM_ BU_ %-.*s \"%-.*s\"\n", len, name, s-comment, comment);
			} else
			if (strncmp(s, "BO_ ", 4)==0){
				s+=4;
				char* name = NULL, *comment=NULL;
				int len = 0;
				if (isalpha(s[0])) {
					name = s++;
					while (isalnum(s[0]) || s[0]=='_') s++;
					len = s - name;
					while (isspace(s[0])) s++;
				}
				if (s[0]=='"' && (s[1]!='"'&& s[1]!='\0')){
					s++;
					comment = s++;
					while (s[0]!='"' && s[0]!='\0') s++;
				}
				if (verbose) printf ("CM_ BO_ %-.*s \"%-.*s\"\n", len, name, s-comment, comment);
			} else
				printf ("CM_ \n");
		} else
		if (strncmp(s, "BA_ ", 4)==0){// comment
			s+=4;
			char* attr=NULL;
			int len=0;
			if (s[0]=='"' && (s[1]!='"'&& s[1]!='\0')){
				s++;
				attr = s++;
				while (s[0]!='"' && s[0]!='\0') s++;
				len = s - attr;
				s++;
				while (isspace(s[0])) s++;
			}
			if (strncmp(s, "BO_ ", 4)==0){
				s+=4;
				int idx = strtol(s, &s, 10);
				while (isspace(s[0])) s++;
				int value = strtol(s, &s, 10);
				if (verbose) printf ("BA_ \"%-.*s\" BO_ %d %d;\n", len, attr, idx, value);
				object = g_tree_lookup(dbc->objects, GUINT_TO_POINTER(idx));
				if (object){
					GQuark attr_id = _id(attr, len);
					g_datalist_id_set_data(&object->attrs, attr_id, GUINT_TO_POINTER(value));
				}
			}
		} else
		if (strncmp(s, "VAL_", 4)==0){// comment
			s+=4;
			//printf ("VAL_ \n");
		} else
		if (strncmp(s, "BU_", 3)==0){// bus units
			s+=3;
			while (isspace(s[0])) s++;
			if (s[0]==':') {
				s++;
				if (verbose){ 
					printf ("BU_:");
					do {
						// есть вариант превратить в массив gchar**
						while (isspace(s[0])) s++;
						if (isalpha(s[0])) {
							char* name = s++;
							while (isalnum(s[0]) || s[0]=='_') s++;
							printf (" %-.*s", s-name, name);
							GQuark id = _id(name, s - name);
						}
					} while (s[0]!='\0');
					printf ("\n");
				}
			}
		}
		if (feof(fp)) break;
	}
	fclose(fp);	
	
	GString* str = can_dbc_gen_header(dbc, "evm_can_h");
	printf("%s\n", str->str);
	g_string_free(str, TRUE);
	return 0;
}