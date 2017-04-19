#ifndef __CS4624_H__
#define __CS4624_H__


#include "register.h"


#define DSP_MAX_PCM_CHANNELS 32
#define DSP_MAX_SRC_NR       14

/*
 *  constants
 */

#define CS46XX_BA0_SIZE		  0x1000
#define CS46XX_BA1_DATA0_SIZE 0x3000
#define CS46XX_BA1_DATA1_SIZE 0x3800
#define CS46XX_BA1_PRG_SIZE	  0x7000
#define CS46XX_BA1_REG_SIZE	  0x0100

#define DSP_PCM_MAIN_CHANNEL        1
#define DSP_PCM_REAR_CHANNEL        2
#define DSP_PCM_CENTER_LFE_CHANNEL  3
#define DSP_PCM_S71_CHANNEL         4 /* surround 7.1 */
#define DSP_IEC958_CHANNEL          5

/*
 * dma缓存流
 */
struct mychip_dma_stream{
	struct snd_dma_buffer hw_buf;
	unsigned int ctl;
	unsigned int shift;	/* Shift count to trasform frames in bytes */
	struct snd_pcm_indirect pcm_rec;
	struct snd_pcm_substream *substream;

	struct dsp_pcm_channel_descriptor * pcm_channel;
	int pcm_channel_id;    /* Fron Rear, Center Lfe  ... */
};

// IO区域
struct snd_mychip_region{
	char name[24];
	unsigned long base;
	void __iomem *remap_addr;   //虚拟地址，系统可访问  
	unsigned long size;
	struct resource *resource;
};

struct snd_mychip{
	int irq;

	struct snd_card *card;
	struct snd_pcm *pcm;
	struct pci_dev *pci;

	int nr_ac97_codecs;
	struct snd_ac97_bus *ac97_bus;
	struct snd_ac97 *ac97[MAX_NR_AC97];

	unsigned long ba0_addr;
	unsigned long ba1_addr;

	union {
		struct {
			struct snd_mychip_region ba0;
		} name;
		struct snd_mychip_region idx[1];     // 用name和idx访问是等价的
	} ba0_region;                            // ba0_addr映射的region

	union {
		struct {
			struct snd_mychip_region data0;
			struct snd_mychip_region data1;
			struct snd_mychip_region pmem;
			struct snd_mychip_region reg;
		} name;
		struct snd_mychip_region idx[4];     // 用name和idx访问是等价的
	} ba1_region;							 // ba1_addr映射的region

	//等价于声明
	// union {
	// 	struct {
	// 		struct snd_mychip_region ba0;
	// 		struct snd_mychip_region data0;
	// 		struct snd_mychip_region data1;
	// 		struct snd_mychip_region pmem;
	// 		struct snd_mychip_region reg;
	// 	} name;
	// 	struct snd_mychip_region idx[5];     // 用name和idx访问是等价的
	// } region;

	//struct snd_rawmidi *rmidi;
	//struct snd_rawmidi_substream *midi_input;
	//struct snd_rawmidi_substream *midi_output;

	struct mychip_dma_stream *capt, *play;
	unsigned int play_ctl, capt_ctl;  //和*capt,*play里面的ctl一个意思，不过在初始化阶段还没有给capt,play指针分配空间，所以用局部变量暂时存储信息,start_dsp和trigger中用到
	spinlock_t reg_lock;
};

#endif
