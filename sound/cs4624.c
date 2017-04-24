//ALSA声卡驱动
//参考http://blog.csdn.net/droidphone

#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm-indirect.h>
#include <sound/pcm_params.h>
#include <sound/ac97_codec.h>
#include <linux/delay.h>

#include "cs4624.h"
#include "register.h"
#include "cs4624_image.h"
// 调试打印输出
#define CS4624_DEBUG
#ifdef CS4624_DEBUG
#define FUNC_LOG()  printk(KERN_EMERG "FUNC_LOG: [%d][%s()]\n", __LINE__, __FUNCTION__)
#endif


//模块信息
#define DRIVER_NAME "cs4624"
MODULE_AUTHOR("CHENQL");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("pci driver for cs4624 card");
MODULE_SUPPORTED_DEVICE("{{Cirrus Logic,Sound Fusion (CS4622)},"
		"{Cirrus Logic,Sound Fusion (CS4624)}}");


static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

static DEFINE_PCI_DEVICE_TABLE(snd_mychip_ids) = {
	{ PCI_VDEVICE(CIRRUS, 0x6001), 0, },   /* CS4280 */
	{ PCI_VDEVICE(CIRRUS, 0x6003), 0, },   /* CS4624 */
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, snd_mychip_ids);


/*
 *  constants
 */

#define MYCHIP_BA0_SIZE         0x1000
#define MYCHIP_BA1_DATA0_SIZE 0x3000
#define MYCHIP_BA1_DATA1_SIZE 0x3800
#define MYCHIP_BA1_PRG_SIZE     0x7000
#define MYCHIP_BA1_REG_SIZE     0x0100

#define MYCHIP_MIN_PERIOD_SIZE 2048
#define MYCHIP_MAX_PERIOD_SIZE 2048
#define MYCHIP_FRAGS 2

#define MYCHIP_FIFO_SIZE 32

//---------------------IO-----------------
/*
 *  common I/O routines
 */
static inline void snd_mychip_pokeBA1(struct snd_mychip *chip, unsigned long reg, unsigned int val){
	unsigned int bank = reg >> 16;
	unsigned int offset = reg & 0xffff;
	writel(val, chip->ba1_region.idx[bank].remap_addr + offset);
}

static inline unsigned int snd_mychip_peekBA1(struct snd_mychip *chip, unsigned long reg){
	unsigned int bank = reg >> 16;
	unsigned int offset = reg & 0xffff;
	return readl(chip->ba1_region.idx[bank].remap_addr + offset);
}

static inline void snd_mychip_pokeBA0(struct snd_mychip *chip, unsigned long offset, unsigned int val){
	writel(val, chip->ba0_region.idx[0].remap_addr + offset);
}

static inline unsigned int snd_mychip_peekBA0(struct snd_mychip *chip, unsigned long offset){
	return readl(chip->ba0_region.idx[0].remap_addr + offset);
}

static unsigned short snd_mychip_codec_read(struct snd_mychip *chip, unsigned short reg, int codec_index){
	int count;
	unsigned short result, tmp;
	u32 offset = 0;

	//   if (snd_BUG_ON(codec_index != MYCHIP_PRIMARY_CODEC_INDEX &&
	//               codec_index != MYCHIP_SECONDARY_CODEC_INDEX))
	//        return -EINVAL;

	//chip->active_ctrl(chip, 1);

	if (codec_index == MYCHIP_SECONDARY_CODEC_INDEX)
		offset = MYCHIP_SECONDARY_CODEC_OFFSET;

	/*
	 *  1. Write ACCAD = Command Address Register = 46Ch for AC97 register address
	 *  2. Write ACCDA = Command Data Register = 470h    for data to write to AC97 
	 *  3. Write ACCTL = Control Register = 460h for initiating the write7---55
	 *  4. Read ACCTL = 460h, DCV should be reset by now and 460h = 17h
	 *  5. if DCV not cleared, break and return error
	 *  6. Read ACSTS = Status Register = 464h, check VSTS bit
	 */

	snd_mychip_peekBA0(chip, BA0_ACSDA + offset);

	tmp = snd_mychip_peekBA0(chip, BA0_ACCTL);
	if ((tmp & ACCTL_VFRM) == 0) {
		snd_printk(KERN_WARNING  "mychip: ACCTL_VFRM not set 0x%x\n",tmp);
		snd_mychip_pokeBA0(chip, BA0_ACCTL, (tmp & (~ACCTL_ESYN)) | ACCTL_VFRM );
		msleep(50);
		tmp = snd_mychip_peekBA0(chip, BA0_ACCTL + offset);
		snd_mychip_pokeBA0(chip, BA0_ACCTL, tmp | ACCTL_ESYN | ACCTL_VFRM );

	}

	/*
	 *  Setup the AC97 control registers on the CS461x to send the
	 *  appropriate command to the AC97 to perform the read.
	 *  ACCAD = Command Address Register = 46Ch
	 *  ACCDA = Command Data Register = 470h
	 *  ACCTL = Control Register = 460h
	 *  set DCV - will clear when process completed
	 *  set CRW - Read command
	 *  set VFRM - valid frame enabled
	 *  set ESYN - ASYNC generation enabled
	 *  set RSTN - ARST# inactive, AC97 codec not reset
	 */

	snd_mychip_pokeBA0(chip, BA0_ACCAD, reg);
	snd_mychip_pokeBA0(chip, BA0_ACCDA, 0);

	if (codec_index == MYCHIP_PRIMARY_CODEC_INDEX) {
		snd_mychip_pokeBA0(chip, BA0_ACCTL,/* clear ACCTL_DCV */ ACCTL_CRW | 
				ACCTL_VFRM | ACCTL_ESYN |
				ACCTL_RSTN);
		snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_DCV | ACCTL_CRW |
				ACCTL_VFRM | ACCTL_ESYN |
				ACCTL_RSTN);
	} else {
		snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_DCV | ACCTL_TC |
				ACCTL_CRW | ACCTL_VFRM | ACCTL_ESYN |
				ACCTL_RSTN);
	}

	/*
	 *  Wait for the read to occur.
	 */
	for (count = 0; count < 1000; count++) {
		/*
		 *  First, we want to wait for a short time.
		 */
		udelay(10);
		/*
		 *  Now, check to see if the read has completed.
		 *  ACCTL = 460h, DCV should be reset by now and 460h = 17h
		 */
		if (!(snd_mychip_peekBA0(chip, BA0_ACCTL) & ACCTL_DCV))
			goto ok1;
	}

	snd_printk(KERN_ERR "AC'97 read problem (ACCTL_DCV), reg = 0x%x\n", reg);
	result = 0xffff;
	goto end;

ok1:
	/*
	 *  Wait for the valid status bit to go active.
	 */
	for (count = 0; count < 100; count++) {
		/*
		 *  Read the AC97 status register.
		 *  ACSTS = Status Register = 464h
		 *  VSTS - Valid Status
		 */
		if (snd_mychip_peekBA0(chip, BA0_ACSTS + offset) & ACSTS_VSTS)
			goto ok2;
		udelay(10);
	}

	snd_printk(KERN_ERR "AC'97 read problem (ACSTS_VSTS), codec_index %d, reg = 0x%x\n", codec_index, reg);
	result = 0xffff;
	goto end;

ok2:
	/*
	 *  Read the data returned from the AC97 register.
	 *  ACSDA = Status Data Register = 474h
	 */
#if 0
	printk(KERN_DEBUG "e) reg = 0x%x, val = 0x%x, BA0_ACCAD = 0x%x\n", reg,
			snd_mychip_peekBA0(chip, BA0_ACSDA),
			snd_mychip_peekBA0(chip, BA0_ACCAD));
#endif

	//snd_mychip_peekBA0(chip, BA0_ACCAD);
	result = snd_mychip_peekBA0(chip, BA0_ACSDA + offset);
end:
	//chip->active_ctrl(chip, -1);
	return result;
}

static unsigned short snd_mychip_ac97_read(struct snd_ac97 * ac97, unsigned short reg){
	struct snd_mychip *chip = ac97->private_data;
	unsigned short val;
	int codec_index = ac97->num;

	//   if (snd_BUG_ON(codec_index != mychip_PRIMARY_CODEC_INDEX &&
	//               codec_index != mychip_SECONDARY_CODEC_INDEX))
	//        return 0xffff;

	val = snd_mychip_codec_read(chip, reg, codec_index);

	return val;
}


