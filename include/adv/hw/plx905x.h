/*******************************************************************************
              Copyright (c) 1983-2009 Advantech Co., Ltd.
********************************************************************************
THIS IS AN UNPUBLISHED WORK CONTAINING CONFIDENTIAL AND PROPRIETARY INFORMATION
WHICH IS THE PROPERTY OF ADVANTECH CORP., ANY DISCLOSURE, USE, OR REPRODUCTION,
WITHOUT WRITTEN AUTHORIZATION FROM ADVANTECH CORP., IS STRICTLY PROHIBITED. 

================================================================================
REVISION HISTORY
--------------------------------------------------------------------------------
$Log: $
--------------------------------------------------------------------------------
$NoKeywords:  $
*/


//
// PCI bridge chip register format definition for Plx9054/Plx9056
// Note: The Plx9054 and Plx9056 are register compatible.
// 

#ifndef _PLX905x_REGISTER_DEFINE
#define _PLX905x_REGISTER_DEFINE

//-----------------------------------------------------------------------------   
// Maximum DMA transfer size (in bytes).
//
//-----------------------------------------------------------------------------   
#define PCI905x_MAXIMUM_TRANSFER_LENGTH    (-1) // unlimited

//-----------------------------------------------------------------------------   
// The DMA_TRANSFER_ELEMENTS (the 9656's hardware scatter/gather list element)
// must be aligned on a 16-byte boundary.  This is because the lower 4 bits of
// the DESC_PTR.Address contain bit fields not included in the "next" address.
//-----------------------------------------------------------------------------   
#define PCI905x_DTE_ALIGNMENT_16           0xF 

//-----------------------------------------------------------------------------   
// Number of DMA channels supported by PLX Chip
//-----------------------------------------------------------------------------   
#define PCI905x_DMA_CHANNELS               (2) 

//-----------------------------------------------------------------------------   
// DMA-64 enabled
//-----------------------------------------------------------------------------
#if defined(_WIN64)
#   define PLX_DMA_ADDR_64          1
#endif

#if defined(__linux__)
#  if defined(CONFIG_HIGHMEM64G) || defined(CONFIG_X86_64) || defined(CONFIG_ARCH_DMA_ADDR_T_64BIT)
#     define PLX_DMA_ADDR_64       1
#  endif
#endif
 
//-----------------------------------------------------------------------------
// DMA Transfer Element (DTE)
// 
// NOTE: This structure is modeled after the DMAPADRx, DMALADRx, DMASIZx and 
//       DMAADPRx registers. See DataBook Registers description: 11-74 to 11-77.
//-----------------------------------------------------------------------------   
#define PLX_DESC_IN_LOCAL    (0)
#define PLX_DESC_IN_PCI      (1)

#define PLX_DESC_DIR_TO_DEV    (0)
#define PLX_DESC_DIR_FROM_DEV  (1)

typedef struct _PLX_DESC_PTR_ {    
   uint32  DescLocation  : 1  ;  // TRUE - Desc in PCI (host) memory
   uint32  LastElement   : 1  ;  // TRUE - last NTE in chain
   uint32  TermCountInt  : 1  ;  // TRUE - Interrupt on term count.
   uint32  DirOfTransfer : 1  ;  // see defines below
   uint32  Address       : 28 ;   
} PLX_DESC_PTR;

//
// Plx9056 scatter/gather list entry
typedef struct _PLX_SGL_ENTRY {   
   uint32        PciAddrLow;
   uint32        LocAddress;
   uint32        DataLength;
   PLX_DESC_PTR  DescPtr;
#if defined(PLX_DMA_ADDR_64)
   uint32        PciAddrHigh;
   uint32        Pad[3];
#endif
} PLX_SGL_ENTRY, * PPLX_SGL_ENTRY;

#define PLX_DESC_PTR_ADDR_SHIFT (4)
#define PLX_DESC_PTR_ADDR(a)    (((uint32)(a)) >> PLX_DESC_PTR_ADDR_SHIFT)

#if defined(PLX_DMA_ADDR_64)
#  define PLX_DAC_CHAIN_LOAD  1
#  define PLX_SET_PCI_ADDR(entry, phys) \
   do {\
      entry->PciAddrLow  = (uint32)phys; \
      entry->PciAddrHigh = (uint32)(phys >> 32); \
   } while(0)
#else
#  define PLX_DAC_CHAIN_LOAD  0
#  define PLX_SET_PCI_ADDR(entry, phys) do { entry->PciAddrLow = phys; } while(0)
#endif

