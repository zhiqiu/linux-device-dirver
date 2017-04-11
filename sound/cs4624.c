//ALSA声卡驱动
//参考http://blog.csdn.net/droidphone

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <sound/core.h>
#include <sound/initval.h>
#include <sound/control.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

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


#define CS46XX_MIN_PERIOD_SIZE 64
#define CS46XX_MAX_PERIOD_SIZE 1024*1024
#define CS46XX_FRAGS 2

//每个card 设备可以最多拥有4个 pcm 实例。一个pcm实例对应予一个pcm设备文件。
//根据硬件手册定义硬件
static struct snd_pcm_hardware snd_mycard_playback =
{
	.info =			(SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED | 
			SNDRV_PCM_INFO_BLOCK_TRANSFER /*|*/
			/*SNDRV_PCM_INFO_RESUME*/),
	.formats =		(SNDRV_PCM_FMTBIT_S8 | SNDRV_PCM_FMTBIT_U8 |
			SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S16_BE |
			SNDRV_PCM_FMTBIT_U16_LE | SNDRV_PCM_FMTBIT_U16_BE),
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		5500,
	.rate_max =		48000,
	.channels_min =		1,
	.channels_max =		2,
	.buffer_bytes_max =	(256 * 1024),
	.period_bytes_min =	CS46XX_MIN_PERIOD_SIZE,
	.period_bytes_max =	CS46XX_MAX_PERIOD_SIZE,
	.periods_min =		CS46XX_FRAGS,
	.periods_max =		1024,
	.fifo_size =		0,
};

static struct snd_pcm_hardware snd_mycard_capture =
{
	.info =			(SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_BLOCK_TRANSFER /*|*/
			/*SNDRV_PCM_INFO_RESUME*/),
	.formats =		SNDRV_PCM_FMTBIT_S16_LE,
	.rates =		SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_8000_48000,
	.rate_min =		5500,
	.rate_max =		48000,
	.channels_min =		2,
	.channels_max =		2,
	.buffer_bytes_max =	(256 * 1024),
	.period_bytes_min =	CS46XX_MIN_PERIOD_SIZE,
	.period_bytes_max =	CS46XX_MAX_PERIOD_SIZE,
	.periods_min =		CS46XX_FRAGS,
	.periods_max =		1024,
	.fifo_size =		0,
};

// 关于peroid的概念有这样的描述：The “period” is a term that corresponds to a fragment in the OSS world. The period defines the size at which a PCM interrupt is generated. 
// peroid的概念很重要
static unsigned int period_sizes[] = { 64, 128, 256, 512, 1024, 2048, 4096, 8192 };

static struct snd_pcm_hw_constraint_list hw_constraints_period_sizes = {
	.count = ARRAY_SIZE(period_sizes),
	.list = period_sizes,
	.mask = 0
};

//open函数为PCM模块设定支持的传输模式、数据格式、通道数、period等参数，并为playback/capture stream分配相应的DMA通道。
static int snd_mycard_playback_open(struct snd_pcm_substream *substream){
	//	struct snd_soc_pcm_runtime *rtd = substream->private_data;  
	// snd_pcm_runtime是pcm运行时的信息。
	// 当打开一个pcm子流时，pcm运行时实例就会分配给这个子流。
	// 它拥有很多信息：hw_params和sw_params配置拷贝，缓冲区指针，mmap记录，自旋锁等。snd_pcm_runtime对于驱动程序操作集函数是只读的，仅pcm中间层可以改变或更新这些信息。
	//	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;  
	struct snd_pcm_runtime *runtime = substream->runtime;  
	//	struct audio_stream_a *s = runtime->private_data;  
	int res;  

	//if (!cpu_dai->active) {  
	//    audio_dma_request(&s[0], audio_dma_callback); //为playback stream分配DMA  
	//    audio_dma_request(&s[1], audio_dma_callback); //为capture stream分配DMA  
	//}  

	//设定runtime硬件参数  
	runtime->hw = snd_mycard_playback;

	/* Ensure that buffer size is a multiple of period size */  
	res = snd_pcm_hw_constraint_list(runtime, 0,
			SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 
			&hw_constraints_period_sizes);

	return res;  
}

