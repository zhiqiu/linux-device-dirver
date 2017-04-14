#ifndef __CS4624_H__
#define __CS4624_H__



/*
 *  Direct registers
 */

#define CS4281_BA0_SIZE		0x1000
#define CS4281_BA1_SIZE		0x10000

/*
 *  BA0 registers
 */
#define BA0_HISR		0x0000	/* Host Interrupt Status Register */
#define BA0_HISR_INTENA		(1<<31)	/* Internal Interrupt Enable Bit */
#define BA0_HISR_MIDI		(1<<22)	/* MIDI port interrupt */
#define BA0_HISR_FIFOI		(1<<20)	/* FIFO polled interrupt */
#define BA0_HISR_DMAI		(1<<18)	/* DMA interrupt (half or end) */
#define BA0_HISR_FIFO(c)	(1<<(12+(c))) /* FIFO channel interrupt */
#define BA0_HISR_DMA(c)		(1<<(8+(c)))  /* DMA channel interrupt */
#define BA0_HISR_GPPI		(1<<5)	/* General Purpose Input (Primary chip) */
#define BA0_HISR_GPSI		(1<<4)	/* General Purpose Input (Secondary chip) */
#define BA0_HISR_GP3I		(1<<3)	/* GPIO3 pin Interrupt */
#define BA0_HISR_GP1I		(1<<2)	/* GPIO1 pin Interrupt */
#define BA0_HISR_VUPI		(1<<1)	/* VOLUP pin Interrupt */
#define BA0_HISR_VDNI		(1<<0)	/* VOLDN pin Interrupt */

#define BA0_HICR		0x0008	/* Host Interrupt Control Register */
#define BA0_HICR_CHGM		(1<<1)	/* INTENA Change Mask */
#define BA0_HICR_IEV		(1<<0)	/* INTENA Value */
#define BA0_HICR_EOI		(3<<0)	/* End of Interrupt command */

#define BA0_HIMR		0x000c	/* Host Interrupt Mask Register */
					/* Use same contants as for BA0_HISR */

#define BA0_IIER		0x0010	/* ISA Interrupt Enable Register */

#define BA0_HDSR0		0x00f0	/* Host DMA Engine 0 Status Register */
#define BA0_HDSR1		0x00f4	/* Host DMA Engine 1 Status Register */
#define BA0_HDSR2		0x00f8	/* Host DMA Engine 2 Status Register */
#define BA0_HDSR3		0x00fc	/* Host DMA Engine 3 Status Register */

#define BA0_HDSR_CH1P		(1<<25)	/* Channel 1 Pending */
#define BA0_HDSR_CH2P		(1<<24)	/* Channel 2 Pending */
#define BA0_HDSR_DHTC		(1<<17)	/* DMA Half Terminal Count */
#define BA0_HDSR_DTC		(1<<16)	/* DMA Terminal Count */
#define BA0_HDSR_DRUN		(1<<15)	/* DMA Running */
#define BA0_HDSR_RQ		(1<<7)	/* Pending Request */

