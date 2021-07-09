#include <linux/init.h>
#include <linux/module.h> //Header file we must include with any LKM
#include <linux/kernel.h>
#include <linux/netdevice.h> //net_device
#include <linux/etherdevice.h> //eth_type_trans

#define SNULL_RX_INTR 0x0001
#define SNULL_TX_INTR 0x0002

struct snull_priv
{
    struct net_device_stats stats;
    int status;
    struct snull_packet *ppool;
    struct snull_packet *rx_queue;  /* List of incoming packets */
    int rx_int_enabled;             /* Interupts enable=1 disable=0 */
    int tx_packetlen;
    u8 *tx_packetdata;
    struct sk_buff *skb;
    spinlock_t lock;
};

struct snull_packet {
    struct snull_packet *next;
    struct net_device *dev;
    int datalen;
    u8 data[ETH_DATA_LEN];
};

static struct net_device *snull_devs[2];

int snull_open(struct net_device *dev)
{
    /* request_region(  ), request_irq(  ), ....  (like fops->open) */

    /*
     * Assign the hardware address of the board: use "\0SNULx", where
     * x is 0 or 1. The first byte is '\0' to avoid being a multicast
     * address (the first byte of multicast addrs is odd).
     */
    if (dev == snull_devs[0])
    {
        memcpy(dev->dev_addr, "\0SNUL0", ETH_ALEN);
    }
    else if (dev == snull_devs[1])
    {
        memcpy(dev->dev_addr, "\0SNUL1", ETH_ALEN);
    }
    netif_start_queue(dev);
    return 0;
}

int snull_release(struct net_device *dev)
{
    /* release ports, irq and such -- like fops->close */

    netif_stop_queue(dev); /* can't transmit any more */
    return 0;
}

int snull_tx(struct sk_buff *skb, struct net_device *dev)
{
    int len;
    char *data, shortpkt[ETH_ZLEN];
    struct snull_priv *priv = netdev_priv(dev);

    data = skb->data;
    len = skb->len;
    //if the len is less than the shortest ethernet frame size,
    //then add zero to buffer so that length becomes ETH_ZLEN=60
    if (len < ETH_ZLEN)
    {
        memset(shortpkt, 0, ETH_ZLEN);
        memcpy(shortpkt, skb->data, skb->len);
        len = ETH_ZLEN;
        data = shortpkt;
    }

    //new api to save the timestamp for trans_start
    netif_trans_update(dev);

    /* Remember the skb, so we can free it at interrupt time */
    priv->skb = skb;

    /* actual deliver of data is device-specific, and not shown here */
    printk(KERN_INFO "here we should call the snull_hw_tx method, currently it is commented out");
    //snull_hw_tx(data, len, dev);

    return 0; /* Our simple device can not fail */
}

//If timeout happens when packet being transmitted,
//then one must call the interupt to clean up
//and make the queue available
void snull_tx_timeout(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);

    printk("Timeout happened in snull interupt.... \n");
    priv->status = SNULL_TX_INTR;
    //call interupt by ourselves to clean up the timed out packet
//    snull_interrupt(0, dev, NULL);
    //collect the stats
    priv->stats.tx_errors++;
    //restart the transmission queue
    netif_wake_queue(dev);
    return;
}

void snull_rx(struct net_device *dev, struct snull_packet *pkt)
{
    struct sk_buff *skb;
    struct snull_priv *priv = netdev_priv(dev);

    /*
     * The packet has been retrieved from the transmission
     * medium. Build an skb around it, so upper layers can handle it
     */
    skb = dev_alloc_skb(pkt->datalen + 2);
    if (!skb) {
        if (printk_ratelimit(  ))
            printk(KERN_NOTICE "snull rx: low on mem - packet dropped\n");
        priv->stats.rx_dropped++;
        goto out;
    }
    memcpy(skb_put(skb, pkt->datalen), pkt->data, pkt->datalen);

    /* Write metadata, and then pass to the receive level */
    skb->dev = dev;
    skb->protocol = eth_type_trans(skb, dev);
    skb->ip_summed = CHECKSUM_UNNECESSARY; /* don't check it */
    priv->stats.rx_packets++;
    priv->stats.rx_bytes += pkt->datalen;
    //hand off the skb to upper layers
    netif_rx(skb);
  out:
    return;
}

