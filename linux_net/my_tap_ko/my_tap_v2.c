
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/poll.h>

#define DEVNAME "mytap"
#define MAX_QUEUE 1024

static struct net_device *mytap_dev;
static dev_t devno;
static struct cdev mytap_cdev;
static struct class *mytap_class;
static struct device *mytap_device;

static struct sk_buff_head tx_queue;
static DECLARE_WAIT_QUEUE_HEAD(read_wait);
static bool stopping;

/* ---------------- 网卡侧 ---------------- */

static int mytap_open(struct net_device *dev)
{
    netif_start_queue(dev);
    pr_info("mytap: netdev opened\n");
    return 0;
}

static int mytap_stop(struct net_device *dev)
{
    netif_stop_queue(dev);
    pr_info("mytap: netdev stopped\n");
    return 0;
}

/*
 * Linux 网络栈要从 mytap0 发出报文时，
 * 会调用 ndo_start_xmit。
 *
 * 我们把 skb 放进队列，等待用户态 read()。
 */
static netdev_tx_t mytap_xmit(struct sk_buff *skb,
                              struct net_device *dev)
{
    if (READ_ONCE(stopping) ||
        skb_queue_len(&tx_queue) >= MAX_QUEUE) {
        dev->stats.tx_dropped++;
        dev_kfree_skb(skb);
        return NETDEV_TX_OK;
    }

    skb_queue_tail(&tx_queue, skb);
    dev->stats.tx_packets++;
    dev->stats.tx_bytes += skb->len;

    wake_up_interruptible(&read_wait);
    return NETDEV_TX_OK;
}

static const struct net_device_ops mytap_ops = {
    .ndo_open       = mytap_open,
    .ndo_stop       = mytap_stop,
    .ndo_start_xmit = mytap_xmit,
};

/* ---------------- 字符设备侧 ---------------- */

static int mytap_chr_open(struct inode *inode,
                          struct file *file)
{
    pr_info("mytap: char device opened\n");
    return 0;
}

/*
 * 用户态 read():
 * 从网卡发送队列中取出一个 skb。
 *
 * 没有报文时阻塞等待。
 */
static ssize_t mytap_read(struct file *file,
                          char __user *buf,
                          size_t len,
                          loff_t *ppos)
{
    struct sk_buff *skb;
    int ret;

    ret = wait_event_interruptible(
        read_wait,
        !skb_queue_empty(&tx_queue) ||
        READ_ONCE(stopping)
    );

    if (ret)
        return ret;

    if (READ_ONCE(stopping))
        return -ESHUTDOWN;

    skb = skb_dequeue(&tx_queue);
    if (!skb)
        return -EAGAIN;

    if (len < skb->len) {
        skb_queue_head(&tx_queue, skb);
        return -EMSGSIZE;
    }

    len = skb->len;

    if (copy_to_user(buf, skb->data, len)) {
        skb_queue_head(&tx_queue, skb);
        return -EFAULT;
    }

    dev_kfree_skb(skb);
    return len;
}

/*
 * 用户态 write():
 * 将一个完整的 Ethernet frame 注入 Linux 接收路径。
 */
static ssize_t mytap_write(struct file *file,
                           const char __user *buf,
                           size_t len,
                           loff_t *ppos)
{
    struct sk_buff *skb;
    int ret;

    if (len < ETH_HLEN || len > 65535)
        return -EINVAL;

    if (READ_ONCE(stopping))
        return -ESHUTDOWN;

    skb = netdev_alloc_skb(mytap_dev, len + NET_IP_ALIGN);
    if (!skb)
        return -ENOMEM;

    skb_reserve(skb, NET_IP_ALIGN);

    if (copy_from_user(skb_put(skb, len), buf, len)) {
        dev_kfree_skb(skb);
        return -EFAULT;
    }

    skb->dev = mytap_dev;
    skb->protocol = eth_type_trans(skb, mytap_dev);

    ret = netif_rx(skb);

    if (ret == NET_RX_DROP) {
        mytap_dev->stats.rx_dropped++;
        return -ENOBUFS;
    }

    mytap_dev->stats.rx_packets++;
    mytap_dev->stats.rx_bytes += len;

    return len;
}

static const struct file_operations mytap_fops = {
    .owner = THIS_MODULE,
    .open  = mytap_chr_open,
    .read  = mytap_read,
    .write = mytap_write,
};

/* ---------------- 模块生命周期 ---------------- */

static int __init mytap_init(void)
{
    int ret;

    skb_queue_head_init(&tx_queue);

    mytap_dev = alloc_netdev(
        0, "mytap%d", NET_NAME_UNKNOWN, ether_setup);

    if (!mytap_dev)
        return -ENOMEM;

    mytap_dev->netdev_ops = &mytap_ops;
    eth_hw_addr_random(mytap_dev);

    ret = register_netdev(mytap_dev);
    if (ret)
        goto err_netdev;

    ret = alloc_chrdev_region(&devno, 0, 1, DEVNAME);
    if (ret)
        goto err_unregister;

    cdev_init(&mytap_cdev, &mytap_fops);
    mytap_cdev.owner = THIS_MODULE;

    ret = cdev_add(&mytap_cdev, devno, 1);
    if (ret)
        goto err_chrdev;

    mytap_class = class_create(DEVNAME);
    if (IS_ERR(mytap_class)) {
        ret = PTR_ERR(mytap_class);
        goto err_cdev;
    }

    mytap_device = device_create(
        mytap_class, NULL, devno, NULL, DEVNAME);

    if (IS_ERR(mytap_device)) {
        ret = PTR_ERR(mytap_device);
        goto err_class;
    }

    pr_info("mytap: registered mytap0 and /dev/mytap\n");
    return 0;

err_class:
    class_destroy(mytap_class);
err_cdev:
    cdev_del(&mytap_cdev);
err_chrdev:
    unregister_chrdev_region(devno, 1);
err_unregister:
    unregister_netdev(mytap_dev);
err_netdev:
    free_netdev(mytap_dev);
    return ret;
}

static void __exit mytap_exit(void)
{
    WRITE_ONCE(stopping, true);
    wake_up_interruptible(&read_wait);

    device_destroy(mytap_class, devno);
    class_destroy(mytap_class);
    cdev_del(&mytap_cdev);
    unregister_chrdev_region(devno, 1);
    unregister_netdev(mytap_dev);

    skb_queue_purge(&tx_queue);
    free_netdev(mytap_dev);

    pr_info("mytap: unloaded\n");
}

module_init(mytap_init);
module_exit(mytap_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Educational TAP-like network device");