#define BA0_DCA0		0x0110	/* Host DMA Engine 0 Current Address */
#define BA0_DCC0		0x0114	/* Host DMA Engine 0 Current Count */
#define BA0_DBA0		0x0118	/* Host DMA Engine 0 Base Address */
#define BA0_DBC0		0x011c	/* Host DMA Engine 0 Base Count */
#define BA0_DCA1		0x0120	/* Host DMA Engine 1 Current Address */
#define BA0_DCC1		0x0124	/* Host DMA Engine 1 Current Count */
#define BA0_DBA1		0x0128	/* Host DMA Engine 1 Base Address */
#define BA0_DBC1		0x012c	/* Host DMA Engine 1 Base Count */
#define BA0_DCA2		0x0130	/* Host DMA Engine 2 Current Address */
#define BA0_DCC2		0x0134	/* Host DMA Engine 2 Current Count */
#define BA0_DBA2		0x0138	/* Host DMA Engine 2 Base Address */
#define BA0_DBC2		0x013c	/* Host DMA Engine 2 Base Count */
#define BA0_DCA3		0x0140	/* Host DMA Engine 3 Current Address */
#define BA0_DCC3		0x0144	/* Host DMA Engine 3 Current Count */
#define BA0_DBA3		0x0148	/* Host DMA Engine 3 Base Address */
#define BA0_DBC3		0x014c	/* Host DMA Engine 3 Base Count */
#define BA0_DMR0		0x0150	/* Host DMA Engine 0 Mode */
#define BA0_DCR0		0x0154	/* Host DMA Engine 0 Command */
#define BA0_DMR1		0x0158	/* Host DMA Engine 1 Mode */
#define BA0_DCR1		0x015c	/* Host DMA Engine 1 Command */
#define BA0_DMR2		0x0160	/* Host DMA Engine 2 Mode */
#define BA0_DCR2		0x0164	/* Host DMA Engine 2 Command */
#define BA0_DMR3		0x0168	/* Host DMA Engine 3 Mode */
#define BA0_DCR3		0x016c	/* Host DMA Engine 3 Command */

#define BA0_DMR_DMA		(1<<29)	/* Enable DMA mode */
#define BA0_DMR_POLL		(1<<28)	/* Enable poll mode */
#define BA0_DMR_TBC		(1<<25)	/* Transfer By Channel */
#define BA0_DMR_CBC		(1<<24)	/* Count By Channel (0 = frame resolution) */
#define BA0_DMR_SWAPC		(1<<22)	/* Swap Left/Right Channels */
#define BA0_DMR_SIZE20		(1<<20)	/* Sample is 20-bit */
#define BA0_DMR_USIGN		(1<<19)	/* Unsigned */
#define BA0_DMR_BEND		(1<<18)	/* Big Endian */
#define BA0_DMR_MONO		(1<<17)	/* Mono */
#define BA0_DMR_SIZE8		(1<<16)	/* Sample is 8-bit */
#define BA0_DMR_TYPE_DEMAND	(0<<6)
#define BA0_DMR_TYPE_SINGLE	(1<<6)
#define BA0_DMR_TYPE_BLOCK	(2<<6)
#define BA0_DMR_TYPE_CASCADE	(3<<6)	/* Not supported */
#define BA0_DMR_DEC		(1<<5)	/* Access Increment (0) or Decrement (1) */
#define BA0_DMR_AUTO		(1<<4)	/* Auto-Initialize */
#define BA0_DMR_TR_VERIFY	(0<<2)	/* Verify Transfer */
#define BA0_DMR_TR_WRITE	(1<<2)	/* Write Transfer */
#define BA0_DMR_TR_READ		(2<<2)	/* Read Transfer */

#define BA0_DCR_HTCIE		(1<<17)	/* Half Terminal Count Interrupt */
#define BA0_DCR_TCIE		(1<<16)	/* Terminal Count Interrupt */
#define BA0_DCR_MSK		(1<<0)	/* DMA Mask bit */

#define BA0_FCR0		0x0180	/* FIFO Control 0 */
#define BA0_FCR1		0x0184	/* FIFO Control 1 */
#define BA0_FCR2		0x0188	/* FIFO Control 2 */
#define BA0_FCR3		0x018c	/* FIFO Control 3 */

#define BA0_FCR_FEN		(1<<31)	/* FIFO Enable bit */
#define BA0_FCR_DACZ		(1<<30)	/* DAC Zero */
#define BA0_FCR_PSH		(1<<29)	/* Previous Sample Hold */
#define BA0_FCR_RS(x)		(((x)&0x1f)<<24) /* Right Slot Mapping */
#define BA0_FCR_LS(x)		(((x)&0x1f)<<16) /* Left Slot Mapping */
#define BA0_FCR_SZ(x)		(((x)&0x7f)<<8)	/* FIFO buffer size (in samples) */
#define BA0_FCR_OF(x)		(((x)&0x7f)<<0)	/* FIFO starting offset (in samples) */

