#ifndef __CS4624_H__
#define __CS4624_H__


struct mychip_dma_stream{
	struct snd_pcm_substream *stream;
	int rate;
	int stream_id;

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

	struct mychip_dma_stream dma_stream[2];
};

#endif