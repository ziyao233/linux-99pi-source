/*
 * Loongson SPI driver
 *
 * Copyright (C) 2017 Juxin Gao <gaojuxin@loongson.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/spi/spi.h>
#include <linux/pci.h>
#include <linux/of.h>
/*define spi register */
#define	SPCR	0x00
#define	SPSR	0x01
#define FIFO	0x02
#define	SPER	0x03
#define	PARA	0x04
#define	SPCS	0x04
#define	SFCS	0x05
#define	TIMI	0x06

#define PARA_MEM_EN	0x01
#define SPSR_SPIF	0x80
#define SPSR_WCOL	0x40
#define SPCR_SPE	0x40

extern unsigned long bus_clock;
struct ls_spi {
	struct work_struct	work;
	spinlock_t			lock;

	struct	list_head	msg_queue;
	struct	spi_master	*master;
	void	__iomem		*base;
	int cs_active;
	unsigned int hz;
	unsigned char spcr, sper, spsr;
	unsigned char para, sfcs, timi;
	struct workqueue_struct	*wq;
	unsigned int mode;
};

static inline int set_cs(struct ls_spi *ls_spi, struct spi_device  *spi, int val);

static void ls_spi_write_reg(struct ls_spi *spi,
		unsigned char reg, unsigned char data)
{
	writeb(data, spi->base +reg);
}

static char ls_spi_read_reg(struct ls_spi *spi,
		unsigned char reg)
{
	return readb(spi->base + reg);
}

static int ls_spi_update_state(struct ls_spi *ls_spi,struct spi_device *spi,
		struct spi_transfer *t)
{
	unsigned int hz;
	unsigned int div, div_tmp;
	unsigned int bit;
	unsigned long clk;
	unsigned char val;
	const char rdiv[12] = {0, 1, 4, 2, 3, 5, 6, 7, 8, 9, 10, 11};

	hz  = t ? t->speed_hz : spi->max_speed_hz;

	if (!hz)
		hz = spi->max_speed_hz;

	if ((hz && ls_spi->hz != hz) || ((spi->mode ^ ls_spi->mode) & (SPI_CPOL | SPI_CPHA))) {
		clk = 100000000;
		div = DIV_ROUND_UP(clk, hz);

		if (div < 2)
			div = 2;

		if (div > 4096)
			div = 4096;

		bit = fls(div) - 1;
		if ((1<<bit) == div)
			bit--;
		div_tmp = rdiv[bit];

		dev_dbg(&spi->dev, "clk = %ld hz = %d div_tmp = %d bit = %d\n",
				clk, hz, div_tmp, bit);

		ls_spi->hz = hz;
		ls_spi->spcr = div_tmp & 3;
		ls_spi->sper = (div_tmp >> 2) & 3;

		val = ls_spi_read_reg(ls_spi, SPCR);
		val &= ~0xc;
		if (spi->mode & SPI_CPOL)
		   val |= 8;
		if (spi->mode & SPI_CPHA)
		   val |= 4;
		ls_spi_write_reg(ls_spi, SPCR, (val & ~3) | ls_spi->spcr);
		val = ls_spi_read_reg(ls_spi, SPER);
		ls_spi_write_reg(ls_spi, SPER, (val & ~3) | ls_spi->sper);
		ls_spi->mode &= SPI_NO_CS;
		ls_spi->mode |= spi->mode;
	}

	return 0;
}



static int ls_spi_setup(struct spi_device *spi)
{
	struct ls_spi *ls_spi;

	ls_spi = spi_master_get_devdata(spi->master);
	if (spi->bits_per_word %8)
		return -EINVAL;

	if(spi->chip_select >= spi->master->num_chipselect)
		return -EINVAL;

	ls_spi_update_state(ls_spi, spi, NULL);

	set_cs(ls_spi, spi, 1);

	return 0;
}