#define BA0_FPDR0		0x0190	/* FIFO Polled Data 0 */
#define BA0_FPDR1		0x0194	/* FIFO Polled Data 1 */
#define BA0_FPDR2		0x0198	/* FIFO Polled Data 2 */
#define BA0_FPDR3		0x019c	/* FIFO Polled Data 3 */

#define BA0_FCHS		0x020c	/* FIFO Channel Status */
#define BA0_FCHS_RCO(x)		(1<<(7+(((x)&3)<<3))) /* Right Channel Out */
#define BA0_FCHS_LCO(x)		(1<<(6+(((x)&3)<<3))) /* Left Channel Out */
#define BA0_FCHS_MRP(x)		(1<<(5+(((x)&3)<<3))) /* Move Read Pointer */
#define BA0_FCHS_FE(x)		(1<<(4+(((x)&3)<<3))) /* FIFO Empty */
#define BA0_FCHS_FF(x)		(1<<(3+(((x)&3)<<3))) /* FIFO Full */
#define BA0_FCHS_IOR(x)		(1<<(2+(((x)&3)<<3))) /* Internal Overrun Flag */
#define BA0_FCHS_RCI(x)		(1<<(1+(((x)&3)<<3))) /* Right Channel In */
#define BA0_FCHS_LCI(x)		(1<<(0+(((x)&3)<<3))) /* Left Channel In */

#define BA0_FSIC0		0x0210	/* FIFO Status and Interrupt Control 0 */
#define BA0_FSIC1		0x0214	/* FIFO Status and Interrupt Control 1 */
#define BA0_FSIC2		0x0218	/* FIFO Status and Interrupt Control 2 */
#define BA0_FSIC3		0x021c	/* FIFO Status and Interrupt Control 3 */

#define BA0_FSIC_FIC(x)		(((x)&0x7f)<<24) /* FIFO Interrupt Count */
#define BA0_FSIC_FORIE		(1<<23) /* FIFO OverRun Interrupt Enable */
#define BA0_FSIC_FURIE		(1<<22) /* FIFO UnderRun Interrupt Enable */
#define BA0_FSIC_FSCIE		(1<<16)	/* FIFO Sample Count Interrupt Enable */
#define BA0_FSIC_FSC(x)		(((x)&0x7f)<<8) /* FIFO Sample Count */
#define BA0_FSIC_FOR		(1<<7)	/* FIFO OverRun */
#define BA0_FSIC_FUR		(1<<6)	/* FIFO UnderRun */
#define BA0_FSIC_FSCR		(1<<0)	/* FIFO Sample Count Reached */

#define BA0_PMCS		0x0344	/* Power Management Control/Status */
#define BA0_CWPR		0x03e0	/* Configuration Write Protect */

#define BA0_EPPMC		0x03e4	/* Extended PCI Power Management Control */
#define BA0_EPPMC_FPDN		(1<<14) /* Full Power DowN */

#define BA0_GPIOR		0x03e8	/* GPIO Pin Interface Register */

#define BA0_SPMC		0x03ec	/* Serial Port Power Management Control (& ASDIN2 enable) */
#define BA0_SPMC_GIPPEN		(1<<15)	/* GP INT Primary PME# Enable */
#define BA0_SPMC_GISPEN		(1<<14)	/* GP INT Secondary PME# Enable */
#define BA0_SPMC_EESPD		(1<<9)	/* EEPROM Serial Port Disable */
#define BA0_SPMC_ASDI2E		(1<<8)	/* ASDIN2 Enable */
#define BA0_SPMC_ASDO		(1<<7)	/* Asynchronous ASDOUT Assertion */
#define BA0_SPMC_WUP2		(1<<3)	/* Wakeup for Secondary Input */
#define BA0_SPMC_WUP1		(1<<2)	/* Wakeup for Primary Input */
#define BA0_SPMC_ASYNC		(1<<1)	/* Asynchronous ASYNC Assertion */
#define BA0_SPMC_RSTN		(1<<0)	/* Reset Not! */