static void snd_mychip_codec_write(struct snd_mychip *chip, unsigned short reg, unsigned short val, int codec_index){
	int count;

	//   if (snd_BUG_ON(codec_index != mychip_PRIMARY_CODEC_INDEX &&
	//               codec_index != mychip_SECONDARY_CODEC_INDEX))
	//        return;

	//chip->active_ctrl(chip, 1);

	/*
	 *  1. Write ACCAD = Command Address Register = 46Ch for AC97 register address
	 *  2. Write ACCDA = Command Data Register = 470h    for data to write to AC97
	 *  3. Write ACCTL = Control Register = 460h for initiating the write
	 *  4. Read ACCTL = 460h, DCV should be reset by now and 460h = 07h
	 *  5. if DCV not cleared, break and return error
	 */

	/*
	 *  Setup the AC97 control registers on the CS461x to send the
	 *  appropriate command to the AC97 to perform the read.
	 *  ACCAD = Command Address Register = 46Ch
	 *  ACCDA = Command Data Register = 470h
	 *  ACCTL = Control Register = 460h
	 *  set DCV - will clear when process completed
	 *  reset CRW - Write command
	 *  set VFRM - valid frame enabled
	 *  set ESYN - ASYNC generation enabled
	 *  set RSTN - ARST# inactive, AC97 codec not reset
	 */
	snd_mychip_pokeBA0(chip, BA0_ACCAD , reg);
	snd_mychip_pokeBA0(chip, BA0_ACCDA , val);
	snd_mychip_peekBA0(chip, BA0_ACCTL);

	if (codec_index == MYCHIP_PRIMARY_CODEC_INDEX) {
		snd_mychip_pokeBA0(chip, BA0_ACCTL, /* clear ACCTL_DCV */ ACCTL_VFRM |
				ACCTL_ESYN | ACCTL_RSTN);
		snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_DCV | ACCTL_VFRM |
				ACCTL_ESYN | ACCTL_RSTN);
	} else {
		snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_DCV | ACCTL_TC |
				ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);
	}

	for (count = 0; count < 4000; count++) {
		/*
		 *  First, we want to wait for a short time.
		 */
		udelay(10);
		/*
		 *  Now, check to see if the write has completed.
		 *  ACCTL = 460h, DCV should be reset by now and 460h = 07h
		 */
		if (!(snd_mychip_peekBA0(chip, BA0_ACCTL) & ACCTL_DCV)) {
			goto end;
		}
	}
	snd_printk(KERN_ERR "AC'97 write problem, codec_index = %d, reg = 0x%x, val = 0x%x\n", codec_index, reg, val);
end:
	return ;
	//chip->active_ctrl(chip, -1);
}

static void snd_mychip_ac97_write(struct snd_ac97 *ac97, unsigned short reg, unsigned short val){
	struct snd_mychip *chip = ac97->private_data;
	int codec_index = ac97->num;

	//   if (snd_BUG_ON(codec_index != mychip_PRIMARY_CODEC_INDEX &&
	//               codec_index != mychip_SECONDARY_CODEC_INDEX))
	//        return;

	snd_mychip_codec_write(chip, reg, val, codec_index);
}

//---------------------RATE---------------

/*
 *  Sample rate routines
 */

#define GOF_PER_SEC 200

static void snd_mychip_set_play_sample_rate(struct snd_mychip *chip, unsigned int rate)
{
	unsigned long flags;
	unsigned int tmp1, tmp2;
	unsigned int phiIncr;
	unsigned int correctionPerGOF, correctionPerSec;

	/*
	 *  Compute the values used to drive the actual sample rate conversion.
	 *  The following formulas are being computed, using inline assembly
	 *  since we need to use 64 bit arithmetic to compute the values:
	 *
	 *  phiIncr = floor((Fs,in * 2^26) / Fs,out)
	 *  correctionPerGOF = floor((Fs,in * 2^26 - Fs,out * phiIncr) /
	 *                                   GOF_PER_SEC)
	 *  ulCorrectionPerSec = Fs,in * 2^26 - Fs,out * phiIncr -M
	 *                       GOF_PER_SEC * correctionPerGOF
	 *
	 *  i.e.
	 *
	 *  phiIncr:other = dividend:remainder((Fs,in * 2^26) / Fs,out)
	 *  correctionPerGOF:correctionPerSec =
	 *      dividend:remainder(ulOther / GOF_PER_SEC)
	 */
	tmp1 = rate << 16;
	phiIncr = tmp1 / 48000;
	tmp1 -= phiIncr * 48000;
	tmp1 <<= 10;
	phiIncr <<= 10;
	tmp2 = tmp1 / 48000;
	phiIncr += tmp2;
	tmp1 -= tmp2 * 48000;
	correctionPerGOF = tmp1 / GOF_PER_SEC;
	tmp1 -= correctionPerGOF * GOF_PER_SEC;
	correctionPerSec = tmp1;

	/*
	 *  Fill in the SampleRateConverter control block.
	 */
	spin_lock_irqsave(&chip->reg_lock, flags);
	snd_mychip_pokeBA1(chip, BA1_PSRC,
			((correctionPerSec << 16) & 0xFFFF0000) | (correctionPerGOF & 0xFFFF));
	snd_mychip_pokeBA1(chip, BA1_PPI, phiIncr);
	spin_unlock_irqrestore(&chip->reg_lock, flags);
}

static void snd_mychip_set_capture_sample_rate(struct snd_mychip *chip, unsigned int rate)
{
	unsigned long flags;
	unsigned int phiIncr, coeffIncr, tmp1, tmp2;
	unsigned int correctionPerGOF, correctionPerSec, initialDelay;
	unsigned int frameGroupLength, cnt;

	/*
	 *  We can only decimate by up to a factor of 1/9th the hardware rate.
	 *  Correct the value if an attempt is made to stray outside that limit.
	 */
	if ((rate * 9) < 48000)
		rate = 48000 / 9;

	/*
	 *  We can not capture at at rate greater than the Input Rate (48000).
	 *  Return an error if an attempt is made to stray outside that limit.
	 */
	if (rate > 48000)
		rate = 48000;

	/*
	 *  Compute the values used to drive the actual sample rate conversion.
	 *  The following formulas are being computed, using inline assembly
	 *  since we need to use 64 bit arithmetic to compute the values:
	 *
	 *     coeffIncr = -floor((Fs,out * 2^23) / Fs,in)
	 *     phiIncr = floor((Fs,in * 2^26) / Fs,out)
	 *     correctionPerGOF = floor((Fs,in * 2^26 - Fs,out * phiIncr) /
	 *                                GOF_PER_SEC)
	 *     correctionPerSec = Fs,in * 2^26 - Fs,out * phiIncr -
	 *                          GOF_PER_SEC * correctionPerGOF
	 *     initialDelay = ceil((24 * Fs,in) / Fs,out)
	 *
	 * i.e.
	 *
	 *     coeffIncr = neg(dividend((Fs,out * 2^23) / Fs,in))
	 *     phiIncr:ulOther = dividend:remainder((Fs,in * 2^26) / Fs,out)
	 *     correctionPerGOF:correctionPerSec =
	 *       dividend:remainder(ulOther / GOF_PER_SEC)
	 *     initialDelay = dividend(((24 * Fs,in) + Fs,out - 1) / Fs,out)
	 */

	tmp1 = rate << 16;
	coeffIncr = tmp1 / 48000;
	tmp1 -= coeffIncr * 48000;
	tmp1 <<= 7;
	coeffIncr <<= 7;
	coeffIncr += tmp1 / 48000;
	coeffIncr ^= 0xFFFFFFFF;
	coeffIncr++;
	tmp1 = 48000 << 16;
	phiIncr = tmp1 / rate;
	tmp1 -= phiIncr * rate;
	tmp1 <<= 10;
	phiIncr <<= 10;
	tmp2 = tmp1 / rate;
	phiIncr += tmp2;
	tmp1 -= tmp2 * rate;
	correctionPerGOF = tmp1 / GOF_PER_SEC;
	tmp1 -= correctionPerGOF * GOF_PER_SEC;
	correctionPerSec = tmp1;
	initialDelay = ((48000 * 24) + rate - 1) / rate;

	/*
	 *  Fill in the VariDecimate control block.
	 */
	spin_lock_irqsave(&chip->reg_lock, flags);
	snd_mychip_pokeBA1(chip, BA1_CSRC,
			((correctionPerSec << 16) & 0xFFFF0000) | (correctionPerGOF & 0xFFFF));
	snd_mychip_pokeBA1(chip, BA1_CCI, coeffIncr);
	snd_mychip_pokeBA1(chip, BA1_CD,
			(((BA1_VARIDEC_BUF_1 + (initialDelay << 2)) << 16) & 0xFFFF0000) | 0x80);
	snd_mychip_pokeBA1(chip, BA1_CPI, phiIncr);
	spin_unlock_irqrestore(&chip->reg_lock, flags);

	/*
	 *  Figure out the frame group length for the write back task.  Basically,
	 *  this is just the factors of 24000 (2^6*3*5^3) that are not present in
	 *  the output sample rate.
	 */
	frameGroupLength = 1;
	for (cnt = 2; cnt <= 64; cnt *= 2) {
		if (((rate / cnt) * cnt) != rate)
			frameGroupLength *= 2;
	}
	if (((rate / 3) * 3) != rate) {
		frameGroupLength *= 3;
	}
	for (cnt = 5; cnt <= 125; cnt *= 5) {
		if (((rate / cnt) * cnt) != rate) 
			frameGroupLength *= 5;
	}

	/*
	 * Fill in the WriteBack control block.
	 */
	spin_lock_irqsave(&chip->reg_lock, flags);
	snd_mychip_pokeBA1(chip, BA1_CFG1, frameGroupLength);
	snd_mychip_pokeBA1(chip, BA1_CFG2, (0x00800000 | frameGroupLength));
	snd_mychip_pokeBA1(chip, BA1_CCST, 0x0000FFFF);
	snd_mychip_pokeBA1(chip, BA1_CSPB, ((65536 * rate) / 24000));
	snd_mychip_pokeBA1(chip, (BA1_CSPB + 4), 0x0000FFFF);
	spin_unlock_irqrestore(&chip->reg_lock, flags);
}