void snull_release_buffer(struct snull_packet *pkt)
{
    unsigned long flags;
    struct snull_priv *priv = netdev_priv(pkt->dev);
    
    spin_lock_irqsave(&priv->lock, flags);
    pkt->next = priv->ppool;
    priv->ppool = pkt;
    spin_unlock_irqrestore(&priv->lock, flags);
    if (netif_queue_stopped(pkt->dev) && pkt->next == NULL)
        netif_wake_queue(pkt->dev);
}

static void snull_regular_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
    int statusword;
    struct snull_priv *priv;
    struct snull_packet *pkt = NULL;
    /*
     * As usual, check the "device" pointer to be sure it is
     * really interrupting.
     * Then assign "struct device *dev"
     */
    struct net_device *dev = (struct net_device *)dev_id;
    /* ... and check with hw if it's really ours */

    /* paranoid */
    if (!dev)
        return;

    /* Lock the device */
    priv = netdev_priv(dev);
    spin_lock(&priv->lock);

    /* retrieve statusword: real netdevices use I/O instructions */
    statusword = priv->status;
    priv->status = 0;
    if (statusword & SNULL_RX_INTR) {
        /* send it to snull_rx for handling */
        pkt = priv->rx_queue;
        if (pkt) {
            priv->rx_queue = pkt->next;
            snull_rx(dev, pkt);
        }
    }
    if (statusword & SNULL_TX_INTR) {
        /* a transmission is over: free the skb */
        priv->stats.tx_packets++;
        priv->stats.tx_bytes += priv->tx_packetlen;
        dev_kfree_skb(priv->skb);
    }

    /* Unlock the device and we are done */
    spin_unlock(&priv->lock);
    if (pkt) snull_release_buffer(pkt); /* Do this outside the lock! */
    return;
}

static const struct net_device_ops snull_netdev_ops = {
    .ndo_open            = snull_open,
    .ndo_stop            = snull_release,
    .ndo_start_xmit      = snull_tx,
    //.ndo_do_ioctl        = snull_ioctl,
    //.ndo_set_config      = snull_config,
    //.ndo_get_stats       = snull_stats,
    //.ndo_change_mtu      = snull_change_mtu,
    .ndo_tx_timeout      = snull_tx_timeout,
};

//initialize method, invoked by register_netdev()
void snull_init(struct net_device *dev)
{
    struct snull_priv *priv = netdev_priv(dev);
    ether_setup(dev); /* assign some of the fields */

    //initialize the api
    dev->netdev_ops = &snull_netdev_ops;
    dev->watchdog_timeo = 10; //jeffies
    //initialize the snull_priv structure
    priv = netdev_priv(dev);
    memset(priv, 0, sizeof(struct snull_priv));
    spin_lock_init(&priv->lock);
    priv->rx_int_enabled = 1; /* enable receive interrupts */
}

static void hello_world_exit(void)
{
    int i;
    for (i = 0; i < 2;  i++)
    {
        if (snull_devs[i])
        {
            //removes the interface from the system, (from the module?)
            unregister_netdev(snull_devs[i]);
            //snull_teardown_pool(snull_devs[i]); //TODO invetigate this function
            free_netdev(snull_devs[i]);
        }
    }
    printk(KERN_INFO "uninstalled successfully\n");
    return;
}

static int hello_world_init(void)
{
    int result, i;
    //initialize
    snull_devs[0] = alloc_netdev(sizeof(struct snull_priv), "sn%d", NET_NAME_UNKNOWN, snull_init);
    snull_devs[1] = alloc_netdev(sizeof(struct snull_priv), "sn%d", NET_NAME_UNKNOWN, snull_init);
    if (snull_devs[0] == NULL || snull_devs[1] == NULL)
    {
        //TODO Cleanup the allocated structs
        hello_world_exit();
        printk(KERN_INFO "snull: Failed to allocate the memory");
        return -1;
    }
    //upon successful initialize, register the device
    for (i = 0; i < 2; i++)
    {
        if ((result = register_netdev(snull_devs[i])))
        {
            printk(KERN_INFO "snull: error %i registering device \"%s\"\n", result, snull_devs[i]->name);
            hello_world_exit();
            return -2;
        }
    }
    printk(KERN_INFO "successfully allocated and registered the net_device structure\n");
    return 0;
}

module_init(hello_world_init);
module_exit(hello_world_exit);

MODULE_LICENSE("GPL");