#define BA0_CFLR		0x03f0	/* Configuration Load Register (EEPROM or BIOS) */
#define BA0_CFLR_DEFAULT	0x00000001 /* CFLR must be in AC97 link mode */
#define BA0_IISR		0x03f4	/* ISA Interrupt Select */
#define BA0_TMS			0x03f8	/* Test Register */
#define BA0_SSVID		0x03fc	/* Subsystem ID register */

#define BA0_CLKCR1		0x0400	/* Clock Control Register 1 */
#define BA0_CLKCR1_CLKON	(1<<25)	/* Read Only */
#define BA0_CLKCR1_DLLRDY	(1<<24)	/* DLL Ready */
#define BA0_CLKCR1_DLLOS	(1<<6)	/* DLL Output Select */
#define BA0_CLKCR1_SWCE		(1<<5)	/* Clock Enable */
#define BA0_CLKCR1_DLLP		(1<<4)	/* DLL PowerUp */
#define BA0_CLKCR1_DLLSS	(((x)&3)<<3) /* DLL Source Select */

#define BA0_FRR			0x0410	/* Feature Reporting Register */
#define BA0_SLT12O		0x041c	/* Slot 12 GPIO Output Register for AC-Link */

#define BA0_SERMC		0x0420	/* Serial Port Master Control */
#define BA0_SERMC_FCRN		(1<<27)	/* Force Codec Ready Not */
#define BA0_SERMC_ODSEN2	(1<<25)	/* On-Demand Support Enable ASDIN2 */
#define BA0_SERMC_ODSEN1	(1<<24)	/* On-Demand Support Enable ASDIN1 */
#define BA0_SERMC_SXLB		(1<<21)	/* ASDIN2 to ASDOUT Loopback */
#define BA0_SERMC_SLB		(1<<20)	/* ASDOUT to ASDIN2 Loopback */
#define BA0_SERMC_LOVF		(1<<19)	/* Loopback Output Valid Frame bit */
#define BA0_SERMC_TCID(x)	(((x)&3)<<16) /* Target Secondary Codec ID */
#define BA0_SERMC_PXLB		(5<<1)	/* Primary Port External Loopback */
#define BA0_SERMC_PLB		(4<<1)	/* Primary Port Internal Loopback */
#define BA0_SERMC_PTC		(7<<1)	/* Port Timing Configuration */
#define BA0_SERMC_PTC_AC97	(1<<1)	/* AC97 mode */
#define BA0_SERMC_MSPE		(1<<0)	/* Master Serial Port Enable */

#define BA0_SERC1		0x0428	/* Serial Port Configuration 1 */
#define BA0_SERC1_SO1F(x)	(((x)&7)>>1) /* Primary Output Port Format */
#define BA0_SERC1_AC97		(1<<1)
#define BA0_SERC1_SO1EN		(1<<0)	/* Primary Output Port Enable */

#define BA0_SERC2		0x042c	/* Serial Port Configuration 2 */
#define BA0_SERC2_SI1F(x)	(((x)&7)>>1) /* Primary Input Port Format */
#define BA0_SERC2_AC97		(1<<1)
#define BA0_SERC2_SI1EN		(1<<0)	/* Primary Input Port Enable */

#define BA0_SLT12M		0x045c	/* Slot 12 Monitor Register for Primary AC-Link */

#define BA0_ACCTL		0x0460	/* AC'97 Control */
#define BA0_ACCTL_TC		(1<<6)	/* Target Codec */
#define BA0_ACCTL_CRW		(1<<4)	/* 0=Write, 1=Read Command */
#define BA0_ACCTL_DCV		(1<<3)	/* Dynamic Command Valid */
#define BA0_ACCTL_VFRM		(1<<2)	/* Valid Frame */
#define BA0_ACCTL_ESYN		(1<<1)	/* Enable Sync */