// --------------------PCM----------------
//

//每个card 设备可以最多拥有4个 pcm 实例。一个pcm实例对应予一个pcm设备文件。
//根据硬件手册定义硬件
static struct snd_pcm_hardware snd_mychip_playback ={
	.info =             (SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED | 
			SNDRV_PCM_INFO_BLOCK_TRANSFER /*|*/
			/*SNDRV_PCM_INFO_RESUME*/),
	.formats =          (SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_U8 |
			SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |
			SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE),
	.rates =       SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =         5500,
	.rate_max =         48000,
	.channels_min =          1,
	.channels_max =          2,
	.buffer_bytes_max = (256 * 1024),
	.period_bytes_min = MYCHIP_MIN_PERIOD_SIZE,
	.period_bytes_max = MYCHIP_MAX_PERIOD_SIZE,
	.periods_min =      MYCHIP_FRAGS,
	.periods_max =      1024,
	.fifo_size =        0,
};

static struct snd_pcm_hardware snd_mychip_capture ={
	.info =             (SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_BLOCK_TRANSFER /*|*/
			/*SNDRV_PCM_INFO_RESUME*/),
	.formats =          SNDRV_PCM_FMTBIT_S16_LE,
	.rates =       SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =         5500,
	.rate_max =         48000,
	.channels_min =          2,
	.channels_max =          2,
	.buffer_bytes_max = (256 * 1024),
	.period_bytes_min = MYCHIP_MIN_PERIOD_SIZE,
	.period_bytes_max = MYCHIP_MAX_PERIOD_SIZE,
	.periods_min =      MYCHIP_FRAGS,
	.periods_max =      1024,
	.fifo_size =        0,
};

// 关于peroid的概念有这样的描述：The “period” is a term that corresponds to a fragment in the OSS world. The period defines the size at which a PCM interrupt is generated. 
// peroid的概念很重要
static unsigned int period_sizes[] = { 32, 64, 128, 256, 512, 1024, 2048 };

static struct snd_pcm_hw_constraint_list hw_constraints_period_sizes = {
	.count = ARRAY_SIZE(period_sizes),
	.list = period_sizes,
	.mask = 0
};

static void snd_mychip_pb_trans_copy(struct snd_pcm_substream *substream, struct snd_pcm_indirect *rec, size_t bytes){
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream * cpcm = runtime->private_data;
	memcpy(cpcm->hw_buf.area + rec->hw_data, runtime->dma_area + rec->sw_data, bytes);
}

static int snd_mychip_playback_transfer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream * cpcm = runtime->private_data;
	snd_pcm_indirect_playback_transfer(substream, &cpcm->pcm_rec, snd_mychip_pb_trans_copy);
	return 0;
}

static void snd_mychip_cp_trans_copy(struct snd_pcm_substream *substream,
		struct snd_pcm_indirect *rec, size_t bytes)
{
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	memcpy(runtime->dma_area + rec->sw_data,
			chip->capt->hw_buf.area + rec->hw_data, bytes);
}

static int snd_mychip_capture_transfer(struct snd_pcm_substream *substream)
{
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	snd_pcm_indirect_capture_transfer(substream, &chip->capt->pcm_rec, snd_mychip_cp_trans_copy);
	return 0;
}
static int snd_mychip_playback_open_channel(struct snd_pcm_substream *substream, int pcm_channel_id){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	// snd_pcm_runtime是pcm运行时的信息。
	// 当打开一个pcm子流时，pcm运行时实例就会分配给这个子流。
	// 它拥有很多信息：hw_params和sw_params配置拷贝，缓冲区指针，mmap记录，自旋锁等。snd_pcm_runtime对于驱动程序操作集函数是只读的，仅pcm中间层可以改变或更新这些信息。
	struct snd_pcm_runtime *runtime = substream->runtime;  
	struct mychip_dma_stream *dma; 

	// 分配dma内存空间，设置chip->play = dma
	dma = kzalloc(sizeof(*dma), GFP_KERNEL);
	if(dma == NULL){
		return -ENOMEM;
	}
	if (snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(chip->pci),
				PAGE_SIZE, &dma->hw_buf) < 0) {
		kfree(dma);
		return -ENOMEM;
	}
	dma->substream = substream;
	//dma->pcm_channel = NULL; 
	//dma->pcm_channel_id = pcm_channel_id;

	chip->play = dma;


	//设定runtime硬件参数  
	runtime->private_data = dma;  
	runtime->hw = snd_mychip_playback;
	//runtime->private_free = snd_mychip_pcm_free_substream;

	/* Ensure that buffer size is a multiple of period size */  
	snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 
			&hw_constraints_period_sizes);

	return 0;  
}

static struct snd_pcm_ops snd_mychip_playback_ops;
static struct snd_pcm_ops snd_mychip_playback_indirect_ops;
//open函数为PCM模块设定支持的传输模式、数据格式、通道数、period等参数，并为playback/capture stream分配相应的DMA通道。
static int snd_mychip_playback_open(struct snd_pcm_substream *substream){
	snd_printdd("open front channel\n");
	return snd_mychip_playback_open_channel(substream, DSP_PCM_MAIN_CHANNEL);
}

//close函数，停止dma，释放数据
static int snd_mychip_playback_close(struct snd_pcm_substream *substream){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	struct mychip_dma_stream *dma = substream->runtime->private_data;
	chip->play = NULL;
	dma->substream = NULL;
	snd_dma_free_pages(&dma->hw_buf);
	return 0;  
}

//hw_params函数为substream（每打开一个playback或capture，ALSA core均产生相应的一个substream）设定DMA的源（目的）地址，以及DMA缓冲区的大小。
static int snd_mychip_playback_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params){
	int err;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream *dma = runtime->private_data;

	if (params_periods(hw_params) == MYCHIP_FRAGS) {
		if (runtime->dma_area != dma->hw_buf.area)
			snd_pcm_lib_free_pages(substream);
		runtime->dma_area = dma->hw_buf.area;
		runtime->dma_addr = dma->hw_buf.addr;
		runtime->dma_bytes = dma->hw_buf.bytes;
		substream->ops = &snd_mychip_playback_ops;
		FUNC_LOG();
	}else {
		if (runtime->dma_area == dma->hw_buf.area) {
			runtime->dma_area = NULL;
			runtime->dma_addr = 0;
			runtime->dma_bytes = 0;
		}
		if ((err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params))) < 0) {
			return err;
		}
		substream->ops = &snd_mychip_playback_indirect_ops;
		FUNC_LOG();
	}

	return 0;  
}

//hw_params函数为substream（每打开一个playback或capture，ALSA core均产生相应的一个substream）设定DMA的源（目的）地址，以及DMA缓冲区的大小。
static int snd_mychip_playback_hw_free(struct snd_pcm_substream *substream){
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream *dma = runtime->private_data;

	if (runtime->dma_area != dma->hw_buf.area)
		snd_pcm_lib_free_pages(substream);

	runtime->dma_area = NULL;
	runtime->dma_addr = 0;
	runtime->dma_bytes = 0;
	return 0;  
}

// 当pcm“准备好了”调用该函数。在这里根据channels、buffer_bytes等来设定DMA传输参数()，跟具体硬件平台相关。
// 注：每次调用snd_pcm_prepare()的时候均会调用prepare函数。
static int snd_mychip_playback_prepare(struct snd_pcm_substream *substream){
	unsigned int tmp;
	unsigned int pfie;
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream *dma = runtime->private_data;
	/*在此做设定一些硬件配置
	 *例如....
	 */
	//mychip_set_sample_format(chip, runtime->format);
	//mychip_set_sample_rate(chip, runtime->rate);
	//mychip_set_channels(chip, runtime->channels);
	//mychip_set_dma_setup(chip, runtime->dma_addr,
	//chip->buffer_size,
	//chip->period_size);

	pfie = snd_mychip_peekBA1(chip, BA1_PFIE);
	pfie &= ~0x0000f03f;
	dma->shift = 2;
	/* if to convert from stereo to mono */
	if (runtime->channels == 1) {
		dma->shift--;
		pfie |= 0x00002000;
	}
	/* if to convert from 8 bit to 16 bit */
	if (snd_pcm_format_width(runtime->format) == 8) {
		dma->shift--;
		pfie |= 0x00001000;
	}
	/* if to convert to unsigned */
	if (snd_pcm_format_unsigned(runtime->format))
		pfie |= 0x00008000;

	/* Never convert byte order when sample stream is 8 bit */
	if (snd_pcm_format_width(runtime->format) != 8) {
		/* convert from big endian to little endian */
		if (snd_pcm_format_big_endian(runtime->format))
			pfie |= 0x00004000;
	}

	memset(&dma->pcm_rec, 0, sizeof(dma->pcm_rec));
	dma->pcm_rec.sw_buffer_size = snd_pcm_lib_buffer_bytes(substream);
	dma->pcm_rec.hw_buffer_size = runtime->period_size * MYCHIP_FRAGS << dma->shift;

	snd_mychip_pokeBA1(chip, BA1_PBA, dma->hw_buf.addr);
	tmp = snd_mychip_peekBA1(chip, BA1_PDTC);
	tmp &= ~0x000003ff;
	tmp |= (4 << dma->shift) - 1;
	snd_mychip_pokeBA1(chip, BA1_PDTC, tmp);
	snd_mychip_pokeBA1(chip, BA1_PFIE, pfie);
	snd_mychip_set_play_sample_rate(chip, runtime->rate);

	return 0;  
}
// 当pcm开始、停止、暂停的时候都会调用trigger函数。
// Trigger函数里面的操作应该是原子的，不要在调用这些操作时进入睡眠，trigger函数应尽量小，甚至仅仅是触发DMA。
static int snd_mychip_playback_trigger(struct snd_pcm_substream *substream, int cmd){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	int res = 0;  
	unsigned int tmp;
	spin_lock(&chip->reg_lock);  

	switch (cmd) {  
		case SNDRV_PCM_TRIGGER_START:  
		case SNDRV_PCM_TRIGGER_RESUME:  
			FUNC_LOG();
			if (substream->runtime->periods != MYCHIP_FRAGS){
				snd_mychip_playback_transfer(substream);
			}
			tmp = snd_mychip_peekBA1(chip, BA1_PCTL);
			tmp &= 0x0000ffff;
			FUNC_LOG();
			snd_mychip_pokeBA1(chip, BA1_PCTL, chip->play_ctl | tmp);
			break;  

		case SNDRV_PCM_TRIGGER_STOP:  
		case SNDRV_PCM_TRIGGER_SUSPEND:  
			tmp = snd_mychip_peekBA1(chip, BA1_PCTL);
			tmp &= 0x0000ffff;
			snd_mychip_pokeBA1(chip, BA1_PCTL, tmp);
			FUNC_LOG();
			//dma_ctrl(prtd->params->channel, DMAOP_STOP); //DMA停止  
			break;  

		default:  
			res = -EINVAL;  
			break;  
	}  

	spin_unlock(&chip->reg_lock);  

	return res;  

}