//
// PCI Bridge chip -- PLX9056 -- register format define---------------------------------------------------------------

//
//Local Address Space 0 Local Base Address (Remap)
// format of Local Address Space 0 Local Base Address (Remap)
typedef union _PLX_LAS0_BA {
	uint32 Value;
	struct {
		uint32 Spc0En   : 1;  /* Space 0 Enable. Writing a 1 enables decoding of PCI addresses for Direct
							         Slave access to Local Bus Space 0. Writing a 0 disables decoding. */
		uint32 Reserved : 1;

		uint32 IoLowBit : 2;  /* If Local Bus Space 0 is mapped into Memory space, bits are not used. When
							         mapped into I/O space, included with bits [31:4] for remapping. */
		uint32 BaseAddr : 28; /* Remap PCI Address to Local Address Space 0 into Local Address Space.
							          Bits in this register remap (replace) PCI Address bits used in decode as
							          Local Address bits.
							          Note: Remap Address value must be a multiple of the Range (not the Range register).*/
	};
} PLX_LAS0_BA;

#define BR_PLX_LAS0BA     0x4

//
// Mode/DMA Arbitration ------------------------------------------------------------------------------------
// format of Mode/DMA Arbitration
typedef union _PLX_MOD_DMA_ARB {
	uint32 Value;
	struct {
		uint32 LocBusLatTmr   : 8;  /* Local Bus Latency Timer. Number of Local Bus clock cycles to occur before
								            de-asserting HOLD and releasing the Local Bus.*/ 

		uint32 LocBusPseTmr   : 8;  /* Local Bus Pause Timer. Number of Local Bus Clock cycles to occur before
								            reasserting HOLD after releasing the Local Bus. The pause timer is valid only
								            during DMA.*/

		uint32 LocBusLatTmrEn : 1;  /* Local Bus Latency Timer Enable. Writing a 1 enables the latency timer.
								            Writing a 0 disables the latency timer.*/
		uint32 LocBusPseTmrEn : 1;  /* Local Bus Pause Timer Enable. Writing a 1 enables the pause timer. Writing
								            a 0 disables the pause timer.*/
		uint32 LocBusBREQEn   : 1;  /* Local Bus BREQ Enable. Writing a 1 enables the Local Bus BR#/BREQi.
								            When BR#/BREQi is active, the PCI 9056 de-asserts HOLD and releases
								            the Local Bus.*/
		uint32 DmaCHPriority  : 2;  /* DMA Channel Priority. Writing a 00 indicates a rotational priority scheme.
								            Writing a 01 indicates Channel 0 has priority. Writing a 10 indicates Channel 1
								            has priority. Writing an 11 indicates reserved.*/
		uint32 LocBusDSRBM    : 1;  /* Local Bus Direct Slave Release Bus Mode. When set to 1, the PCI 9056
								            de-asserts HOLD and releases the Local Bus when the Direct Slave Write
								            FIFO becomes empty during a Direct Slave Write or when the Direct Slave
								            Read FIFO becomes full during a Direct Slave Read.*/
		uint32 DsLockEn       : 1;  /* Direct Slave LOCK# Enable. Writing a 1 enables Direct Slave locked
								            sequences. Writing a 0 disables Direct Slave locked sequences.*/
		uint32 PciReqMode     : 1;  /* PCI Request Mode. Writing a 1 causes the PCI 9056 to de-assert REQ# when
								            it asserts FRAME during a Master cycle. Writing a 0 causes the PCI 9056 to
								            leave REQ# asserted for the entire Bus Master cycle.*/

		uint32 DelayReadMode        : 1;  /* Delayed Read Mode. When set to 1, the PCI 9056 operates in Delayed
										            Transaction mode for Direct Slave reads. The PCI 9056 issues a Retry to
										            the PCI Host and prefetches Read data.*/
		uint32 PciRdNoWrMode        : 1;  /* PCI Read No Write Mode. Writing a 1 forces a Retry on writes if a read
										            is pending. Writing a 0 allows writes to occur while a read is pending.*/ 
		uint32 PciRdWithWrFluMode   : 1;  /* PCI Read with Write Flush Mode. Writing a 1 submits a request to flush
										             a pending Read cycle if a Write cycle is detected. Writing a 0 submits
										             a request to not effect pending reads when a Write cycle occurs
										             (PCI r2.2-compatible).*/
		uint32 GateLBLTmrWithBREQi  : 1;  /* Gate Local Bus Latency Timer with BREQi (C and J modes only).*/

		uint32 PciRdNoFluMode       : 1;  /* PCI Read No Flush Mode. Writing a 1 submits a request to not flush the
										             Read FIFO if the PCI Read cycle completes (Read Ahead mode). Writing a 0
										             submits a request to flush the Read FIFO if a PCI Read cycle completes.*/
		uint32 SubsysIDEn           : 1;  /* When set to 0, reads from the PCI Configuration Register address 00h returns
										             Device ID and Vendor ID. When set to 1, reads from the PCI Configuration
										             register address 00h returns Subsystem ID and Subsystem Vendor ID.*/
		uint32 FifoFullStatusFlag   : 1;  /* FIFO Full Status Flag. When set to 1, the Direct Master Write FIFO is almost
										            full. Reflects the value of the DMPAF pin. */
		uint32 BwIOSelect           : 1;  /* BIGEND#/WAIT# Input/Output Select (M mode only). Writing a 1 selects
										            the wait functionality of the signal. Writing a 0 selects Big Endian input
										            functionality.*/
	};
} PLX_MOD_DMA_ARB;