#define BA0_ACSTS		0x0464	/* AC'97 Status */
#define BA0_ACSTS_VSTS		(1<<1)	/* Valid Status */
#define BA0_ACSTS_CRDY		(1<<0)	/* Codec Ready */

#define BA0_ACOSV		0x0468	/* AC'97 Output Slot Valid */
#define BA0_ACOSV_SLV(x)	(1<<((x)-3))

#define BA0_ACCAD		0x046c	/* AC'97 Command Address */
#define BA0_ACCDA		0x0470	/* AC'97 Command Data */

#define BA0_ACISV		0x0474	/* AC'97 Input Slot Valid */
#define BA0_ACISV_SLV(x)	(1<<((x)-3))

#define BA0_ACSAD		0x0478	/* AC'97 Status Address */
#define BA0_ACSDA		0x047c	/* AC'97 Status Data */
#define BA0_JSPT		0x0480	/* Joystick poll/trigger */
#define BA0_JSCTL		0x0484	/* Joystick control */
#define BA0_JSC1		0x0488	/* Joystick control */
#define BA0_JSC2		0x048c	/* Joystick control */
#define BA0_JSIO		0x04a0

#define BA0_MIDCR		0x0490	/* MIDI Control */
#define BA0_MIDCR_MRST		(1<<5)	/* Reset MIDI Interface */
#define BA0_MIDCR_MLB		(1<<4)	/* MIDI Loop Back Enable */
#define BA0_MIDCR_TIE		(1<<3)	/* MIDI Transmuit Interrupt Enable */
#define BA0_MIDCR_RIE		(1<<2)	/* MIDI Receive Interrupt Enable */
#define BA0_MIDCR_RXE		(1<<1)	/* MIDI Receive Enable */
#define BA0_MIDCR_TXE		(1<<0)	/* MIDI Transmit Enable */

#define BA0_MIDCMD		0x0494	/* MIDI Command (wo) */

#define BA0_MIDSR		0x0494	/* MIDI Status (ro) */
#define BA0_MIDSR_RDA		(1<<15)	/* Sticky bit (RBE 1->0) */
#define BA0_MIDSR_TBE		(1<<14) /* Sticky bit (TBF 0->1) */
#define BA0_MIDSR_RBE		(1<<7)	/* Receive Buffer Empty */
#define BA0_MIDSR_TBF		(1<<6)	/* Transmit Buffer Full */

#define BA0_MIDWP		0x0498	/* MIDI Write */
#define BA0_MIDRP		0x049c	/* MIDI Read (ro) */

#define BA0_AODSD1		0x04a8	/* AC'97 On-Demand Slot Disable for primary link (ro) */
#define BA0_AODSD1_NDS(x)	(1<<((x)-3))

#define BA0_AODSD2		0x04ac	/* AC'97 On-Demand Slot Disable for secondary link (ro) */
#define BA0_AODSD2_NDS(x)	(1<<((x)-3))

#define BA0_CFGI		0x04b0	/* Configure Interface (EEPROM interface) */
#define BA0_SLT12M2		0x04dc	/* Slot 12 Monitor Register 2 for secondary AC-link */
#define BA0_ACSTS2		0x04e4	/* AC'97 Status Register 2 */
#define BA0_ACISV2		0x04f4	/* AC'97 Input Slot Valid Register 2 */
#define BA0_ACSAD2		0x04f8	/* AC'97 Status Address Register 2 */
#define BA0_ACSDA2		0x04fc	/* AC'97 Status Data Register 2 */
#define BA0_FMSR		0x0730	/* FM Synthesis Status (ro) */
#define BA0_B0AP		0x0730	/* FM Bank 0 Address Port (wo) */
#define BA0_FMDP		0x0734	/* FM Data Port */
#define BA0_B1AP		0x0738	/* FM Bank 1 Address Port */
#define BA0_B1DP		0x073c	/* FM Bank 1 Data Port */