static snd_pcm_uframes_t snd_mychip_playback_direct_pointer(struct snd_pcm_substream *substream){
	struct mychip_dma_stream *dma = substream->runtime->private_data;
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);

	size_t ptr;
	ptr = snd_mychip_peekBA1(chip, BA1_PBA);
	ptr -= dma->hw_buf.addr;
	return ptr >> dma->shift;
}
static snd_pcm_uframes_t snd_mychip_playback_indirect_pointer(struct snd_pcm_substream *substream){
	struct mychip_dma_stream *dma = substream->runtime->private_data;
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);

	size_t ptr;
	ptr = snd_mychip_peekBA1(chip, BA1_PBA);

	ptr -= dma->hw_buf.addr;
	return snd_pcm_indirect_playback_pointer(substream, &dma->pcm_rec, ptr);
}



static struct snd_pcm_ops snd_mychip_capture_ops;
static struct snd_pcm_ops snd_mychip_capture_indirect_ops;

//open函数为PCM模块设定支持的传输模式、数据格式、通道数、period等参数，并为playback/capture stream分配相应的DMA通道。
static int snd_mychip_capture_open(struct snd_pcm_substream *substream){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;  
	struct mychip_dma_stream *dma; 

	// 分配dma内存空间，设置chip->capt = dma
	dma = kzalloc(sizeof(*dma), GFP_KERNEL);
	if(dma == NULL){
		return -ENOMEM;
	}
	if (snd_dma_alloc_pages(SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(chip->pci),
				PAGE_SIZE, &dma->hw_buf) < 0){
		kfree(dma);
		return -ENOMEM;
	}
	dma->substream = substream;
	chip->capt = dma;
	runtime->hw = snd_mychip_capture;
	runtime->private_data = dma;
	FUNC_LOG();
	//if (chip->accept_valid)
	//   substream->runtime->hw.info |= SNDRV_PCM_INFO_MMAP_VALID;
	return 0;
}

//close函数，停止dma，释放数据
static int snd_mychip_capture_close(struct snd_pcm_substream *substream){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	struct mychip_dma_stream *dma = substream->runtime->private_data;
	chip->capt = NULL;
	dma->substream = NULL;
	snd_dma_free_pages(&dma->hw_buf);
	return 0;  
}

//hw_params函数为substream（每打开一个playback或capture，ALSA core均产生相应的一个substream）设定DMA的源（目的）地址，以及DMA缓冲区的大小。
static int snd_mychip_capture_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params){
	int err;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream *dma = runtime->private_data;

	if (params_periods(hw_params) == MYCHIP_FRAGS) {
		if (runtime->dma_area != dma->hw_buf.area)
			snd_pcm_lib_free_pages(substream);
		runtime->dma_area = dma->hw_buf.area;
		runtime->dma_addr = dma->hw_buf.addr;
		runtime->dma_bytes = dma->hw_buf.bytes;
		FUNC_LOG();
		substream->ops = &snd_mychip_capture_ops;
	}else {
		if (runtime->dma_area == dma->hw_buf.area) {
			runtime->dma_area = NULL;
			runtime->dma_addr = 0;
			runtime->dma_bytes = 0;
		}
		if ((err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params))) < 0) {
			return err;
		}
		substream->ops = &snd_mychip_capture_indirect_ops;
		FUNC_LOG();
	}

	return 0;  
}
static int snd_mychip_capture_hw_free(struct snd_pcm_substream *substream){
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream *dma = runtime->private_data;

	if (runtime->dma_area != dma->hw_buf.area)
		snd_pcm_lib_free_pages(substream);

	runtime->dma_area = NULL;
	runtime->dma_addr = 0;
	runtime->dma_bytes = 0;
	return 0;  
}


// 当pcm“准备好了”调用该函数。在这里根据channels、buffer_bytes等来设定DMA传输参数()，跟具体硬件平台相关。
// 注：每次调用snd_pcm_prepare()的时候均会调用prepare函数。
static int snd_mychip_capture_prepare(struct snd_pcm_substream *substream){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct mychip_dma_stream *dma = runtime->private_data;

	dma->shift = 2;

	dma->pcm_rec.hw_buffer_size = runtime->period_size * MYCHIP_FRAGS << dma->shift;

	snd_mychip_pokeBA1(chip, BA1_CBA, dma->hw_buf.addr);
	chip->capt->shift = 2;
	memset(&dma->pcm_rec, 0, sizeof(dma->pcm_rec));
	dma->pcm_rec.sw_buffer_size = snd_pcm_lib_buffer_bytes(substream);
	dma->pcm_rec.hw_buffer_size = runtime->period_size * MYCHIP_FRAGS << dma->shift;

	snd_mychip_set_capture_sample_rate(chip, runtime->rate);

	return 0;  
}
// 当pcm开始、停止、暂停的时候都会调用trigger函数。
// Trigger函数里面的操作应该是原子的，不要在调用这些操作时进入睡眠，trigger函数应尽量小，甚至仅仅是触发DMA。
static int snd_mychip_capture_trigger(struct snd_pcm_substream *substream, int cmd){
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);
	int res = 0;  
	unsigned int tmp;
	spin_lock(&chip->reg_lock);  

	switch (cmd) {  
		case SNDRV_PCM_TRIGGER_START:  
		case SNDRV_PCM_TRIGGER_RESUME:  
			tmp = snd_mychip_peekBA1(chip, BA1_CCTL);
			tmp &= 0xffff0000;
			snd_mychip_pokeBA1(chip, BA1_CCTL, chip->capt_ctl | tmp);
			FUNC_LOG();
			break;  

		case SNDRV_PCM_TRIGGER_STOP:  
		case SNDRV_PCM_TRIGGER_SUSPEND:  
			tmp = snd_mychip_peekBA1(chip, BA1_CCTL);
			tmp &= 0xffff0000;
			snd_mychip_pokeBA1(chip, BA1_CCTL, tmp);
			FUNC_LOG();
			break;  

		default:  
			res = -EINVAL;  
			break;  
	}  

	spin_unlock(&chip->reg_lock);  

	return res;  

}

static snd_pcm_uframes_t snd_mychip_capture_direct_pointer(struct snd_pcm_substream *substream){
	struct mychip_dma_stream *dma = substream->runtime->private_data;
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);

	size_t ptr;
	ptr = snd_mychip_peekBA1(chip, BA1_CBA);
	ptr -= dma->hw_buf.addr;
	return ptr >> dma->shift;
}

static snd_pcm_uframes_t snd_mychip_capture_indirect_pointer(struct snd_pcm_substream *substream){
	struct mychip_dma_stream *dma = substream->runtime->private_data;
	struct snd_mychip *chip = snd_pcm_substream_chip(substream);

	size_t ptr;
	ptr = snd_mychip_peekBA1(chip, BA1_CBA);
	ptr -= dma->hw_buf.addr;
	return snd_pcm_indirect_capture_pointer(substream, &chip->capt->pcm_rec, ptr);
}

static struct snd_pcm_ops snd_mychip_playback_ops = {
	.open = snd_mychip_playback_open,
	.close = snd_mychip_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_mychip_playback_hw_params,
	.hw_free = snd_mychip_playback_hw_free,
	.prepare = snd_mychip_playback_prepare,
	.trigger = snd_mychip_playback_trigger,
	.pointer = snd_mychip_playback_direct_pointer,
};