#define BR_PLX_MODDMAARB       0x8   // Low word

//
// Local Address Space 0/Expansion ROM Bus Region Descriptor-------------------------------------------------------
// format of Local Address Space 0/Expansion ROM Bus Region Descriptor
typedef union _PLX_LAS0_BUS_RGN_DPR {
	uint32 Value;
	struct {
		uint32 MemSpc0LBWidth : 2;  /* Memory Space 0 Local Bus Width. Writing a 00 indicates an 8-bit bus width.
								            Writing a 01 indicates a 16-bit bus width. Writing a 10 or 11 indicates a 32-bit
								            bus width. */
		uint32 MemSpc0WaitState : 4; /* Memory Space 0 Internal Wait States (data-to-data; 0-15 wait states). */

		uint32 MemSpc0TRInputEn : 1; /* Memory Space 0 TA#/READY# Input Enable. Writing a 1 enables
									            TA#/READY# input. Writing a 0 disables TA#/READY# input. */
		uint32 MemSpc0BTERMInputEn : 1; /* Memory Space 0 BTERM# Input Enable. Writing a 1 enables BTERM#
									            input. Writing a 0 disables BTERM# input. */

		uint32 MemSpc0PrefetchDis : 1;  /* Memory Space 0 Prefetch Disable. When mapped into Memory space,
									            writing a 0 enables Read prefetching. Writing a 1 disables prefetching. If
									            prefetching is disabled, the PCI 9056 disconnects after each Memory read. */
		uint32 EPROMPrefetchDis : 1;    /* Expansion ROM Space Prefetch Disable. Writing a 0 enables Read
									            prefetching. Writing a 1 disables prefetching. If prefetching is disabled,
									            the PCI 9056 disconnects after each Memory read. */
		uint32 PrefetchCntrEn : 1;      /* Prefetch Counter Enable. When set to 1 and Memory prefetching is enabled,
									            the PCI 9056 prefetches up to the number of Lwords specified in prefetch
									            count. When set to 0, the PCI 9056 ignores the count and continues
									            prefetching until it is terminated by the PCI Bus. */
		uint32 PrefetchCounter : 4;     /* Prefetch Counter. Number of Lwords to prefetch during Memory Read cycles
									            (0-15). A count of zero selects a prefetch of 16 Lwords. */
		uint32 Reserved : 1;            /* */

		uint32 EPROMSpcBusWidth : 2;      /* Expansion ROM Space Local Bus Width. Writing a 00 indicates an 8-bit bus
										             width. Writing a 01 indicates a 16-bit bus width. Writing a 10 or 11 indicates a
										             32-bit bus width. */
		uint32 EPROMSpcWaitState : 4;     /* Expansion ROM Space Internal Wait States (data-to-data; 0-15 wait states). */
		uint32 EPROMSpcTRInputEn : 1;     /* Expansion ROM Space TA#/READY# Input Enable. Writing a 1 enables
										            TA#/READY# input. Writing a 0 disables TA#/READY# input. */
		uint32 EPROMSpcBTERMInputEn : 1;  /* Expansion ROM Space BTERM# Input Enable. Writing a 1 enables
										            BTERM# input. Writing a 0 disables BTERM# input. */

		uint32 MemSpc0BurstEn : 1;        /* Memory Space 0 Burst Enable. Writing a 1 enables bursting. Writing a 0
										            disables bursting */
		uint32 ExtraLongLoad  : 1;        /* Extra Long Load from Serial EEPROM. Writing a 1 loads the Subsystem ID
										            and Local Address Space 1 registers. Writing a 0 indicates not to load them. */
		uint32 EPROMSpc0BurstEn : 1;      /* Expansion ROM Space Burst Enable. Writing a 1 enables bursting. Writing
										            a 0 disables bursting. */
		uint32 DSPCIWriteMode : 1;        /* Direct Slave PCI Write Mode. Writing a 0 indicates the PCI 9056 should
										             disconnect when the Direct Slave Write FIFO is full. Writing a 1 indicates the
										             PCI 9056 should de-assert TRDY# when the Direct Slave Write FIFO is full. */
		uint32 DSRetryDelayClk : 4;       /* Direct Slave Retry Delay Clocks. Contains the value (multiplied by 8) of the
										             number of PCI Bus clocks after receiving a PCI-to-Local Read or Write access
										             and not successfully completing a transfer. Pertains to Direct Slave writes only
										             when the Direct Slave PCI Write Mode bit is set (bit [27]=1). */
	};
} PLX_LAS0_BUS_RGN_DPR;