#define BA0_SSPM		0x0740	/* Sound System Power Management */
#define BA0_SSPM_MIXEN		(1<<6)	/* Playback SRC + FM/Wavetable MIX */
#define BA0_SSPM_CSRCEN		(1<<5)	/* Capture Sample Rate Converter Enable */
#define BA0_SSPM_PSRCEN		(1<<4)	/* Playback Sample Rate Converter Enable */
#define BA0_SSPM_JSEN		(1<<3)	/* Joystick Enable */
#define BA0_SSPM_ACLEN		(1<<2)	/* Serial Port Engine and AC-Link Enable */
#define BA0_SSPM_FMEN		(1<<1)	/* FM Synthesis Block Enable */

#define BA0_DACSR		0x0744	/* DAC Sample Rate - Playback SRC */
#define BA0_ADCSR		0x0748	/* ADC Sample Rate - Capture SRC */

#define BA0_SSCR		0x074c	/* Sound System Control Register */
#define BA0_SSCR_HVS1		(1<<23)	/* Hardwave Volume Step (0=1,1=2) */
#define BA0_SSCR_MVCS		(1<<19)	/* Master Volume Codec Select */
#define BA0_SSCR_MVLD		(1<<18)	/* Master Volume Line Out Disable */
#define BA0_SSCR_MVAD		(1<<17)	/* Master Volume Alternate Out Disable */
#define BA0_SSCR_MVMD		(1<<16)	/* Master Volume Mono Out Disable */
#define BA0_SSCR_XLPSRC		(1<<8)	/* External SRC Loopback Mode */
#define BA0_SSCR_LPSRC		(1<<7)	/* SRC Loopback Mode */
#define BA0_SSCR_CDTX		(1<<5)	/* CD Transfer Data */
#define BA0_SSCR_HVC		(1<<3)	/* Harware Volume Control Enable */

#define BA0_FMLVC		0x0754	/* FM Synthesis Left Volume Control */
#define BA0_FMRVC		0x0758	/* FM Synthesis Right Volume Control */
#define BA0_SRCSA		0x075c	/* SRC Slot Assignments */
#define BA0_PPLVC		0x0760	/* PCM Playback Left Volume Control */
#define BA0_PPRVC		0x0764	/* PCM Playback Right Volume Control */
#define BA0_PASR		0x0768	/* playback sample rate */
#define BA0_CASR		0x076C	/* capture sample rate */

/* Source Slot Numbers - Playback */
#define SRCSLOT_LEFT_PCM_PLAYBACK		0
#define SRCSLOT_RIGHT_PCM_PLAYBACK		1
#define SRCSLOT_PHONE_LINE_1_DAC		2
#define SRCSLOT_CENTER_PCM_PLAYBACK		3
#define SRCSLOT_LEFT_SURROUND_PCM_PLAYBACK	4
#define SRCSLOT_RIGHT_SURROUND_PCM_PLAYBACK	5
#define SRCSLOT_LFE_PCM_PLAYBACK		6
#define SRCSLOT_PHONE_LINE_2_DAC		7
#define SRCSLOT_HEADSET_DAC			8
#define SRCSLOT_LEFT_WT				29  /* invalid for BA0_SRCSA */
#define SRCSLOT_RIGHT_WT			30  /* invalid for BA0_SRCSA */

/* Source Slot Numbers - Capture */
#define SRCSLOT_LEFT_PCM_RECORD			10
#define SRCSLOT_RIGHT_PCM_RECORD		11
#define SRCSLOT_PHONE_LINE_1_ADC		12
#define SRCSLOT_MIC_ADC				13
#define SRCSLOT_PHONE_LINE_2_ADC		17
#define SRCSLOT_HEADSET_ADC			18
#define SRCSLOT_SECONDARY_LEFT_PCM_RECORD	20
#define SRCSLOT_SECONDARY_RIGHT_PCM_RECORD	21
#define SRCSLOT_SECONDARY_PHONE_LINE_1_ADC	22
#define SRCSLOT_SECONDARY_MIC_ADC		23
#define SRCSLOT_SECONDARY_PHONE_LINE_2_ADC	27
#define SRCSLOT_SECONDARY_HEADSET_ADC		28