static struct snd_pcm_ops snd_mychip_playback_indirect_ops = {
	.open = snd_mychip_playback_open,
	.close = snd_mychip_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_mychip_playback_hw_params,
	.hw_free = snd_mychip_playback_hw_free,
	.prepare = snd_mychip_playback_prepare,
	.trigger = snd_mychip_playback_trigger,
	.pointer = snd_mychip_playback_indirect_pointer,
	.ack =	snd_mychip_playback_transfer,
};

static struct snd_pcm_ops snd_mychip_capture_ops = {
	.open = snd_mychip_capture_open,
	.close = snd_mychip_capture_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_mychip_capture_hw_params,
	.hw_free = snd_mychip_capture_hw_free,
	.prepare = snd_mychip_capture_prepare,
	.trigger = snd_mychip_capture_trigger,
	.pointer = snd_mychip_capture_direct_pointer,
};

static struct snd_pcm_ops snd_mychip_capture_indirect_ops = {
	.open = snd_mychip_capture_open,
	.close = snd_mychip_capture_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = snd_mychip_capture_hw_params,
	.hw_free = snd_mychip_capture_hw_free,
	.prepare = snd_mychip_capture_prepare,
	.trigger = snd_mychip_capture_trigger,
	.pointer = snd_mychip_capture_indirect_pointer,
	.ack = snd_mychip_capture_transfer,
};


static int __init snd_mychip_new_pcm(struct snd_mychip* chip){
	struct snd_pcm *pcm;
	int err;
	//    int snd_pcm_new(struct snd_card *card, const char *id, int device, int playback_count, int capture_count, struct snd_pcm ** rpcm);
	if ((err = snd_pcm_new(chip->card, "My Chip", 0 , 1, 1, &pcm)) < 0){
		return err;
	}
	pcm->private_data = chip;
	strcpy(pcm->name, "My Chip");
	chip->pcm = pcm;

	// 设置操作函数
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_mychip_playback_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,  &snd_mychip_capture_ops);

	//分配缓存
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV, snd_dma_pci_data(chip->pci), 64*1024, 256*1024);
	return 0;
}










//---------------CONTROL--------------------

static void snd_mychip_mixer_free_ac97_bus(struct snd_ac97_bus *bus)
{
	struct snd_mychip *chip = bus->private_data;

	chip->ac97_bus = NULL;
}

static void snd_mychip_mixer_free_ac97(struct snd_ac97 *ac97)
{
	struct snd_mychip *chip = ac97->private_data;

	//if (snd_BUG_ON(ac97 != chip->ac97[CS46XX_PRIMARY_CODEC_INDEX] &&
	//	       ac97 != chip->ac97[CS46XX_SECONDARY_CODEC_INDEX]))
	//	return;

	if (ac97 == chip->ac97[MYCHIP_PRIMARY_CODEC_INDEX]) {
		chip->ac97[MYCHIP_PRIMARY_CODEC_INDEX] = NULL;
		//chip->eapd_switch = NULL;
	}
	else
		chip->ac97[MYCHIP_SECONDARY_CODEC_INDEX] = NULL;
}
//info回调函数用于获取control的详细信息。它的主要工作就是填充通过参数传入的snd_ctl_elem_info对象，以下例子是一个具有单个元素的boolean型control的info回调
static int snd_my_ctl_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo){  
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;    //type字段指出该control的值类型，值类型可以是BOOLEAN, INTEGER, ENUMERATED, BYTES,IEC958和INTEGER64之一
	uinfo->count = 2;        //count字段指出了改control中包含有多少个元素单元，比如，立体声的音量control左右两个声道的音量值，它的count字段等于2。
	uinfo->value.integer.min = 0;    //value字段是一个联合体（union），value的内容和control的类型有关。可以指定最大、最小和步长。
	uinfo->value.integer.max = 0x7fff;
	return 0;  
}  

// 从volume寄存器读数据，放到ucontrol->value.integer.value[0]中
static int snd_my_ctl_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol){
	struct snd_mychip *chip = snd_kcontrol_chip(kcontrol);
	//ucontrol->value.integer.value[0] = get_some_value(chip);
	//如果private字段这样设定的 .private_value = reg | (shift << 16) | (mask << 24);
	int reg = kcontrol->private_value;
	unsigned int val = snd_mychip_peekBA1(chip, reg);
	ucontrol->value.integer.value[0] = 0xffff - (val >> 16);
	ucontrol->value.integer.value[1] = 0xffff - (val & 0xffff);
	return 0; 
}

// 从volume寄存器写数据
// 当control的值被改变时，put回调必须要返回1，如果值没有被改变，则返回0。如果发生了错误，则返回一个负数的错误号。
static int snd_my_ctl_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol){
	struct snd_mychip *chip = snd_kcontrol_chip(kcontrol);
	int reg = kcontrol->private_value;
	unsigned int val = ((0xffff - ucontrol->value.integer.value[0]) << 16 | 
			(0xffff - ucontrol->value.integer.value[1]));
	unsigned int current_value = snd_mychip_peekBA1(chip, reg);
	int changed = 0;  
	if (current_value != val) {  
		snd_mychip_pokeBA1(chip, reg, val);
		changed = 1; 
	}  
	return changed;
}  

static struct snd_kcontrol_new snd_mychip_controls[] = {
	{	
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,  //iface字段指出了control的类型
		.name = "DAC volume",     //因为control的作用是按名字来归类的
		//.index = 0,    //index字段用于保存该control的在该卡中的编号如果声卡中有不止一个codec，每个codec中有相同名字的control，这时我们可以通过index来区分这些controls。当
		//index为0时，则可以忽略这种区分策略。
		//.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,   //access字段包含了该control的访问类型。每一个bit代表一种访问类型，这些访问类型可以多个“或”运算组合在一起。未定义（.access==0），此时也认为是READWRITE类型。
		//.private_value = 0xffff,    //private_value字段包含了一个任意的长整数类型值。该值可以通过info，get，put这几个回调函数访问。你可以自己决定如何使用该字段，例如可以
		//把它拆分成多个位域，又或者是一个指针，指向某一个数据结构。
		.info = snd_my_ctl_info,    // 音量信息
		.get = snd_my_ctl_get,      // 读音量
		.put = snd_my_ctl_put,      // 写音量
		.private_value = BA1_PVOL,
	},
	{
		.iface = SNDRV_CTL_ELEM_IFACE_MIXER,  //iface字段指出了control的类型
		.name = "ADC volume",     //因为control的作用是按名字来归类的
		//		.index = 0,    //index字段用于保存该control的在该卡中的编号如果声卡中有不止一个codec，每个codec中有相同名字的control，这时我们可以通过index来区分这些controls。当
		//index为0时，则可以忽略这种区分策略。
		//		.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,   //access字段包含了该control的访问类型。每一个bit代表一种访问类型，这些访问类型可以多个“或”运算组合在一起。未定义（.access==0），此时也认为是READWRITE类型。
		//		.private_value = 0xffff,    //private_value字段包含了一个任意的长整数类型值。该值可以通过info，get，put这几个回调函数访问。你可以自己决定如何使用该字段，例如可以
		//把它拆分成多个位域，又或者是一个指针，指向某一个数据结构。
		.info = snd_my_ctl_info,    // 音量信息
		.get = snd_my_ctl_get,      // 读音量
		.put = snd_my_ctl_put,      // 写音量
		.private_value = BA1_CVOL,
	},

};

static int __init mychip_detect_codec(struct snd_mychip *chip, int codec)
{
	int idx, err;
	struct snd_ac97_template ac97;

	memset(&ac97, 0, sizeof(ac97));
	ac97.private_data = chip;
	ac97.private_free = snd_mychip_mixer_free_ac97;
	ac97.num = codec;

	if (codec == MYCHIP_SECONDARY_CODEC_INDEX) {
		snd_mychip_codec_write(chip, AC97_RESET, 0, codec);
		udelay(10);
		if (snd_mychip_codec_read(chip, AC97_RESET, codec) & 0x8000) {
			snd_printdd("snd_mychip: seconadry codec not present\n");
			return -ENXIO;
		}
	}

	snd_mychip_codec_write(chip, AC97_MASTER, 0x8000, codec);
	for (idx = 0; idx < 100; ++idx) {
		if (snd_mychip_codec_read(chip, AC97_MASTER, codec) == 0x8000) {
			err = snd_ac97_mixer(chip->ac97_bus, &ac97, &chip->ac97[codec]);
			return err;
		}
		msleep(10);
	}
	snd_printdd("snd_mychip: codec %d detection timeout\n", codec);
	return -ENXIO;
}

static int __init snd_mychip_new_mixer(struct snd_mychip *chip){
	struct snd_card *card = chip->card;
	int i, err;
	static struct snd_ac97_bus_ops ops = {
		.write = snd_mychip_ac97_write,
		.read = snd_mychip_ac97_read,
	};
	chip->nr_ac97_codecs = 0;
	snd_printdd("snd_mychip: detecting primary codec\n");
	if ((err = snd_ac97_bus(card, 0, &ops, chip, &chip->ac97_bus)) < 0){
		return err;
	}
	chip->ac97_bus->private_free = snd_mychip_mixer_free_ac97_bus;

	if (mychip_detect_codec(chip, MYCHIP_PRIMARY_CODEC_INDEX) < 0)
		return -ENXIO;
	chip->nr_ac97_codecs = 1;

	for (i = 0; i < ARRAY_SIZE(snd_mychip_controls); i++) {
		struct snd_kcontrol *kctl;
		kctl = snd_ctl_new1(&snd_mychip_controls[i], chip);
		if ((err = snd_ctl_add(card, kctl)) < 0)
			return err;
	}

	//memset(&id, 0, sizeof(id));
	//id.iface = SNDRV_CTL_ELEM_IFACE_MIXER;
	//strcpy(id.name, "External Amplifier");
	//chip->eapd_switch = snd_ctl_find_id(chip->card, &id);
	//err = snd_ctl_add(card, snd_ctl_new1(&snd_mychip_control, chip));  
	return 0;
}