#define BR_PLX_LAS0BUSRGNDPR   0x18  // Low word

// Interrupt Control / Status register -------------------------------------------------------------------------
// format of Interrupt Control / Status register
typedef union _PLX_INT_CSR {
	uint32 Value;
	struct {
		uint32  TLERRIfRMAEn : 1;      /* Enable Local Bus TEA#/LSERR#. Writing a 1 enables PCI 9056 to assert
									           TEA#/LSERR# interrupt when the Received Master Abort bit is set
									           (PCISR[13]=1 or INTCSR[6]=1). */
		uint32  TLERRIfPCIPERREn : 1;  /* Enable Local Bus TEA#/LSERR# when a PCI parity error occurs during
									            a PCI 9056 Master Transfer or a PCI 9056 Slave access. */
		uint32 GenPCISERR : 1;         /* Generate PCI Bus SERR# Interrupt. When set to 0, writing 1 asserts the
									            PCI Bus SERR# interrupt. */
		uint32 MailBoxIntEn : 1;       /* Mailbox Interrupt Enable. Writing a 1 enables a Local Interrupt to be
									           asserted when the PCI Bus writes to MBOX0 through MBOX3. To clear
									           a Local Interrupt, the Local Bus Master must read the Mailbox. Used in
									           conjunction with the Local Interrupt Output Enable bit (INTCSR[16]). */

		uint32 PwrMgrIntEn : 1;      /* Power Management Interrupt Enable. Writing a 1 enables a Local Interrupt
									         to be asserted when the Power Management Power State changes. */
		uint32 PwrMgrInt : 1;       /* Power Management Interrupt. When set to 1, indicates a Power
								            Management interrupt is pending. A Power Management interrupt is
								            caused by a change in the Power State register (PMCSR). Writing a 1
								            clears the interrupt */
		uint32 LocParityErrEn : 1; /* Direct Master Write/Direct Slave Read Local Data Parity Check Error
								           Enable. Writing a 1 enables a Local Data Parity error signal to be asserted
								           through the LSERR#/TEA# pin. INTCSR[0] must be enabled for this to have
								           an effect. */
		uint32 LocParityErr : 1; /* Direct Master Write/Direct Slave Read Local Data Parity Check Error
								         Status. When set to 1, indicates the PCI 9056 has detected a Local Data
								         Parity check error, even if the Check Parity Error bit is disabled. Writing 1
								         clears this bit to 0. */

		uint32 PciIntEn : 1;        /* PCI Interrupt Enable. Writing a 1 enables PCI interrupts. */
		uint32 PciDrBellIntEn : 1;  /* PCI Doorbell Interrupt Enable. Writing a 1 enables Doorbell interrupts. Used
								            in conjunction with the PCI Interrupt Enable bit (INTCSR[8]). Clearing the
								            doorbell interrupt bits that caused the interrupt also clears the interrupt. */
		uint32 PciAbortIntEn : 1;   /* PCI Abort Interrupt Enable. Values of 1 enables Master Abort or Master
								            detect of a Target Abort to assert a PCI interrupt. Used in conjunction with the
								            PCI Interrupt Enable bit (INTCSR[8]). Clearing the abort status bits also clears
								            the PCI interrupt */
		uint32 LocIntInputEn : 1; /* Local Interrupt Input Enable. Writing a 1 enables a Local interrupt input to
								          assert a PCI interrupt. Used in conjunction with the PCI Interrupt Enable bit
								          (INTCSR[8]). Clearing the Local Bus cause of the interrupt also clears the
								          interrupt. */

		uint32 RetryAbortEn : 1; /* Retry Abort Enable. Writing a 1 enables the PCI 9056 to treat 256 Master
								         consecutive Retries to a Target as a Target Abort. Writing a 0 enables the
								         PCI 9056 to attempt Master Retries indefinitely */
		uint32 PciDrBellIntActive : 1; /* PCI Doorbell Interrupt Active. When set to 1, indicates the PCI Doorbell
									            interrupt is active. */
		uint32 PciAbortIntActive : 1;  /* PCI Abort Interrupt Active. When set to 1, indicates the PCI Abort interrupt
									            is active. */
		uint32 LocIntInputActive : 1;  /* Local Input Interrupt Active. When set to 1, indicates the Local Input
									            interrupt is active. */

		uint32 LocIntOutputEn : 1;  /* Local Interrupt Output Enable. Writing a 1 enables Local interrupt output.
								            Used in conjunction with the Mailbox Interrupt Enable bit (INTCSR[3]). */
		uint32 LocDrBellIntEn : 1;  /* Local Doorbell Interrupt Enable. Writing a 1 enables Doorbell interrupts.
								            Used in conjunction with the Local Interrupt Enable bit. Clearing the Local
								            Doorbell Interrupt bits that caused the interrupt also clears the interrupt. */
		uint32 LocDMA0IntEn : 1;    /* Local DMA Channel 0 Interrupt Enable. Writing a 1 enables DMA Channel 0
								            interrupts. Used in conjunction with the Local Interrupt Enable bit. Clearing the
								            DMA status bits also clears the interrupt. */
		uint32 LocDMA1IntEn : 1;    /* Local DMA Channel 1 Interrupt Enable. Writing a 1 enables DMA Channel 1
								            interrupts. Used in conjunction with the Local Interrupt Enable bit. Clearing the
								            DMA status bits also clears the interrupt. */

		uint32 LocDrBellIntActive : 1; /* Local Doorbell Interrupt Active. Reading a 1 indicates the Local Doorbell
									            interrupt is active. */
		uint32 Dma0IntActive : 1;      /* DMA Channel 0 Interrupt Active. Reading a 1 indicates the DMA Channel 0
									            interrupt is active. */
		uint32 Dma1IntActive : 1;      /* DMA Channel 1 Interrupt Active. Reading a 1 indicates the DMA Channel 1
									            interrupt is active. */
		uint32 BISTIntActive : 1;      /* BIST Interrupt Active. Reading a 1 indicates the BIST interrupt is active.
									           The BIST (built-in self test) interrupt is asserted by writing a 1 to bit 6 of the
									           PCI Configuration BIST register. Clearing bit 6 clears the interrupt. Refer to
									           the PCIBISTR register for a description of the self test. */

		uint32 DMIsBusMaster : 1;      /* Reading a 0 indicates the Direct Master was the Bus Master during a Master
									            or Target Abort. */
		uint32 Dma0IsBusMaster : 1;    /* Reading a 0 indicates DMA Channel 0 was the Bus Master during a Master
									            or Target Abort. */
		uint32 Dma1IsBusMaster : 1;    /* Reading a 0 indicates DMA Channel 1 was the Bus Master during a Master
									            or Target Abort. */
		uint32 TargeAbortBy9056 : 1;   /* Reading a 0 indicates a Target Abort was asserted by the PCI 9056 after
									            256 consecutive Master retries to a Target. */

		uint32 PciWrToMBOX0 : 1;       /* Reading a 1 indicates the PCI Bus wrote data to MBOX0. Enabled only
									            if the Mailbox Interrupt Enable bit is set (INTCSR[3]=1). */
		uint32 PciWrToMBOX1 : 1;       /* Reading a 1 indicates the PCI Bus wrote data to MBOX1. Enabled only
									            if the Mailbox Interrupt Enable bit is set (INTCSR[3]=1). */
		uint32 PciWrToMBOX2 : 1;       /* Reading a 1 indicates the PCI Bus wrote data to MBOX2. Enabled only
									            if the Mailbox Interrupt Enable bit is set (INTCSR[3]=1). */
		uint32 PciWrToMBOX3 : 1;       /* Reading a 1 indicates the PCI Bus wrote data to MBOX3. Enabled only
									            if the Mailbox Interrupt Enable bit is set (INTCSR[3]=1). */

	};
} PLX_INT_CSR;