/* Source Slot Numbers - Others */
#define SRCSLOT_POWER_DOWN			31

/* MIDI modes */
#define CS4281_MODE_OUTPUT		(1<<0)
#define CS4281_MODE_INPUT		(1<<1)

/* joystick bits */
/* Bits for JSPT */
#define JSPT_CAX                                0x00000001
#define JSPT_CAY                                0x00000002
#define JSPT_CBX                                0x00000004
#define JSPT_CBY                                0x00000008
#define JSPT_BA1                                0x00000010
#define JSPT_BA2                                0x00000020
#define JSPT_BB1                                0x00000040
#define JSPT_BB2                                0x00000080

/* Bits for JSCTL */
#define JSCTL_SP_MASK                           0x00000003
#define JSCTL_SP_SLOW                           0x00000000
#define JSCTL_SP_MEDIUM_SLOW                    0x00000001
#define JSCTL_SP_MEDIUM_FAST                    0x00000002
#define JSCTL_SP_FAST                           0x00000003
#define JSCTL_ARE                               0x00000004

/* Data register pairs masks */
#define JSC1_Y1V_MASK                           0x0000FFFF
#define JSC1_X1V_MASK                           0xFFFF0000
#define JSC1_Y1V_SHIFT                          0
#define JSC1_X1V_SHIFT                          16
#define JSC2_Y2V_MASK                           0x0000FFFF
#define JSC2_X2V_MASK                           0xFFFF0000
#define JSC2_Y2V_SHIFT                          0
#define JSC2_X2V_SHIFT                          16

/* JS GPIO */
#define JSIO_DAX                                0x00000001
#define JSIO_DAY                                0x00000002
#define JSIO_DBX                                0x00000004
#define JSIO_DBY                                0x00000008
#define JSIO_AXOE                               0x00000010
#define JSIO_AYOE                               0x00000020
#define JSIO_BXOE                               0x00000040
#define JSIO_BYOE                               0x00000080

/*
 *
 */
struct mychip_dma_stream{
	struct snd_pcm_substream *substream;
	unsigned int regDBA;		/* offset to DBA register */
	unsigned int regDCA;		/* offset to DCA register */
	unsigned int regDBC;		/* offset to DBC register */
	unsigned int regDCC;		/* offset to DCC register */
	unsigned int regDMR;		/* offset to DMR register */
	unsigned int regDCR;		/* offset to DCR register */
	unsigned int regHDSR;		/* offset to HDSR register */
	unsigned int regFCR;		/* offset to FCR register */
	unsigned int regFSIC;		/* offset to FSIC register */
	unsigned int valDMR;		/* DMA mode */
	unsigned int valDCR;		/* DMA command */
	unsigned int valFCR;		/* FIFO control */
	unsigned int fifo_offset;	/* FIFO offset within BA1 */
	unsigned char left_slot;	/* FIFO left slot */
	unsigned char right_slot;	/* FIFO right slot */
	int frag;
};

struct mychip{
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct pci_dev *pci;

	struct snd_ac97 *ac97;
	struct snd_ac97_bus *ac97_bus;

	unsigned long port;
	int irq;

	void __iomem *ba0;		/* virtual (accessible) address */
	void __iomem *ba1;		/* virtual (accessible) address */
	unsigned long ba0_addr;
	unsigned long ba1_addr;

	struct mychip_dma_stream dma[4];

	unsigned char src_left_play_slot;
	unsigned char src_right_play_slot;
	unsigned char src_left_rec_slot;
	unsigned char src_right_rec_slot;

	unsigned int spurious_dhtc_irq;
	unsigned int spurious_dtc_irq;
	spinlock_t reg_lock;
};

#endif