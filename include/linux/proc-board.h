#ifdef CONFIG_ARCH_ADVANTECH

#ifndef _LINUX_PROC_BOARD_H
#define _LINUX_PROC_BOARD_H

extern char board_type[20];
extern char board_cpu[20];

/* Board CPU */
#define IS_DUALQUAD	( strncmp(board_cpu,"DualQuad", 8)==0 ? 1 : 0)
#define IS_DUALLITESOLO	( strncmp(board_cpu,"DualLiteSolo", 12)==0 ? 1 : 0)

/* Board Type */
#define IS_UBC_DS31	( strncmp(board_type,"UBC-DS31", 8)==0 ? 1 : 0)
#define IS_UBC_DS31_A1	( strncmp(board_type,"UBC-DS31 A1", 11)==0 ? 1 : 0)

#define IS_UBC_220	( strncmp(board_type,"UBC-220", 7)==0 ? 1 : 0)
#define IS_UBC_220_A1	( strncmp(board_type,"UBC-220 A1", 10)==0 ? 1 : 0)

#define IS_RSB_4410	( strncmp(board_type,"RSB-4410", 8)==0 ? 1 : 0)
#define IS_RSB_4410_A1	( strncmp(board_type,"RSB-4410 A1", 11)==0 ? 1 : 0)
#define IS_RSB_4410_A2	( strncmp(board_type,"RSB-4410 A2", 11)==0 ? 1 : 0)

#define IS_ROM_3420	( strncmp(board_type,"ROM-3420", 8)==0 ? 1 : 0)
#define IS_ROM_3420_A1	( strncmp(board_type,"ROM-3420 A1", 11)==0 ? 1 : 0)

#define IS_ROM_5420	( strncmp(board_type,"ROM-5420", 8)==0 ? 1 : 0)
#define IS_ROM_5420_A1	( strncmp(board_type,"ROM-5420 A1", 11)==0 ? 1 : 0)
#define IS_ROM_5420_B1	( strncmp(board_type,"ROM-5420 B1", 11)==0 ? 1 : 0)

#define IS_ROM_7420	( strncmp(board_type,"ROM-7420", 8)==0 ? 1 : 0)
#define IS_ROM_7420_A1	( strncmp(board_type,"ROM-7420 A1", 11)==0 ? 1 : 0)

#define IS_WISE_3310	( strncmp(board_type,"WISE-3310", 9)==0 ? 1 : 0)
#define IS_WISE_3310_A1	( strncmp(board_type,"WISE-3310 A1", 12)==0 ? 1 : 0)

#define IS_ROM_7421     ( strncmp(board_type,"ROM-7421", 8)==0 ? 1 : 0)
#define IS_ROM_7421_A1  ( strncmp(board_type,"ROM-7421 A1", 11)==0 ? 1 : 0)

#define IS_RSB_4411     ( strncmp(board_type,"RSB-4411", 8)==0 ? 1 : 0)
#define IS_RSB_4411_A1  ( strncmp(board_type,"RSB-4411 A1", 11)==0 ? 1 : 0)

#endif /* _LINUX_PROC_BOARD_H */
#endif /* CONFIG_ARCH_ADVANTECH */