#define BR_PLX_INTCSR        0x68   // Low word

//
// DMA Mode   -----------------------------------------
typedef union _PLX_DMA_MODE {
	uint32 Value;
	struct {
		uint32 LocBusWidth : 2;     /* Local Bus Width. Writing a 00 indicates an 8-bit bus width. Writing a 01
								            indicates a 16-bit bus width. Writing a 10 or 11 indicates a 32-bit bus width. */
		uint32 IntlWaitState : 4;   /* Internal Wait States (data-to-data). */
		uint32 TRInputEn : 1;       /* TA#/READY# Input Enable. Writing a 1 enables TA#/READY# input. Writing
								            a 0 disables TA#/READY# input. */
		uint32 BTERMInputEn : 1;    /* BTERM# Input Enable. Writing a 1 enables BTERM# input. Writing a 0 disables
								            BTERM# input. */

		uint32 LocBurstEn : 1;   /* Local Burst Enable. Writing a 1 enables Local bursting. Writing a 0 disables
								         Local bursting. */
		uint32 SGModeEn : 1;     /* Scatter/Gather Mode. Writing a 1 indicates Scatter/Gather mode is enabled.
								         For Scatter/Gather mode, DMA source address, destination address, and byte
								         count are loaded from memory in PCI or Local Address spaces. Writing a 0
								         indicates Block mode is enabled. */
		uint32 DoneIntEn : 1;  /* Done Interrupt Enable. Writing a 1 enables an interrupt when done. Writing a 0
							           disables an interrupt when done. If DMA Clear Count mode is enabled, the
							           interrupt does not occur until the byte count is cleared. */
		uint32 LocAddrMode : 1;  /* Local Addressing Mode. Writing a 1 holds the Local Address bus constant.
								         Writing a 0 indicates the Local Address is incremented. */

		uint32 DemandModeEn : 1; /* Demand Mode. Writing a 1 causes the DMA controller to operate in Demand
								         mode. In Demand mode, the DMA controller transfers data when its DREQ0#
								         input is asserted. Asserts DACK0# to indicate the current Local Bus transfer is
								         in response to DREQ0# input. DMA controller transfers Lwords (32 bits) of data.
								         This may result in multiple transfers for an 8- or 16-bit bus. */
		uint32 MWIModeEn : 1;    /* Memory Write and Invalidate Mode for DMA Transfers. When set to 1, the
								         PCI 9056 performs Memory Write and Invalidate cycles to the PCI Bus. The
								         PCI 9056 supports Memory Write and Invalidate sizes of 8 or 16 Lwords. Size is
								         specified in the System Cache Line Size bits (PCICLSR[7:0]). If a size other than
								         8 or 16 is specified, the PCI 9056 performs Write transfers rather than Memory
								         Write and Invalidate transfers. Transfers must start and end at cache line
								         boundaries. */
		uint32 DmaEOTEn : 1;     /* DMA EOT# Enable. Writing a 1 enables the EOT# input pin. Writing a 0 disables
								         the EOT# input pin. */
		uint32 TermModeSelect : 1;  /* Fast/Slow Terminate Mode Select. Writing a 0 sets PCI 9056 into the Slow
								            Terminate mode. As a result in C or J modes, BLAST# is asserted on the last
								            Data transfer to terminate DMA transfer. As a result in M mode, BDIP# is
								            de-asserted at the nearest 16-byte boundary and stops the DMA transfer.
								            Writing a 1 indicates that if EOT# is asserted or DREQ0# is de-asserted
								            in Demand mode during DMA will immediately terminate the DMA transfer.
								            In M mode, writing a 1 indicates BDIP# output is disabled. As a result, the
								            PCI 9056 DMA transfer terminates immediately when EOT# is asserted
								            or when DREQ0# is de-asserted in Demand mode */

		uint32 DmaClrCountMode : 1;  /* DMA Clear Count Mode. Writing a 1 clears the byte count in each Scatter/
									         Gather descriptor when the corresponding DMA transfer is complete. */
		uint32 DmaIntSelect : 1;    /* DMA Channel x Interrupt Select. Writing a 1 routes the DMA Channel 0
								            interrupt to the PCI Bus interrupt. Writing a 0 routes the DMA Channel 0 interrupt
								            to the Local Bus interrupt. */
		uint32 DacChainLoad : 1;    /* DAC Chain Load. When set to 1, enables the descriptor to load the PCI Dual
								            Address Cycle value. Otherwise, it uses the register contents. */
		uint32 EotEndLink : 1;      /* EOT# End Link. Used only for Scatter/Gather DMA transfers. When EOT# is
								            asserted, value of 1 indicates the DMA transfer ends the current Scatter/Gather
								            link and continues with the remaining Scatter/Gather transfers. When EOT# is
								            asserted, value of 0 indicates the DMA transfer ends the current Scatter/Gather
								            transfer and does not continue with the remaining Scatter/Gather transfers. */

		uint32 ValidModeEn : 1;     /* Valid Mode Enable. Value of 0 indicates the Valid bit (DMASIZ0[31]) is ignored.
								            Value of 1 indicates the DMA descriptors are processed only when the Valid bit
								            is set (DMASIZ0[31]). If the Valid bit is set, the transfer count is 0, and the
								            descriptor is not the last descriptor in the chain. The DMA controller then moves
								            to the next descriptor in the chain. */
		uint32 ValidStopCtl : 1;    /* Valid Stop Control. Value of 0 indicates the DMA Chaining controller
								            continuously polls a descriptor with the Valid bit set to 0 (invalid descriptor) if
								            the Valid Mode Enable bit is set (bit [20]=1). Value of 1 indicates the Chaining
								            controller stops polling when the Valid bit with a value of 0 is detected
								            (DMASIZ0[31]=0). In this case, the CPU must restart the DMA controller by setting
								            the Start bit (DMACSR0[1]=1). A pause sets the DMA Done bit (DMACSR0[4]). */
		uint32 Reserved : 10;
	};
} PLX_DMA_MODE;