//---------------mychip-----------------------
static void snd_mychip_proc_start(struct snd_mychip *chip)
{
	int cnt;

	/*
	 *  Set the frame timer to reflect the number of cycles per frame.
	 */
	snd_mychip_pokeBA1(chip, BA1_FRMT, 0xadf);
	/*
	 *  Turn on the run, run at frame, and DMA enable bits in the local copy of
	 *  the SP control register.
	 */
	snd_mychip_pokeBA1(chip, BA1_SPCR, SPCR_RUN | SPCR_RUNFR | SPCR_DRQEN);
	/*
	 *  Wait until the run at frame bit resets itself in the SP control
	 *  register.
	 */
	for (cnt = 0; cnt < 25; cnt++) {
		udelay(50);
		if (!(snd_mychip_peekBA1(chip, BA1_SPCR) & SPCR_RUNFR))
			break;
	}

	if (snd_mychip_peekBA1(chip, BA1_SPCR) & SPCR_RUNFR)
		snd_printk(KERN_ERR "SPCR_RUNFR never reset\n");
}

static void snd_mychip_proc_stop(struct snd_mychip *chip)
{
	/*
	 *  Turn off the run, run at frame, and DMA enable bits in the local copy of
	 *  the SP control register.
	 */
	snd_mychip_pokeBA1(chip, BA1_SPCR, 0);
}
int snd_mychip_download(struct snd_mychip *chip,
		u32 *src,
		unsigned long offset,
		unsigned long len)
{
	void __iomem *dst;
	unsigned int bank = offset >> 16;
	offset = offset & 0xffff;

	//if (snd_BUG_ON((offset & 3) || (len & 3)))
	// return -EINVAL;
	dst = chip->ba1_region.idx[bank].remap_addr + offset;
	len /= sizeof(u32);

	/* writel already converts 32-bit value to right endianess */
	while (len-- > 0) {
		writel(*src++, dst);
		dst += sizeof(u32);
	}
	return 0;
}

int snd_mychip_download_image(struct snd_mychip *chip)
{
	int idx, err;
	unsigned long offset = 0;

	for (idx = 0; idx < BA1_MEMORY_COUNT; idx++) {
		if ((err = snd_mychip_download(chip,
						&BA1Struct.map[offset],
						BA1Struct.memory[idx].offset,
						BA1Struct.memory[idx].size)) < 0)
			return err;
		offset += BA1Struct.memory[idx].size >> 2;
	} 
	return 0;
}

static void snd_mychip_reset(struct snd_mychip *chip){
	int idx;

	/*
	 *  Write the reset bit of the SP control register.
	 */
	snd_mychip_pokeBA1(chip, BA1_SPCR, SPCR_RSTSP);

	/*
	 *  Write the control register.
	 */
	snd_mychip_pokeBA1(chip, BA1_SPCR, SPCR_DRQEN);

	/*
	 *  Clear the trap registers.
	 */
	for (idx = 0; idx < 8; idx++) {
		snd_mychip_pokeBA1(chip, BA1_DREG, DREG_REGID_TRAP_SELECT + idx);
		snd_mychip_pokeBA1(chip, BA1_TWPR, 0xFFFF);
	}
	snd_mychip_pokeBA1(chip, BA1_DREG, 0);

	/*
	 *  Set the frame timer to reflect the number of cycles per frame.
	 */
	snd_mychip_pokeBA1(chip, BA1_FRMT, 0xadf);
}

/*
 * stop the h/w
 */
static void snd_mychip_hw_stop(struct snd_mychip *chip)
{
	unsigned int tmp;

	tmp = snd_mychip_peekBA1(chip, BA1_PFIE);
	tmp &= ~0x0000f03f;
	tmp |=  0x00000010;
	snd_mychip_pokeBA1(chip, BA1_PFIE, tmp);     /* playback interrupt disable */

	tmp = snd_mychip_peekBA1(chip, BA1_CIE);
	tmp &= ~0x0000003f;
	tmp |=  0x00000011;
	snd_mychip_pokeBA1(chip, BA1_CIE, tmp); /* capture interrupt disable */

	/*
	 *  Stop playback DMA.
	 */
	tmp = snd_mychip_peekBA1(chip, BA1_PCTL);
	snd_mychip_pokeBA1(chip, BA1_PCTL, tmp & 0x0000ffff);

	/*
	 *  Stop capture DMA.
	 */
	tmp = snd_mychip_peekBA1(chip, BA1_CCTL);
	snd_mychip_pokeBA1(chip, BA1_CCTL, tmp & 0xffff0000);

	/*
	 *  Reset the processor.
	 */
	snd_mychip_reset(chip);

	snd_mychip_proc_stop(chip);

	/*
	 *  Power down the PLL.
	 */
	snd_mychip_pokeBA0(chip, BA0_CLKCR1, 0);

	/*
	 *  Turn off the Processor by turning off the software clock enable flag in 
	 *  the clock control register.
	 */
	tmp = snd_mychip_peekBA0(chip, BA0_CLKCR1) & ~CLKCR1_SWCE;
	snd_mychip_pokeBA0(chip, BA0_CLKCR1, tmp);
}
static int __exit snd_mychip_free(struct snd_mychip *chip){
	int idx;
	struct snd_mychip_region *region;
	if (chip->ba0_region.idx[0].resource)
		snd_mychip_hw_stop(chip);

	if (chip->irq >= 0){
		free_irq(chip->irq, chip);
	}

	for (idx = 0; idx < 5; idx++) {
		if(idx == 0) region = &chip->ba0_region.idx[0];
		else region = &chip->ba1_region.idx[idx-1];
		if (region->remap_addr)
			iounmap(region->remap_addr);
		release_and_free_resource(region->resource);
	}

	//disable PCI入口
	pci_disable_device(chip->pci);
	//释放内存
	kfree(chip);
	return 0;
}

static int snd_mychip_dev_free(struct snd_device *device)
{
	struct snd_mychip *chip = device->device_data;

	return snd_mychip_free(chip);
}


static irqreturn_t snd_mychip_interrupt(int irq, void *dev_id)
{
	struct snd_mychip *chip = dev_id;
	unsigned int status;

	if (chip == NULL)
		return IRQ_NONE;

	/*
	 *  Read the Interrupt Status Register to clear the interrupt
	 */
	status = snd_mychip_peekBA0(chip, BA0_HISR);
	if ((status & 0x7fffffff) == 0) {
		snd_mychip_pokeBA0(chip, BA0_HICR, HICR_CHGM | HICR_IEV);
		return IRQ_NONE;
	}
	//snd_printk(KERN_ERR "mychip: status = %x\n", status);
	/* old dsp */
	if ((status & HISR_VC0) && chip->play) {
		if (chip->play->substream){
			snd_pcm_period_elapsed(chip->play->substream);
		}
	}
	if ((status & HISR_VC1) && chip->capt) {
		if (chip->capt->substream){
			snd_pcm_period_elapsed(chip->capt->substream);
		}
	}


	// TODO midi handler 

	/* EOI to the PCI part... reenables interrupts */
	snd_mychip_pokeBA0(chip, BA0_HICR, HICR_CHGM | HICR_IEV);

	return IRQ_HANDLED;
}
static int mychip_wait_for_fifo(struct snd_mychip * chip,int retry_timeout) {
	u32 i, status = 0;
	/*
	 * Make sure the previous FIFO write operation has completed.
	 */
	for(i = 0; i < 50; i++){
		status = snd_mychip_peekBA0(chip, BA0_SERBST);

		if( !(status & SERBST_WBSY) )
			break;

		mdelay(retry_timeout);
	}

	if(status & SERBST_WBSY) {
		snd_printk(KERN_ERR "mychip: failure waiting for "
				"FIFO command to complete\n");
		return -EINVAL;
	}

	return 0;
}

