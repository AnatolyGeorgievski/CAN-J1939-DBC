#ifndef SYS_CAN_H
#define SYS_CAN_H
#include <stdint.h>
//#include <sys/socket.h>

#define CAN_EFF_FLAG 0x80000000U /* EFF/SFF is set in the MSB */
#define CAN_RTR_FLAG 0x40000000U /* remote transmission request */
#define CAN_ERR_FLAG 0x20000000U /* error message frame */

#define CAN_SFF_MASK 0x000007FFU /* standard frame format (SFF) */
#define CAN_EFF_MASK 0x1FFFFFFFU /* extended frame format (EFF) */

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
#if 0
struct sockaddr_can {
	sa_family_t can_family;
	int         can_ifindex;
	union {
		/* transport protocol class address info (e.g. ISOTP) */
		struct { canid_t rx_id, tx_id; } tp;
		/* J1939 address information */
		struct {
			/* 8 byte name when using dynamic addressing */
			uint64_t name;
			/* pgn:
			 * 8 bit: PS in PDU2 case, else 0
			 * 8 bit: PF
			 * 1 bit: DP
			 * 1 bit: reserved
			 */
			uint32_t pgn;
			/* 1 byte address */
			uint8_t addr;
		} j1939;
		/* reserved for future CAN protocols address information */
	} can_addr;
};
#endif//

#define CAN_MTU   (sizeof(struct can_frame))   // == 16  -- Classical CAN frame
#define CANFD_MTU (sizeof(struct canfd_frame)) // == 72  -- CAN FD frame
#endif// SYS_CAN_H