#define BR_PLX_DMAMODE0   0x80   // (DMAMODE0; PCI:80h, LOC:100h) DMA Channel 0 Mode
#define BR_PLX_DMAMODE1   0x94   // (DMAMODE1; PCI:94h, LOC:114h) DMA Channel 1 Mode

#define BR_PLX_DMAPADR0   0x84   // (DMAPADR0; PCI:84h, LOC:104h) DMA Channel 0 PCI Address
#define BR_PLX_DMAPADR1   0x98   // (DMAPADR1; PCI:98h, LOC:118h) DMA Channel 1 PCI Address

#define BR_PLX_DMALADR0   0x88   // (DMALADR0; PCI:88h, LOC:108h) DMA Channel 0 Local Address
#define BR_PLX_DMALADR1   0x9C   // (DMALADR1; PCI:9Ch, LOC:11Ch) DMA Channel 1 Local Address

#define BR_PLX_DMASIZ0    0x8C   // (DMASIZ0; PCI:8Ch, LOC:10Ch) DMA Channel 0 Transfer Size (bytes)
#define BR_PLX_DMASIZ1    0xA0   // (DMASIZ1; PCI:A0h, LOC:120h) DMA Channel 1 Transfer Size (bytes)


//
// DMA Descriptor Pointer -----------------------------------------
// format of DMA Descriptor
typedef union _PLX_DMA_DPR {
	uint32  Value;
	struct {
		uint32 NextDPRLoc : 1;  /* Descriptor Location. Writing a 1 indicates PCI Address space. Writing a 0
							            indicates Local Address space. */
		uint32 EndOfChain  : 1;   /* End of Chain. Writing a 1 indicates end of chain. Writing a 0 indicates not
								         end of chain descriptor. (Same as Block mode.) */
		uint32 IntrAfterTC : 1;  /* Interrupt after Terminal Count. Writing a 1 causes an interrupt to be
								         asserted after the terminal count for this descriptor is reached. Writing a 0
								         disables interrupts from being asserted. */
		uint32 TransferDir : 1;  /* Direction of Transfer. Writing a 1 indicates transfers from the Local Bus to
								         the PCI Bus. Writing a 0 indicates transfers from the PCI Bus to the Local Bus. */
		uint32  NextDPRAddr : 28; /* Next Descriptor Address. Qword-aligned (bits [3:0]=0000). */
	};
} PLX_DMA_DPR;