static void snd_mychip_clear_serial_FIFOs(struct snd_mychip *chip){
	int idx, powerdown = 0;
	unsigned int tmp;

	/*
	 *  See if the devices are powered down.  If so, we must power them up first
	 *  or they will not respond.
	 */
	tmp = snd_mychip_peekBA0(chip, BA0_CLKCR1);
	if (!(tmp & CLKCR1_SWCE)) {
		snd_mychip_pokeBA0(chip, BA0_CLKCR1, tmp | CLKCR1_SWCE);
		powerdown = 1;
	}

	/*
	 *  We want to clear out the serial port FIFOs so we don't end up playing
	 *  whatever random garbage happens to be in them.  We fill the sample FIFOS
	 *  with zero (silence).
	 */
	snd_mychip_pokeBA0(chip, BA0_SERBWP, 0);

	/*
	 *  Fill all 256 sample FIFO locations.
	 */
	for (idx = 0; idx < 0xFF; idx++) {
		/*
		 *  Make sure the previous FIFO write operation has completed.
		 */
		if (mychip_wait_for_fifo(chip,1)) {
			snd_printdd ("failed waiting for FIFO at addr (%02X)\n",idx);

			if (powerdown)
				snd_mychip_pokeBA0(chip, BA0_CLKCR1, tmp);

			break;
		}
		/*
		 *  Write the serial port FIFO index.
		 */
		snd_mychip_pokeBA0(chip, BA0_SERBAD, idx);
		/*
		 *  Tell the serial port to load the new value into the FIFO location.
		 */
		snd_mychip_pokeBA0(chip, BA0_SERBCM, SERBCM_WRC);
	}
	/*
	 *  Now, if we powered up the devices, then power them back down again.
	 *  This is kinda ugly, but should never happen.
	 */
	if (powerdown)
		snd_mychip_pokeBA0(chip, BA0_CLKCR1, tmp);
}

static int snd_mychip_init(struct snd_mychip *chip){
	int timeout;
	/* 
	 *  First, blast the clock control register to zero so that the PLL starts
	 *  out in a known state, and blast the master serial port control register
	 *  to zero so that the serial ports also start out in a known state.
	 */
	snd_mychip_pokeBA0(chip, BA0_CLKCR1, 0);
	snd_mychip_pokeBA0(chip, BA0_SERMC1, 0);

	/*
	 *  If we are in AC97 mode, then we must set the part to a host controlled
	 *  AC-link.  Otherwise, we won't be able to bring up the link.
	 */        

	snd_mychip_pokeBA0(chip, BA0_SERACC, SERACC_HSP | SERACC_CHIP_TYPE_1_03); /* 1.03 codec */


	/*
	 *  Drive the ARST# pin low for a minimum of 1uS (as defined in the AC97
	 *  spec) and then drive it high.  This is done for non AC97 modes since
	 *  there might be logic external to the CS461x that uses the ARST# line
	 *  for a reset.
	 */
	snd_mychip_pokeBA0(chip, BA0_ACCTL, 0);

	udelay(50);
	snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_RSTN);

	/*
	 *  The first thing we do here is to enable sync generation.  As soon
	 *  as we start receiving bit clock, we'll start producing the SYNC
	 *  signal.
	 */
	snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_ESYN | ACCTL_RSTN);

	/*
	 *  Now wait for a short while to allow the AC97 part to start
	 *  generating bit clock (so we don't try to start the PLL without an
	 *  input clock).
	 */
	mdelay(10);

	/*
	 *  Set the serial port timing configuration, so that
	 *  the clock control circuit gets its clock from the correct place.
	 */
	snd_mychip_pokeBA0(chip, BA0_SERMC1, SERMC1_PTC_AC97);

	/*
	 *  Write the selected clock control setup to the hardware.  Do not turn on
	 *  SWCE yet (if requested), so that the devices clocked by the output of
	 *  PLL are not clocked until the PLL is stable.
	 */
	snd_mychip_pokeBA0(chip, BA0_PLLCC, PLLCC_LPF_1050_2780_KHZ | PLLCC_CDR_73_104_MHZ);
	snd_mychip_pokeBA0(chip, BA0_PLLM, 0x3a);
	snd_mychip_pokeBA0(chip, BA0_CLKCR2, CLKCR2_PDIVS_8);

	/*
	 *  Power up the PLL.
	 */
	snd_mychip_pokeBA0(chip, BA0_CLKCR1, CLKCR1_PLLP);

	/*
	 *  Wait until the PLL has stabilized.
	 */
	msleep(100);

	/*
	 *  Turn on clocking of the core so that we can setup the serial ports.
	 */
	snd_mychip_pokeBA0(chip, BA0_CLKCR1, CLKCR1_PLLP | CLKCR1_SWCE);

	/*
	 * Enable FIFO  Host Bypass
	 */
	snd_mychip_pokeBA0(chip, BA0_SERBCF, SERBCF_HBP);

	/*
	 *  Fill the serial port FIFOs with silence.
	 */
	snd_mychip_clear_serial_FIFOs(chip);

	/*
	 *  Set the serial port FIFO pointer to the first sample in the FIFO.
	 */
	/* snd_mychip_pokeBA0(chip, BA0_SERBSP, 0); */

	/*
	 *  Write the serial port configuration to the part.  The master
	 *  enable bit is not set until all other values have been written.
	 */
	snd_mychip_pokeBA0(chip, BA0_SERC1, SERC1_SO1F_AC97 | SERC1_SO1EN);
	snd_mychip_pokeBA0(chip, BA0_SERC2, SERC2_SI1F_AC97 | SERC1_SO1EN);
	snd_mychip_pokeBA0(chip, BA0_SERMC1, SERMC1_PTC_AC97 | SERMC1_MSPE);

	mdelay(5);


	/*
	 * Wait for the codec ready signal from the AC97 codec.
	 */
	timeout = 150;
	while (timeout-- > 0) {
		/*
		 *  Read the AC97 status register to see if we've seen a CODEC READY
		 *  signal from the AC97 codec.
		 */
		if (snd_mychip_peekBA0(chip, BA0_ACSTS) & ACSTS_CRDY)
			goto ok1;
		msleep(10);
	}


	snd_printk(KERN_ERR "create - never read codec ready from AC'97\n");
	snd_printk(KERN_ERR "it is not probably bug, try to use CS4236 driver\n");
	return -EIO;
ok1:

	/*
	 *  Assert the vaid frame signal so that we can start sending commands
	 *  to the AC97 codec.
	 */
	snd_mychip_pokeBA0(chip, BA0_ACCTL, ACCTL_VFRM | ACCTL_ESYN | ACCTL_RSTN);


	/*
	 *  Wait until we've sampled input slots 3 and 4 as valid, meaning that
	 *  the codec is pumping ADC data across the AC-link.
	 */
	timeout = 150;
	while (timeout-- > 0) {
		/*
		 *  Read the input slot valid register and see if input slots 3 and
		 *  4 are valid yet.
		 */
		if ((snd_mychip_peekBA0(chip, BA0_ACISV) & (ACISV_ISV3 | ACISV_ISV4)) == (ACISV_ISV3 | ACISV_ISV4))
			goto ok2;
		msleep(10);
	}

	/* This may happen on a cold boot with a Terratec SiXPack 5.1.
	   Reloading the driver may help, if there's other soundcards 
	   with the same problem I would like to know. (Benny) */

	snd_printk(KERN_ERR "ERROR: snd-mychip: never read ISV3 & ISV4 from AC'97\n");
	snd_printk(KERN_ERR "       Try reloading the ALSA driver, if you find something\n");
	snd_printk(KERN_ERR "       broken or not working on your soundcard upon\n");
	snd_printk(KERN_ERR "       this message please report to alsa-devel@alsa-project.org\n");

	return -EIO;

ok2:

	/*
	 *  Now, assert valid frame and the slot 3 and 4 valid bits.  This will
	 *  commense the transfer of digital audio data to the AC97 codec.
	 */

	snd_mychip_pokeBA0(chip, BA0_ACOSV, ACOSV_SLV3 | ACOSV_SLV4);


	/*
	 *  Power down the DAC and ADC.  We will power them up (if) when we need
	 *  them.
	 */
	/* snd_mychip_pokeBA0(chip, BA0_AC97_POWERDOWN, 0x300); */

	/*
	 *  Turn off the Processor by turning off the software clock enable flag in 
	 *  the clock control register.
	 */
	/* tmp = snd_mychip_peekBA0(chip, BA0_CLKCR1) & ~CLKCR1_SWCE; */
	/* snd_mychip_pokeBA0(chip, BA0_CLKCR1, tmp); */

	return 0;
}
static void snd_mychip_enable_stream_irqs(struct snd_mychip *chip)
{
	unsigned int tmp;

	snd_mychip_pokeBA0(chip, BA0_HICR, HICR_IEV | HICR_CHGM);

	tmp = snd_mychip_peekBA1(chip, BA1_PFIE);
	tmp &= ~0x0000f03f;
	snd_mychip_pokeBA1(chip, BA1_PFIE, tmp);     /* playback interrupt enable */

	tmp = snd_mychip_peekBA1(chip, BA1_CIE);
	tmp &= ~0x0000003f;
	tmp |=  0x00000001;
	snd_mychip_pokeBA1(chip, BA1_CIE, tmp); /* capture interrupt enable */
}

int __init snd_mychip_start_dsp(struct snd_mychip *chip)
{    
	unsigned int tmp;
	/*
	 *  Reset the processor.
	 */
	snd_mychip_reset(chip);
	/*
	 *  Download the image to the processor.
	 */
	/* old image */
	if (snd_mychip_download_image(chip) < 0) {
		snd_printk(KERN_ERR "image download error\n");
		return -EIO;
	}
	FUNC_LOG();
	/*
	 *  Stop playback DMA.
	 */
	tmp = snd_mychip_peekBA1(chip, BA1_PCTL);
	chip->play_ctl = tmp & 0xffff0000;
	snd_mychip_pokeBA1(chip, BA1_PCTL, tmp & 0x0000ffff);

	FUNC_LOG();
	/*
	 *  Stop capture DMA.
	 */
	tmp = snd_mychip_peekBA1(chip, BA1_CCTL);
	chip->capt_ctl = tmp & 0x0000ffff;
	snd_mychip_pokeBA1(chip, BA1_CCTL, tmp & 0xffff0000);

	mdelay(5);

	snd_mychip_set_play_sample_rate(chip, 8000);
	snd_mychip_set_capture_sample_rate(chip, 8000);

	snd_mychip_proc_start(chip);

	snd_mychip_enable_stream_irqs(chip);


	return 0;
}