static int ls_spi_write_read_8bit( struct spi_device *spi,
		const u8 **tx_buf, u8 **rx_buf, unsigned int num)
{
	struct ls_spi *ls_spi;
	ls_spi = spi_master_get_devdata(spi->master);

	if (tx_buf && *tx_buf){
		ls_spi_write_reg(ls_spi, FIFO, *((*tx_buf)++));
		while((ls_spi_read_reg(ls_spi, SPSR) & 0x1) == 1);
	}else{
		ls_spi_write_reg(ls_spi, FIFO, 0);
		while((ls_spi_read_reg(ls_spi, SPSR) & 0x1) == 1);
	}

	if (rx_buf && *rx_buf) {
		*(*rx_buf)++ = ls_spi_read_reg(ls_spi, FIFO);
	}else{
		ls_spi_read_reg(ls_spi, FIFO);
	}

	return 1;
}


static unsigned int ls_spi_write_read(struct spi_device *spi, struct spi_transfer *xfer)
{
	struct ls_spi *ls_spi;
	unsigned int count;
	const u8 *tx = xfer->tx_buf;
	u8 *rx = xfer->rx_buf;

	ls_spi = spi_master_get_devdata(spi->master);
	count = xfer->len;

	do {
		if (ls_spi_write_read_8bit(spi, &tx, &rx, count) < 0)
			goto out;
		count--;
	} while (count);

out:
	return xfer->len - count;

}

static inline int set_cs(struct ls_spi *ls_spi, struct spi_device  *spi, int val)
{
	if (spi->mode  & SPI_CS_HIGH)
		val = !val;
	if (ls_spi->mode & SPI_NO_CS) {
		ls_spi_write_reg(ls_spi, SPCS, val);
	} else {
		int cs = ls_spi_read_reg(ls_spi, SFCS) & ~(0x11 << spi->chip_select);
		ls_spi_write_reg(ls_spi, SFCS, (val ? (0x11 << spi->chip_select):(0x1 << spi->chip_select)) | cs);
	}
	return 0;
}

static void ls_spi_work(struct work_struct *work)
{
	struct ls_spi *ls_spi =
		container_of(work, struct ls_spi, work);
	int param;

	spin_lock(&ls_spi->lock);
	param = ls_spi_read_reg(ls_spi, PARA);
	ls_spi_write_reg(ls_spi, PARA, param&~1);
	while (!list_empty(&ls_spi->msg_queue)) {

		struct spi_message *m;
		struct spi_device  *spi;
		struct spi_transfer *t = NULL;

		m = container_of(ls_spi->msg_queue.next, struct spi_message, queue);

		list_del_init(&m->queue);
		spin_unlock(&ls_spi->lock);

		spi = m->spi;

		/*in here set cs*/
		set_cs(ls_spi, spi, 0);

		list_for_each_entry(t, &m->transfers, transfer_list) {

			/*setup spi clock*/
			ls_spi_update_state(ls_spi, spi, t);

			if (t->len)
				m->actual_length +=
					ls_spi_write_read(spi, t);
		}

		set_cs(ls_spi, spi, 1);
		m->complete(m->context);


		spin_lock(&ls_spi->lock);
	}

	ls_spi_write_reg(ls_spi, PARA, param);
	spin_unlock(&ls_spi->lock);
}



static int ls_spi_transfer(struct spi_device *spi, struct spi_message *m)
{
	struct ls_spi	*ls_spi;
	struct spi_transfer *t = NULL;

	m->actual_length = 0;
	m->status		 = 0;
	if (list_empty(&m->transfers) || !m->complete)
		return -EINVAL;

	ls_spi = spi_master_get_devdata(spi->master);

	list_for_each_entry(t, &m->transfers, transfer_list) {

		if (t->tx_buf == NULL && t->rx_buf == NULL && t->len) {
			dev_err(&spi->dev,
					"message rejected : "
					"invalid transfer data buffers\n");
			goto msg_rejected;
		}

		/*other things not check*/

	}

	spin_lock(&ls_spi->lock);
	list_add_tail(&m->queue, &ls_spi->msg_queue);
	queue_work(ls_spi->wq, &ls_spi->work);
	spin_unlock(&ls_spi->lock);

	return 0;
msg_rejected:

	m->status = -EINVAL;
	if (m->complete)
		m->complete(m->context);
	return -EINVAL;
}