#define BR_PLX_DMADPR0    0x90    // (DMADPR0; PCI:90h, LOC:110h) DMA Channel 0 Descriptor Pointer
#define BR_PLX_DMADPR1    0xA4    // (DMADPR1; PCI:A4h, LOC:124h) DMA Channel 1 Descriptor Pointer


//
// DMA Channel Command/Status -----------------------------------------
// format of DMA Descriptor
typedef union _PLX_DMA_CSR {
	uint8 Value;
	struct {
		uint8 Enable : 1;   /* Channel x Enable. Writing a 1 enables channel to transfer data. Writing a 0
						         disables the channel x from starting a DMA transfer, and if in the process of
						         transferring data, suspends the transfer (pause). */
		uint8 Start  : 1;  /* Channel x Start. Writing a 1 causes the channel to start transferring data
						         if the channel x is enabled. */
		uint8 Abort : 1;   /* Channel x Abort. Writing a 1 causes the channel to abort current transfer.
					           Channel x Enable bit must be cleared (bit [0]=0). Sets Channel x Done
					           (bit [4]=1) when abort is complete. */
		uint8 ClearInt : 1; /* Channel x Clear Interrupt. Writing a 1 clears Channel x interrupts. */
		uint8 Done     : 1; /* Channel x Done. Reading a 1 indicates a channel transfer is complete.
						         Reading a 0 indicates a channel transfer is not complete. */
		uint8 Reserved : 3;
	};
} PLX_DMA_CSR;