//__devinit在linux3.8以上内核中去掉了,所以用__init
static int __init snd_mychip_create(struct snd_card *card, struct pci_dev *pci, struct snd_mychip **rchip){
	struct snd_mychip *chip;
	struct snd_mychip_region *region;
	int err, i;
	static struct snd_device_ops ops = {
		.dev_free = snd_mychip_dev_free,
	};
	*rchip = NULL;

	//enable pci入口
	//在PCI 设备驱动中，在分配资源之前先要调用pci_enable_device().同样，你也要设定
	//合适的PCI DMA mask 限制i/o接口的范围。在一些情况下，你也需要设定pci_set_master().
	if ((err = pci_enable_device(pci)) < 0){
		return err;
	}

	//检查PCI是否可用，设置28bit DMA
	// DMA_BIT_MASK(28)  代替 DMA_28BIT_MASK
	// if (pci_set_dma_mask(pci, DMA_BIT_MASK(28)) < 0 ||
	//        pci_set_consistent_dma_mask(pci,DMA_BIT_MASK(28)) < 0 ){
	//   FUNC_LOG();
	//        pci_disable_device(pci);
	//   return -ENXIO;
	// }

	//分配内存，并初始化为0
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL){
		pci_disable_device(pci);
		return -ENOMEM;
	}

	spin_lock_init(&chip->reg_lock);

	chip->card = card;
	chip->pci = pci;
	chip->irq = -1;


	//(1) 初始化PCI资源
	// I/O端口的分配

	//chip->port = pci_resource_start(pci, 0);
	chip->ba0_addr = pci_resource_start(pci, 0);
	chip->ba1_addr = pci_resource_start(pci, 1);
	pci_set_master(pci);

	FUNC_LOG();

	if (!chip->ba0_addr || !chip->ba1_addr) {
		snd_printk(KERN_ERR "wrong address(es) - ba0 = 0x%lx, ba1 = 0x%lx\n",
				chip->ba0_addr, chip->ba1_addr);
		snd_mychip_free(chip);
		return -ENOMEM;
	}

	region = &chip->ba0_region.name.ba0;
	strcpy(region->name, "MYCHIP_BA0");
	region->base = chip->ba0_addr;
	region->size = MYCHIP_BA0_SIZE;
	if ((region->resource = request_mem_region(region->base, region->size, region->name)) == NULL){
		snd_printk(KERN_ERR "unable to request memory region 0x%lx-0x%lx\n", region->base, region->base + region->size - 1);
		snd_mychip_free(chip);
		return -EBUSY;
	}
	region->remap_addr = ioremap_nocache(region->base, region->size);
	if (region->remap_addr == NULL) {
		snd_printk(KERN_ERR "%s ioremap problem\n", region->name);
		snd_mychip_free(chip);
		return -ENOMEM;
	}

	region = &chip->ba1_region.name.data0;
	strcpy(region->name, "MYCHIP_BA1_data0");
	region->base = chip->ba1_addr + BA1_SP_DMEM0;
	region->size = MYCHIP_BA1_DATA0_SIZE;

	region = &chip->ba1_region.name.data1;
	strcpy(region->name, "MYCHIP_BA1_data1");
	region->base = chip->ba1_addr + BA1_SP_DMEM1;
	region->size = MYCHIP_BA1_DATA1_SIZE;

	region = &chip->ba1_region.name.pmem;
	strcpy(region->name, "MYCHIP_BA1_pmem");
	region->base = chip->ba1_addr + BA1_SP_PMEM;
	region->size = MYCHIP_BA1_PRG_SIZE;

	region = &chip->ba1_region.name.reg;
	strcpy(region->name, "MYCHIP_BA1_reg");
	region->base = chip->ba1_addr + BA1_SP_REG;
	region->size = MYCHIP_BA1_REG_SIZE;

	for(i = 0; i < 4; i++){
		region = &chip->ba1_region.idx[i];
		if ((region->resource = request_mem_region(region->base, region->size, region->name)) == NULL){
			snd_printk(KERN_ERR "unable to request memory region 0x%lx-0x%lx\n", region->base, region->base + region->size - 1);
			snd_mychip_free(chip);
			return -EBUSY;
		}
		region->remap_addr = ioremap_nocache(region->base, region->size);
		if (region->remap_addr == NULL) {
			snd_printk(KERN_ERR "%s ioremap problem\n", region->name);
			snd_mychip_free(chip);
			return -ENOMEM;
		}
	}

	// 分配一个中断源
	if (request_irq(pci->irq, snd_mychip_interrupt, IRQF_SHARED, "My Chip",chip)){
		FUNC_LOG();
		snd_printk(KERN_ERR "unable to grab IRQ %d\n", pci->irq);
		snd_mychip_free(chip);
		return -EBUSY;
	}
	chip->irq = pci->irq;

	//(2) chip hardware的初始化

	if ((err = snd_mychip_init(chip))) {
		FUNC_LOG();
		snd_mychip_free(chip);
		return err;
	}

	//(3) 关联到card中
	//对于大部分的组件来说，device_level已经定义好了。对于用户自定义的组件，你可以用SNDRV_DEV_LOWLEVEL
	//然后，把芯片的专有数据注册为声卡的一个低阶设备

	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0) {
		snd_mychip_free(chip);
		return err;
	}

	snd_card_set_dev(card, &pci->dev);
	*rchip = chip;
	FUNC_LOG();
	return 0;
}


static int __init snd_mychip_probe(struct pci_dev *pci, const struct pci_device_id *pci_id){

	struct snd_card *card;
	struct snd_pcm *pcm;
	struct snd_mychip *chip;
	int err;

	static int dev;
	//(1) 检查设备索引
	if (dev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[dev]) {
		dev ++;
		return -ENOENT;
	}

	//(2) 创建声卡设备实例
	//snd_card可以说是整个ALSA音频驱动最顶层的一个结构
	//snd_card_new(linux 2.6.22以上被snd_card_create代替)
	err = snd_card_create(index[dev], id[dev], THIS_MODULE, 0, &card);
	if(err < 0){
		return err;
	}

	FUNC_LOG();

	//(3) 创建声卡芯片专用的资源组件
	// 声明为mychip，例如中断资源、io资源、dma资源等。
	if ((err = snd_mychip_create(card, pci, &chip)) < 0){
		snd_card_free(card);
		return err;
	}
	card->private_data = chip;

	/* enable PCI device */
	if ((err = pci_enable_device(pci)) < 0){
		return err;
	}

	FUNC_LOG();

	//(4)，设置Driver的ID和名字
	//驱动的一些结构变量保存chip 的ID字串。它会在alsa-lib 配置的时候使用，所以要保证他的唯一和简单。
	//甚至一些相同的驱动可以拥有不同的 ID,可以区别每种chip类型的各种功能。
	//shortname域是一个更详细的名字。longname域将会在/proc/asound/cards中显示。
	strcpy(card->driver,"My Chip");
	strcpy(card->shortname,"My Own Chip 123");
	sprintf(card->longname,"%s at 0x%lx/0x%lx irq %i",
			card->shortname, chip->ba0_addr, chip->ba1_addr, chip->irq);

	//(5)，创建声卡的功能部件（逻辑设备），例如PCM，Mixer，MIDI等
	//创建pcm设备

	if((err = snd_mychip_new_pcm(chip)) < 0){
		snd_card_free(card);
		return err;
	}

	//创建mixer control设备
	if((err = snd_mychip_new_mixer(chip)) < 0){
		snd_card_free(card);
		return err;
	}


	printk(KERN_EMERG "cs4624: snd_control created, control = %p", &pcm);

	if ((err = snd_mychip_start_dsp(chip)) < 0) {
		snd_card_free(card);
		return err;
	}
	//(6) 注册card实例
	if((err = snd_card_register(card)) < 0){
		snd_card_free(card);  
		return err;  
	}

	//(7) 设定PCI驱动数据
	pci_set_drvdata(pci, card);
	dev ++;
	FUNC_LOG();
	return 0;
}

static void __exit snd_mychip_remove(struct pci_dev *dev){
	struct snd_card *card = pci_get_drvdata(dev);
	if(card){
		snd_card_free(card);
		pci_set_drvdata(dev, NULL);
	}
}


//----------------------mydriver---------------------------
struct pci_driver mydriver={
	.name = KBUILD_MODNAME,
	.id_table = snd_mychip_ids,
	.probe = snd_mychip_probe,
	.remove = snd_mychip_remove,
};

static int __init pci_mydriver_init(void){
	FUNC_LOG();
	return pci_register_driver(&mydriver);
}


static void __exit pci_mydriver_exit(void){
	FUNC_LOG();
	pci_unregister_driver(&mydriver);
}



module_init(pci_mydriver_init);
module_exit(pci_mydriver_exit);


// TODO CAPTURE