//close函数，停止dma，释放数据
static int snd_mycard_playback_close(struct snd_pcm_substream *substream){
	return 0;  
}

//hw_params函数为substream（每打开一个playback或capture，ALSA core均产生相应的一个substream）设定DMA的源（目的）地址，以及DMA缓冲区的大小。
static int snd_mycard_playback_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params){
	int err;
	err = snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
	return err;  
}

//hw_params函数为substream（每打开一个playback或capture，ALSA core均产生相应的一个substream）设定DMA的源（目的）地址，以及DMA缓冲区的大小。
static int snd_mycard_playback_hw_free(struct snd_pcm_substream *substream){
	int err;
	err = snd_pcm_lib_free_pages(substream);
	return err;  
}

// 当pcm“准备好了”调用该函数。在这里根据channels、buffer_bytes等来设定DMA传输参数()，跟具体硬件平台相关。
// 注：每次调用snd_pcm_prepare()的时候均会调用prepare函数。
static int snd_mycard_playback_prepare(struct snd_pcm_substream *substream){
	// 
	return 0;  
}
// 当pcm开始、停止、暂停的时候都会调用trigger函数。
// Trigger函数里面的操作应该是原子的，不要在调用这些操作时进入睡眠，trigger函数应尽量小，甚至仅仅是触发DMA。
static int snd_mycard_playback_trigger(struct snd_pcm_substream *substream, int cmd){

	//struct runtime_data *prtd = substream->runtime->private_data;  
	int res = 0;  

	//spin_lock(&prtd->lock);  

	switch (cmd) {  
		case SNDRV_PCM_TRIGGER_START:  
		case SNDRV_PCM_TRIGGER_RESUME:  
		case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:  
			//prtd->state |= ST_RUNNING;  
			//dma_ctrl(prtd->params->channel, DMAOP_START); //DMA开启  
			break;  

		case SNDRV_PCM_TRIGGER_STOP:  
		case SNDRV_PCM_TRIGGER_SUSPEND:  
		case SNDRV_PCM_TRIGGER_PAUSE_PUSH:  
			//prtd->state &= ~ST_RUNNING;  
			//dma_ctrl(prtd->params->channel, DMAOP_STOP); //DMA停止  
			break;  

		default:  
			res = -EINVAL;  
			break;  
	}  

	//spin_unlock(&prtd->lock);  

	return res;  

}

static struct snd_pcm_ops snd_mycard_playback_ops = {
	.open = snd_mycard_playback_open,
	.close = snd_mycard_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params =    snd_mycard_playback_hw_params,
	.hw_free =              snd_mycard_playback_hw_free,
	.prepare =              snd_mycard_playback_prepare,
	.trigger =              snd_mycard_playback_trigger,
	//        .pointer =              snd_mycard_playback_direct_pointer,
};

static struct snd_pcm_ops snd_mycard_capture_ops = {
	.open = snd_mycard_playback_open,
	.close = snd_mycard_playback_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params =    snd_mycard_playback_hw_params,
	.hw_free =              snd_mycard_playback_hw_free,
	.prepare =              snd_mycard_playback_prepare,
	.trigger =              snd_mycard_playback_trigger,
	//        .pointer =              snd_mycard_playback_direct_pointer,
};

//info回调函数用于获取control的详细信息。它的主要工作就是填充通过参数传入的snd_ctl_elem_info对象，以下例子是一个具有单个元素的boolean型control的info回调
static int snd_my_ctl_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo){  
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;    //type字段指出该control的值类型，值类型可以是BOOLEAN, INTEGER, ENUMERATED, BYTES,IEC958和INTEGER64之一
	uinfo->count = 1;		//count字段指出了改control中包含有多少个元素单元，比如，立体声的音量control左右两个声道的音量值，它的count字段等于2。
	uinfo->value.integer.min = 0;    //value字段是一个联合体（union），value的内容和control的类型有关。可以指定最大、最小和步长。
	uinfo->value.integer.max = 100;
	uinfo->value.integer.step = 1; 
	return 0;  
}  