#define BR_PLX_DMACSR0     0xA8 // (DMACSR0; PCI:A8h, LOC:128h) DMA Channel 0 Command/Status
#define BR_PLX_DMACSR1     0xA9 // (DMACSR1; PCI:A9h, LOC:129h) DMA Channel 1 Command/Status

//
// DMA Threshold -------------------------------------------------------------
// format of DMA Threshold
typedef union _PLX_DMA_THR {
	uint32 Value;
	struct {
		uint32 Dma0PLAF : 4;  /* DMA Channel 0 PCI-to-Local Almost Full (C0PLAF). Number of full entries
							       (divided by two, minus one) in the FIFO before requesting the Local Bus for writes.
							       (C0PLAF+1) + (C0PLAE+1) should be <= a FIFO Depth of 32. */
		uint32 Dma0LPAE : 4;  /* DMA Channel 0 Local-to-PCI Almost Empty (C0LPAE). Number of empty
							          entries (divided by two, minus one) in the FIFO before requesting the Local
							          Bus for reads. (C0LPAF+1) + (C0LPAE+1) should be <= a FIFO depth of 32. */
		uint32 Dma0LPAF : 4;  /* DMA Channel 0 Local-to-PCI Almost Full (C0LPAF). Number of full entries
							         (divided by two, minus one) in the FIFO before requesting the PCI Bus for writes. */
		uint32 Dma0PLAE : 4;  /* DMA Channel 0 PCI-to-Local Almost Empty (C0PLAE). Number of empty
							          entries (divided by two, minus one) in the FIFO before requesting the PCI Bus
							          for reads. */

		uint32 Dma1PLAF : 4;  /* DMA Channel 1 PCI-to-Local Almost Full (C0PLAF). Number of full entries
							          (divided by two, minus one) in the FIFO before requesting the Local Bus for writes.
							          (C0PLAF+1) + (C0PLAE+1) should be <= a FIFO Depth of 32. */
		uint32 Dma1LPAE : 4;  /* DMA Channel 1 Local-to-PCI Almost Empty (C0LPAE). Number of empty
							          entries (divided by two, minus one) in the FIFO before requesting the Local
							          Bus for reads. (C0LPAF+1) + (C0LPAE+1) should be <= a FIFO depth of 32. */
		uint32 Dma1LPAF : 4;  /* DMA Channel 1 Local-to-PCI Almost Full (C0LPAF). Number of full entries
							         (divided by two, minus one) in the FIFO before requesting the PCI Bus for writes. */
		uint32 Dma1PLAE : 4;  /* DMA Channel 1 PCI-to-Local Almost Empty (C0PLAE). Number of empty
							          entries (divided by two, minus one) in the FIFO before requesting the PCI Bus
							          for reads. */
	};
} PLX_DMA_THR;

#define BR_PLX_DMATHR     0xB0  // (DMATHR; PCI:B0h, LOC:130h) DMA Threshold

#define BR_PLX_DMADAC0    0xB4  // (DMADAC0; PCI:B4h, LOC:134h) DMA Channel 0 PCI Dual Address Cycle Address 
#define BR_PLX_DMADAC1    0xB8  // (DMADAC1; PCI:B8h, LOC:138h) DMA Channel 1 PCI Dual Address Cycle Address 

#endif // _PLX905x_REGISTER_DEFINE