static void ls_spi_reginit(struct ls_spi *ls_spi_dev)
{
	unsigned char val;

	val = ls_spi_read_reg(ls_spi_dev, SPCR);
	val &= ~SPCR_SPE;
	ls_spi_write_reg(ls_spi_dev, SPCR, val);

	ls_spi_write_reg(ls_spi_dev, SPSR, (SPSR_SPIF | SPSR_WCOL));

	val = ls_spi_read_reg(ls_spi_dev, SPCR);
	val |= SPCR_SPE;
	ls_spi_write_reg(ls_spi_dev, SPCR, val);
}

static int ls_spi_init_master(struct device *dev, struct resource *res)
{
	struct spi_master *master;
	struct ls_spi *spi;
	int ret;

	master = spi_alloc_master(dev, sizeof(struct ls_spi));
	if (master == NULL) {
		dev_dbg(dev, "master allocation failed\n");
		return-ENOMEM;
	}

	master->mode_bits = SPI_CPOL | SPI_CPHA | SPI_CS_HIGH ;
	master->setup = ls_spi_setup;
	master->transfer = ls_spi_transfer;
	master->num_chipselect = 4;
#ifdef CONFIG_OF
	master->dev.of_node = of_node_get(dev->of_node);
#endif
	dev_set_drvdata(dev, master);

	spi = spi_master_get_devdata(master);

	spi->wq	= create_singlethread_workqueue("spi-ls");

	spi->base = ioremap(res->start, (res->end - res->start)+1);
	if (spi->base == NULL) {
		dev_err(dev, "Cannot map IO\n");
		ret = -ENXIO;
		goto unmap_io;
	}

	ls_spi_reginit(spi);

	spi->mode = 0;
	if (of_get_property(dev->of_node, "spi-nocs", NULL))
		spi->mode |= SPI_NO_CS;

	INIT_WORK(&spi->work, ls_spi_work);

	spin_lock_init(&spi->lock);
	INIT_LIST_HEAD(&spi->msg_queue);

	ret = spi_register_master(master);
	if (ret < 0)
		goto unmap_io;

	return ret;

unmap_io:
	iounmap(spi->base);
	kfree(master);
	spi_master_put(master);
	return ret;
}

static int ls_spi_platform_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *res;
	int ret;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(dev, "Cannot get IORESOURCE_MEM\n");
		return -ENOENT;
	}

	ret = ls_spi_init_master(dev, res);
	if (ret)
		dev_err(dev, "Init master failed\n");

	return ret;
}

#ifdef CONFIG_PM
static int ls_spi_suspend(struct device *dev)
{
	struct ls_spi *ls_spi;
	struct spi_master *master;

	master = dev_get_drvdata(dev);
	ls_spi = spi_master_get_devdata(master);

	ls_spi->spcr = ls_spi_read_reg(ls_spi, SPCR);
	ls_spi->sper = ls_spi_read_reg(ls_spi, SPER);
	ls_spi->spsr = ls_spi_read_reg(ls_spi, SPSR);
	ls_spi->para = ls_spi_read_reg(ls_spi, PARA);
	ls_spi->sfcs = ls_spi_read_reg(ls_spi, SFCS);
	ls_spi->timi = ls_spi_read_reg(ls_spi, TIMI);

#ifdef CONFIG_LOONGSON_MACH2K
	if (!(ls_spi->para & PARA_MEM_EN))
		ls_spi_write_reg(ls_spi, PARA, ls_spi->para | PARA_MEM_EN);
#endif

	return 0;
}

static int ls_spi_resume(struct device *dev)
{
	struct ls_spi *ls_spi;
	struct spi_master *master;

	master = dev_get_drvdata(dev);
	ls_spi = spi_master_get_devdata(master);

	ls_spi_write_reg(ls_spi, SPCR, ls_spi->spcr);
	ls_spi_write_reg(ls_spi, SPER, ls_spi->sper);
	ls_spi_write_reg(ls_spi, SPSR, ls_spi->spsr);
	ls_spi_write_reg(ls_spi, PARA, ls_spi->para);
	ls_spi_write_reg(ls_spi, SFCS, ls_spi->sfcs);
	ls_spi_write_reg(ls_spi, TIMI, ls_spi->timi);

	return 0;
}

