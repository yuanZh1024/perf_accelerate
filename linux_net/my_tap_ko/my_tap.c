#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/skbuff.h>
#include <linux/mutex.h>

#define DEVICE_NAME "mytap"
#define MYTAP_BIND  _IO('M', 1)

static struct net_device *mytap_dev;

static dev_t devno;
static struct cdev mytap_cdev;
static struct class *mytap_class;

/*
 * 一个非常简化的 packet buffer。
 *
 * 真正 TAP 会涉及队列、waitqueue、poll、并发等。
 * 这里为了教学，只保存一个 packet。
 */
#define BUF_SIZE 2048

static char packet_buf[BUF_SIZE];
static int packet_len;
static DEFINE_MUTEX(packet_lock);


/*
 * ---------------------------------------------------------
 * 1. net_device 操作
 * ---------------------------------------------------------
 */

static int mytap_open(struct net_device *dev)
{
    pr_info("mytap: net_device open\n");
    return 0;
}

static int mytap_stop(struct net_device *dev)
{
    pr_info("mytap: net_device stop\n");
    return 0;
}


/*
 * 用户态 write() 的 packet 最终可以通过这里进入网络设备。
 *
 * 这里我们只打印 packet。
 */
static netdev_tx_t mytap_xmit(struct sk_buff *skb,
                              struct net_device *dev)
{
    pr_info("mytap: xmit packet len=%u\n", skb->len);

    /*
     * 真实网络设备通常会：
     *
     *   物理网卡 -> DMA
     *
     * 虚拟网卡则可以：
     *
     *   skb -> 另一个处理路径
     *
     * 我们这里简单丢掉。
     */

    dev_kfree_skb(skb);

    return NETDEV_TX_OK;
}


static const struct net_device_ops mytap_netdev_ops = {
    .ndo_open       = mytap_open,
    .ndo_stop       = mytap_stop,
    .ndo_start_xmit = mytap_xmit,
};


/*
 * ---------------------------------------------------------
 * 2. 字符设备
 * ---------------------------------------------------------
 */


/*
 * 用户态：
 *
 *     fd = open("/dev/mytap", ...)
 *
 * 最终进入这里。
 */
static int mytap_chr_open(struct inode *inode,
                          struct file *file)
{
    pr_info("mytap: /dev/mytap opened\n");
    return 0;
}


/*
 * 用户态：
 *
 *     ioctl(fd, MYTAP_BIND)
 *
 * 到这里。
 *
 * 教学版里 ioctl 不做复杂事情，
 * 只是告诉我们：
 *
 * "这个 fd 已经绑定到 mytap0"
 */
static long mytap_ioctl(struct file *file,
                        unsigned int cmd,
                        unsigned long arg)
{
    switch (cmd) {

    case MYTAP_BIND:

        pr_info("mytap: fd bound to mytap0\n");

        return 0;

    default:
        return -EINVAL;
    }
}


/*
 * 用户态：
 *
 *     write(fd, packet, len)
 *
 * 到这里。
 *
 * 我们把 packet 保存到 packet_buf。
 */
static ssize_t mytap_write(struct file *file,
                           const char __user *buf,
                           size_t len,
                           loff_t *ppos)
{
    if (len > BUF_SIZE)
        return -EINVAL;

    mutex_lock(&packet_lock);

    if (copy_from_user(packet_buf, buf, len)) {
        mutex_unlock(&packet_lock);
        return -EFAULT;
    }

    packet_len = len;

    mutex_unlock(&packet_lock);

    pr_info("mytap: received packet from userspace len=%zu\n",
            len);

    return len;
}


/*
 * 用户态：
 *
 *     read(fd, buf, ...)
 *
 * 到这里。
 */
static ssize_t mytap_read(struct file *file,
                          char __user *buf,
                          size_t len,
                          loff_t *ppos)
{
    int ret;

    mutex_lock(&packet_lock);

    if (packet_len == 0) {
        mutex_unlock(&packet_lock);
        return 0;
    }

    if (len < packet_len) {
        mutex_unlock(&packet_lock);
        return -EINVAL;
    }

    if (copy_to_user(buf, packet_buf, packet_len)) {
        mutex_unlock(&packet_lock);
        return -EFAULT;
    }

    ret = packet_len;
    packet_len = 0;

    mutex_unlock(&packet_lock);

    pr_info("mytap: sent packet to userspace len=%d\n", ret);

    return ret;
}


static const struct file_operations mytap_fops = {
    .owner          = THIS_MODULE,
    .open           = mytap_chr_open,
    .read           = mytap_read,
    .write          = mytap_write,
    .unlocked_ioctl = mytap_ioctl,
};


/*
 * ---------------------------------------------------------
 * 3. 模块初始化
 * ---------------------------------------------------------
 */

static int __init mytap_init(void)
{
    int ret;

    pr_info("mytap: init\n");

    /*
     * 第一步：
     *
     * 创建 net_device
     *
     * 本质：
     *
     *     struct net_device
     *             ↓
     *          mytap0
     */
    mytap_dev = alloc_netdev(
        0,
        "mytap%d",
        NET_NAME_UNKNOWN,
        ether_setup
    );

    if (!mytap_dev)
        return -ENOMEM;

    mytap_dev->netdev_ops = &mytap_netdev_ops;

    /*
     * 注册到 Linux 网络子系统。
     */
    ret = register_netdev(mytap_dev);

    if (ret) {
        free_netdev(mytap_dev);
        return ret;
    }


    /*
     * 第二步：
     *
     * 创建字符设备。
     *
     * 这相当于提供：
     *
     *     /dev/mytap
     */

    ret = alloc_chrdev_region(
        &devno,
        0,
        1,
        DEVICE_NAME
    );

    if (ret)
        goto err_netdev;


    cdev_init(&mytap_cdev, &mytap_fops);

    ret = cdev_add(
        &mytap_cdev,
        devno,
        1
    );

    if (ret)
        goto err_chrdev;


    mytap_class = class_create(DEVICE_NAME);

    if (IS_ERR(mytap_class)) {
        ret = PTR_ERR(mytap_class);
        goto err_cdev;
    }


    device_create(
        mytap_class,
        NULL,
        devno,
        NULL,
        DEVICE_NAME
    );

    pr_info("mytap: created mytap0\n");
    pr_info("mytap: created /dev/mytap\n");

    return 0;


err_cdev:
    cdev_del(&mytap_cdev);

err_chrdev:
    unregister_chrdev_region(devno, 1);

err_netdev:
    unregister_netdev(mytap_dev);
    free_netdev(mytap_dev);

    return ret;
}


/*
 * ---------------------------------------------------------
 * 4. 模块退出
 * ---------------------------------------------------------
 */

static void __exit mytap_exit(void)
{
    pr_info("mytap: exit\n");

    device_destroy(
        mytap_class,
        devno
    );

    class_destroy(mytap_class);

    cdev_del(&mytap_cdev);

    unregister_chrdev_region(
        devno,
        1
    );

    unregister_netdev(
        mytap_dev
    );

    free_netdev(
        mytap_dev
    );
}

module_init(mytap_init);
module_exit(mytap_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("demo");
MODULE_DESCRIPTION("A minimal TAP-like device for learning");