// 从volume寄存器读数据，放到ucontrol->value.integer.value[0]中
static int snd_my_ctl_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol){
	struct mychip *chip = snd_kcontrol_chip(kcontrol);
	//ucontrol->value.integer.value[0] = get_some_value(chip);
	//如果private字段这样设定的 .private_value = reg | (shift << 16) | (mask << 24);
	//int reg = kcontrol->private_value & 0xff;
	//int shift = (kcontrol->private_value >> 16) & 0xff;
	//int mask = (kcontrol->private_value >> 24) & 0xff;

	return 0; 
}

// 从volume寄存器写数据
// 当control的值被改变时，put回调必须要返回1，如果值没有被改变，则返回0。如果发生了错误，则返回一个负数的错误号。
static int snd_my_ctl_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol){
	struct mychip *chip = snd_kcontrol_chip(kcontrol);
	int changed = 0;  
	//if (chip->current_value !=  
	//    ucontrol->value.integer.value[0]) {  
	//    change_current_value(chip, ucontrol->value.integer.value[0]);  
	//    changed = 1;  
	//}  
	return changed;  
}  
static struct snd_kcontrol_new snd_mycard_control = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,  //iface字段指出了control的类型
	.name = "mycard_control",     //因为control的作用是按名字来归类的
	.index = 0,    //index字段用于保存该control的在该卡中的编号如果声卡中有不止一个codec，每个codec中有相同名字的control，这时我们可以通过index来区分这些controls。当
	//index为0时，则可以忽略这种区分策略。
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,   //access字段包含了该control的访问类型。每一个bit代表一种访问类型，这些访问类型可以多个“或”运算组合在一起。未定义（.access==0），此时也认为是READWRITE类型。
	.private_value = 0xffff,    //private_value字段包含了一个任意的长整数类型值。该值可以通过info，get，put这几个回调函数访问。你可以自己决定如何使用该字段，例如可以
	//把它拆分成多个位域，又或者是一个指针，指向某一个数据结构。
	.info = snd_my_ctl_info,    // 音量信息
	.get = snd_my_ctl_get,      //      读音量
	.put = snd_my_ctl_put,               //      写音量
};



struct mychip{
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct pci_dev *pci;
	unsigned long port;
	int irq;
};

static int __devexit snd_mychip_free(struct mychip *chip){
	if (chip->irq >= 0){
		free_irq(chip->irq, chip);
	}

	//释放io 和memory
	pci_release_regions(chip->pci);

	//disable PCI入口
	pci_disable_device(chip->pci);

	//释放内存
	kfree(chip);
}

static irqreturn_t snd_mychip_interrup(int irq, void *dev_id){
	struct mychip *chip = dev_id;
	return IRQ_HANDLED;
}

static int __devinit snd_mychip_create(struct snd_card *card, struct pci_dev *pci, struct mychip **rchip){
	struct mychip *chip;
	int err;
	static struct snd_device_ops ops = {
		.dev_freee = snd_mychip_dev_free,
	};
	*rchip = NULL;

	//enable pci入口
	//在PCI 设备驱动中，在分配资源之前先要调用pci_enable_device().同样，你也要设定
	//合适的PCI DMA mask 限制i/o接口的范围。在一些情况下，你也需要设定pci_set_master().
	if ((err = pci_enable_device(pci)) < 0){
		return err;
	}

	//检查PCI是否可用，设置28bit DMA
	if (pci_set_dma_mask(pci, DMA_28BIT_MASK) < 0 ||
			pci_set_consistent_dma_mask(pci,DMA_28BIT_MASK) < 0 ){
		FUNC_LOG();
 		pci_disable_device(pci);
		return -ENXIO;
	}

	//分配内存，并初始化为0
	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (chip == NULL){
		pci_disable_device(pci);
		return -ENOMEM;
	}

	chip->card = card;
	chip->pci = pci;
	chip->irq = -1;

	//(1) 初始化PCI资源
	// I/O端口的分配
	if ((err = pci_request_regions(pci, "My Chip")) < 0){
		kfree(chip);
		pci_disable_device(pci);
		return err;
	}
	chip->port = pci_resource_start(pci, 0);
	// 分配一个中断源
	if (request_irq(pci->irq, snd_mychip_interrupt,
			IRQF_SHARED, "My Chip",chip){
		FUNC_LOG();
		snd_mychip_free(chip);
		return -EBUSY;
	}
	chip->irq = pci->irq;

	//(2) chip hardware的初始化

	//(3) 关联到card中
	//对于大部分的组件来说，device_level已经定义好了。对于用户自定义的组件，你可以用SNDRV_DEV_LOWLEVEL
	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0) {
		snd_mychip_free(chip);
		return err;
	}
	snd_card_set_dev(card, &pci->dev);
	*rchip = chip;
	return 0;
}