static const struct dev_pm_ops ls_spi_dev_pm_ops = {
	.suspend	= ls_spi_suspend,
	.resume		= ls_spi_resume,
};

#define LS_DEV_PM_OPS (&ls_spi_dev_pm_ops)
#else
#define LS_DEV_PM_OPS NULL
#endif


#ifdef CONFIG_OF
static struct of_device_id ls_spi_id_table[] = {
	{ .compatible = "loongson,ls-spi", },
	{ },
};
MODULE_DEVICE_TABLE(of, ls_spi_id_table);
#endif
static struct platform_driver ls_spi_driver = {
	.probe = ls_spi_platform_probe,
	.driver	= {
		.name	= "ls-spi",
		.owner	= THIS_MODULE,
		.bus = &platform_bus_type,
		.pm = LS_DEV_PM_OPS,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(ls_spi_id_table),
#endif
	},
};

#ifdef CONFIG_PCI
static struct spi_master *ls7a_spi_master;

struct spi_master *get_ls7a_spi_master(void)
{
	return ls7a_spi_master;
}
EXPORT_SYMBOL(get_ls7a_spi_master);

static int ls_spi_pci_register(struct pci_dev *pdev,
                 const struct pci_device_id *ent)
{
	struct device *dev = &pdev->dev;
	struct resource res[2];
	unsigned char v8;
	int ret;

	/* Enable device in PCI config */
	ret = pci_enable_device(pdev);
	if (ret < 0) {
		dev_err(dev, "Cannot enable PCI device\n");
		goto err_out;
	}

	/* request the mem regions */
	ret = pci_request_region(pdev, 0, "ls-spi io");
	if (ret < 0) {
		dev_err(dev, "Cannot request region 0.\n");
		goto err_out;
	}

	res[0].start = pci_resource_start(pdev, 0);
	res[0].end = pci_resource_end(pdev, 0);
	/* need api from pci irq */
	ret = pci_read_config_byte(pdev, PCI_INTERRUPT_LINE, &v8);
	if (ret == PCIBIOS_SUCCESSFUL) {
		res[1].start = v8;
		res[1].end = v8;
	}

	ret = ls_spi_init_master(dev, res);
	if (!ret)
		ls7a_spi_master = dev_get_drvdata(dev);

err_out:
	return ret;
}

static void ls_spi_pci_unregister(struct pci_dev *pdev)
{
	pci_release_region(pdev, 0);
}

static struct pci_device_id ls_spi_devices[] = {
	{PCI_DEVICE(0x14, 0x7a0b)},
	{PCI_DEVICE(0x14, 0x7a1b)},
	{PCI_DEVICE(0x14, 0x7a2b)},
	{0, 0, 0, 0, 0, 0, 0}
};

static struct pci_driver ls_spi_pci_driver = {
	.name       = "ls-spi-pci",
	.id_table   = ls_spi_devices,
	.probe      = ls_spi_pci_register,
	.remove     = ls_spi_pci_unregister,
};
#endif


static int __init ls_spi_init(void)
{
	int ret;

	ret =  platform_driver_register(&ls_spi_driver);
#ifdef CONFIG_PCI
	if(!ret)
		ret = pci_register_driver(&ls_spi_pci_driver);
#endif
	return ret;
}

static void __exit ls_spi_exit(void)
{
	platform_driver_unregister(&ls_spi_driver);
#ifdef CONFIG_PCI
	pci_unregister_driver(&ls_spi_pci_driver);
#endif
}

subsys_initcall(ls_spi_init);
module_exit(ls_spi_exit);

MODULE_AUTHOR("Juxin Gao <gaojuxin@loongson.cn>");
MODULE_DESCRIPTION("Loongson SPI driver");
MODULE_LICENSE("GPL");