//然后，把芯片的专有数据注册为声卡的一个低阶设备
//注册为低阶设备主要是为了当声卡被注销时，芯片专用数据所占用的内存可以被自动地释放。
static int snd_mychip_dev_free(struct snd_device *device)
{
	struct mychip *chip = device->device_data;

	return snd_mychip_free(chip);
}

static struct snd_device_ops mychip_dev_ops = {
	.dev_free = snd_mychip_dev_free,
};

int snd_mychip_init(struct mychip *chip){
	return 0;
}



static int __devinit snd_mychip_probe(struct pci_dev *pci, const struct pci_device_id *pci_id){
	
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct mychip *chip;
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
	//struct snd_card定义在include/sound/core.h中，
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
	chip->card = card;
	/* enable PCI device */

	if ((err = pci_enable_device(pci)) < 0){
		return err;
	}
	chip->pci = pci;
	//然后，把芯片的专有数据注册为声卡的一个低阶设备
	//注册为低阶设备主要是为了当声卡被注销时，芯片专用数据所占用的内存可以被自动地释放。
	
	snd_mychip_init(chip);

	FUNC_LOG();

	//(4)，设置Driver的ID和名字
	//驱动的一些结构变量保存chip 的ID字串。它会在alsa-lib 配置的时候使用，所以要保证他的唯一和简单。
	//甚至一些相同的驱动可以拥有不同的 ID,可以区别每种chip类型的各种功能。
	//shortname域是一个更详细的名字。longname域将会在/proc/asound/cards中显示。
	strcpy(card->driver,"My Chip");
	strcpy(card->shortname,"My Own Chip 123");
	sprintf(card->longname,"%s at 0x%1x irq %i",
		card->shortname, chip->ioport, chip->irq);

	//(5)，创建声卡的功能部件（逻辑设备），例如PCM，Mixer，MIDI等
	// //创建pcm设备
	// err = snd_pcm_new(card, "mycard_pcm", 0, 1, 0, &pcm);
	// if(err < 0){
	// 	snd_card_free(card);
	// 	return err;
	// }
	// pcm->private_data = chip;
	// chip->pcm = pcm;
	// strcpy(pcm->name, "CS4624");
	// snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_mycard_playback_ops);
	// //设置DMA buffer
	// snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
	// 		snd_dma_pci_data(chip->pci), 64*1024, 256*1024);


	// //创建mixer control设备
	// err = snd_ctl_add(card, snd_ctl_new1(&snd_mycard_control, chip));  
	// if (err < 0){
	// 	snd_card_free(card);
	// 	return err;
	// }
	// printk(KERN_EMERG "cs4624: snd_control created, control = %p", &pcm);

	//(6) 注册card实例
	if((err = snd_card_register(card)) < 0){
		snd_card_free(card);  
		return err;  
	}

	//(7) 设定PCI驱动数据
	pci_set_drvdata(pci, card);
	dev ++;
	return 0;
}

static int __devexit mycard_audio_remove(struct pci_dev *dev){
	struct snd_card *card = pci_get_drvdata(dev);
	if(card){
		snd_card_free(card);
		pci_set_drvdata(dev, NULL);
	}
	return 0;
}

struct pci_driver mydriver={
	.name = KBUILD_MODNAME,
	.id_table = snd_mychip_ids,
	.probe = snd_mychip_probe,
	.remove = mycard_audio_remove,
#ifdef CONFIG_PM
	//.syspend = mycard_audio_suspend,
	//.resume = mycard_audio_suspend,
#endif
};

static int __init pci_mydriver_init(void){
	FUNC_LOG();
	return pci_register_driver(&mydriver);
}


static void __init pci_mydriver_exit(void){
	FUNC_LOG();
	pci_unregister_driver(&mydriver);
}

module_init(pci_mydriver_init);
module_exit(pci_mydriver_exit);
